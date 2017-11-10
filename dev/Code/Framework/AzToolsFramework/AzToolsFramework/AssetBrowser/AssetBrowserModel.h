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
#pragma once

#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

#include <QAbstractTableModel>
#include <QVariant>
#include <QMimeData>

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        class AssetBrowserEntry;
        class RootAssetBrowserEntry;
        class AssetEntryChangeset;

        class AssetBrowserModel
            : public QAbstractTableModel
            , public AssetBrowserModelRequestsBus::Handler
        {
            Q_OBJECT
        public:
            AZ_CLASS_ALLOCATOR(AssetBrowserModel, AZ::SystemAllocator, 0);

            explicit AssetBrowserModel(QObject* parent = nullptr);
            ~AssetBrowserModel();

            //////////////////////////////////////////////////////////////////////////
            // QAbstractTableModel
            //////////////////////////////////////////////////////////////////////////
            QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
            int rowCount(const QModelIndex& parent = QModelIndex()) const override;
            int columnCount(const QModelIndex& parent = QModelIndex()) const override;
            QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
            Qt::ItemFlags flags(const QModelIndex& index) const override;
            QMimeData* mimeData(const QModelIndexList& indexes) const override;
            QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
            QModelIndex parent(const QModelIndex& child) const override;

            //////////////////////////////////////////////////////////////////////////
            // AssetBrowserModelRequestsBus
            //////////////////////////////////////////////////////////////////////////
            bool IsLoaded() const override;
            void BeginAddEntry(AssetBrowserEntry* parent) override;
            void EndAddEntry() override;
            void BeginRemoveEntry(AssetBrowserEntry* entry) override;
            void EndRemoveEntry() override;

            AZStd::shared_ptr<RootAssetBrowserEntry> GetRootEntry() const;
            void SetRootEntry(AZStd::shared_ptr<RootAssetBrowserEntry> rootEntry);

            static void SourceIndexesToAssetIds(const QModelIndexList& indexes, AZStd::vector<AZ::Data::AssetId>& assetIds);
            static void SourceIndexesToAssetDatabaseEntries(const QModelIndexList& indexes, AZStd::vector<AssetBrowserEntry*>& entries);
            
            const static int m_column;

        private:
            AZStd::shared_ptr<RootAssetBrowserEntry> m_rootEntry;
            bool m_loaded;
            bool m_addingEntry;
            bool m_removingEntry;

            bool GetEntryIndex(AssetBrowserEntry* entry, QModelIndex& index) const;
        };
    } // namespace AssetBrowser
} // namespace AzToolsFramework
