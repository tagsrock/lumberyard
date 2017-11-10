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
#include <AzCore/Module/Module.h>
#include <AzCore/Component/ComponentApplicationBus.h>

namespace AZ
{
    Module::~Module()
    {
        for (AZ::ComponentDescriptor* descriptor : m_descriptors)
        {
            // Deletes and "un-reflects" the descriptor
            descriptor->ReleaseDescriptor();
        }
    }

    void Module::RegisterComponentDescriptors()
    {
        for (const ComponentDescriptor* descriptor : m_descriptors)
        {
            AZ_Warning("AZ::Module", descriptor, "Null module descriptor is being skipped (%s)", RTTI_GetType().ToString<AZStd::string>().c_str());
            EBUS_EVENT(ComponentApplicationBus, RegisterComponentDescriptor, descriptor);
        }
    }
}
