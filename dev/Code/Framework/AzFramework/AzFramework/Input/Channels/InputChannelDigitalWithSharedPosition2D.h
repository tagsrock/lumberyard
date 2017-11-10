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

#include <AzFramework/Input/Channels/InputChannelDigital.h>

#include <AzCore/std/smart_ptr/shared_ptr.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Class for input channels that emit one dimensional digital input value and share a position.
    //! Examples: mouse button
    class InputChannelDigitalWithSharedPosition2D : public InputChannelDigital
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(InputChannelDigitalWithSharedPosition2D, AZ::SystemAllocator, 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] inputChannelId Id of the input channel being constructed
        //! \param[in] inputDevice Input device that owns the input channel
        //! \param[in] sharedPositionData Shared ptr to the common position data
        explicit InputChannelDigitalWithSharedPosition2D(
            const AzFramework::InputChannelId& inputChannelId,
            const InputDevice& inputDevice,
            const AZStd::shared_ptr<InputChannel::PositionData2D>& sharedPositionData);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Disable copying
        AZ_DISABLE_COPY_MOVE(InputChannelDigitalWithSharedPosition2D);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Default destructor
        ~InputChannelDigitalWithSharedPosition2D() override = default;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Access to the shared position data
        //! \return Pointer to the shared position data
        const AzFramework::InputChannel::CustomData* GetCustomData() const override;

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Variables
        const AZStd::shared_ptr<InputChannel::PositionData2D> m_sharedPositionData; //!< Position data
    };
} // namespace LmbrCentral
