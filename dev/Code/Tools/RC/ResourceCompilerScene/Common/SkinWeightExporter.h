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

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>
#include <SceneAPI/SceneCore/Components/ExportingComponent.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class ISkinWeightData;
        }
    }

    namespace RC
    {
        struct ResolveRootBoneFromNodeContext;
        struct MeshNodeExportContext;

        class SkinWeightExporter
            : public SceneAPI::SceneCore::ExportingComponent
        {
        public:
            using BoneNameIdMap = AZStd::unordered_map<AZStd::string, int>;

            AZ_COMPONENT(SkinWeightExporter, "{97C7D185-14F5-4BB1-AAE0-120A722882D1}", SceneAPI::SceneCore::ExportingComponent);

            SkinWeightExporter();
            ~SkinWeightExporter() override = default;

            static void Reflect(ReflectContext* context);

            SceneAPI::Events::ProcessingResult ResolveRootBoneFromNode(ResolveRootBoneFromNodeContext& context);
            SceneAPI::Events::ProcessingResult ProcessSkinWeights(MeshNodeExportContext& context);

        protected:
#if defined(AZ_COMPILER_MSVC) && AZ_COMPILER_MSVC <= 1800
            // Workaround for VS2013 - Delete the copy constructor and make it private
            // https://connect.microsoft.com/VisualStudio/feedback/details/800328/std-is-copy-constructible-is-broken
            SkinWeightExporter(const SkinWeightExporter&) = delete;
#endif
            void SetSkinWeights(MeshNodeExportContext& context, BoneNameIdMap boneNameIdMap);
            int GetGlobalBoneId(const AZStd::shared_ptr<const SceneAPI::DataTypes::ISkinWeightData>& skinWeights, BoneNameIdMap boneNameIdMap, int boneId);
        };
    } // namespace RC
} // namespace AZ