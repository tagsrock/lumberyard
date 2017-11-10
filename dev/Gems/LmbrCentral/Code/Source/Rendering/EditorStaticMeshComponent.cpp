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

#include <IAISystem.h>

#include "EditorStaticMeshComponent.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Rtti/BehaviorContext.h>
#include <AzCore/Asset/AssetManager.h>

#include <MathConversion.h>

#include <INavigationSystem.h> // For updating nav tiles on creation of editor physics.
#include <IPhysics.h> // For basic physicalization at edit-time for object snapping.

namespace LmbrCentral
{
    void EditorStaticMeshComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorStaticMeshComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Static Mesh Render Node", &EditorStaticMeshComponent::m_mesh)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();

            if (editContext)
            {
                editContext->Class<EditorStaticMeshComponent>("Static Mesh", "The Static Mesh component is the primary method of adding static visual geometry to entities")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Rendering")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/StaticMesh.png")
                        ->Attribute(AZ::Edit::Attributes::PrimaryAssetType, AZ::AzTypeInfo<LmbrCentral::StaticMeshAsset>::Uuid())
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/StaticMesh.png")
                        ->Attribute(AZ::Edit::Attributes::PreferNoViewportIcon, true)
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorStaticMeshComponent::m_mesh);

                editContext->Class<StaticMeshComponentRenderNode::StaticMeshRenderOptions>(
                    "Render Options", "Rendering options for the mesh.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Options")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)

                    ->DataElement(AZ::Edit::UIHandlers::Slider, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_opacity, "Opacity", "Opacity value")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, 1.f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_maxViewDist, "Max view distance", "Maximum view distance in meters.")
                        ->Attribute(AZ::Edit::Attributes::Suffix, " m")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::Max, &StaticMeshComponentRenderNode::GetDefaultMaxViewDist)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_viewDistMultiplier, "View distance multiplier", "Adjusts max view distance. If 1.0 then default is used. 1.1 would be 10% further than default.")
                        ->Attribute(AZ::Edit::Attributes::Suffix, "x")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.f)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_lodRatio, "LOD distance ratio", "Controls LOD ratio over distance.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                        ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->Attribute(AZ::Edit::Attributes::Max, 255)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_castShadows, "Cast dynamic shadows", "Casts dynamic shadows (shadow maps).")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_castLightmap, "Cast static shadows", "Casts static shadows (lightmap).")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_useVisAreas, "Use VisAreas", "Allow VisAreas to control this component's visibility.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Advanced")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, false)

                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_rainOccluder, "Rain occluder", "Occludes dynamic raindrops.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_affectDynamicWater, "Affect dynamic water", "Will generate ripples in dynamic water.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_receiveWind, "Receive wind", "Receives wind.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_acceptDecals, "Accept decals", "Can receive decals.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_affectNavmesh, "Affect navmesh", "Will affect navmesh generation.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::m_visibilityOccluder, "Visibility occluder", "Is appropriate for occluding visibility of other objects.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::StaticMeshRenderOptions::OnChanged)
                    ;

                editContext->Class<StaticMeshComponentRenderNode>(
                    "Mesh Rendering", "Attach geometry to the entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_visible, "Visible", "Is currently visible.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::RefreshRenderState)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_staticMeshAsset, "Static asset", "Static mesh asset reference")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::OnAssetPropertyChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_material, "Material override", "Optionally specify an override material.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::OnAssetPropertyChanged)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &StaticMeshComponentRenderNode::m_renderOptions, "Render options", "Render/draw options.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &StaticMeshComponentRenderNode::RefreshRenderState)
                    ;
            }
        }

        if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<EditorStaticMeshComponent>()->RequestBus("MeshComponentRequestBus");
        }
    }

    void EditorStaticMeshComponent::Activate()
    {
        EditorComponentBase::Activate();

        m_mesh.AttachToEntity(m_entity->GetId());

        bool currentVisibility = true;
        AzToolsFramework::EditorVisibilityRequestBus::EventResult(currentVisibility, GetEntityId(), &AzToolsFramework::EditorVisibilityRequests::GetCurrentVisibility);
        m_mesh.UpdateAuxiliaryRenderFlags(!currentVisibility, ERF_HIDDEN);

        // Note we are purposely connecting to buses before calling m_mesh.CreateMesh().
        // m_mesh.CreateMesh() can result in events (eg: OnMeshCreated) that we want receive.
        MaterialRequestBus::Handler::BusConnect(m_entity->GetId());
        MeshComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        MeshComponentNotificationBus::Handler::BusConnect(m_entity->GetId());
        StaticMeshComponentRequestBus::Handler::BusConnect(m_entity->GetId());
        RenderNodeRequestBus::Handler::BusConnect(m_entity->GetId());
        AZ::TransformNotificationBus::Handler::BusConnect(m_entity->GetId());
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());

        auto renderOptionsChangeCallback =
            [this]()
        {
            this->m_mesh.ApplyRenderOptions();

            AffectNavmesh();
        };
        m_mesh.m_renderOptions.m_changeCallback = renderOptionsChangeCallback;

        m_mesh.CreateMesh();
    }

    void EditorStaticMeshComponent::Deactivate()
    {
        MaterialRequestBus::Handler::BusDisconnect();
        MeshComponentRequestBus::Handler::BusDisconnect();
        MeshComponentNotificationBus::Handler::BusDisconnect();
        StaticMeshComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();

        DestroyEditorPhysics();

        m_mesh.m_renderOptions.m_changeCallback = 0;

        m_mesh.DestroyMesh();
        m_mesh.AttachToEntity(AZ::EntityId());

        EditorComponentBase::Deactivate();
    }

    void EditorStaticMeshComponent::OnMeshCreated(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        (void)asset;

        CreateEditorPhysics();

        if (m_physicalEntity)
        {
            OnTransformChanged(GetTransform()->GetLocalTM(), GetTransform()->GetWorldTM());
        }
    }

    void EditorStaticMeshComponent::OnMeshDestroyed()
    {
        DestroyEditorPhysics();
    }

    IRenderNode* EditorStaticMeshComponent::GetRenderNode()
    {
        return &m_mesh;
    }

    float EditorStaticMeshComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }

    void EditorStaticMeshComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        if (m_physicalEntity)
        {
            const AZ::Vector3 newScale = world.RetrieveScale();

            if (!m_physScale.IsClose(newScale))
            {
                // Scale changes require re-physicalizing.
                DestroyEditorPhysics();
                CreateEditorPhysics();
            }

            Matrix34 transform = AZTransformToLYTransform(world);

            pe_params_pos par_pos;
            par_pos.pMtx3x4 = &transform;
            m_physicalEntity->SetParams(&par_pos);
        }
    }

    AZ::Aabb EditorStaticMeshComponent::GetWorldBounds()
    {
        return m_mesh.CalculateWorldAABB();
    }

    AZ::Aabb EditorStaticMeshComponent::GetLocalBounds()
    {
        return m_mesh.CalculateLocalAABB();
    }

    void EditorStaticMeshComponent::SetMeshAsset(const AZ::Data::AssetId& id)
    {
        m_mesh.SetMeshAsset(id);
        EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddDirtyEntity, GetEntityId());
    }

    void EditorStaticMeshComponent::SetMaterial(_smart_ptr<IMaterial> material)
    {
        m_mesh.SetMaterial(material);

        EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
    }

    _smart_ptr<IMaterial> EditorStaticMeshComponent::GetMaterial()
    {
        return m_mesh.GetMaterial();
    }

    void EditorStaticMeshComponent::SetPrimaryAsset(const AZ::Data::AssetId& id)
    {
        SetMeshAsset(id);
    }

    void EditorStaticMeshComponent::OnEntityVisibilityChanged(bool visibility)
    {
            m_mesh.UpdateAuxiliaryRenderFlags(!visibility, ERF_HIDDEN);
            m_mesh.RefreshRenderState();
    }

    void EditorStaticMeshComponent::DisplayEntity(bool& handled)
    {
        if (m_mesh.HasMesh())
        {
            // Only allow Sandbox to draw the default sphere if we don't have a
            // visible mesh.
            handled = true;
        }
    }

    void EditorStaticMeshComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        if (StaticMeshComponent* meshComponent = gameEntity->CreateComponent<StaticMeshComponent>())
        {
            m_mesh.CopyPropertiesTo(meshComponent->m_staticMeshRenderNode);
        }
    }

    void EditorStaticMeshComponent::CreateEditorPhysics()
    {
        DestroyEditorPhysics();

        if (!GetTransform())
        {
            return;
        }

        IStatObj* geometry = m_mesh.GetEntityStatObj();
        if (!geometry)
        {
            return;
        }

        if (gEnv->pPhysicalWorld)
        {
            m_physicalEntity = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, nullptr, &m_mesh, PHYS_FOREIGN_ID_STATIC);
            m_physicalEntity->AddRef();

            pe_geomparams params;
            geometry->Physicalize(m_physicalEntity, &params);

            // Immediately set transform, otherwise physics doesn't propgate the world change.
            const AZ::Transform& transform = GetTransform()->GetWorldTM();
            Matrix34 cryTransform = AZTransformToLYTransform(transform);
            pe_params_pos par_pos;
            par_pos.pMtx3x4 = &cryTransform;
            m_physicalEntity->SetParams(&par_pos);

            // Store scale at point of physicalization so we know when to re-physicalize.
            // CryPhysics doesn't support dynamic scale changes.
            m_physScale = transform.RetrieveScale();

            AffectNavmesh();
        }
    }

    void EditorStaticMeshComponent::DestroyEditorPhysics()
    {
        // If physics is completely torn down, all physical entities are by extension completely invalid (dangling pointers).
        // It doesn't matter that we held a reference.
        if (gEnv->pPhysicalWorld)
        {
            if (m_physicalEntity)
            {
                m_physicalEntity->Release();
                gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_physicalEntity);
            }
        }

        m_physicalEntity = nullptr;
    }

    IStatObj* EditorStaticMeshComponent::GetStatObj()
    {
        return m_mesh.GetEntityStatObj();
    }

    bool EditorStaticMeshComponent::GetVisibility()
    {
        return m_mesh.GetVisible();
    }

    void EditorStaticMeshComponent::SetVisibility(bool isVisible)
    {
        m_mesh.SetVisible(isVisible);
    }

    void EditorStaticMeshComponent::AffectNavmesh()
    {
        if ( m_physicalEntity )
        {
            pe_params_foreign_data foreignData;
            m_physicalEntity->GetParams(&foreignData);

            if (m_mesh.m_renderOptions.m_affectNavmesh)
            {
                foreignData.iForeignFlags &= ~PFF_EXCLUDE_FROM_STATIC;
            }
            else
            {
                foreignData.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
            }
            m_physicalEntity->SetParams(&foreignData);

            // Refresh the nav tile when the flag changes.
            INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
            if (pNavigationSystem)
            {
                pNavigationSystem->WorldChanged(AZAabbToLyAABB(GetWorldBounds()));
            }
        }
    }
} // namespace LmbrCentral
