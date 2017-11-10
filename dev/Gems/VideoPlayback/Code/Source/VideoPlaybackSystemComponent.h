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

/*
    This video playback system component's job is to provide the video playback services
    and to provide a way to handle video asset types. 
*/

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/string/string.h>
#include "Decoder.h"

namespace AZ 
{
    namespace VideoPlayback
    {
        class VideoPlaybackSystemComponent
            : public AZ::Component
        {
        public:
            AZ_COMPONENT(VideoPlaybackSystemComponent, "{B64489C9-E92A-4BD3-9012-6EEA745442F8}");

            static void Reflect(AZ::ReflectContext* context);

            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
            static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        protected:
            ////////////////////////////////////////////////////////////////////////
            // VideoPlaybackRequestBus interface implementation

            ////////////////////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////////////////////
            // AZ::Component interface implementation
            void Init() override;
            void Activate() override;
            void Deactivate() override;
            ////////////////////////////////////////////////////////////////////////
        };
    }//namespace VideoPlayback
}//namespace AZ