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

#include "UiGameEntityContext.h"
#include <AzCore/Serialization/Utils.h>
#include <LyShine/Bus/UiCanvasBus.h>
#include <LyShine/Bus/UiElementBus.h>
#include <LyShine/Bus/UiTransformBus.h>
#include <LyShine/Bus/UiTransform2dBus.h>
#include <LyShine/UiComponentTypes.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
UiGameEntityContext::UiGameEntityContext(AZ::EntityId canvasEntityId)
    : m_canvasEntityId(canvasEntityId)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiGameEntityContext::~UiGameEntityContext()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiGameEntityContext::HandleLoadedRootSliceEntity(AZ::Entity* rootEntity, bool remapIds, AZ::SliceComponent::EntityIdToEntityIdMap* idRemapTable)
{
    AZ_Assert(m_rootAsset, "The context has not been initialized.");

    if (!AzFramework::EntityContext::HandleLoadedRootSliceEntity(rootEntity, remapIds, idRemapTable))
    {
        return false;
    }

    AZ::SliceComponent::EntityList entities;
    GetRootSlice()->GetEntities(entities);

    GetRootSlice()->SetIsDynamic(true);

    InitializeEntities(entities);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiGameEntityContext::CreateUiEntity(const char* name)
{
    AZ::Entity* entity = CreateEntity(name);

    if (entity)
    {
        // we don't currently do anything extra here, UI entities are not automatically
        // Init'ed and Activate'd when they are created. We wait until the required components
        // are added before Init and Activate
    }

    return entity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::AddUiEntity(AZ::Entity* entity)
{
    AZ_Assert(entity, "Supplied entity is invalid.");

    AddEntity(entity);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::AddUiEntities(const AzFramework::EntityContext::EntityList& entities)
{
    AZ::SliceAsset* rootSlice = m_rootAsset.Get();

    for (AZ::Entity* entity : entities)
    {
        AZ_Assert(!AzFramework::EntityIdContextQueryBus::MultiHandler::BusIsConnectedId(entity->GetId()), "Entity already in context.");
        rootSlice->GetComponent()->AddEntity(entity);
    }

    HandleEntitiesAdded(entities);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiGameEntityContext::CloneUiEntities(const AZStd::vector<AZ::EntityId>& sourceEntities, AzFramework::EntityContext::EntityList& resultEntities)
{
    resultEntities.clear();

    AZ::SliceComponent::InstantiatedContainer sourceObjects;
    for (const AZ::EntityId& id : sourceEntities)
    {
        AZ::Entity* entity = nullptr;
        EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, id);
        if (entity)
        {
            sourceObjects.m_entities.push_back(entity);
        }
    }

    AZ::SliceComponent::EntityIdToEntityIdMap idMap;
    AZ::SliceComponent::InstantiatedContainer* clonedObjects =
        AZ::EntityUtils::CloneObjectAndFixEntities(&sourceObjects, idMap);
    if (!clonedObjects)
    {
        AZ_Error("UiEntityContext", false, "Failed to clone source entities.");
        return false;
    }

    resultEntities = clonedObjects->m_entities;

    AddUiEntities(resultEntities);

    sourceObjects.m_entities.clear();
    clonedObjects->m_entities.clear();
    delete clonedObjects;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiGameEntityContext::DestroyUiEntity(AZ::EntityId entityId)
{
    return EntityContext::DestroyEntity(entityId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiGameEntityContext::DestroyEntity(AZ::Entity* entity)
{
    AZ_Assert(entity, "Invalid entity passed to DestroyEntity");
    AZ_Assert(m_rootAsset, "The context has not been initialized.");

    AZ::SliceAsset* rootSlice = m_rootAsset.Get();

    AzFramework::EntityContextId owningContextId = AzFramework::EntityContextId::CreateNull();
    EBUS_EVENT_ID_RESULT(owningContextId, entity->GetId(), AzFramework::EntityIdContextQueryBus, GetOwningContextId);
    AZ_Assert(owningContextId == m_contextId, "Entity does not belong to this context, and therefore can not be safely destroyed by this context.");

    if (owningContextId == m_contextId)
    {
        HandleEntityRemoved(entity->GetId());
        rootSlice->GetComponent()->RemoveEntity(entity);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::InitUiContext()
{
    InitContext();

    GetRootSlice()->Instantiate();

    UiEntityContextRequestBus::Handler::BusConnect(GetContextId());
    UiGameEntityContextBus::Handler::BusConnect(GetContextId());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::DestroyUiContext()
{
    UiEntityContextRequestBus::Handler::BusDisconnect();
    UiGameEntityContextBus::Handler::BusDisconnect();

    DestroyContext();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiGameEntityContext::SaveToStreamForGame(AZ::IO::GenericStream& stream, AZ::DataStream::StreamType streamType)
{
    if (!m_rootAsset)
    {
        return false;
    }

    AZ::Entity* rootSliceEntity = m_rootAsset.Get()->GetEntity();
    return AZ::Utils::SaveObjectToStream<AZ::Entity>(stream, AZ::ObjectStream::ST_XML, rootSliceEntity);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::OnContextEntitiesAdded(const AzFramework::EntityContext::EntityList& entities)
{
    EntityContext::OnContextEntitiesAdded(entities);

    InitializeEntities(entities);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::InitializeEntities(const AzFramework::EntityContext::EntityList& entities)
{
    // UI entities are now automatically activated on creation

    for (AZ::Entity* entity : entities)
    {
        if (entity->GetState() == AZ::Entity::ES_CONSTRUCTED)
        {
            entity->Init();
        }
    }

    for (AZ::Entity* entity : entities)
    {
        if (entity->GetState() == AZ::Entity::ES_INIT)
        {
            entity->Activate();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
bool UiGameEntityContext::ValidateEntitiesAreValidForContext(const EntityList& entities)
{
    // All entities in a slice being instantiated in the UI editor should
    // have the UiElementComponent on them.
    for (AZ::Entity* entity : entities)
    {
        if (!entity->FindComponent(LyShine::UiElementComponentUuid))
        {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AzFramework::SliceInstantiationTicket UiGameEntityContext::InstantiateDynamicSlice(
    const AZ::Data::Asset<AZ::Data::AssetData>& sliceAsset, const AZ::Vector2& position, bool isViewportPosition,
    AZ::Entity* parent, const AZ::EntityUtils::EntityIdMapper& customIdMapper)
{
    if (sliceAsset.GetId().IsValid())
    {
        m_instantiatingDynamicSlices.push_back(InstantiatingDynamicSlice(sliceAsset, position, isViewportPosition, parent));

        const AzFramework::SliceInstantiationTicket ticket = InstantiateSlice(sliceAsset, customIdMapper);
        if (ticket)
        {
            AzFramework::SliceInstantiationResultBus::MultiHandler::BusConnect(ticket);
        }

        return ticket;
    }

    return AzFramework::SliceInstantiationTicket();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::OnSlicePreInstantiate(const AZ::Data::AssetId& sliceAssetId, const AZ::SliceComponent::SliceInstanceAddress& sliceAddress)
{
    for (auto instantiatingIter = m_instantiatingDynamicSlices.begin(); instantiatingIter != m_instantiatingDynamicSlices.end(); ++instantiatingIter)
    {
        if (instantiatingIter->m_asset.GetId() == sliceAssetId)
        {
            const AZ::SliceComponent::EntityList& entities = sliceAddress.second->GetInstantiated()->m_entities;

            // If the context was loaded from a stream and Ids were remapped, fix up entity Ids in that slice that
            // point to entities in the stream (i.e. level entities).
            if (!m_loadedEntityIdMap.empty())
            {
                AZ::SliceComponent::InstantiatedContainer instanceEntities;
                instanceEntities.m_entities = entities;
                AZ::EntityUtils::ReplaceEntityRefs(&instanceEntities,
                    [this](const AZ::EntityId& originalId, bool isEntityId) -> AZ::EntityId
                    {
                        if (!isEntityId)
                        {
                            auto iter = m_loadedEntityIdMap.find(originalId);
                            if (iter != m_loadedEntityIdMap.end())
                            {
                                return iter->second;
                            }
                        }
                        return originalId;

                    }, m_serializeContext);

                instanceEntities.m_entities.clear();
            }

            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::OnSliceInstantiated(const AZ::Data::AssetId& sliceAssetId, const AZ::SliceComponent::SliceInstanceAddress& instance)
{
    const AzFramework::SliceInstantiationTicket& ticket = *AzFramework::SliceInstantiationResultBus::GetCurrentBusId();

    AzFramework::SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);

    for (auto instantiatingIter = m_instantiatingDynamicSlices.begin(); instantiatingIter != m_instantiatingDynamicSlices.end(); ++instantiatingIter)
    {
        if (instantiatingIter->m_asset.GetId() == sliceAssetId)
        {
            const AZ::SliceComponent::EntityList& entities = instance.second->GetInstantiated()->m_entities;

            // Initialize the new entities and create a set of all the top-level entities.
            AZStd::unordered_set<AZ::Entity*> topLevelEntities;
            for (AZ::Entity* entity : entities)
            {
                if (entity->GetState() == AZ::Entity::ES_CONSTRUCTED)
                {
                    entity->Init();
                }
                if (entity->GetState() == AZ::Entity::ES_INIT)
                {
                    entity->Activate();
                }

                topLevelEntities.insert(entity);
            }

            // remove anything from the topLevelEntities set that is referenced as the child of another element in the list
            for (AZ::Entity* entity : entities)
            {
                LyShine::EntityArray children;
                EBUS_EVENT_ID_RESULT(children, entity->GetId(), UiElementBus, GetChildElements);
                
                for (auto child : children)
                {
                    topLevelEntities.erase(child);
                }
            }

            // This can be null is nothing is selected. That is OK, the usage of it below treats that as meaning
            // add as a child of the root element.
            AZ::Entity* parent = instantiatingIter->m_parent;

            // Now topLevelElements contains all of the top-level elements in the set of newly instantiated entities
            // Copy the topLevelEntities set into a list
            LyShine::EntityArray entitiesToInit;
            for (auto entity : topLevelEntities)
            {
                entitiesToInit.push_back(entity);
            }

            // There must be at least one element
            AZ_Assert(entitiesToInit.size() >= 1, "There must be at least one top-level entity in a UI slice.");

            // Initialize the internal parent pointers and the canvas pointer in the elements
            // We do this before adding the elements, otherwise the GetUniqueChildName code in FixupCreatedEntities will
            // already see the new elements and think the names are not unique
            EBUS_EVENT_ID(m_canvasEntityId, UiCanvasBus, FixupCreatedEntities, entitiesToInit, true, parent);

            // Add all of the top-level entities as children of the parent
            for (auto entity : topLevelEntities)
            {
                EBUS_EVENT_ID(m_canvasEntityId, UiCanvasBus, AddElement, entity, parent, nullptr);
            }

            // Here we adjust the position of the instantiated entities so that if the slice was instantiated from the
            // viewport menu we instantiate it at the mouse position
            if (instantiatingIter->m_isViewportPosition)
            {
                // This is the same behavior as the old "Add elements from prefab" had.
                const AZ::Vector2& desiredViewportPosition = instantiatingIter->m_position;

                AZ::Entity* rootElement = entitiesToInit[0];

                // Transform pivot position to canvas space
                AZ::Vector2 pivotPos;
                EBUS_EVENT_ID_RESULT(pivotPos, rootElement->GetId(), UiTransformBus, GetCanvasSpacePivotNoScaleRotate);

                // Transform destination position to canvas space
                AZ::Matrix4x4 transformFromViewport;
                EBUS_EVENT_ID(rootElement->GetId(), UiTransformBus, GetTransformFromViewport, transformFromViewport);
                AZ::Vector3 destPos3 = transformFromViewport * AZ::Vector3(desiredViewportPosition.GetX(), desiredViewportPosition.GetY(), 0.0f);
                AZ::Vector2 destPos(destPos3.GetX(), destPos3.GetY());

                AZ::Vector2 offsetDelta = destPos - pivotPos;

                // Adjust offsets on all top level elements
                for (auto entity : entitiesToInit)
                {
                    UiTransform2dInterface::Offsets offsets;
                    EBUS_EVENT_ID_RESULT(offsets, entity->GetId(), UiTransform2dBus, GetOffsets);
                    EBUS_EVENT_ID(entity->GetId(), UiTransform2dBus, SetOffsets, offsets + offsetDelta);
                }
            }
            else if (!instantiatingIter->m_position.IsZero())
            {
                AZ::Entity* rootElement = entitiesToInit[0];
                EBUS_EVENT_ID(rootElement->GetId(), UiTransformBus, MoveLocalPositionBy, instantiatingIter->m_position);
            }

            EBUS_EVENT(UiGameEntityContextNotificationBus, OnSliceInstantiated, sliceAssetId, instance, ticket);
            m_instantiatingDynamicSlices.erase(instantiatingIter);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiGameEntityContext::OnSliceInstantiationFailed(const AZ::Data::AssetId& sliceAssetId)
{
    const AzFramework::SliceInstantiationTicket& ticket = *AzFramework::SliceInstantiationResultBus::GetCurrentBusId();

    AzFramework::SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);

    for (auto instantiatingIter = m_instantiatingDynamicSlices.begin(); instantiatingIter != m_instantiatingDynamicSlices.end(); ++instantiatingIter)
    {
        if (instantiatingIter->m_asset.GetId() == sliceAssetId)
        {
            EBUS_EVENT(UiGameEntityContextNotificationBus, OnSliceInstantiationFailed, sliceAssetId, ticket);

            m_instantiatingDynamicSlices.erase(instantiatingIter);
            break;
        }
    }
}