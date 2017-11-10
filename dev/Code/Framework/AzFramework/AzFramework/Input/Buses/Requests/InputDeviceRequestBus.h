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

#include <AzFramework/Input/Channels/InputChannelId.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/EBus/EBus.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    class InputChannel;
    class InputDevice;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! EBus interface used to query input devices for their associated input channels and state
    class InputDeviceRequests : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests can be addressed to a specific InputDeviceId so that they are only
        //! handled by one input device that has connected to the bus using that unique id, or they
        //! can be broadcast to all input devices that have connected to the bus, regardless of id.
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests should be handled by only one input device connected to each id
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests can be addressed to a specific InputDeviceId
        using BusIdType = InputDeviceId;

        ////////////////////////////////////////////////////////////////////////////////////////////
        ///@{
        //! Alias for verbose container class
        using InputDeviceIdSet = AZStd::unordered_set<InputDeviceId>;
        using InputChannelIdSet = AZStd::unordered_set<InputChannelId>;
        using InputDeviceByIdMap = AZStd::unordered_map<InputDeviceId, const InputDevice*>;
        using InputChannelByIdMap = AZStd::unordered_map<InputChannelId, const InputChannel*>;
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Finds a specific input device (convenience function)
        //! \param[in] deviceId Id of the input device to find
        //! \return Pointer to the input device if it was found, nullptr if it was not
        static const InputDevice* FindInputDevice(const InputDeviceId& deviceId);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Gets the input device that is uniquely identified by the InputDeviceId used to address
        //! the call to this EBus function. Calls to this EBus method should never be broadcast to
        //! all connected input devices, otherwise the device returned will effectively be random.
        //! \return Pointer to the input device if it exists, nullptr otherwise
        virtual const InputDevice* GetInputDevice() const = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Request the ids of all currently enabled input devices. This does not imply they are all
        //! connected, or even available on the current platform, just that they are enabled for the
        //! application (meaning they will generate input when available / connected to the system).
        //!
        //! Can be called using either:
        //! - EBus<>::Broadcast (all input devices will add their id to o_deviceIds)
        //! - EBus<>::Event(id) (the given device will add its id to o_deviceIds - not very useful!)
        //!
        //! \param[out] o_deviceIds The set of input device ids to return
        virtual void GetInputDeviceIds(InputDeviceIdSet& o_deviceIds) const = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Request a map of all currently enabled input devices by id. This does not imply they are
        //! connected, or even available on the current platform, just that they are enabled for the
        //! application (meaning they will generate input when available / connected to the system).
        //!
        //! Can be called using either:
        //! - EBus<>::Broadcast (all input devices will add themselves to o_devicesById)
        //! - EBus<>::Event(id) (the given input device will add itself to o_devicesById)
        //!
        //! \param[out] o_devicesById The map of input devices (keyed by their id) to return
        virtual void GetInputDevicesById(InputDeviceByIdMap& o_devicesById) const = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Request the ids of all input channels associated with an input device.
        //!
        //! Can be called using either:
        //! - EBus<>::Broadcast (all input devices will add all their channel ids to o_channelIds)
        //! - EBus<>::Event(id) (the given device will add all of its channel ids to o_channelIds)
        //!
        //! \param[out] o_channelIds The set of input channel ids to return
        virtual void GetInputChannelIds(InputChannelIdSet& o_channelIds) const = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Request all input channels associated with an input device.
        //!
        //! Can be called using either:
        //! - EBus<>::Broadcast (all input devices will add all their channels to o_channelsById)
        //! - EBus<>::Event(id) (the given device will add all of its channels to o_channelsById)
        //!
        //! \param[out] o_channelsById The map of input channels (keyed by their id) to return
        virtual void GetInputChannelsById(InputChannelByIdMap& o_channelsById) const = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Tick/update input devices.
        //!
        //! Can be called using either:
        //! - EBus<>::Broadcast (all input devices are ticked/updated)
        //! - EBus<>::Event(id) (the given device is ticked/updated)
        virtual void TickInputDevice() = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Default destructor
        virtual ~InputDeviceRequests() = default;
    };
    using InputDeviceRequestBus = AZ::EBus<InputDeviceRequests>;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline const InputDevice* InputDeviceRequests::FindInputDevice(const InputDeviceId& deviceId)
    {
        const InputDevice* inputDevice = nullptr;
        InputDeviceRequestBus::EventResult(inputDevice, deviceId, &InputDeviceRequests::GetInputDevice);
        return inputDevice;
    }
} // namespace AzFramework
