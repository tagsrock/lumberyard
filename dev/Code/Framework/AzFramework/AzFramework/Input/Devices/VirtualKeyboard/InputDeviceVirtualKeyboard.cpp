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

#include <AzFramework/Input/Devices/VirtualKeyboard/InputDeviceVirtualKeyboard.h>
#include <AzFramework/Input/Utils/ProcessRawInputEventQueues.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputDeviceId InputDeviceVirtualKeyboard::Id("virtual_keyboard");

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceVirtualKeyboard::Command::EditEnter("virtual_keyboard_edit_enter");
    const InputChannelId InputDeviceVirtualKeyboard::Command::EditClear("virtual_keyboard_edit_clear");
    const InputChannelId InputDeviceVirtualKeyboard::Command::NavigationBack("virtual_keyboard_navigation_back");
    const AZStd::array<InputChannelId, 3> InputDeviceVirtualKeyboard::Command::All =
    {{
        EditClear,
        EditEnter,
        NavigationBack
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceVirtualKeyboard::Implementation::CustomCreateFunctionType InputDeviceVirtualKeyboard::Implementation::CustomCreateFunctionPointer = nullptr;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceVirtualKeyboard::InputDeviceVirtualKeyboard()
        : InputDevice(Id)
        , m_allChannelsById()
        , m_pimpl(nullptr)
    {
        // Create all command input channels
        for (const InputChannelId& channelId : Command::All)
        {
            InputChannel* channel = aznew InputChannel(channelId, *this);
            m_allChannelsById[channelId] = channel;
            m_commandChannelsById[channelId] = channel;
        }

        // Create the platform specific implementation
        m_pimpl = Implementation::CustomCreateFunctionPointer ?
                  Implementation::CustomCreateFunctionPointer(*this) :
                  Implementation::Create(*this);

        // Connect to the text entry request bus
        InputTextEntryRequestBus::Handler::BusConnect(GetInputDeviceId());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceVirtualKeyboard::~InputDeviceVirtualKeyboard()
    {
        // Disconnect from the text entry request bus
        InputTextEntryRequestBus::Handler::BusDisconnect(GetInputDeviceId());

        // Destroy the platform specific implementation
        delete m_pimpl;

        // Destroy all command input channels
        for (const auto& channelById : m_commandChannelsById)
        {
            delete channelById.second;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputDevice::InputChannelByIdMap& InputDeviceVirtualKeyboard::GetInputChannelsById() const
    {
        return m_allChannelsById;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceVirtualKeyboard::IsSupported() const
    {
        return m_pimpl != nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceVirtualKeyboard::IsConnected() const
    {
        return m_pimpl ? m_pimpl->IsConnected() : false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::TickInputDevice()
    {
        if (m_pimpl)
        {
            m_pimpl->TickInputDevice();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::TextEntryStarted(float activeTextFieldNormalizedBottomY)
    {
        if (m_pimpl)
        {
            m_pimpl->TextEntryStarted(activeTextFieldNormalizedBottomY);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::TextEntryStopped()
    {
        if (m_pimpl)
        {
            m_pimpl->TextEntryStopped();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceVirtualKeyboard::Implementation::Implementation(InputDeviceVirtualKeyboard& inputDevice)
        : m_inputDevice(inputDevice)
        , m_rawCommandEventQueue()
        , m_rawTextEventQueue()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceVirtualKeyboard::Implementation::~Implementation()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::Implementation::QueueRawCommandEvent(
        const InputChannelId& inputChannelId)
    {
        // Virtual keyboard commands are unique in that they don't go through states like most
        // other input channels. Rather, they simply dispatch one-off 'fire and forget' events.
        // But we still want to queue them so that they're dispatched in ProcessRawEventQueues
        // at the same as all other input events during the call to TickInputDevice each frame.
        m_rawCommandEventQueue.push_back(inputChannelId);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::Implementation::QueueRawTextEvent(const AZStd::string& textUTF8)
    {
        m_rawTextEventQueue.push_back(textUTF8);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::Implementation::ProcessRawEventQueues()
    {
        // Process all raw input events that were queued since the last call to this function.
        // Text events should be processed first in case text input is disabled by a command event.
        ProcessRawInputTextEventQueue(m_rawTextEventQueue);

        // Virtual keyboard commands are unique in that they don't go through states like most
        // other input channels. Rather, they simply dispatch one-off 'fire and forget' events.
        for (const InputChannelId& channelId : m_rawCommandEventQueue)
        {
            const auto& channelIt = m_inputDevice.m_commandChannelsById.find(channelId);
            if (channelIt != m_inputDevice.m_commandChannelsById.end() && !channelIt->second)
            {
                const InputChannel& channel = *(channelIt->second);
                m_inputDevice.BroadcastInputChannelEvent(channel);
            }
            else
            {
                // Unknown channel id, warn but handle gracefully
                AZ_Warning("InputDeviceVirtualKeyboard::Implementation::ProcessRawEventQueues", false,
                           "Raw input event queued with unrecognized id: %s", channelId.GetName());
            }
        }
        m_rawCommandEventQueue.clear();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceVirtualKeyboard::Implementation::ResetInputChannelStates()
    {
        m_inputDevice.ResetInputChannelStates();
    }
} // namespace AzFramework
