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

#include <AzToolsFramework/AssetBrowser/AssetSelectionModel.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>
#include <AzToolsFramework/AssetBrowser/EBusFindAssetTypeByName.h>

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        namespace
        {
            FilterConstType ProductsNoFoldersFilter()
            {
                EntryTypeFilter* productFilter = new EntryTypeFilter();
                productFilter->SetEntryType(AssetBrowserEntry::AssetEntryType::Product);
                // in case entry is a source or folder, it may still contain relevant product
                productFilter->SetFilterPropagation(AssetBrowserEntryFilter::PropagateDirection::Down);

                EntryTypeFilter* foldersFilter = new EntryTypeFilter();
                foldersFilter->SetEntryType(AssetBrowserEntry::AssetEntryType::Folder);

                InverseFilter* noFoldersFilter = new InverseFilter();
                noFoldersFilter->SetFilter(FilterConstType(foldersFilter));

                CompositeFilter* compFilter = new CompositeFilter(CompositeFilter::LogicOperatorType::AND);
                compFilter->AddFilter(FilterConstType(productFilter));
                compFilter->AddFilter(FilterConstType(noFoldersFilter));

                return FilterConstType(compFilter);
            }
        }

        AssetSelectionModel::AssetSelectionModel()
            : m_multiselect(false)
        {
        }

        FilterConstType AssetSelectionModel::GetSelectionFilter() const
        {
            return m_selectionFilter;
        }

        void AssetSelectionModel::SetSelectionFilter(FilterConstType filter)
        {
            m_selectionFilter = filter;
        }

        FilterConstType AssetSelectionModel::GetDisplayFilter() const
        {
            return m_displayFilter;
        }

        void AssetSelectionModel::SetDisplayFilter(FilterConstType filter)
        {
            m_displayFilter = filter;
        }

        bool AssetSelectionModel::GetMultiselect() const
        {
            return m_multiselect;
        }

        void AssetSelectionModel::SetMultiselect(bool multiselect)
        {
            m_multiselect = multiselect;
        }

        const AZStd::vector<AZ::Data::AssetId>& AssetSelectionModel::GetSelectedAssetIds() const
        {
            return m_selectedAssetIds;
        }

        void AssetSelectionModel::SetSelectedAssetIds(const AZStd::vector<AZ::Data::AssetId>& selectedAssetIds)
        {
            m_selectedAssetIds = selectedAssetIds;
        }

        void AssetSelectionModel::SetSelectedAssetId(const AZ::Data::AssetId& selectedAssetId)
        {
            m_selectedAssetIds.clear();
            m_selectedAssetIds.push_back(selectedAssetId);
        }

        AZStd::vector<const AssetBrowserEntry*>& AssetSelectionModel::GetResults()
        {
            return m_results;
        }

        const AssetBrowserEntry* AssetSelectionModel::GetResult()
        {
            return m_results.front();
        }

        bool AssetSelectionModel::IsValid() const
        {
            return !m_results.empty();
        }

        AssetSelectionModel AssetSelectionModel::AssetTypeSelection(AZ::Data::AssetType assetType, bool multiselect)
        {
            AssetSelectionModel selection;

            AssetTypeFilter* assetTypeFilter = new AssetTypeFilter();
            assetTypeFilter->SetAssetType(assetType);
            assetTypeFilter->SetFilterPropagation(AssetBrowserEntryFilter::PropagateDirection::Down);
            auto assetTypeFilterPtr = FilterConstType(assetTypeFilter);

            selection.SetDisplayFilter(assetTypeFilterPtr);

            CompositeFilter* compFilter = new CompositeFilter(CompositeFilter::LogicOperatorType::AND);
            compFilter->AddFilter(assetTypeFilterPtr);
            compFilter->AddFilter(ProductsNoFoldersFilter());

            selection.SetSelectionFilter(FilterConstType(compFilter));
            selection.SetMultiselect(multiselect);

            return selection;
        }

        AssetSelectionModel AssetSelectionModel::AssetTypeSelection(const char* assetTypeName, bool multiselect)
        {
            EBusFindAssetTypeByName result(assetTypeName);
            AZ::AssetTypeInfoBus::BroadcastResult(result, &AZ::AssetTypeInfo::GetAssetType);
            return AssetTypeSelection(result.GetAssetType(), multiselect);
        }

        AssetSelectionModel AssetSelectionModel::AssetGroupSelection(const char* group, bool multiselect)
        {
            AssetSelectionModel selection;

            AssetGroupFilter* assetGroupFilter = new AssetGroupFilter();
            assetGroupFilter->SetAssetGroup(group);
            assetGroupFilter->SetFilterPropagation(AssetBrowserEntryFilter::PropagateDirection::Down);

            selection.SetDisplayFilter(FilterConstType(assetGroupFilter));
            selection.SetSelectionFilter(ProductsNoFoldersFilter());
            selection.SetMultiselect(multiselect);

            return selection;
        }

        AssetSelectionModel AssetSelectionModel::EverythingSelection(bool multiselect) 
        {
            AssetSelectionModel selection;
         
            CompositeFilter* compFilter = new CompositeFilter(CompositeFilter::LogicOperatorType::OR);
            selection.SetDisplayFilter(FilterConstType(compFilter));
            selection.SetSelectionFilter(ProductsNoFoldersFilter());
            selection.SetMultiselect(multiselect);

            return selection;
        }
    } // namespace AssetBrowser
} // namespace AzToolsFramework.