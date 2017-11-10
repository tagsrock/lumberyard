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

CommandHierarchyItemCreateFromData::CommandHierarchyItemCreateFromData(UndoStack* stack,
    HierarchyWidget* hierarchy,
    EntityHelpers::EntityIdList& parents,
    bool createAsChildOfSelection,
    HierarchyHelpers::Creator creator,
    QString& dataSource)
    : QUndoCommand()
    , m_stack(stack)
    , m_hierarchy(hierarchy)
    , m_parents(parents)
    , m_createAsChildOfSelection(createAsChildOfSelection)
    , m_creator(creator)
    , m_entries()
{
    setText(QString("create element") + (m_parents.empty() ? "" : "s") + QString(" from ") + dataSource);
}

void CommandHierarchyItemCreateFromData::undo()
{
    UndoStackExecutionScope s(m_stack);

    HierarchyHelpers::Delete(m_hierarchy, m_entries);
}

void CommandHierarchyItemCreateFromData::redo()
{
    UndoStackExecutionScope s(m_stack);

    if (m_entries.empty())
    {
        // This is the first call to redo().
        LyShine::EntityArray completeListOfNewlyCreatedTopLevelElements = HierarchyHelpers::CreateItemsAndElements(m_hierarchy,
                m_hierarchy->selectedItems(),
                m_createAsChildOfSelection,
                m_creator);

        // LyShine::EntityArray -> HierarchyItemRawPtrList.
        HierarchyItemRawPtrList items;
        for (auto e : completeListOfNewlyCreatedTopLevelElements)
        {
            items.push_back(dynamic_cast<HierarchyItem*>(HierarchyHelpers::ElementToItem(m_hierarchy, e, false)));
        }

        // true: Put the serialized data in m_undoXml.
        HierarchyClipboard::Serialize(m_hierarchy, m_hierarchy->selectedItems(), &items, m_entries, true);
        AZ_Assert(!m_entries.empty(), "Failed to serialize");
    }
    else
    {
        HierarchyHelpers::CreateItemsAndElements(m_hierarchy, m_entries);
    }

    HierarchyHelpers::ExpandParents(m_hierarchy, m_entries);

    m_hierarchy->clearSelection();
    HierarchyHelpers::SetSelectedItems(m_hierarchy, &m_entries);
}

void CommandHierarchyItemCreateFromData::Push(UndoStack* stack,
    HierarchyWidget* hierarchy,
    QTreeWidgetItemRawPtrQList& selectedItems,
    bool createAsChildOfSelection,
    HierarchyHelpers::Creator creator,
    QString dataSource)
{
    if (stack->GetIsExecuting())
    {
        // This is a redundant Qt notification.
        // Nothing else to do.
        return;
    }

    stack->push(new CommandHierarchyItemCreateFromData(stack,
            hierarchy,
            SelectionHelpers::GetSelectedElementIds(hierarchy,
                selectedItems,
                true),
            createAsChildOfSelection,
            creator,
            dataSource));
}
