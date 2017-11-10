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

#include <AzToolsFramework/AssetDatabase/AssetDatabaseConnection.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserComponent.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserModel.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserThumbnailer.h>
#include <AzToolsFramework/AssetBrowser/AssetEntryChangeset.h>

#include <chrono>

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        AssetBrowserComponent::AssetBrowserComponent()
            : m_databaseConnection(aznew AssetDatabase::AssetDatabaseConnection())
            , m_rootEntry(aznew RootAssetBrowserEntry())
            , m_thumbnailProvider(aznew AssetBrowserThumbnailer())
            , m_dbReady(false)
            , m_waitingForMore(false)
            , m_disposed(false)
            , m_assetBrowserModel(aznew AssetBrowserModel())
            , m_changeset(new AssetEntryChangeset(m_databaseConnection, m_rootEntry))
        {
            m_assetBrowserModel->SetRootEntry(m_rootEntry);
        }

        AssetBrowserComponent::~AssetBrowserComponent() {}

        void AssetBrowserComponent::Activate()
        {
            m_disposed = false;
            m_waitingForMore = false;
            m_thread = AZStd::thread(AZStd::bind(&AssetBrowserComponent::UpdateAssets, this));

            AssetBrowserComponentRequestsBus::Handler::BusConnect();
            AzFramework::AssetCatalogEventBus::Handler::BusConnect();
            AZ::TickBus::Handler::BusConnect();
            AssetSystemBus::Handler::BusConnect();
            m_thumbnailProvider->BusConnect();
        }

        void AssetBrowserComponent::Deactivate()
        {
            m_disposed = true;
            NotifyUpdateThread();
            if (m_thread.joinable())
            {
                m_thread.join(); // wait for the thread to finish
                m_thread = AZStd::thread(); // destroy
            }
            AssetBrowserComponentRequestsBus::Handler::BusDisconnect();
            AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
            AZ::TickBus::Handler::BusDisconnect();
            AssetSystemBus::Handler::BusDisconnect();
            m_thumbnailProvider->BusDisconnect();
        }

        void AssetBrowserComponent::Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
            if (serialize)
            {
                serialize->Class<AssetBrowserComponent, AZ::Component>();
            }
        }

        void AssetBrowserComponent::DatabaseInitialized()
        {
            m_databaseConnection->OpenDatabase();
            PopulateAssets();
            m_dbReady = true;
        }

        AssetBrowserModel* AssetBrowserComponent::GetAssetBrowserModel()
        {
            return m_assetBrowserModel.get();
        }

        void AssetBrowserComponent::OnTick(float /*deltaTime*/, AZ::ScriptTimePoint /*time*/)
        {
            m_changeset->Synchronize();
        }

        void AssetBrowserComponent::SourceFileRemoved(AZStd::string /*assetId*/, AZStd::string /*scanFolder*/, AZ::Uuid sourceUUID)
        {
            m_changeset->RemoveSource(sourceUUID);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::OnCatalogAssetAdded(const AZ::Data::AssetId& assetId)
        {
            m_changeset->AddEntry(assetId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::OnCatalogAssetRemoved(const AZ::Data::AssetId& assetId)
        {
            m_changeset->RemoveEntry(assetId);
            if (m_dbReady)
            {
                NotifyUpdateThread();
            }
        }

        void AssetBrowserComponent::PopulateAssets()
        {
            m_changeset->PopulateEntries();
            NotifyUpdateThread();
        }

        void AssetBrowserComponent::UpdateAssets()
        {
            while (true)
            {
                m_updateWait.acquire();

                // kill thread if component is destroyed
                if (m_disposed)
                {
                    return;
                }

                // wait for db or more updates
                m_waitingForMore = true;
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100));
                m_waitingForMore = false;

                m_changeset->Update();
            }
        }

        void AssetBrowserComponent::NotifyUpdateThread()
        {
            // do not release the sempahore again if query thread is waiting for more update requests
            // otherwise it would needlessly spin another turn
            if (!m_waitingForMore)
            {
                m_updateWait.release();
            }
        }
    } // namespace AssetBrowser
} // namespace AzToolsFramework
#include <AssetBrowser/AssetBrowserComponent.moc>
