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
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/utils.h>
#include <AzCore/Component/EntityId.h>
#include <AzFramework/Input/System/InputSystemComponent.h>
#include <InputTypes.h>

namespace AZ
{
    class EntityId;

    /** 
     * This class is used both as the profile id for customizing a particular
     * input as well as the argument for what the new profile will be.
    */
    struct EditableInputRecord
    {
        AZ_TYPE_INFO(EditableInputRecord, "{86B216E5-D40D-474A-8EE7-629591EC75EE}");
        EditableInputRecord() = default;
        EditableInputRecord(Input::ProfileId profile, Input::ProcessedEventName eventGroup, AZStd::string deviceName, AZStd::string inputName)
            : m_profile(profile)
            , m_eventGroup(eventGroup)
            , m_deviceName(deviceName)
            , m_inputName(inputName)
        {}

        bool operator==(const EditableInputRecord& rhs) const
        {
            return m_profile == rhs.m_profile
                && m_eventGroup == rhs.m_eventGroup
                && m_deviceName == rhs.m_deviceName
                && m_inputName == rhs.m_inputName;
        }

        Input::ProfileId m_profile;
        Input::ProcessedEventName m_eventGroup;
        AZStd::string m_deviceName;
        AZStd::string m_inputName;
    };
    using EditableInputRecords = AZStd::vector<EditableInputRecord>;

    /**
     * With this bus you can request a list of all editable inputs
    */
    class GlobalInputRecordRequests
        : public AZ::EBusTraits
    {
    public:
        virtual void GatherEditableInputRecords(EditableInputRecords& outResults) = 0;
    };
    using GlobalInputRecordRequestBus = AZ::EBus<GlobalInputRecordRequests>;

    /**
     * With this bus you can change an input binding at run time
     * This uses the EditableInputRecord as the bus id which will combine
     * all of the necessary information to uniquely identify the input you
     * are trying to Set
    */ 
    class InputRecordRequests
        : public AZ::EBusTraits
    {
    public:
        static const EBusAddressPolicy AddressPolicy = EBusAddressPolicy::ById;
        typedef EditableInputRecord BusIdType;
        virtual void SetInputRecord(const EditableInputRecord& newInputRecord) = 0;
    };
    using InputRecordRequestBus = AZ::EBus<InputRecordRequests>;

    /**
     * With this bus you can query for registered device names, as well as their
     * registered inputs
    */ 
    class InputRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~InputRequests() = default;

        // ToDo: When CryInput and the AZ_FRAMEWORK_INPUT_ENABLED #define are removed,
        // the following functions should be deleted and the rest of the file moved to:
        // Gems/InputManagementFramework/Code/Include/InputManagementFramework/InputRequestBus.h
#if !defined(AZ_FRAMEWORK_INPUT_ENABLED)
        /**
         * Get a list of names of devices registered with the input system
         * DEPRECATED: Use AzFramework::InputDeviceRequests::GetInputDeviceIds
        */
        virtual const AZStd::vector<AZStd::string> GetRegisteredDeviceList() = 0;

        /**
         * Get a list of names of inputs registered on a given device
         * DEPRECATED: Use AzFramework::InputDeviceRequests::GetInputChannelIds
        */
        virtual const AZStd::vector<AZStd::string> GetInputListByDevice(const AZStd::string& deviceName) = 0;

        /**
        * This will request a device mapping to your profileId from the input system
        */
        virtual void RequestDeviceMapping(const Input::ProfileId& profileId, const AZ::EntityId& requester) = 0;

        /**
        * This will get the profile associated with a device index
        */
        virtual Input::ProfileId GetProfileIdByDeviceIndex(AZ::u8 deviceIndex) = 0;
#else
        /**
        * This will request a mapping from a profileId to a device index. The mapped index is returned.
        */
        virtual AZ::u8 RequestDeviceIndexMapping(const Input::ProfileId& profileId) = 0;

        /**
        * This will get the device index mapped to a profileId, or 0 if none is mapped
        */
        virtual AZ::u8 GetMappedDeviceIndex(const Input::ProfileId& profileId) = 0;
#endif // !defined(AZ_FRAMEWORK_INPUT_ENABLED)

        /**
        * This will clear all profile<->device mappings
        */
        virtual void ClearAllDeviceMappings() = 0;
        
        /**
        * This will push the desired context onto the top of the input context stack making it active
        */
        virtual void PushContext(const AZStd::string&) = 0;

        /**
        * This will pop the top context from the input context stack, the new top will become the active context
        */
        virtual void PopContext() = 0;

        /**
        * This will pop all contexts from the input context stack.  The stack will be empty and the default "" context will be active
        */
        virtual void PopAllContexts() = 0;

        /**
        * This will return the name of the top of the input context stack
        */
        virtual AZStd::string GetCurrentContext() = 0;

        /**
        * This will return a list of the current context stack names.  
        * The first element in the list is the bottom of the stack and the last element is the top of the input context stack
        */
        virtual AZStd::vector<AZStd::string> GetContextStack() = 0;
    };

    using InputRequestBus = AZ::EBus<InputRequests>;
} // namespace AZ


namespace AZStd
{
    template <>
    struct hash <AZ::EditableInputRecord>
    {
        inline size_t operator()(const AZ::EditableInputRecord& record) const
        {
            size_t retVal = static_cast<size_t>(record.m_eventGroup);
            AZStd::hash_combine(retVal, record.m_deviceName);
            AZStd::hash_combine(retVal, record.m_inputName);
            return retVal;
        }
    };
}
