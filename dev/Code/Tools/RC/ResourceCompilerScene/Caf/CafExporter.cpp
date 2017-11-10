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

#include <RC/ResourceCompilerScene/Caf/CafExporter.h>
#include <Cry_Geo.h>
#include <CGFContent.h>

#include <AzToolsFramework/Debug/TraceContext.h>

#include <SceneAPI/SceneCore/DataTypes/Groups/IAnimationGroup.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Containers/Utilities/Filters.h>

#include <RC/ResourceCompilerScene/Caf/CafExportContexts.h>

namespace AZ
{
    namespace RC
    {
        namespace SceneContainers = AZ::SceneAPI::Containers;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;

        CafExporter::CafExporter(IConvertContext* convertContext)
            : CallProcessorBinder()
            , m_convertContext(convertContext)
        {
            BindToCall(&CafExporter::ProcessContext);
            ActivateBindings();
        }

        SceneEvents::ProcessingResult CafExporter::ProcessContext(SceneEvents::ExportEventContext& context) const
        {
            const SceneContainers::SceneManifest& manifest = context.GetScene().GetManifest();
            
            auto valueStorage = manifest.GetValueStorage();
            auto view = SceneContainers::MakeDerivedFilterView<SceneDataTypes::IAnimationGroup>(valueStorage);

            SceneEvents::ProcessingResultCombiner result;
            for (const SceneDataTypes::IAnimationGroup& animationGroup : view)
            {
                AZ_TraceContext("Animation group", animationGroup.GetName());
                result += SceneEvents::Process<CafGroupExportContext>(context, animationGroup, Phase::Construction);
                result += SceneEvents::Process<CafGroupExportContext>(context, animationGroup, Phase::Filling);
                result += SceneEvents::Process<CafGroupExportContext>(context, animationGroup, Phase::Finalizing);
            }
            return result.GetResult();
        }
    }
}
