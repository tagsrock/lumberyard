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

#ifndef ENTITY_PROPERTY_EDITOR_H
#define ENTITY_PROPERTY_EDITOR_H

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/Undo/UndoSystem.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/ToolsComponents/EditorInspectorComponentBus.h>
#include <QtWidgets/QWidget>

#pragma once

class QLabel;
class QSpacerItem;
class QMenu;

namespace Ui
{
    class EntityPropertyEditorUI;
}

namespace AZ
{
    struct ClassDataReflection;
    class Component;
    class Entity;
}

namespace UndoSystem
{
    class URSequencePoint;
}

namespace AzToolsFramework
{
    class ComponentEditor;
    class ComponentPaletteWidget;
    struct SourceControlFileInfo;
    using ComponentEditorVector = AZStd::vector<ComponentEditor*>;

    /**
     * the entity property editor shows all components for a given entity or set of entities.
     * it displays their values and lets you edit them.  The editing actually happens through the sub editor parts, though.
     * only components which the selected entities have in common are displayed (if theres more than one)
     * if there are components that are not in common, there is a message indicating that this is the case.
     * each component is shown as a heading which can be expanded into an actual component specific property editor.
     * so this widget is actually only interested in specifically what entities are selected, what their components are,
     * and what is in common.
     */
    class EntityPropertyEditor
        : public QWidget
        , private ToolsApplicationEvents::Bus::Handler
        , public IPropertyEditorNotify
        , public AzToolsFramework::EditorEntityContextNotificationBus::Handler
        , public AZ::EntitySystemBus::Handler
    {
        Q_OBJECT;
    public:

        AZ_CLASS_ALLOCATOR(EntityPropertyEditor, AZ::SystemAllocator, 0)

        EntityPropertyEditor(QWidget* pParent = NULL, Qt::WindowFlags flags = 0);
        virtual ~EntityPropertyEditor();

        virtual void BeforeUndoRedo();
        virtual void AfterUndoRedo();

        static void Reflect(AZ::ReflectContext* context);

        // implementation of IPropertyEditorNotify:

        // CALLED FOR UNDO PURPOSES
        void BeforePropertyModified(InstanceDataNode* pNode) override;
        void AfterPropertyModified(InstanceDataNode* pNode) override;
        void SetPropertyEditingActive(InstanceDataNode* pNode) override;
        void SetPropertyEditingComplete(InstanceDataNode* pNode) override;
        void SealUndoStack() override;

        // Context menu population for entity component properties.
        void RequestPropertyContextMenu(InstanceDataNode* node, const QPoint& globalPos) override;

        /// Set filter for what appears in the "Add Components" menu.
        void SetAddComponentMenuFilter(ComponentFilter componentFilter);

        void SetAllowRename(bool allowRename);

    private:

        struct SharedComponentInfo
        {
            SharedComponentInfo(AZ::Component* component, AZ::Component* sliceReferenceComponent);
            AZ::Entity::ComponentArrayType m_instances;
            AZ::Component* m_sliceReferenceComponent;
        };

        using SharedComponentArray = AZStd::vector<SharedComponentInfo>;

        //////////////////////////////////////////////////////////////////////////
        // ToolsApplicationEvents::Bus::Handler
        virtual void BeforeEntitySelectionChanged();
        virtual void AfterEntitySelectionChanged();

        virtual void EntityParentChanged(AZ::EntityId, AZ::EntityId, AZ::EntityId) {}
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        /// AzToolsFramework::EditorEntityContextNotificationBus implementation
        void OnStartPlayInEditor() override;
        void OnStopPlayInEditor() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::EntitySystemBus::Handler
        void OnEntityActivated(const AZ::EntityId& entityId) override;
        void OnEntityDeactivated(const AZ::EntityId& entityId) override;
        void OnEntityNameChanged(const AZ::EntityId& entityId, const AZStd::string& name) override;
        //////////////////////////////////////////////////////////////////////////

        bool IsEntitySelected(const AZ::EntityId& id) const;
        bool IsSingleEntitySelected(const AZ::EntityId& id) const;

        virtual void GotSceneSourceControlStatus(AzToolsFramework::SourceControlFileInfo& fileInfo);
        virtual void PerformActionsBasedOnSceneStatus(bool sceneIsNew, bool readOnly);

        // enable/disable editor
        void EnableEditor(bool enabled);

        virtual void InvalidatePropertyDisplay(PropertyModificationRefreshLevel level);

        void MarkPropertyEditorBusyStart();
        void MarkPropertyEditorBusyEnd();

        void QueuePropertyRefresh();
        void ClearInstances(bool invalidateImmediately = true);

        AZStd::vector<AZ::Component*> GetAllComponentsForEntityInOrder(AZ::Entity* entity);
        void BuildSharedComponentArray(SharedComponentArray& sharedComponentArray);
        void BuildSharedComponentUI(SharedComponentArray& sharedComponentArray);
        ComponentEditor* CreateComponentEditor();
        void UpdateEntityIcon();
        void UpdateEntityDisplay();
        void SortComponentsByOrder(AZ::Entity::ComponentArrayType& componentsToSort, ComponentOrderArray& componentOrderList);
        bool ShouldDisplayComponent(AZ::Component* component) const;
        bool IsComponentRemovable(const AZ::Component* component) const;
        bool AreComponentsRemovable(const AZ::Entity::ComponentArrayType& components) const;

        void AddMenuOptionsForComponents(QMenu& menu, const QPoint& position);
        void AddMenuOptionsForFields(InstanceDataNode* fieldNode, InstanceDataNode* componentNode, const AZ::SerializeContext::ClassData* componentClassData, QMenu& menu);
        void AddMenuOptionForSliceReset(QMenu& menu);

        void ContextMenuActionPullFieldData(AZ::Component* parentComponent, InstanceDataNode* fieldNode);
        void ContextMenuActionSetDataFlag(InstanceDataNode* node, AZ::DataPatch::Flag flag, bool additive);

        /// Given an InstanceDataNode, calculate a DataPatch address relative to the entity.
        /// @return true if successful.
        bool GetEntityDataPatchAddress(const InstanceDataNode* node, AZ::DataPatch::AddressType& dataPatchAddressOut, AZ::EntityId* entityIdOut=nullptr) const;

        // Custom function for comparing values of InstanceDataNodes
        bool CompareInstanceDataNodeValues(const InstanceDataNode* sourceNode, const InstanceDataNode* targetNode);

        void OnDisplayComponentEditorMenu(const QPoint& position);
        void OnRequestRequiredComponents(const QPoint& position, const QSize& size, const AZStd::vector<AZ::ComponentServiceType>& services);

        AZ::Component* ExtractMatchingComponent(AZ::Component* component, AZ::Entity::ComponentArrayType& availableComponents);

        void SetEntityIconToDefault();
        void PopupAssetBrowserForEntityIcon();

        void HideComponentPalette();
        void ShowComponentPalette(
            ComponentPaletteWidget* componentPalette,
            const QPoint& position,
            const QSize& size,
            const AZStd::vector<AZ::ComponentServiceType>& serviceFilter);

        QAction* m_addComponentAction;
        QAction* m_removeAction;
        QAction* m_cutAction;
        QAction* m_copyAction;
        QAction* m_pasteAction;
        QAction* m_enableAction;
        QAction* m_disableAction;
        QAction* m_moveUpAction;
        QAction* m_moveDownAction;
        QAction* m_resetToSliceAction;

        void CreateActions();
        void UpdateActions();

        AZStd::vector<AZ::Component*> GetCopyableComponents() const;
        void RemoveComponents(const AZStd::vector<AZ::Component*>& components);
        void RemoveComponents();
        void CutComponents();
        void CopyComponents();
        void PasteComponents();
        void EnableComponents(const AZStd::vector<AZ::Component*>& components);
        void EnableComponents();
        void DisableComponents(const AZStd::vector<AZ::Component*>& components);
        void DisableComponents();
        void MoveComponentsUp();
        void MoveComponentsDown();

        void ResetToSlice();

        bool DoesOwnFocus() const;
        bool DoesIntersectWidget(const QRect& rectGlobal, const QWidget* widget) const;
        bool DoesIntersectSelectedComponentEditor(const QRect& rectGlobal) const;
        bool DoesIntersectNonSelectedComponentEditor(const QRect& rectGlobal) const;

        void ClearComponentEditorSelection();
        void SelectRangeOfComponentEditors(const AZ::s32 index1, const AZ::s32 index2, bool selected = true);
        void SelectIntersectingComponentEditors(const QRect& rectGlobal, bool selected = true);
        void ToggleIntersectingComponentEditors(const QRect& rectGlobal);
        AZ::s32 GetComponentEditorIndex(const ComponentEditor* componentEditor) const;
        ComponentEditorVector GetIntersectingComponentEditors(const QRect& rectGlobal) const;
        ComponentEditorVector GetSelectedComponentEditors() const;
        AZStd::vector<AZ::Component*> GetSelectedComponents() const;

        void SaveComponentEditorState();
        void LoadComponentEditorState();
        void ClearComponentEditorState();

        struct ComponentEditorSaveState
        {
            bool m_expanded = true;
            bool m_selected = false;
        };
        AZStd::unordered_map<AZ::ComponentId, ComponentEditorSaveState> m_componentEditorSaveStateTable;

        //widget overrides
        void contextMenuEvent(QContextMenuEvent* event) override;
        bool eventFilter(QObject* object, QEvent* event) override;
        bool m_selectionEventAccepted;

        bool m_isBuildingProperties;

        Ui::EntityPropertyEditorUI* m_gui;

        // Global app serialization context, cached for internal usage during the life of the control.
        AZ::SerializeContext* m_serializeContext;

        AZ::s32 m_componentEditorLastSelectedIndex;
        size_t m_componentEditorsUsed;
        ComponentEditorVector m_componentEditors;

        using ComponentPropertyEditorMap = AZStd::unordered_map<AZ::Component*, ComponentEditor*>;
        ComponentPropertyEditorMap m_componentToEditorMap;

        ComponentPaletteWidget* m_componentPalette;

        AzToolsFramework::UndoSystem::URSequencePoint* m_currentUndoOperation;
        InstanceDataNode* m_currentUndoNode;

        bool m_sceneIsNew;

        // The busy system tracks when components are being changed, this allows
        // a refresh when the busy counter hits zero, in case multiple things are making
        // changes to an object to mark it as busy.
        int m_propertyEditBusy;

        // the spacer's job is to make sure that its always at the end of the list of components.
        QSpacerItem* m_spacer;
        bool m_isAlreadyQueuedRefresh;
        bool m_shouldScrollToNewComponents;
        bool m_shouldScrollToNewComponentsQueued;

        // IDs of entities currently bound to this property editor.
        AZStd::vector<AZ::EntityId> m_selectedEntityIds;

        ComponentFilter m_componentFilter;

        // Pointer to entity that first entity is compared against for the purpose of rendering deltas vs. slice in the property grid.
        AZStd::unique_ptr<AZ::Entity> m_sliceCompareToEntity;

        // Temporary buffer to use when calculating a data patch address.
        AZ::DataPatch::AddressType m_dataPatchAddressBuffer;

        private slots:
        void OnPropertyRefreshRequired(); // refresh is needed for a property.
        void UpdateContents();
        void OnAddComponent();
        void OnEntityNameChanged();
        void ScrollToNewComponent();
        void QueueScrollToNewComponent();
        void BuildEntityIconMenu();
    };

}

#endif