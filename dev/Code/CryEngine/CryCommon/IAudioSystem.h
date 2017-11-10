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

#pragma once

#include <IAudioInterfacesCommonData.h>
#include <ILipSyncProvider.h>

#include <AzCore/Component/Component.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/std/parallel/mutex.h>


// Name of the audio system module class, used when loading CrySoundSystem library.
#define AUDIO_SYSTEM_MODULE_NAME            "EngineModule_CrySoundSystem"

// Note:
//   Need this explicit here to prevent circular includes to IEntity.
// Summary:
//   Unique identifier for each entity instance.
typedef unsigned int EntityId;

// External forward declarations.
struct IVisArea;
struct ICVar;

namespace Audio
{
    // Internal forward declarations.
    struct SAudioRequest;


    enum EATLDataScope : TATLEnumFlagsType
    {
        eADS_NONE           = 0,
        eADS_GLOBAL         = 1,
        eADS_LEVEL_SPECIFIC = 2,
        eADS_ALL            = 3,
    };

    enum EAudioManagerRequestType : TATLEnumFlagsType
    {
        eAMRT_NONE                      = 0,
        eAMRT_INIT_AUDIO_IMPL           = BIT(0),
        eAMRT_RELEASE_AUDIO_IMPL        = BIT(1),
        eAMRT_REFRESH_AUDIO_SYSTEM      = BIT(2),
        eAMRT_RESERVE_AUDIO_OBJECT_ID   = BIT(3),
        eAMRT_LOSE_FOCUS                = BIT(4),
        eAMRT_GET_FOCUS                 = BIT(5),
        eAMRT_MUTE_ALL                  = BIT(6),
        eAMRT_UNMUTE_ALL                = BIT(7),
        eAMRT_STOP_ALL_SOUNDS           = BIT(8),
        eAMRT_PARSE_CONTROLS_DATA       = BIT(9),
        eAMRT_PARSE_PRELOADS_DATA       = BIT(10),
        eAMRT_CLEAR_CONTROLS_DATA       = BIT(11),
        eAMRT_CLEAR_PRELOADS_DATA       = BIT(12),
        eAMRT_PRELOAD_SINGLE_REQUEST    = BIT(13),
        eAMRT_UNLOAD_SINGLE_REQUEST     = BIT(14),
        eAMRT_UNLOAD_AFCM_DATA_BY_SCOPE = BIT(15),
        eAMRT_DRAW_DEBUG_INFO           = BIT(16), // Only used internally!
        eAMRT_ADD_REQUEST_LISTENER      = BIT(17),
        eAMRT_REMOVE_REQUEST_LISTENER   = BIT(18),
        eAMRT_CHANGE_LANGUAGE           = BIT(19),
        eAMRT_RETRIGGER_AUDIO_CONTROLS  = BIT(20),
    };

    enum EAudioCallbackManagerRequestType : TATLEnumFlagsType
    {
        eACMRT_NONE                             = 0,
        eACMRT_REPORT_STARTED_EVENT             = BIT(0),   // Only relevant for delayed playback.
        eACMRT_REPORT_FINISHED_EVENT            = BIT(1),   // Only used internally!
        eACMRT_REPORT_FINISHED_TRIGGER_INSTANCE = BIT(2),   // Only used internally!
        eACMRT_REPORT_PROCESSED_OBSTRUCTION_RAY = BIT(3),   // Only used internally!
    };

    enum EAudioListenerRequestType : TATLEnumFlagsType
    {
        eALRT_NONE = 0,
        eALRT_SET_POSITION = BIT(0),
    };

    enum EAudioObjectRequestType : TATLEnumFlagsType
    {
        eAORT_NONE                      = 0,
        eAORT_PREPARE_TRIGGER           = BIT(0),
        eAORT_UNPREPARE_TRIGGER         = BIT(1),
        eAORT_EXECUTE_TRIGGER           = BIT(2),
        eAORT_STOP_TRIGGER              = BIT(3),
        eAORT_STOP_ALL_TRIGGERS         = BIT(4),
        eAORT_SET_POSITION              = BIT(5),
        eAORT_SET_RTPC_VALUE            = BIT(6),
        eAORT_SET_SWITCH_STATE          = BIT(7),
        eAORT_SET_VOLUME                = BIT(8),
        eAORT_SET_ENVIRONMENT_AMOUNT    = BIT(9),
        eAORT_RESET_ENVIRONMENTS        = BIT(10),
        eAORT_RESET_RTPCS               = BIT(11),
        eAORT_RELEASE_OBJECT            = BIT(12),
        eAORT_EXECUTE_SOURCE_TRIGGER    = BIT(13),  ///< Execute a trigger associated with an Audio Source (External file or Input stream)
    };

    enum EAudioObjectObstructionCalcType : TATLEnumFlagsType
    {
        eAOOCT_IGNORE       = 0,
        eAOOCT_SINGLE_RAY   = 1,
        eAOOCT_MULTI_RAY    = 2,

        eAOOCT_NONE         = 3,    // used only as a "default" state, nothing should use this at runtime.

        eAOOCT_COUNT,
    };

    enum EAudioControlType : TATLEnumFlagsType
    {
        eACT_NONE           = 0,
        eACT_AUDIO_OBJECT   = 1,
        eACT_TRIGGER        = 2,
        eACT_RTPC           = 3,
        eACT_SWITCH         = 4,
        eACT_SWITCH_STATE   = 5,
        eACT_PRELOAD        = 6,
        eACT_ENVIRONMENT    = 7,
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename T>
    AZ_FORCE_INLINE T AudioStringToID(const char* const source)
    {
        return static_cast<T>(AZ::Crc32(source));
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Function Callback Typedefs
    using AudioRequestCallbackType = void(*)(const SAudioRequestInfo* const);
    using TriggerFinishedCallbackType = void(*)(const TAudioObjectID, const TAudioControlID, void* const);


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Audio Manager Requests
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct SAudioManagerRequestDataBase
        : public SAudioRequestDataBase
    {
        explicit SAudioManagerRequestDataBase(const EAudioManagerRequestType ePassedType = eAMRT_NONE)
            : SAudioRequestDataBase(eART_AUDIO_MANAGER_REQUEST)
            , eType(ePassedType)
        {}

        ~SAudioManagerRequestDataBase() override {}

        const EAudioManagerRequestType eType;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template <EAudioManagerRequestType T>
    struct SAudioManagerRequestData
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData()
            : SAudioManagerRequestDataBase(T)
        {}

        ~SAudioManagerRequestData() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_INIT_AUDIO_IMPL>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData()
            : SAudioManagerRequestDataBase(eAMRT_INIT_AUDIO_IMPL)
        {}

        ~SAudioManagerRequestData<eAMRT_INIT_AUDIO_IMPL>() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_RELEASE_AUDIO_IMPL> : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData()
            : SAudioManagerRequestDataBase(eAMRT_RELEASE_AUDIO_IMPL)
        {}

        ~SAudioManagerRequestData<eAMRT_RELEASE_AUDIO_IMPL>() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_RESERVE_AUDIO_OBJECT_ID>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(TAudioObjectID* const pPassedObjectID, const char* const sPassedObjectName = nullptr)
            : SAudioManagerRequestDataBase(eAMRT_RESERVE_AUDIO_OBJECT_ID)
            , pObjectID(pPassedObjectID)
            , sObjectName(sPassedObjectName)
        {}

        ~SAudioManagerRequestData<eAMRT_RESERVE_AUDIO_OBJECT_ID>()override {}

        TAudioObjectID* const pObjectID;
        const char* const sObjectName;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_ADD_REQUEST_LISTENER>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(const void* const pPassedObjectToListenTo, AudioRequestCallbackType passedFunc, EAudioRequestType passedRequestType, TATLEnumFlagsType passedSpecificRequestMask)
            : SAudioManagerRequestDataBase(eAMRT_ADD_REQUEST_LISTENER)
            , pObjectToListenTo(pPassedObjectToListenTo)
            , func(passedFunc)
            , requestType(passedRequestType)
            , specificRequestMask(passedSpecificRequestMask)
        {}

        ~SAudioManagerRequestData<eAMRT_ADD_REQUEST_LISTENER>()override {}

        const void* const pObjectToListenTo;
        AudioRequestCallbackType func;
        const EAudioRequestType requestType;
        const TATLEnumFlagsType specificRequestMask;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_REMOVE_REQUEST_LISTENER>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(const void* const pPassedObjectToListenTo, AudioRequestCallbackType passedFunc)
            : SAudioManagerRequestDataBase(eAMRT_REMOVE_REQUEST_LISTENER)
            , pObjectToListenTo(pPassedObjectToListenTo)
            , func(passedFunc)
        {}

        ~SAudioManagerRequestData<eAMRT_REMOVE_REQUEST_LISTENER>()override {}

        const void* const pObjectToListenTo;
        AudioRequestCallbackType func;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_PARSE_CONTROLS_DATA>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(const char* const sControlsFolderPath, const EATLDataScope ePassedDataScope)
            : SAudioManagerRequestDataBase(eAMRT_PARSE_CONTROLS_DATA)
            , sFolderPath(sControlsFolderPath)
            , eDataScope(ePassedDataScope)
        {}

        ~SAudioManagerRequestData<eAMRT_PARSE_CONTROLS_DATA>()override {}

        const char* const sFolderPath;
        const EATLDataScope eDataScope;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_PARSE_PRELOADS_DATA>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(const char* const sControlsFolderPath, const EATLDataScope ePassedDataScope)
            : SAudioManagerRequestDataBase(eAMRT_PARSE_PRELOADS_DATA)
            , sFolderPath(sControlsFolderPath)
            , eDataScope(ePassedDataScope)
        {}

        ~SAudioManagerRequestData<eAMRT_PARSE_PRELOADS_DATA>()override {}

        const char* const sFolderPath;
        const EATLDataScope eDataScope;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_CLEAR_CONTROLS_DATA>
        : public SAudioManagerRequestDataBase
    {
        explicit SAudioManagerRequestData(const EATLDataScope ePassedDataScope = eADS_NONE)
            : SAudioManagerRequestDataBase(eAMRT_CLEAR_CONTROLS_DATA)
            , eDataScope(ePassedDataScope)
        {}

        ~SAudioManagerRequestData<eAMRT_CLEAR_CONTROLS_DATA>()override {}

        const EATLDataScope eDataScope;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_CLEAR_PRELOADS_DATA>
        : public SAudioManagerRequestDataBase
    {
        explicit SAudioManagerRequestData(const EATLDataScope ePassedDataScope = eADS_NONE)
            : SAudioManagerRequestDataBase(eAMRT_CLEAR_PRELOADS_DATA)
            , eDataScope(ePassedDataScope)
        {}

        ~SAudioManagerRequestData<eAMRT_CLEAR_PRELOADS_DATA>()override {}

        const EATLDataScope eDataScope;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_PRELOAD_SINGLE_REQUEST>
        : public SAudioManagerRequestDataBase
    {
        explicit SAudioManagerRequestData(const TAudioPreloadRequestID nRequestID = INVALID_AUDIO_PRELOAD_REQUEST_ID, const bool bPassedAutoLoadOnly = false)
            : SAudioManagerRequestDataBase(eAMRT_PRELOAD_SINGLE_REQUEST)
            , nPreloadRequestID(nRequestID)
            , bAutoLoadOnly(bPassedAutoLoadOnly)
        {}

        ~SAudioManagerRequestData<eAMRT_PRELOAD_SINGLE_REQUEST>()override {}

        const TAudioPreloadRequestID nPreloadRequestID;
        const bool bAutoLoadOnly;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_UNLOAD_SINGLE_REQUEST>
        : public SAudioManagerRequestDataBase
    {
        explicit SAudioManagerRequestData(const TAudioPreloadRequestID nRequestID = INVALID_AUDIO_PRELOAD_REQUEST_ID)
            : SAudioManagerRequestDataBase(eAMRT_UNLOAD_SINGLE_REQUEST)
            , nPreloadRequestID(nRequestID)
        {}

        ~SAudioManagerRequestData<eAMRT_UNLOAD_SINGLE_REQUEST>()override {}

        const TAudioPreloadRequestID nPreloadRequestID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_UNLOAD_AFCM_DATA_BY_SCOPE>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(const EATLDataScope eScope = eADS_NONE)
            : SAudioManagerRequestDataBase(eAMRT_UNLOAD_AFCM_DATA_BY_SCOPE)
            , eDataScope(eScope)
        {}

        ~SAudioManagerRequestData<eAMRT_UNLOAD_AFCM_DATA_BY_SCOPE>()override {}

        const EATLDataScope eDataScope;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_REFRESH_AUDIO_SYSTEM>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData(const char* const sPassedLevelName)
            : SAudioManagerRequestDataBase(eAMRT_REFRESH_AUDIO_SYSTEM)
            , sLevelName(sPassedLevelName)
        {}

        ~SAudioManagerRequestData<eAMRT_REFRESH_AUDIO_SYSTEM>()override {}

        const char* const sLevelName;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_CHANGE_LANGUAGE>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData()
            : SAudioManagerRequestDataBase(eAMRT_CHANGE_LANGUAGE)
        {}

        ~SAudioManagerRequestData<eAMRT_CHANGE_LANGUAGE>()override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioManagerRequestData<eAMRT_RETRIGGER_AUDIO_CONTROLS>
        : public SAudioManagerRequestDataBase
    {
        SAudioManagerRequestData()
            : SAudioManagerRequestDataBase(eAMRT_RETRIGGER_AUDIO_CONTROLS)
        {}

        ~SAudioManagerRequestData<eAMRT_RETRIGGER_AUDIO_CONTROLS>()override {}
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Audio Callback Manager Requests
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct SAudioCallbackManagerRequestDataBase
        : public SAudioRequestDataBase
    {
        explicit SAudioCallbackManagerRequestDataBase(const EAudioCallbackManagerRequestType ePassedType = eACMRT_NONE)
            : SAudioRequestDataBase(eART_AUDIO_CALLBACK_MANAGER_REQUEST)
            , eType(ePassedType)
        {}

        ~SAudioCallbackManagerRequestDataBase() override {}

        const EAudioCallbackManagerRequestType eType;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template <EAudioCallbackManagerRequestType T>
    struct SAudioCallbackManagerRequestData
        : public SAudioCallbackManagerRequestDataBase
    {
        SAudioCallbackManagerRequestData()
            : SAudioCallbackManagerRequestDataBase(T)
        {}

        ~SAudioCallbackManagerRequestData() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioCallbackManagerRequestData<eACMRT_REPORT_STARTED_EVENT>
        : public SAudioCallbackManagerRequestDataBase
    {
        SAudioCallbackManagerRequestData(TAudioEventID const nPassedEventID)
            : SAudioCallbackManagerRequestDataBase(eACMRT_REPORT_STARTED_EVENT)
            , nEventID(nPassedEventID)
        {}

        ~SAudioCallbackManagerRequestData<eACMRT_REPORT_STARTED_EVENT>() override {}

        TAudioEventID const nEventID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioCallbackManagerRequestData<eACMRT_REPORT_FINISHED_EVENT>
        : public SAudioCallbackManagerRequestDataBase
    {
        SAudioCallbackManagerRequestData(const TAudioEventID nPassedEventID, const bool bPassedSuccess)
            : SAudioCallbackManagerRequestDataBase(eACMRT_REPORT_FINISHED_EVENT)
            , nEventID(nPassedEventID)
            , bSuccess(bPassedSuccess)
        {}

        ~SAudioCallbackManagerRequestData<eACMRT_REPORT_FINISHED_EVENT>()override {}

        const TAudioEventID nEventID;
        const bool bSuccess;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioCallbackManagerRequestData<eACMRT_REPORT_FINISHED_TRIGGER_INSTANCE>
        : public SAudioCallbackManagerRequestDataBase
    {
        SAudioCallbackManagerRequestData(TAudioControlID const nPassedControlID)
            : SAudioCallbackManagerRequestDataBase(eACMRT_REPORT_FINISHED_TRIGGER_INSTANCE)
            , nAudioTriggerID(nPassedControlID)
        {}

        ~SAudioCallbackManagerRequestData<eACMRT_REPORT_FINISHED_TRIGGER_INSTANCE>()override {}

        TAudioControlID const nAudioTriggerID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioCallbackManagerRequestData<eACMRT_REPORT_PROCESSED_OBSTRUCTION_RAY>
        : public SAudioCallbackManagerRequestDataBase
    {
        explicit SAudioCallbackManagerRequestData(const TAudioObjectID nPassedObjectID, const size_t nPassedRayID = (size_t)-1)
            : SAudioCallbackManagerRequestDataBase(eACMRT_REPORT_PROCESSED_OBSTRUCTION_RAY)
            , nObjectID(nPassedObjectID)
            , nRayID(nPassedRayID)
        {}

        ~SAudioCallbackManagerRequestData<eACMRT_REPORT_PROCESSED_OBSTRUCTION_RAY>()override {}

        const TAudioObjectID nObjectID;
        const size_t nRayID;
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Audio Object Requests
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct SAudioObjectRequestDataBase
        : public SAudioRequestDataBase
    {
        explicit SAudioObjectRequestDataBase(const EAudioObjectRequestType ePassedType = eAORT_NONE)
            : SAudioRequestDataBase(eART_AUDIO_OBJECT_REQUEST)
            , eType(ePassedType)
        {}

        ~SAudioObjectRequestDataBase() override {}

        const EAudioObjectRequestType eType;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template <EAudioObjectRequestType T>
    struct SAudioObjectRequestData
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(T)
        {}

        ~SAudioObjectRequestData() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_EXECUTE_TRIGGER>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_EXECUTE_TRIGGER)
            , nTriggerID(INVALID_AUDIO_CONTROL_ID)
            , fTimeUntilRemovalInMS(0.0f)
            , eLipSyncMethod(eLSM_None)
        {}

        SAudioObjectRequestData(const TAudioControlID nPassedTriggerID, const float fPassedTimeUntilRemovalInMS)
            : SAudioObjectRequestDataBase(eAORT_EXECUTE_TRIGGER)
            , nTriggerID(nPassedTriggerID)
            , fTimeUntilRemovalInMS(fPassedTimeUntilRemovalInMS)
            , eLipSyncMethod(eLSM_None)
        {}

        ~SAudioObjectRequestData<eAORT_EXECUTE_TRIGGER>()override {}

        TAudioControlID nTriggerID;
        float fTimeUntilRemovalInMS;
        ELipSyncMethod eLipSyncMethod;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_PREPARE_TRIGGER>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_PREPARE_TRIGGER)
            , nTriggerID(INVALID_AUDIO_CONTROL_ID)
        {}

        explicit SAudioObjectRequestData(const TAudioControlID nPassedTriggerID)
            : SAudioObjectRequestDataBase(eAORT_PREPARE_TRIGGER)
            , nTriggerID(nPassedTriggerID)
        {}

        ~SAudioObjectRequestData<eAORT_PREPARE_TRIGGER>()override {}

        TAudioControlID nTriggerID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_UNPREPARE_TRIGGER>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_UNPREPARE_TRIGGER)
            , nTriggerID(INVALID_AUDIO_CONTROL_ID)
        {}

        explicit SAudioObjectRequestData(const TAudioControlID nPassedTriggerID)
            : SAudioObjectRequestDataBase(eAORT_UNPREPARE_TRIGGER)
            , nTriggerID(nPassedTriggerID)
        {}

        ~SAudioObjectRequestData<eAORT_UNPREPARE_TRIGGER>()override {}

        TAudioControlID nTriggerID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_STOP_TRIGGER>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_STOP_TRIGGER)
            , nTriggerID(INVALID_AUDIO_CONTROL_ID)
        {}

        explicit SAudioObjectRequestData(const TAudioControlID nPassedTriggerID)
            : SAudioObjectRequestDataBase(eAORT_STOP_TRIGGER)
            , nTriggerID(nPassedTriggerID)
        {}

        ~SAudioObjectRequestData<eAORT_STOP_TRIGGER>()override {}

        TAudioControlID nTriggerID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_STOP_ALL_TRIGGERS>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_STOP_ALL_TRIGGERS)
            , m_filterByOwner(false)
        {}

        SAudioObjectRequestData(bool filterByOwner)
            : SAudioObjectRequestDataBase(eAORT_STOP_ALL_TRIGGERS)
            , m_filterByOwner(filterByOwner)
        {}

        ~SAudioObjectRequestData<eAORT_STOP_ALL_TRIGGERS>()override {}

        const bool m_filterByOwner;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_SET_POSITION>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_SET_POSITION)
            , oPosition()
        {}

        explicit SAudioObjectRequestData(const SATLWorldPosition& oPassedPosition)
            : SAudioObjectRequestDataBase(eAORT_SET_POSITION)
            , oPosition(oPassedPosition)
        {}

        ~SAudioObjectRequestData<eAORT_SET_POSITION>()override {}

        SATLWorldPosition oPosition;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_SET_RTPC_VALUE>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_SET_RTPC_VALUE)
            , nControlID(INVALID_AUDIO_CONTROL_ID)
            , fValue(0.0f)
        {}

        SAudioObjectRequestData(const TAudioControlID nPassedControlID, const float fPassedValue)
            : SAudioObjectRequestDataBase(eAORT_SET_RTPC_VALUE)
            , nControlID(nPassedControlID)
            , fValue(fPassedValue)
        {}

        ~SAudioObjectRequestData<eAORT_SET_RTPC_VALUE>()override {}

        TAudioControlID nControlID;
        float fValue;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_SET_SWITCH_STATE>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_SET_SWITCH_STATE)
            , nSwitchID(INVALID_AUDIO_CONTROL_ID)
            , nStateID(INVALID_AUDIO_SWITCH_STATE_ID)
        {}

        SAudioObjectRequestData(const TAudioControlID nPassedControlID, const TAudioSwitchStateID nPassedStateID)
            : SAudioObjectRequestDataBase(eAORT_SET_SWITCH_STATE)
            , nSwitchID(nPassedControlID)
            , nStateID(nPassedStateID)
        {}

        ~SAudioObjectRequestData<eAORT_SET_SWITCH_STATE>()override {}

        TAudioControlID nSwitchID;
        TAudioSwitchStateID nStateID;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_SET_VOLUME>
        : public SAudioObjectRequestDataBase
    {
        explicit SAudioObjectRequestData(const float fPassedVolume = 1.0f)
            : SAudioObjectRequestDataBase(eAORT_SET_VOLUME)
            , fVolume(fPassedVolume)
        {}

        ~SAudioObjectRequestData<eAORT_SET_VOLUME>()override {}

        const float fVolume;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_SET_ENVIRONMENT_AMOUNT>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_SET_ENVIRONMENT_AMOUNT)
            , nEnvironmentID(INVALID_AUDIO_ENVIRONMENT_ID)
            , fAmount(1.0f)
        {}

        SAudioObjectRequestData(const TAudioEnvironmentID nPassedEnvironmentID, const float fPassedAmount)
            : SAudioObjectRequestDataBase(eAORT_SET_ENVIRONMENT_AMOUNT)
            , nEnvironmentID(nPassedEnvironmentID)
            , fAmount(fPassedAmount)
        {}

        ~SAudioObjectRequestData<eAORT_SET_ENVIRONMENT_AMOUNT>()override {}

        TAudioEnvironmentID nEnvironmentID;
        float fAmount;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_RESET_ENVIRONMENTS>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_RESET_ENVIRONMENTS)
        {}

        ~SAudioObjectRequestData<eAORT_RESET_ENVIRONMENTS>()override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_RESET_RTPCS>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_RESET_RTPCS)
        {}

        ~SAudioObjectRequestData<eAORT_RESET_RTPCS>() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_RELEASE_OBJECT>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData()
            : SAudioObjectRequestDataBase(eAORT_RELEASE_OBJECT)
        {}

        ~SAudioObjectRequestData<eAORT_RELEASE_OBJECT>()override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioObjectRequestData<eAORT_EXECUTE_SOURCE_TRIGGER>
        : public SAudioObjectRequestDataBase
    {
        SAudioObjectRequestData(TAudioControlID triggerId, TAudioSourceId sourceId)
            : SAudioObjectRequestDataBase(eAORT_EXECUTE_SOURCE_TRIGGER)
            , m_triggerId(triggerId)
            , m_sourceId(sourceId)
        {}

        ~SAudioObjectRequestData<eAORT_EXECUTE_SOURCE_TRIGGER>() override {}

        TAudioControlID m_triggerId;
        TAudioSourceId m_sourceId;
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Audio Listener Requests
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct SAudioListenerRequestDataBase
        : public SAudioRequestDataBase
    {
        explicit SAudioListenerRequestDataBase(const EAudioListenerRequestType ePassedType = eALRT_NONE)
            : SAudioRequestDataBase(eART_AUDIO_LISTENER_REQUEST)
            , eType(ePassedType)
        {}

        ~SAudioListenerRequestDataBase() override {}

        const EAudioListenerRequestType eType;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<EAudioListenerRequestType T>
    struct SAudioListenerRequestData
        : public SAudioListenerRequestDataBase
    {
        SAudioListenerRequestData()
            : SAudioListenerRequestDataBase(T)
        {}

        ~SAudioListenerRequestData() override {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct SAudioListenerRequestData<eALRT_SET_POSITION>
        : public SAudioListenerRequestDataBase
    {
        SAudioListenerRequestData()
            : SAudioListenerRequestDataBase(eALRT_SET_POSITION)
        {}

        explicit SAudioListenerRequestData(const SATLWorldPosition& oWorldPosition)
            : SAudioListenerRequestDataBase(eALRT_SET_POSITION)
            , oNewPosition(oWorldPosition)
        {}

        ~SAudioListenerRequestData<eALRT_SET_POSITION>()override {}

        SATLWorldPosition oNewPosition;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct SAudioSystemInfo
    {
        SAudioSystemInfo()
            : nCountUsedAudioTriggers(0)
            , nCountUnusedAudioTriggers(0)
            , nCountUsedAudioEvents(0)
            , nCountUnusedAudioEvents(0)
        {}

        size_t nCountUsedAudioTriggers;
        size_t nCountUnusedAudioTriggers;
        size_t nCountUsedAudioEvents;
        size_t nCountUnusedAudioEvents;

        Vec3 oListenerPos;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct IAudioProxy
    {
        virtual ~IAudioProxy() = default;

        virtual void Initialize(const char* const sObjectName, const bool bInitAsync = true) = 0;
        virtual void Release() = 0;
        virtual void Reset() = 0;
        virtual void ExecuteTrigger(const TAudioControlID nTriggerID, const ELipSyncMethod eLipSyncMethod, const SAudioCallBackInfos& rCallbackInfos = SAudioCallBackInfos::GetEmptyObject()) = 0;
        virtual void StopAllTriggers() = 0;
        virtual void StopTrigger(const TAudioControlID nTriggerID) = 0;
        virtual void SetSwitchState(const TAudioControlID nSwitchID, const TAudioSwitchStateID nStateID) = 0;
        virtual void SetRtpcValue(const TAudioControlID nRtpcID, const float fValue) = 0;
        virtual void SetObstructionCalcType(const EAudioObjectObstructionCalcType eObstructionType) = 0;
        virtual void SetPosition(const SATLWorldPosition& rPosition) = 0;
        virtual void SetPosition(const Vec3& rPosition) = 0;
        virtual void SetEnvironmentAmount(const TAudioEnvironmentID nEnvironmentID, const float fAmount) = 0;
        virtual void SetCurrentEnvironments(const EntityId nEntityToIgnore = 0) = 0;
        virtual void SetLipSyncProvider(ILipSyncProvider* const pILipSyncProvider) = 0;
        virtual void ResetRtpcValues() = 0;
        virtual TAudioObjectID GetAudioObjectID() const = 0;
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    class AudioSystemRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~AudioSystemRequests() = default;

        ///////////////////////////////////////////////////////////////////////////////////////////////
        // EBusTraits - Single Bus Address, Single Handler
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        ///////////////////////////////////////////////////////////////////////////////////////////////

        virtual bool Initialize() = 0;
        virtual void Release() = 0;
        virtual void ExternalUpdate() = 0;

        virtual void PushRequest(const SAudioRequest& rAudioRequestData) = 0;
        virtual void PushRequestBlocking(const SAudioRequest& audioRequestData) = 0;

        virtual void AddRequestListener(
            AudioRequestCallbackType callBack,
            void* const objectToListenTo,
            const EAudioRequestType requestType = eART_AUDIO_ALL_REQUESTS,
            const TATLEnumFlagsType specificRequestMask = ALL_AUDIO_REQUEST_SPECIFIC_TYPE_FLAGS) = 0;
        virtual void RemoveRequestListener(
            AudioRequestCallbackType callBack,
            void* const requestOwner) = 0;

        virtual TAudioControlID GetAudioTriggerID(const char* const sAudioTriggerName) const = 0;
        virtual TAudioControlID GetAudioRtpcID(const char* const sAudioRtpcName) const = 0;
        virtual TAudioControlID GetAudioSwitchID(const char* const sAudioSwitchName) const = 0;
        virtual TAudioSwitchStateID GetAudioSwitchStateID(const TAudioControlID nSwitchID, const char* const sAudioSwitchStateName) const = 0;
        virtual TAudioPreloadRequestID GetAudioPreloadRequestID(const char* const sAudioPreloadRequestName) const = 0;
        virtual TAudioEnvironmentID GetAudioEnvironmentID(const char* const sAudioEnvironmentName) const = 0;

        virtual bool ReserveAudioListenerID(TAudioObjectID& rAudioObjectID) = 0;
        virtual bool ReleaseAudioListenerID(const TAudioObjectID nAudioObjectID) = 0;
        virtual bool SetAudioListenerOverrideID(const TAudioObjectID nAudioObjectID) = 0;

        virtual void GetInfo(SAudioSystemInfo& rAudioSystemInfo) = 0;
        virtual const char* GetControlsPath() const = 0;
        virtual void UpdateControlsPath() = 0;

        virtual IAudioProxy* GetFreeAudioProxy() = 0;
        virtual void FreeAudioProxy(IAudioProxy* const pIAudioProxy) = 0;

        virtual TAudioSourceId CreateAudioSource(const SAudioInputConfig& sourceConfig) = 0;
        virtual void DestroyAudioSource(TAudioSourceId sourceId) = 0;

        virtual const char* GetAudioControlName(const EAudioControlType controlType, const TATLIDType atlID) const = 0;
        virtual const char* GetAudioSwitchStateName(const TAudioControlID switchID, const TAudioSwitchStateID stateID) const = 0;

        virtual void OnCVarChanged(ICVar* const pCVar) = 0;
    };

    using AudioSystemRequestBus = AZ::EBus<AudioSystemRequests>;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    class AudioSystemThreadSafeRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~AudioSystemThreadSafeRequests() = default;

        ///////////////////////////////////////////////////////////////////////////////////////////////
        // EBusTraits - Single Bus Address, Single Handler, Mutex, Queued
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const bool EnableEventQueue = true;
        using MutexType = AZStd::mutex;
        ///////////////////////////////////////////////////////////////////////////////////////////////

        virtual void PushRequestThreadSafe(const SAudioRequest& audioRequestData) = 0;
    };

    using AudioSystemThreadSafeRequestBus = AZ::EBus<AudioSystemThreadSafeRequests>;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    struct IAudioSystem
        : public AZ::Component
        , public AudioSystemRequestBus::Handler
        , public AudioSystemThreadSafeRequestBus::Handler
    {
        ~IAudioSystem() override = default;
    };

} // namespace Audio
