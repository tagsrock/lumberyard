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
#include <InputManagementFramework/InputSubComponent.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <InputNotificationBus.h>
#include <InputRequestBus.h>

namespace AZ
{
    class ReflectContext;
}

namespace Input
{
    //////////////////////////////////////////////////////////////////////////
    /// Input handles raw input from any source and outputs Pressed, Held, and Released input events
    class Input
        : public InputSubComponent
#if defined(AZ_FRAMEWORK_INPUT_ENABLED)
        , protected AzFramework::InputChannelEventListener
#else
        , protected AZ::InputNotificationBus::Handler
        , protected AZ::TickBus::Handler
#endif // defined(AZ_FRAMEWORK_INPUT_ENABLED)
        , protected AZ::GlobalInputRecordRequestBus::Handler
        , protected AZ::InputRecordRequestBus::Handler
    {
    public:
        Input();
        ~Input() override = default;
        AZ_RTTI(Input, "{546C9EBC-90EF-4F03-891A-0736BE2A487E}", InputSubComponent);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // IInputSubComponent
        void Activate(const AZ::InputEventNotificationId& eventNotificationId) override;
        void Deactivate(const AZ::InputEventNotificationId& eventNotificationId) override;

    protected:
        AZStd::string GetEditorText() const;
        const AZStd::vector<AZStd::string> GetInputDeviceTypes() const;
        const AZStd::vector<AZStd::string> GetInputNamesBySelectedDevice() const;

        AZ::Crc32 OnDeviceSelected();

        //////////////////////////////////////////////////////////////////////////
        // AZ::GlobalInputRecordRequests::Handler
        void GatherEditableInputRecords(AZ::EditableInputRecords& outResults) override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::EditableInputRecord::Handler
        void SetInputRecord(const AZ::EditableInputRecord& newInputRecord) override;

#if defined(AZ_FRAMEWORK_INPUT_ENABLED)
        //////////////////////////////////////////////////////////////////////////
        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;
#else
        //////////////////////////////////////////////////////////////////////////
        // AZ::InputNotificationBus::Handler
        void OnNotifyInputEvent(const SInputEvent& completeInputEvent) override;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Tickbus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
#endif // defined(AZ_FRAMEWORK_INPUT_ENABLED)

        //////////////////////////////////////////////////////////////////////////
        // Non Reflected Data
        AZ::InputEventNotificationId m_outgoingBusId;
#if !defined(AZ_FRAMEWORK_INPUT_ENABLED)
        float m_lastKnownEventValue = 0.f;
#else
        bool m_wasPressed = false;
#endif // !defined(AZ_FRAMEWORK_INPUT_ENABLED)

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_eventValueMultiplier = 1.f;
        AZStd::string m_inputName = "";
        AZStd::string m_inputDeviceType = "";
        float m_deadZone = 0.2f;

    };
} // namespace Input
