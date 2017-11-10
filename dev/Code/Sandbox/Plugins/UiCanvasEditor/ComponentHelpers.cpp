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
#include "stdafx.h"

#include "EditorCommon.h"
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/API/EntityCompositionRequestBus.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <LyShine/Bus/UiSystemBus.h>
#include <LyShine/Bus/UiLayoutManagerBus.h>

namespace ComponentHelpers
{
    AZStd::string GetComponentIconPath(const AZ::SerializeContext::ClassData* componentClass)
    {
        AZStd::string iconPath = "Editor/Icons/Components/Component_Placeholder.png";
   
        auto editorElementData = componentClass->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData);
        if (auto iconAttribute = editorElementData->FindAttribute(AZ::Edit::Attributes::Icon))
        {
            if (auto iconAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(iconAttribute))
            {
                AZStd::string iconAttributeValue = iconAttributeData->Get(nullptr);
                if (!iconAttributeValue.empty())
                {
                    iconPath = AZStd::move(iconAttributeValue);
                }
            }
        }

        // use absolute path if possible
        AZStd::string iconFullPath;
        bool pathFound = false;
        using AssetSysReqBus = AzToolsFramework::AssetSystemRequestBus;
        AssetSysReqBus::BroadcastResult(pathFound, &AssetSysReqBus::Events::GetFullSourcePathFromRelativeProductPath, iconPath, iconFullPath);

        if (pathFound)
        {
            iconPath = iconFullPath;
        }

        return iconPath;
    }

    const char* GetFriendlyComponentName(const AZ::SerializeContext::ClassData& classData)
    {
        return classData.m_editData
                ? classData.m_editData->m_name
                : classData.m_name;
    }

    bool AppearsInUIComponentMenu(const AZ::SerializeContext::ClassData& classData)
    {
        return AzToolsFramework::AppearsInAddComponentMenu(classData, AZ_CRC("UI", 0x27ff46b0));
    }

    bool IsAddableByUser(const AZ::SerializeContext::ClassData* classData)
    {
        if (classData->m_editData)
        {
            if (auto editorDataElement = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
            {
                if (auto addableAttribute = editorDataElement->FindAttribute(AZ::Edit::Attributes::AddableByUser))
                {
                    // skip this component (return early) if user is not allowed to add it directly
                    if (auto addableData = azdynamic_cast<AZ::Edit::AttributeData<bool>*>(addableAttribute))
                    {
                        if (!addableData->Get(nullptr))
                        {
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    bool CanCreateComponentOnEntity(AZ::SerializeContext* serializeContext, const AZ::SerializeContext::ClassData& componentClassData, AZ::Entity* entity)
    {
        // Get the componentDescriptor from the componentClassData
        AZ::ComponentDescriptor* componentDescriptor = nullptr;
        EBUS_EVENT_ID_RESULT(componentDescriptor, componentClassData.m_typeId, AZ::ComponentDescriptorBus, GetDescriptor);
        if (!componentDescriptor)
        {
            AZStd::string message = AZStd::string::format("ComponentDescriptor not found for %s", GetFriendlyComponentName(componentClassData));
            AZ_Assert(false, message.c_str());
            return false;
        }

        // Get the incompatible, provided and required services from the descriptor
        AZ::ComponentDescriptor::DependencyArrayType incompatibleServices;
        componentDescriptor->GetIncompatibleServices(incompatibleServices, nullptr);

        AZ::ComponentDescriptor::DependencyArrayType providedServices;
        componentDescriptor->GetProvidedServices(providedServices, nullptr);

        AZ::ComponentDescriptor::DependencyArrayType requiredServices;
        componentDescriptor->GetRequiredServices(requiredServices, nullptr);

        // Check if component can be added to this entity
        AZ::ComponentDescriptor::DependencyArrayType services;
        for (const AZ::Component* component : entity->GetComponents())
        {
            const AZ::Uuid& existingComponentTypeId = AzToolsFramework::GetUnderlyingComponentType(*component);

            AZ::ComponentDescriptor* existingComponentDescriptor = nullptr;
            EBUS_EVENT_ID_RESULT(existingComponentDescriptor, existingComponentTypeId, AZ::ComponentDescriptorBus, GetDescriptor);
            if (!componentDescriptor)
            {
                return false;
            }

            services.clear();
            existingComponentDescriptor->GetProvidedServices(services, nullptr);

            // Check that none of the services currently provided by the entity are incompatible services
            // for the new component.
            // Also check that all of the required services of the new component are provided by the
            // existing components
            for (const AZ::ComponentServiceType& service : services)
            {
                for (const AZ::ComponentServiceType& incompatibleService : incompatibleServices)
                {
                    if (service == incompatibleService)
                    {
                        return false;
                    }
                }

                for (auto iter = requiredServices.begin(); iter != requiredServices.end(); ++iter)
                {
                    const AZ::ComponentServiceType& requiredService = *iter;
                    if (service == requiredService)
                    {
                        // this required service has been matched - remove from list
                        requiredServices.erase(iter);
                        break;
                    }
                }
            }

            services.clear();
            existingComponentDescriptor->GetIncompatibleServices(services, nullptr);

            // Check that none of the services provided by the component are incompatible with any
            // of the services currently provided by the entity
            for (const AZ::ComponentServiceType& service : services)
            {
                for (const AZ::ComponentServiceType& providedService : providedServices)
                {
                    if (service == providedService)
                    {
                        return false;
                    }
                }
            }
        }

        if (requiredServices.size() > 0)
        {
            // some required services were not provided by the components on the entity
            return false;
        }

        return true;
    }

    bool CanRemoveComponentFromEntity(AZ::SerializeContext* serializeContext, const AZ::Component* componentToRemove, AZ::Entity* entity)
    {
        const AZ::Uuid& componentToRemoveTypeId = AzToolsFramework::GetUnderlyingComponentType(*componentToRemove);
        const AZ::SerializeContext::ClassData* classData = serializeContext->FindClassData(componentToRemoveTypeId);

        if (!IsAddableByUser(classData))
        {
            return false;
        }

        // Go through all the components on the entity (except this one) and collect all the required services
        // and all the provided services
        AZ::ComponentDescriptor::DependencyArrayType allRequiredServices;
        AZ::ComponentDescriptor::DependencyArrayType allProvidedServices;
        AZ::ComponentDescriptor::DependencyArrayType services;
        for (const AZ::Component* component : entity->GetComponents())
        {
            if (component == componentToRemove)
            {
                continue;
            }

            const AZ::Uuid& componentTypeId = AzToolsFramework::GetUnderlyingComponentType(*component);

            AZ::ComponentDescriptor* componentDescriptor = nullptr;
            EBUS_EVENT_ID_RESULT(componentDescriptor, componentTypeId, AZ::ComponentDescriptorBus, GetDescriptor);
            if (!componentDescriptor)
            {
                return false;
            }

            services.clear();
            componentDescriptor->GetRequiredServices(services, nullptr);

            for (auto requiredService : services)
            {
                allRequiredServices.push_back(requiredService);
            }

            services.clear();
            componentDescriptor->GetProvidedServices(services, nullptr);

            for (auto providedService : services)
            {
                allProvidedServices.push_back(providedService);
            }
        }

        // remove all the satisfied services from the required services lits
        for (auto providedService : allProvidedServices)
        {
            for (auto iter = allRequiredServices.begin(); iter != allRequiredServices.end(); ++iter)
            {
                const AZ::ComponentServiceType& requiredService = *iter;
                if (providedService == requiredService)
                {
                    // this required service has been matched - remove from list
                    allRequiredServices.erase(iter);
                    break;
                }
            }
        }

        // if there is anything left in required services then check if the component we are removing
        // provides any of those services, if so we cannot remove it
        if (allRequiredServices.size() > 0)
        {
            AZ::ComponentDescriptor* componentDescriptor = nullptr;
            EBUS_EVENT_ID_RESULT(componentDescriptor, componentToRemoveTypeId, AZ::ComponentDescriptorBus, GetDescriptor);
            if (!componentDescriptor)
            {
                return false;
            }

            // Get the services provided by the component to be deleted
            AZ::ComponentDescriptor::DependencyArrayType providedServices;
            componentDescriptor->GetProvidedServices(providedServices, nullptr);

            for (auto requiredService: allRequiredServices)
            {
                // Check that none of the services currently still by the entity are the any of the ones we want to remove
                for (auto providedService : providedServices)
                {
                    if (requiredService == providedService)
                    {
                        // this required service is being provided by the component we want to remove
                        return false;
                    }
                }
            }
        }

        return true;
    }

    QList<QAction*> CreateAddComponentActions(HierarchyWidget* hierarchy,
        QTreeWidgetItemRawPtrQList& selectedItems,
        QWidget* parent)
    {
        if (selectedItems.empty())
        {
            // Nothing has been selected.
            // Nothing to do.
            return QList<QAction*>();
        }

        HierarchyItemRawPtrList items = SelectionHelpers::GetSelectedHierarchyItems(hierarchy,
                selectedItems);

        AzToolsFramework::EntityIdList entitiesSelected;
        for (auto item : items)
        {
            entitiesSelected.push_back(item->GetEntityId());
        }

        using ComponentsList = AZStd::vector < const AZ::SerializeContext::ClassData* >;
        ComponentsList componentsList;

        // Get the serialize context.
        AZ::SerializeContext* serializeContext;
        EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(serializeContext, "We should have a valid context!");

        // Gather all components that match our filter and group by category.
        serializeContext->EnumerateDerived<AZ::Component>(
            [ &componentsList ] (const AZ::SerializeContext::ClassData* classData, const AZ::Uuid& knownType) -> bool
            {
                (void)knownType;

                if (AppearsInUIComponentMenu(*classData))
                {
                    if (!IsAddableByUser(classData))
                    {
                        return true;
                    }

                    componentsList.push_back(classData);
                }

                return true;
            }
        );

        // Create a component list that is in the same order that the components were registered in
        const AZStd::vector<AZ::Uuid>* componentOrderList;
        ComponentsList orderedComponentsList;
        EBUS_EVENT_RESULT(componentOrderList, UiSystemBus, GetComponentTypesForMenuOrdering);
        for (auto& componentType : *componentOrderList)
        {
            auto iter = AZStd::find_if(componentsList.begin(), componentsList.end(),
                [componentType](const AZ::SerializeContext::ClassData* classData) -> bool
                    {
                        return (classData->m_typeId == componentType);
                    }
                );

            if (iter != componentsList.end())
            {
                orderedComponentsList.push_back(*iter);
                componentsList.erase(iter);
            }
        }
        // add any remaining component desciptiors to the end of the ordered list
        // (just to catch any component types that were not registered for ordering)
        for (auto componentClass : componentsList)
        {
            orderedComponentsList.push_back(componentClass);
        }

        QList<QAction*> result;
        {
            // Add an action for each factory.
            for (auto componentClass : orderedComponentsList)
            {
                const char* typeName = componentClass->m_editData->m_name;

                AZStd::string iconPath = GetComponentIconPath(componentClass);

                QString iconUrl = QString(iconPath.c_str());

                bool isEnabled = false;
                for (auto i : items)
                {
                    AZ::Entity* entity = i->GetElement();
                    if (CanCreateComponentOnEntity(serializeContext, *componentClass, entity))
                    {
                        isEnabled = true;

                        // Stop iterating thru the items
                        // and go to the next factory.
                        break;
                    }
                }

                QAction* action = new QAction(QIcon(iconUrl), typeName, parent);
                action->setEnabled(isEnabled);
                QObject::connect(action,
                    &QAction::triggered,
                    [ serializeContext, hierarchy, componentClass, items ](bool checked)
                    {
                        hierarchy->GetEditorWindow()->GetProperties()->BeforePropertyModified(nullptr);

                        for (auto i : items)
                        {
                            AZ::Entity* entity = i->GetElement();
                            if (CanCreateComponentOnEntity(serializeContext, *componentClass, entity))
                            {
                                entity->Deactivate();
                                AZ::Component* component;
                                EBUS_EVENT_ID_RESULT(component, componentClass->m_typeId, AZ::ComponentDescriptorBus, CreateComponent);
                                entity->AddComponent(component);
                                entity->Activate();
                            }
                        }

                        hierarchy->GetEditorWindow()->GetProperties()->AfterPropertyModified(nullptr);

                        // This is necessary to update the PropertiesWidget.
                        hierarchy->SignalUserSelectionHasChanged(hierarchy->selectedItems());
                    });

                result.push_back(action);
            }
        }

        return result;
    }

    QList<QAction*> CreateRemoveComponentActions(HierarchyWidget* hierarchy,
        QTreeWidgetItemRawPtrQList& selectedItems,
        const AZ::Component* optionalOnlyThisComponentType)
    {
        if (selectedItems.empty())
        {
            // Nothing has been selected.
            // Nothing to do.
            return QList<QAction*>();
        }

        HierarchyItemRawPtrList items = SelectionHelpers::GetSelectedHierarchyItems(hierarchy,
                selectedItems);

        // Get the serialize context.
        AZ::SerializeContext* serializeContext;
        EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(serializeContext, "We should have a valid context!");

        // Get all the factories currently in-use by the selected items.
        AZStd::vector<AZ::Uuid> componentTypesForMenu;
        AZStd::vector<AZ::Uuid> componentTypesThatCanBeRemoved;
        if (optionalOnlyThisComponentType)
        {
            const AZ::Uuid& componentTypeId = AzToolsFramework::GetUnderlyingComponentType(*optionalOnlyThisComponentType);

            componentTypesForMenu.push_back(componentTypeId);

            bool canRemove = true;
            for (auto i : items)
            {
                AZ::Entity* entity = i->GetElement();
                if (!CanRemoveComponentFromEntity(serializeContext, optionalOnlyThisComponentType, entity))
                {
                    canRemove = false;
                }
            }

            if (canRemove)
            {
                componentTypesThatCanBeRemoved.push_back(componentTypeId);
            }
        }
        else
        {
            // Make a list of all components on the selected entities
            for (auto i : items)
            {
                for (auto c : i->GetElement()->GetComponents())
                {
                    const AZ::Uuid& componentTypeId = AzToolsFramework::GetUnderlyingComponentType(*c);
                    auto iter = AZStd::find(componentTypesForMenu.begin(), componentTypesForMenu.end(), componentTypeId);
                    if (iter == componentTypesForMenu.end())
                    {
                        componentTypesForMenu.push_back(componentTypeId);
                        componentTypesThatCanBeRemoved.push_back(componentTypeId);
                    }
                }
            }

            // remove components from the list if they cannot be romved on any of the entities
            for (auto i : items)
            {
                for (auto c : i->GetElement()->GetComponents())
                {
                    const AZ::Uuid& componentTypeId = AzToolsFramework::GetUnderlyingComponentType(*c);

                    AZ::Entity* entity = i->GetElement();
                    bool canRemove = true;
                    if (!CanRemoveComponentFromEntity(serializeContext, optionalOnlyThisComponentType, entity))
                    {
                        auto iter = AZStd::find(componentTypesThatCanBeRemoved.begin(), componentTypesThatCanBeRemoved.end(), componentTypeId);
                        if (iter != componentTypesThatCanBeRemoved.end())
                        {
                            componentTypesThatCanBeRemoved.erase(iter);
                        }
                    }
                }
            }
        }

        QList<QAction*> result;
        // Add an action for each factory.
        for (auto componentTypeId : componentTypesForMenu)
        {
            AZ::ComponentDescriptor* componentDescriptor = nullptr;
            EBUS_EVENT_ID_RESULT(componentDescriptor, componentTypeId, AZ::ComponentDescriptorBus, GetDescriptor);
            if (!componentDescriptor)
            {
                continue;
            }

            const char* typeName = componentDescriptor->GetName();

            QString title = QString("Remove component %1").arg(typeName);

            QAction* action = new QAction(title, hierarchy);
            QObject::connect(action,
                &QAction::triggered,
                [ hierarchy, componentTypeId, items ](bool checked)
                {
                    hierarchy->GetEditorWindow()->GetProperties()->BeforePropertyModified(nullptr);

                    for (auto i : items)
                    {
                        // We got this factory from LyShine so we know this is a UI component
                        AZ::Entity* element = i->GetElement();
                        AZ::Component* component = element->FindComponent(componentTypeId);
                        if (component)
                        {
                            element->Deactivate();
                            element->RemoveComponent(component);
                            element->Activate();
                            delete component;
                        }
                    }
                    hierarchy->GetEditorWindow()->GetProperties()->AfterPropertyModified(nullptr);

                    // This is necessary to update the PropertiesWidget.
                    hierarchy->SignalUserSelectionHasChanged(hierarchy->selectedItems());
                });

           auto iter = AZStd::find(componentTypesThatCanBeRemoved.begin(), componentTypesThatCanBeRemoved.end(), componentTypeId);
           if (iter == componentTypesThatCanBeRemoved.end())
           {
                action->setEnabled(false);
           }

            result.push_back(action);
        }

        return result;
    }

    AZStd::vector<ComponentTypeData> GetAllComponentTypesThatCanAppearInAddComponentMenu()
    {
        using ComponentsList = AZStd::vector<ComponentTypeData>;
        ComponentsList componentsList;

        // Get the serialize context.
        AZ::SerializeContext* serializeContext;
        EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(serializeContext, "We should have a valid context!");

        const AZStd::list<AZ::ComponentDescriptor*>* lyShineComponentDescriptors;
        EBUS_EVENT_RESULT(lyShineComponentDescriptors, UiSystemBus, GetLyShineComponentDescriptors);

        // Gather all components that match our filter and group by category.
        serializeContext->EnumerateDerived<AZ::Component>(
            [ &componentsList, lyShineComponentDescriptors ] (const AZ::SerializeContext::ClassData* classData, const AZ::Uuid& knownType) -> bool
            {
                (void)knownType;

                if (AppearsInUIComponentMenu(*classData))
                {
                    if (!IsAddableByUser(classData))
                    {
                        // skip this component (return early) if user is not allowed to add it directly
                        return true;
                    }

                    bool isLyShineComponent = false;
                    for (auto descriptor : *lyShineComponentDescriptors)
                    {
                        if (descriptor->GetUuid() == classData->m_typeId)
                        {
                            isLyShineComponent = true;
                            break;
                        }
                    }
                    ComponentTypeData data;
                    data.classData = classData;
                    data.isLyShineComponent = isLyShineComponent;
                    componentsList.push_back(data);
                }

                return true;
            }
        );

        return componentsList;
    }

}   // namespace ComponentHelpers
