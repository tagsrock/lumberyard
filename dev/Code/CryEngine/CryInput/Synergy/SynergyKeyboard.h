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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#pragma once

#include <AzFramework/Input/System/InputSystemComponent.h>
#if defined(AZ_FRAMEWORK_INPUT_ENABLED)

#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>

#include <AzCore/std/parallel/mutex.h>

#include "RawInputNotificationBus_synergy.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace SynergyInput
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Synergy specific implementation for keyboard input devices. This should eventually be moved
    //! to a Gem, with InputDeviceMouseSynergy and RawInputNotificationsSynergy they both depend on.
    class InputDeviceKeyboardSynergy : public AzFramework::InputDeviceKeyboard::Implementation
                                     , public RawInputNotificationBusSynergy::Handler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(InputDeviceKeyboardSynergy, AZ::SystemAllocator, 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Custom factory create function
        //! \param[in] inputDevice Reference to the input device being implemented
        static Implementation* Create(AzFramework::InputDeviceKeyboard& inputDevice);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] inputDevice Reference to the input device being implemented
        InputDeviceKeyboardSynergy(AzFramework::InputDeviceKeyboard& inputDevice);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Destructor
        ~InputDeviceKeyboardSynergy() override;

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceKeyboard::Implementation::IsConnected
        bool IsConnected() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceKeyboard::Implementation::TextEntryStarted
        void TextEntryStarted(float activeTextFieldNormalizedBottomY = 0.0f) override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceKeyboard::Implementation::TextEntryStopped
        void TextEntryStopped() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceKeyboard::Implementation::TickInputDevice
        void TickInputDevice() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref RawInputNotificationsSynergy::OnRawKeyboardKeyDownEvent
        virtual void OnRawKeyboardKeyDownEvent(uint32_t scanCode, ModifierMask activeModifiers);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref RawInputNotificationsSynergy::OnRawKeyboardKeyUpEvent
        virtual void OnRawKeyboardKeyUpEvent(uint32_t scanCode, ModifierMask activeModifiers);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref RawInputNotificationsSynergy::OnRawKeyboardKeyRepeatEvent
        virtual void OnRawKeyboardKeyRepeatEvent(uint32_t scanCode, ModifierMask activeModifiers);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Thread safe method to queue raw key events to be processed in the main thread update
        //! \param[in] scanCode The scan code of the key
        //! \param[in] rawKeyState The raw key state
        void ThreadSafeQueueRawKeyEvent(uint32_t scanCode, bool rawKeyState);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Thread safe method to queue raw text events to be processed in the main thread update
        //! \param[in] textUTF8 The text to queue (encoded using UTF-8)
        void ThreadSafeQueueRawTextEvent(const AZStd::string& textUTF8);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Translate a key event to an ASCII character. This is required because synergy only sends
        //! raw key events, not translated text input. While we would ideally support the full range
        //! of UTF-8 text input, that is beyond the scope of this debug/development only class. Note
        //! that this function assumes an ANSI mechanical keyboard layout with a standard QWERTY key
        //! mapping, and will not produce correct results if used with other key layouts or mappings.
        //! \param[in] scanCode The scan code of the key
        //! \param[in] activeModifiers The bit mask of currently active modifier keys
        //! \return If the scan code and active modifiers produce a valid ASCII character
        char TranslateRawKeyEventToASCIIChar(uint32_t scanCode, ModifierMask activeModifiers);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Variables
        RawKeyEventQueueByIdMap      m_threadAwareRawKeyEventQueuesById;
        AZStd::mutex                 m_threadAwareRawKeyEventQueuesByIdMutex;

        AZStd::vector<AZStd::string> m_threadAwareRawTextEventQueue;
        AZStd::mutex                 m_threadAwareRawTextEventQueueMutex;

        bool                         m_hasTextEntryStarted;
    };
} // namespace SynergyInput

#elif defined(USE_SYNERGY_INPUT)

#include "InputDevice.h"

struct IInput;
class CSynergyContext;

class CSynergyKeyboard
    : public CInputDevice
{
public:
    CSynergyKeyboard(IInput& input, CSynergyContext* pContext);
    virtual ~CSynergyKeyboard();

    // IInputDevice overrides
    virtual int GetDeviceIndex() const { return eIDT_Keyboard; }
    virtual bool Init();
    virtual void Update(bool bFocus);
    virtual char GetInputCharAscii(const SInputEvent& event);
    // ~IInputDevice

private:
    _smart_ptr<CSynergyContext> m_pContext;
    void SetupKeys();
    void ProcessKey(uint32 key, bool bPressed, bool bRepeat, uint32 modificators);
    uint32 PackModificators(uint32 modificators);
    void TypeASCIIString(const char* pString);
};

#endif // USE_SYNERGY_INPUT
