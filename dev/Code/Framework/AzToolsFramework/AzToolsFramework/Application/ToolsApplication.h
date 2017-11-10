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

#ifndef AZTOOLSFRAMEWORK_TOOLSAPPLICATION_H
#define AZTOOLSFRAMEWORK_TOOLSAPPLICATION_H

#include <AzCore/base.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzFramework/Application/Application.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Commands/PreemptiveUndoCache.h>

#pragma once

namespace AzToolsFramework
{
    class ToolsApplication
        : public AzFramework::Application
        , public ToolsApplicationRequests::Bus::Handler
    {
    public:
        AZ_RTTI(ToolsApplication, "{2895561E-BE90-4CC3-8370-DD46FCF74C01}", AzFramework::Application);
        AZ_CLASS_ALLOCATOR(ToolsApplication, AZ::SystemAllocator, 0);

        ToolsApplication();
        ~ToolsApplication();

        void Stop();

        void ReflectSerialize() override;

        AZ::ComponentTypeList GetRequiredSystemComponents() const override;

    protected:

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::Application
        void StartCommon(AZ::Entity* systemEntity) override;
        void RegisterCoreComponents() override;
        bool AddEntity(AZ::Entity* entity) override;
        bool RemoveEntity(AZ::Entity* entity) override;
        const char* GetCurrentConfigurationName() const override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // ToolsApplicationRequests::Bus::Handler
        void PreExportEntity(AZ::Entity& source, AZ::Entity& target) override;
        void PostExportEntity(AZ::Entity& source, AZ::Entity& target) override;

        void MarkEntitySelected(AZ::EntityId entityId) override;
        void MarkEntityDeselected(AZ::EntityId entityId) override;
        void SetEntityHighlighted(AZ::EntityId entityId, bool highlighted) override;

        void AddDirtyEntity(AZ::EntityId entityId) override;
        int RemoveDirtyEntity(AZ::EntityId entityId) override;
        bool IsDuringUndoRedo() override { return m_isDuringUndoRedo; }
        void UndoPressed() override;
        void RedoPressed() override;
        UndoSystem::URSequencePoint* BeginUndoBatch(const char* label) override;
        UndoSystem::URSequencePoint* ResumeUndoBatch(UndoSystem::URSequencePoint* token, const char* label) override;
        void EndUndoBatch() override;

        bool IsEntityEditable(AZ::EntityId entityId) override;
        bool AreEntitiesEditable(const EntityIdList& entityIds) override;

        void CheckoutPressed() override;
        SourceControlFileInfo GetSceneSourceControlInfo() override;

        const EntityIdList& GetSelectedEntities() override { return m_selectedEntities; }
        const EntityIdList& GetHighlightedEntities() override { return m_highlightedEntities; }
        void SetSelectedEntities(const EntityIdList& selectedEntities) override;
        bool IsSelectable(const AZ::EntityId& entityId) override;
        bool IsSelected(const AZ::EntityId& entityId) override;
        UndoSystem::UndoStack* GetUndoStack() override { return m_undoStack; }
        UndoSystem::URSequencePoint* GetCurrentUndoBatch() override { return m_currentBatchUndo; }
        PreemptiveUndoCache* GetUndoCache() { return &m_undoCache; }

        EntityIdSet GatherEntitiesAndAllDescendents(const EntityIdList& inputEntities) override;

        void DeleteSelected() override;
        void DeleteEntities(const EntityIdList& entities) override;
        void DeleteEntitiesAndAllDescendants(const EntityIdList& entities) override;
        bool FindCommonRoot(const AzToolsFramework::EntityIdSet& entitiesToBeChecked, AZ::EntityId& commonRootEntityId
            , AzToolsFramework::EntityIdList* topLevelEntities = nullptr) override;
        bool FindCommonRootInactive(const AzToolsFramework::EntityList& entitiesToBeChecked, AZ::EntityId& commonRootEntityId, 
            AzToolsFramework::EntityList* topLevelEntities = nullptr) override;

        bool RequestEditForFileBlocking(const char* assetPath, const char* progressMessage, const RequestEditProgressCallback& progressCallback) override;
        void RequestEditForFile(const char* assetPath, RequestEditResultCallback resultCallback) override;
        
        void EnterEditorIsolationMode() override;
        void ExitEditorIsolationMode() override;
        bool IsEditorInIsolationMode() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::SimpleAssetRequests::Bus::Handler
        struct PathAssetEntry
        {
        public:
            PathAssetEntry(const char* path)
                : m_path(path) {}
            PathAssetEntry(AZStd::string&& path)
                : m_path(AZStd::move(path)) {}
            AZStd::string m_path;
        };
        //////////////////////////////////////////////////////////////////////////

        void CreateUndosForDirtyEntities();
        void ConsistencyCheckUndoCache();

    protected:

        AZ::Aabb                            m_selectionBounds;
        EntityIdList                        m_selectedEntities;
        EntityIdList                        m_highlightedEntities;
        UndoSystem::UndoStack*              m_undoStack;
        UndoSystem::URSequencePoint*        m_currentBatchUndo;
        AZStd::unordered_set<AZ::EntityId>  m_dirtyEntities;
        PreemptiveUndoCache                 m_undoCache;
        bool                                m_isDuringUndoRedo;
        bool                                m_isInIsolationMode;
        EntityIdSet                         m_isolatedEntityIdSet;
    };
} // namespace AzToolsFramework

#endif // AZTOOLSFRAMEWORK_TOOLSAPPLICATION_H
