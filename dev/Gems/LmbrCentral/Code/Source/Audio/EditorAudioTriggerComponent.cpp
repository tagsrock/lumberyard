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
#include "EditorAudioTriggerComponent.h"

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace LmbrCentral
{
    //=========================================================================
    void EditorAudioTriggerComponent::Reflect(AZ::ReflectContext* context)
    {
        auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<EditorAudioTriggerComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Play Trigger", &EditorAudioTriggerComponent::m_defaultPlayTrigger)
                ->Field("Stop Trigger", &EditorAudioTriggerComponent::m_defaultStopTrigger)
                ->Field("Plays Immediately", &EditorAudioTriggerComponent::m_playsImmediately)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorAudioTriggerComponent>("Audio Trigger", "The Audio Trigger component provides basic play and stop features so that you can set up Audio Translation Layer (ATL) play and stop triggers that can be executed on demand")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Audio")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/AudioTrigger")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/AudioTrigger.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement("AudioControl", &EditorAudioTriggerComponent::m_defaultPlayTrigger, "Default 'play' Trigger", "The default ATL Trigger control used by 'Play'")
                    ->DataElement("AudioControl", &EditorAudioTriggerComponent::m_defaultStopTrigger, "Default 'stop' Trigger", "The default ATL Trigger control used by 'Stop'")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorAudioTriggerComponent::m_playsImmediately, "Plays immediately", "Play when this component is Activated")
                    ;
            }
        }
    }

    //=========================================================================
    EditorAudioTriggerComponent::EditorAudioTriggerComponent()
    {
        m_defaultPlayTrigger.m_propertyType = AzToolsFramework::AudioPropertyType::Trigger;
        m_defaultStopTrigger.m_propertyType = AzToolsFramework::AudioPropertyType::Trigger;
    }

    //=========================================================================
    void EditorAudioTriggerComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<AudioTriggerComponent>(m_defaultPlayTrigger.m_controlName, m_defaultStopTrigger.m_controlName, m_playsImmediately);
    }

} // namespace LmbrCentral
