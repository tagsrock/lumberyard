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

// TODO - Determine if this code is deprecated. A CVar closely tied to its use was removed

#ifndef CRYINCLUDE_CRYMOVIE_MOVIE_H
#define CRYINCLUDE_CRYMOVIE_MOVIE_H

#pragma once

#include "IMovieSystem.h"
#include <CrySizer.h>

struct PlayingSequence
{
    //! Sequence playing
    _smart_ptr<IAnimSequence> sequence;

    //! Start/End/Current playing time for this sequence.
    float startTime;
    float endTime;
    float currentTime;
    float currentSpeed;

    //! Sequence from other sequence's sequence track
    bool trackedSequence;
    bool bSingleFrame;

    void GetMemoryUsage(ICrySizer* pSizer) const
    {
        pSizer->AddObject(sequence);
    }
};

class CLightAnimWrapper
    : public ILightAnimWrapper
{
public:
    // ILightAnimWrapper interface
    virtual bool Resolve();

public:
    static CLightAnimWrapper* Create(const char* name);
    static void ReconstructCache();
    static IAnimSequence* GetLightAnimSet();
    static void SetLightAnimSet(IAnimSequence* pLightAnimSet);
    static void InvalidateAllNodes();

private:
    typedef std::map<string, CLightAnimWrapper*> LightAnimWrapperCache;
    static LightAnimWrapperCache ms_lightAnimWrapperCache;
    static _smart_ptr<IAnimSequence> ms_pLightAnimSet;

private:
    static CLightAnimWrapper* FindLightAnim(const char* name);
    static void CacheLightAnim(const char* name, CLightAnimWrapper* p);
    static void RemoveCachedLightAnim(const char* name);

private:
    CLightAnimWrapper(const char* name);
    virtual ~CLightAnimWrapper();
};

struct IConsoleCmdArgs;

struct ISkeletonAnim;

class CMovieSystem
    : public IMovieSystem
{
    typedef std::vector<PlayingSequence> PlayingSequences;

public:
    CMovieSystem(ISystem* system);

    void Release() { delete this; };

    void SetUser(IMovieUser* pUser) { m_pUser = pUser; }
    IMovieUser* GetUser() { return m_pUser; }

    bool Load(const char* pszFile, const char* pszMission);

    ISystem* GetSystem() { return m_pSystem; }

    IAnimTrack* CreateTrack(EAnimCurveType type);

    IAnimSequence* CreateSequence(const char* sequence, bool bLoad = false, uint32 id = 0, ESequenceType = eSequenceType_Legacy);
    IAnimSequence* LoadSequence(const char* pszFilePath);
    IAnimSequence* LoadSequence(XmlNodeRef& xmlNode, bool bLoadEmpty = true);

    void AddSequence(IAnimSequence* pSequence);
    void RemoveSequence(IAnimSequence* pSequence);
    IAnimSequence* FindSequence(const char* sequence) const override;
    IAnimSequence* FindSequence(const AZ::EntityId& componentEntitySequenceId) const override;
    IAnimSequence* FindSequenceById(uint32 id) const override;
    IAnimSequence* GetSequence(int i) const;
    int GetNumSequences() const;
    IAnimSequence* GetPlayingSequence(int i) const;
    int GetNumPlayingSequences() const;
    bool IsCutScenePlaying() const;

    uint32 GrabNextSequenceId() override
    { return m_nextSequenceId++; }
    void OnSetSequenceId(uint32 sequenceId) override;

    int OnSequenceRenamed(const char* before, const char* after);
    int OnCameraRenamed(const char* before, const char* after);

    bool AddMovieListener(IAnimSequence* pSequence, IMovieListener* pListener);
    bool RemoveMovieListener(IAnimSequence* pSequence, IMovieListener* pListener);

    void RemoveAllSequences();

    //////////////////////////////////////////////////////////////////////////
    // Sequence playback.
    //////////////////////////////////////////////////////////////////////////
    void PlaySequence(const char* sequence, IAnimSequence* parentSeq = NULL, bool bResetFX = true,
        bool bTrackedSequence = false, float startTime = -FLT_MAX, float endTime = -FLT_MAX);
    void PlaySequence(IAnimSequence* seq, IAnimSequence* parentSeq = NULL, bool bResetFX = true,
        bool bTrackedSequence = false, float startTime = -FLT_MAX, float endTime = -FLT_MAX);
    void PlayOnLoadSequences();

    bool StopSequence(const char* sequence);
    bool StopSequence(IAnimSequence* seq);
    bool AbortSequence(IAnimSequence* seq, bool bLeaveTime = false);

    void StopAllSequences();
    void StopAllCutScenes();
    void Pause(bool bPause);

    void Reset(bool bPlayOnReset, bool bSeekToStart);
    void StillUpdate();
    void PreUpdate(const float dt);
    void PostUpdate(const float dt);
    void Render();

    void StartCapture(const ICaptureKey& key);
    void EndCapture();
    void ControlCapture();

    bool IsPlaying(IAnimSequence* seq) const;

    void Pause();
    void Resume();

    virtual void PauseCutScenes();
    virtual void ResumeCutScenes();

    void SetRecording(bool recording) { m_bRecording = recording; };
    bool IsRecording() const { return m_bRecording; };

    void EnableCameraShake(bool bEnabled){ m_bEnableCameraShake = bEnabled; };
    bool IsCameraShakeEnabled() const {return m_bEnableCameraShake; };

    void SetCallback(IMovieCallback* pCallback) { m_pCallback = pCallback; }
    IMovieCallback* GetCallback() { return m_pCallback; }
    void Callback(IMovieCallback::ECallbackReason Reason, IAnimNode* pNode);

    void Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bRemoveOldNodes = false, bool bLoadEmpty = true);

    const SCameraParams& GetCameraParams() const { return m_ActiveCameraParams; }
    void SetCameraParams(const SCameraParams& Params);

    void SendGlobalEvent(const char* pszEvent);
    void SetSequenceStopBehavior(ESequenceStopBehavior behavior);
    IMovieSystem::ESequenceStopBehavior GetSequenceStopBehavior();

    float GetPlayingTime(IAnimSequence* pSeq);
    bool SetPlayingTime(IAnimSequence* pSeq, float fTime);

    float GetPlayingSpeed(IAnimSequence* pSeq);
    bool SetPlayingSpeed(IAnimSequence* pSeq, float fTime);

    bool GetStartEndTime(IAnimSequence* pSeq, float& fStartTime, float& fEndTime);
    bool SetStartEndTime(IAnimSequence* pSeq, const float fStartTime, const float fEndTime);

    void GoToFrame(const char* seqName, float targetFrame);

    const char* GetOverrideCamName() const
    { return m_mov_overrideCam->GetString(); }

    virtual bool IsPhysicsEventsEnabled() const { return m_bPhysicsEventsEnabled; }
    virtual void EnablePhysicsEvents(bool enable) { m_bPhysicsEventsEnabled = enable; }

    virtual void EnableBatchRenderMode(bool bOn) { m_bBatchRenderMode = bOn; }
    virtual bool IsInBatchRenderMode() const { return m_bBatchRenderMode; }

    int GetEntityNodeParamCount() const;
    CAnimParamType GetEntityNodeParamType(int index) const;
    const char* GetEntityNodeParamName(int index) const;
    IAnimNode::ESupportedParamFlags GetEntityNodeParamFlags(int index) const;

    ILightAnimWrapper* CreateLightAnimWrapper(const char* name) const;

    void GetMemoryUsage(ICrySizer* pSizer) const
    {
        pSizer->AddObject(this, sizeof(*this));
        pSizer->AddObject(m_sequences);
        pSizer->AddObject(m_playingSequences);
        pSizer->AddObject(m_movieListenerMap);
    }

    void SerializeNodeType(EAnimNodeType& animNodeType, XmlNodeRef& xmlNode, bool bLoading, const uint version, int flags) override;
    virtual void SerializeParamType(CAnimParamType& animParamType, XmlNodeRef& xmlNode, bool bLoading, const uint version);

    static const char* GetParamTypeName(const CAnimParamType& animParamType);

    void OnCameraCut();

    void LogUserNotificationMsg(const AZStd::string& msg) override;
    void ClearUserNotificationMsgs() override;
    const AZStd::string& GetUserNotificationMsgs() const override;

private:
    void NotifyListeners(IAnimSequence* pSequence, IMovieListener::EMovieEvent event);

    void InternalStopAllSequences(bool bIsAbort, bool bAnimate);
    bool InternalStopSequence(IAnimSequence* seq, bool bIsAbort, bool bAnimate);

    bool FindSequence(IAnimSequence* sequence, PlayingSequences::const_iterator& sequenceIteratorOut) const;
    bool FindSequence(IAnimSequence* sequence, PlayingSequences::iterator& sequenceIteratorOut);

#if !defined(_RELEASE)
    static void GoToFrameCmd(IConsoleCmdArgs* pArgs);
    static void ListSequencesCmd(IConsoleCmdArgs* pArgs);
    static void PlaySequencesCmd(IConsoleCmdArgs* pArgs);
    AZStd::string m_notificationLogMsgs;     // buffer to hold movie user warnings, errors and notifications for the editor.
#endif

    void DoNodeStaticInitialisation();
    void UpdateInternal(const float dt, const bool bPreUpdate);

#ifdef MOVIESYSTEM_SUPPORT_EDITING
    virtual EAnimNodeType GetNodeTypeFromString(const char* pString) const;
    virtual CAnimParamType GetParamTypeFromString(const char* pString) const;
#endif

    ISystem* m_pSystem;

    IMovieUser* m_pUser;
    IMovieCallback* m_pCallback;

    CTimeValue m_lastUpdateTime;

    typedef std::vector<_smart_ptr<IAnimSequence> > Sequences;
    Sequences m_sequences;

    PlayingSequences m_playingSequences;

    typedef std::vector<IMovieListener*> TMovieListenerVec;
    typedef std::map<IAnimSequence*, TMovieListenerVec> TMovieListenerMap;

    // a container which maps sequences to all interested listeners
    // listeners is a vector (could be a set in case we have a lot of listeners, stl::push_back_unique!)
    TMovieListenerMap m_movieListenerMap;

    bool    m_bRecording;
    bool    m_bPaused;
    bool    m_bCutscenesPausedInEditor;
    bool    m_bEnableCameraShake;

    SCameraParams m_ActiveCameraParams;

    ESequenceStopBehavior m_sequenceStopBehavior;

    bool m_bStartCapture;
    bool m_bEndCapture;
    ICaptureKey m_captureKey;
    float m_fixedTimeStepBackUp;
    ICVar* m_cvar_capture_file_format;
    ICVar* m_cvar_capture_frame_once;
    ICVar* m_cvar_capture_folder;
    ICVar* m_cvar_t_FixedStep;
    ICVar* m_cvar_capture_frames;
    ICVar* m_cvar_capture_file_prefix;
    ICVar* m_cvar_capture_buffer;

    static int m_mov_NoCutscenes;
    ICVar* m_mov_overrideCam;

    bool m_bPhysicsEventsEnabled;

    bool m_bBatchRenderMode;

    // next available sequenceId
    uint32 m_nextSequenceId;

    void ShowPlayedSequencesDebug();

public:
    static float m_mov_cameraPrecacheTime;
#if !defined(_RELEASE)
    static int m_mov_DebugEvents;
    static int m_mov_debugCamShake;
#endif
};

#endif // CRYINCLUDE_CRYMOVIE_MOVIE_H
