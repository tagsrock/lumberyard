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

#include "NullArchiveComponent.h"

#include <AzCore/Component/TickBus.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AzToolsFramework
{

    void NullArchiveComponent::Activate()
    {
        ArchiveCommands::Bus::Handler::BusConnect();
    }

    void NullArchiveComponent::Deactivate()
    {
        ArchiveCommands::Bus::Handler::BusDisconnect();
    }

    void NullArchiveComponent::ExtractArchive(const AZStd::string& /*archivePath*/, const AZStd::string& /*destinationPath*/, AZ::Uuid /*taskHandle*/, const ArchiveResponseCallback& respCallback)
    {
        // Always report we failed to extract
        EBUS_QUEUE_FUNCTION(AZ::TickBus, respCallback, false);
    }

    void NullArchiveComponent::CancelTasks(AZ::Uuid /*taskHandle*/)
    {
    }

    void NullArchiveComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
        if (serialize)
        {
            serialize->Class<NullArchiveComponent, AZ::Component>()
                ->SerializerForEmptyClass()
                ;
        }
    }
} // namespace AzToolsFramework
