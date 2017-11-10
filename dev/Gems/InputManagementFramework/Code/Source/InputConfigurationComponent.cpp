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
#include "InputConfigurationComponent.h"
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <InputRequestBus.h>
#include <PlayerProfileRequestBus.h>
#include <AzCore/Serialization/DataPatch.h>

namespace Input
{
    void InputConfigurationComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("InputConfigurationService"));
    }

    void InputConfigurationComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<InputSubComponent>()
                ->Version(1)
                ;

            serializeContext->Class<InputConfigurationComponent>()
                ->Version(2)
                ->Field("Input Event Bindings", &InputConfigurationComponent::m_inputEventBindingsAsset)
                ->Field("Input Contexts", &InputConfigurationComponent::m_inputContexts);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<InputSubComponent>
                    ("InputSubComponent", "The base class for all input handlers. Implementations will be found in other gems")
                    ;

                editContext->Class<InputConfigurationComponent>("Input",
                    "The Input component allows an entity to bind a set of inputs to an event by referencing a .inputbindings file")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Gameplay")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/InputConfig.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/InputConfig.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &InputConfigurationComponent::m_inputEventBindingsAsset, "Input to event bindings",
                    "Asset containing input to event binding information.")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, true)
                        ->Attribute("EditButton", "Editor/Icons/Assets/InputBindings")
                        ->Attribute("EditDescription", "Open in Input Bindings Editor")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &InputConfigurationComponent::m_inputContexts, "Input contexts", "These are the contexts valid for this input binding.  The default context is empty string")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, true)
                    ;
            }
        }
    }

    void InputConfigurationComponent::Init()
    {
    }

    void InputConfigurationComponent::Activate()
    {
        const char* profileName = nullptr;
        AZ::PlayerProfileRequestBus::BroadcastResult(profileName, &AZ::PlayerProfileRequests::GetCurrentProfileForCurrentUser);
        m_associatedProfileName = profileName;

        AZ::PlayerProfileNotificationBus::Handler::BusConnect();
        AZ::Data::AssetBus::Handler::BusConnect(m_inputEventBindingsAsset.GetId());

        // connect to all of our input contexts or "" if none are specified
        if (m_inputContexts.empty())
        {
            AZ::InputContextNotificationBus::MultiHandler::BusConnect(AZ::Crc32(""));
        }
        else
        {
            for (const AZStd::string& context : m_inputContexts)
            {
                AZ::InputContextNotificationBus::MultiHandler::BusConnect(AZ::Crc32(context.c_str()));
            }
        }
    }

    void InputConfigurationComponent::Deactivate()
    {
        AZ::InputContextNotificationBus::MultiHandler::BusDisconnect();
        AZ::PlayerProfileNotificationBus::Handler::BusDisconnect();
        AZ::Data::AssetBus::Handler::BusDisconnect();
        m_inputEventBindings.Deactivate(Input::ProfileId(m_associatedProfileName.c_str()));
    }

    void InputConfigurationComponent::OnProfileSaving()
    {
        if (m_inputEventBindingsAsset.IsReady())
        {
            AZ::DataPatch customBindingsPatch;
            customBindingsPatch.Create(&m_inputEventBindingsAsset.GetAs<InputEventBindingsAsset>()->m_bindings, &m_inputEventBindings);
            if (customBindingsPatch.IsData())
            {
                // store a non empty data patch using the asset id as the key
                AZStd::string assetIdAsString;
                m_inputEventBindingsAsset.GetId().ToString(assetIdAsString);
                bool isStored = false;
                AZ::PlayerProfileRequestBus::BroadcastResult(isStored, &AZ::PlayerProfileRequests::StoreData, assetIdAsString.c_str(), reinterpret_cast<void*>(&customBindingsPatch), AZ::AzTypeInfo<AZ::DataPatch>::Uuid(), nullptr);
            }
        }
    }

    void InputConfigurationComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (InputEventBindingsAsset* inputAsset = asset.GetAs<InputEventBindingsAsset>())
        {
            m_inputEventBindings = inputAsset->m_bindings;

            //patch the data
            void* rawData = nullptr;
            AZStd::string assetIdAsString;
            inputAsset->GetId().ToString(assetIdAsString);
            bool isLoaded = false;
            AZ::PlayerProfileRequestBus::BroadcastResult(isLoaded, &AZ::PlayerProfileRequests::RetrieveData, assetIdAsString.c_str(), &rawData, nullptr);
            AZ::DataPatch* customBindingsPatch = reinterpret_cast<AZ::DataPatch*>(rawData);
            if (customBindingsPatch && customBindingsPatch->IsValid())
            {
                if (InputEventBindings* patchedBindings = customBindingsPatch->Apply(&m_inputEventBindings))
                {
                    m_inputEventBindings = *patchedBindings;
                }
            }
        }
        else
        {
            AZ_Error("Input Configuration", false, "Input bindings asset is not the correct type.");
        }
    }

    void InputConfigurationComponent::OnInputContextActivated()
    {
#if defined(AZ_FRAMEWORK_INPUT_ENABLED)
        AZ::InputRequestBus::Broadcast(&AZ::InputRequests::RequestDeviceIndexMapping, AZ::Crc32(m_associatedProfileName.c_str()));
#else
        AZ::InputRequestBus::Broadcast(&AZ::InputRequests::RequestDeviceMapping, AZ::Crc32(m_associatedProfileName.c_str()), GetEntityId());
#endif // defined(AZ_FRAMEWORK_INPUT_ENABLED)
        m_inputEventBindings.Activate(Input::ProfileId(m_associatedProfileName.c_str()));
    }

    void InputConfigurationComponent::OnInputContextDeactivated()
    {
        m_inputEventBindings.Deactivate(Input::ProfileId(m_associatedProfileName.c_str()));
    }
}
