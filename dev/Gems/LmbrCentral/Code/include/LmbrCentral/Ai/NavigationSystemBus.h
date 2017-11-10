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

#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Math/Vector3.h>

namespace LmbrCentral
{
    /**
    * The result returned from the ray cast
    */
    struct NavRayCastResult
    {
        using NavigationMeshId = AZ::u32;

        AZ_TYPE_INFO(NavRayCastResult, "{3135761F-9998-4623-A374-9364069E13AE}");
        AZ_CLASS_ALLOCATOR_DECL

        bool m_collision = false; ///< True if there was a collision
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();  ///< The position of the hit in world space
        NavigationMeshId m_meshId = 0; ///< The mesh id of the navigation mesh hit
    };

    /*!
    * NavigationSystemComponentRequests
    * Requests serviced by the Navigation System component.
    */
    class NavigationSystemRequests
        : public AZ::EBusTraits
    {
    public:

        //////////////////////////////////////////////////////////////////////////
        // EBus Traits overrides (Configuring this Ebus)
        // Using Defaults
        //////////////////////////////////////////////////////////////////////////

        virtual ~NavigationSystemRequests() {}

        /**
        * Call the Navigation System RayCastWorld, and return the result

         * \param begin         The origin of the ray
         * \param direction     The direction for the ray to travel
         * \param maxDistance   The maximum distance the ray will travel
         */
        virtual NavRayCastResult RayCast(const AZ::Vector3& begin, const AZ::Vector3& direction, float maxDistance) { return NavRayCastResult(); }
    };

    using NavigationSystemRequestBus = AZ::EBus<NavigationSystemRequests>;

} // namespace LmbrCentral
