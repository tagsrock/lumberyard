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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneSystem.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxTransformImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxImporterUtilities.h>
#include <SceneAPI/FbxSDKWrapper/FbxNodeWrapper.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneData/GraphData/TransformData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneBuilder
        {
            const char* FbxTransformImporter::s_transformNodeName = "transform";

            FbxTransformImporter::FbxTransformImporter()
            {
                BindToCall(&FbxTransformImporter::ImportTransform);
            }

            void FbxTransformImporter::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (serializeContext)
                {
                    serializeContext->Class<FbxTransformImporter, SceneCore::LoadingComponent>()->Version(1);
                }
            }

            Events::ProcessingResult FbxTransformImporter::ImportTransform(SceneNodeAppendedContext& context)
            {
                AZ_TraceContext("Importer", "Transform");

                Transform localTransform;

                if (!GetBindPoseLocalTransform(context.m_sourceScene, context.m_sourceNode, localTransform))
                {
                    localTransform = context.m_sourceNode.EvaluateLocalTransform();
                    Transform geoTransform = context.m_sourceNode.GetGeometricTransform(); // transform of the pivot
                    localTransform *= geoTransform; 
                }

                if (localTransform == Transform::Identity())
                {
                    return Events::ProcessingResult::Ignored;
                }

#if !defined(MOTIONCANVAS_GEM_ENABLED)
                context.m_sourceSceneSystem.SwapTransformForUpAxis(localTransform);
#endif
                context.m_sourceSceneSystem.ConvertUnit(localTransform);

                AZStd::shared_ptr<SceneData::GraphData::TransformData> transformData = 
                    AZStd::make_shared<SceneData::GraphData::TransformData>(localTransform);
                AZ_Assert(transformData, "Failed to allocate tranform data.");
                if (!transformData)
                {
                    return Events::ProcessingResult::Failure;
                }


                // If it is non-endpoint data populated node, add a transform attribute
                if (context.m_scene.GetGraph().HasNodeContent(context.m_currentGraphPosition))
                {
                    if (!context.m_scene.GetGraph().IsNodeEndPoint(context.m_currentGraphPosition))
                    {
                        Containers::SceneGraph::NodeIndex newIndex =
                            context.m_scene.GetGraph().AddChild(context.m_currentGraphPosition, s_transformNodeName);

                        AZ_Assert(newIndex.IsValid(), "Failed to create SceneGraph node for attribute.");
                        if (!newIndex.IsValid())
                        {
                            return Events::ProcessingResult::Failure;
                        }

                        Events::ProcessingResult transformAttributeResult;
                        SceneAttributeDataPopulatedContext dataPopulated(context, transformData, newIndex, 
                            s_transformNodeName);
                        transformAttributeResult = Events::Process(dataPopulated);

                        if (transformAttributeResult != Events::ProcessingResult::Failure)
                        {
                            transformAttributeResult = AddAttributeDataNodeWithContexts(dataPopulated);
                        }

                        return transformAttributeResult;
                    }
                }
                else
                {
                    bool addedData = context.m_scene.GetGraph().SetContent(
                        context.m_currentGraphPosition,
                        transformData);
                
                    AZ_Assert(addedData, "Failed to add node data");
                    return addedData ? Events::ProcessingResult::Success : Events::ProcessingResult::Failure;
                }

                return Events::ProcessingResult::Ignored;
            }
        } // namespace FbxSceneBuilder
    } // namespace SceneAPI
} // namespace AZ