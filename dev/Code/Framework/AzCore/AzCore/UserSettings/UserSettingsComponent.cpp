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
#ifndef AZ_UNITY_BUILD

#include <AzCore/UserSettings/UserSettingsComponent.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Crc.h>

namespace AZ
{
    //-----------------------------------------------------------------------------
    UserSettingsComponent::UserSettingsComponent(u32 providerId)
        : m_providerId(providerId)
    {
    }

    //-----------------------------------------------------------------------------
    UserSettingsComponent::~UserSettingsComponent()
    {
    }

    //-----------------------------------------------------------------------------
    void UserSettingsComponent::Activate()
    {
        Load();
        m_provider.Activate(m_providerId);

        UserSettingsComponentRequestBus::Handler::BusConnect();
    }

    //-----------------------------------------------------------------------------
    void UserSettingsComponent::Deactivate()
    {
        UserSettingsComponentRequestBus::Handler::BusDisconnect();

        Save();
        m_provider.Deactivate();
    }

    //-----------------------------------------------------------------------------
    void UserSettingsComponent::Load()
    {
        AZStd::string settingsPath;
        EBUS_EVENT_RESULT(settingsPath, UserSettingsFileLocatorBus, ResolveFilePath, m_providerId);
        //AZ_Warning("UserSettings", !settingsPath.empty(), "Failed to resolve file path for settings provider %u. Make sure there is a handler active for the UserSettingsFileLocatorBus.", static_cast<u32>(m_providerId));
        SerializeContext* serializeContext = nullptr;
        EBUS_EVENT_RESULT(serializeContext, ComponentApplicationBus, GetSerializeContext);
        AZ_Warning("UserSettings", serializeContext != nullptr, "Failed to retrieve the serialization context. User settings cannot be loaded.");
        if (!settingsPath.empty() && serializeContext != nullptr)
        {
            m_provider.Load(settingsPath.c_str(), serializeContext);
        }
    }

    //-----------------------------------------------------------------------------
    void UserSettingsComponent::Save()
    {
        AZStd::string settingsPath;
        EBUS_EVENT_RESULT(settingsPath, UserSettingsFileLocatorBus, ResolveFilePath, m_providerId);
        //AZ_Warning("UserSettings", !settingsPath.empty(), "Failed to resolve file path for settings provider %u. Make sure there is a handler active for the UserSettingsFileLocatorBus.", static_cast<u32>(m_providerId));
        SerializeContext* serializeContext = nullptr;
        EBUS_EVENT_RESULT(serializeContext, ComponentApplicationBus, GetSerializeContext);
        AZ_Warning("UserSettings", serializeContext != nullptr, "Failed to retrieve the serialization context. User settings cannot be stored.");
        if (!settingsPath.empty() && serializeContext != nullptr)
        {
            m_provider.Save(settingsPath.c_str(), serializeContext);
        }
    }

    //-----------------------------------------------------------------------------
    void UserSettingsComponent::GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("UserSettingsService", 0xa0eadff5));
    }

    //-----------------------------------------------------------------------------
    void UserSettingsComponent::GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("MemoryService", 0x5c4d473c));
    }

    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    //-----------------------------------------------------------------------------
    void UserSettingsComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            UserSettingsProvider::Reflect(serializeContext);

            serializeContext->Class<UserSettingsComponent, AZ::Component>()
                ->Version(3)
                ->Field("ProviderId", &UserSettingsComponent::m_providerId)
                ;

            if (EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<UserSettingsComponent>(
                    "User Settings", "Provides userdata storage for all system components")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Editor")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                    //->DataElement(AZ::Edit::UIHandlers::Default,&UserSettingsComponent::m_settingsPath,"File path","File location for the user settings file")
                    //    ->Attribute("Folder",AZ_CRC("Relative", 0x6e5b37d9)); // add a relative to the application folder (you can't get out of it)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &UserSettingsComponent::m_providerId, "ProviderId", "The settings group this provider with handle.")
                        ->EnumAttribute(UserSettings::CT_LOCAL, "Local")
                        ->EnumAttribute(UserSettings::CT_GLOBAL, "Global")
                    ;
            }
        }
    }
    //-----------------------------------------------------------------------------
}   // namespace AZ

using namespace AZ;


#endif  // AZ_UNITY_BUILD
