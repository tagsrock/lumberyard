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
#include "StdAfx.h"
#include <StaticDataManager.h>
#include <CSVStaticData.h>
#include <StaticDataInterface.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/IO/SystemFile.h>
#include <IStaticDataMonitor.h>

#include <StaticData/StaticDataBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>

#include <sstream>
#include <string>

#define CSV_TAG ".csv"

namespace CloudCanvas
{
    namespace StaticData
    {

        StaticDataManager::StaticDataManager()
        {
            AddExtensionType(CSV_TAG, StaticDataType::CSV);
        }

        StaticDataManager::~StaticDataManager()
        {

        }

        // Update Bus
        class BehaviorStaticDataUpdateNotificationBusHandler : public StaticDataUpdateBus::Handler, public AZ::BehaviorEBusHandler
        {
        public:
            AZ_EBUS_BEHAVIOR_BINDER(BehaviorStaticDataUpdateNotificationBusHandler, "{7828EEFD-79D8-4C60-85CC-3AEE43F1F1F6}", AZ::SystemAllocator,
                TypeReloaded);

            void TypeReloaded(const AZStd::string& outputFile) override
            {
                Call(FN_TypeReloaded, outputFile);
            }

        };

        //
        // Component Implementations
        //
        void StaticDataManager::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("StaticData"));
        }

        void StaticDataManager::Init()
        {

        }

        void StaticDataManager::Activate()
        {
            StaticDataRequestBus::Handler::BusConnect(m_entity->GetId());
            CloudCanvas::DynamicContent::DynamicContentUpdateBus::Handler::BusConnect();
        }

        void StaticDataManager::Deactivate()
        {
            StaticDataRequestBus::Handler::BusDisconnect();
            CloudCanvas::DynamicContent::DynamicContentUpdateBus::Handler::BusDisconnect();
        }

        void StaticDataManager::Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<StaticDataManager>()
                    ->Version(1);

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<StaticDataManager>("StaticData", "CloudCanvas StaticData Component")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Cloud Gem Framework")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/StaticData.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/StaticData.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"));
                }
            }

            AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
            if (behaviorContext)
            {
                behaviorContext->EBus<StaticDataRequestBus>("StaticDataRequestBus")
                    ->Event("GetIntValue", &StaticDataRequestBus::Events::GetIntValue)
                    ->Event("GetStrValue", &StaticDataRequestBus::Events::GetStrValue)
                    ->Event("GetDoubleValue", &StaticDataRequestBus::Events::GetDoubleValue)
                    ->Event("LoadRelativeFile", &StaticDataRequestBus::Events::LoadRelativeFile)
                    ;

                behaviorContext->EBus<StaticDataUpdateBus>("StaticDataUpdateBus")
                    ->Handler<BehaviorStaticDataUpdateNotificationBusHandler>()
                    ;
            }
        }

        bool StaticDataManager::LoadAll()
        {

            EBUS_EVENT(CloudCanvas::StaticData::StaticDataMonitorRequestBus, RemoveAll);

            {
                AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());
                m_data.clear();
            }

            LoadAssetAndUserDirectory(csvDir, CSV_TAG, StaticDataType::CSV);
            return true;
        }

        bool StaticDataManager::ReloadTagType(const char* tagName)
        {
            return ReloadType(tagName);
        }

        bool StaticDataManager::ReloadType(const char* tagName)
        {
            // Currently this just reloads everything
            LoadAll();

            return true;
        }

        StaticDataInterfacePtr StaticDataManager::GetDataType(const char* tagName) const
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());

            auto dataIter = m_data.find(tagName);
            if (dataIter != m_data.end())
            {
                return dataIter->second;
            }
            return StaticDataInterfacePtr{};
        }

        StaticDataTypeList StaticDataManager::GetDataTypeList()
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());

            StaticDataTypeList returnList;

            for (auto thisType : m_data)
            {
                returnList.push_back(thisType.first);
            }
            return returnList;
        }

        ReturnInt StaticDataManager::GetIntValue(const char* tagName, const char* structName, const char* fieldName, bool& wasSuccess)
        {
            return GetIntValueInternal(tagName, structName, fieldName, wasSuccess);
        }

        ReturnInt StaticDataManager::GetIntValueInternal(const char* tagName, const char* structName, const char* fieldName, bool& wasSuccess) const
        {
            StaticDataInterfacePtr thisBuffer = GetDataType(tagName);
            if (thisBuffer)
            {
                return thisBuffer->GetIntValue(structName, fieldName, wasSuccess);
            }
            wasSuccess = false;
            return 0;
        }

        ReturnDouble StaticDataManager::GetDoubleValue(const char* tagName, const char* structName, const char* fieldName, bool& wasSuccess)
        {
            return GetDoubleValueInternal(tagName, structName, fieldName, wasSuccess);
        }

        ReturnDouble StaticDataManager::GetDoubleValueInternal(const char* tagName, const char* structName, const char* fieldName, bool& wasSuccess) const
        {
            StaticDataInterfacePtr thisBuffer = GetDataType(tagName);
            if (thisBuffer)
            {
                return thisBuffer->GetDoubleValue(structName, fieldName, wasSuccess);
            }
            wasSuccess = false;
            return 0;
        }

        ReturnStr StaticDataManager::GetStrValue(const char* tagName, const char* structName, const char* fieldName, bool& wasSuccess)
        {
            return GetStrValueInternal(tagName, structName, fieldName, wasSuccess);
        }

        ReturnStr StaticDataManager::GetStrValueInternal(const char* tagName, const char* structName, const char* fieldName, bool& wasSuccess) const
        {
            StaticDataInterfacePtr thisBuffer = GetDataType(tagName);
            if (thisBuffer)
            {
                return thisBuffer->GetStrValue(structName, fieldName,  wasSuccess);
            }
            wasSuccess = false;
            return "";
        }

        StaticDataManager::StaticDataInterfacePtrInternal StaticDataManager::CreateInterface(StaticDataType dataType, const char* initData, const char* tagName)
        {
            switch (dataType)
            {
            case StaticDataType::CSV:
            {
                StaticDataInterfacePtrInternal thisInterface = AZStd::make_shared<CSVStaticData>();
                thisInterface->LoadData(initData);
                SetInterface(tagName, thisInterface);
                return thisInterface;
            }
            break;
            }
            return StaticDataInterfacePtrInternal{};
        }

        void StaticDataManager::SetInterface(const char* tagName, StaticDataInterfacePtrInternal someInterface)
        {
            {
                AZStd::lock_guard<AZStd::mutex> thisLock(GetDataMutex());
                m_data[tagName] = someInterface;
            }
            EBUS_EVENT(StaticDataUpdateBus, TypeReloaded, AZStd::string{ tagName });
        }

        void StaticDataManager::RemoveInterface(const char* tagName)
        {
            SetInterface(tagName, StaticDataInterfacePtrInternal{});
        }

        void StaticDataManager::LoadRelativeFile(const char* relativeFile)
        {
            // Currently our file names act as our data types, but are stripped of their extensions

            AZStd::string tagStr(GetTagFromFile(relativeFile));

            AZ::IO::HandleType readHandle = gEnv->pCryPak->FOpen(relativeFile, "rt");

            if (readHandle != AZ::IO::InvalidHandle)
            {
                size_t fileSize = gEnv->pCryPak->FGetSize(readHandle);
                if (fileSize > 0)
                {

                    AZStd::string fileBuf;
                    fileBuf.resize(fileSize);

                    size_t read = gEnv->pCryPak->FRead(fileBuf.data(), fileSize, readHandle);

                    CreateInterface(GetTypeFromFile(relativeFile), fileBuf.data(), tagStr.c_str());
                }
                gEnv->pCryPak->FClose(readHandle);
            }
            else
            {
                RemoveInterface(tagStr.c_str());
            }
        }

        void StaticDataManager::LoadAssetAndUserDirectory(const char* dirName, const char* extensionType, StaticDataType dataType)
        {
            AZStd::string writeFolder{ "@assets@/" };
            writeFolder += dirName;

            LoadDirectoryDataType(writeFolder.c_str(), extensionType, dataType);

            writeFolder = "@user@/";
            writeFolder += dirName;

            LoadDirectoryDataType(writeFolder.c_str(), extensionType, dataType);
        }

        void StaticDataManager::LoadDirectoryDataType(const char* dirName, const char* extensionType, StaticDataType dataType)
        {
            AZStd::string sanitizedString = ResolveAndSanitize(dirName);
            MakeEndInSlash(sanitizedString);

            if (!sanitizedString.length())
            {
                return;
            }

            EBUS_EVENT(CloudCanvas::StaticData::StaticDataMonitorRequestBus, AddPath, sanitizedString, false);

            AddExtensionType(extensionType, dataType);
            AddExtensionForDirectory(sanitizedString.c_str(), extensionType);

            StaticDataFileSet dataSet;
            GetFilesForExtension(sanitizedString.c_str(), extensionType, dataSet);

            for (auto thisFile : dataSet)
            {
                LoadRelativeFile(thisFile.c_str());
            }
        }

        void StaticDataManager::GetFilesForExtension(const char* dirName, const char* extensionType, StaticDataFileSet& addSet) const
        {
            AZStd::string sanitizedString = ResolveAndSanitize(dirName);
            if (!sanitizedString.length())
            {
                return;
            }

            if (gEnv->pCryPak)
            {

                _finddata_t fd;
                intptr_t handle = gEnv->pCryPak->FindFirst((sanitizedString + "*" + extensionType).c_str(), &fd);

                if (handle < 0)
                {
                    return;
                }

                do
                {

                    addSet.insert(sanitizedString + fd.name);

                } while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);
                gEnv->pCryPak->FindClose(handle);
            }
        }

        AZStd::string StaticDataManager::ResolveAndSanitize(const char* dirName) const
        {
            char resolvedGameFolder[AZ_MAX_PATH_LEN] = { 0 };
            if (!gEnv->pFileIO->ResolvePath(dirName, resolvedGameFolder, AZ_MAX_PATH_LEN))
            {
                return{};
            }
            AZStd::string sanitizedString;

            EBUS_EVENT_RESULT(sanitizedString, CloudCanvas::StaticData::StaticDataMonitorRequestBus, GetSanitizedName, resolvedGameFolder);

            return sanitizedString.length() ? sanitizedString : resolvedGameFolder;
        }

        StaticDataFileSet StaticDataManager::GetFilesForDirectory(const char* dirName)
        {
            StaticDataFileSet fileSet;

            AZStd::string sanitizedString = ResolveAndSanitize(dirName);
            if (!sanitizedString.length())
            {
                return fileSet;
            }

            StaticDataExtensionList extensionList = GetExtensionsForDirectory(sanitizedString.c_str());

            for (auto fileExtension : extensionList)
            {
                GetFilesForExtension(sanitizedString.c_str(), fileExtension.c_str(), fileSet);
            }
            return fileSet;
        }

        void StaticDataManager::AddExtensionType(const char* extensionStr, StaticDataType dataType)
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(m_extensionToTypeMutex);
            m_extensionToTypeMap[GetExtensionFromFile(extensionStr)] = dataType;
        }

        void StaticDataManager::AddExtensionForDirectory(const char* dirName, const char* extensionName)
        {
            AZStd::string extensionStr{ GetExtensionFromFile(extensionName) };
            AZStd::string dirStr{ dirName };
            MakeEndInSlash(dirStr);

            AZStd::lock_guard<AZStd::mutex> thisLock(m_directoryToExtensionMutex);

            auto entryIter = m_directoryToExtensionMap.find(dirStr);
            if (entryIter == m_directoryToExtensionMap.end())
            {
                m_directoryToExtensionMap[dirStr] = StaticDataExtensionList{ extensionStr };
            }
            else
            {

                for (auto thisEntry : entryIter->second)
                {
                    if (thisEntry == extensionStr)
                    {
                        return;
                    }
                }
                entryIter->second.push_back(extensionStr);
            }
        }

        bool StaticDataManager::HasDirectoryAndExtension(const AZStd::string& directoryString, const AZStd::string& extensionString) const
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(m_directoryToExtensionMutex);

            auto entryIter = m_directoryToExtensionMap.find(directoryString);
            if (entryIter != m_directoryToExtensionMap.end())
            {
                for (auto thisEntry : entryIter->second)
                {
                    if (thisEntry == extensionString)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        StaticDataExtensionList StaticDataManager::GetExtensionsForDirectory(const char* dirName) const
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(m_directoryToExtensionMutex);

            auto entryIter = m_directoryToExtensionMap.find(dirName);
            if (entryIter == m_directoryToExtensionMap.end())
            {
                return StaticDataExtensionList{ };
            }

            return entryIter->second;
        }

        StaticDataManager::StaticDataType StaticDataManager::GetTypeFromExtension(const AZStd::string& extensionStr) const
        {
            AZStd::lock_guard<AZStd::mutex> thisLock(m_extensionToTypeMutex);
            auto findIter = m_extensionToTypeMap.find(extensionStr);
            if (findIter != m_extensionToTypeMap.end())
            {
                return findIter->second;
            }
            return StaticDataType::NONE;
        }
        AZStd::string StaticDataManager::GetExtensionFromFile(const char* fileName) const
        {
            AZStd::string fileStr(fileName);
            AZStd::size_t findPos = fileStr.find_last_of('.');

            if (findPos == AZStd::string::npos)
            {
                return{};
            }
            return fileStr.substr(findPos + 1);
        }
        StaticDataManager::StaticDataType StaticDataManager::GetTypeFromFile(const char* fileStr) const
        {
            return GetTypeFromExtension(GetExtensionFromFile(fileStr).c_str());
        }
        StaticDataTagType StaticDataManager::GetTagFromFile(const char* fileName) const
        {
            AZStd::string fileStr(fileName);
            AZStd::size_t findPos = fileStr.find_last_of('\\');

            if (findPos != AZStd::string::npos)
            {
                fileStr = fileStr.substr(findPos + 1);
            }
            findPos = fileStr.find_last_of('/');

            if (findPos != AZStd::string::npos)
            {
                fileStr = fileStr.substr(findPos + 1);
            }
            findPos = fileStr.find_last_of('.');
            if (findPos != AZStd::string::npos)
            {
                fileStr = fileStr.substr(0, findPos);
            }
            return fileStr;
        }

        void StaticDataManager::NewContentReady(const AZStd::string& filePath)
        {
            if (filePath.length())
            {
                if (IsLoadedData(filePath))
                {
                    LoadRelativeFile(filePath.c_str());
                }
            }
        }

        void StaticDataManager::NewPakContentReady(const AZStd::string& pakFileName)
        {

        }

        void StaticDataManager::RequestsCompleted()
        {

        }

        void StaticDataManager::MakeEndInSlash(AZStd::string& someString)
        {
            if (someString.length() && someString[someString.length() - 1] != '/')
            {
                someString += '/';
            }
        }

        AZStd::string StaticDataManager::GetDirectoryFromFullPath(const AZStd::string& filePath)
        {
            auto lastPos = filePath.find_last_of('/');

            if (lastPos != AZStd::string::npos)
            {
                AZStd::string directoryPath = filePath.substr(0, lastPos);
                MakeEndInSlash(directoryPath);
                return directoryPath;
            }
            return{};
        }
        // Is this a file we should care about reloading?
        bool StaticDataManager::IsLoadedData(const AZStd::string& filePath) const
        {
            AZStd::string directoryPath = GetDirectoryFromFullPath(filePath);

            AZStd::string extensionType = GetExtensionFromFile(filePath.c_str());

            return HasDirectoryAndExtension(directoryPath, extensionType);
        }
    }
}