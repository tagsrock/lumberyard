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

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/Component/Component.h>
#include <AzFramework/Network/SocketConnection.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

namespace AzToolsFramework
{
    namespace AssetSystem
    {
        /**
        * A tools level component for interacting with the asset processor
        *
        * Currently used to translate between full and relative asset paths, 
        * and to query information about asset processor jobs
        */
        class AssetSystemComponent
            : public AZ::Component
            , private AzToolsFramework::AssetSystemRequestBus::Handler
            , private AzToolsFramework::AssetSystemJobRequestBus::Handler
        {
        public:
            AZ_COMPONENT(AssetSystemComponent, "{B1352D59-945B-446A-A7E1-B2D3EB717C6D}")

            AssetSystemComponent() = default;
            virtual ~AssetSystemComponent() = default;

            //////////////////////////////////////////////////////////////////////////
            // AZ::Component overrides
            void Init() override {}
            void Activate() override;
            void Deactivate() override;
            //////////////////////////////////////////////////////////////////////////

            static void Reflect(AZ::ReflectContext* context);
            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        private:

            //////////////////////////////////////////////////////////////////////////
            // AzToolsFramework::AssetSystemRequestBus::Handler overrides
            const char* GetAbsoluteDevGameFolderPath() override;
            const char* GetAbsoluteDevRootFolderPath() override;
            bool GetRelativeProductPathFromFullSourceOrProductPath(const AZStd::string& fullPath, AZStd::string& outputPath) override;
            bool GetFullSourcePathFromRelativeProductPath(const AZStd::string& relPath, AZStd::string& fullPath) override;
            void UpdateQueuedEvents() override;
            virtual bool GetSourceAssetInfoById(const AZ::Uuid& guid, AZStd::string& watchFolder, AZStd::string& relativePath) override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // AzToolsFramework::AssetSystemJobRequest::Bus::Handler overrides
            virtual AZ::Outcome<AssetSystem::JobInfoContainer> GetAssetJobsInfo(const AZStd::string& path, const bool escalateJobs) override;
            virtual AZ::Outcome<JobInfoContainer> GetAssetJobsInfoByAssetID(const AZ::Data::AssetId& assetId, const bool escalateJobs) override;
            virtual AZ::Outcome<JobInfoContainer> GetAssetJobsInfoByJobKey(const AZStd::string& jobKey, const bool escalateJobs) override;
            virtual AZ::Outcome<JobStatus> GetAssetJobsStatusByJobKey(const AZStd::string& jobKey, const bool escalateJobs) override;
            virtual AZ::Outcome<AZStd::string> GetJobLog(AZ::u64 jobrunkey) override;
            //////////////////////////////////////////////////////////////////////////

            AzFramework::SocketConnection::TMessageCallbackHandle m_cbHandle = 0;
        };


    } // namespace AssetSystem
} // namespace AzToolsFramework

