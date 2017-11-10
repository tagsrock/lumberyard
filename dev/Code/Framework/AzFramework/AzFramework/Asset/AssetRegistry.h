
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

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/unordered_map.h>

#pragma once

namespace AzFramework
{
    /**
    * Data storage for asset registry.
    * Maintained separate to facilitate easy serialization to/from disk.
    */
    class AssetRegistry
    {
    public:
        AZ_TYPE_INFO(AssetRegistry, "{5DBC20D9-7143-48B3-ADEE-CCBD2FA6D443}");
        AZ_CLASS_ALLOCATOR(AssetRegistry, AZ::SystemAllocator, 0);

        AssetRegistry() = default;

        void RegisterAsset(AZ::Data::AssetId id, const AZ::Data::AssetInfo& assetInfo);
        void UnregisterAsset(AZ::Data::AssetId id);

        void RegisterLegacyAssetMapping(const AZ::Data::AssetId& legacyId, const AZ::Data::AssetId& newId);
        void UnregisterLegacyAssetMapping(const AZ::Data::AssetId& legacyId);

        //! LEGACY - do not use in new code unless interfacing with legacy systems.  
        //! All new systems should be referring to assets by ID/Type only and should not need to look up by path/
        AZ::Data::AssetId GetAssetIdByPath(const char* assetPath) const;

        using AssetIdToInfoMap = AZStd::unordered_map < AZ::Data::AssetId, AZ::Data::AssetInfo >;
        AssetIdToInfoMap m_assetIdToInfo;

        void Clear();

        // see if the asset ID has been remapped to a new Id:
        AZ::Data::AssetId GetAssetIdByLegacyAssetId(const AZ::Data::AssetId& legacyAssetId);

        static void ReflectSerialize(AZ::SerializeContext* serializeContext);

    private:
        // use these only through the legacy getters/setters above.
        using AssetPathToIdMap = AZStd::unordered_map < AZ::Uuid, AZ::Data::AssetId >;
        using LegacyAssetIdToRealAssetIdMap = AZStd::unordered_map<AZ::Data::AssetId, AZ::Data::AssetId>;
        
        AssetPathToIdMap m_assetPathToId; // for legacy lookups only
        LegacyAssetIdToRealAssetIdMap m_legacyAssetIdToRealAssetId; // for when we change the UUID-creation scheme
        
        //! LEGACY - do not use in new code unless interfacing with legacy systems.
        //! given an assetPath and AssetID, this stores it in the registry to use with the above GetAssetIdByPath function.
        //! Called automatically by RegisterAsset.
        void SetAssetIdByPath(const char* assetPath, const AZ::Data::AssetId& id);

    };

} // namespace AzFramework
