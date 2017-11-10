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
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Slice/SliceBus.h>

#include <AzCore/Component/EntityUtils.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Asset/AssetManager.h>

///////////////////////////////////////////////////////
// Temp while the asset system is done
#include <AzCore/Serialization/Utils.h>
///////////////////////////////////////////////////////

namespace AZ
{
    namespace Converters
    {
        // SliceReference Version 1 -> 2
        // SliceReference::m_instances converted from AZStd::list<SliceInstance> to AZStd::unordered_set<SliceInstance>.
        bool SliceReferenceVersionConverter(SerializeContext& context, SerializeContext::DataElementNode& classElement)
        {
            if (classElement.GetVersion() < 2)
            {
                const int instancesIndex = classElement.FindElement(AZ_CRC("Instances", 0x7a270069));
                if (instancesIndex > -1)
                {
                    auto& instancesElement = classElement.GetSubElement(instancesIndex);

                    // Extract sub-elements, which we can just re-add under the set.
                    AZStd::vector<SerializeContext::DataElementNode> subElements;
                    subElements.reserve(instancesElement.GetNumSubElements());
                    for (int i = 0, n = instancesElement.GetNumSubElements(); i < n; ++i)
                    {
                        subElements.push_back(instancesElement.GetSubElement(i));
                    }

                    // Convert to unordered_set.
                    using SetType = AZStd::unordered_set<SliceComponent::SliceInstance>;
                    if (instancesElement.Convert<SetType>(context))
                    {
                        for (const SerializeContext::DataElementNode& subElement : subElements)
                        {
                            instancesElement.AddElement(subElement);
                        }
                    }

                    return true;
                }

                return false;
            }

            return true;
        }

    } // namespace Converters

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::Reflect(ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<DataFlagsPerEntity>()
                ->Version(1)
                ->Field("EntityToDataFlags", &DataFlagsPerEntity::m_entityToDataFlags)
                ;
        }
    }

    //=========================================================================
    SliceComponent::DataFlagsPerEntity::DataFlagsPerEntity(const IsValidEntityFunction& isValidEntityFunction)
        : m_isValidEntityFunction(isValidEntityFunction)
    {
        AZ_Assert(m_isValidEntityFunction, "DataFlagsPerEntity requires a function for checking entity validity");
    }
        
    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::CopyDataFlagsFrom(const DataFlagsPerEntity& rhs)
    {
        m_entityToDataFlags = rhs.m_entityToDataFlags;
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::CopyDataFlagsFrom(DataFlagsPerEntity&& rhs)
    {
        m_entityToDataFlags = AZStd::move(rhs.m_entityToDataFlags);
    }

    //=========================================================================
    const DataPatch::FlagsMap& SliceComponent::DataFlagsPerEntity::GetEntityDataFlags(EntityId entityId) const
    {
        auto foundDataFlags = m_entityToDataFlags.find(entityId);
        if (foundDataFlags != m_entityToDataFlags.end())
        {
            return foundDataFlags->second;
        }

        static const DataPatch::FlagsMap emptyFlagsMap;
        return emptyFlagsMap;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::SetEntityDataFlags(EntityId entityId, const DataPatch::FlagsMap& dataFlags)
    {
        if (IsValidEntity(entityId))
        {
            if (!dataFlags.empty())
            {
                m_entityToDataFlags[entityId] = dataFlags;
            }
            else
            {
                // If entity has no data flags, erase the entity's map entry.
                m_entityToDataFlags.erase(entityId);
            }
            return true;
        }
        return false;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::ClearEntityDataFlags(EntityId entityId)
    {
        if (IsValidEntity(entityId))
        {
            m_entityToDataFlags.erase(entityId);
            return true;
        }
        return false;
    }

    //=========================================================================
    DataPatch::Flags SliceComponent::DataFlagsPerEntity::GetEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress) const
    {
        auto foundEntityDataFlags = m_entityToDataFlags.find(entityId);
        if (foundEntityDataFlags != m_entityToDataFlags.end())
        {
            const DataPatch::FlagsMap& entityDataFlags = foundEntityDataFlags->second;
            auto foundDataFlags = entityDataFlags.find(dataAddress);
            if (foundDataFlags != entityDataFlags.end())
            {
                return foundDataFlags->second;
            }
        }

        return 0;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::SetEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress, DataPatch::Flags flags)
    {
        if (IsValidEntity(entityId))
        {
            if (flags != 0)
            {
                m_entityToDataFlags[entityId][dataAddress] = flags;
            }
            else
            {
                // If clearing the flags, erase the data address's map entry.
                // If the entity no longer has any data flags, erase the entity's map entry.
                auto foundEntityDataFlags = m_entityToDataFlags.find(entityId);
                if (foundEntityDataFlags != m_entityToDataFlags.end())
                {
                    DataPatch::FlagsMap& entityDataFlags = foundEntityDataFlags->second;
                    entityDataFlags.erase(dataAddress);
                    if (entityDataFlags.empty())
                    {
                        m_entityToDataFlags.erase(foundEntityDataFlags);
                    }
                }
            }

            return true;
        }
        return false;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::IsValidEntity(EntityId entityId) const
    {
        if (m_isValidEntityFunction)
        {
            if (m_isValidEntityFunction(entityId))
            {
                return true;
            }
            return false;
        }
        return true; // if no validity function is set, always return true
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::Cleanup(const EntityList& validEntities)
    {
        AZStd::unordered_set<EntityId> validEntityIds;
        for (const Entity* entity : validEntities)
        {
            validEntityIds.insert(entity->GetId());
        }

        for (auto entityDataFlagIterator = m_entityToDataFlags.begin(); entityDataFlagIterator != m_entityToDataFlags.end(); )
        {
            EntityId entityId = entityDataFlagIterator->first;
            if (validEntityIds.find(entityId) != validEntityIds.end())
            {
                // TODO LY-52686: Prune flags if their address doesn't line up with anything in this entity.

                ++entityDataFlagIterator;
            }
            else
            {
                entityDataFlagIterator = m_entityToDataFlags.erase(entityDataFlagIterator);
            }
        }
    }

    //=========================================================================
    // SliceComponent::InstantiatedContainer::~InstanceContainer
    //=========================================================================
    SliceComponent::InstantiatedContainer::~InstantiatedContainer()
    {
        DeleteEntities();
    }

    //=========================================================================
    // SliceComponent::InstantiatedContainer::DeleteEntities
    //=========================================================================
    void SliceComponent::InstantiatedContainer::DeleteEntities()
    {
        for (Entity* entity : m_entities)
        {
            delete entity;
        }
        m_entities.clear();
    }

    //=========================================================================
    // SliceComponent::SliceInstance::SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::SliceInstance(const SliceInstanceId& id)
        : m_instantiated(nullptr)
        , m_instanceId(id)
        , m_dataFlags(GenerateValidEntityFunction(this))
    {
    }

    //=========================================================================
    // SliceComponent::SliceInstance::SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::SliceInstance(SliceInstance&& rhs)
        : m_dataFlags(GenerateValidEntityFunction(this))
    {
        m_instantiated = rhs.m_instantiated;
        rhs.m_instantiated = nullptr;
        m_baseToNewEntityIdMap = AZStd::move(rhs.m_baseToNewEntityIdMap);
        m_entityIdToBaseCache = AZStd::move(rhs.m_entityIdToBaseCache);
        m_dataPatch = AZStd::move(rhs.m_dataPatch);
        m_dataFlags.CopyDataFlagsFrom(AZStd::move(rhs.m_dataFlags));
        m_instanceId = rhs.m_instanceId;
        rhs.m_instanceId = SliceInstanceId::CreateNull();
    }

    //=========================================================================
    // SliceComponent::SliceInstance::SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::SliceInstance(const SliceInstance& rhs)
        : m_dataFlags(GenerateValidEntityFunction(this))
    {
        m_instantiated = rhs.m_instantiated;
        m_baseToNewEntityIdMap = rhs.m_baseToNewEntityIdMap;
        m_entityIdToBaseCache = rhs.m_entityIdToBaseCache;
        m_dataPatch = rhs.m_dataPatch;
        m_dataFlags.CopyDataFlagsFrom(rhs.m_dataFlags);
        m_instanceId = rhs.m_instanceId;
    }

    //=========================================================================
    // SliceComponent::SliceInstance::BuildReverseLookUp
    //=========================================================================
    void SliceComponent::SliceInstance::BuildReverseLookUp() const
    {
        m_entityIdToBaseCache.clear();
        for (const auto& entityIdPair : m_baseToNewEntityIdMap)
        {
            m_entityIdToBaseCache.insert(AZStd::make_pair(entityIdPair.second, entityIdPair.first));
        }
    }

    //=========================================================================
    // SliceComponent::SliceInstance::~SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::~SliceInstance()
    {
        delete m_instantiated;
    }

    //=========================================================================
    // SliceComponent::SliceInstance::GenerateValidEntityFunction
    //=========================================================================
    SliceComponent::DataFlagsPerEntity::IsValidEntityFunction SliceComponent::SliceInstance::GenerateValidEntityFunction(const SliceInstance* instance)
    {
        auto isValidEntityFunction = [instance](EntityId entityId)
        {
            const EntityIdToEntityIdMap& entityIdToBaseMap = instance->GetEntityIdToBaseMap();
            return entityIdToBaseMap.find(entityId) != entityIdToBaseMap.end();
        };

        return isValidEntityFunction;
    }

    //=========================================================================
    // SliceComponent::SliceInstance::GetDataFlagsForPatching
    //=========================================================================
    DataPatch::FlagsMap SliceComponent::SliceInstance::GetDataFlagsForPatching() const
    {
        // Collect all entities' data flags
        DataPatch::FlagsMap dataFlags;

        for (auto& baseIdInstanceIdPair : GetEntityIdMap())
        {
            // Make the addressing relative to InstantiatedContainer (m_dataFlags stores flags relative to the individual entity)
            DataPatch::AddressType addressPrefix;
            addressPrefix.push_back(AZ_CRC("Entities", 0x50ec64e5));
            addressPrefix.push_back(static_cast<u64>(baseIdInstanceIdPair.first));

            for (auto& addressDataFlagsPair : m_dataFlags.GetEntityDataFlags(baseIdInstanceIdPair.second))
            {
                const DataPatch::AddressType& originalAddress = addressDataFlagsPair.first;

                DataPatch::AddressType prefixedAddress;
                prefixedAddress.reserve(addressPrefix.size() + originalAddress.size());
                prefixedAddress.insert(prefixedAddress.end(), addressPrefix.begin(), addressPrefix.end());
                prefixedAddress.insert(prefixedAddress.end(), originalAddress.begin(), originalAddress.end());

                dataFlags.emplace(AZStd::move(prefixedAddress), addressDataFlagsPair.second);
            }
        }

        return dataFlags;
    }

    //=========================================================================
    // SliceComponent::SliceReference::SliceReference
    //=========================================================================
    SliceComponent::SliceReference::SliceReference()
        : m_isInstantiated(false)
        , m_component(nullptr)
    {
    }
    
    //=========================================================================
    // SliceComponent::SliceReference::CreateEmptyInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::CreateEmptyInstance(const SliceInstanceId& instanceId)
    {
        SliceInstance* instance = &(*m_instances.emplace(instanceId).first);
        return instance;
    }

    //=========================================================================
    // SliceComponent::SliceReference::CreateInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::CreateInstance(const AZ::EntityUtils::EntityIdMapper& customMapper)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // create an empty instance (just copy of the exiting data)
        SliceInstance* instance = CreateEmptyInstance();

        if (m_isInstantiated)
        {
            AZ_Assert(m_asset.IsReady(), "If we an in instantiated state all dependent asset should be ready!");
            SliceComponent* dependentSlice = m_asset.Get()->GetComponent();

            InstantiatedContainer sourceObjects;
            dependentSlice->GetEntities(sourceObjects.m_entities);

            instance->m_instantiated = dependentSlice->GetSerializeContext()->CloneObject(&sourceObjects);

            AZ::EntityUtils::ReplaceEntityIds(
                instance->m_instantiated,
                [&](const EntityId& originalId, bool isEntityId) -> EntityId
                {
                    EntityId newId = customMapper ? customMapper(originalId, isEntityId) : Entity::MakeId();
                    instance->m_baseToNewEntityIdMap.insert(AZStd::make_pair(originalId, newId));
                    return newId;
                }, dependentSlice->GetSerializeContext());

            AZ::EntityUtils::ReplaceEntityRefs(
                instance->m_instantiated,
                [instance](const EntityId& originalId, bool /*isEntityId*/) -> EntityId
                {
                    auto findIt = instance->m_baseToNewEntityIdMap.find(originalId);
                    if (findIt == instance->m_baseToNewEntityIdMap.end())
                    {
                        return originalId; // Referenced EntityId is not part of the slice, so keep the same id reference.
                    }
                    else
                    {
                        return findIt->second; // return the remapped id
                    }
                }, dependentSlice->GetSerializeContext());

            AZ_Assert(m_component, "We need a valid component to use this operation!");

            if (!m_component->m_entityInfoMap.empty())
            {
                AddInstanceToEntityInfoMap(*instance);
            }

            sourceObjects.m_entities.clear(); // make sure we don't delete the entities as we don't own them.
        }

        return instance;
    }

    //=========================================================================
    // SliceComponent::SliceReference::CloneInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::CloneInstance(SliceComponent::SliceInstance* instance, 
                                                                                 SliceComponent::EntityIdToEntityIdMap& sourceToCloneEntityIdMap)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // check if source instance belongs to this slice reference
        auto findIt = AZStd::find_if(m_instances.begin(), m_instances.end(), [instance](const SliceInstance& element) -> bool { return &element == instance; });
        if (findIt == m_instances.end())
        {
            AZ_Error("Slice", false, "SliceInstance %p doesn't belong to this SliceReference %p!", instance, this);
            return nullptr;
        }

        SliceInstance* newInstance = CreateEmptyInstance();

        if (m_isInstantiated)
        {
            SerializeContext* serializeContext = m_asset.Get()->GetComponent()->GetSerializeContext();

            // clone the entities
            newInstance->m_instantiated = EntityUtils::CloneObjectAndFixEntities(instance->m_instantiated, sourceToCloneEntityIdMap, serializeContext);

            const EntityIdToEntityIdMap& instanceToBaseSliceEntityIdMap = instance->GetEntityIdToBaseMap();
            for (AZStd::pair<AZ::EntityId, AZ::EntityId>& sourceIdToCloneId : sourceToCloneEntityIdMap)
            {
                EntityId sourceId = sourceIdToCloneId.first;
                EntityId cloneId = sourceIdToCloneId.second;

                auto instanceIdToBaseId = instanceToBaseSliceEntityIdMap.find(sourceId);
                if (instanceIdToBaseId == instanceToBaseSliceEntityIdMap.end())
                {
                    AZ_Assert(false, "An entity cloned (id: %s) couldn't be found in the source slice instance!", sourceId.ToString().c_str());
                }
                EntityId baseId = instanceIdToBaseId->second;

                newInstance->m_baseToNewEntityIdMap.insert(AZStd::make_pair(baseId, cloneId));
                newInstance->m_entityIdToBaseCache.insert(AZStd::make_pair(cloneId, baseId));

                newInstance->m_dataFlags.SetEntityDataFlags(cloneId, instance->m_dataFlags.GetEntityDataFlags(sourceId));
            }

            if (!m_component->m_entityInfoMap.empty())
            {
                AddInstanceToEntityInfoMap(*newInstance);
            }
        }
        else
        {
            // clone data patch
            AZ_Assert(false, "todo regenerate the entity map id and copy data flags");
            newInstance->m_dataPatch = instance->m_dataPatch;
        }

        return newInstance;
    }

    //=========================================================================
    // SliceComponent::SliceReference::FindInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::FindInstance(const SliceInstanceId& instanceId)
    {
        auto iter = m_instances.find_as(instanceId, AZStd::hash<SliceInstanceId>(), 
            [](const SliceInstanceId& id, const SliceInstance& instance)
            {
                return (id == instance.GetId());
            }
        );

        if (iter != m_instances.end())
        {
            return &(*iter);
        }

        return nullptr;
    }

    //=========================================================================
    // SliceComponent::SliceReference::RemoveInstance
    //=========================================================================
    bool SliceComponent::SliceReference::RemoveInstance(SliceComponent::SliceInstance* instance)
    {
        for (auto instanceIt = m_instances.begin(); instanceIt != m_instances.end(); ++instanceIt)
        {
            if (&*instanceIt == instance)
            {
                RemoveInstanceFromEntityInfoMap(*instance);
                m_instances.erase(instanceIt);
                return true;
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::SliceReference::RemoveInstance
    //=========================================================================
    bool SliceComponent::SliceReference::RemoveEntity(EntityId entityId, bool isDeleteEntity, SliceInstance* instance)
    {
        if (!instance)
        {
            instance = m_component->FindSlice(entityId).second;

            if (!instance)
            {
                return false; // can't find an instance the owns this entity
            }
        }

        auto entityIt = AZStd::find_if(instance->m_instantiated->m_entities.begin(), instance->m_instantiated->m_entities.end(), [entityId](Entity* entity) -> bool { return entity->GetId() == entityId; });
        if (entityIt != instance->m_instantiated->m_entities.end())
        {
            if (isDeleteEntity)
            {
                delete *entityIt;
            }
            instance->m_instantiated->m_entities.erase(entityIt);
            if (instance->m_entityIdToBaseCache.empty())
            {
                instance->BuildReverseLookUp();
            }

            instance->m_dataFlags.ClearEntityDataFlags(entityId);

            auto entityToBaseIt = instance->m_entityIdToBaseCache.find(entityId);
            AZ_Assert(entityToBaseIt != instance->m_entityIdToBaseCache.end(), "Reverse lookup cache is inconsistent, please check it's logic!");
            instance->m_baseToNewEntityIdMap.erase(entityToBaseIt->second);
            instance->m_entityIdToBaseCache.erase(entityToBaseIt);
            return true;
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::SliceReference::GetInstances
    //=========================================================================
    const SliceComponent::SliceReference::SliceInstances& SliceComponent::SliceReference::GetInstances() const
    {
        return m_instances;
    }

    //=========================================================================
    // SliceComponent::SliceReference::IsInstantiated
    //=========================================================================
    bool SliceComponent::SliceReference::IsInstantiated() const
    {
        return m_isInstantiated;
    }

    //=========================================================================
    // SliceComponent::SliceReference::Instantiate
    //=========================================================================
    bool SliceComponent::SliceReference::Instantiate(const AZ::ObjectStream::FilterDescriptor& filterDesc)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (m_isInstantiated)
        {
            return true;
        }

        if (!m_asset.IsReady())
        {
            AZ_Error("Slice", false, "Slice asset %s (%s) not ready or not found! Slice data based on the asset will likely be lost.",
                m_asset.GetHint().c_str(), m_asset.GetId().ToString<AZStd::string>().c_str());
            return false;
        }

        m_isInstantiated = true;

        for (SliceInstance& instance : m_instances)
        {
            InstantiateInstance(instance, filterDesc);
        }
        return true;
    }

    //=========================================================================
    // SliceComponent::SliceReference::UnInstantiate
    //=========================================================================
    void SliceComponent::SliceReference::UnInstantiate()
    {
        if (m_isInstantiated)
        {
            m_isInstantiated = false;

            for (SliceInstance& instance : m_instances)
            {
                delete instance.m_instantiated;
                instance.m_instantiated = nullptr;
            }
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::InstantiateInstance
    //=========================================================================
    void SliceComponent::SliceReference::InstantiateInstance(SliceInstance& instance, const AZ::ObjectStream::FilterDescriptor& filterDesc)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        InstantiatedContainer sourceObjects;
        SliceComponent* dependentSlice = m_asset.Get()->GetComponent();

        DataPatch& dataPatch = instance.m_dataPatch;
        EntityIdToEntityIdMap& entityIdMap = instance.m_baseToNewEntityIdMap;
        dependentSlice->GetEntities(sourceObjects.m_entities);

        // An empty map indicates its a fresh instance (i.e. has never be instantiated and then serialized).
        if (entityIdMap.empty())
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "SliceComponent::SliceReference::InstantiateInstance:FreshInstanceClone");

            // Generate new Ids and populate the map.
            AZ_Assert(!dataPatch.IsValid(), "Data patch is valid for slice instance, but entity Id map is not!");
            instance.m_instantiated = EntityUtils::CloneObjectAndFixEntities(&sourceObjects, entityIdMap, dependentSlice->GetSerializeContext());
        }
        else
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "SliceComponent::SliceReference::InstantiateInstance:CloneAndApplyDataPatches");

            // Clone entities while applying any data patches.
            AZ_Assert(dataPatch.IsValid(), "Data patch is not valid for existing slice instance!");
            instance.m_instantiated = dataPatch.Apply(&sourceObjects, dependentSlice->GetSerializeContext(), filterDesc);

            // Remap Ids & references.
            EntityUtils::ReplaceEntityIdsAndEntityRefs(instance.m_instantiated, [&entityIdMap](const EntityId& sourceId, bool isEntityId) -> EntityId
            {
                EntityId remappedId;
                auto findIt = entityIdMap.find(sourceId);
                if (findIt != entityIdMap.end())
                {
                    return findIt->second;
                }
                else
                {
                    if (isEntityId)
                    {
                        const EntityId id = Entity::MakeId();
                        entityIdMap[sourceId] = id;
                        return id;
                    }

                    return sourceId;
                }
            }, dependentSlice->GetSerializeContext());

            // Prune any entities in from our map that are no longer present in the dependant slice.
            if (entityIdMap.size() != sourceObjects.m_entities.size())
            {
                const SliceComponent::EntityInfoMap& dependentInfoMap = dependentSlice->GetEntityInfoMap();
                for (auto mapIt = entityIdMap.begin(); mapIt != entityIdMap.end(); )
                {
                    auto findInDependentIt = dependentInfoMap.find(mapIt->first);
                    if (findInDependentIt == dependentInfoMap.end())
                    {
                        mapIt = entityIdMap.erase(mapIt);
                    }
                    else
                    {
                        ++mapIt;
                    }
                }
            }
        }

        // Ensure reverse lookup is cleared (recomputed on access).
        instance.m_entityIdToBaseCache.clear();

        // don't delete the entities
        sourceObjects.m_entities.clear();
    }

    //=========================================================================
    // SliceComponent::SliceReference::AddInstanceToEntityInfoMap
    //=========================================================================
    void SliceComponent::SliceReference::AddInstanceToEntityInfoMap(SliceInstance& instance)
    {
        AZ_Assert(m_component, "You need to have a valid component set to update the global entityInfoMap!");
        if (instance.m_instantiated)
        {
            auto& entityInfoMap = m_component->m_entityInfoMap;
            for (Entity* entity : instance.m_instantiated->m_entities)
            {
                entityInfoMap.insert(AZStd::make_pair(entity->GetId(), EntityInfo(entity, SliceInstanceAddress(this, &instance))));
            }
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::RemoveInstanceFromEntityInfoMap
    //=========================================================================
    void SliceComponent::SliceReference::RemoveInstanceFromEntityInfoMap(SliceInstance& instance)
    {
        AZ_Assert(m_component, "You need to have a valid component set to update the global entityInfoMap!");
        if (!m_component->m_entityInfoMap.empty() && instance.m_instantiated)
        {
            for (Entity* entity : instance.m_instantiated->m_entities)
            {
                m_component->m_entityInfoMap.erase(entity->GetId());
            }
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::GetInstanceEntityAncestry
    //=========================================================================
    bool SliceComponent::SliceReference::GetInstanceEntityAncestry(const EntityId& instanceEntityId, EntityAncestorList& ancestors, u32 maxLevels) const
    {
        maxLevels = AZStd::GetMax(maxLevels, static_cast<u32>(1));

        // End recursion when we've reached the max level of ancestors requested
        if (ancestors.size() == maxLevels)
        {
            return true;
        }

        // Locate the instance containing the input entity, which should be a live instanced entity.
        for (const SliceInstance& instance : m_instances)
        {
            // Given the instance's entity Id, resolve the Id of the source entity in the asset.
            auto foundIt = instance.GetEntityIdToBaseMap().find(instanceEntityId);
            if (foundIt != instance.GetEntityIdToBaseMap().end())
            {
                const EntityId assetEntityId = foundIt->second;

                // Ancestor is assetEntityId in this instance's asset.
                const EntityInfoMap& assetEntityInfoMap = m_asset.Get()->GetComponent()->GetEntityInfoMap();
                auto entityInfoIt = assetEntityInfoMap.find(assetEntityId);
                if (entityInfoIt != assetEntityInfoMap.end())
                {
                    ancestors.emplace_back(entityInfoIt->second.m_entity, SliceInstanceAddress(const_cast<SliceReference*>(this), const_cast<SliceInstance*>(&instance)));
                    if (entityInfoIt->second.m_sliceAddress.first)
                    {
                        return entityInfoIt->second.m_sliceAddress.first->GetInstanceEntityAncestry(assetEntityId, ancestors, maxLevels);
                    }
                }
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::SliceReference::ComputeDataPatch
    //=========================================================================
    void SliceComponent::SliceReference::ComputeDataPatch()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // Get source entities from the base asset (instantiate if needed)
        InstantiatedContainer source;
        m_asset.Get()->GetComponent()->GetEntities(source.m_entities);
        SerializeContext* serializeContext = m_asset.Get()->GetComponent()->GetSerializeContext();

        // Compute the delta/changes for each instance
        for (SliceInstance& instance : m_instances)
        {
            // remap entity ids to the "original"
            const EntityIdToEntityIdMap& reverseLookUp = instance.GetEntityIdToBaseMap();
            EntityUtils::ReplaceEntityIdsAndEntityRefs(instance.m_instantiated, [&reverseLookUp](const EntityId& sourceId, bool /*isEntityId*/) -> EntityId
            {
                auto findIt = reverseLookUp.find(sourceId);
                if (findIt != reverseLookUp.end())
                {
                    return findIt->second;
                }
                else
                {
                    return sourceId;
                }
            }, serializeContext);

            // compute the delta (what we changed from the base slice)
            instance.m_dataPatch.Create(&source, instance.m_instantiated, instance.GetDataFlagsForPatching(), serializeContext);

            // remap entity ids back to the "instance onces"
            EntityUtils::ReplaceEntityIdsAndEntityRefs(instance.m_instantiated, [&instance](const EntityId& sourceId, bool /*isEntityId*/) -> EntityId
            {
                auto findIt = instance.m_baseToNewEntityIdMap.find(sourceId);
                if (findIt != instance.m_baseToNewEntityIdMap.end())
                {
                    return findIt->second;
                }
                else
                {
                    return sourceId;
                }
            }, serializeContext);

            // clean up orphaned data flags (ex: for entities that no longer exist).
            instance.m_dataFlags.Cleanup(instance.m_instantiated->m_entities);
        }

        source.m_entities.clear(); // Release entities from the container's ownership. We were only using it for enumeration.
    }

    //=========================================================================
    // SliceComponent
    //=========================================================================
    SliceComponent::SliceComponent()
        : m_myAsset(nullptr)
        , m_serializeContext(nullptr)
        , m_slicesAreInstantiated(false)
        , m_allowPartialInstantiation(true)
        , m_isDynamic(false)
        , m_filterFlags(0)
    {
    }

    //=========================================================================
    // ~SliceComponent
    //=========================================================================
    SliceComponent::~SliceComponent()
    {
        for (Entity* entity : m_entities)
        {
            delete entity;
        }
    }

    //=========================================================================
    // SliceComponent::GetNewEntities
    //=========================================================================
    const SliceComponent::EntityList& SliceComponent::GetNewEntities() const
    {
        return m_entities;
    }

    //=========================================================================
    // SliceComponent::GetEntities
    //=========================================================================
    bool SliceComponent::GetEntities(EntityList& entities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        bool result = true;

        // make sure we have all entities instantiated
        if (!Instantiate())
        {
            result = false;
        }

        // add all entities from base slices
        for (const SliceReference& slice : m_slices)
        {
            for (const SliceInstance& instance : slice.m_instances)
            {
                entities.insert(entities.end(), instance.m_instantiated->m_entities.begin(), instance.m_instantiated->m_entities.end());
            }
        }

        // add new entities (belong to the current slice)
        entities.insert(entities.end(), m_entities.begin(), m_entities.end());

        return result;
    }

    //=========================================================================
    // SliceComponent::GetSlices
    //=========================================================================
    const SliceComponent::SliceList& SliceComponent::GetSlices() const
    {
        return m_slices;
    }

    //=========================================================================
    // SliceComponent::GetSlice
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::GetSlice(const Data::Asset<SliceAsset>& sliceAsset)
    {
        return GetSlice(sliceAsset.GetId());
    }

    //=========================================================================
    // SliceComponent::GetSlice
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::GetSlice(const Data::AssetId& assetId)
    {
        for (SliceReference& slice : m_slices)
        {
            if (slice.m_asset.GetId() == assetId)
            {
                return &slice;
            }
        }

        return nullptr;
    }

    //=========================================================================
    // SliceComponent::Instantiate
    //=========================================================================
    bool SliceComponent::Instantiate()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);
        AZStd::unique_lock<AZStd::mutex> lock(m_instantiateMutex);

        if (m_slicesAreInstantiated)
        {
            return true;
        }

        // check if assets are loaded
        bool result = true;
        for (SliceReference& slice : m_slices)
        {
            if (!slice.Instantiate(m_assetLoadFilterCB))
            {
                result = false;
            }
        }

        m_slicesAreInstantiated = result;

        if (!result)
        {
            if (m_allowPartialInstantiation)
            {
                // Strip any references that failed to instantiate.
                for (auto iter = m_slices.begin(); iter != m_slices.end(); )
                {
                    if (!iter->IsInstantiated())
                    {
                        iter = m_slices.erase(iter);
                    }
                    else
                    {
                        ++iter;
                        m_slicesAreInstantiated = true; // At least one reference was instantiated.
                    }
                }
            }
            else
            {
                // Completely roll back instantiation.
                for (SliceReference& slice : m_slices)
                {
                    if (slice.IsInstantiated())
                    {
                        slice.UnInstantiate();
                    }
                }
            }
        }

        return result;
    }

    //=========================================================================
    // SliceComponent::IsInstantiated
    //=========================================================================
    bool SliceComponent::IsInstantiated() const
    {
        return m_slicesAreInstantiated;
    }

    //=========================================================================
    // SliceComponent::AddSlice
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::AddSlice(const Data::Asset<SliceAsset>& sliceAsset, const EntityUtils::EntityIdMapper& customMapper)
    {
        SliceReference* slice = AddOrGetSliceReference(sliceAsset);
        return SliceInstanceAddress(slice, slice->CreateInstance(customMapper));
    }

    //=========================================================================
    // SliceComponent::AddSliceInstance
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::AddSlice(SliceReference& sliceReference)
    {
        // Assert for input parameters
        // Assert that we don't already have a reference for this assetId
        AZ_Assert(!Data::AssetBus::MultiHandler::BusIsConnectedId(sliceReference.GetSliceAsset().GetId()), "We already have a slice reference to this asset");
        AZ_Assert(false == sliceReference.m_isInstantiated, "Slice reference is already instantiated.");

        Data::AssetBus::MultiHandler::BusConnect(sliceReference.GetSliceAsset().GetId());
        m_slices.emplace_back(AZStd::move(sliceReference));
        SliceReference* slice = &m_slices.back();
        slice->m_component = this;

        // check if we instantiated but the reference is not, instantiate
        // if the reference is and we are not, delete it
        if (m_slicesAreInstantiated)
        {
            slice->Instantiate(m_assetLoadFilterCB);
        }

        // Refresh entity map for newly-created instances.
        BuildEntityInfoMap();

        return slice;
    }
    
    //=========================================================================
    // SliceComponent::GetEntityRestoreInfo
    //=========================================================================
    bool SliceComponent::GetEntityRestoreInfo(const EntityId entityId, EntityRestoreInfo& restoreInfo)
    {
        restoreInfo = EntityRestoreInfo();

        const EntityInfoMap& entityInfo = GetEntityInfoMap();
        auto entityInfoIter = entityInfo.find(entityId);
        if (entityInfoIter != entityInfo.end())
        {
            SliceReference* reference = entityInfoIter->second.m_sliceAddress.first;
            if (reference)
            {
                SliceInstance* instance = entityInfoIter->second.m_sliceAddress.second;
                AZ_Assert(instance, "Entity %llu was found to belong to reference %s, but instance is invalid.", 
                          entityId, reference->GetSliceAsset().GetId().ToString<AZStd::string>().c_str());

                EntityAncestorList ancestors;
                reference->GetInstanceEntityAncestry(entityId, ancestors, 1);
                if (!ancestors.empty())
                {
                    restoreInfo = EntityRestoreInfo(reference->GetSliceAsset(), instance->GetId(), ancestors.front().m_entity->GetId(), instance->m_dataFlags.GetEntityDataFlags(entityId));
                    return true;
                }
                else
                {
                    AZ_Error("Slice", false, "Entity with id %llu was found, but has no valid ancestry.", entityId);
                }
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::RestoreEntity
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::RestoreEntity(Entity* entity, const EntityRestoreInfo& restoreInfo)
    {
        Data::Asset<SliceAsset> asset = Data::AssetManager::Instance().FindAsset(restoreInfo.m_assetId);

        if (!asset.IsReady())
        {
            AZ_Error("Slice", false, "Slice asset %s is not ready. Caller needs to ensure the asset is loaded.", restoreInfo.m_assetId.ToString<AZStd::string>().c_str());
            return SliceComponent::SliceInstanceAddress(nullptr, nullptr);
        }

        if (!IsInstantiated())
        {
            AZ_Error("Slice", false, "Cannot add entities to existing instances if the slice hasn't yet been instantiated.");
            return SliceComponent::SliceInstanceAddress(nullptr, nullptr);
        }
        
        SliceComponent* sourceSlice = asset.GetAs<SliceAsset>()->GetComponent();
        sourceSlice->Instantiate();
        auto& sourceEntityMap = sourceSlice->GetEntityInfoMap();
        if (sourceEntityMap.find(restoreInfo.m_ancestorId) == sourceEntityMap.end())
        {
            AZ_Error("Slice", false, "Ancestor Id of %llu is invalid. It must match an entity in source asset %s.", 
                     restoreInfo.m_ancestorId, asset.GetId().ToString<AZStd::string>().c_str());
            return SliceComponent::SliceInstanceAddress(nullptr, nullptr);
        }

        const SliceComponent::SliceInstanceAddress address = FindSlice(entity);
        if (address.first)
        {
            return address;
        }

        SliceReference* reference = AddOrGetSliceReference(asset);
        SliceInstance* instance = reference->FindInstance(restoreInfo.m_instanceId);

        if (!instance)
        {
            // We're creating an instance just to hold the entity we're re-adding. We don't want to instantiate the underlying asset.
            instance = reference->CreateEmptyInstance(restoreInfo.m_instanceId);
            instance->m_instantiated = aznew InstantiatedContainer();
        }

        // Add the entity to the instance, and wipe the reverse lookup cache so it's updated on access.
        instance->m_instantiated->m_entities.push_back(entity);
        instance->m_baseToNewEntityIdMap[restoreInfo.m_ancestorId] = entity->GetId();
        instance->m_entityIdToBaseCache.clear();
        instance->m_dataFlags.SetEntityDataFlags(entity->GetId(), restoreInfo.m_dataFlags);

        BuildEntityInfoMap();

        return SliceInstanceAddress(reference, instance);
    }

    //=========================================================================
    // SliceComponent::GetReferencedSliceAssets
    //=========================================================================
    void SliceComponent::GetReferencedSliceAssets(AssetIdSet& idSet, bool recurse)
    {
        for (auto& sliceReference : m_slices)
        {
            const Data::Asset<SliceAsset>& referencedSliceAsset = sliceReference.GetSliceAsset();
            const Data::AssetId referencedSliceAssetId = referencedSliceAsset.GetId();
            if (idSet.find(referencedSliceAssetId) == idSet.end())
            {
                idSet.insert(referencedSliceAssetId);
                if (recurse)
                {
                    referencedSliceAsset.Get()->GetComponent()->GetReferencedSliceAssets(idSet, recurse);
                }
            }
        }
    }

    //=========================================================================
    // SliceComponent::AddSliceInstance
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::AddSliceInstance(SliceReference* sliceReference, SliceInstance* sliceInstance)
    {
        if (sliceReference && sliceInstance)
        {
            // sanity check that instance belongs to slice reference
            auto findIt = AZStd::find_if(sliceReference->m_instances.begin(), sliceReference->m_instances.end(), [sliceInstance](const SliceInstance& element) -> bool { return &element == sliceInstance; });
            if (findIt == sliceReference->m_instances.end())
            {
                AZ_Error("Slice", false, "SliceInstance %p doesn't belong to SliceReference %p!", sliceInstance, sliceReference);
                return SliceInstanceAddress(nullptr, nullptr);
            }

            if (!m_slicesAreInstantiated && sliceReference->m_isInstantiated)
            {
                // if we are not instantiated, but the source sliceInstance is we need to instantiate
                // to capture any changes that might come with it.
                if (!Instantiate())
                {
                    return SliceInstanceAddress(nullptr, nullptr);
                }
            }

            SliceReference* newReference = GetSlice(sliceReference->m_asset);
            if (!newReference)
            {
                Data::AssetBus::MultiHandler::BusConnect(sliceReference->m_asset.GetId());
                m_slices.push_back();
                newReference = &m_slices.back();
                newReference->m_component = this;
                newReference->m_asset = sliceReference->m_asset;
                newReference->m_isInstantiated = m_slicesAreInstantiated;
            }

            // Move the instance to the new reference and remove it from its old owner.
            // Note: we have to preserve the Id, since it's used as a hash in storage.
            const SliceInstanceId instanceId = sliceInstance->GetId();
            sliceReference->RemoveInstanceFromEntityInfoMap(*sliceInstance);
            SliceInstance& newInstance = *newReference->m_instances.emplace(AZStd::move(*sliceInstance)).first;
            sliceInstance->SetId(instanceId);

            if (!m_entityInfoMap.empty())
            {
                newReference->AddInstanceToEntityInfoMap(newInstance);
            }

            sliceReference->RemoveInstance(sliceInstance);

            if (newReference->m_isInstantiated && !sliceReference->m_isInstantiated)
            {
                // the source instance is not instantiated, make sure we instantiate it.
                newReference->InstantiateInstance(newInstance, m_assetLoadFilterCB);
            }

            return SliceInstanceAddress(newReference, &newInstance);
        }
        return SliceInstanceAddress(nullptr, nullptr);
    }

    //=========================================================================
    // SliceComponent::RemoveSlice
    //=========================================================================
    bool SliceComponent::RemoveSlice(const Data::Asset<SliceAsset>& sliceAsset)
    {
        for (auto sliceIt = m_slices.begin(); sliceIt != m_slices.end(); ++sliceIt)
        {
            if (sliceIt->m_asset == sliceAsset)
            {
                Data::AssetBus::MultiHandler::BusDisconnect(sliceAsset.GetId());
                m_slices.erase(sliceIt);
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveSlice
    //=========================================================================
    bool SliceComponent::RemoveSlice(const SliceReference* slice)
    {
        if (slice)
        {
            return RemoveSlice(slice->m_asset);
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveSlice
    //=========================================================================
    bool SliceComponent::RemoveSliceInstance(SliceInstance* instance)
    {
        for (SliceReference& sliceReference : m_slices)
        {
            // note move this function to the slice reference for consistency
            if (sliceReference.RemoveInstance(instance))
            {
                if (sliceReference.m_instances.empty())
                {
                    RemoveSlice(&sliceReference);
                }
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::AddEntity
    //=========================================================================
    void SliceComponent::AddEntity(Entity* entity)
    {
        AZ_Assert(entity, "You passed an invalid entity!");
        m_entities.push_back(entity);

        if (!m_entityInfoMap.empty())
        {
            m_entityInfoMap.insert(AZStd::make_pair(entity->GetId(), EntityInfo(entity, SliceInstanceAddress(nullptr, nullptr))));
        }
    }

    //=========================================================================
    // SliceComponent::RemoveEntity
    //=========================================================================
    bool SliceComponent::RemoveEntity(Entity* entity, bool isDeleteEntity, bool isRemoveEmptyInstance)
    {
        if (entity)
        {
            return RemoveEntity(entity->GetId(), isDeleteEntity, isRemoveEmptyInstance);
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveEntity
    //=========================================================================
    bool SliceComponent::RemoveEntity(EntityId entityId, bool isDeleteEntity, bool isRemoveEmptyInstance)
    {
        EntityInfoMap& entityInfoMap = GetEntityInfoMap();
        auto entityInfoMapIt = entityInfoMap.find(entityId);
        if (entityInfoMapIt != entityInfoMap.end())
        {
            if (entityInfoMapIt->second.m_sliceAddress.second == nullptr)
            {
                // should be in the entity lists
                auto entityIt = AZStd::find_if(m_entities.begin(), m_entities.end(), [entityId](Entity* entity) -> bool { return entity->GetId() == entityId; });
                if (entityIt != m_entities.end())
                {
                    if (isDeleteEntity)
                    {
                        delete *entityIt;
                    }
                    entityInfoMap.erase(entityInfoMapIt);
                    m_entities.erase(entityIt);
                    return true;
                }
            }
            else
            {
                SliceReference* sliceReference = entityInfoMapIt->second.m_sliceAddress.first;
                SliceInstance* sliceInstance = entityInfoMapIt->second.m_sliceAddress.second;

                if (sliceReference->RemoveEntity(entityId, isDeleteEntity, sliceInstance))
                {
                    if (isRemoveEmptyInstance)
                    {
                        if (sliceInstance->m_instantiated->m_entities.empty())
                        {
                            RemoveSliceInstance(sliceInstance);
                        }
                    }

                    entityInfoMap.erase(entityInfoMapIt);
                    return true;
                }
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::FindEntity
    //=========================================================================
    AZ::Entity* SliceComponent::FindEntity(EntityId entityId)
    {
        const EntityInfoMap& entityInfoMap = GetEntityInfoMap();
        auto entityInfoMapIt = entityInfoMap.find(entityId);
        if (entityInfoMapIt != entityInfoMap.end())
        {
            return entityInfoMapIt->second.m_entity;
        }

        return nullptr;
    }

    //=========================================================================
    // SliceComponent::FindSlice
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::FindSlice(Entity* entity)
    {
        // if we have valid entity pointer and we are instantiated (if we have not instantiated the entities, there is no way
        // to have a pointer to it... that pointer belongs somewhere else).
        if (entity && m_slicesAreInstantiated)
        {
            return FindSlice(entity->GetId());
        }

        return SliceInstanceAddress(nullptr, nullptr);
    }

    //=========================================================================
    // SliceComponent::FindSlice
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::FindSlice(EntityId entityId)
    {
        if (entityId.IsValid())
        {
            const EntityInfoMap& entityInfo = GetEntityInfoMap();
            auto entityInfoIter = entityInfo.find(entityId);
            if (entityInfoIter != entityInfo.end())
            {
                return entityInfoIter->second.m_sliceAddress;
            }
        }

        return SliceInstanceAddress(nullptr, nullptr);
    }

    //=========================================================================
    // SliceComponent::GetEntityInfoMap
    //=========================================================================
    SliceComponent::EntityInfoMap& SliceComponent::GetEntityInfoMap()
    {
        if (m_entityInfoMap.empty())
        {
            BuildEntityInfoMap();
        }

        return m_entityInfoMap;
    }

    //=========================================================================
    // SliceComponent::ListenForAssetChanges
    //=========================================================================
    void SliceComponent::ListenForAssetChanges()
    {
        if (!m_serializeContext)
        {
            // use the default app serialize context
            EBUS_EVENT_RESULT(m_serializeContext, ComponentApplicationBus, GetSerializeContext);
            if (!m_serializeContext)
            {
                AZ_Error("Slices", false, "SliceComponent: No serialize context provided! Failed to get component application default serialize context! ComponentApp is not started or SliceComponent serialize context should not be null!");
            }
        }

        // Listen for asset events and set reference to myself
        for (SliceReference& slice : m_slices)
        {
            slice.m_component = this;
            Data::AssetBus::MultiHandler::BusConnect(slice.m_asset.GetId());
        }
    }

    //=========================================================================
    // SliceComponent::Activate
    //=========================================================================
    void SliceComponent::Activate()
    {
        ListenForAssetChanges();
    }

    //=========================================================================
    // SliceComponent::Deactivate
    //=========================================================================
    void SliceComponent::Deactivate()
    {
        Data::AssetBus::MultiHandler::BusDisconnect();
    }

    //=========================================================================
    // SliceComponent::Deactivate
    //=========================================================================
    void SliceComponent::OnAssetReloaded(Data::Asset<Data::AssetData> /*asset*/)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (!m_myAsset)
        {
            AZ_Assert(false, "Cannot reload a slice component that is not owned by an asset.");
            return;
        }

        // One of our dependent assets has changed.
        // We need to identify any dependent assets that have changed, since there may be multiple
        // due to the nature of cascaded slice reloading.
        // Because we're about to re-construct our own asset and dispatch the change notification, 
        // we need to handle all pending dependent changes.
        bool dependencyHasChanged = false;
        for (SliceReference& slice : m_slices)
        {
            Data::Asset<SliceAsset> dependentAsset = Data::AssetManager::Instance().FindAsset(slice.m_asset.GetId());

            if (slice.m_asset.Get() != dependentAsset.Get())
            {
                dependencyHasChanged = true;
                break;
            }
        }

        if (!dependencyHasChanged)
        {
            return;
        }

        // Create new SliceAsset data based on our data. We don't want to update our own
        // data in place, but instead propagate through asset reloading. Otherwise data
        // patches are not maintained properly up the slice dependency chain.
        SliceComponent* updatedAssetComponent = Clone(*m_serializeContext);
        Entity* updatedAssetEntity = aznew Entity();
        updatedAssetEntity->AddComponent(updatedAssetComponent);

        Data::Asset<SliceAsset> updatedAsset(m_myAsset->Clone());
        updatedAsset.Get()->SetData(updatedAssetEntity, updatedAssetComponent);
        updatedAssetComponent->SetMyAsset(updatedAsset.Get());
        updatedAssetComponent->ListenForAssetChanges();

        // Update data patches against the old version of the asset.
        updatedAssetComponent->PrepareSave();

        // Update asset reference for any modified dependencies, and re-instantiate nested instances.
        for (SliceReference& slice : updatedAssetComponent->m_slices)
        {
            Data::Asset<SliceAsset> dependentAsset = Data::AssetManager::Instance().FindAsset(slice.m_asset.GetId());

            if (dependentAsset.Get() == slice.m_asset.Get())
            {
                continue;
            }

            // Asset data differs. Acquire new version and re-instantiate.
            slice.m_asset = dependentAsset;

            if (slice.m_isInstantiated && !slice.m_instances.empty())
            {
                for (SliceInstance& instance : slice.m_instances)
                {
                    delete instance.m_instantiated;
                    instance.m_instantiated = nullptr;
                }
                slice.m_isInstantiated = false;

                slice.Instantiate(m_assetLoadFilterCB);
            }
        }

        // Rebuild entity info map based on new instantiations.
        if (updatedAssetComponent->m_slicesAreInstantiated)
        {
            updatedAssetComponent->BuildEntityInfoMap();
        }

        // We did not really reload our assets, but we have changed in-memory, so update our data in the DB and notify listeners.
        AZ_Assert(m_myAsset, "My asset is not set. It should be set by the SliceAssetHandler. Make sure you asset is always created and managed though the AssetDatabase and handlers!");
        Data::AssetManager::Instance().ReloadAssetFromData(updatedAsset);
    }

    //=========================================================================
    // SliceComponent::GetProvidedServices
    //=========================================================================
    void SliceComponent::GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("Prefab", 0xa60af5fc));
    }

    //=========================================================================
    // SliceComponent::GetDependentServices
    //=========================================================================
    void SliceComponent::GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
    }

    /**
    * \note when we reflecting we can check if the class is inheriting from IEventHandler
    * and use the this->events.
    */
    class SliceComponentSerializationEvents
        : public SerializeContext::IEventHandler
    {
        /// Called right before we start reading from the instance pointed by classPtr.
        void OnReadBegin(void* classPtr)
        {
            SliceComponent* component = reinterpret_cast<SliceComponent*>(classPtr);
            component->PrepareSave();
        }

        /// Called right after we finish writing data to the instance pointed at by classPtr.
        void OnWriteEnd(void* classPtr) override
        {
            EBUS_EVENT(SliceAssetSerializationNotificationBus, OnWriteDataToSliceAssetEnd, *reinterpret_cast<SliceComponent*>(classPtr));
        }
    };

    //=========================================================================
    // Reflect
    //=========================================================================
    void SliceComponent::PrepareSave()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (m_slicesAreInstantiated)
        {
            // if we had the entities instantiated, recompute the deltas.
            for (SliceReference& slice : m_slices)
            {
                slice.ComputeDataPatch();
            }
        }
    }

    //=========================================================================
    // SliceComponent::BuildEntityInfoMap
    //=========================================================================
    void SliceComponent::BuildEntityInfoMap()
    {
        m_entityInfoMap.clear();

        for (Entity* entity : m_entities)
        {
            m_entityInfoMap.insert(AZStd::make_pair(entity->GetId(), EntityInfo(entity, SliceInstanceAddress(nullptr, nullptr))));
        }

        for (SliceReference& slice : m_slices)
        {
            for (SliceInstance& instance : slice.m_instances)
            {
                slice.AddInstanceToEntityInfoMap(instance);
            }
        }
    }

    //=========================================================================
    // ApplyEntityMapId
    //=========================================================================
    void SliceComponent::ApplyEntityMapId(EntityIdToEntityIdMap& destination, const EntityIdToEntityIdMap& remap)
    {
        // apply the instance entityIdMap on the top of the base
        for (const auto& entityIdPair : remap)
        {
            auto iteratorBoolPair = destination.insert(entityIdPair);
            if (!iteratorBoolPair.second)
            {
                // if not inserted just overwrite the value
                iteratorBoolPair.first->second = entityIdPair.second;
            }
        }
    }

    //=========================================================================
    // SliceComponent::AddOrGetSliceReference
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::AddOrGetSliceReference(const Data::Asset<SliceAsset>& sliceAsset)
    {
        SliceReference* reference = GetSlice(sliceAsset.GetId());
        if (!reference)
        {
            Data::AssetBus::MultiHandler::BusConnect(sliceAsset.GetId());
            m_slices.push_back();
            reference = &m_slices.back();
            reference->m_component = this;
            reference->m_asset = sliceAsset;
            reference->m_isInstantiated = m_slicesAreInstantiated;
        }

        return reference;
    }

    //=========================================================================
    // Clone
    //=========================================================================
    SliceComponent* SliceComponent::Clone(AZ::SerializeContext& serializeContext, SliceInstanceToSliceInstanceMap* sourceToCloneSliceInstanceMap) const
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        SliceComponent* clonedComponent = serializeContext.CloneObject(this);

        if (!clonedComponent)
        {
            AZ_Error("SliceAsset", false, "Failed to clone asset.");
            return nullptr;
        }

        AZ_Assert(clonedComponent, "Cloned asset doesn't contain a slice component.");
        AZ_Assert(clonedComponent->GetSlices().size() == GetSlices().size(),
            "Cloned asset does not match source asset.");

        const SliceComponent::SliceList& myReferences = m_slices;
        SliceComponent::SliceList& clonedReferences = clonedComponent->m_slices;

        auto myReferencesIt = myReferences.begin();
        auto clonedReferencesIt = clonedReferences.begin();

        // For all slice references, clone instantiated instances.
        for (; myReferencesIt != myReferences.end(); ++myReferencesIt, ++clonedReferencesIt)
        {
            const SliceComponent::SliceReference::SliceInstances& myRefInstances = myReferencesIt->m_instances;
            SliceComponent::SliceReference::SliceInstances& clonedRefInstances = clonedReferencesIt->m_instances;

            AZ_Assert(myRefInstances.size() == clonedRefInstances.size(),
                "Cloned asset reference does not contain the same number of instances as the source asset reference.");

            auto myRefInstancesIt = myRefInstances.begin();
            auto clonedRefInstancesIt = clonedRefInstances.begin();

            for (; myRefInstancesIt != myRefInstances.end(); ++myRefInstancesIt, ++clonedRefInstancesIt)
            {
                const SliceComponent::SliceInstance& myRefInstance = (*myRefInstancesIt);
                SliceComponent::SliceInstance& clonedRefInstance = (*clonedRefInstancesIt);

                if (sourceToCloneSliceInstanceMap)
                {
                    SliceComponent::SliceInstanceAddress sourceAddress(const_cast<SliceComponent::SliceReference*>(&(*myReferencesIt)), const_cast<SliceComponent::SliceInstance*>(&myRefInstance));
                    SliceComponent::SliceInstanceAddress clonedAddress(&(*clonedReferencesIt), &clonedRefInstance);
                    (*sourceToCloneSliceInstanceMap)[sourceAddress] = clonedAddress;
                }

                // Slice instances should not support copying natively, but to clone we copy member-wise
                // and clone instantiated entities.
                clonedRefInstance.m_baseToNewEntityIdMap = myRefInstance.m_baseToNewEntityIdMap;
                clonedRefInstance.m_entityIdToBaseCache = myRefInstance.m_entityIdToBaseCache;
                clonedRefInstance.m_dataPatch = myRefInstance.m_dataPatch;
                clonedRefInstance.m_dataFlags.CopyDataFlagsFrom(myRefInstance.m_dataFlags);
                clonedRefInstance.m_instantiated = serializeContext.CloneObject(myRefInstance.m_instantiated);
            }

            clonedReferencesIt->m_isInstantiated = myReferencesIt->m_isInstantiated;
            clonedReferencesIt->m_asset = myReferencesIt->m_asset;
            clonedReferencesIt->m_component = clonedComponent;
        }

        // Finally, mark the cloned asset as instantiated.
        clonedComponent->m_slicesAreInstantiated = IsInstantiated();

        return clonedComponent;
    }

    //=========================================================================
    // Reflect
    //=========================================================================
    void SliceComponent::Reflect(ReflectContext* reflection)
    {
        DataFlagsPerEntity::Reflect(reflection);

        SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(reflection);
        if (serializeContext)
        {
            (void)serializeContext;
            serializeContext->Class<SliceComponent, Component>()->
                Version(1)->
                EventHandler<SliceComponentSerializationEvents>()->
                Field("Entities", &SliceComponent::m_entities)->
                Field("Prefabs", &SliceComponent::m_slices)->
                Field("IsDynamic", &SliceComponent::m_isDynamic);

            serializeContext->Class<InstantiatedContainer>()->
                Version(1)->
                Field("Entities", &InstantiatedContainer::m_entities);

            serializeContext->Class<SliceInstance>()->
                Version(3)->
                Field("Id", &SliceInstance::m_instanceId)->
                Field("EntityIdMap", &SliceInstance::m_baseToNewEntityIdMap)->
                Field("DataPatch", &SliceInstance::m_dataPatch)->
                Field("DataFlags", &SliceInstance::m_dataFlags); // added at v3

            serializeContext->Class<SliceReference>()->
                Version(2, &Converters::SliceReferenceVersionConverter)->
                Field("Instances", &SliceReference::m_instances)->
                Field("Asset", &SliceReference::m_asset);

            serializeContext->Class<EntityRestoreInfo>()->
                Version(1)->
                Field("AssetId", &EntityRestoreInfo::m_assetId)->
                Field("InstanceId", &EntityRestoreInfo::m_instanceId)->
                Field("AncestorId", &EntityRestoreInfo::m_ancestorId)->
                Field("DataFlags", &EntityRestoreInfo::m_dataFlags); // added at v1
        }
    }
} // namespace AZ
