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

#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneData/Behaviors/SoftNameTypes.h>

#if defined(MOTIONCANVAS_GEM_ENABLED)
#include <SceneAPI/SceneData/Behaviors/ActorGroup.h>
#include <SceneAPI/SceneData/Behaviors/EFXMeshRuleBehavior.h>
#include <SceneAPI/SceneData/Behaviors/EFXSkinRuleBehavior.h>
#include <SceneAPI/SceneData/Behaviors/EFXMotionGroupBehavior.h>
#endif

namespace AZ
{
    class ReflectContext;

    namespace SceneAPI
    {
        namespace SceneData
        {
            class Registry
            {
            public:
                using ComponentDescriptorList = AZStd::vector<AZ::ComponentDescriptor*>;

                AZ_CLASS_ALLOCATOR(Registry, SystemAllocator, 0)

                Registry();
                static void RegisterComponents(ComponentDescriptorList& components);

            private:
                SoftNameTypes m_softNameTypes;

#if defined(MOTIONCANVAS_GEM_ENABLED)
                EFXMeshRuleBehavior m_EFXMeshRuleBehaviors;
                EFXSkinRuleBehavior m_EFXSkinRuleBehavior;
                Behaviors::ActorGroup m_actorGroupBehaviors;
                Behaviors::EFXMotionGroupBehavior   m_motionGroupBehavior;
#endif
            };
        } // namespace SceneData
    } // namespace SceneAPI
} // namespace AZ
