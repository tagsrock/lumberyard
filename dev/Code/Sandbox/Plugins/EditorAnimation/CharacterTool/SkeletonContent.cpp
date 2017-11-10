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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "pch.h"
#include "CharacterToolSystem.h"
#include "EntryList.h"
#include "ExplorerFileList.h"
#include "SkeletonContent.h"
#include "SkeletonList.h"
#include <ICryAnimation.h>

namespace CharacterTool
{
    void SkeletonContent::Serialize(IArchive& ar)
    {
        Serialization::SContext<SkeletonParameters> context(ar, &skeletonParameters);
        IDefaultSkeleton* skeleton = gEnv->pCharacterManager->LoadModelSKELUnsafeManualRef(skeletonParameters.skeletonFileName, CA_CharEditModel);
        Serialization::SContext<IDefaultSkeleton> contextSkeleton(ar, skeleton);

        ar(skeletonParameters.includes, "includes", "+Includes");
        if (ar.IsEdit() && ar.IsOutput())
        {
            System* system = ar.FindContext<System>();
            UpdateIncludedAnimationSet(system->skeletonList.get());
            ar(includedAnimationSetFilter, "includedAnimationSetFilter", "+!Included Animation Set Filter");
        }
        ar(skeletonParameters.animationSetFilter, "animationSetFilter", "+[+]Animation Set Filter");
        ar(ResourceFilePath(skeletonParameters.animationEventDatabase, "Animation Events"), "animationEventDatabase", "<Events");

        ar(ResourceFolderPath(skeletonParameters.dbaPath, "Animations"), "dbaPath", "<DBA Path");
        ar.Doc("Folder path for DBA files. All DBA files from this folder will be included.");
        
        ar(skeletonParameters.individualDBAs, "individualDBAs", "Individual DBAs");

        ar(skeletonParameters.bboxExtension, "bboxExtension", "-Bounding Box Extension");
        ar(skeletonParameters.bboxIncludes, "boundingBoxInclude", "-Bounding Box Include");

        ar(skeletonParameters.jointLods, "lods", "Joint LOD");

        auto& ikDefinition = skeletonParameters.ikDefinition;
        ar(ikDefinition, "ikDefinition", ikDefinition.HasEnabledDefinitions() ? "IK Definition" : "-IK Definition");
    }

    void SkeletonContent::GetDependencies(vector<string>* deps) const
    {
        for (size_t i = 0; i < skeletonParameters.includes.size(); ++i)
        {
            deps->push_back(skeletonParameters.includes[i].filename);
        }
    }

    static void ExpandIncludes(AnimationSetFilter* outFilter, const std::vector<string>& includeStack, const vector<SkeletonParametersInclude>& includes, const string& selfPath, ExplorerFileList* skeletonList)
    {
        std::vector<string> stack = includeStack;
        std::vector<AnimationFilterFolder> includedFolders;
        for (size_t i = 0; i < includes.size(); ++i)
        {
            const string& filename = includes[i].filename;
            if (stl::find(includeStack, filename) || filename == selfPath)
            {
                CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Recursive inclusion of CHRAPARAMS: '%s'", includes[i].filename.c_str());
                continue;
            }

            SEntry<SkeletonContent>* entry = skeletonList->GetEntryByPath<SkeletonContent>(filename.c_str());
            if (entry)
            {
                skeletonList->LoadOrGetChangedEntry(entry->id);

                includedFolders.insert(includedFolders.end(),
                    entry->content.skeletonParameters.animationSetFilter.folders.begin(),
                    entry->content.skeletonParameters.animationSetFilter.folders.end());

                AnimationSetFilter filter;
                stack.push_back(filename);
                ExpandIncludes(&filter, stack, entry->content.skeletonParameters.includes, selfPath, skeletonList);
                stack.pop_back();
                includedFolders.insert(includedFolders.end(),
                    filter.folders.begin(), filter.folders.end());
            }
        }

        outFilter->folders.insert(outFilter->folders.begin(),
            includedFolders.begin(), includedFolders.end());
    }

    void SkeletonContent::UpdateIncludedAnimationSet(ExplorerFileList* skeletonList)
    {
        includedAnimationSetFilter = AnimationSetFilter();
        const string selfPath = PathUtil::ReplaceExtension(skeletonParameters.skeletonFileName, ".chrparams");
        ExpandIncludes(&includedAnimationSetFilter, std::vector<string>(), skeletonParameters.includes, selfPath, skeletonList);
    }

    void SkeletonContent::ComposeCompleteAnimationSetFilter(AnimationSetFilter* outFilter, ExplorerFileList* skeletonList) const
    {
        *outFilter = skeletonParameters.animationSetFilter;
        const string selfPath = PathUtil::ReplaceExtension(skeletonParameters.skeletonFileName, ".chrparams");
        ExpandIncludes(&*outFilter, vector<string>(), skeletonParameters.includes, selfPath, skeletonList);
    }
}
