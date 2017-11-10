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

#include <fbxsdk.h>
#include <SceneAPI/FbxSceneBuilder/ImportContexts/FbxImportContexts.h>
#include <SceneAPI/FbxSDKWrapper/FbxTimeWrapper.h>
#include <SceneAPI/SceneCore/Components/LoadingComponent.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneBuilder
        {
            class FbxAnimationImporter
                : public SceneCore::LoadingComponent
            {
            public:
                AZ_COMPONENT(FbxAnimationImporter, "{26ABDA62-9DB7-4B4D-961D-44B5F5F56808}", SceneCore::LoadingComponent);

                FbxAnimationImporter();
                ~FbxAnimationImporter() override = default;

                static void Reflect(ReflectContext* context);

                Events::ProcessingResult ImportAnimation(SceneNodeAppendedContext& context);

            protected:
#if defined(AZ_COMPILER_MSVC) && AZ_COMPILER_MSVC <= 1800
                // Workaround for VS2013 - Delete the copy constructor and make it private
                // https://connect.microsoft.com/VisualStudio/feedback/details/800328/std-is-copy-constructible-is-broken
                FbxAnimationImporter(const FbxAnimationImporter&) = delete;
#endif
                static const char* s_animationNodeName;
                static const FbxSDKWrapper::FbxTimeWrapper::TimeMode s_defaultTimeMode;
            };
        } // namespace FbxSceneBuilder
    } // namespace SceneAPI
} // namespace AZ