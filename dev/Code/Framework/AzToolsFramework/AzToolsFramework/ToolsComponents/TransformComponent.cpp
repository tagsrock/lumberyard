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
#include "stdafx.h"
#include "TransformComponent.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Slice/SliceComponent.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Network/NetBindable.h>
#include <AzFramework/Math/MathUtils.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>
#include <AzToolsFramework/ToolsComponents/TransformComponentBus.h>

namespace AzToolsFramework
{
    namespace Components
    {
        namespace Internal
        {
            // Decompose a transform into euler angles in degrees, scale (along basis, any shear will be dropped), and translation.
            void DecomposeTransform(const AZ::Transform& transform, AZ::Vector3& translation, AZ::Vector3& rotation, AZ::Vector3& scale)
            {
                AZ::Transform tx = transform;
                scale = tx.ExtractScaleExact();
                translation = tx.GetTranslation();
                rotation = AzFramework::ConvertTransformToEulerDegrees(tx);
            }

            bool TransformComponentDataConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
            {
                if (classElement.GetVersion() < 6)
                {
                    // In v6, "Slice Transform" became slice-relative.
                    const int sliceRelTransformIdx = classElement.FindElement(AZ_CRC("Slice Transform", 0x4f156fd1));
                    if (sliceRelTransformIdx >= 0)
                    {
                    // Convert slice-relative transform/root to standard parent-child relationship.
                    const int sliceRootIdx = classElement.FindElement(AZ_CRC("Slice Root", 0x9f115e1f));
                    const int parentIdx = classElement.FindElement(AZ_CRC("Parent Entity", 0x5b1b276c));
                    const int editorTransformIdx = classElement.FindElement(AZ_CRC("Transform Data", 0xf0a2bb50));
                    const int cachedTransformIdx = classElement.FindElement(AZ_CRC("Cached World Transform", 0x571fab30));

                    if (editorTransformIdx >= 0 && sliceRootIdx >= 0 && parentIdx >= 0)
                    {
                        auto& sliceTransformElement = classElement.GetSubElement(sliceRelTransformIdx);
                        auto& sliceRootElement = classElement.GetSubElement(sliceRootIdx);
                        auto& parentElement = classElement.GetSubElement(parentIdx);
                        auto& editorTransformElement = classElement.GetSubElement(editorTransformIdx);

                        AZ::Transform sliceRelTransform;
                        if (sliceTransformElement.GetData(sliceRelTransform))
                        {
                            // If the entity already has a parent assigned, we don't need to fix anything up.
                            // We only need to convert slice root to parent for non-child entities.
                            const int parentIdValueIdx = parentElement.FindElement(AZ_CRC("id", 0xbf396750));
                            AZ::u64 parentId = 0;
                            if (parentIdValueIdx >= 0)
                            {
                                parentElement.GetSubElement(parentIdValueIdx).GetData(parentId);
                            }

                            AZ::EntityId sliceRootId;
                            const int entityIdValueIdx = sliceRootElement.FindElement(AZ_CRC("id", 0xbf396750));

                            if (entityIdValueIdx < 0)
                            {
                                return false;
                            }

                            if (parentId == static_cast<AZ::u64>(AZ::EntityId()) && sliceRootElement.GetSubElement(entityIdValueIdx).GetData(sliceRootId))
                            {
                                // Upgrading the data itself is only relevant when a slice root was actually defined.
                                if (sliceRootId.IsValid())
                                {
                                    // Cached transforms weren't nullified in really old slices.
                                    if (cachedTransformIdx >= 0)
                                    {
                                        auto& cachedTransformElement = classElement.GetSubElement(cachedTransformIdx);
                                        cachedTransformElement.Convert<AZ::Transform>(context);
                                        cachedTransformElement.SetData(context, AZ::Transform::Identity());
                                    }

                                    // Our old slice root Id is now our parent Id.
                                    // Note - this could be ourself, but we can't know yet, so it gets fixed up in Init().
                                    parentElement.Convert<AZ::EntityId>(context);
                                    parentElement.SetData(context, sliceRootId);

                                    // Decompose the old slice-relative transform and set it as a our editor transform,
                                    // since the entity is now our parent.
                                    EditorTransform editorTransform;
                                    DecomposeTransform(sliceRelTransform, editorTransform.m_translate, editorTransform.m_rotate, editorTransform.m_scale);
                                    editorTransformElement.Convert<EditorTransform>(context);
                                    editorTransformElement.SetData(context, editorTransform);
                                }
                            }

                            // Finally, remove old fields.
                            classElement.RemoveElementByName(AZ_CRC("Slice Transform", 0x4f156fd1));
                            classElement.RemoveElementByName(AZ_CRC("Slice Root", 0x9f115e1f));
                            }
                        }
                        }
                    }

                if (classElement.GetVersion() < 7)
                {
                    // "IsStatic" added at v7.
                    // Old versions of TransformComponent are assumed to be non-static.
                    classElement.AddElementWithData(context, "IsStatic", false);
                }

                return true;
            }
        } // namespace Internal

        TransformComponent::TransformComponent()
            : m_suppressTransformChangedEvent(false)
            , m_cachedWorldTransform(AZ::Transform::Identity())
            , m_isSyncEnabled(true)
            , m_parentActivationTransformMode(AzFramework::TransformComponent::ParentActivationTransformMode::MaintainOriginalRelativeTransform)
            , m_isStatic(false)
        {
        }

        TransformComponent::~TransformComponent()
        {
        }

        void TransformComponent::Init()
        {
            // Required only after an up-conversion from version < 6 to >= 6.
            // We used to store slice root entity Id, which could be our own Id.
            // Since we don't have an entity association during data conversion,
            // we have to fix up this case post-entity-assignment.
            if (m_parentEntityId == GetEntityId())
            {
                m_parentEntityId = AZ::EntityId();
            }
        }

        void TransformComponent::Activate()
        {
            TransformComponentMessages::Bus::Handler::BusConnect(GetEntityId());
            AZ::TransformBus::Handler::BusConnect(GetEntityId());

            // for drag + drop child entity from one parent to another, undo/redo
            if (m_parentEntityId.IsValid())
            {
                AZ::EntityBus::Handler::BusConnect(m_parentEntityId);

                m_previousParentEntityId = m_parentEntityId;
 
                EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, EntityParentChanged, GetEntityId(), m_parentEntityId, AZ::EntityId());
                EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, EntityParentChanged, GetEntityId(), m_parentEntityId, AZ::EntityId());
            }
            // it includes the process of create/delete entity
            else
            {
                CheckApplyCachedWorldTransform(AZ::Transform::Identity());
                UpdateCachedWorldTransform();
            }
        }

        void TransformComponent::Deactivate()
        {
            AZ::TransformHierarchyInformationBus::Handler::BusDisconnect();
            TransformComponentMessages::Bus::Handler::BusDisconnect();
            AZ::TransformBus::Handler::BusDisconnect();

            AZ::TransformNotificationBus::MultiHandler::BusDisconnect();
            AZ::EntityBus::Handler::BusDisconnect();
        }

        // This is called when our transform changes directly, or our parent's has changed.
        void TransformComponent::OnTransformChanged(const AZ::Transform& /*parentLocalTM*/, const AZ::Transform& parentWorldTM)
        {
            if (GetEntity())
            {
                SetDirty();

                // Update parent-relative transform.
                auto localTM = GetLocalTM();
                auto worldTM = parentWorldTM * localTM;

                UpdateCachedWorldTransform();

                EBUS_EVENT_ID(GetEntityId(), AZ::TransformNotificationBus, OnTransformChanged, localTM, worldTM);
            }
        }

        void TransformComponent::OnTransformChanged()
        {
            if (!m_suppressTransformChangedEvent)
            {
                auto parent = GetParentTransformComponent();
                if (parent)
                {
                    OnTransformChanged(parent->GetLocalTM(), parent->GetWorldTM());
                }
                else
                {
                    OnTransformChanged(AZ::Transform::Identity(), AZ::Transform::Identity());
                }
            }
        }

        void TransformComponent::UpdateCachedWorldTransform()
        {
            const AZ::Transform& worldTransform = GetWorldTM();
            if (m_cachedWorldTransformParent != m_parentEntityId || !worldTransform.IsClose(m_cachedWorldTransform))
            {
                m_cachedWorldTransformParent = GetParentId();
                m_cachedWorldTransform = GetWorldTM();
                if (GetEntity())
                {
                    SetDirty();
                }
            }
        }

        void TransformComponent::ClearCachedWorldTransform()
        {
            m_cachedWorldTransform = AZ::Transform::Identity();
            m_cachedWorldTransformParent = AZ::EntityId();
        }

        void TransformComponent::CheckApplyCachedWorldTransform(const AZ::Transform& parentWorld)
        {
            if (m_parentEntityId != m_cachedWorldTransformParent)
            {
                if (!m_cachedWorldTransform.IsClose(AZ::Transform::Identity()))
                {
                    SetLocalTM(parentWorld.GetInverseFull() * m_cachedWorldTransform);
                }
            }
        }

        AZ::Transform TransformComponent::GetLocalTranslationTM() const
        {
            return AZ::Transform::CreateTranslation(m_editorTransform.m_translate);
        }

        AZ::Transform TransformComponent::GetLocalRotationTM() const
        {
            return AzFramework::ConvertEulerDegreesToTransformPrecise(m_editorTransform.m_rotate);
        }

        AZ::Transform TransformComponent::GetLocalScaleTM() const
        {
            return AZ::Transform::CreateScale(m_editorTransform.m_scale);
        }

        const AZ::Transform& TransformComponent::GetLocalTM()
        {
            m_localTransformCache = GetLocalTranslationTM() * GetLocalRotationTM() * GetLocalScaleTM();
            return m_localTransformCache;
        }

        // given a local transform, update local transform.
        void TransformComponent::SetLocalTM(const AZ::Transform& finalTx)
        {
            AZ::Vector3 tx, rot, scale;
            Internal::DecomposeTransform(finalTx, tx, rot, scale);

            m_editorTransform.m_translate = tx;
            m_editorTransform.m_rotate = rot;
            m_editorTransform.m_scale = scale;

            OnTransformChanged();
        }

        const EditorTransform& TransformComponent::GetLocalEditorTransform()
        {
            return m_editorTransform;
        }

        void TransformComponent::SetLocalEditorTransform(const EditorTransform& dest)
        {
            m_editorTransform = dest;

            OnTransformChanged();
        }

        const AZ::Transform& TransformComponent::GetWorldTM()
        {
            m_worldTransformCache = GetParentWorldTM() * GetLocalTM();
            return m_worldTransformCache;
        }

        void TransformComponent::SetWorldTM(const AZ::Transform& finalTx)
        {
            AZ::Transform parentGlobalToWorldInverse = GetParentWorldTM().GetInverseFull();
            SetLocalTM(parentGlobalToWorldInverse * finalTx);
        }

        void TransformComponent::GetLocalAndWorld(AZ::Transform& localTM, AZ::Transform& worldTM)
        {
            localTM = GetLocalTM();
            worldTM = GetWorldTM();
        }

        void TransformComponent::SetWorldTranslation(const AZ::Vector3& newPosition)
        {
            AZ::Transform newWorldTransform = GetWorldTM();
            newWorldTransform.SetTranslation(newPosition);
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetLocalTranslation(const AZ::Vector3& newPosition)
        {
            AZ::Transform newLocalTransform = GetLocalTM();
            newLocalTransform.SetTranslation(newPosition);
            SetLocalTM(newLocalTransform);
        }

        AZ::Vector3 TransformComponent::GetWorldTranslation()
        {
            return GetWorldTM().GetPosition();
        }

        AZ::Vector3 TransformComponent::GetLocalTranslation()
        {
            return GetLocalTM().GetPosition();
        }

        void TransformComponent::MoveEntity(const AZ::Vector3& offset)
        {
            const AZ::Vector3& worldPosition = GetWorldTM().GetPosition();
            SetWorldTranslation(worldPosition + offset);
        }

        void TransformComponent::SetWorldX(float newX)
        {
            const AZ::Vector3& worldPosition = GetWorldTM().GetPosition();
            SetWorldTranslation(AZ::Vector3(newX, worldPosition.GetY(), worldPosition.GetZ()));
        }

        void TransformComponent::SetWorldY(float newY)
        {
            const AZ::Vector3& worldPosition = GetWorldTM().GetPosition();
            SetWorldTranslation(AZ::Vector3(worldPosition.GetX(), newY, worldPosition.GetZ()));
        }

        void TransformComponent::SetWorldZ(float newZ)
        {
            const AZ::Vector3& worldPosition = GetWorldTM().GetPosition();
            SetWorldTranslation(AZ::Vector3(worldPosition.GetX(), worldPosition.GetY(), newZ));
        }

        float TransformComponent::GetWorldX()
        {
            return GetWorldTranslation().GetX();
        }

        float TransformComponent::GetWorldY()
        {
            return GetWorldTranslation().GetY();
        }

        float TransformComponent::GetWorldZ()
        {
            return GetWorldTranslation().GetZ();
        }

        void TransformComponent::SetLocalX(float x)
        {
            m_editorTransform.m_translate.SetX(x);
            TransformChanged();
        }

        void TransformComponent::SetLocalY(float y)
        {
            m_editorTransform.m_translate.SetY(y);
            TransformChanged();
        }

        void TransformComponent::SetLocalZ(float z)
        {
            m_editorTransform.m_translate.SetZ(z);
            TransformChanged();
        }

        float TransformComponent::GetLocalX()
        {
            return m_editorTransform.m_translate.GetX();
        }

        float TransformComponent::GetLocalY()
        {
            return m_editorTransform.m_translate.GetY();
        }

        float TransformComponent::GetLocalZ()
        {
            return m_editorTransform.m_translate.GetZ();
        }

        void TransformComponent::SetRotation(const AZ::Vector3& eulerAnglesRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetRotation is deprecated, please use SetLocalRotation");

            AZ::Transform newWorldTransform = GetWorldTM();
            newWorldTransform.SetRotationPartFromQuaternion(AzFramework::ConvertEulerRadiansToQuaternion(eulerAnglesRadians));
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetRotationQuaternion(const AZ::Quaternion& quaternion)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetRotationQuaternion is deprecated, please use SetLocalRotation");

            AZ::Transform newWorldTransform = GetWorldTM();
            newWorldTransform.SetRotationPartFromQuaternion(quaternion);
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetRotationX(float eulerAngleRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetRotationX is deprecated, please use SetLocalRotation");

            AZ::Transform newWorldTransform = GetWorldTM();
            newWorldTransform.SetRotationPartFromQuaternion(AZ::Quaternion::CreateRotationX(eulerAngleRadians));
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetRotationY(float eulerAngleRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetRotationY is deprecated, please use SetLocalRotation");

            AZ::Transform newWorldTransform = GetWorldTM();
            newWorldTransform.SetRotationPartFromQuaternion(AZ::Quaternion::CreateRotationY(eulerAngleRadians));
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetRotationZ(float eulerAngleRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetRotationZ is deprecated, please use SetLocalRotation");

            AZ::Transform newWorldTransform = GetWorldTM();
            newWorldTransform.SetRotationPartFromQuaternion(AZ::Quaternion::CreateRotationZ(eulerAngleRadians));
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::RotateByX(float eulerAngleRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "RotateByX is deprecated, please use RotateAroundLocalX");

            SetWorldTM(GetWorldTM() * AZ::Transform::CreateRotationX(eulerAngleRadians));
        }

        void TransformComponent::RotateByY(float eulerAngleRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "RotateByY is deprecated, please use RotateAroundLocalY");

            SetWorldTM(GetWorldTM() * AZ::Transform::CreateRotationY(eulerAngleRadians));
        }

        void TransformComponent::RotateByZ(float eulerAngleRadians)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "RotateByZ is deprecated, please use RotateAroundLocalZ");

            SetWorldTM(GetWorldTM() * AZ::Transform::CreateRotationZ(eulerAngleRadians));
        }

        AZ::Vector3 TransformComponent::GetRotationEulerRadians()
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "GetRotationEulerRadians is deprecated, please use GetWorldRotation");

            return AzFramework::ConvertTransformToEulerRadians(GetWorldTM());
        }

        AZ::Quaternion TransformComponent::GetRotationQuaternion()
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "GetRotationQuaternion is deprecated, please use GetWorldRotationQuaternion");

            return AZ::Quaternion::CreateFromTransform(GetWorldTM());
        }

        float TransformComponent::GetRotationX()
        {
            return GetRotationEulerRadians().GetX();
        }

        float TransformComponent::GetRotationY()
        {
            return GetRotationEulerRadians().GetY();
        }

        float TransformComponent::GetRotationZ()
        {
            return GetRotationEulerRadians().GetZ();
        }

        AZ::Vector3 TransformComponent::GetWorldRotation()
        {
            AZ::Transform rotate = GetWorldTM();
            rotate.ExtractScaleExact();
            AZ::Vector3 angles = AzFramework::ConvertTransformToEulerRadians(rotate);
            return angles;
        }

        AZ::Quaternion TransformComponent::GetWorldRotationQuaternion()
        {
            AZ::Transform rotate = GetWorldTM();
            rotate.ExtractScaleExact();
            AZ::Quaternion quat = AZ::Quaternion::CreateFromTransform(rotate);
            return quat;
        }

        void TransformComponent::SetLocalRotation(const AZ::Vector3& eulerAnglesRadian)
        {
            m_editorTransform.m_rotate = AzFramework::RadToDeg(eulerAnglesRadian);
            TransformChanged();
        }

        void TransformComponent::SetLocalRotationQuaternion(const AZ::Quaternion& quaternion)
        {
            m_editorTransform.m_rotate = AzFramework::ConvertQuaternionToEulerDegrees(quaternion);
            TransformChanged();
        }

        void TransformComponent::RotateAroundLocalX(float eulerAngleRadian)
        {
            AZ::Transform localRotate = AzFramework::ConvertEulerDegreesToTransformPrecise(m_editorTransform.m_rotate);
            AZ::Vector3 xAxis = localRotate.GetBasisX();
            AZ::Quaternion xRotate = AZ::Quaternion::CreateFromAxisAngle(xAxis, eulerAngleRadian);
            AZ::Quaternion currentRotate = AzFramework::ConvertEulerDegreesToQuaternion(m_editorTransform.m_rotate);
            AZ::Quaternion newRotate = xRotate * currentRotate;
            newRotate.NormalizeExact();
            m_editorTransform.m_rotate = AzFramework::ConvertQuaternionToEulerDegrees(newRotate);

            TransformChanged();
        }

        void TransformComponent::RotateAroundLocalY(float eulerAngleRadian)
        {
            AZ::Transform localRotate = AzFramework::ConvertEulerDegreesToTransformPrecise(m_editorTransform.m_rotate);
            AZ::Vector3 yAxis = localRotate.GetBasisY();
            AZ::Quaternion yRotate = AZ::Quaternion::CreateFromAxisAngle(yAxis, eulerAngleRadian);
            AZ::Quaternion currentRotate = AzFramework::ConvertEulerDegreesToQuaternion(m_editorTransform.m_rotate);
            AZ::Quaternion newRotate = yRotate * currentRotate;
            newRotate.NormalizeExact();
            m_editorTransform.m_rotate = AzFramework::ConvertQuaternionToEulerDegrees(newRotate);

            TransformChanged();
        }

        void TransformComponent::RotateAroundLocalZ(float eulerAngleRadian)
        {
            AZ::Transform localRotate = AzFramework::ConvertEulerDegreesToTransformPrecise(m_editorTransform.m_rotate);
            AZ::Vector3 zAxis = localRotate.GetBasisZ();
            AZ::Quaternion zRotate = AZ::Quaternion::CreateFromAxisAngle(zAxis, eulerAngleRadian);
            AZ::Quaternion currentRotate = AzFramework::ConvertEulerDegreesToQuaternion(m_editorTransform.m_rotate);
            AZ::Quaternion newRotate = zRotate * currentRotate;
            newRotate.NormalizeExact();
            m_editorTransform.m_rotate = AzFramework::ConvertQuaternionToEulerDegrees(newRotate);

            TransformChanged();
        }

        AZ::Vector3 TransformComponent::GetLocalRotation()
        {
            AZ::Vector3 result = AzFramework::DegToRad(m_editorTransform.m_rotate);
            return result;
        }

        AZ::Quaternion TransformComponent::GetLocalRotationQuaternion()
        {
            AZ::Quaternion result = AzFramework::ConvertEulerDegreesToQuaternion(m_editorTransform.m_rotate);
            return result;
        }

        void TransformComponent::SetScale(const AZ::Vector3& newScale)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetScale is deprecated, please use SetLocalScale");

            AZ::Transform newWorldTransform = GetWorldTM();
            AZ::Vector3 prevScale = newWorldTransform.ExtractScale();
            if (!prevScale.IsClose(newScale))
            {
                newWorldTransform.MultiplyByScale(newScale);
                SetWorldTM(newWorldTransform);
            }
        }

        void TransformComponent::SetScaleX(float newScale)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetScaleX is deprecated, please use SetLocalScaleX");

            AZ::Transform newWorldTransform = GetWorldTM();
            AZ::Vector3 scale = newWorldTransform.ExtractScale();
            scale.SetX(newScale);
            newWorldTransform.MultiplyByScale(scale);
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetScaleY(float newScale)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetScaleY is deprecated, please use SetLocalScaleY");

            AZ::Transform newWorldTransform = GetWorldTM();
            AZ::Vector3 scale = newWorldTransform.ExtractScale();
            scale.SetY(newScale);
            newWorldTransform.MultiplyByScale(scale);
            SetWorldTM(newWorldTransform);
        }

        void TransformComponent::SetScaleZ(float newScale)
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "SetScaleZ is deprecated, please use SetLocalScaleZ");

            AZ::Transform newWorldTransform = GetWorldTM();
            AZ::Vector3 scale = newWorldTransform.ExtractScale();
            scale.SetZ(newScale);
            newWorldTransform.MultiplyByScale(scale);
            SetWorldTM(newWorldTransform);
        }

        AZ::Vector3 TransformComponent::GetScale()
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "GetScale is deprecated, please use GetLocalScale");

            return GetWorldTM().RetrieveScale();
        }

        float TransformComponent::GetScaleX()
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "GetScaleX is deprecated, please use GetLocalScale");

            AZ::Vector3 scale = GetWorldTM().RetrieveScale();
            return scale.GetX();
        }

        float TransformComponent::GetScaleY()
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "GetScaleY is deprecated, please use GetLocalScale");

            AZ::Vector3 scale = GetWorldTM().RetrieveScale();
            return scale.GetY();
        }

        float TransformComponent::GetScaleZ()
        {
            AZ_Warning("AzToolsFramework::TransformComponent", false, "GetScaleZ is deprecated, please use GetLocalScale");

            AZ::Vector3 scale = GetWorldTM().RetrieveScale();
            return scale.GetZ();
        }

        void TransformComponent::SetLocalScale(const AZ::Vector3& scale)
        {
            m_editorTransform.m_scale = scale;
            TransformChanged();
        }

        void TransformComponent::SetLocalScaleX(float scaleX)
        {
            m_editorTransform.m_scale.SetX(scaleX);
            TransformChanged();
        }

        void TransformComponent::SetLocalScaleY(float scaleY)
        {
            m_editorTransform.m_scale.SetY(scaleY);
            TransformChanged();
        }

        void TransformComponent::SetLocalScaleZ(float scaleZ)
        {
            m_editorTransform.m_scale.SetZ(scaleZ);
            TransformChanged();
        }

        AZ::Vector3 TransformComponent::GetLocalScale()
        {
            return m_editorTransform.m_scale;
        }

        const AZ::Transform& TransformComponent::GetParentWorldTM() const
        {
            auto parent = GetParentTransformComponent();
            if (parent)
            {
                return parent->GetWorldTM();
            }
            return AZ::Transform::Identity();
        }

        void TransformComponent::SetParentImpl(AZ::EntityId parentId, bool relative)
        {
            // If the parent id to be set is the same as the current parent id
            // Or if the component belongs to an entity and the entity's id is the same as the id being set as parent
            if (parentId == m_parentEntityId || (GetEntity() && (GetEntityId() == parentId)))
            {
                return;
            }

            // Entity is not associated if we're just doing data preparation (slice construction).
            if (!GetEntity() || GetEntity()->GetState() == AZ::Entity::ES_CONSTRUCTED)
            {
                m_previousParentEntityId = m_parentEntityId = parentId;
                return;
            }

            // Prevent this from parenting to its own child. Check if this entity is in the new parent's hierarchy.
            auto potentialParentTransformComponent = GetTransformComponent(parentId);
            if (potentialParentTransformComponent && potentialParentTransformComponent->IsEntityInHierarchy(GetEntityId()))
            {
                return;
            }

            auto oldParentId = m_parentEntityId;

            // SetLocalTM calls below can confuse listeners, because transforms are mathematically
            // detached before the ParentChanged events are dispatched. Suppress OnTransformChanged()
            // until the transaction is complete.
            m_suppressTransformChangedEvent = true;

            if (m_parentEntityId.IsValid())
            {
                AZ::TransformHierarchyInformationBus::Handler::BusDisconnect();
                AZ::TransformNotificationBus::MultiHandler::BusDisconnect(m_parentEntityId);
                AZ::EntityBus::Handler::BusDisconnect(m_parentEntityId);

                if (!relative)
                {
                    SetLocalTM(GetParentWorldTM() * GetLocalTM());
                }

                TransformComponent* parentTransform = GetParentTransformComponent();
                if (parentTransform)
                {
                    auto& parentChildIds = GetParentTransformComponent()->m_childrenEntityIds;
                    parentChildIds.erase(AZStd::remove(parentChildIds.begin(), parentChildIds.end(), GetEntityId()), parentChildIds.end());
                }

                m_parentEntityId.SetInvalid();
                m_previousParentEntityId = m_parentEntityId;
            }

            if (parentId.IsValid())
            {
                AZ::TransformNotificationBus::MultiHandler::BusConnect(parentId);
                AZ::TransformHierarchyInformationBus::Handler::BusConnect(parentId);

                m_parentEntityId = parentId;
                m_previousParentEntityId = m_parentEntityId;

                if (!relative)
                {
                    AZ::Transform parentXform = GetParentWorldTM();
                    AZ::Transform inverseXform = parentXform.GetInverseFull();
                    SetLocalTM(inverseXform * GetLocalTM());
                }

                // OnEntityActivated will trigger immediately if the parent is active
                AZ::EntityBus::Handler::BusConnect(m_parentEntityId);
            }

            m_suppressTransformChangedEvent = false;

            // This is for Create Entity as child / Drag+drop parent update / add component
            EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, EntityParentChanged, GetEntityId(), parentId, oldParentId);
            EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, EntityParentChanged, GetEntityId(), parentId, oldParentId);
            EBUS_EVENT_ID(GetEntityId(), AZ::TransformNotificationBus, OnParentChanged, oldParentId, parentId);

            OnTransformChanged();
        }

        void TransformComponent::SetParent(AZ::EntityId parentId)
        {
            SetParentImpl(parentId, false);
        }

        void TransformComponent::SetParentRelative(AZ::EntityId parentId)
        {
            SetParentImpl(parentId, true);
        }

        AZ::EntityId TransformComponent::GetParentId()
        {
            return m_parentEntityId;
        }

        AZStd::vector<AZ::EntityId> TransformComponent::GetChildren()
        {
            AZStd::vector<AZ::EntityId> children;
            EBUS_EVENT_ID(GetEntityId(), AZ::TransformHierarchyInformationBus, GatherChildren, children);
            return children;
        }

        AZStd::vector<AZ::EntityId> TransformComponent::GetAllDescendants()
        {
            AZStd::vector<AZ::EntityId> descendants = GetChildren();
            for (size_t i = 0; i < descendants.size(); ++i)
            {
                EBUS_EVENT_ID(descendants[i], AZ::TransformHierarchyInformationBus, GatherChildren, descendants);
            }
            return descendants;
        }

        void TransformComponent::GatherChildren(AZStd::vector<AZ::EntityId>& children)
        {
            children.push_back(GetEntityId());
        }

        bool TransformComponent::IsStaticTransform()
        {
            return m_isStatic;
        }

        TransformComponent* TransformComponent::GetParentTransformComponent() const
        {
            return GetTransformComponent(m_parentEntityId);
        }

        TransformComponent* TransformComponent::GetTransformComponent(AZ::EntityId otherEntityId) const
        {
            if (!otherEntityId.IsValid())
            {
                return nullptr;
            }

            AZ::Entity* pEntity = nullptr;
            EBUS_EVENT_RESULT(pEntity, AZ::ComponentApplicationBus, FindEntity, otherEntityId);
            if (!pEntity)
            {
                return nullptr;
            }

            return pEntity->FindComponent<TransformComponent>();
        }

        AZ::TransformInterface* TransformComponent::GetParent()
        {
            return GetParentTransformComponent();
        }

        void TransformComponent::OnEntityActivated(const AZ::EntityId& parentEntityId)
        {
            AZ::TransformNotificationBus::MultiHandler::BusConnect(parentEntityId);
            AZ::TransformHierarchyInformationBus::Handler::BusConnect(parentEntityId);

            // Our parent entity has just been activated.
            AZ_Assert(parentEntityId == m_parentEntityId,
                "Received Activation message for an entity other than our parent.");

            TransformComponent* parentTransform = GetParentTransformComponent();
            if (parentTransform)
            {
                // Prevent circular parent/child relationships potentially generated through slice data hierarchies.
                // This doesn't only occur through direct user assignment of parent (which is handled separately),
                // but can also occur through cascading of slicces, so we need to validate on activation as well.
                if (GetEntity() && parentTransform->IsEntityInHierarchy(GetEntityId()))
                {
                    AZ_Error("Transform Component", false,
                             "Slice data propagation for Entity %s [%llu] has resulted in circular parent/child relationships. "
                             "Parent assignment for this entity has been reset.",
                             GetEntity()->GetName().c_str(),
                             GetEntityId());

                    SetParent(AZ::EntityId());
                    return;
                }

                bool isDuringUndoRedo = false;
                EBUS_EVENT_RESULT(isDuringUndoRedo, AzToolsFramework::ToolsApplicationRequests::Bus, IsDuringUndoRedo);
                if (!isDuringUndoRedo)
                {
                    // When parent comes online, compute local TM from world TM.
                    CheckApplyCachedWorldTransform(parentTransform->GetWorldTM());
                }
                else
                {
                    // During undo operations, just apply our local TM.
                    OnTransformChanged(AZ::Transform::Identity(), parentTransform->GetWorldTM());
                }

                auto& parentChildIds = GetParentTransformComponent()->m_childrenEntityIds;
                if (parentChildIds.end() == AZStd::find(parentChildIds.begin(), parentChildIds.end(), GetEntityId()))
                {
                    parentChildIds.push_back(GetEntityId());
                }
            }

            UpdateCachedWorldTransform();
        }

        void TransformComponent::OnEntityDeactivated(const AZ::EntityId& parentEntityId)
        {
            AZ_Assert(parentEntityId == m_parentEntityId,
                "Received Deactivation message for an entity other than our parent.");

            AZ::TransformNotificationBus::MultiHandler::BusDisconnect(parentEntityId);
        }

        bool TransformComponent::IsEntityInHierarchy(AZ::EntityId entityId)
        {
            /// Begin 1.7 Release hack - #TODO - LMBR-37330
            if (GetParentId() == GetEntityId())
            {
                m_parentEntityId = m_previousParentEntityId;
            }
            /// End 1.7 Release hack

            AZ::EntityId parentId = GetParentId();
            if (parentId == entityId)
            {
                return true;
            }
            if (!parentId.IsValid())
            {
                return false;
            }
            auto parentTComp = GetParentTransformComponent();
            if (!parentTComp)
            {
                return false;
            }

            return parentTComp->IsEntityInHierarchy(entityId);
        }

        AZ::u32 TransformComponent::ParentChanged()
        {
            // Prevent setting the parent to the entity itself.
            // When this happens, make sure to refresh the interface, so it goes back where it was.
            if (m_parentEntityId == GetEntityId())
            {
                m_parentEntityId = m_previousParentEntityId;
                return AZ::Edit::PropertyRefreshLevels::ValuesOnly;
            }
            auto parentId = m_parentEntityId;
            m_parentEntityId = m_previousParentEntityId;
            SetParent(parentId);

            return AZ::Edit::PropertyRefreshLevels::None;
        }

        AZ::u32 TransformComponent::TransformChanged()
        {
            OnTransformChanged();
            return AZ::Edit::PropertyRefreshLevels::None;
        }

        void TransformComponent::ModifyEditorTransform(AZ::Vector3& vec, const AZ::Vector3& data, const AZ::Transform& parentInverse)
        {
            if (data.IsZero())
            {
                return;
            }

            auto delta = parentInverse * data;

            vec += delta;

            OnTransformChanged();
        }

        void TransformComponent::TranslateBy(const AZ::Vector3& data)
        {
            auto parent = GetParentWorldTM();
            parent.SetTranslation(AZ::Vector3::CreateZero());
            parent.InvertFull();

            ModifyEditorTransform(m_editorTransform.m_translate, data, parent);
        }

        void TransformComponent::RotateBy(const AZ::Vector3& data)
        {
            auto parent = GetParentWorldTM();
            parent.SetTranslation(AZ::Vector3::CreateZero());
            parent.InvertFull();

            ModifyEditorTransform(m_editorTransform.m_rotate, data, parent);
        }

        void TransformComponent::ScaleBy(const AZ::Vector3& data)
        {
            //scale is always local
            ModifyEditorTransform(m_editorTransform.m_scale, data, AZ::Transform::Identity());
        }

        void TransformComponent::BuildGameEntity(AZ::Entity* gameEntity)
        {
            AzFramework::TransformComponentConfiguration configuration;
            configuration.m_parentId = m_parentEntityId;
            configuration.m_isBoundToNetwork = m_isSyncEnabled;
            configuration.m_transform = GetLocalTM();
            configuration.m_worldTransform = GetWorldTM();
            configuration.m_parentActivationTransformMode = m_parentActivationTransformMode;
            configuration.m_isStatic = m_isStatic;

            gameEntity->CreateComponent<AzFramework::TransformComponent>(configuration);
        }

        void TransformComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        void TransformComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        void TransformComponent::Reflect(AZ::ReflectContext* context)
        {
            // reflect data for script, serialization, editing..
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<EditorTransform>()->
                    Field("Translate", &EditorTransform::m_translate)->
                    Field("Rotate", &EditorTransform::m_rotate)->
                    Field("Scale", &EditorTransform::m_scale)->
                    Version(1);

                serializeContext->Class<TransformComponent, EditorComponentBase>()->
                    Field("Parent Entity", &TransformComponent::m_parentEntityId)->
                    Field("Transform Data", &TransformComponent::m_editorTransform)->
                    Field("Cached World Transform", &TransformComponent::m_cachedWorldTransform)->
                    Field("Cached World Transform Parent", &TransformComponent::m_cachedWorldTransformParent)->
                    Field("Sync Enabled", &TransformComponent::m_isSyncEnabled)->
                    Field("Parent Activation Transform Mode", &TransformComponent::m_parentActivationTransformMode)->
                    Field("IsStatic", &TransformComponent::m_isStatic)->
                    Version(7, &Internal::TransformComponentDataConverter);

                AZ::EditContext* ptrEdit = serializeContext->GetEditContext();
                if (ptrEdit)
                {
                    ptrEdit->Class<TransformComponent>("Transform", "Controls the placement of the entity in the world in 3d")->
                        ClassElement(AZ::Edit::ClassElements::EditorData, "")->
                            Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Transform.png")->
                            Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Transform.png")->
                            Attribute(AZ::Edit::Attributes::AutoExpand, true)->
                        DataElement(0, &TransformComponent::m_parentEntityId, "Parent entity", "")->
                            Attribute(AZ::Edit::Attributes::ChangeNotify, &TransformComponent::ParentChanged)->
                            Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::DontGatherReference | AZ::Edit::SliceFlags::NotPushableOnSliceRoot)->
                        DataElement(0, &TransformComponent::m_editorTransform, "Values", "")->
                            Attribute(AZ::Edit::Attributes::ChangeNotify, &TransformComponent::TransformChanged)->
                            Attribute(AZ::Edit::Attributes::AutoExpand, true)->
                        DataElement(AZ::Edit::UIHandlers::ComboBox, &TransformComponent::m_parentActivationTransformMode, 
                            "Parent activation", "Configures relative transform behavior when parent activates.")->
                            EnumAttribute(AzFramework::TransformComponent::ParentActivationTransformMode::MaintainOriginalRelativeTransform, "Original relative transform")->
                            EnumAttribute(AzFramework::TransformComponent::ParentActivationTransformMode::MaintainCurrentWorldTransform, "Current world transform")->
                        DataElement(0, &TransformComponent::m_isSyncEnabled, "Bind to network", "Enable binding to the network.")->
                        DataElement(0, &TransformComponent::m_isStatic ,"Static", "Static entities are highly optimized and cannot be moved during runtime.")->
                        DataElement(0, &TransformComponent::m_cachedWorldTransformParent, "Cached Parent Entity", "")->
                            Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::DontGatherReference | AZ::Edit::SliceFlags::NotPushable)->
                            Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)->
                        DataElement(0, &TransformComponent::m_cachedWorldTransform, "Cached World Transform", "")->
                            Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::NotPushable)->
                            Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)
                        ;

                    ptrEdit->Class<EditorTransform>("Values", "XYZ PYR")->
                        DataElement(0, &EditorTransform::m_translate, "Translate", "Local Position (Relative to parent) in meters.")->
                            Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                            Attribute(AZ::Edit::Attributes::Suffix, " m")->
                            Attribute(AZ::Edit::Attributes::Min, -AZ::Constants::MaxFloatBeforePrecisionLoss)->
                            Attribute(AZ::Edit::Attributes::Max, AZ::Constants::MaxFloatBeforePrecisionLoss)->
                            Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::NotPushableOnSliceRoot)->
                        DataElement(0, &EditorTransform::m_rotate, "Rotate", "Local Rotation (Relative to parent) in degrees.")->
                            Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                            Attribute(AZ::Edit::Attributes::Suffix, " deg")->
                        DataElement(0, &EditorTransform::m_scale, "Scale", "Local Scale")->
                            Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                            Attribute(AZ::Edit::Attributes::Min, 0.01f)
                        ;
                }
            }

            AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
            if (behaviorContext)
            {
                // string-name differs from class-name to avoid collisions with the other "TransformComponent" (AzFramework::TransformComponent).
                behaviorContext->Class<TransformComponent>("EditorTransformBus")->RequestBus("TransformBus");
            }
        }
    }
} // namespace AzToolsFramework
