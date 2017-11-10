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

#include <SliceBuilder/Source/UiSliceBuilderWorker.h>
#include <SliceBuilder/Source/TraceDrillerHook.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/Slice/SliceAssetHandler.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/Utils.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzCore/Component/ComponentApplication.h>
#include <LyShine/Bus/Tools/UiSystemToolsBus.h>
#include <LyShine/UiAssetTypes.h>

namespace SliceBuilder
{
    static const char* const s_uiSliceBuilder = "UiSliceBuilder";

    void UiSliceBuilderWorker::ShutDown()
    {
        m_isShuttingDown = true;
    }

    AZ::Uuid UiSliceBuilderWorker::GetUUID()
    {
        return AZ::Uuid::CreateString("{2708874f-52e8-48db-bbc4-4c33fa8ceb2e}");
    }

    void UiSliceBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        TraceDrillerHook traceDrillerHook(true);

        AZStd::string fullPath;
        AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), fullPath, false);
        AzFramework::StringFunc::Path::Normalize(fullPath);

        AZ_TracePrintf(s_uiSliceBuilder, "CreateJobs for UI canvas \"%s\"", fullPath.c_str());

        // Open the source canvas file
        AZ::IO::FileIOStream stream(fullPath.c_str(), AZ::IO::OpenMode::ModeRead);
        if (!stream.IsOpen())
        {
            AZ_Warning(s_uiSliceBuilder, false, "CreateJobs for \"%s\" failed because the source file could not be opened.", fullPath.c_str());
            return;
        }

        // Asset filter always returns false to prevent parsing dependencies, but makes note of the slice dependencies
        auto assetFilter = [&response](const AZ::Data::Asset<AZ::Data::AssetData>& asset)
        {
            if (asset.GetType() == AZ::AzTypeInfo<AZ::SliceAsset>::Uuid())
            {
                bool isSliceDependency = (0 == (asset.GetFlags() & static_cast<AZ::u8>(AZ::Data::AssetFlags::OBJECTSTREAM_NO_LOAD)));

                if (isSliceDependency)
                {
                    AssetBuilderSDK::SourceFileDependency dependency;
                    dependency.m_sourceFileDependencyUUID = asset.GetId().m_guid;

                    response.m_sourceFileDependencyList.push_back(dependency);
                }
            }

            return false;
        };

        // Serialize in the canvas from the stream. This uses the LyShineSystemComponent to do it because
        // it does some complex support for old canvas formats
        UiSystemToolsInterface::CanvasAssetHandle* canvasAsset = nullptr;
        UiSystemToolsBus::BroadcastResult(canvasAsset, &UiSystemToolsInterface::LoadCanvasFromStream, stream, AZ::ObjectStream::FilterDescriptor(assetFilter));
        if (!canvasAsset)
        {
            AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed to load canvas from stream.", fullPath.c_str());
            return;
        }

        // Flush asset database events to ensure no asset references are held by closures queued on Ebuses.
        AZ::Data::AssetManager::Instance().DispatchEvents();

        // Fail gracefully if any errors occurred while serializing in the editor UI canvas.
        // i.e. missing assets or serialization errors.
        if (traceDrillerHook.GetErrorCount() > 0)
        {
            AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed due to errors loading editor UI canvas.", fullPath.c_str());
            UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
            return;
        }

        const char* compilerVersion = "3";
        const size_t platformCount = request.GetEnabledPlatformsCount();

        for (size_t i = 0; i < platformCount; ++i)
        {
            AssetBuilderSDK::JobDescriptor jobDescriptor;
            jobDescriptor.m_priority = 0;
            jobDescriptor.m_critical = true;
            jobDescriptor.m_jobKey = "RC Slice";
            jobDescriptor.m_platform = request.GetEnabledPlatformAt(i);
            jobDescriptor.m_additionalFingerprintInfo = AZStd::string(compilerVersion).append(azrtti_typeid<AZ::DynamicSliceAsset>().ToString<AZStd::string>());

            response.m_createJobOutputs.push_back(jobDescriptor);
        }

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;

        UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
    }

    void UiSliceBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const
    {
        // .uicanvas files are converted as they are copied to the cache
        // a) to flatten all prefab instances
        // b) to replace any editor components with runtime components

        TraceDrillerHook traceDrillerHook(true);

        AZStd::string fullPath;
        AZStd::string fileNameOnly;
        AZStd::string outputPath;
        AzFramework::StringFunc::Path::GetFullFileName(request.m_sourceFile.c_str(), fileNameOnly);
        AzFramework::StringFunc::Path::Join(request.m_tempDirPath.c_str(), fileNameOnly.c_str(), outputPath, true, true, true);
        AzFramework::StringFunc::Path::ConstructFull(request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), fullPath, false);
        AzFramework::StringFunc::Path::Normalize(fullPath);

        AZ_TracePrintf(s_uiSliceBuilder, "Processing UI canvas \"%s\"", fullPath.c_str());

        // Open the source canvas file
        AZ::IO::FileIOStream stream(fullPath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);
        if (!stream.IsOpen())
        {
            AZ_Warning(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed because source file could not be opened.", fullPath.c_str());
            return;
        }

        // Serialize in the canvas from the stream. This uses the LyShineSystemComponent to do it because
        // it does some complex support for old canvas formats
        UiSystemToolsInterface::CanvasAssetHandle* canvasAsset = nullptr;
        UiSystemToolsBus::BroadcastResult(canvasAsset, &UiSystemToolsInterface::LoadCanvasFromStream, stream,
            AZ::ObjectStream::FilterDescriptor(AZ::ObjectStream::AssetFilterSlicesOnly));
        if (!canvasAsset)
        {
            AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed to load canvas from stream.", fullPath.c_str());
            return;
        }

        // Flush asset manager events to ensure no asset references are held by closures queued on Ebuses.
        AZ::Data::AssetManager::Instance().DispatchEvents();

        // Fail gracefully if any errors occurred while serializing in the editor UI canvas.
        // i.e. missing assets or serialization errors.
        if (traceDrillerHook.GetErrorCount() > 0)
        {
            AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed due to errors loading editor UI canvas.", fullPath.c_str());
            UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
            return;
        }

        // Get the prefab component from the canvas
        AZ::Entity* canvasSliceEntity = nullptr;
        UiSystemToolsBus::BroadcastResult(canvasSliceEntity, &UiSystemToolsInterface::GetRootSliceEntity, canvasAsset);

        if (!canvasSliceEntity)
        {
            AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed to find the root slice entity.", fullPath.c_str());
            UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
            return;
        }

        AZStd::string prefabBuffer;
        AZ::IO::ByteContainerStream<AZStd::string > prefabStream(&prefabBuffer);
        if (!AZ::Utils::SaveObjectToStream<AZ::Entity>(prefabStream, AZ::ObjectStream::ST_XML, canvasSliceEntity))
        {
            AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed due to errors serializing editor UI canvas.", fullPath.c_str());
            UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
            return;
        }

        AZ::SerializeContext* context;
        AZ::ComponentApplicationBus::BroadcastResult(context, &AZ::ComponentApplicationBus::Events::GetSerializeContext);

        prefabStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_processingMutex);

            AZ::Data::Asset<AZ::SliceAsset> asset;
            asset.Create(AZ::Data::AssetId(AZ::Uuid::CreateRandom()));
            AZ::SliceAssetHandler assetHandler(context);
            if (!assetHandler.LoadAssetData(asset, &prefabStream, AZ::ObjectStream::AssetFilterSlicesOnly))
            {
                AZ_Error(s_uiSliceBuilder, false, "Failed to load the serialized Slice Asset.");
                UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
                return;
            }

            // Flush asset manager events to ensure no asset references are held by closures queued on Ebuses.
            AZ::Data::AssetManager::Instance().DispatchEvents();

            // Fail gracefully if any errors occurred while serializing in the editor UI canvas.
            // i.e. missing assets or serialization errors.
            if (traceDrillerHook.GetErrorCount() > 0)
            {
                AZ_Error(s_uiSliceBuilder, false, "Compiling UI canvas \"%s\" failed due to errors deserializing editor UI canvas.", fullPath.c_str());
                UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
                return;
            }

            // Get the prefab component from the prefab asset
            AZ::SliceComponent* sourceSlice = (asset.Get()) ? asset.Get()->GetComponent() : nullptr;

            // Now create a flattened prefab component from the source one
            if (sourceSlice)
            {
                AZ::SliceComponent::EntityList sourceEntities;
                sourceSlice->GetEntities(sourceEntities);
                AZ::Entity exportSliceEntity;
                AZ::SliceComponent* exportSlice = exportSliceEntity.CreateComponent<AZ::SliceComponent>();

                // For export, components can assume they're initialized, but not activated.
                for (AZ::Entity* sourceEntity : sourceEntities)
                {
                    if (sourceEntity->GetState() == AZ::Entity::ES_CONSTRUCTED)
                    {
                        sourceEntity->Init();
                    }
                }

                if (traceDrillerHook.GetErrorCount() > 0)
                {
                    AZ_Error(s_uiSliceBuilder, false, "Failed to instantiate entities.");
                    return;
                }

                // Prepare entities for export. This involves invoking BuildGameEntity on source
                // entity's components, targeting a separate entity for export.
                for (AZ::Entity* sourceEntity : sourceEntities)
                {
                    AZ::Entity* exportEntity = aznew AZ::Entity(sourceEntity->GetName().c_str());
                    exportEntity->SetId(sourceEntity->GetId());

                    const AZ::Entity::ComponentArrayType& editorComponents = sourceEntity->GetComponents();
                    for (AZ::Component* component : editorComponents)
                    {
                        auto* asEditorComponent =
                            azrtti_cast<AzToolsFramework::Components::EditorComponentBase*>(component);

                        if (asEditorComponent)
                        {
                            size_t oldComponentCount = exportEntity->GetComponents().size();
                            asEditorComponent->BuildGameEntity(exportEntity);
                            if (exportEntity->GetComponents().size() > oldComponentCount)
                            {
                                AZ::Component* newComponent = exportEntity->GetComponents().back();
                                AZ_Error("Export", asEditorComponent->GetId() != AZ::InvalidComponentId, "For entity \"%s\", component \"%s\" doesn't have a valid component id",
                                    sourceEntity->GetName().c_str(), asEditorComponent->RTTI_GetType().ToString<AZStd::string>().c_str());
                                newComponent->SetId(asEditorComponent->GetId());
                            }
                        }
                        else
                        {
                            // The component is already runtime-ready. I.e. it is not an editor component.
                            // Clone the component and add it to the export entity
                            AZ::Component* clonedComponent = context->CloneObject(component);
                            exportEntity->AddComponent(clonedComponent);
                        }
                    }

                    // Pre-sort prior to exporting so it isn't required at instantiation time.
                    const AZ::Entity::DependencySortResult sortResult = exportEntity->EvaluateDependencies();
                    if (AZ::Entity::DSR_OK != sortResult)
                    {
                        const char* sortResultError = "";
                        switch (sortResult)
                        {
                        case AZ::Entity::DSR_CYCLIC_DEPENDENCY:
                            sortResultError = "Cyclic dependency found";
                            break;
                        case AZ::Entity::DSR_MISSING_REQUIRED:
                            sortResultError = "Required services missing";
                            break;
                        }

                        AZ_Error(s_uiSliceBuilder, false, "For UI canvas \"%s\", Entity \"%s\" [0x%llx] dependency evaluation failed: %s. Compiled canvas cannot be generated.",
                            fullPath.c_str(), exportEntity->GetName().c_str(), static_cast<AZ::u64>(exportEntity->GetId()),
                            sortResultError);
                        UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
                        return;
                    }

                    exportSlice->AddEntity(exportEntity);
                }

                AZ::SliceComponent::EntityList exportEntities;
                exportSlice->GetEntities(exportEntities);

                if (exportEntities.size() != sourceEntities.size())
                {
                    AZ_Error(s_uiSliceBuilder, false, "Entity export list size must match that of the import list.");
                    UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
                    return;
                }

                // Save runtime UI canvas to disk.
                AZ::IO::FileIOStream outputStream(outputPath.c_str(), AZ::IO::OpenMode::ModeWrite);
                if (outputStream.IsOpen())
                {
                    exportSliceEntity.RemoveComponent(exportSlice);
                    UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::ReplaceRootSliceSliceComponent, canvasAsset, exportSlice);
                    UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::SaveCanvasToStream, canvasAsset, outputStream);
                    outputStream.Close();

                    AZ_TracePrintf(s_uiSliceBuilder, "Output file %s", outputPath.c_str());
                }
                else
                {
                    AZ_Error(s_uiSliceBuilder, false, "Failed to open output file %s", outputPath.c_str());
                    UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
                    return;
                }

                // Finalize entities for export. This will remove any export components temporarily
                // assigned by the source entity's components.
                auto sourceIter = sourceEntities.begin();
                auto exportIter = exportEntities.begin();
                for (; sourceIter != sourceEntities.end(); ++sourceIter, ++exportIter)
                {
                    AZ::Entity* sourceEntity = *sourceIter;
                    AZ::Entity* exportEntity = *exportIter;

                    const AZ::Entity::ComponentArrayType& editorComponents = sourceEntity->GetComponents();
                    for (AZ::Component* component : editorComponents)
                    {
                        auto* asEditorComponent =
                            azrtti_cast<AzToolsFramework::Components::EditorComponentBase*>(component);

                        if (asEditorComponent)
                        {
                            asEditorComponent->FinishedBuildingGameEntity(exportEntity);
                        }
                    }
                }

                AssetBuilderSDK::JobProduct jobProduct(outputPath);
                jobProduct.m_productAssetType = azrtti_typeid<LyShine::CanvasAsset>();
                jobProduct.m_productSubID = 0;
                response.m_outputProducts.push_back(jobProduct);
            }
            else
            {
                AZ_Error(s_uiSliceBuilder, false, "Failed to find the slice component from the serialized slice asset.");
                UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
                return;
            }


            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;

            UiSystemToolsBus::Broadcast(&UiSystemToolsInterface::DestroyCanvas, canvasAsset);
        }

        AZ_TracePrintf(s_uiSliceBuilder, "Finished processing uicanvas %s", fullPath.c_str());
    }
}
