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

// The Tools Framework AssetProcessorMessages header is for all of the asset processor messages that should only
// be available to tools, not the runtime.

#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzCore/Asset/AssetCommon.h>

namespace AZ
{
    class SerializeContext;
}

namespace AzToolsFramework
{
    namespace AssetSystem
    {
        //!  Request the jobs information for a given asset from the AssetProcessor
        class AssetJobsInfoRequest
            : public AzFramework::AssetSystem::BaseAssetProcessorMessage
        {
        public:
            AZ_CLASS_ALLOCATOR(AssetJobsInfoRequest, AZ::OSAllocator, 0);
            AZ_RTTI(AssetJobsInfoRequest, "{E5DEF45C-C4CF-47ED-843F-97B3C4A3D5B3}", AzFramework::AssetSystem::BaseAssetProcessorMessage);
            static void Reflect(AZ::ReflectContext* context);
            static unsigned int MessageType();
            bool RequireFencing() override;

            AssetJobsInfoRequest() = default;
            AssetJobsInfoRequest(const AZ::OSString& searchTerm);
            unsigned int GetMessageType() const override;

            AZ::OSString m_searchTerm;
            AZ::Data::AssetId m_assetId;
            bool m_isSearchTermJobKey = false;
            bool m_escalateJobs = true;
        };

        //! This will be send in response to the AssetJobsInfoRequest request,
        //! and will contain jobs information for the requested asset along with the jobid
        class AssetJobsInfoResponse
            : public AzFramework::AssetSystem::BaseAssetProcessorMessage
        {
        public:
            AZ_CLASS_ALLOCATOR(AssetJobsInfoResponse, AZ::OSAllocator, 0);
            AZ_RTTI(AssetJobsInfoResponse, "{743AFB3B-F24C-4546-BEEC-2769442B52DB}", AzFramework::AssetSystem::BaseAssetProcessorMessage);
            static void Reflect(AZ::ReflectContext* context);

            AssetJobsInfoResponse() = default;
            AssetJobsInfoResponse(AssetSystem::JobInfoContainer& jobList, bool isSuccess);
            unsigned int GetMessageType() const override;
            bool m_isSuccess = false;
            AssetSystem::JobInfoContainer m_jobList;
        };

        //!  Request the log data for a given jobId from the AssetProcessor
        class AssetJobLogRequest
            : public AzFramework::AssetSystem::BaseAssetProcessorMessage
        {
        public:
            AZ_CLASS_ALLOCATOR(AssetJobLogRequest, AZ::OSAllocator, 0);
            AZ_RTTI(AssetJobLogRequest, "{8E69F76E-F25D-486E-BC3F-26BB3FF5A3A3}", AzFramework::AssetSystem::BaseAssetProcessorMessage);
            static void Reflect(AZ::ReflectContext* context);
            static unsigned int MessageType();
            bool RequireFencing() override;

            AssetJobLogRequest() = default;
            AssetJobLogRequest(AZ::s64 jobRunKey);
            unsigned int GetMessageType() const override;

            AZ::u64 m_jobRunKey;
        };

        //! This will be sent in response to the AssetJobLogRequest request, and will contain the complete job log as a string
        class AssetJobLogResponse
            : public AzFramework::AssetSystem::BaseAssetProcessorMessage
        {
        public:
            AZ_CLASS_ALLOCATOR(AssetJobLogResponse, AZ::OSAllocator, 0);
            AZ_RTTI(AssetJobLogResponse, "{4CBB55AB-24E3-4A7A-ACB7-54069289AF2C}", AzFramework::AssetSystem::BaseAssetProcessorMessage);
            static void Reflect(AZ::ReflectContext* context);

            AssetJobLogResponse() = default;
            AssetJobLogResponse(const AZStd::string& jobLog, bool isSuccess);
            unsigned int GetMessageType() const override;

            bool m_isSuccess = false;
            AZStd::string m_jobLog;
        };

        //! Tools side message that a source file has changed or been removed
        class SourceFileNotificationMessage
            : public AzFramework::AssetSystem::BaseAssetProcessorMessage
        {
        public:
            enum NotificationType : unsigned int
            {
                FileChanged,
                FileRemoved,
                FileFailed,
            };

            AZ_CLASS_ALLOCATOR(SourceFileNotificationMessage, AZ::OSAllocator, 0);
            AZ_RTTI(SourceFileNotificationMessage, "{61126952-242A-4299-B1D6-4D0E24DB1B06}", AzFramework::AssetSystem::BaseAssetProcessorMessage);
            static void Reflect(AZ::ReflectContext* context);
            static unsigned int MessageType();

            SourceFileNotificationMessage() = default;
            SourceFileNotificationMessage(const AZ::OSString& relPath, const AZ::OSString& scanFolder, NotificationType type, AZ::Uuid sourceUUID);
            unsigned int GetMessageType() const override;

            AZ::OSString m_relativeSourcePath;
            AZ::OSString m_scanFolder;
            AZ::Uuid m_sourceUUID;
            NotificationType m_type;
        };
    } // namespace AssetSystem
} // namespace AzToolsFramework
