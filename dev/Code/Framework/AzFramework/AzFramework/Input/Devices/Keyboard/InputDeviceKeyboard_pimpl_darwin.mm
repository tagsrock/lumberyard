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

#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_darwin.h>
#include <AzFramework/Input/Buses/Requests/RawInputRequestBus_darwin.h>

#include <AppKit/NSEvent.h>
#include <AppKit/NSTextView.h>
#include <AppKit/NSWindow.h>
#include <Carbon/Carbon.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface KeyboardTextView : NSTextView
{
    AzFramework::InputDeviceKeyboard::Implementation* m_inputDevice;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (id)initWithInputDevice: (AzFramework::InputDeviceKeyboard::Implementation*)inputDevice;

////////////////////////////////////////////////////////////////////////////////////////////////////
- (BOOL)shouldChangeTextInRange: (NSRange) range
              replacementString: (NSString*) string;

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)deleteBackward: (id) sender;

@end // KeyboardTextView Interface

////////////////////////////////////////////////////////////////////////////////////////////////////
@implementation KeyboardTextView

////////////////////////////////////////////////////////////////////////////////////////////////////
- (id)initWithInputDevice: (AzFramework::InputDeviceKeyboard::Implementation*)inputDevice
{
    if ((self = [super initWithFrame: CGRectZero]))
    {
        self->m_inputDevice = inputDevice;
    }

    return self;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (BOOL)shouldChangeTextInRange: (NSRange) range
              replacementString: (NSString*) string
{
    // Only queue text events if this application's window has focus
    if (NSApplication.sharedApplication.mainWindow.keyWindow)
    {
        const AZStd::string textUTF8 = string.UTF8String;
        m_inputDevice->QueueRawTextEvent(textUTF8);
    }

    // Return false so that the text field itself does not update.
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)deleteBackward: (id) sender
{
    // Only queue text events if this application's window has focus
    if (NSApplication.sharedApplication.mainWindow.keyWindow)
    {
        const AZStd::string textUTF8 = "\b";
        m_inputDevice->QueueRawTextEvent(textUTF8);
    }
}
@end // KeyboardTextView Implementation

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
    using namespace AzFramework;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Table of key ids indexed by their osx key code
    const AZStd::array<const InputChannelId*, 128> InputChannelIdByKeyCodeTable =
    {{
        &InputDeviceKeyboard::Key::AlphanumericA,         // 0x00 kVK_ANSI_A
        &InputDeviceKeyboard::Key::AlphanumericS,         // 0x01 kVK_ANSI_S
        &InputDeviceKeyboard::Key::AlphanumericD,         // 0x02 kVK_ANSI_D
        &InputDeviceKeyboard::Key::AlphanumericF,         // 0x03 kVK_ANSI_F
        &InputDeviceKeyboard::Key::AlphanumericH,         // 0x04 kVK_ANSI_H
        &InputDeviceKeyboard::Key::AlphanumericG,         // 0x05 kVK_ANSI_G
        &InputDeviceKeyboard::Key::AlphanumericZ,         // 0x06 kVK_ANSI_Z
        &InputDeviceKeyboard::Key::AlphanumericX,         // 0x07 kVK_ANSI_X
        &InputDeviceKeyboard::Key::AlphanumericC,         // 0x08 kVK_ANSI_C
        &InputDeviceKeyboard::Key::AlphanumericV,         // 0x09 kVK_ANSI_V
        &InputDeviceKeyboard::Key::SupplementaryISO,      // 0x0A kVK_ISO_Section
        &InputDeviceKeyboard::Key::AlphanumericB,         // 0x0B kVK_ANSI_B
        &InputDeviceKeyboard::Key::AlphanumericQ,         // 0x0C kVK_ANSI_Q
        &InputDeviceKeyboard::Key::AlphanumericW,         // 0x0D kVK_ANSI_W
        &InputDeviceKeyboard::Key::AlphanumericE,         // 0x0E kVK_ANSI_E
        &InputDeviceKeyboard::Key::AlphanumericR,         // 0x0F kVK_ANSI_R

        &InputDeviceKeyboard::Key::AlphanumericY,         // 0x10 kVK_ANSI_Y
        &InputDeviceKeyboard::Key::AlphanumericT,         // 0x11 kVK_ANSI_T
        &InputDeviceKeyboard::Key::Alphanumeric1,         // 0x12 kVK_ANSI_1
        &InputDeviceKeyboard::Key::Alphanumeric2,         // 0x13 kVK_ANSI_2
        &InputDeviceKeyboard::Key::Alphanumeric3,         // 0x14 kVK_ANSI_3
        &InputDeviceKeyboard::Key::Alphanumeric4,         // 0x15 kVK_ANSI_4
        &InputDeviceKeyboard::Key::Alphanumeric6,         // 0x16 kVK_ANSI_6
        &InputDeviceKeyboard::Key::Alphanumeric5,         // 0x17 kVK_ANSI_5
        &InputDeviceKeyboard::Key::PunctuationEquals,     // 0x18 kVK_ANSI_Equal
        &InputDeviceKeyboard::Key::Alphanumeric9,         // 0x19 kVK_ANSI_9
        &InputDeviceKeyboard::Key::Alphanumeric7,         // 0x1A kVK_ANSI_7
        &InputDeviceKeyboard::Key::PunctuationHyphen,     // 0x1B kVK_ANSI_Minus
        &InputDeviceKeyboard::Key::Alphanumeric8,         // 0x1C kVK_ANSI_8
        &InputDeviceKeyboard::Key::Alphanumeric0,         // 0x1D kVK_ANSI_0
        &InputDeviceKeyboard::Key::PunctuationBracketR,   // 0x1E kVK_ANSI_RightBracket
        &InputDeviceKeyboard::Key::AlphanumericO,         // 0x1F kVK_ANSI_O

        &InputDeviceKeyboard::Key::AlphanumericU,         // 0x20 kVK_ANSI_U
        &InputDeviceKeyboard::Key::PunctuationBracketL,   // 0x21 kVK_ANSI_LeftBracket
        &InputDeviceKeyboard::Key::AlphanumericI,         // 0x22 kVK_ANSI_I
        &InputDeviceKeyboard::Key::AlphanumericP,         // 0x23 kVK_ANSI_P
        &InputDeviceKeyboard::Key::EditEnter,             // 0x24 kVK_Return
        &InputDeviceKeyboard::Key::AlphanumericL,         // 0x25 kVK_ANSI_L
        &InputDeviceKeyboard::Key::AlphanumericJ,         // 0x26 kVK_ANSI_J
        &InputDeviceKeyboard::Key::PunctuationApostrophe, // 0x27 kVK_ANSI_Quote
        &InputDeviceKeyboard::Key::AlphanumericK,         // 0x28 kVK_ANSI_K
        &InputDeviceKeyboard::Key::PunctuationSemicolon,  // 0x29 kVK_ANSI_Semicolon
        &InputDeviceKeyboard::Key::PunctuationBackslash,  // 0x2A kVK_ANSI_Backslash
        &InputDeviceKeyboard::Key::PunctuationComma,      // 0x2B kVK_ANSI_Comma
        &InputDeviceKeyboard::Key::PunctuationSlash,      // 0x2C kVK_ANSI_Slash
        &InputDeviceKeyboard::Key::AlphanumericN,         // 0x2D kVK_ANSI_N
        &InputDeviceKeyboard::Key::AlphanumericM,         // 0x2E kVK_ANSI_M
        &InputDeviceKeyboard::Key::PunctuationPeriod,     // 0x2F kVK_ANSI_Period

        &InputDeviceKeyboard::Key::EditTab,               // 0x30 kVK_Tab
        &InputDeviceKeyboard::Key::EditSpace,             // 0x31 kVK_Space
        &InputDeviceKeyboard::Key::PunctuationTilde,      // 0x32 kVK_ANSI_Grave
        &InputDeviceKeyboard::Key::EditBackspace,         // 0x33 kVK_Delete
        nullptr,                                          // 0x34 ?
        &InputDeviceKeyboard::Key::Escape,                // 0x35 kVK_Escape
        &InputDeviceKeyboard::Key::ModifierSuperR,        // 0x36 kVK_RightCommand
        &InputDeviceKeyboard::Key::ModifierSuperL,        // 0x37 kVK_Command
        &InputDeviceKeyboard::Key::ModifierShiftL,        // 0x38 kVK_Shift
        &InputDeviceKeyboard::Key::EditCapsLock,          // 0x39 kVK_CapsLock
        &InputDeviceKeyboard::Key::ModifierAltL,          // 0x3A kVK_Option
        &InputDeviceKeyboard::Key::ModifierControlL,      // 0x3B kVK_Control
        &InputDeviceKeyboard::Key::ModifierShiftR,        // 0x3C kVK_RightShift
        &InputDeviceKeyboard::Key::ModifierAltR,          // 0x3D kVK_RightOption
        &InputDeviceKeyboard::Key::ModifierControlR,      // 0x3E kVK_RightControl
        nullptr,                                          // 0x3F kVK_Function

        &InputDeviceKeyboard::Key::Function17,            // 0x40 kVK_F17
        &InputDeviceKeyboard::Key::NumPadDecimal,         // 0x41 kVK_ANSI_KeypadDecimal
        nullptr,                                          // 0x42 ?
        &InputDeviceKeyboard::Key::NumPadMultiply,        // 0x43 kVK_ANSI_KeypadMultiply
        nullptr,                                          // 0x44 ?
        &InputDeviceKeyboard::Key::NumPadAdd,             // 0x45 kVK_ANSI_KeypadPlus
        nullptr,                                          // 0x46 ?
        &InputDeviceKeyboard::Key::NumLock,               // 0x47 kVK_ANSI_KeypadClear
        nullptr,                                          // 0x48 kVK_VolumeUp
        nullptr,                                          // 0x49 kVK_VolumeDown
        nullptr,                                          // 0x4A kVK_Mute
        &InputDeviceKeyboard::Key::NumPadDivide,          // 0x4B kVK_ANSI_KeypadDivide
        &InputDeviceKeyboard::Key::NumPadEnter,           // 0x4C kVK_ANSI_KeypadEnter
        nullptr,                                          // 0x4D ?
        &InputDeviceKeyboard::Key::NumPadSubtract,        // 0x4E kVK_ANSI_KeypadMinus
        &InputDeviceKeyboard::Key::Function18,            // 0x4F kVK_F18
  
        &InputDeviceKeyboard::Key::Function19,            // 0x50 kVK_F19
        nullptr,                                          // 0x51 kVK_ANSI_KeypadEquals
        &InputDeviceKeyboard::Key::NumPad0,               // 0x52 kVK_ANSI_Keypad0
        &InputDeviceKeyboard::Key::NumPad1,               // 0x53 kVK_ANSI_Keypad1
        &InputDeviceKeyboard::Key::NumPad2,               // 0x54 kVK_ANSI_Keypad2
        &InputDeviceKeyboard::Key::NumPad3,               // 0x55 kVK_ANSI_Keypad3
        &InputDeviceKeyboard::Key::NumPad4,               // 0x56 kVK_ANSI_Keypad4
        &InputDeviceKeyboard::Key::NumPad5,               // 0x57 kVK_ANSI_Keypad5
        &InputDeviceKeyboard::Key::NumPad6,               // 0x58 kVK_ANSI_Keypad6
        &InputDeviceKeyboard::Key::NumPad7,               // 0x59 kVK_ANSI_Keypad7
        &InputDeviceKeyboard::Key::Function20,            // 0x5A kVK_F20
        &InputDeviceKeyboard::Key::NumPad8,               // 0x5B kVK_ANSI_Keypad8
        &InputDeviceKeyboard::Key::NumPad9,               // 0x5C kVK_ANSI_Keypad9
        nullptr,                                          // 0x5D kVK_JIS_Yen
        nullptr,                                          // 0x5E kVK_JIS_Underscore
        nullptr,                                          // 0x5F kVK_JIS_KeypadComma

        &InputDeviceKeyboard::Key::Function05,            // 0x60 kVK_F5
        &InputDeviceKeyboard::Key::Function06,            // 0x61 kVK_F6
        &InputDeviceKeyboard::Key::Function07,            // 0x62 kVK_F7
        &InputDeviceKeyboard::Key::Function03,            // 0x63 kVK_F3
        &InputDeviceKeyboard::Key::Function08,            // 0x64 kVK_F8
        &InputDeviceKeyboard::Key::Function09,            // 0x65 kVK_F9
        nullptr,                                          // 0x66 kVK_JIS_Eisu
        &InputDeviceKeyboard::Key::Function11,            // 0x67 kVK_F11
        nullptr,                                          // 0x68 kVK_JIS_Kana
        &InputDeviceKeyboard::Key::Function13,            // 0x69 kVK_F13
        &InputDeviceKeyboard::Key::Function16,            // 0x6A kVK_F16
        &InputDeviceKeyboard::Key::Function14,            // 0x6B kVK_F14
        nullptr,                                          // 0x6C ?
        &InputDeviceKeyboard::Key::Function10,            // 0x6D kVK_F10
        nullptr,                                          // 0x6E ?
        &InputDeviceKeyboard::Key::Function12,            // 0x6F kVK_F12

        nullptr,                                          // 0x70 ?
        &InputDeviceKeyboard::Key::Function15,            // 0x71 kVK_F15
        nullptr,                                          // 0x72 kVK_Help
        &InputDeviceKeyboard::Key::NavigationHome,        // 0x73 kVK_Home
        &InputDeviceKeyboard::Key::NavigationPageUp,      // 0x74 kVK_PageUp
        &InputDeviceKeyboard::Key::NavigationDelete,      // 0x75 kVK_ForwardDelete
        &InputDeviceKeyboard::Key::Function04,            // 0x76 kVK_F4
        &InputDeviceKeyboard::Key::NavigationEnd,         // 0x77 kVK_End
        &InputDeviceKeyboard::Key::Function02,            // 0x78 kVK_F2
        &InputDeviceKeyboard::Key::NavigationPageDown,    // 0x79 kVK_PageDown
        &InputDeviceKeyboard::Key::Function01,            // 0x7A kVK_F1
        &InputDeviceKeyboard::Key::NavigationArrowLeft,   // 0x7B kVK_LeftArrow
        &InputDeviceKeyboard::Key::NavigationArrowRight,  // 0x7C kVK_RightArrow
        &InputDeviceKeyboard::Key::NavigationArrowDown,   // 0x7D kVK_DownArrow
        &InputDeviceKeyboard::Key::NavigationArrowUp,     // 0x7E kVK_UpArrow
        nullptr                                           // 0x7F ?
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // NSEventType enum constant names were changed in macOS 10.12, but our min-spec is still 10.10
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101200 // __MAC_10_12 may not be defined by all earlier sdks
    static const NSEventType NSEventTypeKeyDown         = NSKeyDown;
    static const NSEventType NSEventTypeKeyUp           = NSKeyUp;
    static const NSEventType NSEventTypeFlagsChanged    = NSFlagsChanged;

    // kVK_RightCommand was also added in macOS 10.12
    static const int kVK_RightCommand                   = 0x36;
#endif // __MAC_OS_X_VERSION_MAX_ALLOWED < 101200
}

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Platform specific implementation for osx keyboard input devices
    class InputDeviceKeyboardOsx : public InputDeviceKeyboard::Implementation
                                 , public RawInputNotificationBusOsx::Handler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(InputDeviceKeyboardOsx, AZ::SystemAllocator, 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] inputDevice Reference to the input device being implemented
        InputDeviceKeyboardOsx(InputDeviceKeyboard& inputDevice);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Destructor
        ~InputDeviceKeyboardOsx() override;

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
        //! \ref AzFramework::RawInputNotificationsOsx::OnRawInputEvent
        void OnRawInputEvent(const NSEvent* nsEvent) override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Update the application's first responder based on whether text input should be active
        void UpdateFirstResponder();

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Convenience function to queue standard key events processed in OnRawInputEvent
        //! \param[in] keyCode The darwin specific key code
        //! \param[in] keyState The key state (down or up)
        void QueueRawStandardKeyEvent(AZ::u32 keyCode, bool keyState);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Convenience function to queue modifier key events processed in OnRawInputEvent
        //! \param[in] keyCode The darwin specific key code
        //! \param[in] modifierFlags The event's modifier flags
        void QueueRawModifierKeyEvent(AZ::u32 keyCode, AZ::u32 modifierFlags);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! A custom NSTextView used to receive text events
        KeyboardTextView* m_textView = nullptr;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Has text entry been started?
        bool m_hasTextEntryStarted = false;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Does the application's main window currently have focus?
        bool m_hasFocus = false;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceKeyboard::Implementation* InputDeviceKeyboard::Implementation::Create(InputDeviceKeyboard& inputDevice)
    {
        return aznew InputDeviceKeyboardOsx(inputDevice);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceKeyboardOsx::InputDeviceKeyboardOsx(InputDeviceKeyboard& inputDevice)
        : InputDeviceKeyboard::Implementation(inputDevice)
        , m_textView(nullptr)
        , m_hasTextEntryStarted(false)
        , m_hasFocus(false)
    {
        // Create a KeyboardTextView that we can call makeFirstResponder on to enable text input.
        m_textView = [[KeyboardTextView alloc] initWithInputDevice: this];

        // Add something to the text view so delete works.
        m_textView.string = @" ";

        // Add the text view to the application's root view.
        [NSApplication.sharedApplication.mainWindow.contentView addSubview: m_textView];

        RawInputNotificationBusOsx::Handler::BusConnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceKeyboardOsx::~InputDeviceKeyboardOsx()
    {
        RawInputNotificationBusOsx::Handler::BusDisconnect();

        if (m_textView)
        {
            [m_textView removeFromSuperview];
            [m_textView release];
            m_textView = nullptr;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceKeyboardOsx::IsConnected() const
    {
        // If necessary we may be able to determine the connected state using the I/O Kit HIDManager:
        // https://developer.apple.com/library/content/documentation/DeviceDrivers/Conceptual/HID/new_api_10_5/tn2187.html
        //
        // Doing this may allow (and perhaps even force) us to distinguish between multiple physical
        // devices of the same type. But given support for multiple keyboards is a fairly niche need
        // we'll keep things simple (for now) and assume there's one (and only 1) keyboard connected
        // at all times. In practice this means if multiple physical keyboards are connected we will
        // process input from them all, but treat all the input as if it comes from the same device.
        //
        // If it becomes necessary to determine connected states of keyboard devices (and/or support
        // distinguishing between multiple physical keyboards) we should implement this function and
        // call BroadcastInputDeviceConnectedEvent/BroadcastInputDeviceDisconnectedEvent when needed.
        //
        // Note that doing so will require modifying how we create and manage keyboard input devices
        // in InputSystemComponent/InputSystemComponentWin so we create multiple InputDeviceKeyboard
        // instances (somehow associating each with a raw input device id), along with modifying the
        // InputDeviceKeyboardOsx::OnRawInputEvent function to filter incoming events by this raw id.
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::TextEntryStarted(float /*activeTextFieldNormalizedBottomY*/)
    {
        m_hasTextEntryStarted = true;
        UpdateFirstResponder();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::TextEntryStopped()
    {
        m_hasTextEntryStarted = false;
        UpdateFirstResponder();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::TickInputDevice()
    {
        // We should probably only do this when we detect that the application loses or gains focus,
        // but there may be other ways we can lose first responder status so we'll check every frame.
        UpdateFirstResponder();

        // Pump the osx event loop to ensure that it has dispatched all input events. Other systems
        // (or other input devices) may also do this so some (or all) input events may have already
        // been dispatched, but they are queued until ProcessRawEventQueues is called below so that
        // all raw input events are processed at the same time every frame.
        RawInputRequestBusOsx::Broadcast(&RawInputRequestsOsx::PumpRawEventLoop);

        const bool hadFocus = m_hasFocus;
        m_hasFocus = NSApplication.sharedApplication.mainWindow.keyWindow;
        if (m_hasFocus)
        {
            // Process raw event queues once each frame while this application's window has focus
            ProcessRawEventQueues();
        }
        else if (hadFocus)
        {
            // This application's window no longer has focus, process any events that are queued,
            // before resetting the state of all this input device's associated input channels.
            ProcessRawEventQueues();
            ResetInputChannelStates();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::OnRawInputEvent(const NSEvent* nsEvent)
    {
        if (!NSApplication.sharedApplication.mainWindow.keyWindow)
        {
            return;
        }

        switch (nsEvent.type)
        {
            case NSEventTypeKeyDown:
            {
                // We can ignore repeat events here...
                if (!nsEvent.isARepeat)
                {
                    QueueRawStandardKeyEvent(nsEvent.keyCode, true);
                }

                // ...but they should still generate text.
            #if !defined(ALWAYS_DISPATCH_KEYBOARD_TEXT_INPUT)
                if (m_hasTextEntryStarted)
            #endif // defined(ALWAYS_DISPATCH_KEYBOARD_TEXT_INPUT)
                {
                    [m_textView interpretKeyEvents: [NSArray arrayWithObject: nsEvent]];
                }
            }
            break;
            case NSEventTypeKeyUp:
            {
                QueueRawStandardKeyEvent(nsEvent.keyCode, false);
            }
            break;
            case NSEventTypeFlagsChanged:
            {
                QueueRawModifierKeyEvent(nsEvent.keyCode, nsEvent.modifierFlags);
            }
            break;
            default:
            {
                // Ignore
            }
            break;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::UpdateFirstResponder()
    {
    #if defined(ALWAYS_DISPATCH_KEYBOARD_TEXT_INPUT)
        const bool shouldBeFirstResponder = true;
    #else
        const bool shouldBeFirstResponder = m_hasTextEntryStarted;
    #endif // defined(ALWAYS_DISPATCH_KEYBOARD_TEXT_INPUT)
        const bool isFirstResponder = NSApplication.sharedApplication.mainWindow.firstResponder == m_textView;
        if (shouldBeFirstResponder && !isFirstResponder)
        {
            [NSApplication.sharedApplication.mainWindow makeFirstResponder: m_textView];
        }
        else if (!shouldBeFirstResponder && isFirstResponder)
        {
            [NSApplication.sharedApplication.mainWindow makeFirstResponder: nil];
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::QueueRawStandardKeyEvent(AZ::u32 keyCode, bool keyState)
    {
        if ((keyCode == kVK_ISO_Section || keyCode == kVK_ANSI_Grave) &&
            KBGetLayoutType(LMGetKbdType()) == kKeyboardISO)
        {
            // osx swaps these two key codes for keyboards that use an ISO mechanical layout,
            // so we have to swap them back.
            keyCode = (kVK_ISO_Section + kVK_ANSI_Grave) - keyCode;
        }

        const InputChannelId* channelId = (keyCode < InputChannelIdByKeyCodeTable.size()) ?
                                          InputChannelIdByKeyCodeTable[keyCode] : nullptr;
        if (channelId)
        {
            QueueRawKeyEvent(*channelId, keyState);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceKeyboardOsx::QueueRawModifierKeyEvent(AZ::u32 keyCode, AZ::u32 modifierFlags)
    {
        const InputChannelId* channelId = (keyCode < InputChannelIdByKeyCodeTable.size()) ?
                                          InputChannelIdByKeyCodeTable[keyCode] : nullptr;
        if (!channelId)
        {
            return;
        }

        switch (keyCode)
        {
            case kVK_Option:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICELALTKEYMASK);
            }
            break;
            case kVK_RightOption:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICERALTKEYMASK);
            }
            break;
            case kVK_Control:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICELCTLKEYMASK);
            }
            break;
            case kVK_RightControl:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICERCTLKEYMASK);
            }
            break;
            case kVK_Shift:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICELSHIFTKEYMASK);
            }
            break;
            case kVK_RightShift:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICERSHIFTKEYMASK);
            }
            break;
            case kVK_Command:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICELCMDKEYMASK);
            }
            break;
            case kVK_RightCommand:
            {
                QueueRawKeyEvent(*channelId, modifierFlags & NX_DEVICERCMDKEYMASK);
            }
            break;
            case kVK_CapsLock:
            {
                // Caps lock is annoying in that it only reports events on key up (when the state of
                // the key changes), and never on key down, making it unlike all other keyboard keys.
                // While not ideal, simply sending both 'down' and 'up' events in succession when we
                // detect a change to the caps lock modifier works well enough, although it means we
                // will never be able to detect if the caps lock key is being held down.
                QueueRawKeyEvent(*channelId, true);
                QueueRawKeyEvent(*channelId, false);
            }
            break;
            default:
            {
                // Not a supported modifier key
            }
            break;
        }
    }
} // namespace AzFramework
