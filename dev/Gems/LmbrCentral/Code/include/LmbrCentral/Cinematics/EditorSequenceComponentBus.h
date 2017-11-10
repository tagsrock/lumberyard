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

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/std/string/string.h>
#include <IMovieSystem.h>

#include "SequenceComponentBus.h"

namespace LmbrCentral
{
    /*!
    * EditorSequenceComponentRequests EBus Interface
    * Messages serviced by DirectorComponents.
    */
    class EditorSequenceComponentRequests
        : public AZ::ComponentBus
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides - application is a singleton
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;  // Only one component on a entity can implement the events
        //////////////////////////////////////////////////////////////////////////

        //! Adds an entity to be animated
        virtual void AddEntityToAnimate(AZ::EntityId entityToAnimate) = 0;

        //! Remove EntityToAnimate
        virtual void RemoveEntityToAnimate(AZ::EntityId removedEntityId) = 0;

        //! Marks the entity's layer as dirty in the Editor. Returns true if the layer was marked as dirty, false otherwise.
        virtual bool MarkEntityLayerAsDirty() const = 0;

        //! Fills in a list of all animatable properites for a given component on a given entity.
        virtual void GetAllAnimatablePropertiesForComponent(IAnimNode::AnimParamInfos& addressList, AZ::EntityId id, AZ::ComponentId componentId) = 0;

        //! Fills in a list of all animatable component ids for the given entity.
        virtual void GetAnimatableComponents(AZStd::vector<AZ::ComponentId>& componentIds, AZ::EntityId id) = 0;

        //! Call before the Director Component is saved from the Editor
        virtual void OnBeforeSave() = 0;

        //! Return the EAnimValue type for the given address
        virtual EAnimValue GetValueType(const AZStd::string& animatableAddress) = 0;
    };

    using EditorSequenceComponentRequestBus = AZ::EBus<EditorSequenceComponentRequests>;

// defined in the bus header so we can refer to it in the Editor code
#define EditorSequenceComponentTypeId "{C02DC0E2-D0F3-488B-B9EE-98E28077EC56}"

} // namespace LmbrCentral