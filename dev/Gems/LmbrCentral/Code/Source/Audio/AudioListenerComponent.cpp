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

#include "StdAfx.h"
#include "AudioListenerComponent.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzCore/Debug/Trace.h>

#include <ISystem.h>
#include <IAudioSystem.h>
#include <MathConversion.h>

namespace LmbrCentral
{
    //=========================================================================
    //! Script Interface

    //=========================================================================
    void AudioListenerComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AudioListenerComponent, AZ::Component>()
                ->Version(1)
                ->Field("Rotation Entity", &AudioListenerComponent::m_rotationEntity)
                ->Field("Position Entity", &AudioListenerComponent::m_positionEntity)
                ->Field("Fixed offset", &AudioListenerComponent::m_fixedOffset)
                ;
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<AudioListenerComponentRequestBus>("AudioListenerComponentRequestBus")
                ->Event("SetRotationEntity", &AudioListenerComponentRequestBus::Events::SetRotationEntity)
                ->Event("SetPositionEntity", &AudioListenerComponentRequestBus::Events::SetPositionEntity)
                ->Event("SetFullTransformEntity", &AudioListenerComponentRequestBus::Events::SetFullTransformEntity)
                ;
        }
    }

    //=========================================================================
    void AudioListenerComponent::Activate()
    {
        m_transform = AZ::Transform::CreateIdentity();

        m_listenerObjectId = INVALID_AUDIO_OBJECT_ID;
        // todo: Change the ReserveID apis to return the ID instead of a bool
        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::ReserveAudioListenerID, m_listenerObjectId);
        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::SetAudioListenerOverrideID, m_listenerObjectId);

        RefreshBusConnections(m_rotationEntity, m_positionEntity);

        AudioListenerComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    //=========================================================================
    void AudioListenerComponent::Deactivate()
    {
        AZ::EntityBus::MultiHandler::BusDisconnect();
        AZ::TransformNotificationBus::MultiHandler::BusDisconnect();
        AudioListenerComponentRequestBus::Handler::BusDisconnect();

        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::ReleaseAudioListenerID, m_listenerObjectId);
        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::SetAudioListenerOverrideID, INVALID_AUDIO_OBJECT_ID);
        m_listenerObjectId = INVALID_AUDIO_OBJECT_ID;
    }

    //=========================================================================
    void AudioListenerComponent::SetRotationEntity(const AZ::EntityId entityId)
    {
        RefreshBusConnections(entityId, m_currentRotationEntity);
    }

    //=========================================================================
    void AudioListenerComponent::SetPositionEntity(const AZ::EntityId entityId)
    {
        RefreshBusConnections(m_currentPositionEntity, entityId);
    }

    //=========================================================================
    void AudioListenerComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        const AZ::EntityId* busId = AZ::TransformNotificationBus::GetCurrentBusId();
        if (!busId)
        {
            AZ_ErrorOnce("AudioListenerComponent", busId != nullptr,
                "AudioListenerComponent - Bus Id is null!");
            return;
        }

        AZ::EntityId entityId = *busId;

        if (entityId == m_currentRotationEntity)
        {
            // update orientation only:
            // cache the old position, copy transform, and replace position.
            // is there a more direct way to do this?
            AZ::Vector3 position = m_transform.GetPosition();
            m_transform = world;
            m_transform.SetPosition(position);
        }

        if (entityId == m_currentPositionEntity)
        {
            // update position
            m_transform.SetPosition(world.GetPosition());
        }

        SendListenerPosition();
    }

    //=========================================================================
    void AudioListenerComponent::OnEntityActivated(const AZ::EntityId& entityId)
    {
        // Do a fetch of the transforms to sync when a linked Entity activates, because
        // it may not move initially.
        AZ::TransformBus::EventResult(m_transform, m_currentRotationEntity, &AZ::TransformBus::Events::GetWorldTM);

        AZ::Transform position = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(position, m_currentPositionEntity, &AZ::TransformBus::Events::GetWorldTM);

        m_transform.SetPosition(position.GetPosition());

        SendListenerPosition();

        AZ::TransformNotificationBus::MultiHandler::BusConnect(entityId);
    }

    //=========================================================================
    void AudioListenerComponent::OnEntityDeactivated(const AZ::EntityId& entityId)
    {
        AZ::TransformNotificationBus::MultiHandler::BusDisconnect(entityId);
    }

    //=========================================================================
    void AudioListenerComponent::SendListenerPosition()
    {
        // Offset the position
        AZ::Transform transform = m_transform;
        transform.SetPosition(m_transform.GetPosition() + m_fixedOffset);

        // Send an update request
        Audio::SAudioListenerRequestData<Audio::eALRT_SET_POSITION> requestData(AZTransformToLYTransform(transform));
        Audio::SAudioRequest request;
        request.nAudioObjectID = m_listenerObjectId;
        request.nFlags = Audio::eARF_PRIORITY_NORMAL;
        request.pOwner = this;
        request.pData = &requestData;

        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequest, request);
    }

    //=========================================================================
    void AudioListenerComponent::RefreshBusConnections(const AZ::EntityId rotationEntityId, const AZ::EntityId positionEntityId)
    {
        // Entity used for Orientation

        if (m_currentRotationEntity.IsValid())
        {
            AZ::EntityBus::MultiHandler::BusDisconnect(m_currentRotationEntity);
            AZ::TransformNotificationBus::MultiHandler::BusDisconnect(m_currentRotationEntity);
        }

        if (rotationEntityId.IsValid())
        {
            AZ::EntityBus::MultiHandler::BusConnect(rotationEntityId);
            m_currentRotationEntity = rotationEntityId;
        }
        else
        {
            AZ::TransformNotificationBus::MultiHandler::BusConnect(GetEntityId());
            m_currentRotationEntity = GetEntityId();
        }

        // Entity used for Position

        if (m_currentPositionEntity.IsValid())
        {
            AZ::EntityBus::MultiHandler::BusDisconnect(m_currentPositionEntity);
            AZ::TransformNotificationBus::MultiHandler::BusDisconnect(m_currentPositionEntity);
        }

        if (positionEntityId.IsValid())
        {
            AZ::EntityBus::MultiHandler::BusConnect(positionEntityId);
            m_currentPositionEntity = positionEntityId;
        }
        else
        {
            AZ::TransformNotificationBus::MultiHandler::BusConnect(GetEntityId());
            m_currentPositionEntity = GetEntityId();
        }

        // Do a fetch of the transforms to sync upon connecting.
        // This is only useful if either of the Entities are using 'this' Entity.
        // If either of them use other Entities, the transform will be synced via OnEntityActivated.
        if (m_currentRotationEntity == GetEntityId() || m_currentPositionEntity == GetEntityId())
        {
            AZ::TransformBus::EventResult(m_transform, m_currentRotationEntity, &AZ::TransformBus::Events::GetWorldTM);

            AZ::Transform position = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(position, m_currentPositionEntity, &AZ::TransformBus::Events::GetWorldTM);

            m_transform.SetPosition(position.GetPosition());

            SendListenerPosition();
        }
    }

} // namespace LmbrCentral
