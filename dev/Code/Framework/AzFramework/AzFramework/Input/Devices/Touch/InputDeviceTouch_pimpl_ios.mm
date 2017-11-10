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

#include <AzFramework/Input/Devices/Touch/InputDeviceTouch.h>
#include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_ios.h>
#include <AzFramework/Input/Buses/Requests/RawInputRequestBus_ios.h>

#include <UIKit/UIKit.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Platform specific implementation for ios touch input devices
    class InputDeviceTouchIos : public InputDeviceTouch::Implementation
                              , public RawInputNotificationBusIos::Handler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(InputDeviceTouchIos, AZ::SystemAllocator, 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] inputDevice Reference to the input device being implemented
        InputDeviceTouchIos(InputDeviceTouch& inputDevice);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Destructor
        ~InputDeviceTouchIos() override;

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceTouch::Implementation::IsConnected
        bool IsConnected() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceTouch::Implementation::TickInputDevice
        void TickInputDevice() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        ///@{
        //! Process raw touch events (that will always be dispatched on the main thread)
        //! \param[in] uiTouch The raw touch data
        void OnRawTouchEventBegan(const UITouch* uiTouch) override;
        void OnRawTouchEventMoved(const UITouch* uiTouch) override;
        void OnRawTouchEventEnded(const UITouch* uiTouch) override;
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Convenience function to initialize a raw touch event
        static RawTouchEvent InitRawTouchEvent(const UITouch* touch, uint32_t index);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! ios does not provide us with the index of a touch, but it does persist UITouch objects
        //! throughout a multi-touch sequence, so we can keep track of the touch indices ourselves.
        AZStd::vector<const UITouch*> m_activeTouches;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceTouch::Implementation* InputDeviceTouch::Implementation::Create(InputDeviceTouch& inputDevice)
    {
        return aznew InputDeviceTouchIos(inputDevice);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceTouchIos::InputDeviceTouchIos(InputDeviceTouch& inputDevice)
        : InputDeviceTouch::Implementation(inputDevice)
        , m_activeTouches()
    {
        // The maximum number of active touches tracked by ios is actually device dependent, and at
        // this time appears to be 5 for iPhone/iPodTouch and 11 for iPad. There is no API to query
        // or set this, but ten seems more than sufficient for most applications, especially games.
        m_activeTouches.resize(InputDeviceTouch::Touch::All.size());

        RawInputNotificationBusIos::Handler::BusConnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceTouchIos::~InputDeviceTouchIos()
    {
        RawInputNotificationBusIos::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceTouchIos::IsConnected() const
    {
        // Touch input is always available on ios
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceTouchIos::TickInputDevice()
    {
        // Pump the ios event loop to ensure that it has dispatched all input events. Other systems
        // (or other input devices) may also do this so some (or all) input events may have already
        // been dispatched, but they are queued until ProcessRawEventQueues is called below so that
        // all raw input events are processed at the same time every frame.
        RawInputRequestBusIos::Broadcast(&RawInputRequestsIos::PumpRawEventLoop);

        // Process the raw event queues once each frame
        ProcessRawEventQueues();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceTouchIos::OnRawTouchEventBegan(const UITouch* uiTouch)
    {
        for (uint32_t i = 0; i < InputDeviceTouch::Touch::All.size(); ++i)
        {
            // Use the first available index.
            if (m_activeTouches[i] == nullptr)
            {
                m_activeTouches[i] = uiTouch;

                const RawTouchEvent rawTouchEvent = InitRawTouchEvent(uiTouch, i);
                QueueRawTouchEvent(rawTouchEvent);

                break;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceTouchIos::OnRawTouchEventMoved(const UITouch* uiTouch)
    {
        for (uint32_t i = 0; i < InputDeviceTouch::Touch::All.size(); ++i)
        {
            if (m_activeTouches[i] == uiTouch)
            {
                const RawTouchEvent rawTouchEvent = InitRawTouchEvent(uiTouch, i);
                QueueRawTouchEvent(rawTouchEvent);

                break;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceTouchIos::OnRawTouchEventEnded(const UITouch* uiTouch)
    {
        for (uint32_t i = 0; i < InputDeviceTouch::Touch::All.size(); ++i)
        {
            if (m_activeTouches[i] == uiTouch)
            {
                const RawTouchEvent rawTouchEvent = InitRawTouchEvent(uiTouch, i);
                QueueRawTouchEvent(rawTouchEvent);

                m_activeTouches[i] = nullptr;

                break;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceTouch::Implementation::RawTouchEvent InputDeviceTouchIos::InitRawTouchEvent(
        const UITouch* touch,
        uint32_t index)
    {
        CGPoint touchLocation = [touch locationInView: touch.view];
        CGSize viewSize = [touch.view bounds].size;

        const float normalizedLocationX = touchLocation.x / viewSize.width;
        const float normalizedLocationY = touchLocation.y / viewSize.height;
        float pressure = touch.force / touch.maximumPossibleForce;

        RawTouchEvent::State state = RawTouchEvent::State::Began;
        switch (touch.phase)
        {
            case UITouchPhaseBegan:
            {
                state = RawTouchEvent::State::Began;
            }
            break;
            case UITouchPhaseMoved:
            case UITouchPhaseStationary: // Should never happen but if so treat it the same as moved
            {
                state = RawTouchEvent::State::Moved;
            }
            break;
            case UITouchPhaseEnded:
            case UITouchPhaseCancelled: // Should never happen but if so treat it the same as ended
            {
                state = RawTouchEvent::State::Ended;
                pressure = 0.0f;
            }
            break;
        }

        return RawTouchEvent(normalizedLocationX,
                             normalizedLocationY,
                             pressure,
                             index,
                             state);
    }
} // namespace AzFramework
