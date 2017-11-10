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

//-------------------------------------------------------------------------------

namespace
{
    void CreateItems(HierarchyWidget* widget,
        LyShine::EntityArray& completeListOfNewlyCreatedTopLevelElements)
    {
        // Create items for all new elements.
        widget->CreateItems(completeListOfNewlyCreatedTopLevelElements);

        // IMPORTANT: The CTRL key is down when we paste items.
        // This has the side effect of ADDING to the selection,
        // instead of replacing it. The solution is to explicitely
        // UNSELECT the previously selected items BEFORE selecting
        // the newly created items.
        widget->clearSelection();

        // Expand and select.
        {
            HierarchyHelpers::ExpandParents(widget, completeListOfNewlyCreatedTopLevelElements);

            HierarchyHelpers::SetSelectedItems(widget, &completeListOfNewlyCreatedTopLevelElements);
        }
    }
} // anonymous namespace.

//-------------------------------------------------------------------------------

namespace HierarchyHelpers
{
    //-------------------------------------------------------------------------------

    void Delete(HierarchyWidget* hierarchy,
        SerializeHelpers::SerializedEntryList& entries)
    {
        hierarchy->SetIsDeleting(true);
        {
            for (auto& e : entries)
            {
                // IMPORTANT: It's SAFE to delete a HierarchyItem. Its
                // destructor will take care of removing the item from
                // the parent container, AND deleting all child items.
                // There's no risk of leaking memory. We just have to
                // make sure we don't have any dangling pointers.

                delete HierarchyHelpers::ElementToItem(hierarchy, e.m_id, false);
            }
        }
        hierarchy->SetIsDeleting(false);
    }

    bool HandleDeselect(QTreeWidgetItem* widgetItem, const bool controlKeyPressed)
    {
        if (widgetItem && widgetItem->isSelected())
        {
            // Ctrl+clicking a selected element should de-select it
            if (controlKeyPressed)
            {
                widgetItem->setSelected(false);
                return true;
            }
        }

        return false;
    }

    //-------------------------------------------------------------------------------

    QAction* CreateAddElementAction(HierarchyWidget* hierarchy,
        QTreeWidgetItemRawPtrQList& selectedItems,
        bool addAtRoot,
        const QPoint* optionalPos)
    {
        QAction* action = new QAction(QIcon(":/Icons/Eye_Open.png"),
                QString("&Empty element%1").arg(!addAtRoot && selectedItems.size() > 1 ? "s" : ""),
                hierarchy);
        QObject::connect(action,
            &QAction::triggered,
            [hierarchy, addAtRoot, optionalPos](bool checked)
            {
                if (addAtRoot)
                {
                    hierarchy->clearSelection();
                }
                hierarchy->AddElement(hierarchy->selectedItems(), optionalPos);
            });
        return action;
    }

    //-------------------------------------------------------------------------------

    void CreateItemsAndElements(HierarchyWidget* widget,
        SerializeHelpers::SerializedEntryList& entryList)
    {
        LyShine::EntityArray completeListOfNewlyCreatedTopLevelElements;

        // Create elements
        {
            // Because the entries use m_insertAboveThisId to correctly insert elements
            // in the right place and two siblings can be in the list of entries, the later
            // sibling has to be inserted first so that the earlier one can find the element
            // it should be before. We know that the SerializedEntryList is created in the order
            // that elements are in the element hierarchy. So we iterate in reverse order over the
            // SerializedEntryList while inserting the elements.
            for (auto iter = entryList.rbegin(); iter != entryList.rend(); ++iter)
            {
                auto& e = *iter;
                SerializeHelpers::RestoreSerializedElements(widget->GetEditorWindow()->GetCanvas(),
                    EntityHelpers::GetEntity(e.m_parentId),
                    EntityHelpers::GetEntity(e.m_insertAboveThisId),
                    widget->GetEditorWindow()->GetEntityContext(),
                    e.m_undoXml.c_str(),
                    false,
                    &completeListOfNewlyCreatedTopLevelElements);
            }
        }

        // because we iterated backward above the completeListOfNewlyCreatedTopLevelElements is now
        // in the reverse order of what the items should be in the HierarchyWidget. CreateItems
        // relies on them being in the correct order so we reverse the list.
        std::reverse(completeListOfNewlyCreatedTopLevelElements.begin(), completeListOfNewlyCreatedTopLevelElements.end());

        // Now create the items in the QTreeWidget
        CreateItems(widget, completeListOfNewlyCreatedTopLevelElements);
    }

    //-------------------------------------------------------------------------------

    LyShine::EntityArray CreateItemsAndElements(HierarchyWidget* widget,
        QTreeWidgetItemRawPtrQList& selectedItems,
        bool createAsChildOfSelection,
        Creator creator)
    {
        LyShine::EntityArray completeListOfNewlyCreatedTopLevelElements;

        // Create elements
        {
            HierarchyItem* parent = nullptr;
            {
                HierarchyItem* selectedItem = nullptr;
                {
                    HierarchyItemRawPtrList items;
                    SelectionHelpers::GetListOfTopLevelSelectedItems(widget,
                        selectedItems,
                        widget->invisibleRootItem(),
                        items);
                    selectedItem = (items.empty() ? nullptr : items.front());
                }

                // It's ok for parent to be nullptr.
                if (createAsChildOfSelection)
                {
                    // Create as a child of the selectedItem.
                    parent = selectedItem;
                }
                else
                {
                    // Create as a sibling of the selectedItem.
                    parent = selectedItem ? selectedItem->Parent() : nullptr;
                }
            }

            // Create.
            {
                LyShine::EntityArray listOfNewlyCreatedTopLevelElements;

                creator(parent, listOfNewlyCreatedTopLevelElements);

                if (listOfNewlyCreatedTopLevelElements.empty())
                {
                    // This happens when the serialization version numbers DON'T match.
                    QMessageBox(QMessageBox::Critical,
                        "Error",
                        QString("Failed to load elements. The serialization format is incompatible."),
                        QMessageBox::Ok, widget->GetEditorWindow()).exec();

                    // Nothing more to do.
                    return LyShine::EntityArray();
                }

                completeListOfNewlyCreatedTopLevelElements.push_back(listOfNewlyCreatedTopLevelElements);
            }
        }

        // Create the items to go along the elements created above.
        CreateItems(widget, completeListOfNewlyCreatedTopLevelElements);

        return completeListOfNewlyCreatedTopLevelElements;
    }

    //-------------------------------------------------------------------------------

    QTreeWidgetItem* ElementToItem(HierarchyWidget* widget, AZ::Entity* element, bool defaultToInvisibleRootItem)
    {
        if (!element)
        {
            return (defaultToInvisibleRootItem ? widget->invisibleRootItem() : nullptr);
        }

        return ElementToItem(widget, element->GetId(), defaultToInvisibleRootItem);
    }

    QTreeWidgetItem* ElementToItem(HierarchyWidget* widget, AZ::EntityId elementId, bool defaultToInvisibleRootItem)
    {
        if (!elementId.IsValid())
        {
            return (defaultToInvisibleRootItem ? widget->invisibleRootItem() : nullptr);
        }

        auto i = widget->GetEntityItemMap().find(elementId);
        if (i == widget->GetEntityItemMap().end())
        {
            // Not found.
            return (defaultToInvisibleRootItem ? widget->invisibleRootItem() : nullptr);
        }
        else
        {
            // Found.
            return i->second;
        }
    }

    //-------------------------------------------------------------------------------

    QTreeWidgetItem* _GetItem(HierarchyWidget* widget,
        QTreeWidgetItem* i)
    {
        return i;
    }

    QTreeWidgetItem* _GetItem(HierarchyWidget* widget,
        HierarchyItem* i)
    {
        return i;
    }

    QTreeWidgetItem* _GetItem(HierarchyWidget* widget,
        SerializeHelpers::SerializedEntry& e)
    {
        return HierarchyHelpers::ElementToItem(widget, e.m_id, false);
    }

    QTreeWidgetItem* _GetItem(HierarchyWidget* widget,
        AZ::Entity* e)
    {
        return HierarchyHelpers::ElementToItem(widget, e, false);
    }

    QTreeWidgetItem* _GetItem(HierarchyWidget* widget,
        AZ::EntityId e)
    {
        return HierarchyHelpers::ElementToItem(widget, e, false);
    }

    //-------------------------------------------------------------------------------

    void SetSelectedItem(HierarchyWidget* widget,
        AZ::Entity* element)
    {
        LyShine::EntityArray elementUnderCursor;
        if (element)
        {
            elementUnderCursor.push_back(element);
        }
        SetSelectedItems(widget, &elementUnderCursor);
    }

    bool CompareOrderInElementHierarchy(HierarchyItem* item1, HierarchyItem* item2)
    {
        AZ::Entity* entity1 = item1->GetElement();
        AZ::Entity* entity2 = item2->GetElement();

        return EntityHelpers::CompareOrderInElementHierarchy(entity1, entity2);
    }

    void SortByHierarchyOrder(HierarchyItemRawPtrList &itemList)
    {
        itemList.sort(CompareOrderInElementHierarchy);
    }

    //-------------------------------------------------------------------------------
}   // namespace HierarchyHelpers
