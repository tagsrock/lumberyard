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

#include "ShapeComponentBus.h"
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace LmbrCentral
{
    /**
     * Configuration data for CompoundShapeConfiguration
     */
    class CompoundShapeConfiguration
    {
    public:

        AZ_CLASS_ALLOCATOR(CompoundShapeConfiguration, AZ::SystemAllocator, 0);
        AZ_RTTI(CompoundShapeConfiguration, "{4CEB4E5C-4CBD-4A84-88BA-87B23C103F3F}");
        static void Reflect(AZ::ReflectContext* context)
        {
            auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<CompoundShapeConfiguration>()
                    ->Version(1)
                    ->Field("Child Shape Entities", &CompoundShapeConfiguration::m_childEntities);

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<CompoundShapeConfiguration>("Configuration", "Compound shape configuration parameters")
                        ->DataElement(0, &CompoundShapeConfiguration::m_childEntities,
                        "Child Shape Entities", "A list of entities that have shapes on them which when combined, act as the compound shape")
                        ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, true)
                        ->ElementAttribute(AZ::Edit::Attributes::RequiredService, AZ_CRC("ShapeService"));
                }
            }
        }

        virtual ~CompoundShapeConfiguration() = default;

        const AZStd::list<AZ::EntityId>& GetChildEntities()
        {
            return m_childEntities;
        }

    private:
        AZStd::list<AZ::EntityId> m_childEntities;
    };
   

    /*!
    * Services provided by the Compound Shape Component
    */
    class CompoundShapeComponentRequests : public AZ::ComponentBus
    {
    public:
        virtual CompoundShapeConfiguration GetCompoundShapeConfiguration() = 0;
    };

    // Bus to service the Compound Shape component event group
    using CompoundShapeComponentRequestsBus = AZ::EBus<CompoundShapeComponentRequests>;

} // namespace LmbrCentral