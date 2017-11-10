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
#ifndef TRANSFORM_COMPONENT_H_
#define TRANSFORM_COMPONENT_H_

#include <AzCore/Component/Component.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Asset/assetcommon.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/EntityBus.h>

#include <AzFramework/Components/TransformComponent.h>

#include "TransformComponentBus.h"
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include "EditorComponentBase.h"
#include <AzCore/Component/EntityId.h>

#pragma once

namespace AzToolsFramework
{
    namespace Components
    {
        // the transform component is referenced by other components in the same entity
        // it is not an asset.
        class TransformComponent
            : public EditorComponentBase
            , public AZ::TransformBus::Handler
            , private TransformComponentMessages::Bus::Handler
            , private AZ::EntityBus::Handler
            , private AZ::TransformNotificationBus::MultiHandler
            , private AZ::TransformHierarchyInformationBus::Handler
        {
        public:
            friend class TransformComponentFactory;

            AZ_COMPONENT(TransformComponent, ToolsTransformComponentTypeId, EditorComponentBase)

            TransformComponent();
            virtual ~TransformComponent();

            //////////////////////////////////////////////////////////////////////////
            // AZ::EntityBus::Handler
            void OnEntityActivated(const AZ::EntityId& parentEntityId) override;
            void OnEntityDeactivated(const AZ::EntityId& parentEntityId) override;

            //////////////////////////////////////////////////////////////////////////
            // AZ::Component
            void Init() override;
            void Activate() override;
            void Deactivate() override;
            //////////////////////////////////////////////////////////////////////////

            AZ::u32 ParentChanged();
            AZ::u32 TransformChanged();

            //////////////////////////////////////////////////////////////////////////
            // AZ::TransformBus
            const AZ::Transform& GetLocalTM() override;
            void SetLocalTM(const AZ::Transform& tm) override;
            const AZ::Transform& GetWorldTM() override;
            void SetWorldTM(const AZ::Transform& tm) override;
            void GetLocalAndWorld(AZ::Transform& localTM, AZ::Transform& worldTM) override;

            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Translation modifiers

            void SetWorldTranslation(const AZ::Vector3& newPosition) override;
            void SetLocalTranslation(const AZ::Vector3& newPosition) override;

            AZ::Vector3 GetWorldTranslation() override;
            AZ::Vector3 GetLocalTranslation() override;

            void MoveEntity(const AZ::Vector3& offset) override;

            void SetWorldX(float newX) override;
            void SetWorldY(float newY) override;
            void SetWorldZ(float newZ) override;

            float GetWorldX() override;
            float GetWorldY() override;
            float GetWorldZ() override;

            void SetLocalX(float x) override;
            void SetLocalY(float y) override;
            void SetLocalZ(float z) override;

            float GetLocalX() override;
            float GetLocalY() override;
            float GetLocalZ() override;

            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Rotation modifiers
            void SetRotation(const AZ::Vector3& eulerAnglesRadians) override;
            void SetRotationQuaternion(const AZ::Quaternion& quaternion) override;
            void SetRotationX(float eulerAngleRadians) override;
            void SetRotationY(float eulerAngleRadians) override;
            void SetRotationZ(float eulerAngleRadians) override;

            void RotateByX(float eulerAngleRadians) override;
            void RotateByY(float eulerAngleRadians) override;
            void RotateByZ(float eulerAngleRadians) override;

            AZ::Vector3 GetRotationEulerRadians() override;
            AZ::Quaternion GetRotationQuaternion() override;

            float GetRotationX() override;
            float GetRotationY() override;
            float GetRotationZ() override;

            AZ::Vector3 GetWorldRotation() override;
            AZ::Quaternion GetWorldRotationQuaternion() override;

            void SetLocalRotation(const AZ::Vector3& eulerAnglesRadian) override;
            void SetLocalRotationQuaternion(const AZ::Quaternion& quaternion) override;

            void RotateAroundLocalX(float eulerAngleRadian) override;
            void RotateAroundLocalY(float eulerAngleRadian) override;
            void RotateAroundLocalZ(float eulerAngleRadian) override;

            AZ::Vector3 GetLocalRotation() override;
            AZ::Quaternion GetLocalRotationQuaternion() override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Scale Modifiers
            void SetScale(const AZ::Vector3& newScale) override;
            void SetScaleX(float newScale) override;
            void SetScaleY(float newScale) override;
            void SetScaleZ(float newScale) override;

            AZ::Vector3 GetScale() override;
            float GetScaleX() override;
            float GetScaleY() override;
            float GetScaleZ() override;

            void SetLocalScale(const AZ::Vector3& scale) override;
            void SetLocalScaleX(float scaleX) override;
            void SetLocalScaleY(float scaleY) override;
            void SetLocalScaleZ(float scaleZ) override;

            AZ::Vector3 GetLocalScale() override;
            //////////////////////////////////////////////////////////////////////////

            AZ::EntityId  GetParentId() override;
            AZ::TransformInterface* GetParent() override;
            void SetParent(AZ::EntityId parentId) override;
            void SetParentRelative(AZ::EntityId parentId) override;
            AZStd::vector<AZ::EntityId> GetChildren() override;
            AZStd::vector<AZ::EntityId> GetAllDescendants() override;
            bool IsStaticTransform() override;

            //////////////////////////////////////////////////////////////////////////
            // AZ::TransformNotificationBus
            void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;
            void OnTransformChanged(); //convienence

            //////////////////////////////////////////////////////////////////////////
            // TransformComponentMessages::Bus
            void TranslateBy(const AZ::Vector3&) override;
            void RotateBy(const AZ::Vector3&) override; // euler in degrees
            void ScaleBy(const AZ::Vector3&) override;
            const EditorTransform& GetLocalEditorTransform() override;
            void SetLocalEditorTransform(const EditorTransform& dest) override;

            /// \return true if the entity is a root-level entity (has no transform parent).
            bool IsRootEntity() const { return !m_parentEntityId.IsValid(); }

            //callable is a lambda taking a AZ::EntityId and returning nothing, will be called with the id of each child
            //Also will be called for children of children, all the way down the hierarchy
            template<typename Callable>
            void ForEachChild(Callable callable)
            {
                for (auto childId : m_childrenEntityIds)
                {
                    callable(childId);
                    auto child = GetTransformComponent(childId);
                    if (child)
                    {
                        child->ForEachChild(callable);
                    }
                }
            }

            virtual void BuildGameEntity(AZ::Entity* gameEntity) override;

            void UpdateCachedWorldTransform();
            void ClearCachedWorldTransform();

        private:

            //////////////////////////////////////////////////////////////////////////
            // TransformHierarchyInformationBus
            void GatherChildren(AZStd::vector<AZ::EntityId>& children) override;
            //////////////////////////////////////////////////////////////////////////

        private:

            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void Reflect(AZ::ReflectContext* context);

            AZ::Transform GetLocalTranslationTM() const;
            AZ::Transform GetLocalRotationTM() const;
            AZ::Transform GetLocalScaleTM() const;

            void AttachToParentImpl(AZ::EntityId parentId);
            TransformComponent* GetParentTransformComponent() const;
            TransformComponent* GetTransformComponent(AZ::EntityId otherEntityId) const;
            bool IsEntityInHierarchy(AZ::EntityId entityId);

            void SetParentImpl(AZ::EntityId parentId, bool relative);
            const AZ::Transform& GetParentWorldTM() const;

            void ModifyEditorTransform(AZ::Vector3& vec, const AZ::Vector3& data, const AZ::Transform& parentInverse);

            void CheckApplyCachedWorldTransform(const AZ::Transform& parentWorld);

            bool m_isStatic;

            AZ::EntityId m_parentEntityId;
            AZ::EntityId m_previousParentEntityId;

            EditorTransform m_editorTransform;

            //these are only used to hold onto the references returned by GetLocalTM and GetWorldTM
            AZ::Transform m_localTransformCache;
            AZ::Transform m_worldTransformCache;
            
            // Drives transform behavior when parent activates. See \ref ParentActivationTransformMode for details.
            AzFramework::TransformComponent::ParentActivationTransformMode m_parentActivationTransformMode;

            // Keeping a world transform along with a parent Id at the time of capture.
            // This is required for dealing with external changes to parent assignment (i.e. slice propagation).
            // A local transform alone isn't enough, since we may've serialized a parent-relative local transform,
            // but detached from the parent via propagation of the parent Id field. In such a case, we need to 
            // know to not erroneously apply the local-space transform we serialized in a world-space capacity.
            AZ::Transform m_cachedWorldTransform;
            AZ::EntityId m_cachedWorldTransformParent;

            AZStd::vector<AZ::EntityId> m_childrenEntityIds;

            bool m_suppressTransformChangedEvent;

            bool m_isSyncEnabled;                       // Used to serialize data required for NetBindable
        };
    }
} // namespace AzToolsFramework

#endif // TRANSFORM_COMPONENT_H_
