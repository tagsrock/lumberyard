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

#include <AzFramework/Input/Buses/Requests/InputSystemRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputSystemCursorRequestBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// TEMP: On almost all platforms, only one system can process raw input. So while CryInput now uses
// AzFramework/Input internally by default, this define can be removed to switch back to the legacy
// CryInput system. In a near future release we will assume it to be defined and remove it entirely
#define AZ_FRAMEWORK_INPUT_ENABLED

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    class InputDeviceGamepad;
    class InputDeviceKeyboard;
    class InputDeviceMotion;
    class InputDeviceMouse;
    class InputDeviceTouch;
    class InputDeviceVirtualKeyboard;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! This system component manages instances of the default input devices supported by the engine.
    //! Other systems/modules/gems/games are free to create additional input device instances of any
    //! type; this system component manages devices that are supported "out of the box", which other
    //! systems (and most games) will expect to be available for platforms where they are supported.
    class InputSystemComponent : public AZ::Component
                               , public AZ::TickBus::Handler
                               , public InputSystemRequestBus::Handler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // AZ::Component Setup
        AZ_COMPONENT(InputSystemComponent, "{CAF3A025-FAC9-4537-B99E-0A800A9326DF}")

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AZ::ComponentDescriptor::GetProvidedServices
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AZ::ComponentDescriptor::GetIncompatibleServices
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AZ::ComponentDescriptor::Reflect
        static void Reflect(AZ::ReflectContext* reflection);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        InputSystemComponent();

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Destructor
        ~InputSystemComponent() override;

    protected:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AZ::Component::Activate
        void Activate() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AZ::Component::Deactivate
        void Deactivate() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AZ::TickEvents::OnTick
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTimePoint) override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputSystemRequests::TickInput
        void TickInput() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputSystemRequests::RecreateEnabledInputDevices
        void RecreateEnabledInputDevices() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Create enabled input devices
        void CreateEnabledInputDevices();

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Destroy enabled input devices
        void DestroyEnabledInputDevices();

    private:

        InputSystemComponent(const InputSystemComponent&) = delete;
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Input Device Variables
        AZStd::vector<AZStd::unique_ptr<InputDeviceGamepad>> m_gamepads;        //!< Game-pad devices
        AZStd::unique_ptr<InputDeviceKeyboard>               m_keyboard;        //!< Keyboard device
        AZStd::unique_ptr<InputDeviceMotion>                 m_motion;          //!< Motion device
        AZStd::unique_ptr<InputDeviceMouse>                  m_mouse;           //!< Mouse device
        AZStd::unique_ptr<InputDeviceTouch>                  m_touch;           //!< Touch device
        AZStd::unique_ptr<InputDeviceVirtualKeyboard>        m_virtualKeyboard; //!< Virtual keyboard device

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Serialized Variables
        AZ::u32 m_gamepadsEnabled;        //!< The number of enabled game-pads
        bool    m_keyboardEnabled;        //!< Is the keyboard enabled?
        bool    m_motionEnabled;          //!< Is motion enabled?
        bool    m_mouseEnabled;           //!< Is the mouse enabled?
        bool    m_touchEnabled;           //!< Is touch enabled?
        bool    m_virtualKeyboardEnabled; //!< Is the virtual keyboard enabled?

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Other Variables
        bool m_currentlyUpdatingInputDevices;   //!< Are we currently updating input devices?
        bool m_recreateInputDevicesAfterUpdate; //!< Should we recreate devices after update?

    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Base class for platform specific implementations of the input system component
        class Implementation
        {
        public:
            ////////////////////////////////////////////////////////////////////////////////////////
            //! Factory create method
            //! \param[in] inputSystem Reference to the input system component
            static Implementation* Create(InputSystemComponent& inputSystem);

            ////////////////////////////////////////////////////////////////////////////////////////
            //! Constructor
            //! \param[in] inputSystem Reference to the input system component
            Implementation(InputSystemComponent& inputSystem) : m_inputSystemComponent(inputSystem) {}

            ////////////////////////////////////////////////////////////////////////////////////////
            // Disable copying
            AZ_DISABLE_COPY_MOVE(Implementation);

            ////////////////////////////////////////////////////////////////////////////////////////
            //! Default destructor
            virtual ~Implementation() = default;

        protected:
            ////////////////////////////////////////////////////////////////////////////////////////
            // Variables
            InputSystemComponent& m_inputSystemComponent; //!< Reference to the input system
        };

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Private pointer to the platform specific implementation
        AZStd::unique_ptr<Implementation> m_pimpl;
    };
} // namespace AzFramework
