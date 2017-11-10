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

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/FileIO.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzFramework/Entity/EntityContextBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Slice/SliceTransaction.h>
#include <AzToolsFramework/UI/UICore/ProgressShield.hxx>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/SourceControl/SourceControlAPI.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

namespace AzToolsFramework
{
    namespace SliceUtilities
    {
        //=========================================================================
        namespace Internal
        {
            AZStd::string MakeTemporaryFilePathForSave(const char* fullPath);
            SliceTransaction::Result SaveSliceToDisk(const SliceTransaction::SliceAssetPtr& asset, const char* targetPath, AZ::SerializeContext* serializeContext);

        } // namespace Internal

        //=========================================================================
        SliceTransaction::TransactionPtr SliceTransaction::BeginNewSlice(const char* name,
            AZ::SerializeContext* serializeContext,
            AZ::u32 /*sliceCreationFlags*/)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            if (!serializeContext)
            {
                AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
                AZ_Assert(serializeContext, "Failed to retrieve serialize context.");
            }

            TransactionPtr newTransaction = aznew SliceTransaction(serializeContext);

            AZ::Entity* entity = aznew AZ::Entity(name ? name : "Slice");

            // Create new empty slice asset.
            newTransaction->m_targetAsset = AZ::Data::AssetManager::Instance().CreateAsset<AZ::SliceAsset>(AZ::Data::AssetId(AZ::Uuid::CreateRandom()));
            AZ::SliceComponent* component = entity->CreateComponent<AZ::SliceComponent>();
            newTransaction->m_targetAsset.Get()->SetData(entity, component);

            newTransaction->m_transactionType = TransactionType::NewSlice;

            return newTransaction;
        }

        //=========================================================================
        SliceTransaction::TransactionPtr SliceTransaction::BeginSlicePush(const SliceAssetPtr& asset,
            AZ::SerializeContext* serializeContext,
            AZ::u32 /*slicePushFlags*/)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            if (!serializeContext)
            {
                AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
                AZ_Assert(serializeContext, "Failed to retrieve serialize context.");
            }

            if (!asset)
            {
                AZ_Error("SliceTransaction", false, "Target asset is not loaded. Ensure the asset is loaded before attempting a push transaction.");
                return TransactionPtr();
            }

            TransactionPtr newTransaction = aznew SliceTransaction(serializeContext);

            // Clone the asset in-memory for manipulation.
            AZ::Entity* entity = aznew AZ::Entity();
            entity->AddComponent(asset.Get()->GetComponent()->Clone(*serializeContext));
            newTransaction->m_targetAsset = aznew AZ::SliceAsset(asset.GetId());
            newTransaction->m_targetAsset.Get()->SetData(entity, entity->FindComponent<AZ::SliceComponent>());

            newTransaction->m_transactionType = TransactionType::UpdateSlice;

            return newTransaction;
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::UpdateEntity(AZ::Entity* entity)
        {
            if (!entity)
            {
                return AZ::Failure(AZStd::string::format("Null source entity for push."));
            }

            if (m_transactionType != TransactionType::UpdateSlice)
            {
                return AZ::Failure(AZStd::string::format("UpdateEntity() is only valid during push transactions, not creation transactions."));
            }

            // Given the asset we're targeting, identify corresponding ancestor for the live entity.
            const AZ::EntityId targetId = FindTargetAncestorAndUpdateInstanceIdMap(entity->GetId(), m_liveToAssetIdMap);
            if (targetId.IsValid())
            {
                m_entitiesToPush.emplace_back(targetId, entity->GetId());
            }
            else
            {
                return AZ::Failure(AZStd::string::format("Unable to locate entity %s [%llu] in target slice.",
                    entity->GetName().c_str(), entity->GetId()));
            }

            return AZ::Success();
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::UpdateEntity(const AZ::EntityId& entityId)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);
            return UpdateEntity(entity);
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::UpdateEntityField(AZ::Entity* entity,
            const InstanceDataNode::Address& fieldNodeAddress)
        {
            if (!entity)
            {
                return AZ::Failure(AZStd::string::format("Null source entity for push."));
            }

            if (m_transactionType != TransactionType::UpdateSlice)
            {
                return AZ::Failure(AZStd::string::format("UpdateEntityField() is only valid during push transactions, not creation transactions."));
            }

            // Given the asset we're targeting, identify corresponding ancestor for the live entity.
            const AZ::EntityId targetId = FindTargetAncestorAndUpdateInstanceIdMap(entity->GetId(), m_liveToAssetIdMap);
            if (targetId.IsValid())
            {
                m_entitiesToPush.emplace_back(targetId, entity->GetId(), fieldNodeAddress);
            }
            else
            {
                return AZ::Failure(AZStd::string::format("Unable to locate entity %s [%llu] in target slice.",
                    entity->GetName().c_str(), entity->GetId()));
            }

            return AZ::Success();
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::UpdateEntityField(const AZ::EntityId& entityId,
            const InstanceDataNode::Address& fieldNodeAddress)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);
            return UpdateEntityField(entity, fieldNodeAddress);
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::AddEntity(const AZ::Entity* entity, AZ::u32 addEntityFlags /* = 0 */)
        {
            if (!entity)
            {
                return AZ::Failure(AZStd::string::format("Invalid entity passed to AddEntity()."));
            }

            if (m_transactionType == TransactionType::None)
            {
                return AZ::Failure(AZStd::string::format("AddEntity() is only valid during during a transaction. This transaction may've already been committed."));
            }

            AZ::SliceComponent::SliceInstanceAddress sliceAddress(nullptr, nullptr);
            AzFramework::EntityIdContextQueryBus::EventResult(sliceAddress, entity->GetId(), &AzFramework::EntityIdContextQueryBus::Events::GetOwningSlice);

            // When adding entities to existing slices, we need to resolve to the asset's entity Ids.
            if (m_transactionType == TransactionType::UpdateSlice)
            {
                // Walk up parent transform chain until we find an entity with a slice ancestor in the target slice.
                // If we don't find one, fail. We need an associated instance so we can fix up Id references.
                AZ::EntityId parentId;
                AZ::TransformBus::EventResult(parentId, entity->GetId(), &AZ::TransformBus::Events::GetParentId);
                AZ::EntityId ancestorId;
                AZ::SliceComponent::EntityIdToEntityIdMap liveToAssetIdMap;
                while (parentId.IsValid())
                {
                    liveToAssetIdMap.clear();
                    ancestorId = FindTargetAncestorAndUpdateInstanceIdMap(parentId, liveToAssetIdMap, &sliceAddress);
                    if (ancestorId.IsValid())
                    {
                        break;
                    }
                    
                    AZ::EntityId currentParentId = parentId;
                    parentId.SetInvalid();
                    AZ::TransformBus::EventResult(parentId, currentParentId, &AZ::TransformBus::Events::GetParentId);
                }

                if (!ancestorId.IsValid())
                {
                    return AZ::Failure(AZStd::string::format("Attempting to add an entity to an existing slice, but the entity could not be found in a hierarchy belonging to the target slice."));
                }

                for (const auto& idPair : liveToAssetIdMap)
                {
                    m_liveToAssetIdMap[idPair.first] = idPair.second;
                }
            }

            if (sliceAddress.first && !(addEntityFlags & SliceAddEntityFlags::DiscardSliceAncestry))
            {
                // Add entity with its slice ancestry
                auto addedSliceInstanceIt = m_addedSliceInstances.find(sliceAddress);
                if (addedSliceInstanceIt == m_addedSliceInstances.end())
                {
                    // This slice instance hasn't been added to the transaction yet, add it
                    SliceTransaction::SliceInstanceToPush& instanceToPush = m_addedSliceInstances[sliceAddress];
                    instanceToPush.m_includeEntireInstance = false;
                    instanceToPush.m_instanceAddress = sliceAddress;
                    instanceToPush.m_entitiesToInclude.insert(entity->GetId());

                    for (const auto& mapPair : sliceAddress.second->GetEntityIdMap())
                    {
                        // We keep the entity ids in the source instances, so our live Id will match the one we write to the asset.
                        m_liveToAssetIdMap[mapPair.second] = mapPair.second;
                    }
                }
                else
                {
                    SliceTransaction::SliceInstanceToPush& instanceToPush = addedSliceInstanceIt->second;
                    if (!instanceToPush.m_includeEntireInstance)
                    {
                        instanceToPush.m_entitiesToInclude.insert(entity->GetId());
                    }
                    else
                    {
                        // Adding a specific entity from a slice instance that is already
                        // being completely included, don't need to do anything (it'll already be covered)
                        return AZ::Success();
                    }
                }
            }
            else
            {
                // Add as loose entity; clone the entity and assign a new Id.
                AZ::Entity* clonedEntity = m_serializeContext->CloneObject(entity);
                clonedEntity->SetId(AZ::Entity::MakeId());
                m_liveToAssetIdMap[entity->GetId()] = clonedEntity->GetId();

                m_targetAsset.Get()->GetComponent()->AddEntity(clonedEntity);
            }

            return AZ::Success();
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::AddEntity(AZ::EntityId entityId, AZ::u32 addEntityFlags /* = 0 */)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);
            return AddEntity(entity, addEntityFlags);
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::AddSliceInstance(const AZ::SliceComponent::SliceInstanceAddress& sliceAddress)
        {
            if (!sliceAddress.first)
            {
                return AZ::Failure(AZStd::string::format("Invalid slice instance address passed to AddSliceInstance()."));
            }

            if (m_transactionType == TransactionType::None)
            {
                return AZ::Failure(AZStd::string::format("AddSliceInstance() is only valid during during a transaction. This transaction may've already been committed."));
            }

            auto addedSliceInstanceIt = m_addedSliceInstances.find(sliceAddress);
            if (addedSliceInstanceIt == m_addedSliceInstances.end())
            {
                // This slice instance hasn't been added to the transaction yet, add it
                SliceTransaction::SliceInstanceToPush& instanceToPush = m_addedSliceInstances[sliceAddress];
                instanceToPush.m_includeEntireInstance = true;
                instanceToPush.m_instanceAddress = sliceAddress;
            }
            else
            {
                SliceTransaction::SliceInstanceToPush& instanceToPush = addedSliceInstanceIt->second;
                if (instanceToPush.m_includeEntireInstance)
                {
                    return AZ::Failure(AZStd::string::format("Slice instance has already been added to the transaction."));
                }
                else
                {
                    // Transaction already has had individual entities from this slice instance added to it, so we just convert
                    // that entry to include all entities
                    instanceToPush.m_includeEntireInstance = true;
                }
            }

            for (const auto& mapPair : sliceAddress.second->GetEntityIdMap())
            {
                // We keep the entity ids in the source instances, so our live Id will match the one we write to the asset.
                m_liveToAssetIdMap[mapPair.second] = mapPair.second;
            }

            return AZ::Success();
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::RemoveEntity(AZ::Entity* entity)
        {
            if (!entity)
            {
                return AZ::Failure(AZStd::string::format("Invalid entity passed to RemoveEntity()."));
            }

            return RemoveEntity(entity->GetId());
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::RemoveEntity(AZ::EntityId entityId)
        {
            if (!entityId.IsValid())
            {
                return AZ::Failure(AZStd::string::format("Invalid entity Id passed to RemoveEntity()."));
            }

            if (m_transactionType != TransactionType::UpdateSlice)
            {
                return AZ::Failure(AZStd::string::format("RemoveEntity() is only valid during during a push transaction."));
            }

            // The user needs to provide the entity as it exists in the target asset, since we can't resolve deleted entities.
            // so the caller isn't required to in that case.
            m_entitiesToRemove.push_back(entityId);

            return AZ::Success();
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::Commit(const char* fullPath,
            SliceTransaction::PreSaveCallback preSaveCallback,
            SliceTransaction::PostSaveCallback postSaveCallback,
            AZ::u32 sliceCommitFlags)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            // Clone asset for final modifications and save.
            // This also releases borrowed entities and slice instances.
            SliceAssetPtr finalAsset = CloneAssetForSave();

            // Check out target asset.
            {
                using ApplicationBus = AzToolsFramework::ToolsApplicationRequestBus;

                bool checkedOutSuccessfully = false;
                ApplicationBus::BroadcastResult(checkedOutSuccessfully, &ApplicationBus::Events::RequestEditForFileBlocking,
                    fullPath, "Checking out for edit...", ApplicationBus::Events::RequestEditProgressCallback());

                if (!checkedOutSuccessfully)
                {
                    return AZ::Failure(AZStd::string::format("Unable to checkout target file \"%s\".", fullPath));
                }
            }

            // Process the transaction.
            switch (m_transactionType)
            {
            case TransactionType::NewSlice:
            {
                // No additional work required; slice asset is populated.
            }
            break;

            case TransactionType::UpdateSlice:
            {
                AZ::SliceComponent* sliceAsset = finalAsset.Get()->GetComponent();

                // Remove any requested entities from the slice.
                for (const AZ::EntityId& removeId : m_entitiesToRemove)
                {
                    // Find the entity's ancestor in the target asset.
                    if (!sliceAsset->RemoveEntity(removeId))
                    {
                        return AZ::Failure(AZStd::string::format("Unable to remove entity [%llu] from target slice.", removeId));
                    }
                }

                // Loop through each field to push, generate an InstanceDataHierarchy for the source entity, and synchronize the field data to the target.
                // We can combine with the above loop, but organizing in two passes makes the processes clearer.
                for (const EntityToPush& entityToPush : m_entitiesToPush)
                {
                    AZ::Entity* sourceEntity = nullptr;
                    if (entityToPush.m_sourceEntityId != entityToPush.m_targetEntityId)
                    {
                        AZ::ComponentApplicationBus::BroadcastResult(sourceEntity, &AZ::ComponentApplicationBus::Events::FindEntity, entityToPush.m_sourceEntityId);
                    }
                    else
                    {
                        sourceEntity = sliceAsset->FindEntity(entityToPush.m_sourceEntityId);
                    }

                    if (!sourceEntity)
                    {
                        return AZ::Failure(AZStd::string::format("Unable to locate source entity with id %s for slice data push. It was not found in the slice, or an instance of the slice.",
                            entityToPush.m_sourceEntityId.ToString().c_str()));
                    }

                    AZ::Entity* targetEntity = sliceAsset->FindEntity(entityToPush.m_targetEntityId);
                    if (!targetEntity)
                    {
                        return AZ::Failure(AZStd::string::format("Unable to locate entity with Id %llu in the target slice.",
                            entityToPush.m_targetEntityId));
                    }

                    InstanceDataHierarchy targetHierarchy;
                    targetHierarchy.AddRootInstance<AZ::Entity>(targetEntity);
                    targetHierarchy.Build(m_serializeContext, AZ::SerializeContext::ENUM_ACCESS_FOR_READ);

                    InstanceDataHierarchy sourceHierarchy;
                    sourceHierarchy.AddRootInstance<AZ::Entity>(sourceEntity);
                    sourceHierarchy.Build(m_serializeContext, AZ::SerializeContext::ENUM_ACCESS_FOR_READ);

                    const InstanceDataNode* sourceNode = &sourceHierarchy;
                    InstanceDataNode* targetNode = &targetHierarchy;

                    // If we're pushing a specific field, resolve the corresponding nodes in both hierarchies.
                    InstanceDataNode::Address elementAddress;
                    if (!entityToPush.m_fieldNodeAddress.empty())
                    {
                        sourceNode = sourceHierarchy.FindNodeByAddress(entityToPush.m_fieldNodeAddress);
                        targetNode = targetHierarchy.FindNodeByAddress(entityToPush.m_fieldNodeAddress);

                        // If the node is a container element, we push at the container level but filter by the element.
                        if (sourceNode && !targetNode)
                        {
                            // Element exists in the source, but not the target. We want to add it to the target.
                            elementAddress = entityToPush.m_fieldNodeAddress;
							
                            // Recurse up trying to find the first matching source/target node
                            // This is necessary anytime we're trying to push a node that requires more than just a leaf node be added
                            while (sourceNode && !targetNode)
                            {
                                sourceNode = sourceNode->GetParent();
                                if (sourceNode)
                                {
                                    targetNode = targetHierarchy.FindNodeByAddress(sourceNode->ComputeAddress());
                                }
                            }
                        }
                        else if (targetNode && !sourceNode)
                        {
                            // Element exists in the target, but not the source. We want to remove it from the target.
                            elementAddress = entityToPush.m_fieldNodeAddress;
                            targetNode = targetNode->GetParent();
                            sourceNode = sourceHierarchy.FindNodeByAddress(targetNode->ComputeAddress());
                        }
                    }

                    if (!sourceNode)
                    {
                        return AZ::Failure(AZStd::string::format("Unable to locate source data node for slice push."));
                    }
                    if (!targetNode)
                    {
                        return AZ::Failure(AZStd::string::format("Unable to locate target data node for slice push."));
                    }

                    bool copyResult = InstanceDataHierarchy::CopyInstanceData(sourceNode, targetNode, m_serializeContext, nullptr, nullptr, elementAddress);
                    if (!copyResult)
                    {
                        return AZ::Failure(AZStd::string::format("Unable to push data node to target for slice push."));
                    }
                }
            }
            break;

            default:
            {
                return AZ::Failure(AZStd::string::format("Transaction cannot be committed because it was never started."));
            }
            break;
            }

            Result result = PreSave(fullPath, finalAsset, preSaveCallback, sliceCommitFlags);
            if (!result)
            {
                return AZ::Failure(AZStd::string::format("Pre-save callback reported failure: %s.", result.TakeError().c_str()));
            }

            result = Internal::SaveSliceToDisk(finalAsset, fullPath, m_serializeContext);
            if (!result)
            {
                return AZ::Failure(AZStd::string::format("Slice asset could not be saved to disk.\n\nAsset path: %s \n\nDetails: %s", fullPath, result.TakeError().c_str()));
            }

            if (postSaveCallback)
            {
                postSaveCallback(TransactionPtr(this), fullPath, finalAsset);
            }

            // Reset the transaction.
            Reset();

            return AZ::Success();
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::Commit(const AZ::Data::AssetId& targetAssetId,
            SliceTransaction::PreSaveCallback preSaveCallback,
            SliceTransaction::PostSaveCallback postSaveCallback,
            AZ::u32 sliceCommitFlags)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            AZStd::string sliceAssetPath;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(sliceAssetPath, &AZ::Data::AssetCatalogRequests::GetAssetPathById, targetAssetId);
            if (sliceAssetPath.empty())
            {
                return AZ::Failure(AZStd::string::format("Failed to resolve path for slice asset %s. Aborting slice push. No assets have been affected.",
                    targetAssetId.ToString<AZStd::string>().c_str()));
            }

            bool fullPathFound = false;
            AZStd::string assetFullPath;
            AssetSystemRequestBus::BroadcastResult(fullPathFound, &AssetSystemRequestBus::Events::GetFullSourcePathFromRelativeProductPath, sliceAssetPath, assetFullPath);

            if (!fullPathFound)
            {
                assetFullPath = AZStd::string::format("@devassets@/%s", sliceAssetPath.c_str());
            }

            return Commit(assetFullPath.c_str(), preSaveCallback, postSaveCallback, sliceCommitFlags);
        }

        //=========================================================================
        const AZ::SliceComponent::EntityIdToEntityIdMap& SliceTransaction::GetLiveToAssetEntityIdMap() const
        {
            return m_liveToAssetIdMap;
        }

        //=========================================================================
        SliceTransaction::SliceTransaction(AZ::SerializeContext* serializeContext)
            : m_transactionType(SliceTransaction::TransactionType::None)
            , m_refCount(0)
        {
            if (!serializeContext)
            {
                AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
                AZ_Assert(serializeContext, "No serialize context was provided, and none could be found.");
            }

            m_serializeContext = serializeContext;
        }

        //=========================================================================
        SliceTransaction::~SliceTransaction()
        {
        }

        //=========================================================================
        SliceTransaction::SliceAssetPtr SliceTransaction::CloneAssetForSave()
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            // Move included slice instances to the target asset temporarily so that they are included in the clone
            for (auto& addedSliceInstanceIt : m_addedSliceInstances)
            {
                SliceTransaction::SliceInstanceToPush& instanceToPush = addedSliceInstanceIt.second;
                instanceToPush.m_instanceAddress = m_targetAsset.Get()->GetComponent()->AddSliceInstance(instanceToPush.m_instanceAddress.first, instanceToPush.m_instanceAddress.second);
            }
            
            // Clone the asset.
            AZ::Entity* finalSliceEntity = aznew AZ::Entity();
            AZ::SliceComponent::SliceInstanceToSliceInstanceMap sourceToCloneSliceInstanceMap;
            finalSliceEntity->AddComponent(m_targetAsset.Get()->GetComponent()->Clone(*m_serializeContext, &sourceToCloneSliceInstanceMap));
            AZ::Data::Asset<AZ::SliceAsset> finalAsset = AZ::Data::AssetManager::Instance().CreateAsset<AZ::SliceAsset>(AZ::Data::AssetId(AZ::Uuid::CreateRandom()));
            finalAsset.Get()->SetData(finalSliceEntity, finalSliceEntity->FindComponent<AZ::SliceComponent>());

            // For slice instances added that should only contain specified entities, cull the undesired entities from final asset
            AZStd::vector<AZ::Entity*> entitiesToDelete;
            for (const auto& addedSliceInstanceIt : m_addedSliceInstances)
            {
                const SliceTransaction::SliceInstanceToPush& instanceToPush = addedSliceInstanceIt.second;
                if (!instanceToPush.m_includeEntireInstance)
                {
                    AZ::SliceComponent::SliceInstanceAddress& finalAssetSliceInstance = sourceToCloneSliceInstanceMap[instanceToPush.m_instanceAddress];
                    const AZ::SliceComponent::InstantiatedContainer* finalAssetInstantiatedContainer = finalAssetSliceInstance.second->GetInstantiated();
                    for (AZ::Entity* finalAssetEntity : finalAssetInstantiatedContainer->m_entities)
                    {
                        AZ::EntityId finalAssetEntityId = finalAssetEntity->GetId();
                        auto foundIt = instanceToPush.m_entitiesToInclude.find(finalAssetEntityId);
                        if (foundIt == instanceToPush.m_entitiesToInclude.end())
                        {
                            entitiesToDelete.push_back(finalAssetEntity);
                        }
                    }

                    for (AZ::Entity* entityToDelete : entitiesToDelete)
                    {
                        finalAsset.Get()->GetComponent()->RemoveEntity(entityToDelete);
                    }
                    entitiesToDelete.clear();
                }
            }


            // Return borrowed slice instances that are no longer needed post-clone.
            // This will transfer them back to the editor entity context.
            {
                using namespace AzFramework;

                for (const auto& addedSliceInstanceIt : m_addedSliceInstances)
                {
                    const SliceTransaction::SliceInstanceToPush& instanceToPush = addedSliceInstanceIt.second;
                    const AZ::SliceComponent::InstantiatedContainer* instantiated = instanceToPush.m_instanceAddress.second->GetInstantiated();
                    if (instantiated && !instantiated->m_entities.empty())
                    {
                        // Get the entity context owning this entity, and give back the slice instance.
                        EntityContextId owningContextId = EntityContextId::CreateNull();
                        EntityIdContextQueryBus::EventResult(owningContextId, instantiated->m_entities.front()->GetId(), &EntityIdContextQueries::GetOwningContextId);
                        if (!owningContextId.IsNull())
                        {
                            AZ::SliceComponent* rootSlice = nullptr;
                            EntityContextRequestBus::EventResult(rootSlice, owningContextId, &EntityContextRequests::GetRootSlice);
                            if (rootSlice)
                            {
                                rootSlice->AddSliceInstance(instanceToPush.m_instanceAddress.first, instanceToPush.m_instanceAddress.second);
                            }
                        }

                    }
                }
            }

            return finalAsset;
        }

        //=========================================================================
        SliceTransaction::Result SliceTransaction::PreSave(const char* fullPath, SliceAssetPtr& asset, PreSaveCallback preSaveCallback, AZ::u32 sliceCommitFlags)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            // Remap live Ids back to those of the asset.
            AZ::EntityUtils::SerializableEntityContainer assetEntities;
            asset.Get()->GetComponent()->GetEntities(assetEntities.m_entities);
            AZ::EntityUtils::ReplaceEntityIdsAndEntityRefs(&assetEntities,
                [this](const AZ::EntityId& originalId, bool /*isEntityId*/) -> AZ::EntityId
                    {
                        auto findIter = m_liveToAssetIdMap.find(originalId);
                        if (findIter != m_liveToAssetIdMap.end())
                        {
                            return findIter->second;
                        }

                        return originalId;
                    }, 
                m_serializeContext);

            // Invoke user pre-save callback.
            if (preSaveCallback)
            {
                Result preSaveResult = preSaveCallback(TransactionPtr(this), fullPath, asset);
                if (!preSaveResult)
                {
                    return preSaveResult;
                }
            }

            // Execute any standard pre-save behavior.
            if (sliceCommitFlags & SliceCommitFlags::ApplyWorldSliceTransformRules)
            {
                if (!VerifyAndApplyWorldTransformRules(asset))
                {
                    return AZ::Failure(AZStd::string::format("Transform root rules for slice push to asset \"%s\" could not be enforced.", fullPath));
                }
            }

            return AZ::Success();
        }

        //=========================================================================
        AZ::EntityId SliceTransaction::FindTargetAncestorAndUpdateInstanceIdMap(AZ::EntityId entityId, AZ::SliceComponent::EntityIdToEntityIdMap& liveToAssetIdMap, const AZ::SliceComponent::SliceInstanceAddress* ignoreSliceInstance) const
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            AZ::SliceComponent* slice = m_targetAsset.Get()->GetComponent();

            if (slice->FindEntity(entityId))
            {
                // Entity is already within the asset (not a live entity as part of an instance).
                return entityId;
            }

            // Entity is live entity, and we need to resolve the appropriate ancestor target.
            AZ::SliceComponent::SliceInstanceAddress instanceAddr(nullptr, nullptr);
            AzFramework::EntityIdContextQueryBus::EventResult(instanceAddr, entityId, &AzFramework::EntityIdContextQueryBus::Events::GetOwningSlice);
            const bool entityIsFromIgnoredSliceInstance = ignoreSliceInstance && ignoreSliceInstance->first && ignoreSliceInstance->first->GetSliceAsset().GetId() == instanceAddr.first->GetSliceAsset().GetId();
            if (instanceAddr.first && !entityIsFromIgnoredSliceInstance)
            {
                bool foundTargetAncestor = false;

                const AZ::SliceComponent::EntityList& entitiesInInstance = instanceAddr.second->GetInstantiated()->m_entities;

                // For every entity in the instance, get ancestry, and walk up the chain until we find
                // the ancestor corresponding to the target asset, building a fully resolved id map along the way.
                AZ::SliceComponent::EntityAncestorList ancestors;
                for (const AZ::Entity* entityInInstance : entitiesInInstance)
                {
                    ancestors.clear();
                    instanceAddr.first->GetInstanceEntityAncestry(entityInInstance->GetId(), ancestors, std::numeric_limits<AZ::u32>::max());
                    for (const AZ::SliceComponent::Ancestor& ancestor : ancestors)
                    {
                        auto& reverseIdMap = ancestor.m_sliceAddress.second->GetEntityIdToBaseMap();
                        auto idIter = liveToAssetIdMap.find(entityInInstance->GetId());
                        if (idIter != liveToAssetIdMap.end())
                        {
                            auto reverseIdIter = reverseIdMap.find(idIter->second);
                            if (reverseIdIter != reverseIdMap.end())
                            {
                                liveToAssetIdMap[entityInInstance->GetId()] = reverseIdIter->second;
                            }
                        }
                        else
                        {
                            auto reverseIdIter = reverseIdMap.find(entityInInstance->GetId());
                            if (reverseIdIter != reverseIdMap.end())
                            {
                                liveToAssetIdMap[entityInInstance->GetId()] = reverseIdIter->second;
                            }
                        }

                        if (ancestor.m_sliceAddress.first->GetSliceAsset().GetId() == m_targetAsset.GetId())
                        {
                            // Found the target asset, so we've resolved the final target Id for this entity.
                            foundTargetAncestor = true;
                            break;
                        }
                    }
                }

                auto findEntityIter = liveToAssetIdMap.find(entityId);
                if (findEntityIter == liveToAssetIdMap.end())
                {
                    return AZ::EntityId();
                }

                AZ_Error("SliceTransaction", foundTargetAncestor,
                         "Failed to locate ancestor in target asset for entity [%llu]. Some Id references may not be updated.",
                         entityId);

                return findEntityIter->second;
            }

            return AZ::EntityId();
        }

        //=========================================================================
        bool SliceTransaction::VerifyAndApplyWorldTransformRules(SliceAssetPtr& targetSlice)
        {
            AZ::SliceComponent::EntityList sliceEntities;
            targetSlice.Get()->GetComponent()->GetEntities(sliceEntities);

            AZ::u32 rootEntityCount = 0;
            for (AZ::Entity* entity : sliceEntities)
            {
                Components::TransformComponent* transformComponent =
                    entity->FindComponent<Components::TransformComponent>();

                if (transformComponent)
                {
                    // Cached world transform is only maintained for once-activated entities, not asset sources.
                    transformComponent->ClearCachedWorldTransform();

                    // Tally up root (non-child) entities.
                    if (!transformComponent->GetParentId().IsValid())
                    {
                        ++rootEntityCount;

                        // Root entity should be at the origin in the slice.
                        AZ::Transform transform = transformComponent->GetWorldTM();
                        transform.SetTranslation(AZ::Vector3::CreateZero());
                        transformComponent->SetWorldTM(transform);
                    }
                }
            }
            // Fail if the slice has more than one rule
            if (rootEntityCount > 1)
            {
                return false;
            }

            // Make sure that the root of a slice never has a parent
            AzToolsFramework::EntityList targetSliceEntities;
            targetSlice.Get()->GetComponent()->GetEntities(targetSliceEntities);

            AZ::EntityId commonRoot;
            AzToolsFramework::EntityList sliceRootEntities;

            bool result = false;
            AzToolsFramework::ToolsApplicationRequests::Bus::BroadcastResult(result, &AzToolsFramework::ToolsApplicationRequests::FindCommonRootInactive,
                                                                             targetSliceEntities, commonRoot, &sliceRootEntities);

            for (auto rootInFinalSlice : sliceRootEntities)
            {
                if (rootInFinalSlice)
                {
                    AzToolsFramework::Components::TransformComponent* transformComponent = rootInFinalSlice->FindComponent<AzToolsFramework::Components::TransformComponent>();

                    if (transformComponent)
                    {
                        transformComponent->SetParent(AZ::EntityId());
                    }
                }
            }

            return true;
        }

        //=========================================================================
        void SliceTransaction::Reset()
        {
            m_transactionType = TransactionType::None;
            m_serializeContext = nullptr;
            m_targetAsset = nullptr;
            m_addedSliceInstances.clear();
            m_liveToAssetIdMap.clear();
            m_entitiesToPush.clear();
            m_entitiesToRemove.clear();
        }

        //=========================================================================
        void SliceTransaction::add_ref()          
        { 
            ++m_refCount; 
        }

        //=========================================================================
        void SliceTransaction::release()
        {
            if (--m_refCount == 0)
            {
                delete this;
            }
        }

        //=========================================================================
        namespace Internal
        {
            //=========================================================================
            AZStd::string MakeTemporaryFilePathForSave(const char* fullPath)
            {
                AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
                AZ_Assert(fileIO, "File IO is not initialized.");

                AZStd::string devAssetPath = fileIO->GetAlias("@devassets@");
                AZStd::string userPath = fileIO->GetAlias("@user@");
                AZStd::string tempPath = fullPath;
                EBUS_EVENT(AzFramework::ApplicationRequests::Bus, NormalizePath, devAssetPath);
                EBUS_EVENT(AzFramework::ApplicationRequests::Bus, NormalizePath, userPath);
                EBUS_EVENT(AzFramework::ApplicationRequests::Bus, NormalizePath, tempPath);
                AzFramework::StringFunc::Replace(tempPath, "@devassets@", devAssetPath.c_str());
                AzFramework::StringFunc::Replace(tempPath, devAssetPath.c_str(), userPath.c_str());
                tempPath.append(".slicetemp");

                return tempPath;
            }

            //=========================================================================
            SliceTransaction::Result SaveSliceToDisk(const SliceTransaction::SliceAssetPtr& asset, const char* targetPath, AZ::SerializeContext* serializeContext)
            {
                AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

                AZ_Assert(asset.Get(), "Invalid asset provided, or asset is not created.");

                AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
                AZ_Assert(fileIO, "File IO is not initialized.");

                if (!serializeContext)
                {
                    EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
                    AZ_Assert(serializeContext, "Failed to retrieve application serialize context.");
                }

                // Write to a temporary location, and later move to the target location.
                const AZStd::string tempFilePath = MakeTemporaryFilePathForSave(targetPath);

                AZ::IO::FileIOStream fileStream(tempFilePath.c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary);
                if (fileStream.IsOpen())
                {
                    // First save slice asset to memory (in the desired file format)
                    AZStd::vector<char> memoryBuffer;
                    AZ::IO::ByteContainerStream<AZStd::vector<char> > memoryStream(&memoryBuffer);

                    bool savedToMemory;
                    {
                        AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "SliceUtilities::Internal::SaveSliceToDisk:SaveToMemoryStream");
                        savedToMemory = AZ::Utils::SaveObjectToStream(memoryStream, AZ::DataStream::ST_XML, asset.Get()->GetEntity(), serializeContext);
                    }

                    if (savedToMemory)
                    {
                        // Now that we have the desired file written in memory, write the in-memory copy to file (done as two separate steps 
                        // as an optimization - writing out XML to FileStream directly has significant overhead)
                        bool savedToFile;
                        {
                            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "SliceUtilities::Internal::SaveSliceToDisk:SaveToFileStream");
                            memoryStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                            savedToFile = fileStream.Write(memoryStream.GetLength(), memoryStream.GetData()->data());
                        }
                        fileStream.Close();

                        if (savedToFile)
                        {
                            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "SliceUtilities::Internal::SaveSliceToDisk:TempToTargetFileReplacement");

                            // Copy scratch file to target location.
                            const bool targetFileExists = fileIO->Exists(targetPath);
                            const bool removedTargetFile = fileIO->Remove(targetPath);

                            if (targetFileExists && !removedTargetFile)
                            {
                                return AZ::Failure(AZStd::string::format("Unable to modify existing target slice file. Please make the slice writeable and try again."));
                            }

                            AZ::IO::Result renameResult = fileIO->Rename(tempFilePath.c_str(), targetPath);
                            if (!renameResult)
                            {
                                return AZ::Failure(AZStd::string::format("Unable to move temporary slice file \"%s\" to target location.", tempFilePath.c_str()));
                            }

                            // Bump the slice asset up in the asset processor's queue.
                            EBUS_EVENT(AzFramework::AssetSystemRequestBus, GetAssetStatus, targetPath);
                            return AZ::Success();
                        }
                        else
                        {
                            return AZ::Failure(AZStd::string::format("Unable to save slice to a temporary file at location: \"%s\".", tempFilePath.c_str()));
                        }
                    }
                    else
                    {
                        fileStream.Close();
                        return AZ::Failure(AZStd::string::format("Unable to save slice to memory before saving to a temporary file at location: \"%s\".", tempFilePath.c_str()));
                    }
                }
                else
                {
                    return AZ::Failure(AZStd::string::format("Unable to create temporary slice file at location: \"%s\".", tempFilePath.c_str()));
                }
            }

        } // namespace Internal

    } // namespace SliceUtilities

} // namespace AzToolsFramework
