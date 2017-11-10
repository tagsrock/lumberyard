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
#include <AzCore/Math/Uuid.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Math/Vector2.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/EntityContext.h>

// Forward declarations
namespace AZ
{
    class Entity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Bus for making requests to the UI game entity context.
class UiGameEntityContextRequests
    : public AZ::EBusTraits
{
public:

    virtual ~UiGameEntityContextRequests() {}

    //////////////////////////////////////////////////////////////////////////
    // EBusTraits overrides. Accessed by EntityContextId
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
    typedef AzFramework::EntityContextId BusIdType;
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
    //////////////////////////////////////////////////////////////////////////

    //! Instantiates a dynamic slice asynchronously.
    //! \return a ticket identifying the spawn request.
    //!         Callers can immediately subscribe to the SliceInstantiationResultBus for this ticket
    //!         to receive result for this specific request.
    virtual AzFramework::SliceInstantiationTicket InstantiateDynamicSlice(
        const AZ::Data::Asset<AZ::Data::AssetData>& /*sliceAsset*/,
        const AZ::Vector2& /*position*/,
        bool /*isViewportPosition*/,
        AZ::Entity* /*parent*/,
        const AZ::EntityUtils::EntityIdMapper& /*customIdMapper*/)
    { return AzFramework::SliceInstantiationTicket(); }
};

using UiGameEntityContextBus = AZ::EBus<UiGameEntityContextRequests>;

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Bus for receiving notifications from the UI game entity context component.
class UiGameEntityContextNotifications
    : public AZ::EBusTraits
{
public:

    virtual ~UiGameEntityContextNotifications() = default;

    /// Fired when a slice has been successfully instantiated.
    virtual void OnSliceInstantiated(const AZ::Data::AssetId& /*sliceAssetId*/,
        const AZ::SliceComponent::SliceInstanceAddress& /*instance*/,
        const AzFramework::SliceInstantiationTicket& /*ticket*/) {}

    /// Fired when a slice asset could not be instantiated.
    virtual void OnSliceInstantiationFailed(const AZ::Data::AssetId& /*sliceAssetId*/,
        const AzFramework::SliceInstantiationTicket& /*ticket*/) {}
};

using UiGameEntityContextNotificationBus = AZ::EBus<UiGameEntityContextNotifications>;