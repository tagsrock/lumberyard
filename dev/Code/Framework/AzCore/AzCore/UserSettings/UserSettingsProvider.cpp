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
#include <AzCore/UserSettings/UserSettingsProvider.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/GenericStreams.h>

namespace AZ
{
    //-----------------------------------------------------------------------------
    //-----------------------------------------------------------------------------
    static bool UserSettingsContainerVersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        if (classElement.GetVersion() == 2)
        {
            AZ::SerializeContext::DataElementNode& mapNode = classElement.GetSubElement(0);
            for (int i = 0; i < mapNode.GetNumSubElements(); ++i)
            {
                AZ::SerializeContext::DataElementNode& pairNode = mapNode.GetSubElement(i);
                AZ::SerializeContext::DataElementNode pointerNode = pairNode.GetSubElement(1);  // copy
                pairNode.GetSubElement(1) = AZ::SerializeContext::DataElementNode();
                pairNode.RemoveElement(1);
                pairNode.AddElement<AZStd::intrusive_ptr<UserSettings> >(context, "value2");
                pointerNode.SetName("element");
                pairNode.GetSubElement(1).AddElement(pointerNode);
            }
            return true;
        }

        return false; // just discard unknown versions
    }
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    void UserSettingsProvider::Activate(u32 bindToProviderId)
    {
        UserSettingsBus::Handler::BusConnect(bindToProviderId);
    }
    //-----------------------------------------------------------------------------
    void UserSettingsProvider::Deactivate()
    {
        UserSettingsBus::Handler::BusDisconnect();
        if (!m_settings.m_map.empty())
        {
            m_settings.m_map.clear();
        }
    }
    //-----------------------------------------------------------------------------
    AZStd::intrusive_ptr<UserSettings> UserSettingsProvider::FindUserSettings(u32 id)
    {
        UserSettingsContainer::MapType::const_iterator it = m_settings.m_map.find(id);
        if (it != m_settings.m_map.end())
        {
            return it->second;
        }
        return nullptr;
    }
    //-----------------------------------------------------------------------------
    void UserSettingsProvider::AddUserSettings(AZ::u32 id, UserSettings* settings)
    {
        AZ_Error("UserSettings", !settings->RTTI_GetType().IsNull(), "Attempting to add user setting type with invalid Uuid. You must reflect this type with the serialize context or we will not be able to save he settings!");
        m_settings.m_map.insert(AZStd::make_pair(id, settings));
    }
    //-----------------------------------------------------------------------------
    bool UserSettingsProvider::Load(const char* settingsPath, SerializeContext* sc)
    {
        bool settingsLoaded = false;
        if (IO::SystemFile::Exists(settingsPath))
        {
            IO::SystemFile settingsFile;
            settingsFile.Open(settingsPath, IO::SystemFile::SF_OPEN_READ_ONLY);
            AZ_Warning("UserSettings", settingsFile.IsOpen(), "UserSettingsProvider cannot open %s. Settings were not loaded!", settingsPath);
            if (settingsFile.IsOpen())
            {
                IO::SystemFileStream settingsFileStream(&settingsFile, false);
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&UserSettingsProvider::OnSettingLoaded, this, AZStd::placeholders::_1, AZStd::placeholders::_2, AZStd::placeholders::_3));
                ObjectStream::FilterDescriptor filter(nullptr, ObjectStream::FilterFlags::FILTERFLAG_IGNORE_UNKNOWN_CLASSES);
                settingsLoaded = ObjectStream::LoadBlocking(&settingsFileStream, *sc, readyCB, filter);
                settingsFile.Close();
            }
        }
        return settingsLoaded;
    }
    //-----------------------------------------------------------------------------
    bool UserSettingsProvider::Save(const char* settingsPath, SerializeContext* sc)
    {
        bool settingsSaved = false;
        AZStd::string tmpFullPath(settingsPath);
        tmpFullPath += ".tmp";
        IO::SystemFile settingsFile;
        settingsFile.Open(tmpFullPath.c_str(), IO::SystemFile::SF_OPEN_WRITE_ONLY | IO::SystemFile::SF_OPEN_CREATE);
        AZ_Error("UserSettings", settingsFile.IsOpen(), "UserSettingsProvider cannot write to temp file %s. Settings were not saved!", tmpFullPath.c_str());
        if (settingsFile.IsOpen())
        {
            IO::SystemFileStream settingsFileStream(&settingsFile, false);
            ObjectStream* objStream = ObjectStream::Create(&settingsFileStream, *sc, ObjectStream::ST_XML);
            bool writtenOk = objStream->WriteClass(&m_settings);
            bool streamOk = objStream->Finalize();
            settingsFile.Close();

            if (writtenOk && streamOk)
            {
                settingsSaved = IO::SystemFile::Rename(tmpFullPath.c_str(), settingsPath, true);
                AZ_Error("UserSettings", settingsSaved, "UserSettingsProvider cannot write to settings file %s. Settings were not saved!", settingsPath);
            }
        }
        return settingsSaved;
    }
    //-----------------------------------------------------------------------------
    void UserSettingsProvider::OnSettingLoaded(void* classPtr, const Uuid& classId, const SerializeContext* sc)
    {
        AZ_Assert(classPtr, "classPtr is nullptr!");
        AZ_Assert(classId == SerializeTypeInfo<UserSettingsContainer>::GetUuid(), "Bad class Id!, the class passed in is not a UserSettingsContainer!");
        UserSettingsContainer* container = sc->Cast<UserSettingsContainer*>(classPtr, classId);
        AZ_Assert(container, "Failed to cast classPtr to UserSettingsContainter*!");
        m_settings.m_map.swap(container->m_map);
        delete container;
    }
    //-----------------------------------------------------------------------------
    void UserSettingsProvider::Reflect(ReflectContext* reflection)
    {
        SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<UserSettingsContainer>()
                ->Version(3, &UserSettingsContainerVersionConverter)
                ->Field("Map", &UserSettingsContainer::m_map);
        }
    }
    //-----------------------------------------------------------------------------
}   // namespace AZ
