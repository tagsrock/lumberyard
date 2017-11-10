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

#include <AzCore/Math/Crc.h>
#include <AzCore/std/hash.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Class that identifies a specific input device
    class InputDeviceId
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] name Name of the input device
        //! \param[in] index Index of the input device (optional)
        explicit InputDeviceId(const char* name = "", AZ::u32 index = 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Default copying
        AZ_DEFAULT_COPY(InputDeviceId);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Default destructor
        ~InputDeviceId() = default;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Access to the input device's name
        //! \return Name of the input device
        const char* GetName() const;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Access to the crc32 of the input device's name
        //! \return crc32 of the input device name
        const AZ::Crc32& GetNameCrc32() const;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Access to the input device's index. Does not correspond to the local player id assigned
        //! to an input device (see InputDevice::GetAssignedLocalPlayerId). For use differentiating
        //! between multiple instances of the same device - regardless of whether a local player id
        //! has been assigned to it. For example, by default the engine supports up to four gamepad
        //! devices that are created at startup using indicies 0->3. As gamepads connect/disconnect
        //! at runtime, we automatically assign the appropriate (system dependent) local player id.
        //! \return Index of the input device
        AZ::u32 GetIndex() const;

        ////////////////////////////////////////////////////////////////////////////////////////////
        ///@{
        //! Equality comparison operator
        //! \param[in] other Another instance of the class to compare for equality
        bool operator==(const InputDeviceId& other) const;
        bool operator!=(const InputDeviceId& other) const;
        ///@}

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Variables
        const char* m_name;  //!< Name of the input device
        AZ::Crc32   m_crc32; //!< Crc32 of the input device
        AZ::u32     m_index; //!< Index of the input device
    };
} // namespace AzFramework

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AZStd
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Hash structure specialization for InputDeviceId
    template<> struct hash<AzFramework::InputDeviceId>
    {
        inline size_t operator()(const AzFramework::InputDeviceId& inputDeviceId) const
        {
            size_t hashValue = inputDeviceId.GetNameCrc32();
            AZStd::hash_combine(hashValue, inputDeviceId.GetIndex());
            return hashValue;
        }
    };
} // namespace AZStd
