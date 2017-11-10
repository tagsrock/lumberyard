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

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Utils/AdjustAnalogInputForDeadZone.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    const char* InputDeviceGamepad::Name("gamepad");
    const InputDeviceId InputDeviceGamepad::IdForIndex0(Name, 0);
    const InputDeviceId InputDeviceGamepad::IdForIndex1(Name, 1);
    const InputDeviceId InputDeviceGamepad::IdForIndex2(Name, 2);
    const InputDeviceId InputDeviceGamepad::IdForIndex3(Name, 3);
    const InputDeviceId InputDeviceGamepad::IdForIndexN(AZ::u32 n) { return InputDeviceId(Name, n); }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceGamepad::Button::A("gamepad_button_a");
    const InputChannelId InputDeviceGamepad::Button::B("gamepad_button_b");
    const InputChannelId InputDeviceGamepad::Button::X("gamepad_button_x");
    const InputChannelId InputDeviceGamepad::Button::Y("gamepad_button_y");
    const InputChannelId InputDeviceGamepad::Button::L1("gamepad_button_l1");
    const InputChannelId InputDeviceGamepad::Button::R1("gamepad_button_r1");
    const InputChannelId InputDeviceGamepad::Button::L3("gamepad_button_l3");
    const InputChannelId InputDeviceGamepad::Button::R3("gamepad_button_r3");
    const InputChannelId InputDeviceGamepad::Button::DU("gamepad_button_d_up");
    const InputChannelId InputDeviceGamepad::Button::DD("gamepad_button_d_down");
    const InputChannelId InputDeviceGamepad::Button::DL("gamepad_button_d_left");
    const InputChannelId InputDeviceGamepad::Button::DR("gamepad_button_d_right");
    const InputChannelId InputDeviceGamepad::Button::Start("gamepad_button_start");
    const InputChannelId InputDeviceGamepad::Button::Select("gamepad_button_select");
    const AZStd::array<InputChannelId, 14> InputDeviceGamepad::Button::All =
    {{
        A,
        B,
        X,
        Y,
        L1,
        R1,
        L3,
        R3,
        DU,
        DD,
        DL,
        DR,
        Start,
        Select
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceGamepad::Trigger::L2("gamepad_trigger_l2");
    const InputChannelId InputDeviceGamepad::Trigger::R2("gamepad_trigger_r2");
    const AZStd::array<InputChannelId, 2> InputDeviceGamepad::Trigger::All =
    {{
        L2,
        R2
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceGamepad::ThumbStickAxis2D::L("gamepad_thumbstick_l");
    const InputChannelId InputDeviceGamepad::ThumbStickAxis2D::R("gamepad_thumbstick_r");
    const AZStd::array<InputChannelId, 2> InputDeviceGamepad::ThumbStickAxis2D::All =
    {{
        L,
        R
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceGamepad::ThumbStickAxis1D::LX("gamepad_thumbstick_l_x");
    const InputChannelId InputDeviceGamepad::ThumbStickAxis1D::LY("gamepad_thumbstick_l_y");
    const InputChannelId InputDeviceGamepad::ThumbStickAxis1D::RX("gamepad_thumbstick_r_x");
    const InputChannelId InputDeviceGamepad::ThumbStickAxis1D::RY("gamepad_thumbstick_r_y");
    const AZStd::array<InputChannelId, 4> InputDeviceGamepad::ThumbStickAxis1D::All =
    {{
        LX,
        LY,
        RX,
        RY
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::LU("gamepad_thumbstick_l_up");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::LD("gamepad_thumbstick_l_down");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::LL("gamepad_thumbstick_l_left");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::LR("gamepad_thumbstick_l_right");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::RU("gamepad_thumbstick_r_up");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::RD("gamepad_thumbstick_r_down");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::RL("gamepad_thumbstick_r_left");
    const InputChannelId InputDeviceGamepad::ThumbStickDirection::RR("gamepad_thumbstick_r_right");
    const AZStd::array<InputChannelId, 8> InputDeviceGamepad::ThumbStickDirection::All =
    {{
        LU,
        LD,
        LL,
        LR,
        RU,
        RD,
        RL,
        RR
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::Implementation::CustomCreateFunctionType InputDeviceGamepad::Implementation::CustomCreateFunctionPointer = nullptr;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::InputDeviceGamepad()
        : InputDeviceGamepad(0) // Delegated constructor
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::InputDeviceGamepad(AZ::u32 index)
        : InputDevice(InputDeviceId(Name, index))
        , m_allChannelsById()
        , m_buttonChannelsById()
        , m_triggerChannelsById()
        , m_thumbStickAxis1DChannelsById()
        , m_thumbStickAxis2DChannelsById()
        , m_thumbStickDirectionChannelsById()
        , m_pimpl(nullptr)
    {
        // Create all digital button input channels
        for (const InputChannelId& channelId : Button::All)
        {
            InputChannelDigital* channel = aznew InputChannelDigital(channelId, *this);
            m_allChannelsById[channelId] = channel;
            m_buttonChannelsById[channelId] = channel;
        }

        // Create all analog trigger input channels
        for (const InputChannelId& channelId : Trigger::All)
        {
            InputChannelAnalog* channel = aznew InputChannelAnalog(channelId, *this);
            m_allChannelsById[channelId] = channel;
            m_triggerChannelsById[channelId] = channel;
        }

        // Create all thumb-stick 1D axis input channels
        for (const InputChannelId& channelId : ThumbStickAxis1D::All)
        {
            InputChannelAxis1D* channel = aznew InputChannelAxis1D(channelId, *this);
            m_allChannelsById[channelId] = channel;
            m_thumbStickAxis1DChannelsById[channelId] = channel;
        }

        // Create all thumb-stick 2D axis input channels
        for (const InputChannelId& channelId : ThumbStickAxis2D::All)
        {
            InputChannelAxis2D* channel = aznew InputChannelAxis2D(channelId, *this);
            m_allChannelsById[channelId] = channel;
            m_thumbStickAxis2DChannelsById[channelId] = channel;
        }

        // Create all thumb-stick direction input channels
        for (const InputChannelId& channelId : ThumbStickDirection::All)
        {
            InputChannelAnalog* channel = aznew InputChannelAnalog(channelId, *this);
            m_allChannelsById[channelId] = channel;
            m_thumbStickDirectionChannelsById[channelId] = channel;
        }

        // Create the platform specific implementation
        m_pimpl = Implementation::CustomCreateFunctionPointer ?
                  Implementation::CustomCreateFunctionPointer(*this) :
                  Implementation::Create(*this);

        // Connect to the haptic feedback request bus
        InputHapticFeedbackRequestBus::Handler::BusConnect(GetInputDeviceId());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::~InputDeviceGamepad()
    {
        // Disconnect from the haptic feedback request bus
        InputHapticFeedbackRequestBus::Handler::BusDisconnect(GetInputDeviceId());

        // Destroy the platform specific implementation
        delete m_pimpl;

        // Destroy all thumb-stick direction input channels
        for (const auto& channelById : m_thumbStickDirectionChannelsById)
        {
            delete channelById.second;
        }

        // Destroy all thumb-stick 2D axis input channels
        for (const auto& channelById : m_thumbStickAxis2DChannelsById)
        {
            delete channelById.second;
        }

        // Destroy all thumb-stick 1D axis input channels
        for (const auto& channelById : m_thumbStickAxis1DChannelsById)
        {
            delete channelById.second;
        }

        // Destroy all analog trigger input channels
        for (const auto& channelById : m_triggerChannelsById)
        {
            delete channelById.second;
        }

        // Destroy all digital button input channels
        for (const auto& channelById : m_buttonChannelsById)
        {
            delete channelById.second;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const GridMate::PlayerId* InputDeviceGamepad::GetAssignedLocalPlayerId() const
    {
        return m_pimpl ? m_pimpl->GetAssignedLocalPlayerId() : nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputDevice::InputChannelByIdMap& InputDeviceGamepad::GetInputChannelsById() const
    {
        return m_allChannelsById;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceGamepad::IsSupported() const
    {
        return m_pimpl != nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceGamepad::IsConnected() const
    {
        return m_pimpl ? m_pimpl->IsConnected() : false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::TickInputDevice()
    {
        if (m_pimpl)
        {
            m_pimpl->TickInputDevice();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::SetVibration(float leftMotorSpeedNormalized,
                                          float rightMotorSpeedNormalized)
    {
        if (m_pimpl)
        {
            m_pimpl->SetVibration(leftMotorSpeedNormalized, rightMotorSpeedNormalized);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::Implementation::Implementation(InputDeviceGamepad& inputDevice)
        : m_inputDevice(inputDevice)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::Implementation::~Implementation()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const GridMate::PlayerId* InputDeviceGamepad::Implementation::GetAssignedLocalPlayerId() const
    {
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::Implementation::BroadcastInputDeviceConnectedEvent() const
    {
        m_inputDevice.BroadcastInputDeviceConnectedEvent();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::Implementation::BroadcastInputDeviceDisconnectedEvent() const
    {
        m_inputDevice.BroadcastInputDeviceDisconnectedEvent();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceGamepad::Implementation::RawGamepadState::RawGamepadState(
        const DigitalButtonIdByBitMaskMap& digitalButtonIdsByBitMask)
    : m_digitalButtonIdsByBitMask(digitalButtonIdsByBitMask)
    , m_digitalButtonStates(0)
    , m_triggerButtonLState(0.0f)
    , m_triggerButtonRState(0.0f)
    , m_thumbStickLeftXState(0.0f)
    , m_thumbStickLeftYState(0.0f)
    , m_thumbStickRightXState(0.0f)
    , m_thumbStickRightYState(0.0f)
    , m_triggerMaximumValue(1.0f)
    , m_triggerDeadZoneValue(0.0f)
    , m_thumbStickMaximumValue(1.0f)
    , m_thumbStickLeftDeadZone(0.0f)
    , m_thumbStickRightDeadZone(0.0f)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::Implementation::RawGamepadState::Reset()
    {
        m_digitalButtonStates = 0;
        m_triggerButtonLState = 0.0f;
        m_triggerButtonRState = 0.0f;
        m_thumbStickLeftXState = 0.0f;
        m_thumbStickLeftYState = 0.0f;
        m_thumbStickRightXState = 0.0f;
        m_thumbStickRightYState = 0.0f;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    float InputDeviceGamepad::Implementation::RawGamepadState::GetLeftTriggerAdjustedForDeadZoneAndNormalized() const
    {
        return AdjustForDeadZoneAndNormalizeAnalogInput(m_triggerButtonLState,
                                                        m_triggerDeadZoneValue,
                                                        m_triggerMaximumValue);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    float InputDeviceGamepad::Implementation::RawGamepadState::GetRightTriggerAdjustedForDeadZoneAndNormalized() const
    {
        return AdjustForDeadZoneAndNormalizeAnalogInput(m_triggerButtonRState,
                                                        m_triggerDeadZoneValue,
                                                        m_triggerMaximumValue);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    AZ::Vector2 InputDeviceGamepad::Implementation::RawGamepadState::GetLeftThumbStickAdjustedForDeadZoneAndNormalized() const
    {
        return AdjustForDeadZoneAndNormalizeThumbStickInput(m_thumbStickLeftXState,
                                                            m_thumbStickLeftYState,
                                                            m_thumbStickLeftDeadZone,
                                                            m_thumbStickMaximumValue);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    AZ::Vector2 InputDeviceGamepad::Implementation::RawGamepadState::GetRightThumbStickAdjustedForDeadZoneAndNormalized() const
    {
        return AdjustForDeadZoneAndNormalizeThumbStickInput(m_thumbStickRightXState,
                                                            m_thumbStickRightYState,
                                                            m_thumbStickRightDeadZone,
                                                            m_thumbStickMaximumValue);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::Implementation::ProcessRawGamepadState(
        const RawGamepadState& rawGamepadState)
    {
        // Update digital button channels
        for (const auto& digitalButtonIdByBitMaskPair : rawGamepadState.m_digitalButtonIdsByBitMask)
        {
            const AZ::u32 buttonState = (rawGamepadState.m_digitalButtonStates & digitalButtonIdByBitMaskPair.first);
            const InputChannelId& channelId = *(digitalButtonIdByBitMaskPair.second);
            m_inputDevice.m_buttonChannelsById[channelId]->ProcessRawInputEvent(buttonState != 0);
        }

        // Update the left analog trigger button channel
        const float valueL2 = rawGamepadState.GetLeftTriggerAdjustedForDeadZoneAndNormalized();
        m_inputDevice.m_triggerChannelsById[InputDeviceGamepad::Trigger::L2]->ProcessRawInputEvent(valueL2);

        // Update the right analog trigger button channel
        const float valueR2 = rawGamepadState.GetRightTriggerAdjustedForDeadZoneAndNormalized();
        m_inputDevice.m_triggerChannelsById[InputDeviceGamepad::Trigger::R2]->ProcessRawInputEvent(valueR2);

        // Update the left thumb-stick channels
        const AZ::Vector2 valuesLeftThumb = rawGamepadState.GetLeftThumbStickAdjustedForDeadZoneAndNormalized();
        m_inputDevice.m_thumbStickAxis2DChannelsById[InputDeviceGamepad::ThumbStickAxis2D::L]->ProcessRawInputEvent(valuesLeftThumb);
        m_inputDevice.m_thumbStickAxis1DChannelsById[InputDeviceGamepad::ThumbStickAxis1D::LX]->ProcessRawInputEvent(valuesLeftThumb.GetX());
        m_inputDevice.m_thumbStickAxis1DChannelsById[InputDeviceGamepad::ThumbStickAxis1D::LY]->ProcessRawInputEvent(valuesLeftThumb.GetY());

        const float leftStickUp = AZ::GetClamp(valuesLeftThumb.GetY(), 0.0f, 1.0f);
        const float leftStickDown = AZ::GetClamp(valuesLeftThumb.GetY(), -1.0f, 0.0f);
        const float leftStickLeft = AZ::GetClamp(valuesLeftThumb.GetX(), -1.0f, 0.0f);
        const float leftStickRight = AZ::GetClamp(valuesLeftThumb.GetX(), 0.0f, 1.0f);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::LU]->ProcessRawInputEvent(leftStickUp);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::LD]->ProcessRawInputEvent(leftStickDown);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::LL]->ProcessRawInputEvent(leftStickLeft);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::LR]->ProcessRawInputEvent(leftStickRight);

        // Update the right thumb-stick channels
        const AZ::Vector2 valuesRightThumb = rawGamepadState.GetRightThumbStickAdjustedForDeadZoneAndNormalized();
        m_inputDevice.m_thumbStickAxis2DChannelsById[InputDeviceGamepad::ThumbStickAxis2D::R]->ProcessRawInputEvent(valuesRightThumb);
        m_inputDevice.m_thumbStickAxis1DChannelsById[InputDeviceGamepad::ThumbStickAxis1D::RX]->ProcessRawInputEvent(valuesRightThumb.GetX());
        m_inputDevice.m_thumbStickAxis1DChannelsById[InputDeviceGamepad::ThumbStickAxis1D::RY]->ProcessRawInputEvent(valuesRightThumb.GetY());

        const float rightStickUp = AZ::GetClamp(valuesRightThumb.GetY(), 0.0f, 1.0f);
        const float rightStickDown = AZ::GetClamp(valuesRightThumb.GetY(), -1.0f, 0.0f);
        const float rightStickLeft = AZ::GetClamp(valuesRightThumb.GetX(), -1.0f, 0.0f);
        const float rightStickRight = AZ::GetClamp(valuesRightThumb.GetX(), 0.0f, 1.0f);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::RU]->ProcessRawInputEvent(rightStickUp);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::RD]->ProcessRawInputEvent(rightStickDown);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::RL]->ProcessRawInputEvent(rightStickLeft);
        m_inputDevice.m_thumbStickDirectionChannelsById[InputDeviceGamepad::ThumbStickDirection::RR]->ProcessRawInputEvent(rightStickRight);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceGamepad::Implementation::ResetInputChannelStates()
    {
        m_inputDevice.ResetInputChannelStates();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    AZ::u32 InputDeviceGamepad::Implementation::GetInputDeviceIndex() const
    {
        return m_inputDevice.GetInputDeviceId().GetIndex();
    }
} // namespace AzFramework
