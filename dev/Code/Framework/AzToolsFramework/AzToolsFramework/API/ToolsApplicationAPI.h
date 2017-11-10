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

#ifndef AZTOOLSFRAMEWORK_TOOLSAPPLICATIONAPI_H
#define AZTOOLSFRAMEWORK_TOOLSAPPLICATIONAPI_H

#include <AzCore/base.h>

#pragma once

#include <AzCore/EBus/EBus.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/containers/vector.h>

#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/SourceControl/SourceControlAPI.h>

namespace AZ
{
    class Entity;
    class Vector2;
    class Entity;
}

class QMenu;
class QWidget;
class QApplication;
struct IEditor;
struct QtViewOptions;

namespace AzToolsFramework
{
    class PreemptiveUndoCache;

    namespace UndoSystem
    {
        class UndoStack;
        class URSequencePoint;
    }

    namespace AssetBrowser
    {
        class AssetSelectionModel;
    }

    using EntityIdList = AZStd::vector<AZ::EntityId>;
    using EntityList = AZStd::vector<AZ::Entity*>;
    using ClassDataList = AZStd::vector<const AZ::SerializeContext::ClassData*>;
    using EntityIdSet = AZStd::unordered_set<AZ::EntityId>;

    //! Return true to accept this type of component.
    using ComponentFilter = AZStd::function<bool(const AZ::SerializeContext::ClassData&)>;

    /**
     * Bus owned by the ToolsApplication. Listen for general ToolsApplication events.
     */
    class ToolsApplicationEvents
        : public AZ::EBusTraits
    {
    public:

        using Bus = AZ::EBus<ToolsApplicationEvents>;

        /*!
         * Fired prior to committing a change in entity selection set.
         */
        virtual void BeforeEntitySelectionChanged() {}

        /*!
         * Fired after committing a change in entity selection set.
         */
        virtual void AfterEntitySelectionChanged() {}


        /*!
        * Fired before committing a change in entity highlighting set.
        */
        virtual void BeforeEntityHighlightingChanged() {}

        /*!
        * Fired after committing a change in entity highlighting set.
        */
        virtual void AfterEntityHighlightingChanged() {}

        /*!
         * Fired when an entity's transform parent has changed.
         */
        virtual void EntityParentChanged(AZ::EntityId /*entityId*/, AZ::EntityId /*newParentId*/, AZ::EntityId /*oldParentId*/) {}

        /*!
         * Fired when a given entity has been unregistered from the application.
         * \param entityId - The Id of the subject entity.
         */
        virtual void EntityDeregistered(AZ::EntityId /*entity*/) {}

        /*!
         * Fired when a given entity has been registered with the application.
         * \param entityId - The Id of the subject entity.
         */
        virtual void EntityRegistered(AZ::EntityId /*entity*/) {}

        /*!
         * Broadcast when the user has created an entity as a child of another entity.
         * This event is broadcast after the entity has been created and activated and
         * all relevant transform component information has been set.
         * \param entityId - The Id of the new entity
         * \param parentId - The Id of the new entity's parent
         */
        virtual void EntityCreatedAsChild(AZ::EntityId /*entityId*/, AZ::EntityId /*parentId*/) {}

        /*!
         * Fired just prior to applying a requested undo or redo operation.
         */
        virtual void BeforeUndoRedo() {}

        /*!
         * Fired just after applying a requested undo or redo operation.
         */
        virtual void AfterUndoRedo() {}

        /*!
         * Fired when a new undo batch has been started.
         * \param label - description of the batch.
         */
        virtual void OnBeginUndo(const char* /*label*/) {}

        /*!
         * Fired when an undo batch has been ended..
         * \param label - description of the batch.
         */
        virtual void OnEndUndo(const char* /*label*/) {}

        /*!
         * Notify property UI to refresh the property tree.
         */
        virtual void InvalidatePropertyDisplay(PropertyModificationRefreshLevel /*level*/) {}

        /*!
         * Process source control status for the specified file.
         */
        virtual void GotSceneSourceControlStatus(SourceControlFileInfo& /*fileInfo*/) {}

        /*!
         * Process scene status.
         */
        virtual void PerformActionsBasedOnSceneStatus(bool /*sceneIsNew*/, bool /*readOnly*/) {}

        /*!
         * Highlight the specified asset in the asset browser.
         */
        virtual void ShowAssetInBrowser(AZStd::string /*assetName*/) {}

        /*!
         * Event sent when the editor is set to Isolation Mode where only selected entities are visible
         */
        virtual void OnEnterEditorIsolationMode() {};

        /*!
         * Event sent when the editor quits Isolation Mode
         */
        virtual void OnExitEditorIsolationMode() {};
    };

    using ToolsApplicationNotificationBus = AZ::EBus<ToolsApplicationEvents>;

    /**
     * Bus used to make general requests to the ToolsApplication.
     */
    class ToolsApplicationRequests
        : public AZ::EBusTraits
    {
    public:

        using Bus = AZ::EBus<ToolsApplicationRequests>;

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        /*!
         * Handles pre-export tasks for an entity, such as generating runtime components on the target.
         */
        virtual void PreExportEntity(AZ::Entity& /*source*/, AZ::Entity& /*target*/) = 0;

        /*!
         * Handles post-export tasks for an entity.
         */
        virtual void PostExportEntity(AZ::Entity& /*source*/, AZ::Entity& /*target*/) = 0;

        /*!
         * Marks an entity as dirty.
         * \param target - The Id of the entity to mark as dirty.
         */
        virtual void AddDirtyEntity(AZ::EntityId target) = 0;

        /*!
        * Removes an entity from the dirty entity set.
        * \param target - The Id of the entity to remove
        * \return 1 if target EntityId was removed successfully, otherwise 0
        */
        virtual int RemoveDirtyEntity(AZ::EntityId target) = 0;

        /*!
         * \return true if an undo/redo operation is in progress.
         */
        virtual bool IsDuringUndoRedo() = 0;

        /*!
         * Notifies the application the user intends to undo the last undo-able operation.
         */
        virtual void UndoPressed() = 0;

        /*!
         * Notifies the application the user intends to reapply the last redo-able operation.
         */
        virtual void RedoPressed() = 0;

        /*!
         * Notifies the application that the user has selected an entity.
         * \param entityId - the Id of the newly selected entity.
         */
        virtual void MarkEntitySelected(AZ::EntityId entityId) = 0;

        /*!
         * Notifies the application that the user has deselected an entity.
         * \param entityId - the Id of the now deselected entity.
         */
        virtual void MarkEntityDeselected(AZ::EntityId entityId) = 0;

        /*!
         * Notifies the application that editor has highlighted an entity, or removed
         * a highlight. This is used for mouse-hover behavior in Sandbox.
         */
        virtual void SetEntityHighlighted(AZ::EntityId entityId, bool highlighted) = 0;

        /*!
         * Starts a new undo batch.
         * \param label - description of the operation.
         * \return a handle for the new batch, which can be used with ResumeUndoBatch().
         */
        virtual UndoSystem::URSequencePoint* BeginUndoBatch(const char* label) = 0;

        /*!
         * Attempts to continue adding to an existing undo batch command.
         * If the specified batchId is on the top of the stack, it is used, otherwise a new
         * handle is returned.
         * \param batchId - the Id of the undo batch (returned from BeginUndoBatch).
         * \param label - description of the operation.
         * \return a handle for the batch.
         */
        virtual UndoSystem::URSequencePoint* ResumeUndoBatch(UndoSystem::URSequencePoint* batchId, const char* label) = 0;

        /*!
         * Completes the current undo batch.
         * It's still possible to resume the batch as long as it's still the most recent one.
         */
        virtual void EndUndoBatch() = 0;

        /*!
         * Retrieves the preemptive undo cache for the application.
         */
        virtual PreemptiveUndoCache* GetUndoCache() = 0;

        /*!
         * \return true if the entity (or entities) can be edited/modified.
         */
        virtual bool IsEntityEditable(AZ::EntityId entityId) = 0;
        virtual bool AreEntitiesEditable(const EntityIdList& entityIds) = 0;

        /*!
         * Notifies the tools application that the user wishes to checkout selected entities.
         */
        virtual void CheckoutPressed() = 0;

        /*!
         * Returns source control info for the current world/scene.
         * Not yet implemented in ToolsApplication.
         */
        virtual SourceControlFileInfo GetSceneSourceControlInfo() = 0;

        /*!
         * Retrieves the set of selected entities.
         * \return a list of entity Ids.
         */
        virtual const EntityIdList& GetSelectedEntities() = 0;

        /*!
         * Retrieves the set of highlighted (but not selected) entities.
         * \return a list of entity Ids.
         */
        virtual const EntityIdList& GetHighlightedEntities() = 0;

        /*!
         * Explicitly specifies the set of selected entities.
         * \param a list of entity Ids.
         */
        virtual void SetSelectedEntities(const EntityIdList& selectedEntities) = 0;

        /*!
         * Returns true if the specified entity is able to be selected (not locked).
         * \param entityId
         */
        virtual bool IsSelectable(const AZ::EntityId& entityId) = 0;

        /*!
         * Returns true if the specified entity is currently selected.
         * \param entityId
         */
        virtual bool IsSelected(const AZ::EntityId& entityId) = 0;

        /*!
         * Retrieves the undo stack.
         * \return a pointer to the undo stack.
         */
        virtual UndoSystem::UndoStack* GetUndoStack() = 0;

        /*!
         * Retrieves the current undo batch.
         * \returns a pointer to the top of the undo stack.
         */
        virtual UndoSystem::URSequencePoint* GetCurrentUndoBatch() = 0;

        /*!
         * Given a list of input entity Ids, gather their children and all descendants as well.
         * \param inputEntities list of entities whose children should be gathered.
         * \return set of entity Ids including input entities and their immediate children.
         */
        virtual EntityIdSet GatherEntitiesAndAllDescendents(const EntityIdList& inputEntities) = 0;

        /*!
         * Delete all currently-selected entities.
         */
        virtual void DeleteSelected() = 0;

        /*!
         * Deletes all specified entities.
         */
        virtual void DeleteEntities(const EntityIdList& entities) = 0;

        /*!
        * Deletes all entities in the provided list, as well as their transform descendants.
        */
        virtual void DeleteEntitiesAndAllDescendants(const EntityIdList& entities) = 0;

        /*!
        * \brief Finds the Common root of an entity list; Also finds the top level entities in a given list of active entities (who share the common root)
        * Example : A(B[D,E{F}],C),G (Letter is entity name, braces hold children)
        *           Sample run | entitiesToBeChecked:(B,D,E,F,C)
        *                           commonRootEntityId: <A> , topLevelEntities: <B,C>, return : <true>
        *           Sample run | entitiesToBeChecked:(E,C)
        *                           commonRootEntityId:<InvalidEntityId> , topLevelEntities: <E,C>, return : <false>
        *           Sample run | entitiesToBeChecked:(A,G,B,E,C)
        *                          commonRootEntityId:<InvalidEntityId> , topLevelEntities: <A,G>, return : <true> (True because both of the top level entities have no parent , which for us is the common parent)
        *           Sample run | entitiesToBeChecked:(A,D)
        *                          commonRootEntityId:<InvalidEntityId> , topLevelEntities: <A,D>, return : <false>
        * \param entitiesToBeChecked List of entities whose parentage is to be found
        * \param commonRootEntityId [Out] Entity id of the common root for the entitiesToBeChecked
        * \param topLevelEntities [Out] List of entities at the top of the hierarchy in entitiesToBeChecked
        * \return boolean value indicating whether entities have a common root or not
        *           IF True commonRootEntityId is the common root of all rootLevelEntities
        *           IF False commonRootEntityId is an invalid entity id
        * NOTE: Requires that the entities to be checked are live, they must be active and available via TransformBus.
        *       \ref entitiesToBeChecked cannot contain nested entities with gaps, see Sample run 4
        */
        virtual bool FindCommonRoot(const AzToolsFramework::EntityIdSet& entitiesToBeChecked, AZ::EntityId& commonRootEntityId
            , AzToolsFramework::EntityIdList* topLevelEntities = nullptr) = 0;

        /**
        * \brief Finds the Common root of an entity list; Also finds the top level entities in a given list of inactive entities (who share the common root)
        * Example : A(B[D,E{F}],C),G (Letter is entity name, braces hold children)
        *           Sample run | entitiesToBeChecked:(B,D,E,F,C)
        *                           commonRootEntityId: <A> , topLevelEntities: <B,C>, return : <true>
        *           Sample run | entitiesToBeChecked:(E,C)
        *                           commonRootEntityId:<InvalidEntityId> , topLevelEntities: <E,C>, return : <false>
        *           Sample run | entitiesToBeChecked:(A,G,B,E,C)
        *                          commonRootEntityId:<InvalidEntityId> , topLevelEntities: <A,G>, return : <true> (True because both of the top level entities have no parent , which for us is the common parent)
        *           Sample run | entitiesToBeChecked:(A,D)
        *                          commonRootEntityId:<InvalidEntityId> , topLevelEntities: <A,D>, return : <false>
        * \param entitiesToBeChecked List of entities whose parentage is to be found
        * \param commonRootEntityId [Out] Entity id of the common root for the entitiesToBeChecked
        * \param topLevelEntities [Out] List of entities at the top of the hierarchy in entitiesToBeChecked
        * \return boolean value indicating whether entities have a common root or not
        *           IF True commonRootEntityId is the common root of all rootLevelEntities
        *           IF False commonRootEntityId is an invalid entity id
        * NOTE: Does not require that the entities to be checked are live, they could be temp or asset entities.
        *       \ref entitiesToBeChecked cannot contain nested entities with gaps, see Sample run 4
        */
        virtual bool FindCommonRootInactive(const AzToolsFramework::EntityList& entitiesToBeChecked, AZ::EntityId& commonRootEntityId, AzToolsFramework::EntityList* topLevelEntities = nullptr) = 0;

        /**
        * Prepares a file for editability. Interacts with source-control if the asset is not already writable, in a blocking fashion.
        * \param path full path of the asset to be made editable.
        * \param progressMessage progress message to display during checkout operation.
        * \param progressCallback user callback for retrieving progress information, provide RequestEditProgressCallback() if no progress reporting is required.
        * \return boolean value indicating if the file is writable after the operation.
        */
        using RequestEditProgressCallback = AZStd::function<void(int& current, int& max)>;
        virtual bool RequestEditForFileBlocking(const char* assetPath, const char* progressMessage, const RequestEditProgressCallback& progressCallback) = 0;

        /**
        * Prepares a file for editability. Interacts with source-control if the asset is not already writable.
        * \param path full path of the asset to be made editable.
        * \param resultCallback user callback to be notified when source control operation is complete. Callback will be invoked with a true success value if the file was made writable.
        *        If the file is already writable at the time the function is called, resultCallback(true) will be invoked immediately.
        */
        using RequestEditResultCallback = AZStd::function<void(bool success)>;
        virtual void RequestEditForFile(const char* assetPath, RequestEditResultCallback resultCallback) = 0;
        
        /*!
         * Enter the Isolation Mode and hide entities that are not selected.
         */
        virtual void EnterEditorIsolationMode() = 0;

        /*!
         * Exit the Isolation Mode and stop hiding entities.
         */
        virtual void ExitEditorIsolationMode() = 0;

        /*!
         * Request if the editor is currently in Isolation Mode
         * /return boolean indicating if the editor is currently in Isolation Mode
         */
        virtual bool IsEditorInIsolationMode() = 0;
    };

    using ToolsApplicationRequestBus = AZ::EBus<ToolsApplicationRequests>;

    /**
     * Bus keyed on entity Id for selection events.
     * Note that upon connection OnSelected() may be immediately invoked.
     */
    class EntitySelectionEvents
        : public AZ::ComponentBus
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides

        /**
         * Custom connection policy notifies handler if entity is already selected.
         */
        template<class Bus>
        struct SelectionConnectionPolicy
            : public AZ::EBusConnectionPolicy<Bus>
        {
            static void Connect(typename Bus::BusPtr& busPtr, typename Bus::Context& context, typename Bus::HandlerNode& handler, const typename Bus::BusIdType& id = 0)
            {
                AZ::EBusConnectionPolicy<Bus>::Connect(busPtr, context, handler, id);
                EntityIdList selectedEntities;
                EBUS_EVENT_RESULT(selectedEntities, ToolsApplicationRequests::Bus, GetSelectedEntities);
                if (AZStd::find(selectedEntities.begin(), selectedEntities.end(), id) != selectedEntities.end())
                {
                    handler->OnSelected();
                }
            }
        };
        template<typename Bus>
        using ConnectionPolicy = SelectionConnectionPolicy<Bus>;

        using Bus = AZ::EBus<EntitySelectionEvents>;
        ////////////////////////////////////////////////////////////////////////

        virtual void OnSelected() {}
        virtual void OnDeselected() {}
    };

    /**
    * Bus for editor requests related to pick mode
    */
    class EditorPickModeRequests
        : public AZ::EBusTraits
    {
    public:

        using Bus = AZ::EBus<EditorPickModeRequests>;

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        //////////////////////////////////////////////////////////////////////////

        /// Starts object pick mode -- next object selection will broadcasted via EntitySelectionEventBus::OnPickModeSelect,
        /// and will not affect general object selection.
        virtual void StartObjectPickMode() {};
        virtual void StopObjectPickMode() {};
        virtual void OnPickModeSelect(AZ::EntityId /*id*/) {}
    };

    /**
     * Bus for general editor requests to be intercepted by the application (e.g. Sandbox).
     */
    class EditorRequests
        : public AZ::EBusTraits
    {
    public:

        using Bus = AZ::EBus<EditorRequests>;

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides

        // PLEASE PLEASE PLEASE don't change this to multiple unless you change all of the
        // calls to this E-BUS that expect a returned value to handle multiple
        // buses listening.
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        /// Registers a view pane (generally a QMainWindow-derived class) with the main editor.
        /// \param name - display name for the pane. Will appear in the window header bar, as well as the context menu.
        /// \param category - category under the Tools menu that will contain the option to open the pane.
        /// \param viewOptions - structure defining various UI options for the pane.
        /// \param widgetCreationFunc - function callback for constructing the pane.
        typedef AZStd::function<QWidget*()> WidgetCreationFunc;
        virtual void RegisterViewPane(const char* /*name*/, const char* /*category*/, const QtViewOptions& /*viewOptions*/, const WidgetCreationFunc& /*widgetCreationFunc*/) {};

        /// Unregisters a view pane by name from the main editor.
        /// \param name - the name of the pane to be unregistered. This must match the name used for registration.
        virtual void UnregisterViewPane(const char* /*name*/) {};

        /// Show an Editor window by name.
        virtual void ShowViewPane(const char* /*paneName*/) {}

        /// Request generation of all level cubemaps.
        virtual void GenerateAllCubemaps() {}

        /// Regenerate cubemap for a particular entity.
        virtual void GenerateCubemapForEntity(AZ::EntityId /*entityId*/, AZStd::string* /*cubemapOutputPath*/) {}

        //! Spawn asset browser for the appropriate asset types.
        virtual void BrowseForAssets(AssetBrowser::AssetSelectionModel& /*selection*/) = 0;

        /// Allow interception of selection / left-mouse clicks in ObjectMode, for customizing selection behavior.
        virtual void HandleObjectModeSelection(const AZ::Vector2& /*point*/, int /*flags*/, bool& /*handled*/) {}

        /// Allow interception of cursor, for customizing selection behavior.
        virtual void UpdateObjectModeCursor(AZ::u32& /*cursorId*/, AZStd::string& /*cursorStr*/) {}

        /// Creates editor-side representation of an underlying entity.
        virtual void CreateEditorRepresentation(AZ::Entity* /*entity*/) { }

        /// Destroys editor-side representation of a given entity.
        virtual bool DestroyEditorRepresentation(AZ::EntityId /*entityId*/, bool /*deleteAZEntity*/) { return false; }

        /// Clone selected entities/slices.
        virtual void CloneSelection(bool& /*handled*/) {}

        /// Delete selected entities/slices
        virtual void DeleteSelectedEntities(bool /*includeDescendants*/) {}

        /// Create a new entity at a default position
        virtual AZ::EntityId CreateNewEntity(AZ::EntityId parentId = AZ::EntityId()) { (void)parentId; return AZ::EntityId(); }

        /// Create a new entity as a child of an existing entity - Intended only to handle explicit requests from the user
        virtual AZ::EntityId CreateNewEntityAsChild(AZ::EntityId /*parentId*/) { return AZ::EntityId(); }

        /// Create a new entity at a specified position
        virtual AZ::EntityId CreateNewEntityAtPosition(const AZ::Vector3& /*pos*/, AZ::EntityId parentId = AZ::EntityId()) { (void)parentId; return AZ::EntityId(); }

        /// Retrieve the main application window.
        virtual QWidget* GetMainWindow() { return nullptr; }

        /// Retrieve main editor interface.
        virtual IEditor* GetEditor() { return nullptr; }

        /// Launches the Lua editor and opens the specified (space separated) files.
        virtual void LaunchLuaEditor(const char* /*files*/) {}

        /// Returns whether a level document is open.
        virtual bool IsLevelDocumentOpen() { return false; }

        /// Return default icon to show in the viewport for components that haven't specified an icon.
        virtual AZStd::string GetDefaultComponentViewportIcon() { return AZStd::string(); }

        /// Return default icon to show in the palette, etc for components that haven't specified an icon.
        virtual AZStd::string GetDefaultComponentEditorIcon() { return AZStd::string(); }

        /// Return default entity icon to show both in viewport and entity-inspector.
        virtual AZStd::string GetDefaultEntityIcon() { return AZStd::string(); }

        /// Return path to icon for component.
        /// Path will be empty if component should have no icon.
        virtual AZStd::string GetComponentEditorIcon(const AZ::Uuid& /*componentType*/) { return AZStd::string(); }

        /**
         * Return the icon image path based on the component type and where it is used.
         * \param componentType         component type
         * \param componentIconAttrib   edit attribute describing where the icon is used, it could be one of Icon, Viewport and HidenIcon
         * \return the path of the icon image
         */
        virtual AZStd::string GetComponentIconPath(const AZ::Uuid& /*componentType*/, AZ::Crc32 /*componentIconAttrib*/) { return AZStd::string(); }

        /// Resource Selector hook, returns a path for a resource.
        virtual AZStd::string SelectResource(const AZStd::string& /*resourceType*/, const AZStd::string& /*previousValue*/) { return AZStd::string(); }

        /// Generate a new default Editable navigation area
        virtual void GenerateNavigationArea(const AZStd::string& /*name*/, const AZ::Vector3& /*position*/, const AZ::Vector3* /*points*/, size_t /*numPoints*/, float /*height*/) { }
    };

    using EditorRequestBus = AZ::EBus<EditorRequests>;

    /**
     * Bus for general editor events.
     */
    class EditorEvents
        : public AZ::EBusTraits
    {
    public:
        using Bus = AZ::EBus<EditorEvents>;

        /// The editor has changed performance specs.
        virtual void OnEditorSpecChange() {}

        enum EditorContextMenuFlags
        {
            eECMF_NONE = 0,
            eECMF_HIDE_ENTITY_CREATION = 0x1,
            eECMF_USE_VIEWPORT_CENTER = 0x2,
        };

        /// Populate global edit-time context menu.
        virtual void PopulateEditorGlobalContextMenu(QMenu * /*menu*/, const AZ::Vector2& /*point*/, int /*flags*/) {}

        /// Anything can override this and return true to skip over the WelcomeScreenDialog
        virtual bool SkipEditorStartupUI() { return false; }

        /// Notify that it's ok to register views
        virtual void NotifyRegisterViews() {}

        /// Notify that the Qt Application object is now ready to be used
        virtual void NotifyQtApplicationAvailable(QApplication* /* application */) {}
    };

    /**
     * RAII Helper class for undo batches.
     *
     * AzToolsFramework::ScopedUndoBatch undoBatch("Batch Name");
     * entity->ChangeData(...);
     * undoBatch.MarkEntityDirty(entity->GetId());
     */
    class ScopedUndoBatch
    {
    public:
        ScopedUndoBatch(const char* batchName)
        {
            EBUS_EVENT(ToolsApplicationRequests::Bus, BeginUndoBatch, batchName);
            EBUS_EVENT_RESULT(m_undoBatch, ToolsApplicationRequests::Bus, GetCurrentUndoBatch);
        }

        ~ScopedUndoBatch()
        {
            EBUS_EVENT(ToolsApplicationRequests::Bus, EndUndoBatch);
        }

        void MarkEntityDirty(const AZ::EntityId& id)
        {
            EBUS_EVENT(ToolsApplicationRequests::Bus, AddDirtyEntity, id);
        }

        UndoSystem::URSequencePoint* GetUndoBatch()
        {
            return m_undoBatch;
        }

    private:
        UndoSystem::URSequencePoint* m_undoBatch;

        // No moves or copies.
        ScopedUndoBatch(const ScopedUndoBatch&) = delete;
        ScopedUndoBatch(ScopedUndoBatch&&) = delete;
        ScopedUndoBatch& operator=(const ScopedUndoBatch&) = delete;
    };
} // namespace AzToolsFramework

#endif // AZTOOLSFRAMEWORK_TOOLSAPPLICATIONAPI_H
