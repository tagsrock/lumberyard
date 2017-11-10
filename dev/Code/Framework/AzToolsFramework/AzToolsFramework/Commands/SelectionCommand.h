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

#ifndef AZTOOLSFRAMEWORK_SELECTIONCOMMAND_H
#define AZTOOLSFRAMEWORK_SELECTIONCOMMAND_H

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Component/componentbus.h>
#include <AzToolsFramework/Undo/UndoSystem.h>

#pragma once

namespace AzToolsFramework
{
    /**
     * Stores an entity selection set for undo/redo.
     */
    class SelectionCommand
        : public UndoSystem::URSequencePoint
    {
    public:
        AZ_CLASS_ALLOCATOR(SelectionCommand, AZ::SystemAllocator, 0);
        AZ_RTTI(SelectionCommand, "{07A0CF6A-79FA-4EA3-B056-1C0DA6F36699}");

        SelectionCommand(const AZStd::vector<AZ::EntityId>& proposedSelection, const AZStd::string& friendlyName);

        virtual void UpdateSelection(const AZStd::vector<AZ::EntityId>& proposedSelection);

        virtual void Post();

        virtual void Undo();
        virtual void Redo();

        const AZStd::vector<AZ::EntityId>& GetInitialSelectionList() const;

    protected:
        AZStd::vector<AZ::EntityId> m_previousSelectionList;
        AZStd::vector<AZ::EntityId> m_proposedSelectionList;
    };
} // namespace AzToolsFramework

#endif // AZTOOLSFRAMEWORK_SELECTIONCOMMAND_H
