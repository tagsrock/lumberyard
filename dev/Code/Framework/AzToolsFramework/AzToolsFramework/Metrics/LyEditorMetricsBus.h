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
#ifndef AZTOOLSFRAMEWORK_LYEDITORMETRICSBUS_H
#define AZTOOLSFRAMEWORK_LYEDITORMETRICSBUS_H

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Uuid.h>

class QAction;
class QString;

namespace AzToolsFramework
{
    enum class MetricsActionTriggerType
    {
        Unknown,

        MenuClick,
        MenuAltKey,
        ToolButton,
        Shortcut,

        Count
    };

    // Bus that can have messages sent when metrics related events occur (user triggered), and can be connected to in order to collect said metrics
    // Note that this bus should be called from the main, UI thread only
    class EditorMetricsEventsBusTraits
        : public AZ::EBusTraits
    {
    public: 
        enum NavigationTrigger
        {
            RightClickMenu,
            ButtonClick,
            DragAndDrop,

            Shortcut,
            ButtonClickToolbar,
            LeftClickMenu,

            Count
        };

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

        virtual ~EditorMetricsEventsBusTraits() {}

        // Send this in the high level UI code, to wrap lower level, system level events and actions
        // so that the lower level code doesn't have to know about how the events were triggered.
        // Or use 
        virtual void BeginUserAction(NavigationTrigger /*behaviour*/) {}

        // Send this in the high level UI code when all of the actions triggered by the user
        // are finished.
        virtual void EndUserAction() {}

        // Triggered when a user creates an entity manually 
        // (via right click mouse, via menu, via drag and drop, etc). 
        // Not triggered on level load or slice instantiation.
        virtual void EntityCreated(const AZ::EntityId& /*entityId*/) {}

        // Triggered when a user deletes an entity manually 
        // (via right click mouse, via menu, via drag and drop, etc).
        // Not triggered on level unload or slice release.
        virtual void EntityDeleted(const AZ::EntityId& /*entityId*/) {}

        // Triggered when a user adds a component manually to an entity
        // (via button click in Entity Inspector, drag+drop from Component Palette to Entity Inspector, Drag+Drop from File Browser to Entity Inspector)
        // Not triggered on level load/unload or slice instantiation/release.
        virtual void ComponentAdded(const AZ::EntityId& /*entityId*/, const AZ::Uuid& /* componentTypeId */) {}

        // Triggered when a user removes a component manually from an entity (via right click mouse)
        // Not triggered on level load/unload or slice instantiation/release.
        virtual void ComponentRemoved(const AZ::EntityId& /*entityId*/, const AZ::Uuid& /* componentTypeId */) {}

        // Triggered when the user changes the parent of an entity
        virtual void EntityParentChanged(const AZ::EntityId& /*entityId*/, const AZ::EntityId&  /*newParentId*/, const AZ::EntityId&  /*oldParentId*/) {}

        // Triggered when a legacy (Cry) entity is created by the user
        virtual void LegacyEntityCreated(const char* /* entityType */, const char* /* scriptEntityType */) {}

        // Triggered when the user triggers an undo of a ComponentEntity object(s)
        virtual void Undo() {}

        // Triggered when the user triggers a redo of a ComponentEntity object(s)
        virtual void Redo() {}

        // Triggered when the user triggers a clone of ComponentEntity object(s)
        virtual void EntitiesCloned() {}

        // Called when a menu is triggered
        virtual void MenuTriggered(const char* /*menuIdentifier*/, AzToolsFramework::MetricsActionTriggerType /* triggerType */ = AzToolsFramework::MetricsActionTriggerType::Unknown) {}

        virtual void RegisterAction(QAction* /*action*/, const QString& /*metricsText*/) {}
        virtual void UnregisterAction(QAction* /*action*/) {}
    };

    using EditorMetricsEventsBus = AZ::EBus<EditorMetricsEventsBusTraits>;


    // Wrapper class, to automatically handle calling BeginUserAction and EndUserAction on the EditorMetricsEventsBus
    class EditorMetricsEventsBusAction
    {
    public:
        EditorMetricsEventsBusAction(EditorMetricsEventsBusTraits::NavigationTrigger behaviour)
        {
            EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, BeginUserAction, behaviour);
        }

        ~EditorMetricsEventsBusAction()
        {
            EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, EndUserAction);
        }
    };
}

#endif // AZTOOLSFRAMEWORK_LYEDITORMETRICSBUS_H
#pragma once