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

#include <AzCore/Component/Component.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/EntityCompositionRequestBus.h>

#include <QFrame>
#include <QIcon>

class QVBoxLayout;

namespace AZ
{
    class Component;
    class SerializeContext;
}

namespace AzToolsFramework
{
    class ComponentEditorHeader;
    class ComponentEditorNotification;
    class IPropertyEditorNotify;
    class ReflectedPropertyEditor;
    enum PropertyModificationRefreshLevel : int;

    /**
     * Widget for editing an AZ::Component (or multiple components of the same type).
     */
    class ComponentEditor
        : public QFrame
    {
        Q_OBJECT;
    public:
        ComponentEditor(AZ::SerializeContext* context, IPropertyEditorNotify* notifyTarget = nullptr, QWidget* parent = nullptr);

        void AddInstance(AZ::Component* componentInstance, AZ::Component* aggregateInstance, AZ::Component* compareInstance);
        void ClearInstances(bool invalidateImmediately);

        void AddNotifications();
        void ClearNotifications();

        void InvalidateAll();
        void QueuePropertyEditorInvalidation(PropertyModificationRefreshLevel refreshLevel);
        void contextMenuEvent(QContextMenuEvent *event) override;

        void UpdateExpandability();
        void SetExpanded(bool expanded);
        bool IsExpanded() const;
        bool IsExpandable() const;

        void SetSelected(bool selected);
        bool IsSelected() const;

        ComponentEditorHeader* GetHeader();
        ReflectedPropertyEditor* GetPropertyEditor();
        AZStd::vector<AZ::Component*>& GetComponents();
        const AZStd::vector<AZ::Component*>& GetComponents() const;

    Q_SIGNALS:
        void OnExpansionContractionDone();
        void OnDisplayComponentEditorMenu(const QPoint& position);
        void OnRequestRemoveComponents(const AZStd::vector<AZ::Component*>& components);
        void OnRequestDisableComponents(const AZStd::vector<AZ::Component*>& components);
        void OnRequestRequiredComponents(const QPoint& position, const QSize& size, const AZStd::vector<AZ::ComponentServiceType>& services);
        void OnRequestSelectionChange(const QPoint& position);

    private:
        /// Set up header for this component type.
        void SetComponentType(const AZ::Uuid& componentType);

        /// Clear header of anything specific to component type.
        void InvalidateComponentType();

        void OnExpanderChanged(bool expanded);
        void OnContextMenuClicked(const QPoint& position);

        ComponentEditorNotification* CreateNotification(const QString& message);
        ComponentEditorNotification* CreateNotificationForConflictingComponents(const QString& message);
        ComponentEditorNotification* CreateNotificationForMissingComponents(const QString& message, const AZStd::vector<AZ::ComponentServiceType>& services);

        bool AreAnyComponentsDisabled() const;
        AzToolsFramework::EntityCompositionRequests::PendingComponentInfo GetPendingComponentInfoForAllComponents() const;
        AzToolsFramework::EntityCompositionRequests::PendingComponentInfo GetPendingComponentInfoForAllComponentsInReverse() const;
        QIcon m_warningIcon;

        ComponentEditorHeader* m_header = nullptr;
        ReflectedPropertyEditor* m_propertyEditor = nullptr;
        QVBoxLayout* m_mainLayout = nullptr;

        AZ::SerializeContext* m_serializeContext;

        /// Type of component being shown
        AZ::Uuid m_componentType = AZ::Uuid::CreateNull();
 
        AZStd::vector<AZ::Component*> m_components;
        AZStd::vector<QWidget*> m_notifications;
        AZ::Crc32 m_savedKeySeed;
    };
}