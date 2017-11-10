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
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzFramework/Components/CameraBus.h>

#include "Components/IComponentCamera.h"
#include "MathConversion.h"
#include "SceneNode.h"
#include "AnimSequence.h"
#include "AnimTrack.h"
#include "EventTrack.h"
#include "ConsoleTrack.h"
#include "MusicTrack.h"
#include "SequenceTrack.h"
#include "GotoTrack.h"
#include "CaptureTrack.h"
#include "ISystem.h"
#include "ITimer.h"
#include "AnimCameraNode.h"
#include "AnimAZEntityNode.h"
#include "AnimComponentNode.h"
#include "Movie.h"

#include <AzCore/Math/MathUtils.h>

#include <IAudioSystem.h>
#include <IConsole.h>

#define s_nodeParamsInitialized s_nodeParamsInitializedScene
#define s_nodeParams s_nodeParamsSene
#define AddSupportedParam AddSupportedParamScene

float const kDefaultCameraFOV = 60.0f;

namespace {
    bool s_nodeParamsInitialized = false;
    std::vector<CAnimNode::SParamInfo> s_nodeParams;

    void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType, int flags = 0)
    {
        CAnimNode::SParamInfo param;
        param.name = sName;
        param.paramType = paramId;
        param.valueType = valueType;
        param.flags = (IAnimNode::ESupportedParamFlags)flags;
        s_nodeParams.push_back(param);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Legacy Camera and Component Entity Camera Implementations of ISceneCamera
    class CLegacySceneCamera
        : public CAnimSceneNode::ISceneCamera
    {
    public:
        CLegacySceneCamera(IEntity* legacyCameraEntity)
            : m_camera(legacyCameraEntity) {}

        virtual ~CLegacySceneCamera() = default;

        const Vec3& GetPosition() const override
        {
            return m_camera->GetPos();
        }
        const Quat& GetRotation() const override
        {
            return m_camera->GetRotation();
        }
        void SetPosition(const Vec3& localPosition) override
        {
            m_camera->SetPos(localPosition);
        }
        void SetRotation(const Quat& localRotation) override
        {
            m_camera->SetRotation(localRotation);
        }
        float GetFoV() const
        {
            return m_camera->GetComponent<IComponentCamera>()
                ? RAD2DEG(m_camera->GetComponent<IComponentCamera>()->GetCamera().GetFov())
                : RAD2DEG(DEFAULT_FOV);
        }
        float GetNearZ() const
        {
            return m_camera->GetComponent<IComponentCamera>()
                ? m_camera->GetComponent<IComponentCamera>()->GetCamera().GetNearPlane()
                : DEFAULT_NEAR;
        }
        void SetNearZAndFOVIfChanged(float fov, float nearZ) override
        {
            IComponentCameraPtr firstCameraComponent = m_camera->GetComponent<IComponentCamera>();
            CCamera& cam = firstCameraComponent->GetCamera();
            if (!AZ::IsClose(cam.GetFov(), fov, RAD_EPSILON) || !AZ::IsClose(cam.GetNearPlane(), nearZ, FLT_EPSILON))
            {
                cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), fov, nearZ, cam.GetFarPlane(), cam.GetPixelAspectRatio());
                firstCameraComponent->SetCamera(cam);
            }
        }
        void TransformPositionFromLocalToWorldSpace(Vec3& position) override
        {
            if (m_camera->GetParent())
            {
                position = m_camera->GetParent()->GetWorldTM() * position;
            }
        }
        void TransformPositionFromWorldToLocalSpace(Vec3& position) override
        {
            if (m_camera->GetParent())
            {
                Matrix34 m = m_camera->GetParent()->GetWorldTM();
                m = m.GetInverted();
                position = m * position;
            }
        }
        void TransformRotationFromLocalToWorldSpace(Quat& rotation) override
        {
            if (m_camera->GetParent())
            {
                rotation = m_camera->GetParent()->GetWorldRotation() * rotation;
            }
        }
        void SetWorldRotation(const Quat& rotation) override
        {
            if (m_camera->GetParent())
            {
                Matrix34 m = m_camera->GetWorldTM();
                m.SetRotationXYZ(Ang3(rotation));
                m.SetTranslation(m_camera->GetWorldTM().GetTranslation());
                m_camera->SetWorldTM(m);
            }
            else
            {
                SetRotation(rotation);
            }
        }
        bool HasParent() const override
        {
            return (m_camera->GetParent() != nullptr);
        }
    private:
        IEntity*    m_camera;
    };

    class CComponentEntitySceneCamera
        : public CAnimSceneNode::ISceneCamera
    {
    public:
        CComponentEntitySceneCamera(const AZ::EntityId& entityId)
            : m_cameraEntityId(entityId) {}

        virtual ~CComponentEntitySceneCamera() = default;

        const Vec3& GetPosition() const override
        {
            AZ::Vector3 pos;
            AZ::TransformBus::EventResult(pos, m_cameraEntityId, &AZ::TransformBus::Events::GetWorldTranslation);
            m_vec3Buffer.Set(pos.GetX(), pos.GetY(), pos.GetZ());
            return m_vec3Buffer;
        }
        const Quat& GetRotation() const override
        {
            AZ::Quaternion quat(AZ::Quaternion::CreateIdentity());
            AZ::TransformBus::EventResult(quat, m_cameraEntityId, &AZ::TransformBus::Events::GetRotationQuaternion);
            m_quatBuffer = AZQuaternionToLYQuaternion(quat);
            return m_quatBuffer;
        }
        void SetPosition(const Vec3& localPosition) override
        {
            AZ::Vector3 pos(localPosition.x, localPosition.y, localPosition.z);
            AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformBus::Events::SetWorldTranslation, pos);
        }
        void SetRotation(const Quat& localRotation) override
        {
            AZ::Quaternion quat = LYQuaternionToAZQuaternion(localRotation);
            AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformBus::Events::SetRotationQuaternion, quat);
        }
        float GetFoV() const
        {
            float retFoV = DEFAULT_FOV;
            Camera::CameraRequestBus::EventResult(retFoV, m_cameraEntityId, &Camera::CameraComponentRequests::GetFov);
            return retFoV;
        }
        float GetNearZ() const
        {
            float retNearZ = DEFAULT_NEAR;
            Camera::CameraRequestBus::EventResult(retNearZ, m_cameraEntityId, &Camera::CameraComponentRequests::GetNearClipDistance);
            return retNearZ;
        }
        void SetNearZAndFOVIfChanged(float fov, float nearZ) override
        {
            float degFoV = AZ::RadToDeg(fov);
            if (!AZ::IsClose(GetFoV(), degFoV, FLT_EPSILON))
            {
                Camera::CameraRequestBus::Event(m_cameraEntityId, &Camera::CameraComponentRequests::SetFov, degFoV);
            }
            if (!AZ::IsClose(GetNearZ(), nearZ, FLT_EPSILON))
            {
                Camera::CameraRequestBus::Event(m_cameraEntityId, &Camera::CameraComponentRequests::SetNearClipDistance, nearZ);
            }
        }
        void TransformPositionFromLocalToWorldSpace(Vec3& position) override
        {
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, m_cameraEntityId, &AZ::TransformBus::Events::GetParentId);
            if (parentId.IsValid())
            {
                AZ::Vector3 pos(position.x, position.y, position.z);
                AZ::Transform worldTM;
                AZ::TransformBus::EventResult(worldTM, parentId, &AZ::TransformBus::Events::GetWorldTM);
                pos = worldTM * pos;
                position.Set(pos.GetX(), pos.GetY(), pos.GetZ());
            }
        }
        void TransformPositionFromWorldToLocalSpace(Vec3& position) override
        {
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, m_cameraEntityId, &AZ::TransformBus::Events::GetParentId);
            if (parentId.IsValid())
            {
                AZ::Vector3 pos(position.x, position.y, position.z);
                AZ::Transform worldTM;
                AZ::TransformBus::EventResult(worldTM, parentId, &AZ::TransformBus::Events::GetWorldTM);
                worldTM = worldTM.GetInverseFast();
                pos = worldTM * pos;
                position.Set(pos.GetX(), pos.GetY(), pos.GetZ());
            }
        }
        void TransformRotationFromLocalToWorldSpace(Quat& rotation) override
        {
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, m_cameraEntityId, &AZ::TransformBus::Events::GetParentId);
            if (parentId.IsValid())
            {
                AZ::Quaternion rot = LYQuaternionToAZQuaternion(rotation);
                AZ::Transform worldTM;
                AZ::TransformBus::EventResult(worldTM, parentId, &AZ::TransformBus::Events::GetWorldTM);
                AZ::Quaternion worldRot = AZ::Quaternion::CreateFromTransform(worldTM);
                rot = worldRot * rot;
                rotation = AZQuaternionToLYQuaternion(rot);
            }
        }
        void SetWorldRotation(const Quat& rotation) override
        {
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, m_cameraEntityId, &AZ::TransformBus::Events::GetParentId);
            if (parentId.IsValid())
            {
                AZ::Quaternion rot = LYQuaternionToAZQuaternion(rotation);
                AZ::Transform parentWorldTM;
                AZ::Transform worldTM;
                AZ::TransformBus::EventResult(parentWorldTM, parentId, &AZ::TransformBus::Events::GetWorldTM);
                AZ::TransformBus::EventResult(worldTM, m_cameraEntityId, &AZ::TransformBus::Events::GetWorldTM);
                parentWorldTM.SetRotationPartFromQuaternion(rot);
                parentWorldTM.SetTranslation(worldTM.GetTranslation());
                AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformBus::Events::SetWorldTM, parentWorldTM);
            }
            else
            {
                SetRotation(rotation);
            }
        }
        bool HasParent() const override
        {
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, m_cameraEntityId, &AZ::TransformBus::Events::GetParentId);
            return parentId.IsValid();
        }
    private:
        AZ::EntityId    m_cameraEntityId;
        mutable Vec3    m_vec3Buffer;       // buffer for returning references
        mutable Quat    m_quatBuffer;       // buffer for returning references
    };
}

//////////////////////////////////////////////////////////////////////////
CAnimSceneNode::CAnimSceneNode(const int id)
    : CAnimNode(id)
{
    m_lastCameraKey = -1;
    m_lastEventKey = -1;
    m_lastConsoleKey = -1;
    m_lastMusicKey = -1;
    m_lastSequenceKey = -1;
    m_nLastGotoKey = -1;
    m_lastCaptureKey = -1;
    m_bLastCapturingEnded = true;
    m_legacyCurrentCameraEntityId = INVALID_ENTITYID;
    m_cvar_t_FixedStep = NULL;
    m_pCamNodeOnHoldForInterp = 0;
    m_CurrentSelectTrack = 0;
    m_CurrentSelectTrackKeyNumber = 0;
    m_lastPrecachePoint = -1.f;
    SetName("Scene");

    CAnimSceneNode::Initialize();

    SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
}

//////////////////////////////////////////////////////////////////////////
CAnimSceneNode::~CAnimSceneNode()
{
    ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Initialize()
{
    if (!s_nodeParamsInitialized)
    {
        s_nodeParamsInitialized = true;
        s_nodeParams.reserve(9);
        AddSupportedParam("Camera", eAnimParamType_Camera, eAnimValue_Select);
        AddSupportedParam("Event", eAnimParamType_Event, eAnimValue_Unknown);
        AddSupportedParam("Sound", eAnimParamType_Sound, eAnimValue_Unknown);
        AddSupportedParam("Sequence", eAnimParamType_Sequence, eAnimValue_Unknown);
        AddSupportedParam("Console", eAnimParamType_Console, eAnimValue_Unknown);
        AddSupportedParam("Music", eAnimParamType_Music, eAnimValue_Unknown);
        AddSupportedParam("GoTo", eAnimParamType_Goto, eAnimValue_DiscreteFloat);
        AddSupportedParam("Capture", eAnimParamType_Capture, eAnimValue_Unknown);
        AddSupportedParam("Timewarp", eAnimParamType_TimeWarp, eAnimValue_Float);
        AddSupportedParam("FixedTimeStep", eAnimParamType_FixedTimeStep, eAnimValue_Float);
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::CreateDefaultTracks()
{
    CreateTrack(eAnimParamType_Camera);
};

//////////////////////////////////////////////////////////////////////////
unsigned int CAnimSceneNode::GetParamCount() const
{
    return s_nodeParams.size();
}

//////////////////////////////////////////////////////////////////////////
CAnimParamType CAnimSceneNode::GetParamType(unsigned int nIndex) const
{
    if (nIndex >= 0 && nIndex < (int)s_nodeParams.size())
    {
        return s_nodeParams[nIndex].paramType;
    }

    return eAnimParamType_Invalid;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimSceneNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
    for (int i = 0; i < (int)s_nodeParams.size(); i++)
    {
        if (s_nodeParams[i].paramType == paramId)
        {
            info = s_nodeParams[i];
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Activate(bool bActivate)
{
    CAnimNode::Activate(bActivate);

    int trackCount = NumTracks();
    for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
    {
        CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
        IAnimTrack* pTrack = m_tracks[paramIndex];

        if (paramId.GetType() != eAnimParamType_Sequence)
        {
            continue;
        }

        CSequenceTrack* pSequenceTrack = (CSequenceTrack*)pTrack;

        for (int currKey = 0; currKey < pSequenceTrack->GetNumKeys(); currKey++)
        {
            ISequenceKey key;

            pSequenceTrack->GetKey(currKey, &key);

            IAnimSequence* pSequence = GetMovieSystem()->FindSequence(key.szSelection);
            if (pSequence)
            {
                if (bActivate)
                {
                    pSequence->Activate();

                    if (key.bOverrideTimes)
                    {
                        key.fDuration = (key.fEndTime - key.fStartTime) > 0.0f ? (key.fEndTime - key.fStartTime) : 0.0f;
                    }
                    else
                    {
                        key.fDuration = pSequence->GetTimeRange().Length();
                    }

                    pTrack->SetKey(currKey, &key);
                }
                else
                {
                    pSequence->Deactivate();
                }
            }
        }

        if (m_cvar_t_FixedStep == NULL)
        {
            m_cvar_t_FixedStep = gEnv->pConsole->GetCVar("t_FixedStep");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::Animate(SAnimContext& ec)
{
    if (ec.bResetting)
    {
        return;
    }

    CSelectTrack* cameraTrack = NULL;
    CEventTrack* pEventTrack = NULL;
    CSequenceTrack* pSequenceTrack = NULL;
    CConsoleTrack* pConsoleTrack = NULL;
    CMusicTrack* pMusicTrack = NULL;
    CGotoTrack* pGotoTrack = NULL;
    CCaptureTrack* pCaptureTrack = NULL;
    /*
    bool bTimeJump = false;
    if (ec.time < m_time)
        bTimeJump = true;
    */

    int nCurrentSoundTrackIndex = 0;

    if (gEnv->IsEditor() && m_time > ec.time)
    {
        m_lastPrecachePoint = -1.f;
    }

    PrecacheDynamic(ec.time);

    size_t nNumAudioTracks = 0;
    int trackCount = NumTracks();
    for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
    {
        CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
        IAnimTrack* pTrack = m_tracks[paramIndex];

        if (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled)
        {
            continue;
        }

        if (pTrack->IsMasked(ec.trackMask))
        {
            continue;
        }

        switch (paramId.GetType())
        {
        case eAnimParamType_Camera:
            cameraTrack = (CSelectTrack*)pTrack;
            break;
        case eAnimParamType_Event:
            pEventTrack = (CEventTrack*)pTrack;
            break;
        case eAnimParamType_Sequence:
            pSequenceTrack = (CSequenceTrack*)pTrack;
            break;
        case eAnimParamType_Console:
            pConsoleTrack = (CConsoleTrack*)pTrack;
            break;
        case eAnimParamType_Music:
            pMusicTrack = (CMusicTrack*)pTrack;
            break;
        case eAnimParamType_Capture:
            pCaptureTrack = (CCaptureTrack*)pTrack;
            break;
        case eAnimParamType_Goto:
            pGotoTrack = (CGotoTrack*)pTrack;
            break;

        case eAnimParamType_Sound:

            ++nNumAudioTracks;
            if (nNumAudioTracks > m_SoundInfo.size())
            {
                m_SoundInfo.resize(nNumAudioTracks);
            }

            AnimateSound(m_SoundInfo, ec, pTrack, nNumAudioTracks);

            break;

        case eAnimParamType_TimeWarp:
        {
            float timeScale = 1.0f;
            pTrack->GetValue(ec.time, timeScale);
            if (timeScale < 0)
            {
                timeScale = 0;
            }
            float fixedTimeStep = 0;
            if (GetSequence()->GetFlags() & IAnimSequence::eSeqFlags_CanWarpInFixedTime)
            {
                fixedTimeStep = GetSequence()->GetFixedTimeStep();
            }
            if (fixedTimeStep == 0)
            {
                if (m_cvar_t_FixedStep && m_cvar_t_FixedStep->GetFVal() != 0)
                {
                    m_cvar_t_FixedStep->Set(0.0f);
                }
                gEnv->pTimer->SetTimeScale(timeScale, ITimer::eTSC_Trackview);
            }
            else if (m_cvar_t_FixedStep)
            {
                m_cvar_t_FixedStep->Set(fixedTimeStep * timeScale);
            }
        }
        break;
        case eAnimParamType_FixedTimeStep:
        {
            float timeStep = 0;
            pTrack->GetValue(ec.time, timeStep);
            if (timeStep < 0)
            {
                timeStep = 0;
            }
            if (m_cvar_t_FixedStep)
            {
                m_cvar_t_FixedStep->Set(timeStep);
            }
        }
        break;
        }
    }

    // Animate Camera Track (aka Select Track)
    
    // Check if a camera override is set by CVar
    const char* overrideCamName = gEnv->pMovieSystem->GetOverrideCamName();
    AZ::EntityId overrideCamId;
    if (overrideCamName != 0 && strlen(overrideCamName) > 0)
    {
        // overriding with a Camera Component entity is done by entityId (as names are not unique among AZ::Entities) - try to convert string to u64 to see if it's an id
        AZ::u64 u64Id = strtoull(overrideCamName, nullptr, /*base (radix)*/ 10);
        if (u64Id)
        {
            overrideCamId = AZ::EntityId(u64Id);
        }
        else
        {
            // search for legacy camera object by name
            IEntity* overrideCamEntity = gEnv->pEntitySystem->FindEntityByName(overrideCamName);
            if (overrideCamEntity)
            {
                overrideCamId = AZ::EntityId(overrideCamEntity->GetId());
            }
        }
    }

    if (overrideCamId.IsValid())      // There is a valid overridden camera.
    {
        if (overrideCamId != gEnv->pMovieSystem->GetCameraParams().cameraEntityId)
        {
            ISelectKey key;
            cry_strcpy(key.szSelection, overrideCamName);
            key.cameraAzEntityId = overrideCamId;
            ApplyCameraKey(key, ec);
        }
    }
    else if (cameraTrack)           // No camera override by CVar, use the camera track
    {
        ISelectKey key;
        int cameraKey = cameraTrack->GetActiveKey(ec.time, &key);
        m_CurrentSelectTrackKeyNumber = cameraKey;
        m_CurrentSelectTrack = cameraTrack;
        ApplyCameraKey(key, ec);
        m_lastCameraKey = cameraKey;
    }

    if (pEventTrack)
    {
        IEventKey key;
        int nEventKey = pEventTrack->GetActiveKey(ec.time, &key);
        if (nEventKey != m_lastEventKey && nEventKey >= 0)
        {
            bool bNotTrigger = key.bNoTriggerInScrubbing && ec.bSingleFrame && key.time != ec.time;
            if (!bNotTrigger)
            {
                ApplyEventKey(key, ec);
            }
        }
        m_lastEventKey = nEventKey;
    }

    if (pConsoleTrack)
    {
        IConsoleKey key;
        int nConsoleKey = pConsoleTrack->GetActiveKey(ec.time, &key);
        if (nConsoleKey != m_lastConsoleKey && nConsoleKey >= 0)
        {
            if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
            {
                ApplyConsoleKey(key, ec);
            }
        }
        m_lastConsoleKey = nConsoleKey;
    }

    if (pMusicTrack)
    {
        bool bMute = gEnv->IsEditor() && (pMusicTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Muted);
        if (bMute == false)
        {
            IMusicKey key;
            int nMusicKey = pMusicTrack->GetActiveKey(ec.time, &key);
            if (nMusicKey != m_lastMusicKey && nMusicKey >= 0)
            {
                if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
                {
                    ApplyMusicKey(key, ec);
                }
            }
            m_lastMusicKey = nMusicKey;
        }
    }

    if (pSequenceTrack)
    {
        ISequenceKey key;
        int nSequenceKey = pSequenceTrack->GetActiveKey(ec.time, &key);
        IAnimSequence* pSequence = GetMovieSystem()->FindSequence(key.szSelection);

        if (!gEnv->IsEditing() && (nSequenceKey != m_lastSequenceKey || !GetMovieSystem()->IsPlaying(pSequence)))
        {
            ApplySequenceKey(pSequenceTrack, m_lastSequenceKey, nSequenceKey, key, ec);
        }
        m_lastSequenceKey = nSequenceKey;
    }

    if (pGotoTrack)
    {
        ApplyGotoKey(pGotoTrack, ec);
    }

    if (pCaptureTrack && gEnv->pMovieSystem->IsInBatchRenderMode() == false)
    {
        ICaptureKey key;
        int nCaptureKey = pCaptureTrack->GetActiveKey(ec.time, &key);
        bool justEnded = false;
        if (!m_bLastCapturingEnded && key.time + key.duration < ec.time)
        {
            justEnded = true;
        }

        if (!ec.bSingleFrame && !(gEnv->IsEditor() && gEnv->IsEditing()))
        {
            if (nCaptureKey != m_lastCaptureKey && nCaptureKey >= 0)
            {
                if (m_bLastCapturingEnded == false)
                {
                    assert(0);
                    gEnv->pMovieSystem->EndCapture();
                    m_bLastCapturingEnded = true;
                }
                gEnv->pMovieSystem->StartCapture(key);
                if (key.once == false)
                {
                    m_bLastCapturingEnded = false;
                }
                m_lastCaptureKey = nCaptureKey;
            }
            else if (justEnded)
            {
                gEnv->pMovieSystem->EndCapture();
                m_bLastCapturingEnded = true;
            }
        }
    }

    m_time = ec.time;
    if (m_pOwner)
    {
        m_pOwner->OnNodeAnimated(this);
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::OnReset()
{
    // If camera from this sequence still active, remove it.
    // reset camera
    SCameraParams CamParams = gEnv->pMovieSystem->GetCameraParams();
    if (CamParams.cameraEntityId.IsValid() && m_legacyCurrentCameraEntityId == GetLegacyEntityId(CamParams.cameraEntityId))
    {
        CamParams.cameraEntityId.SetInvalid();
        CamParams.fFOV = 0;
        CamParams.justActivated = true;
        gEnv->pMovieSystem->SetCameraParams(CamParams);

        if (m_legacyCurrentCameraEntityId)
        {
            if (IEntity* pCameraEntity = gEnv->pEntitySystem->GetEntity(m_legacyCurrentCameraEntityId))
            {
                pCameraEntity->ClearFlags(ENTITY_FLAG_TRIGGER_AREAS);
            }
        }
    }

    if (m_lastSequenceKey >= 0)
    {
        {
            int trackCount = NumTracks();
            for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
            {
                CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
                IAnimTrack* pTrack = m_tracks[paramIndex];

                if (paramId.GetType() != eAnimParamType_Sequence)
                {
                    continue;
                }

                CSequenceTrack* pSequenceTrack = (CSequenceTrack*)pTrack;
                ISequenceKey prevKey;

                pSequenceTrack->GetKey(m_lastSequenceKey, &prevKey);
                GetMovieSystem()->StopSequence(prevKey.szSelection);
            }
        }
    }

    // If the last capturing hasn't finished properly, end it here.
    if (m_bLastCapturingEnded == false)
    {
        GetMovieSystem()->EndCapture();
        m_bLastCapturingEnded = true;
    }

    m_lastEventKey = -1;
    m_lastConsoleKey = -1;
    m_lastMusicKey = -1;
    m_lastSequenceKey = -1;
    m_nLastGotoKey = -1;
    m_lastCaptureKey = -1;
    m_bLastCapturingEnded = true;
    m_legacyCurrentCameraEntityId = INVALID_ENTITYID;

    if (GetTrackForParameter(eAnimParamType_TimeWarp))
    {
        gEnv->pTimer->SetTimeScale(1.0f, ITimer::eTSC_Trackview);
        if (m_cvar_t_FixedStep)
        {
            m_cvar_t_FixedStep->Set(0);
        }
    }
    if (GetTrackForParameter(eAnimParamType_FixedTimeStep) && m_cvar_t_FixedStep)
    {
        m_cvar_t_FixedStep->Set(0);
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::OnStart()
{
    ResetSounds();
}

void CAnimSceneNode::OnPause()
{
}

void CAnimSceneNode::OnLoop()
{
    ResetSounds();
}

void CAnimSceneNode::OnStop()
{
    ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ResetSounds()
{
    for (int i = m_SoundInfo.size(); --i >= 0; )
    {
        m_SoundInfo[i].Reset();
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ReleaseSounds()
{
    // Stop all sounds on the global audio object,
    // but we want to have it filter based on the owner (this)
    // so we don't stop sounds that didn't originate with track view.
    Audio::SAudioRequest request;
    request.nFlags = Audio::eARF_PRIORITY_HIGH;
    request.pOwner = this;

    Audio::SAudioObjectRequestData<Audio::eAORT_STOP_ALL_TRIGGERS> requestData(/*filterByOwner = */ true);
    request.pData = &requestData;
    Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequest, request);
}

//////////////////////////////////////////////////////////////////////////
// InterpolateCameras()
//
// This rather long function takes care of the interpolation (or blending) of
// two camera keys, specifically FoV, nearZ, position and rotation blending.
//
void CAnimSceneNode::InterpolateCameras(SCameraParams& retInterpolatedCameraParams, ISceneCamera* firstCamera, IAnimNode* firstCameraAnimNode,
                                        ISelectKey& firstKey, ISelectKey& secondKey, float time)
{
    IEntity* secondCameraLegacyEntity = nullptr;
    if (!secondKey.cameraAzEntityId.IsValid())
    {
        secondCameraLegacyEntity = gEnv->pEntitySystem->FindEntityByName(secondKey.szSelection);
        if (!secondCameraLegacyEntity)
        {
            // abort - can't interpolate if we can't find a legacy second camera and there isn't a valid Id for a component entity camera
            return;
        }
    }

    static const float EPSILON_TIME = 0.01f;            // consider times within EPSILON_TIME of beginning of blend time to be at the beginning of blend time
    float interpolatedFoV;
    const bool isFirstAnimNodeACamera = (firstCameraAnimNode && firstCameraAnimNode->GetType() == eAnimNodeType_Camera);

    ISceneCamera* secondCamera = secondKey.cameraAzEntityId.IsValid() 
                                ? static_cast<ISceneCamera*>(new CComponentEntitySceneCamera(secondKey.cameraAzEntityId))
                                : static_cast<ISceneCamera*>(new CLegacySceneCamera(secondCameraLegacyEntity));

    if (firstCameraAnimNode)
    {
        m_pCamNodeOnHoldForInterp = firstCameraAnimNode;
        m_pCamNodeOnHoldForInterp->SetSkipInterpolatedCameraNode(true);
    }

    float t = 1 - ((secondKey.time - time) / firstKey.fBlendTime);
    t = min(t, 1.0f);
    t = pow(t, 3) * (t * (t * 6 - 15) + 10);                // use a cubic curve for the camera blend

    bool haveStashedInterpData = (m_InterpolatingCameraStartStates.find(m_CurrentSelectTrackKeyNumber) != m_InterpolatingCameraStartStates.end());
    const bool haveFirstCameraAnimNodeFoV = (isFirstAnimNodeACamera && firstCameraAnimNode->GetTrackForParameter(eAnimParamType_FOV));
    const bool haveFirstCameraAnimNodeNearZ = (isFirstAnimNodeACamera && firstCameraAnimNode->GetTrackForParameter(eAnimParamType_NearZ));

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // at the start of the blend, stash the starting point first camera data to use throughout the interpolation
    if (!haveStashedInterpData)
    {
        InterpolatingCameraStartState camData;

        camData.m_interpolatedCamFirstPos = firstCamera->GetPosition();
        camData.m_interpolatedCamFirstRot = firstCamera->GetRotation();     

        // stash FoV, from track if it exists, otherwise from the first camera entity
        if (haveFirstCameraAnimNodeFoV && firstCameraAnimNode->GetTrackForParameter(eAnimParamType_FOV)->GetNumKeys() > 0)
        {
            firstCameraAnimNode->GetParamValue(time, eAnimParamType_FOV, camData.m_FoV);
        }
        else
        {
            camData.m_FoV = firstCamera->GetFoV();
        }
        // stash nearZ
        if (haveFirstCameraAnimNodeNearZ && firstCameraAnimNode->GetTrackForParameter(eAnimParamType_NearZ)->GetNumKeys() > 0)
        {
            firstCameraAnimNode->GetParamValue(time, eAnimParamType_NearZ, camData.m_nearZ);
        }
        else
        {
            camData.m_nearZ = firstCamera->GetNearZ();
        }

        m_InterpolatingCameraStartStates.insert(AZStd::make_pair(m_CurrentSelectTrackKeyNumber, camData));
    }

    const auto& retStashedInterpCamData = m_InterpolatingCameraStartStates.find(m_CurrentSelectTrackKeyNumber);
    InterpolatingCameraStartState stashedInterpCamData = retStashedInterpCamData->second;

    ///////////////////
    // interpolate FOV
    float secondCameraFOV;

    IAnimNode* secondCameraAnimNode = m_pSequence->FindNodeByName(secondKey.szSelection, this);
    if (secondCameraAnimNode == NULL)
    {
        secondCameraAnimNode = m_pSequence->FindNodeByName(secondKey.szSelection, NULL);
    }

    const bool isSecondAnimNodeACamera = (secondCameraAnimNode && secondCameraAnimNode->GetType() == eAnimNodeType_Camera);

    if (isSecondAnimNodeACamera && secondCameraAnimNode->GetTrackForParameter(eAnimParamType_FOV)
        && secondCameraAnimNode->GetTrackForParameter(eAnimParamType_FOV)->GetNumKeys() > 0)
    {
        secondCameraAnimNode->GetParamValue(time, eAnimParamType_FOV, secondCameraFOV);
    }
    else
    {
        secondCameraFOV = secondCamera->GetFoV();   
    }

    interpolatedFoV = stashedInterpCamData.m_FoV + (secondCameraFOV - stashedInterpCamData.m_FoV) * t;
    // store the interpolated FoV to be returned, in radians
    retInterpolatedCameraParams.fFOV = DEG2RAD(interpolatedFoV);

    if (haveFirstCameraAnimNodeFoV)
    {
        firstCameraAnimNode->SetParamValue(time, eAnimParamType_FOV, interpolatedFoV);
    }

    /////////////////////
    // interpolate NearZ
    float secondCameraNearZ;

    if (isSecondAnimNodeACamera && secondCameraAnimNode->GetTrackForParameter(eAnimParamType_NearZ)
        && secondCameraAnimNode->GetTrackForParameter(eAnimParamType_NearZ)->GetNumKeys() > 0)
    {
        secondCameraAnimNode->GetParamValue(time, eAnimParamType_NearZ, secondCameraNearZ);
    }
    else
    {
        secondCameraNearZ = secondCamera->GetNearZ();
    }

    retInterpolatedCameraParams.fNearZ = stashedInterpCamData.m_nearZ + (secondCameraNearZ - stashedInterpCamData.m_nearZ) * t;

    if (haveFirstCameraAnimNodeNearZ)
    {
        firstCameraAnimNode->SetParamValue(time, eAnimParamType_NearZ, retInterpolatedCameraParams.fNearZ);
    }

    // update the Camera entity's component FOV and nearZ directly if needed (if they weren't set via anim node SetParamValue() above)
    firstCamera->SetNearZAndFOVIfChanged(retInterpolatedCameraParams.fFOV, retInterpolatedCameraParams.fNearZ);

    ////////////////////////
    // interpolate Position
    Vec3 vFirstCamPos = stashedInterpCamData.m_interpolatedCamFirstPos;
    if (isFirstAnimNodeACamera)
    {
        firstCameraAnimNode->GetParamValue(time, eAnimParamType_Position, vFirstCamPos);
        firstCamera->TransformPositionFromLocalToWorldSpace(vFirstCamPos);
    }

    Vec3 secondKeyPos = secondCamera->GetPosition();
    if (isSecondAnimNodeACamera)
    {
        secondCameraAnimNode->GetParamValue(time, eAnimParamType_Position, secondKeyPos);
        secondCamera->TransformPositionFromLocalToWorldSpace(secondKeyPos);
    }

    Vec3 interpolatedPos = vFirstCamPos + (secondKeyPos - vFirstCamPos) * t;


    if (isFirstAnimNodeACamera)
    {
        firstCamera->TransformPositionFromWorldToLocalSpace(interpolatedPos);
        ((CAnimEntityNode*)firstCameraAnimNode)->SetCameraInterpolationPosition(interpolatedPos);
    }
    firstCamera->SetPosition(interpolatedPos);

    ////////////////////////
    // interpolate Rotation
    Quat firstCameraRotation = stashedInterpCamData.m_interpolatedCamFirstRot;
    Quat secondCameraRotation;

    IAnimTrack* pOrgRotationTrack = (firstCameraAnimNode ? firstCameraAnimNode->GetTrackForParameter(eAnimParamType_Rotation) : 0);
    if (pOrgRotationTrack)
    {
        pOrgRotationTrack->GetValue(time, firstCameraRotation);
    }

    if (isFirstAnimNodeACamera)
    {
        firstCamera->TransformRotationFromLocalToWorldSpace(firstCameraRotation);
    }

    secondCameraRotation = secondCamera->GetRotation();
    
    IAnimTrack* pRotationTrack = (secondCameraAnimNode ? secondCameraAnimNode->GetTrackForParameter(eAnimParamType_Rotation) : 0);
    if (pRotationTrack)
    {
        pRotationTrack->GetValue(time, secondCameraRotation);
    }
    
    if (isSecondAnimNodeACamera)
    {
        secondCamera->TransformRotationFromLocalToWorldSpace(secondCameraRotation);
    }

    Quat interpolatedRotation;
    interpolatedRotation.SetSlerp(firstCameraRotation, secondCameraRotation, t);

    firstCamera->SetWorldRotation(interpolatedRotation);

    if (isFirstAnimNodeACamera)
    {
        ((CAnimEntityNode*)firstCameraAnimNode)->SetCameraInterpolationRotation(firstCamera->HasParent() ? firstCamera->GetRotation() : interpolatedRotation);
    }

    // clean-up
    if (secondCamera)
    {
        delete secondCamera;
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyCameraKey(ISelectKey& key, SAnimContext& ec)
{
    ISelectKey nextKey;
    int nextCameraKeyNumber = m_CurrentSelectTrackKeyNumber + 1;
    bool bInterpolateCamera = false;

    if (nextCameraKeyNumber < m_CurrentSelectTrack->GetNumKeys())
    {
        m_CurrentSelectTrack->GetKey(nextCameraKeyNumber, &nextKey);

        float fInterTime = nextKey.time - ec.time;
        if (fInterTime >= 0 && fInterTime <= key.fBlendTime)
        {
            bInterpolateCamera = true;
        }
    }

    // check if we're finished interpolating and there is a camera node on hold for iterpolation. If so, unset it from hold.
    if (!bInterpolateCamera && m_pCamNodeOnHoldForInterp)
    {
        m_pCamNodeOnHoldForInterp->SetSkipInterpolatedCameraNode(false);
        m_pCamNodeOnHoldForInterp = 0;
    }

    // Search sequences for the current Camera's AnimNode, if it exists.
    // First, check the child nodes of this director, then global nodes.
    IAnimNode* firstCameraAnimNode = m_pSequence->FindNodeByName(key.szSelection, this);
    if (firstCameraAnimNode == NULL)
    {
        firstCameraAnimNode = m_pSequence->FindNodeByName(key.szSelection, NULL);
    }

    SCameraParams cameraParams;
    cameraParams.cameraEntityId.SetInvalid();
    cameraParams.fFOV = 0;
    cameraParams.justActivated = true;

    ///////////////////////////////////////////////////////////////////
    // find the Scene Camera (either Legacy or Camera Component Camera)  
    ISceneCamera* firstSceneCamera = nullptr;

    if (key.cameraAzEntityId.IsValid())
    {
        // camera component entity
        cameraParams.cameraEntityId = key.cameraAzEntityId;
        firstSceneCamera = static_cast<ISceneCamera*>(new CComponentEntitySceneCamera(key.cameraAzEntityId));
    }
    else
    {
        // legacy camera entity
        IEntity* legacyFirstCamera = nullptr;
        legacyFirstCamera = gEnv->pEntitySystem->FindEntityByName(key.szSelection);
        if (legacyFirstCamera)
        {
            firstSceneCamera = static_cast<ISceneCamera*>(new CLegacySceneCamera(legacyFirstCamera));
            cameraParams.cameraEntityId = AZ::EntityId(legacyFirstCamera->GetId());
        }
    }

    // get FOV - prefer track data for Legacy Cameras. For Component Cameras, retrieving from the component is fine.
    if (firstCameraAnimNode && firstCameraAnimNode->GetType() == eAnimNodeType_Camera)
    {
        float fFirstCameraFOV = RAD2DEG(DEFAULT_FOV);
        float fFirstCameraNearZ = DEFAULT_NEAR;

        firstCameraAnimNode->GetParamValue(ec.time, eAnimParamType_NearZ, fFirstCameraNearZ);
        cameraParams.fNearZ = fFirstCameraNearZ;

        firstCameraAnimNode->GetParamValue(ec.time, eAnimParamType_FOV, fFirstCameraFOV);
        cameraParams.fFOV = DEG2RAD(fFirstCameraFOV);
    }
    else if (firstSceneCamera)
    {
        cameraParams.fFOV = DEG2RAD(firstSceneCamera->GetFoV());
    }   

    if (bInterpolateCamera && firstSceneCamera)
    {
        InterpolateCameras(cameraParams, firstSceneCamera, firstCameraAnimNode, key, nextKey, ec.time);
    }

    m_legacyCurrentCameraEntityId = GetLegacyEntityId(cameraParams.cameraEntityId);
    gEnv->pMovieSystem->SetCameraParams(cameraParams);

    // This detects when we've switched from one Camera to another on the Camera Track
    // If cameras were interpolated (blended), reset cameras to their pre-interpolated positions and
    // clean up cached data used for the interpolation
    if (m_lastCameraKey != m_CurrentSelectTrackKeyNumber && m_lastCameraKey >= 0)
    {
        const auto& retStashedData = m_InterpolatingCameraStartStates.find(m_lastCameraKey);
        if (retStashedData != m_InterpolatingCameraStartStates.end())
        {
            InterpolatingCameraStartState stashedData = retStashedData->second;
            ISelectKey prevKey;
            ISceneCamera* prevSceneCamera = nullptr;

            m_CurrentSelectTrack->GetKey(m_lastCameraKey, &prevKey);

            if (prevKey.cameraAzEntityId.IsValid())
            {
                prevSceneCamera = static_cast<ISceneCamera*>(new CComponentEntitySceneCamera(prevKey.cameraAzEntityId));
            }
            else
            {
                // legacy camera
                IEntity*   prevCameraEntity;
                prevCameraEntity = gEnv->pEntitySystem->FindEntityByName(prevKey.szSelection);
                if (prevCameraEntity)
                {
                    prevSceneCamera = static_cast<ISceneCamera*>(new CLegacySceneCamera(prevCameraEntity));
                }
            }
            
            if (prevSceneCamera)
            {
                prevSceneCamera->SetPosition(stashedData.m_interpolatedCamFirstPos);
                prevSceneCamera->SetRotation(stashedData.m_interpolatedCamFirstRot);
            }

            IAnimNode* prevCameraAnimNode = m_pSequence->FindNodeByName(prevKey.szSelection, this);
            if (prevCameraAnimNode == NULL)
            {
                prevCameraAnimNode = m_pSequence->FindNodeByName(prevKey.szSelection, NULL);
            }

            if (prevCameraAnimNode && prevCameraAnimNode->GetType() == eAnimNodeType_Camera && prevCameraAnimNode->GetTrackForParameter(eAnimParamType_FOV))
            {
                prevCameraAnimNode->SetParamValue(ec.time, eAnimParamType_FOV, stashedData.m_FoV);
            }
            else if (prevSceneCamera)
            {
                prevSceneCamera->SetNearZAndFOVIfChanged(DEG2RAD(stashedData.m_FoV), stashedData.m_nearZ);
            }

            m_InterpolatingCameraStartStates.erase(m_lastCameraKey);

            // clean up
            if (prevSceneCamera)
            {
                delete prevSceneCamera;
            }
        }
    }

    // clean up
    if (firstSceneCamera)
    {
        delete firstSceneCamera;
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyEventKey(IEventKey& key, SAnimContext& ec)
{
    char funcName[1024];
    cry_strcpy(funcName, "Event_");
    cry_strcat(funcName, key.event);
    gEnv->pMovieSystem->SendGlobalEvent(funcName);
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyAudioKey(char const* const sTriggerName, bool const bPlay /* = true */)
{
    Audio::TAudioControlID nAudioTriggerID = INVALID_AUDIO_CONTROL_ID;
    Audio::AudioSystemRequestBus::BroadcastResult(nAudioTriggerID, &Audio::AudioSystemRequestBus::Events::GetAudioTriggerID, sTriggerName);
    if (nAudioTriggerID != INVALID_AUDIO_CONTROL_ID)
    {
        Audio::SAudioRequest oRequest;
        oRequest.nFlags = Audio::eARF_PRIORITY_HIGH;
        oRequest.pOwner = this;

        if (bPlay)
        {
            Audio::SAudioObjectRequestData<Audio::eAORT_EXECUTE_TRIGGER> oRequestData(nAudioTriggerID, 0.0f);
            oRequest.pData = &oRequestData;
            Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequest, oRequest);
        }
        else
        {
            Audio::SAudioObjectRequestData<Audio::eAORT_STOP_TRIGGER> oRequestData(nAudioTriggerID);
            oRequest.pData = &oRequestData;
            Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequest, oRequest);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplySequenceKey(IAnimTrack* pTrack, int nPrevKey, int nCurrKey, ISequenceKey& key, SAnimContext& ec)
{
    if (nCurrKey >= 0 && key.szSelection[0])
    {
        IAnimSequence* pSequence = GetMovieSystem()->FindSequence(key.szSelection);
        if (pSequence)
        {
            float startTime = -FLT_MAX;
            float endTime = -FLT_MAX;

            if (key.bOverrideTimes)
            {
                key.fDuration = (key.fEndTime - key.fStartTime) > 0.0f ? (key.fEndTime - key.fStartTime) : 0.0f;
                startTime = key.fStartTime;
                endTime = key.fEndTime;
            }
            else
            {
                key.fDuration = pSequence->GetTimeRange().Length();
            }

            pTrack->SetKey(nCurrKey, &key);

            SAnimContext newAnimContext = ec;
            newAnimContext.time = std::min(ec.time - key.time + key.fStartTime, key.fDuration + key.fStartTime);

            if (static_cast<CAnimSequence*>(pSequence)->GetTime() != newAnimContext.time)
            {
                pSequence->Animate(newAnimContext);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyConsoleKey(IConsoleKey& key, SAnimContext& ec)
{
    if (key.command[0])
    {
        gEnv->pConsole->ExecuteString(key.command);
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::ApplyMusicKey(IMusicKey& key, SAnimContext& ec)
{
}

void CAnimSceneNode::ApplyGotoKey(CGotoTrack*   poGotoTrack, SAnimContext& ec)
{
    IDiscreteFloatKey           stDiscreteFloadKey;
    int                                     nCurrentActiveKeyIndex(-1);

    nCurrentActiveKeyIndex = poGotoTrack->GetActiveKey(ec.time, &stDiscreteFloadKey);
    if (nCurrentActiveKeyIndex != m_nLastGotoKey && nCurrentActiveKeyIndex >= 0)
    {
        if (!ec.bSingleFrame)
        {
            if (stDiscreteFloadKey.m_fValue >= 0)
            {
                string fullname = m_pSequence->GetName();
                GetMovieSystem()->GoToFrame(fullname.c_str(), stDiscreteFloadKey.m_fValue);
            }
        }
    }

    m_nLastGotoKey = nCurrentActiveKeyIndex;
}

bool CAnimSceneNode::GetEntityTransform(IAnimSequence* pSequence, IEntity* pEntity, float time, Vec3& vCamPos, Quat& qCamRot)
{
    const uint iNodeCount = pSequence->GetNodeCount();
    for (uint i = 0; i < iNodeCount; ++i)
    {
        IAnimNode* pNode = pSequence->GetNode(i);

        if (pNode && pNode->GetType() == eAnimNodeType_Camera && pNode->GetEntity() == pEntity)
        {
            pNode->GetParamValue(time, eAnimParamType_Position, vCamPos);

            IAnimTrack* pOrgRotationTrack = pNode->GetTrackForParameter(eAnimParamType_Rotation);
            if (pOrgRotationTrack != NULL)
            {
                pOrgRotationTrack->GetValue(time, qCamRot);
            }

            return true;
        }
    }

    return false;
}

bool CAnimSceneNode::GetEntityTransform(IEntity* pEntity, float time, Vec3& vCamPos, Quat& qCamRot)
{
    CRY_ASSERT(pEntity != NULL);

    vCamPos = pEntity->GetPos();
    qCamRot = pEntity->GetRotation();

    bool bFound = GetEntityTransform(m_pSequence, pEntity, time, vCamPos, qCamRot);

    uint numTracks = GetTrackCount();
    for (uint trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        IAnimTrack* pAnimTrack = GetTrackByIndex(trackIndex);
        if (pAnimTrack->GetParameterType() == eAnimParamType_Sequence)
        {
            CSequenceTrack* pSequenceTrack = static_cast<CSequenceTrack*>(pAnimTrack);

            const uint numKeys = pSequenceTrack->GetNumKeys();
            for (uint keyIndex = 0; keyIndex < numKeys; ++keyIndex)
            {
                ISequenceKey key;
                pSequenceTrack->GetKey(keyIndex, &key);

                CAnimSequence* pSubSequence = static_cast<CAnimSequence*>(GetMovieSystem()->FindSequence(key.szSelection));
                if (pSubSequence)
                {
                    bool bSubSequence = GetEntityTransform(pSubSequence, pEntity, time, vCamPos, qCamRot);
                    bFound = bFound || bSubSequence;
                }
            }
        }
    }

    if (pEntity->GetParent())
    {
        IEntity* pParent = pEntity->GetParent();
        if (pParent)
        {
            vCamPos = pParent->GetWorldTM() * vCamPos;
            qCamRot = pParent->GetWorldRotation() * qCamRot;
        }
    }

    return bFound;
}

void CAnimSceneNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
    CAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);

    // To enable renaming even for previously saved director nodes
    SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
}

void CAnimSceneNode::PrecacheStatic(float startTime)
{
    m_lastPrecachePoint = -1.f;

    const uint numTracks = GetTrackCount();
    for (uint trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        IAnimTrack* pAnimTrack = GetTrackByIndex(trackIndex);
        if (pAnimTrack->GetParameterType() == eAnimParamType_Sequence)
        {
            CSequenceTrack* pSequenceTrack = static_cast<CSequenceTrack*>(pAnimTrack);

            const uint numKeys = pSequenceTrack->GetNumKeys();
            for (uint keyIndex = 0; keyIndex < numKeys; ++keyIndex)
            {
                ISequenceKey key;
                pSequenceTrack->GetKey(keyIndex, &key);

                CAnimSequence* pSubSequence = static_cast<CAnimSequence*>(GetMovieSystem()->FindSequence(key.szSelection));
                if (pSubSequence)
                {
                    pSubSequence->PrecacheStatic(startTime - (key.fStartTime + key.time));
                }
            }
        }
    }
}

void CAnimSceneNode::PrecacheDynamic(float time)
{
    const uint numTracks = GetTrackCount();
    float fLastPrecachePoint = m_lastPrecachePoint;

    for (uint trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        IAnimTrack* pAnimTrack = GetTrackByIndex(trackIndex);
        if (pAnimTrack->GetParameterType() == eAnimParamType_Sequence)
        {
            CSequenceTrack* pSequenceTrack = static_cast<CSequenceTrack*>(pAnimTrack);

            const uint numKeys = pSequenceTrack->GetNumKeys();
            for (uint keyIndex = 0; keyIndex < numKeys; ++keyIndex)
            {
                ISequenceKey key;
                pSequenceTrack->GetKey(keyIndex, &key);

                CAnimSequence* pSubSequence = static_cast<CAnimSequence*>(GetMovieSystem()->FindSequence(key.szSelection));
                if (pSubSequence)
                {
                    pSubSequence->PrecacheDynamic(time - (key.fStartTime + key.time));
                }
            }
        }
        else if (pAnimTrack->GetParameterType() == eAnimParamType_Camera)
        {
            const float fPrecacheCameraTime = CMovieSystem::m_mov_cameraPrecacheTime;
            if (fPrecacheCameraTime > 0.f)
            {
                CSelectTrack* pCameraTrack = static_cast<CSelectTrack*>(pAnimTrack);

                ISelectKey key;
                int keyID = pCameraTrack->GetActiveKey(time + fPrecacheCameraTime, &key);

                if (time < key.time && (time + fPrecacheCameraTime) > key.time && key.time > m_lastPrecachePoint)
                {
                    fLastPrecachePoint = max(key.time, fLastPrecachePoint);
                    IEntity* pCameraEntity = gEnv->pEntitySystem->FindEntityByName(key.szSelection);

                    if (pCameraEntity != NULL)
                    {
                        Vec3 vCamPos(ZERO);
                        Quat qCamRot(IDENTITY);
                        if (GetEntityTransform(pCameraEntity, key.time, vCamPos, qCamRot))
                        {
                            gEnv->p3DEngine->AddPrecachePoint(vCamPos, qCamRot.GetColumn1(), fPrecacheCameraTime);
                        }
                        else
                        {
                            CryWarning(VALIDATOR_MODULE_MOVIE, VALIDATOR_WARNING, "Could not find animation node for camera %s in sequence %s", key.szSelection, m_pSequence->GetName());
                        }
                    }
                }
            }
        }
    }

    m_lastPrecachePoint = fLastPrecachePoint;
}

void CAnimSceneNode::InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
    if (paramType.GetType() == eAnimParamType_TimeWarp)
    {
        pTrack->SetValue(0.0f, 1.0f, true);
    }
}

#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam

