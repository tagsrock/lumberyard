/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <SliceBuilder/Source/SliceBuilderComponent.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <LyShine/UiComponentTypes.h>
#include <LyShine/UiAssetTypes.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

namespace SliceBuilder
{
    void BuilderPluginComponent::Activate()
    {
        // Register Slice Builder
        AssetBuilderSDK::AssetBuilderDesc builderDescriptor;
        builderDescriptor.m_name = "Slice Builder";
        builderDescriptor.m_version = 1;
        builderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.slice", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
        builderDescriptor.m_busId = SliceBuilderWorker::GetUUID();
        builderDescriptor.m_createJobFunction = AZStd::bind(&SliceBuilderWorker::CreateJobs, &m_sliceBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDescriptor.m_processJobFunction = AZStd::bind(&SliceBuilderWorker::ProcessJob, &m_sliceBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        m_sliceBuilder.BusConnect(builderDescriptor.m_busId);

        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Handler::RegisterBuilderInformation, builderDescriptor);

        // Register UI Slice Builder
        AssetBuilderSDK::AssetBuilderDesc uiBuilderDescriptor;
        uiBuilderDescriptor.m_name = "UI Slice Builder";
        uiBuilderDescriptor.m_version = 1;
        uiBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.uicanvas", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
        uiBuilderDescriptor.m_busId = UiSliceBuilderWorker::GetUUID();
        uiBuilderDescriptor.m_createJobFunction = AZStd::bind(&UiSliceBuilderWorker::CreateJobs, &m_uiSliceBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        uiBuilderDescriptor.m_processJobFunction = AZStd::bind(&UiSliceBuilderWorker::ProcessJob, &m_uiSliceBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        m_uiSliceBuilder.BusConnect(uiBuilderDescriptor.m_busId);

        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBus::Handler::RegisterBuilderInformation, uiBuilderDescriptor);

        AssetBuilderSDK::ToolsAssetSystemBus::Broadcast(&AssetBuilderSDK::ToolsAssetSystemBus::Handler::RegisterSourceAssetType, azrtti_typeid<AZ::SliceAsset>(), AZ::SliceAsset::GetFileFilter());
        AssetBuilderSDK::ToolsAssetSystemBus::Broadcast(&AssetBuilderSDK::ToolsAssetSystemBus::Handler::RegisterSourceAssetType, azrtti_typeid<LyShine::CanvasAsset>(), LyShine::CanvasAsset::GetFileFilter());
    }

    void BuilderPluginComponent::Deactivate()
    {
        // Finish all queued work
        AZ::Data::AssetBus::ExecuteQueuedEvents();

        AssetBuilderSDK::ToolsAssetSystemBus::Broadcast(&AssetBuilderSDK::ToolsAssetSystemBus::Handler::UnregisterSourceAssetType, azrtti_typeid<AZ::SliceAsset>());
        AssetBuilderSDK::ToolsAssetSystemBus::Broadcast(&AssetBuilderSDK::ToolsAssetSystemBus::Handler::UnregisterSourceAssetType, azrtti_typeid<LyShine::CanvasAsset>());

        m_uiSliceBuilder.BusDisconnect();
        m_sliceBuilder.BusDisconnect();
    }

    void BuilderPluginComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BuilderPluginComponent, AZ::Component>()
                ->Version(1)
                ;
        }
    }
}
