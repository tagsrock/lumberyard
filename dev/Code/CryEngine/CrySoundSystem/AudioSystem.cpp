/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "AudioSystem.h"

#include <SoundCVars.h>
#include <AudioProxy.h>

#include <AzCore/std/bind/bind.h>

namespace Audio
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // CAudioThread
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioThread::~CAudioThread()
    {
        Deactivate();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioThread::Run()
    {
        AZ_Assert(m_audioSystem, "Audio Thread has no Audio System to run!\n");
        m_running = true;
        while (m_running)
        {
            m_audioSystem->InternalUpdate();
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioThread::Activate(CAudioSystem* const audioSystem)
    {
        m_audioSystem = audioSystem;

        AZStd::thread_desc threadDesc;
        threadDesc.m_name = "Audio Thread";


        auto threadFunc = AZStd::bind(&CAudioThread::Run, this);
        m_thread = AZStd::thread(threadFunc, &threadDesc);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioThread::Deactivate()
    {
        if (m_running)
        {
            m_running = false;
            m_thread.join();
        }
    }



    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // CAudioSystem
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioSystem::CAudioSystem()
        : m_bSystemInitialized(false)
        , m_lastUpdateTime(AZStd::chrono::system_clock::now())
        , m_elapsedTime(0.f)
        , m_updatePeriod(0.f)
        , m_oATL()
    {
        m_apAudioProxies.reserve(g_audioCVars.m_nAudioObjectPoolSize);
        m_apAudioProxiesToBeFreed.reserve(16);
        AudioSystemRequestBus::Handler::BusConnect();
        AudioSystemThreadSafeRequestBus::Handler::BusConnect();
        AudioSystemThreadSafeInternalRequestBus::Handler::BusConnect();
        AudioSystemInternalRequestBus::Handler::BusConnect();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioSystem::~CAudioSystem()
    {
        AudioSystemRequestBus::Handler::BusDisconnect();
        AudioSystemThreadSafeRequestBus::Handler::BusDisconnect();
        AudioSystemThreadSafeInternalRequestBus::Handler::BusDisconnect();
        AudioSystemInternalRequestBus::Handler::BusDisconnect();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::Init()
    {
        CryLogAlways("AZ::Component - CAudioSystem::Init()");
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::Activate()
    {
    	CryLogAlways("AZ::Component - CAudioSystem::Activate()");
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::Deactivate()
    {
    	CryLogAlways("AZ::Component - CAudioSystem::Deactivate()");
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::PushRequest(const SAudioRequest& audioRequestData)
    {
        CAudioRequestInternal request(audioRequestData);

        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::PushRequest - called from non-Main thread!");
        AZ_Assert(0 == (request.nFlags & eARF_THREAD_SAFE_PUSH), "AudioSystem::PushRequest - called with flag THREAD_SAFE_PUSH!");
        AZ_Assert(0 == (request.nFlags & eARF_EXECUTE_BLOCKING), "AudioSystem::PushRequest - called with flag EXECUTE_BLOCKING!");

        AudioSystemInternalRequestBus::QueueBroadcast(&AudioSystemInternalRequestBus::Events::ProcessRequestByPriority, request);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::PushRequestBlocking(const SAudioRequest& audioRequestData)
    {
        // Main Thread!
        FUNCTION_PROFILER_ALWAYS(GetISystem(), PROFILE_AUDIO);

        CAudioRequestInternal request(audioRequestData);

        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::PushRequestBlocking - called from non-Main thread!");
        AZ_Assert(0 != (request.nFlags & eARF_EXECUTE_BLOCKING), "AudioSystem::PushRequestBlocking - called without EXECUTE_BLOCKING flag!");

        ProcessRequestBlocking(request);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::PushRequestThreadSafe(const SAudioRequest& audioRequestData)
    {
        CAudioRequestInternal request(audioRequestData);

        AZ_Assert(0 != (request.nFlags & eARF_THREAD_SAFE_PUSH), "AudioSystem::PushRequestThreadSafe - called without THREAD_SAFE_PUSH flag!");
        AZ_Assert(0 == (request.nFlags & eARF_EXECUTE_BLOCKING), "AudioSystem::PushRequestThreadSafe - called with flag EXECUTE_BLOCKING!");

        AudioSystemThreadSafeInternalRequestBus::QueueBroadcast(&AudioSystemThreadSafeInternalRequestBus::Events::ProcessRequestThreadSafe, request);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::AddRequestListener(
        AudioRequestCallbackType func,
        void* const pObjectToListenTo,
        const EAudioRequestType requestType,
        const TATLEnumFlagsType specificRequestMask)
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::AddRequestListener - called from a non-Main thread!");

        SAudioRequest oRequest;
        oRequest.nFlags = (eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING);
        SAudioManagerRequestData<eAMRT_ADD_REQUEST_LISTENER> oRequestData(pObjectToListenTo, func, requestType, specificRequestMask);
        oRequest.pOwner = pObjectToListenTo; // This makes sure that the listener is notified.
        oRequest.pData = &oRequestData;

        PushRequestBlocking(oRequest);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::RemoveRequestListener(AudioRequestCallbackType func, void* const pObjectToListenTo)
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::RemoveRequestListener - called from a non-Main thread!");

        SAudioRequest oRequest;
        oRequest.nFlags = (eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING);
        SAudioManagerRequestData<eAMRT_REMOVE_REQUEST_LISTENER> oRequestData(pObjectToListenTo, func);
        oRequest.pOwner = pObjectToListenTo; // This makes sure that the listener is notified.
        oRequest.pData = &oRequestData;

        PushRequestBlocking(oRequest);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::ExternalUpdate()
    {
        // Main Thread!
        FUNCTION_PROFILER_ALWAYS(GetISystem(), PROFILE_AUDIO);
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::ExternalUpdate - called from non-Main thread!");

        // Notify callbacks on the pending callbacks queue...
        // These are requests that were completed then queued for callback processing to happen here.
        ExecuteRequestCompletionCallbacks(m_pendingCallbacksQueue, m_pendingCallbacksMutex);

        // Notify callbacks from the "thread safe" queue...
        ExecuteRequestCompletionCallbacks(m_threadSafeCallbacksQueue, m_threadSafeCallbacksMutex, true);

        // Free any Audio Proxies that are queued up for deletion...
        for (auto audioProxy : m_apAudioProxiesToBeFreed)
        {
            azdestroy(audioProxy, Audio::AudioSystemAllocator);
        }

        m_apAudioProxiesToBeFreed.clear();

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        DrawAudioDebugData();
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::UpdateTime()
    {
        auto current = AZStd::chrono::system_clock::now();
        m_elapsedTime = AZStd::chrono::duration_cast<duration_ms>(current - m_lastUpdateTime);
        m_lastUpdateTime = current;
        m_updatePeriod += m_elapsedTime;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::InternalUpdate()
    {
        // Audio Thread!
        FUNCTION_PROFILER_ALWAYS(GetISystem(), PROFILE_AUDIO);

        UpdateTime();

        bool handledBlockingRequests = false;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_blockingRequestsMutex);
            handledBlockingRequests = ProcessRequests(m_blockingRequestsQueue);
        }

        if (!handledBlockingRequests)
        {
            // Call the ProcessRequestByPriority events queued up...
            AudioSystemInternalRequestBus::ExecuteQueuedEvents();
        }

        // Call the ProcessRequestThreadSafe events queued up...
        // Note: in the old code, this would be guarded by a try_lock, so it wasn't guaranteed to process these.
        AudioSystemThreadSafeInternalRequestBus::ExecuteQueuedEvents();

        if (m_updatePeriod > AZStd::chrono::milliseconds(2))
        {
            m_oATL.Update(m_updatePeriod.count());
            m_updatePeriod = duration_ms::zero();
        }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_debugNameStoreMutex);
            m_debugNameStore.SyncChanges(m_oATL.GetDebugStore());
        }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

        if (!handledBlockingRequests)
        {
            m_processingEvent.try_acquire_for(AZStd::chrono::milliseconds(2));
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioSystem::Initialize()
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::Initialize - called from a non-Main thread!");

        if (!m_bSystemInitialized)
        {
            m_audioSystemThread.Deactivate();
            m_oATL.Initialize();
            m_audioSystemThread.Activate(this);

            for (int i = 0; i < g_audioCVars.m_nAudioObjectPoolSize; ++i)
            {
                auto audioProxy = azcreate(CAudioProxy, (), Audio::AudioSystemAllocator, "AudioProxy");
                m_apAudioProxies.push_back(audioProxy);
            }

            m_bSystemInitialized = true;
        }

        return m_bSystemInitialized;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::Release()
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::Release - called from a non-Main thread!");

        SAudioRequest request;
        request.nFlags = (eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING);

        // Unload global audio file cache data...
        SAudioManagerRequestData<eAMRT_UNLOAD_AFCM_DATA_BY_SCOPE> unloadAFCM(eADS_GLOBAL);
        request.pData = &unloadAFCM;
        PushRequestBlocking(request);

        // Release the audio implementation...
        SAudioManagerRequestData<eAMRT_RELEASE_AUDIO_IMPL> releaseImpl;
        request.pData = &releaseImpl;
        PushRequestBlocking(request);

        for (auto audioProxy : m_apAudioProxies)
        {
            azdestroy(audioProxy, Audio::AudioSystemAllocator);
        }

        for (auto audioProxy : m_apAudioProxiesToBeFreed)
        {
            azdestroy(audioProxy, Audio::AudioSystemAllocator);
        }

        stl::free_container(m_apAudioProxies);
        stl::free_container(m_apAudioProxiesToBeFreed);

        m_audioSystemThread.Deactivate();
        const bool bSuccess = m_oATL.ShutDown();
        m_bSystemInitialized = false;

        // The AudioSystem must be the last object that is freed from the audio memory pool before the allocator is destroyed.
        azdestroy(this, Audio::AudioSystemAllocator, CAudioSystem);

        g_audioCVars.UnregisterVariables();

        if (AZ::AllocatorInstance<Audio::AudioSystemAllocator>::IsReady())
        {
            AZ::AllocatorInstance<Audio::AudioSystemAllocator>::Destroy();
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioControlID CAudioSystem::GetAudioTriggerID(const char* const sAudioTriggerName) const
    {
        return m_oATL.GetAudioTriggerID(sAudioTriggerName);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioControlID CAudioSystem::GetAudioRtpcID(const char* const sAudioRtpcName) const
    {
        return m_oATL.GetAudioRtpcID(sAudioRtpcName);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioControlID CAudioSystem::GetAudioSwitchID(const char* const sAudioStateName) const
    {
        return m_oATL.GetAudioSwitchID(sAudioStateName);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioSwitchStateID CAudioSystem::GetAudioSwitchStateID(const TAudioControlID nSwitchID, const char* const sAudioSwitchStateName) const
    {
        return m_oATL.GetAudioSwitchStateID(nSwitchID, sAudioSwitchStateName);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioPreloadRequestID CAudioSystem::GetAudioPreloadRequestID(const char* const sAudioPreloadRequestName) const
    {
        return m_oATL.GetAudioPreloadRequestID(sAudioPreloadRequestName);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioEnvironmentID CAudioSystem::GetAudioEnvironmentID(const char* const sAudioEnvironmentName) const
    {
        return m_oATL.GetAudioEnvironmentID(sAudioEnvironmentName);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioSystem::ReserveAudioListenerID(TAudioObjectID& rAudioObjectID)
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::ReserveAudioListenerID - called from a non-Main thread!");
        return m_oATL.ReserveAudioListenerID(rAudioObjectID);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioSystem::ReleaseAudioListenerID(TAudioObjectID const nAudioObjectID)
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::ReleaseAudioListenerID - called from a non-Main thread!");
        return m_oATL.ReleaseAudioListenerID(nAudioObjectID);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioSystem::SetAudioListenerOverrideID(const TAudioObjectID nAudioObjectID)
    {
        return m_oATL.SetAudioListenerOverrideID(nAudioObjectID);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::GetInfo(SAudioSystemInfo& rAudioSystemInfo)
    {
        //TODO: 
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    const char* CAudioSystem::GetControlsPath() const
    {
        // this shouldn't get called before UpdateControlsPath has been called.
        AZ_Assert(!m_sControlsPath.empty(), "AudioSystem::GetControlsPath - controls path has been requested before it has been set!");
        return m_sControlsPath.c_str();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::UpdateControlsPath()
    {
        string controlsPath("libs/gameaudio/");
        controlsPath += m_oATL.GetControlsImplSubPath();
        m_sControlsPath = PathUtil::ToNativePath(controlsPath).c_str();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    IAudioProxy* CAudioSystem::GetFreeAudioProxy()
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::GetFreeAudioProxy - called from a non-Main thread!");
        CAudioProxy* audioProxy = nullptr;

        if (!m_apAudioProxies.empty())
        {
            audioProxy = m_apAudioProxies.back();
            m_apAudioProxies.pop_back();
        }
        else
        {
            audioProxy = azcreate(CAudioProxy, (), Audio::AudioSystemAllocator, "AudioProxyEx");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
            if (!audioProxy)
            {
                g_audioLogger.Log(eALT_FATAL, "AudioSystem::GetFreeAudioProxy - failed to create new AudioProxy instance!");
            }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
        }

        return static_cast<IAudioProxy*>(audioProxy);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::FreeAudioProxy(IAudioProxy* const audioProxyI)
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::FreeAudioProxy - called from a non-Main thread!");
        auto const audioProxy = static_cast<CAudioProxy*>(audioProxyI);

        if (m_apAudioProxies.size() < g_audioCVars.m_nAudioObjectPoolSize)
        {
            m_apAudioProxies.push_back(audioProxy);
        }
        else
        {
            m_apAudioProxiesToBeFreed.push_back(audioProxy);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    TAudioSourceId CAudioSystem::CreateAudioSource(const SAudioInputConfig& sourceConfig)
    {
        return m_oATL.CreateAudioSource(sourceConfig);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::DestroyAudioSource(TAudioSourceId sourceId)
    {
        m_oATL.DestroyAudioSource(sourceId);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    const char* CAudioSystem::GetAudioControlName(const EAudioControlType controlType, const TATLIDType atlID) const
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::GetAudioControlName - called from non-Main thread!");
        const char* sResult = nullptr;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        AZStd::lock_guard<AZStd::mutex> lock(m_debugNameStoreMutex);
        switch (controlType)
        {
            case eACT_AUDIO_OBJECT:
            {
                sResult = m_debugNameStore.LookupAudioObjectName(atlID);
                break;
            }
            case eACT_TRIGGER:
            {
                sResult = m_debugNameStore.LookupAudioTriggerName(atlID);
                break;
            }
            case eACT_RTPC:
            {
                sResult = m_debugNameStore.LookupAudioRtpcName(atlID);
                break;
            }
            case eACT_SWITCH:
            {
                sResult = m_debugNameStore.LookupAudioSwitchName(atlID);
                break;
            }
            case eACT_PRELOAD:
            {
                sResult = m_debugNameStore.LookupAudioPreloadRequestName(atlID);
                break;
            }
            case eACT_ENVIRONMENT:
            {
                sResult = m_debugNameStore.LookupAudioEnvironmentName(atlID);
                break;
            }
            case eACT_SWITCH_STATE: // not handled here, use GetAudioSwitchStateName!
            case eACT_NONE:
            default: // fall-through!
            {
                g_audioLogger.Log(eALT_WARNING, "AudioSystem::GetAudioControlName - called with invalid EAudioControlType!");
                break;
            }
        }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

        return sResult;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    const char* CAudioSystem::GetAudioSwitchStateName(const TAudioControlID switchID, const TAudioSwitchStateID stateID) const
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::GetAudioSwitchStateName - called from non-Main thread!");
        const char* sResult = nullptr;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        AZStd::lock_guard<AZStd::mutex> lock(m_debugNameStoreMutex);
        sResult = m_debugNameStore.LookupAudioSwitchStateName(switchID, stateID);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

        return sResult;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::ExtractCompletedRequests(TAudioRequests& requestQueue, TAudioRequests& extractedCallbacks)
    {
        auto iter(requestQueue.begin());
        auto iterEnd(requestQueue.end());

        while (iter != iterEnd)
        {
            const CAudioRequestInternal& refRequest = (*iter);
            if (refRequest.IsComplete())
            {
                // the request has completed, eligible for notification callback.
                // move the request to the extraction queue.
                extractedCallbacks.push_back(refRequest);
                iter = requestQueue.erase(iter);
                iterEnd = requestQueue.end();
                continue;
            }

            ++iter;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::ExecuteRequestCompletionCallbacks(TAudioRequests& requestQueue, AZStd::mutex& requestQueueMutex, bool tryLock)
    {
        TAudioRequests extractedCallbacks;

        if (tryLock)
        {
            if (requestQueueMutex.try_lock())
            {
                ExtractCompletedRequests(requestQueue, extractedCallbacks);
                requestQueueMutex.unlock();
            }
        }
        else
        {
            AZStd::lock_guard<AZStd::mutex> lock(requestQueueMutex);
            ExtractCompletedRequests(requestQueue, extractedCallbacks);
        }

        // Notify listeners
        for (const auto& callback : extractedCallbacks)
        {
            m_oATL.NotifyListener(callback);
        }

        extractedCallbacks.clear();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::ProcessRequestBlocking(CAudioRequestInternal& request)
    {
        if (m_oATL.CanProcessRequests())
        {
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_blockingRequestsMutex);
                m_blockingRequestsQueue.push_back(request);
            }

            m_processingEvent.release();
            m_mainEvent.acquire();

            ExecuteRequestCompletionCallbacks(m_blockingRequestsQueue, m_blockingRequestsMutex);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::ProcessRequestThreadSafe(CAudioRequestInternal& request)
    {
        // Audio Thread!
        FUNCTION_PROFILER_ALWAYS(GetISystem(), PROFILE_AUDIO);

        if (m_oATL.CanProcessRequests())
        {
            if (request.eStatus == eARS_NONE)
            {
                request.eStatus = eARS_PENDING;
                m_oATL.ProcessRequest(request);
            }

            AZ_Assert(request.eStatus != eARS_PENDING, "AudioSystem::ProcessRequestThreadSafe - ATL finished processing request, but request is still in pending state!");
            if (request.eStatus != eARS_PENDING)
            {
                // push the request onto a callbacks queue for main thread to process later...
                AZStd::lock_guard<AZStd::mutex> lock(m_threadSafeCallbacksMutex);
                m_threadSafeCallbacksQueue.push_back(request);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::ProcessRequestByPriority(CAudioRequestInternal& request)
    {
        // Todo: This should handle request priority, use request priority as bus Address and process in priority order.

        FUNCTION_PROFILER_ALWAYS(GetISystem(), PROFILE_AUDIO);
        AZ_Assert(gEnv->mMainThreadId != CryGetCurrentThreadId(), "AudioSystem::ProcessRequestByPriority - called from Main thread!");

        if (m_oATL.CanProcessRequests())
        {
            if (request.eStatus == eARS_NONE)
            {
                request.eStatus = eARS_PENDING;
                m_oATL.ProcessRequest(request);
            }

            AZ_Assert(request.eStatus != eARS_PENDING, "AudioSystem::ProcessRequestByPriority - ATL finished processing request, but request is still in pending state!");
            if (request.eStatus != eARS_PENDING)
            {
                // push the request onto a callbacks queue for main thread to process later...
                AZStd::lock_guard<AZStd::mutex> lock(m_pendingCallbacksMutex);
                m_pendingCallbacksQueue.push_back(request);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioSystem::ProcessRequests(TAudioRequests& requestQueue)
    {
        bool success = false;

        for (auto& request : requestQueue)
        {
            if (!(request.nInternalInfoFlags & eARIF_WAITING_FOR_REMOVAL))
            {
                if (request.eStatus == eARS_NONE)
                {
                    request.eStatus = eARS_PENDING;
                    m_oATL.ProcessRequest(request);
                    success = true;
                }

                if (request.eStatus != eARS_PENDING)
                {
                    if (request.nFlags & eARF_EXECUTE_BLOCKING)
                    {
                        request.nInternalInfoFlags |= eARIF_WAITING_FOR_REMOVAL;
                        m_mainEvent.release();
                    }
                }
                else
                {
                    g_audioLogger.Log(eALT_ERROR, "AudioSystem::ProcessRequests - request still in Pending state after being processed by ATL!");
                }
            }
        }

        return success;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioSystem::OnCVarChanged(ICVar* const pCvar)
    {
        // nothing?
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
    void CAudioSystem::DrawAudioDebugData()
    {
        AZ_Assert(gEnv->mMainThreadId == CryGetCurrentThreadId(), "AudioSystem::DrawAudioDebugData - called from non-Main thread!");

        if (g_audioCVars.m_nDrawAudioDebug > 0)
        {
            SAudioRequest oRequest;
            oRequest.nFlags = (eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING);
            SAudioManagerRequestData<eAMRT_DRAW_DEBUG_INFO> oRequestData;
            oRequest.pData = &oRequestData;

            PushRequestBlocking(oRequest);
        }
    }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

} // namespace Audio
