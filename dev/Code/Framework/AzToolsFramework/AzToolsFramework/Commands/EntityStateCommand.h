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
#ifndef AZTOOLSFRAMEWORK_ENTITYSTATECOMMAND_H
#define AZTOOLSFRAMEWORK_ENTITYSTATECOMMAND_H

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Component/EntityId.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzToolsFramework/Undo/UndoSystem.h>

#pragma once

namespace AZ
{
    class Entity;
}

namespace AzToolsFramework
{
    // The entity State URSequencePoint stores the state of an entity before and after some change to it.
    // it does so by serializing the entire entity, so its a good "default behavior" that cannot miss any particular change.
    // we can specialize undos (such as the Entity Transform command) to be more specific and narrower in scope
    // but at least an Entity State Command should be able to capture everything in its entirety.
    class EntityStateCommand
        : public UndoSystem::URSequencePoint
    {
    public:
        AZ_RTTI(EntityStateCommand, "{4461579F-9D39-4954-B5D4-0F9388C8D15D}", UndoSystem::URSequencePoint);
        AZ_CLASS_ALLOCATOR(EntityStateCommand, AZ::SystemAllocator, 0);

        EntityStateCommand(UndoSystem::URCommandID ID, const char* friendlyName = nullptr);
        virtual ~EntityStateCommand();

        virtual void Undo() override;
        virtual void Redo() override;

        // capture the initial state - this fills the undo with the initial data if captureUndo is true
        // otherwise is captures the final state.
        void Capture(AZ::Entity* pSourceEntity, bool captureUndo);
        AZ::EntityId GetEntityID() const { return m_entityID; }

    protected:

        void RestoreEntity(const AZ::u8* buffer, AZStd::size_t bufferSizeBytes) const;

        AZ::EntityId m_entityID;                            ///< The Id of the captured entity.
        AzFramework::EntityContextId m_entityContextId;     ///< The entity context to which the entity belongs (if any).
        int m_entityState;                                  ///< The entity state at time of capture (active, constructed, etc).
        bool m_isSelected;                                  ///< Whether the entity was selected at time of capture.

        AZ::SliceComponent::EntityRestoreInfo m_sliceRestoreInfo;

        AZStd::vector<AZ::u8> m_undoState;
        AZStd::vector<AZ::u8> m_redoState;

        // DISABLE COPY
        EntityStateCommand(const EntityStateCommand& other) = delete;
        const EntityStateCommand& operator= (const EntityStateCommand& other) = delete;
    };

    class EntityDeleteCommand
        : public EntityStateCommand
    {
    public:
        AZ_RTTI(EntityDeleteCommand, "{2877DC4C-3F09-4E1A-BE3D-921A021DAB80}", EntityStateCommand);
        AZ_CLASS_ALLOCATOR(EntityDeleteCommand, AZ::SystemAllocator, 0);

        EntityDeleteCommand(UndoSystem::URCommandID ID);
        void Capture(AZ::Entity* pSourceEntity);

        virtual void Undo() override;
        virtual void Redo() override;
    };

    class EntityCreateCommand
        : public EntityStateCommand
    {
    public:
        AZ_RTTI(EntityCreateCommand, "{C1AA9763-9EC8-4F7B-803E-C04EE3DB3DA9}", EntityStateCommand);
        AZ_CLASS_ALLOCATOR(EntityCreateCommand, AZ::SystemAllocator, 0);

        EntityCreateCommand(UndoSystem::URCommandID ID);
        void Capture(AZ::Entity* pSourceEntity);

        virtual void Undo() override;
        virtual void Redo() override;
    };
} // namespace AzToolsFramework

#endif // AZTOOLSFRAMEWORK_ENTITYSTATECOMMAND_H
