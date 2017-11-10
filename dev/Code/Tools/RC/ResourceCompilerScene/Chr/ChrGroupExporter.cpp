

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

#include <Cry_Geo.h> // Needed for CGFContent.h
#include <CGFContent.h>
#include <RC/ResourceCompilerScene/Common/CommonExportContexts.h>
#include <RC/ResourceCompilerScene/Chr/ChrExportContexts.h>
#include <RC/ResourceCompilerScene/Chr/ChrGroupExporter.h>
#include <SceneAPI/SceneCore/DataTypes/Groups/ISkeletonGroup.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneUtil = AZ::SceneAPI::Utilities;
        namespace SceneContainer = AZ::SceneAPI::Containers;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;

        const AZStd::string ChrGroupExporter::fileExtension = "chr";

        ChrGroupExporter::ChrGroupExporter(IAssetWriter* writer, IConvertContext* convertContext)
            : CallProcessorBinder()
            , m_assetWriter(writer)
            , m_convertContext(convertContext)
        {
            BindToCall(&ChrGroupExporter::ProcessContext);
            ActivateBindings();
        }

        SceneEvents::ProcessingResult ChrGroupExporter::ProcessContext(ChrGroupExportContext& context) const
        {
            if (context.m_phase != Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            AZStd::string filename = SceneUtil::FileUtilities::CreateOutputFileName(context.m_group.GetName(), context.m_outputDirectory, fileExtension);
            if (filename.empty())
            {
                AZ_TracePrintf(SceneUtil::ErrorWindow, "Invalid filename, can not be an empty value.\n");
                return SceneEvents::ProcessingResult::Failure;
            }
            if (!SceneUtil::FileUtilities::EnsureTargetFolderExists(filename))
            {
                AZ_TracePrintf(SceneUtil::ErrorWindow, "Invalid filename, target folder does not exist. ('%s')\n", filename.c_str());
                return SceneEvents::ProcessingResult::Failure;
            }


            SceneEvents::ProcessingResultCombiner result;

            CContentCGF cgfContent(filename.c_str());
            AZStd::unordered_map<AZStd::string, int> boneNameIdMap;
            SkeletonExportContext skeletonContextConstruction(context.m_scene, context.m_group.GetSelectedRootBone(), *cgfContent.GetSkinningInfo(), boneNameIdMap, Phase::Construction);
            ConfigureChrContent(cgfContent);
            result += SceneEvents::Process(skeletonContextConstruction);
            result += SceneEvents::Process<SkeletonExportContext>(context.m_scene, context.m_group.GetSelectedRootBone(), *cgfContent.GetSkinningInfo(), boneNameIdMap, Phase::Filling);
            result += SceneEvents::Process<SkeletonExportContext>(context.m_scene, context.m_group.GetSelectedRootBone(), *cgfContent.GetSkinningInfo(), boneNameIdMap, Phase::Finalizing);
            
            AZ_Assert(m_assetWriter != nullptr, "Unable to write CHR due to invalid asset writer.");
            if (m_assetWriter)
            {
                if (!m_assetWriter->WriteCHR(&cgfContent, m_convertContext))
                {
                    AZ_TracePrintf(SceneUtil::ErrorWindow, "Failed writing CHR file ('%s')\n", filename.c_str());
                    result += SceneEvents::ProcessingResult::Failure;
                }
            }
            else
            {
                AZ_TracePrintf(SceneUtil::ErrorWindow, "Failed writing CHR file ('%s')\n", filename.c_str());
                result += SceneEvents::ProcessingResult::Failure;
            }

            return result.GetResult();
        }

        void ChrGroupExporter::ConfigureChrContent(CContentCGF& content) const
        {
            CExportInfoCGF* exportInfo = content.GetExportInfo();
            AZ_Assert(exportInfo != nullptr, "Invalid export info from %s.", content.GetFilename());

            exportInfo->bMergeAllNodes = true;
            exportInfo->bUseCustomNormals = false;
            exportInfo->bCompiledCGF = false;
            exportInfo->bHavePhysicsProxy = false;
            exportInfo->bHaveAutoLods = false;
            exportInfo->bNoMesh = true;
            exportInfo->b8WeightsPerVertex = false;
            exportInfo->bWantF32Vertices = false;
            exportInfo->authorToolVersion = 1;
        }
    }
}
