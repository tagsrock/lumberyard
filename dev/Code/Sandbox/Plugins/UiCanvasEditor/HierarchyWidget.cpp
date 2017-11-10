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
#include <AzToolsFramework/ToolsComponents/EditorEntityIdContainer.h>

HierarchyWidget::HierarchyWidget(EditorWindow* editorWindow)
    : QTreeWidget()
    , m_isDeleting(false)
    , m_editorWindow(editorWindow)
    , m_entityItemMap()
    , m_itemBeingHovered(nullptr)
    , m_inDragStartState(false)
    , m_selectionChangedBeforeDrag(false)
    , m_signalSelectionChange(true)
{
    setMouseTracking(true);

    // Style.
    {
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setSelectionMode(QAbstractItemView::ExtendedSelection);

        setColumnCount(kHierarchyColumnCount);
        setHeader(new HierarchyHeader(this));

        // IMPORTANT: This MUST be done here.
        // This CAN'T be done inside HierarchyHeader.
        header()->setSectionsClickable(true);

        header()->setSectionResizeMode(kHierarchyColumnName, QHeaderView::Stretch);
        header()->setSectionResizeMode(kHierarchyColumnIsVisible, QHeaderView::Fixed);
        header()->setSectionResizeMode(kHierarchyColumnIsSelectable, QHeaderView::Fixed);

        // This controls the width of the last 2 columns; both in the header and in the body of the HierarchyWidget.
        header()->resizeSection(kHierarchyColumnIsVisible, UICANVASEDITOR_HIERARCHY_HEADER_ICON_SIZE);
        header()->resizeSection(kHierarchyColumnIsSelectable, UICANVASEDITOR_HIERARCHY_HEADER_ICON_SIZE);
    }

    // Connect signals.
    {
        // Selection change notification.
        QObject::connect(selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
            SLOT(CurrentSelectionHasChanged(const QItemSelection &, const QItemSelection &)));

        QObject::connect(model(),
            SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &)),
            SLOT(DataHasChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &)));
    }

    QObject::connect(this,
        &QTreeWidget::itemClicked,
        [this](QTreeWidgetItem* item, int column)
        {
            HierarchyItem* i = dynamic_cast<HierarchyItem*>(item);

            if (column == kHierarchyColumnIsVisible)
            {
                CommandHierarchyItemToggleIsVisible::Push(m_editorWindow->GetActiveStack(),
                    this,
                    HierarchyItemRawPtrList({i}));
            }
            else if (column == kHierarchyColumnIsSelectable)
            {
                CommandHierarchyItemToggleIsSelectable::Push(m_editorWindow->GetActiveStack(),
                    this,
                    HierarchyItemRawPtrList({i}));
            }
        });

    QObject::connect(this,
        &QTreeWidget::itemExpanded,
        [this](QTreeWidgetItem* item)
        {
            CommandHierarchyItemToggleIsExpanded::Push(m_editorWindow->GetActiveStack(),
                this,
                dynamic_cast<HierarchyItem*>(item));
        });

    QObject::connect(this,
        &QTreeWidget::itemCollapsed,
        [this](QTreeWidgetItem* item)
        {
            CommandHierarchyItemToggleIsExpanded::Push(m_editorWindow->GetActiveStack(),
                this,
                dynamic_cast<HierarchyItem*>(item));
        });
}

void HierarchyWidget::SetIsDeleting(bool b)
{
    m_isDeleting = b;
}

EntityHelpers::EntityToHierarchyItemMap& HierarchyWidget::GetEntityItemMap()
{
    return m_entityItemMap;
}

EditorWindow* HierarchyWidget::GetEditorWindow()
{
    return m_editorWindow;
}

void HierarchyWidget::CreateItems(const LyShine::EntityArray& elements)
{
    std::list<AZ::Entity*> elementList(elements.begin(), elements.end());

    // Build the rest of the list.
    // Note: This is a breadth-first traversal thru all child elements.
    for (auto& e : elementList)
    {
        LyShine::EntityArray childElements;
        EBUS_EVENT_ID_RESULT(childElements, e->GetId(), UiElementBus, GetChildElements);
        elementList.insert(elementList.end(), childElements.begin(), childElements.end());
    }

    // Create the items.
    for (auto& e : elementList)
    {
        QTreeWidgetItem* parent = HierarchyHelpers::ElementToItem(this, EntityHelpers::GetParentElement(e), true);

        HierarchyItem* child = new HierarchyItem(m_editorWindow,
                parent,
                e->GetName().c_str(),
                e);

        // Reorder.
        {
            int index = -1;
            EBUS_EVENT_ID_RESULT(index, EntityHelpers::GetParentElement(e)->GetId(), UiElementBus, GetIndexOfChild, e);

            parent->removeChild(child);
            parent->insertChild(index, child);
        }
    }
}

void HierarchyWidget::RecreateItems(const LyShine::EntityArray& elements)
{
    // remember the currently selected items so we can restore them
    EntityHelpers::EntityIdList selectedEntityIds = SelectionHelpers::GetSelectedElementIds(this,
        selectedItems(), false);

    ClearAllHierarchyItemEntityIds();

    // Remove all the items from the list (doesn't delete Entities since we cleared the EntityIds)
    clear();

    CreateItems(elements);

    // restore the expanded state of all items
    ApplyElementIsExpanded();

    HierarchyHelpers::SetSelectedItems(this, &selectedEntityIds);
}

AZ::Entity* HierarchyWidget::CurrentSelectedElement() const
{
    auto currentItem = dynamic_cast<HierarchyItem*>(QTreeWidget::currentItem());
    AZ::Entity* currentElement = (currentItem && currentItem->isSelected()) ? currentItem->GetElement() : nullptr;
    return currentElement;
}

void HierarchyWidget::contextMenuEvent(QContextMenuEvent* ev)
{
    // The context menu.
    {
        HierarchyMenu contextMenu(this,
            (HierarchyMenu::Show::kCutCopyPaste |
             HierarchyMenu::Show::kSavePrefab |
             HierarchyMenu::Show::kNew_EmptyElement |
             HierarchyMenu::Show::kNew_ElementFromPrefabs |
             HierarchyMenu::Show::kDeleteElement |
             HierarchyMenu::Show::kNewSlice |
             HierarchyMenu::Show::kNew_InstantiateSlice |
             HierarchyMenu::Show::kPushToSlice),
            true,
            nullptr);

        contextMenu.exec(ev->globalPos());
    }

    QTreeWidget::contextMenuEvent(ev);
}

void HierarchyWidget::SignalUserSelectionHasChanged(QTreeWidgetItemRawPtrQList& selectedItems)
{
    HierarchyItemRawPtrList items = SelectionHelpers::GetSelectedHierarchyItems(this,
            selectedItems);
    SetUserSelection(items.empty() ? nullptr : &items);
}

void HierarchyWidget::CurrentSelectionHasChanged(const QItemSelection& selected,
    const QItemSelection& deselected)
{
    m_selectionChangedBeforeDrag = true;

    // IMPORTANT: This signal is triggered at the right time, but
    // "selected.indexes()" DOESN'T contain ALL the items currently
    // selected. It ONLY contains the newly selected items. To avoid
    // having to track what's added and removed to the selection,
    // we'll use selectedItems().

    if (m_signalSelectionChange)
    {
        SignalUserSelectionHasChanged(selectedItems());
    }
}

void HierarchyWidget::DataHasChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
    if (topLeft == bottomRight)
    {
        // We only care about text changes, which can ONLY be done one at a
        // time. This implies that topLeft must be the same as bottomRight.

        HierarchyItem* hierarchyItem = dynamic_cast<HierarchyItem*>(itemFromIndex(topLeft));
        AZ::Entity* element = hierarchyItem->GetElement();
        AZ_Assert(element, "No entity found for hierarchy item");
        AZ::EntityId entityId = element->GetId();
        QTreeWidgetItem* item = HierarchyHelpers::ElementToItem(this, element, false);
        QString toName(item ? item->text(0) : "");

        CommandHierarchyItemRename::Push(m_editorWindow->GetActiveStack(),
            this,
            entityId,
            element->GetName().c_str(),
            toName);
    }
}

void HierarchyWidget::HandleItemAdd(HierarchyItem* item)
{
    m_entityItemMap[ item->GetEntityId() ] = item;
}

void HierarchyWidget::HandleItemRemove(HierarchyItem* item)
{
    if (item == m_itemBeingHovered)
    {
        m_itemBeingHovered = nullptr;
    }

    m_entityItemMap.erase(item->GetEntityId());
}

void HierarchyWidget::ReparentItems(bool onCreationOfElement,
    QTreeWidgetItemRawPtrList& baseParentItems,
    HierarchyItemRawPtrList& childItems)
{
    CommandHierarchyItemReparent::Push(onCreationOfElement,
        m_editorWindow->GetActiveStack(),
        this,
        childItems,
        baseParentItems);
}

void HierarchyWidget::ClearAllHierarchyItemEntityIds()
{
    // as a simple way of going through all the HierarchyItem's we use the
    // EntityHelpers::EntityToHierarchyItemMap
    for (auto mapItem : m_entityItemMap)
    {
        mapItem.second->ClearEntityId();
    }
}

void HierarchyWidget::ApplyElementIsExpanded()
{
    // Seed the list.
    HierarchyItemRawPtrList allItems;
    HierarchyHelpers::AppendAllChildrenToEndOfList(m_editorWindow->GetHierarchy()->invisibleRootItem(), allItems);

    // Traverse the list.
    blockSignals(true);
    {
        HierarchyHelpers::TraverseListAndAllChildren(allItems,
            [](HierarchyItem* childItem)
            {
                childItem->ApplyElementIsExpanded();
            });
    }
    blockSignals(false);
}

void HierarchyWidget::mousePressEvent(QMouseEvent* ev)
{
    m_selectionChangedBeforeDrag = false;

    HierarchyItem* item = dynamic_cast<HierarchyItem*>(itemAt(ev->pos()));
    if (!item)
    {
        // This allows the user to UNSELECT an item
        // by clicking in an empty area of the widget.
        SetUniqueSelectionHighlight((QTreeWidgetItem*)nullptr);
    }

    // Remember the selected items before the selection change in case a drag is started.
    // When dragging outside the hierarchy, the selection is reverted back to this selection
    m_beforeDragSelection = selectedItems();

    m_signalSelectionChange = false;

    QTreeWidget::mousePressEvent(ev);

    m_signalSelectionChange = true;
}

void HierarchyWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
    HierarchyItem* item = dynamic_cast<HierarchyItem*>(itemAt(ev->pos()));
    if (item)
    {
        // Double-clicking to edit text is only allowed in the FIRST column.
        for (int col = kHierarchyColumnIsVisible; col < kHierarchyColumnCount; ++col)
        {
            QRect r = visualRect(indexFromItem(item, col));
            if (r.contains(ev->pos()))
            {
                // Ignore the event.
                return;
            }
        }
    }

    QTreeWidget::mouseDoubleClickEvent(ev);
}

void HierarchyWidget::startDrag(Qt::DropActions supportedActions)
{
    // This flag is used to determine whether to perform an action on leaveEvent.
    // If an item is dragged really fast outside the hierarchy, this startDrag event is called,
    // but the dragEnterEvent and dragLeaveEvent are replaced with the leaveEvent
    m_inDragStartState = true;

    // Remember the current selection so that we can revert back to it when the items are dragged back into the hierarchy
    m_dragSelection = selectedItems();

    QTreeView::startDrag(supportedActions);
}

void HierarchyWidget::dragEnterEvent(QDragEnterEvent * event)
{
    if (!AcceptsMimeData(event->mimeData()))
    {
        return;
    }

    m_inDragStartState = false;

    if (m_selectionChangedBeforeDrag)
    {
        m_signalSelectionChange = false;

        // Set the current selection to the items being dragged
        clearSelection();
        for (auto i : m_dragSelection)
        {
            i->setSelected(true);
        }

        m_signalSelectionChange = true;
    }

    QTreeView::dragEnterEvent(event);
}

void HierarchyWidget::dragLeaveEvent(QDragLeaveEvent * event)
{
    // This is called when dragging outside the hierarchy, or when a drag is released inside the hierarchy
    // but a dropEvent isn't called (ex. drop item onto itself or press Esc to cancel a drag)

    // Check if mouse position is inside or outside the hierarchy
    QRect widgetRect = geometry();
    QPoint mousePos = mapFromGlobal(QCursor::pos());
    if (widgetRect.contains(mousePos))
    {
        if (m_selectionChangedBeforeDrag)
        {
            // Treat this event as a mouse release (mouseReleaseEvent is not called in this case)
            SignalUserSelectionHasChanged(selectedItems());
        }
    }
    else
    {
        if (m_selectionChangedBeforeDrag)
        {
            m_signalSelectionChange = false;

            // Set the current selection to the items that were selected before the drag
            clearSelection();
            for (auto i : m_beforeDragSelection)
            {
                i->setSelected(true);
            }

            m_signalSelectionChange = true;
        }
    }

    QTreeView::dragLeaveEvent(event);
}

void HierarchyWidget::dropEvent(QDropEvent* ev)
{
    m_inDragStartState = false;
    
    m_signalSelectionChange = false;

    // Get a list of selected items
    QTreeWidgetItemRawPtrQList& selection = selectedItems();

    // Change current selection to only contain top level items. This avoids
    // the default drop behavior from changing the internal hierarchy of 
    // the dragged elements
    QTreeWidgetItemRawPtrQList topLevelSelection;
    SelectionHelpers::GetListOfTopLevelSelectedItems(this, selection, topLevelSelection);
    clearSelection();
    for (auto i : topLevelSelection)
    {
        i->setSelected(true);
    }

    // Set current parent and child index of each selected item
    for (auto i : selection)
    {
        HierarchyItem* item = dynamic_cast<HierarchyItem*>(i);
        if (item)
        {
            QModelIndex itemIndex = indexFromItem(item);

            QTreeWidgetItem* baseParentItem = itemFromIndex(itemIndex.parent());
            if (!baseParentItem)
            {
                baseParentItem = invisibleRootItem();
            }
            HierarchyItem* parentItem = dynamic_cast<HierarchyItem*>(baseParentItem);
            AZ::EntityId parentId = (parentItem ? parentItem->GetEntityId() : AZ::EntityId());

            item->SetPreMove(parentId, itemIndex.row());
        }
    }

    // Do the drop event
    ev->setDropAction(Qt::MoveAction);
    QTreeWidget::dropEvent(ev);

    // Make a list of selected items and their parents
    HierarchyItemRawPtrList childItems;
    QTreeWidgetItemRawPtrList baseParentItems;

    bool itemMoved = false;

    for (auto i : selection)
    {
        HierarchyItem* item = dynamic_cast<HierarchyItem*>(i);
        if (item)
        {
            QModelIndex index = indexFromItem(item);

            QTreeWidgetItem* baseParentItem = itemFromIndex(index.parent()); 
            if (!baseParentItem)
            {
                baseParentItem = invisibleRootItem();
            }
            HierarchyItem* parentItem = dynamic_cast<HierarchyItem*>(baseParentItem);
            AZ::EntityId parentId = parentItem ? parentItem->GetEntityId() : AZ::EntityId();

            if ((item->GetPreMoveChildRow() != index.row()) || (item->GetPreMoveParentId() != parentId))
            {
                // Item has moved
                itemMoved = true;
            }

            childItems.push_back(item);
            baseParentItems.push_back(baseParentItem);
        }
    }

    if (itemMoved)
    {
        ReparentItems(false, baseParentItems, childItems);
    }
    else
    {
        // Items didn't move, but they became unselected so they need to be reselected
        for (auto i : childItems)
        {
            i->setSelected(true);
        }
    }

    m_signalSelectionChange = true;

    if (m_selectionChangedBeforeDrag)
    {
        // Signal a selection change on the mouse release
        SignalUserSelectionHasChanged(selectedItems());
    }
}

QStringList HierarchyWidget::mimeTypes() const
{
    QStringList list = QTreeWidget::mimeTypes();
    list.append(AzToolsFramework::EditorEntityIdContainer::GetMimeType());
    return list;
}

QMimeData* HierarchyWidget::mimeData(const QList<QTreeWidgetItem*> items) const
{
    AzToolsFramework::EditorEntityIdContainer entityIdList;
    for (auto i : items)
    {
        HierarchyItem* item = dynamic_cast<HierarchyItem*>(i);
        AZ::EntityId entityId = item->GetEntityId();
        if (entityId.IsValid())
        {
            entityIdList.m_entityIds.push_back(entityId);
        }
    }
    if (entityIdList.m_entityIds.empty())
    {
        return nullptr;
    }

    AZStd::vector<char> encoded;
    if (!entityIdList.ToBuffer(encoded))
    {
        return nullptr;
    }

    QMimeData* mimeDataPtr = new QMimeData();
    QByteArray encodedData;
    encodedData.resize((int)encoded.size());
    memcpy(encodedData.data(), encoded.data(), encoded.size());

    mimeDataPtr->setData(AzToolsFramework::EditorEntityIdContainer::GetMimeType(), encodedData);
    return mimeDataPtr;
}

bool HierarchyWidget::AcceptsMimeData(const QMimeData *mimeData)
{
    if (!mimeData || !mimeData->hasFormat(AzToolsFramework::EditorEntityIdContainer::GetMimeType()))
    {
        return false;
    }

    QByteArray arrayData = mimeData->data(AzToolsFramework::EditorEntityIdContainer::GetMimeType());

    AzToolsFramework::EditorEntityIdContainer entityIdListContainer;
    if (!entityIdListContainer.FromBuffer(arrayData.constData(), arrayData.size()))
    {
        return false;
    }

    if (entityIdListContainer.m_entityIds.empty())
    {
        return false;
    }

    // Get the entity context that the first dragged entity is attached to
    AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
    EBUS_EVENT_ID_RESULT(contextId, entityIdListContainer.m_entityIds[0], AzFramework::EntityIdContextQueryBus, GetOwningContextId);
    if (contextId.IsNull())
    {
        return false;
    }

    // Check that the entity context is the UI editor entity context
    UiEditorEntityContext* editorEntityContext = m_editorWindow->GetEntityContext();
    if (!editorEntityContext || (editorEntityContext->GetContextId() != contextId))
    {
        return false;
    }

    return true;
}

void HierarchyWidget::mouseMoveEvent(QMouseEvent* ev)
{
    HierarchyItem* itemBeingHovered = dynamic_cast<HierarchyItem*>(itemAt(ev->pos()));
    if (itemBeingHovered)
    {
        // Hovering.

        if (m_itemBeingHovered)
        {
            if (itemBeingHovered == m_itemBeingHovered)
            {
                // Still hovering over the same item.
                // Nothing to do.
            }
            else
            {
                // Hover start over a different item.

                // Hover ends over the previous item.
                m_itemBeingHovered->SetMouseIsHovering(false);

                // Hover starts over the current item.
                m_itemBeingHovered = itemBeingHovered;
                m_itemBeingHovered->SetMouseIsHovering(true);
            }
        }
        else
        {
            // Hover start.
            m_itemBeingHovered = itemBeingHovered;
            m_itemBeingHovered->SetMouseIsHovering(true);
        }
    }
    else
    {
        // Not hovering.

        if (m_itemBeingHovered)
        {
            // Hover end.
            m_itemBeingHovered->SetMouseIsHovering(false);
            m_itemBeingHovered = nullptr;
        }
        else
        {
            // Still not hovering.
            // Nothing to do.
        }
    }

    QTreeWidget::mouseMoveEvent(ev);
}

void HierarchyWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_selectionChangedBeforeDrag)
    {
        // Signal a selection change on the mouse release
        SignalUserSelectionHasChanged(selectedItems());
    }

    QTreeWidget::mouseReleaseEvent(ev);
}

void HierarchyWidget::leaveEvent(QEvent* ev)
{
    ClearItemBeingHovered();

    // If an item is dragged really fast outside the hierarchy, the startDrag event is called,
    // but the dragEnterEvent and dragLeaveEvent are replaced with the leaveEvent.
    // In this case, perform the dragLeaveEvent here
    if (m_inDragStartState)
    {
        QTreeWidgetItemRawPtrQList& selection = selectedItems();

        if (m_selectionChangedBeforeDrag)
        {
            m_signalSelectionChange = false;

            // Set the current selection to the items that were selected before the drag
            clearSelection();
            for (auto i : m_beforeDragSelection)
            {
                i->setSelected(true);
            }

            m_signalSelectionChange = true;
        }

        m_inDragStartState = false;
    }

    QTreeWidget::leaveEvent(ev);
}

void HierarchyWidget::ClearItemBeingHovered()
{
    if (!m_itemBeingHovered)
    {
        // Nothing to do.
        return;
    }

    m_itemBeingHovered->SetMouseIsHovering(false);
    m_itemBeingHovered = nullptr;
}

void HierarchyWidget::DeleteSelectedItems()
{
    DeleteSelectedItems(selectedItems());
}

void HierarchyWidget::DeleteSelectedItems(QTreeWidgetItemRawPtrQList& selectedItems)
{
    CommandHierarchyItemDelete::Push(m_editorWindow->GetActiveStack(),
        this,
        selectedItems);

    // This ensures there's no "current item".
    SetUniqueSelectionHighlight((QTreeWidgetItem*)nullptr);

    // IMPORTANT: This is necessary to indirectly trigger detach()
    // in the PropertiesWidget.
    SetUserSelection(nullptr);
}

void HierarchyWidget::Cut()
{
    QTreeWidgetItemRawPtrQList& selection = selectedItems();

    HierarchyClipboard::CopySelectedItemsToClipboard(this,
        selection);
    DeleteSelectedItems(selection);
}

void HierarchyWidget::Copy()
{
    HierarchyClipboard::CopySelectedItemsToClipboard(this,
        selectedItems());
}

void HierarchyWidget::PasteAsSibling()
{
    HierarchyClipboard::CreateElementsFromClipboard(this,
        selectedItems(),
        false);
}

void HierarchyWidget::PasteAsChild()
{
    HierarchyClipboard::CreateElementsFromClipboard(this,
        selectedItems(),
        true);
}

void HierarchyWidget::AddElement(QTreeWidgetItemRawPtrQList& selectedItems, const QPoint* optionalPos)
{
    CommandHierarchyItemCreate::Push(m_editorWindow->GetActiveStack(),
        this,
        selectedItems,
        [optionalPos](AZ::Entity* element)
        {
            if (optionalPos)
            {
                EntityHelpers::MoveElementToGlobalPosition(element, *optionalPos);
            }
        });
}

void HierarchyWidget::SetUniqueSelectionHighlight(QTreeWidgetItem* item)
{
    clearSelection();

    setCurrentIndex(indexFromItem(item));
}

void HierarchyWidget::SetUniqueSelectionHighlight(AZ::Entity* element)
{
    SetUniqueSelectionHighlight(HierarchyHelpers::ElementToItem(this, element, false));
}

#include <HierarchyWidget.moc>
