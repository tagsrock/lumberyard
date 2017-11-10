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
#include "SequenceAgent.h"
#include <AzCore/Math/Color.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>

namespace LmbrCentral
{
    /////////////////////////////////////////////////////////////////////////////////////////////
    void SequenceAgent::CacheAllVirtualPropertiesFromBehaviorContext(AZ::Entity* entity)
    {
        AZ::BehaviorContext* behaviorContext = nullptr;
        EBUS_EVENT_RESULT(behaviorContext, AZ::ComponentApplicationBus, GetBehaviorContext);

        // Loop through all components on this entity and register all that have BehaviorContext virtual properties
        const AZ::Entity::ComponentArrayType& entityComponents = entity->GetComponents();
        m_addressToBehaviorVirtualPropertiesMap.clear();

        for (AZ::Component* component : entityComponents)
        {
            auto findClassIter = behaviorContext->m_typeToClassMap.find(GetComponentTypeUuid(*component));
            if (findClassIter != behaviorContext->m_typeToClassMap.end())
            {            
                AZ::BehaviorClass* behaviorClass = findClassIter->second;
                // go through all ebuses for this class and find all virtual properties
                for (auto reqBusName = behaviorClass->m_requestBuses.begin(); reqBusName != behaviorClass->m_requestBuses.end(); reqBusName++)
                {
                    auto findBusIter = behaviorContext->m_ebuses.find(*reqBusName);
                    if (findBusIter != behaviorContext->m_ebuses.end())
                    {
                        AZ::BehaviorEBus* behaviorEbus = findBusIter->second;
                        for (auto virtualPropertyIter = behaviorEbus->m_virtualProperties.begin(); virtualPropertyIter != behaviorEbus->m_virtualProperties.end(); virtualPropertyIter++)
                        {
                            LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress   address(component->GetId(), virtualPropertyIter->first);
                            m_addressToBehaviorVirtualPropertiesMap[address] = &virtualPropertyIter->second;
                        }
                    }
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    AZ::Uuid SequenceAgent::GetVirtualPropertyTypeId(const LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress& animatableAddress) const
    {
        AZ::Uuid retTypeUuid = AZ::Uuid::CreateNull();

        auto findIter = m_addressToBehaviorVirtualPropertiesMap.find(animatableAddress);
        if (findIter != m_addressToBehaviorVirtualPropertiesMap.end())
        {
            retTypeUuid = findIter->second->m_getter->m_event->GetResult()->m_typeId;
        }
        return retTypeUuid;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    bool SequenceAgent::SetAnimatedPropertyValue(AZ::EntityId entityId, const LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress& animatableAddress, const LmbrCentral::SequenceComponentRequests::AnimatedValue& value)
    {
        bool changed = false;
        const AZ::Uuid propertyTypeId = GetVirtualPropertyTypeId(animatableAddress);

        auto findIter = m_addressToBehaviorVirtualPropertiesMap.find(animatableAddress);
        if (findIter != m_addressToBehaviorVirtualPropertiesMap.end())
        {
            if (propertyTypeId == AZ::Vector3::TYPEINFO_Uuid())
            {
                AZ::Vector3 vector3Value(.0f, .0f, .0f);
                value.GetValue(vector3Value);               // convert the generic value to a Vector3
                findIter->second->m_setter->m_event->Invoke(entityId, vector3Value);
                changed = true;
            }
            else if (propertyTypeId == AZ::Color::TYPEINFO_Uuid())
            {
                AZ::Vector3 vector3Value(.0f, .0f, .0f);
                value.GetValue(vector3Value);               // convert the generic value to a Vector3
                AZ::Color colorValue(AZ::Color::CreateFromVector3(vector3Value));
                findIter->second->m_setter->m_event->Invoke(entityId, colorValue);
                changed = true;
            }
            else if (propertyTypeId == AZ::Quaternion::TYPEINFO_Uuid())
            {
                AZ::Quaternion quaternionValue(AZ::Quaternion::CreateIdentity());
                value.GetValue(quaternionValue);
                findIter->second->m_setter->m_event->Invoke(entityId, quaternionValue);
                changed = true;
            }
            else if (propertyTypeId == AZ::AzTypeInfo<bool>::Uuid())
            {
                bool boolValue = true;
                value.GetValue(boolValue);                  // convert the generic value to a bool
                findIter->second->m_setter->m_event->Invoke(entityId, boolValue);
                changed = true;
            }
            else
            {
                // fall-through default is to cast to float
                float floatValue = .0f;
                value.GetValue(floatValue);                 // convert the generic value to a float
                findIter->second->m_setter->m_event->Invoke(entityId, floatValue);
                changed = true;
            }
        }
        return changed;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    void SequenceAgent::GetAnimatedPropertyValue(LmbrCentral::SequenceComponentRequests::AnimatedValue& returnValue, AZ::EntityId entityId, const LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress& animatableAddress)
    {
        const AZ::Uuid propertyTypeId = GetVirtualPropertyTypeId(animatableAddress);

        auto findIter = m_addressToBehaviorVirtualPropertiesMap.find(animatableAddress);
        if (findIter != m_addressToBehaviorVirtualPropertiesMap.end())
        {
            if (propertyTypeId == AZ::Vector3::TYPEINFO_Uuid())
            {
                AZ::Vector3 vector3Value(AZ::Vector3::CreateZero());
                findIter->second->m_getter->m_event->InvokeResult(vector3Value, entityId);
                returnValue.SetValue(vector3Value);
            }
            else if (propertyTypeId == AZ::Color::TYPEINFO_Uuid())
            {
                AZ::Color colorValue(AZ::Color::CreateZero());
                findIter->second->m_getter->m_event->InvokeResult(colorValue, entityId);
                returnValue.SetValue((AZ::Vector3)colorValue);
            }
            else if (propertyTypeId == AZ::Quaternion::TYPEINFO_Uuid())
            {
                AZ::Quaternion quaternionValue(AZ::Quaternion::CreateIdentity());
                findIter->second->m_getter->m_event->InvokeResult(quaternionValue, entityId);
                returnValue.SetValue(quaternionValue);
            }
            else if (propertyTypeId == AZ::AzTypeInfo<bool>::Uuid())
            {
                bool boolValue;
                findIter->second->m_getter->m_event->InvokeResult(boolValue, entityId);
                returnValue.SetValue(boolValue);
            }
            else
            {
                // fall-through default is to cast to float
                float floatValue = .0f;
                findIter->second->m_getter->m_event->InvokeResult(floatValue, entityId);
                returnValue.SetValue(floatValue);
            }
        }
    }
}// namespace LmbrCentral
