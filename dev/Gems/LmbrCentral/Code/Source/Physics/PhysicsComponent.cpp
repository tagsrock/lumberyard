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
#include "PhysicsComponent.h"
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Math/Transform.h>
#include <MathConversion.h>
#include <IPhysics.h>

namespace LmbrCentral
{
    extern bool PhysicsComponentV1Converter(AZ::SerializeContext&, AZ::SerializeContext::DataElementNode&);

    class PhysicsComponentNotificationBusHandler
        : public PhysicsComponentNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(PhysicsComponentNotificationBusHandler, "{245B5B85-533C-4A5E-B1DC-F06CAD896D37}", AZ::SystemAllocator, OnPhysicsEnabled, OnPhysicsDisabled, OnCollision);

        void OnPhysicsEnabled() override
        {
            Call(FN_OnPhysicsEnabled);
        }

        void OnPhysicsDisabled() override
        {
            Call(FN_OnPhysicsDisabled);
        }

        void OnCollision(const Collision& collision) override
        {
            Call(FN_OnCollision, collision);
        }
    };

    void PhysicsComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            // PhysicsComponent used to work very differently.
            // We've changed the UUID so that we can do a total conversion of legacy data.
            serializeContext->ClassDeprecate("PhysicsComponent", "{A74FA374-8F68-495B-96C1-0BCC8D00EB61}", PhysicsComponentV1Converter);

            serializeContext->Class<PhysicsComponent, AZ::Component>()
                ->Version(2)
            ;
        }
        if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            using Collision = PhysicsComponentNotifications::Collision;
            // Info about a collision event
            behaviorContext->Class<Collision>()
                ->Attribute(AZ::Script::Attributes::Storage, AZ::Script::Attributes::StorageType::Value)
                ->Property("entity", BehaviorValueGetter(&Collision::m_entity), nullptr)
                ->Property("position", BehaviorValueGetter(&Collision::m_position), nullptr)
                ->Property("normal", BehaviorValueGetter(&Collision::m_normal), nullptr)
                ->Property("impulse", BehaviorValueGetter(&Collision::m_impulse), nullptr)
                ->Property("velocities", BehaviorValueGetter(&Collision::m_velocities), nullptr)
                ->Property("masses", BehaviorValueGetter(&Collision::m_masses), nullptr)
                ->Property("surfaces", BehaviorValueGetter(&Collision::m_surfaces), nullptr)
                ;

            behaviorContext->EBus<PhysicsComponentRequestBus>("PhysicsComponentRequestBus")
                ->Event("EnablePhysics", &PhysicsComponentRequestBus::Events::EnablePhysics)
                ->Event("DisablePhysics", &PhysicsComponentRequestBus::Events::DisablePhysics)
                ->Event("IsPhysicsEnabled", &PhysicsComponentRequestBus::Events::IsPhysicsEnabled)
                ->Event("AddImpulse", &PhysicsComponentRequestBus::Events::AddImpulse)
                ->Event("AddImpulseAtPoint", &PhysicsComponentRequestBus::Events::AddImpulseAtPoint)
                ->Event("AddAngularImpulse", &PhysicsComponentRequestBus::Events::AddAngularImpulse)
                ->Event("AddAngularImpulseAtPoint", &PhysicsComponentRequestBus::Events::AddAngularImpulseAtPoint)
                ->Event("GetVelocity", &PhysicsComponentRequestBus::Events::GetVelocity)
                ->Event("SetVelocity", &PhysicsComponentRequestBus::Events::SetVelocity)
                ->Event("GetAcceleration", &PhysicsComponentRequestBus::Events::GetAcceleration)
                ->Event("GetAngularVelocity", &PhysicsComponentRequestBus::Events::GetAngularVelocity)
                ->Event("SetAngularVelocity", &PhysicsComponentRequestBus::Events::SetAngularVelocity)
                ->Event("GetAngularAcceleration", &PhysicsComponentRequestBus::Events::GetAngularAcceleration)
                ->Event("GetMass", &PhysicsComponentRequestBus::Events::GetMass)
                ->Event("SetMass", &PhysicsComponentRequestBus::Events::SetMass)
                ->Event("GetDensity", &PhysicsComponentRequestBus::Events::GetDensity)
                ->Event("SetDensity", &PhysicsComponentRequestBus::Events::SetDensity)
                ->Event("GetDamping", &PhysicsComponentRequestBus::Events::GetDamping)
                ->Event("SetDamping", &PhysicsComponentRequestBus::Events::SetDamping)
                ->Event("GetMinEnergy", &PhysicsComponentRequestBus::Events::GetMinEnergy)
                ->Event("SetMinEnergy", &PhysicsComponentRequestBus::Events::SetMinEnergy)
                ->Event("GetWaterDamping", &PhysicsComponentRequestBus::Events::GetWaterDamping)
                ->Event("SetWaterDamping", &PhysicsComponentRequestBus::Events::SetWaterDamping)
                ->Event("GetWaterDensity", &PhysicsComponentRequestBus::Events::GetWaterDensity)
                ->Event("SetWaterDensity", &PhysicsComponentRequestBus::Events::SetWaterDensity)
                ->Event("GetWaterResistance", &PhysicsComponentRequestBus::Events::GetWaterResistance)
                ->Event("SetWaterResistance", &PhysicsComponentRequestBus::Events::SetWaterResistance)
                ;

            behaviorContext->EBus<PhysicsComponentNotificationBus>("PhysicsComponentNotificationBus")
                ->Handler<PhysicsComponentNotificationBusHandler>()
                ;
        }
    }

    void PhysicsComponent::Init()
    {
    }

    void PhysicsComponent::Activate()
    {
        PhysicsComponentRequestBus::Handler::BusConnect(GetEntityId());
        CryPhysicsComponentRequestBus::Handler::BusConnect(GetEntityId());

        if (IsEnabledInitially())
        {
            EnablePhysics();
        }
    }

    void PhysicsComponent::Deactivate()
    {
        PhysicsComponentRequestBus::Handler::BusDisconnect();
        CryPhysicsComponentRequestBus::Handler::BusDisconnect();
        DisablePhysics();
    }

    bool PhysicsComponent::IsPhysicsEnabled()
    {
        return m_isPhysicsFullyEnabled;
    }

    void PhysicsComponent::AddImpulse(const AZ::Vector3& impulse)
    {
        if (IsPhysicsEnabled())
        {
            pe_action_impulse action;
            action.impulse = AZVec3ToLYVec3(impulse);
            m_physicalEntity->Action(&action);
        }
    }
    
    void PhysicsComponent::AddImpulseAtPoint(const AZ::Vector3& impulse, const AZ::Vector3& worldSpacePoint)
    {
        if (IsPhysicsEnabled())
        {
            pe_action_impulse action;
            action.impulse = AZVec3ToLYVec3(impulse);
            action.point = AZVec3ToLYVec3(worldSpacePoint);
            m_physicalEntity->Action(&action);
        }
    }

    void PhysicsComponent::AddAngularImpulse(const AZ::Vector3& impulse)
    {
        if (IsPhysicsEnabled())
        {
            pe_action_impulse action;
            action.angImpulse = AZVec3ToLYVec3(impulse);
            action.iApplyTime = 0;
            m_physicalEntity->Action(&action);
        }
    }
    
    void PhysicsComponent::AddAngularImpulseAtPoint(const AZ::Vector3& impulse, const AZ::Vector3& worldSpacePivot)
    {
        if (IsPhysicsEnabled())
        {
            pe_action_impulse action;
            action.angImpulse = AZVec3ToLYVec3(impulse);
            action.point = AZVec3ToLYVec3(worldSpacePivot);
            action.iApplyTime = 0;
            m_physicalEntity->Action(&action);
        }
    }

    AZ::Vector3 PhysicsComponent::GetVelocity()
    {
        AZ::Vector3 velocity = AZ::Vector3::CreateZero();

        if (IsPhysicsEnabled())
        {
            pe_status_dynamics status;
            m_physicalEntity->GetStatus(&status);
            velocity = LYVec3ToAZVec3(status.v);
        }

        return velocity;
    }

    void PhysicsComponent::SetVelocity(const AZ::Vector3& velocity)
    {
        if (IsPhysicsEnabled())
        {
            pe_action_set_velocity action;
            action.v = AZVec3ToLYVec3(velocity);
            m_physicalEntity->Action(&action);
        }
    }

    AZ::Vector3 PhysicsComponent::GetAcceleration()
    {
        AZ::Vector3 velocity = AZ::Vector3::CreateZero();

        if (IsPhysicsEnabled())
        {
            pe_status_dynamics status;
            m_physicalEntity->GetStatus(&status);
            velocity = LYVec3ToAZVec3(status.a);
        }

        return velocity;
    }

    AZ::Vector3 PhysicsComponent::GetAngularVelocity()
    {
        AZ::Vector3 velocity = AZ::Vector3::CreateZero();

        if (IsPhysicsEnabled())
        {
            pe_status_dynamics status;
            m_physicalEntity->GetStatus(&status);
            velocity = LYVec3ToAZVec3(status.w);
        }

        return velocity;
    }

    void PhysicsComponent::SetAngularVelocity(const AZ::Vector3& angularVelocity)
    {
        if (IsPhysicsEnabled())
        {
            pe_action_set_velocity action;
            action.w = AZVec3ToLYVec3(angularVelocity);
            action.bRotationAroundPivot = 1;
            m_physicalEntity->Action(&action);
        }
    }

    AZ::Vector3 PhysicsComponent::GetAngularAcceleration()
    {
        AZ::Vector3 velocity = AZ::Vector3::CreateZero();

        if (IsPhysicsEnabled())
        {
            pe_status_dynamics status;
            m_physicalEntity->GetStatus(&status);
            velocity = LYVec3ToAZVec3(status.wa);
        }

        return velocity;
    }

    float PhysicsComponent::GetMass()
    {
        float mass = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_status_dynamics status;
            m_physicalEntity->GetStatus(&status);
            mass = status.mass;
        }

        return mass;
    }

    void PhysicsComponent::SetMass(float mass)
    {
        if (IsPhysicsEnabled())
        {
            pe_simulation_params params;
            params.mass = mass;
            m_physicalEntity->SetParams(&params);
        }
    }

    float PhysicsComponent::GetDensity()
    {
        float density = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_simulation_params outParams;
            m_physicalEntity->GetParams(&outParams);
            density = outParams.density;
        }

        return density;
    }

    void PhysicsComponent::SetDensity(float density)
    {
        if (IsPhysicsEnabled())
        {
            pe_simulation_params params;
            params.density = density;
            m_physicalEntity->SetParams(&params);
        }
    }

    float PhysicsComponent::GetDamping()
    {
        float damping = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_simulation_params outParams;
            m_physicalEntity->GetParams(&outParams);
            damping = outParams.damping;
        }

        return damping;
    }

    void PhysicsComponent::SetDamping(float damping)
    {
        if (IsPhysicsEnabled())
        {
            pe_simulation_params params;
            params.damping = damping;
            m_physicalEntity->SetParams(&params);
        }
    }

    float PhysicsComponent::GetMinEnergy()
    {
        float minEnergy = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_simulation_params outParams;
            m_physicalEntity->GetParams(&outParams);
            minEnergy = outParams.minEnergy;
        }

        return minEnergy;
    }

    void PhysicsComponent::SetMinEnergy(float minEnergy)
    {
        if (IsPhysicsEnabled())
        {
            pe_simulation_params params;
            params.minEnergy = minEnergy;
            m_physicalEntity->SetParams(&params);
        }
    }

    float PhysicsComponent::GetWaterDamping()
    {
        float waterDamping = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_params_buoyancy waterParams;
            m_physicalEntity->GetParams(&waterParams);
            waterDamping = waterParams.waterDamping;
        }

        return waterDamping;
    }

    void PhysicsComponent::SetWaterDamping(float waterDamping)
    {
        if (IsPhysicsEnabled())
        {
            pe_params_buoyancy waterParams;
            waterParams.waterDamping = waterDamping;
            m_physicalEntity->SetParams(&waterParams);
        }
    }

    float PhysicsComponent::GetWaterDensity()
    {
        float waterDensity = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_params_buoyancy waterParams;
            m_physicalEntity->GetParams(&waterParams);
            waterDensity = waterParams.kwaterDensity;
        }

        return waterDensity;
    }

    void PhysicsComponent::SetWaterDensity(float waterDensity)
    {
        if (IsPhysicsEnabled())
        {
            pe_params_buoyancy waterParams;
            waterParams.kwaterDensity = waterDensity;
            m_physicalEntity->SetParams(&waterParams);
        }
    }

    float PhysicsComponent::GetWaterResistance()
    {
        float waterResistance = 0.0f;

        if (IsPhysicsEnabled())
        {
            pe_params_buoyancy waterParams;
            m_physicalEntity->GetParams(&waterParams);
            waterResistance = waterParams.kwaterResistance;
        }

        return waterResistance;
    }

    void PhysicsComponent::SetWaterResistance(float waterResistance)
    {
        if (IsPhysicsEnabled())
        {
            pe_params_buoyancy waterParams;
            waterParams.kwaterResistance = waterResistance;
            m_physicalEntity->SetParams(&waterParams);
        }
    }

    AZ::Aabb PhysicsComponent::GetAabb()
    {
        if (IsPhysicsEnabled())
        {
            pe_params_bbox bboxParams;
            m_physicalEntity->GetParams(&bboxParams);
            AZ::Aabb result = AZ::Aabb::CreateFromMinMax(LYVec3ToAZVec3(bboxParams.BBox[0]), LYVec3ToAZVec3(bboxParams.BBox[1]));
            return result;
        }
        else
        {
            return AZ::Aabb::CreateNull();
        }
    }

    void PhysicsComponent::EnablePhysics()
    {
        if (m_physicalEntity)
        {
            return;
        }

        // Setup position params
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);
        pe_params_pos positionParameters;
        Matrix34 cryTransform(AZTransformToLYTransform(transform));
        positionParameters.pMtx3x4 = &cryTransform;

        // Create physical entity
        IPhysicalEntity* rawPhysicalEntityPtr = gEnv->pPhysicalWorld->CreatePhysicalEntity(
                GetPhysicsType(), // type
                &positionParameters, // params
                static_cast<uint64>(GetEntityId()), // pForeignData
                PHYS_FOREIGN_ID_COMPONENT_ENTITY, // iForeignData
                -1, // id
                nullptr); // IGeneralMemoryHeap

        if (!rawPhysicalEntityPtr) // probably can't happen, but handle it just in case
        {
            AZ_Assert(false, "Failed to create physical entity.");
            return;
        }

        // IPhysicalEntity is owned by IPhysicalWorld and will not be destroyed until both:
        // - IPhysicalEntity's internal refcount has dropped to zero.
        // - IPhysicalWorld::DestroyPhysicalEntity() has been called on it.
        // We store IPhysicalEntity in a ptr whose custom destructor
        // ensures that these steps are all followed.
        rawPhysicalEntityPtr->AddRef();
        m_physicalEntity.reset(rawPhysicalEntityPtr,
            [](IPhysicalEntity* physicalEntity)
            {
                physicalEntity->Release();
                gEnv->pPhysicalWorld->DestroyPhysicalEntity(physicalEntity);
            });

        // Let subclass configure the physical entity.
        ConfigurePhysicalEntity();

        if (CanInteractWithProximityTriggers())
        {
            // Create Proximity trigger proxy
            EBUS_EVENT_RESULT(m_proximityTriggerProxy, ProximityTriggerSystemRequestBus, CreateEntity, GetEntityId());
            UpdateProximityTriggerProxyAABB();
        }

        // Listen to the physics system for events concerning this entity.
        EntityPhysicsEventBus::Handler::BusConnect(GetEntityId());

        // Add colliders from self and descendants.
        // Note that the PhysicsComponent isn't "fully enabled" until it has
        // geometry from a collider, which might not happen immediately.
        // For example, a MeshColliderComponent might have to wait several frames
        // for a MeshAsset to finish loading.
        AddCollidersFromEntityAndDescendants(GetEntityId());
    }

    void PhysicsComponent::DisablePhysics()
    {
        if (!m_physicalEntity)
        {
            return;
        }

        // Send notification
        if (m_isPhysicsFullyEnabled)
        {
            EBUS_EVENT_ID(GetEntityId(), PhysicsComponentNotificationBus, OnPhysicsDisabled);
            m_isPhysicsFullyEnabled = false;
        }

        // Remove Proximity trigger proxy
        if (m_proximityTriggerProxy)
        {
            EBUS_EVENT(ProximityTriggerSystemRequestBus, RemoveEntity, m_proximityTriggerProxy, false);
            m_proximityTriggerProxy = nullptr;
        }

        // Disconnect from buses concerning the live physics object
        EntityPhysicsEventBus::Handler::BusDisconnect();
        PhysicsSystemEventBus::Handler::BusDisconnect();

        // Stop listening for events from self and descendants
        ColliderComponentEventBus::MultiHandler::BusDisconnect();
        AZ::EntityBus::MultiHandler::BusDisconnect();
        AZ::TransformNotificationBus::MultiHandler::BusDisconnect();

        m_physicalEntity.reset();
        m_contributingColliders.clear();
        m_nextPartId = 0;
        m_changedGeometrySyncState = SyncState::Synced;
    }

    IPhysicalEntity* PhysicsComponent::GetPhysicalEntity()
    {
        return m_physicalEntity.get();
    }

    void PhysicsComponent::GetPhysicsParameters(pe_params& outParameters)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->GetParams(&outParameters);
        }
    }

    void PhysicsComponent::SetPhysicsParameters(const pe_params& parameters)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->SetParams(&parameters);
        }
    }

    void PhysicsComponent::GetPhysicsStatus(pe_status& outStatus)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->GetStatus(&outStatus);
        }
    }

    void PhysicsComponent::ApplyPhysicsAction(const pe_action& action, bool threadSafe)
    {
        if (m_physicalEntity)
        {
            m_physicalEntity->Action(&action, threadSafe ? 1 : 0);
        }
    }

    void PhysicsComponent::AddCollidersFromEntityAndDescendants(const AZ::EntityId& rootEntityId)
    {
        AZ_Assert(m_physicalEntity, "Shouldn't be adding colliders while physics is disabled.");

        // Descendants are sure to be active, so we can query their colliders now.
        AZStd::vector<AZ::EntityId> entityAndDescendants;
        EBUS_EVENT_ID_RESULT(entityAndDescendants, rootEntityId, AZ::TransformBus, GetEntityAndAllDescendants);
        for (auto& entityId : entityAndDescendants)
        {
            AddCollidersFromEntity(entityId);

            // Listen for collider events.
            ColliderComponentEventBus::MultiHandler::BusConnect(entityId);

            // Listen for further descendants being added.
            AZ::TransformNotificationBus::MultiHandler::BusConnect(entityId);
        }
    }

    void PhysicsComponent::AddCollidersFromEntity(const AZ::EntityId& entityId)
    {
        AZ_Assert(m_physicalEntity, "Shouldn't be adding colliders while physics is disabled.");
        AZ_Assert(m_contributingColliders.find(entityId) == m_contributingColliders.end(), "Physics already has colliders from this entity.");

        int finalPartId = ColliderComponentRequests::NoPartsAdded;
        EBUS_EVENT_ID_RESULT(finalPartId, entityId, ColliderComponentRequestBus, AddColliderToPhysicalEntity, *m_physicalEntity, m_nextPartId);

        if (finalPartId == ColliderComponentRequests::NoPartsAdded)
        {
            return;
        }

        m_nextPartId = finalPartId + 1;
        m_contributingColliders.insert(entityId);

        // We need to call ConfigureCollisionGeometry() after changes to geometry have been processed.
        // Set the state to 'queued' and begin listening for Pre/Post physics updates.
        m_changedGeometrySyncState = SyncState::Queued;
        if (!PhysicsSystemEventBus::Handler::BusIsConnected())
        {
            PhysicsSystemEventBus::Handler::BusConnect();
        }

        // Send the OnPhysicsEnabled notification now that we have some geometry.
        if (!m_isPhysicsFullyEnabled)
        {
            m_isPhysicsFullyEnabled = true;
            EBUS_EVENT_ID(GetEntityId(), PhysicsComponentNotificationBus, OnPhysicsEnabled);
        }
    }

    void PhysicsComponent::OnEntityActivated(const AZ::EntityId& entityId)
    {
        AZ_Assert(entityId != GetEntityId(), "Shouldn't be connected to our own EntityBus");
        AZ_Assert(m_physicalEntity, "Shouldn't be listening for entity activation when physics is disabled.");

        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);

        // Now that entity is active, try to add its colliders.
        AddCollidersFromEntityAndDescendants(entityId);
    }

    void PhysicsComponent::OnChildAdded(AZ::EntityId childId)
    {
        if (m_physicalEntity)
        {
            // We want to add colliders from child and its descendants,
            // but we need to wait until child is fully activated.
            // (If child already active, OnEntityActivated fires immediately)
            AZ::EntityBus::MultiHandler::BusConnect(childId);
        }
    }

    void PhysicsComponent::OnColliderChanged()
    {
        AZ_Assert(m_physicalEntity, "Shouldn't be listening for collider events when physics is disabled.");

        // Do we already have geometry from this collider?
        const AZ::EntityId& colliderId = *ColliderComponentEventBus::GetCurrentBusId();
        if (m_contributingColliders.find(colliderId) != m_contributingColliders.end())
        {
            // We don't have a way to remove or modify pre-existing colliders,
            // so if a pre-existing collider has changed we reset everything.
            DisablePhysics();
            EnablePhysics();
        }
        else
        {
            AddCollidersFromEntity(colliderId);
        }
    }

    void PhysicsComponent::OnPostStep(const PostStep& event)
    {
        // Inform the TransformComponent that we've been moved by the physics system
        AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(event.m_entityRotation, event.m_entityPosition);

        // Maintain scale (this must be precise).
        AZ::Transform entityTransform = AZ::Transform::Identity();
        EBUS_EVENT_ID_RESULT(entityTransform, GetEntityId(), AZ::TransformBus, GetWorldTM);
        transform.MultiplyByScale(entityTransform.ExtractScaleExact());

        AZ_Assert(m_isApplyingPhysicsToEntityTransform == false, "two post steps received before a transform event");
        m_isApplyingPhysicsToEntityTransform = true;
        EBUS_EVENT_ID(event.m_entity, AZ::TransformBus, SetWorldTM, transform);
        m_isApplyingPhysicsToEntityTransform = false;
    }

    void PhysicsComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        // Don't care about transform changes on descendants.
        if (*AZ::TransformNotificationBus::GetCurrentBusId() != GetEntityId())
        {
            return;
        }

        // Because physics can create a change in transform through OnPostStep,
        // we need to make sure we are not creating a cycle
        if (!m_isApplyingPhysicsToEntityTransform)
        {
            pe_params_pos positionParameters;
            Matrix34 geomTransform = AZTransformToLYTransform(world);
            positionParameters.pMtx3x4 = &geomTransform;
            SetPhysicsParameters(positionParameters);
        }

        UpdateProximityTriggerProxyAABB();
    }


    void PhysicsComponent::OnPrePhysicsUpdate()
    {
        // Queued requests will be processed during the upcoming physics-update.
        if (m_changedGeometrySyncState == SyncState::Queued)
        {
            m_changedGeometrySyncState = SyncState::Processing;
        }
    }

    void PhysicsComponent::OnPostPhysicsUpdate()
    {
        AZ_Assert(m_physicalEntity, "Shouldn't be listening for updates when physics is disabled.");

        // Requests processed during the preceding physics-update have taken effect.
        if (m_changedGeometrySyncState == SyncState::Processing)
        {
            m_changedGeometrySyncState = SyncState::Synced;

            ConfigureCollisionGeometry();

            PhysicsSystemEventBus::Handler::BusDisconnect();
        }
    }

    void PhysicsComponent::UpdateProximityTriggerProxyAABB()
    {
        // If the proximity trigger entity proxy exists then update its aabb
        if (m_proximityTriggerProxy)
        {
            AZ::Aabb azAabb = GetAabb();
            AABB lyAabb = AZAabbToLyAABB(azAabb);
            ProximityTriggerSystemRequestBus::Broadcast(&ProximityTriggerSystemRequests::MoveEntity, m_proximityTriggerProxy, Vec3(0), lyAabb);
        }
    }

} // namespace LmbrCentral
