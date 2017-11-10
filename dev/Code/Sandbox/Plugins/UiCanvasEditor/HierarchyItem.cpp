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
#include <AzCore/Asset/AssetManager.h>
#include <LyShine/UiComponentTypes.h>

#define UICANVASEDITOR_HIERARCHY_ICON_OPEN                  ":/Icons/Eye_Open.tif"
#define UICANVASEDITOR_HIERARCHY_ICON_OPEN_HIDDEN           ":/Icons/Eye_Open_Hidden.tif"
#define UICANVASEDITOR_HIERARCHY_ICON_OPEN_HOVER            ":/Icons/Eye_Open_Hover.tif"
#define UICANVASEDITOR_HIERARCHY_ICON_PADLOCK_ENABLED_HOVER ":/Icons/Padlock_Enabled_Hover.tif"
#define UICANVASEDITOR_HIERARCHY_ICON_PADLOCK_ENABLED       ":/Icons/Padlock_Enabled.tif"

HierarchyItem::HierarchyItem(EditorWindow* editWindow,
    QTreeWidgetItem* parent,
    const QString label,
    AZ::Entity* optionalElement)
    : QObject()
    , QTreeWidgetItem(parent, QStringList(label))
    , m_editorWindow(editWindow)
    , m_elementId(optionalElement ? optionalElement->GetId() : AZ::EntityId())
    , m_mark(false)
    , m_preMoveChildRow(-1)
    , m_mouseIsHovering(false)
    , m_nonSnappedOffsets()
    , m_nonSnappedZRotation(0.0f)
{
    // Element.
    if (optionalElement)
    {
        // IMPORTANT: We provided an optionalElement, which means that
        // we're building the UI for an existing element, in an existing
        // canvas. Therefore we DON'T have to automatically create an
        // element for this HierarchyItem.
    }
    else
    {
        AZ::Entity* element = nullptr;

        // The element DIDN'T already exists.
        // Create the element.
        EBUS_EVENT_ID_RESULT(element, editWindow->GetCanvas(), UiCanvasBus,
            CreateChildElement, label.toStdString().c_str());

        if (element->GetState() == AZ::Entity::ES_ACTIVE)
        {
            element->Deactivate();    // deactivate so that we can add components
        }

        // add a transform component to the element - all UI elements have a transform
        element->CreateComponent(LyShine::UiTransform2dComponentUuid);

        if (element->GetState() == AZ::Entity::ES_CONSTRUCTED)
        {
            element->Init();      // init
        }

        if (element->GetState() == AZ::Entity::ES_INIT)
        {
            element->Activate();      // activate
        }

        m_elementId = element->GetId();
    }

    AZ_Assert(m_elementId.IsValid(), "Invalid element ID");

    // Connect signals.
    {
        // Register with the entity map for quick lookup.

        QObject::connect(this,
            SIGNAL(SignalItemAdd(HierarchyItem*)),
            m_editorWindow->GetHierarchy(),
            SLOT(HandleItemAdd(HierarchyItem*)));

        QObject::connect(this,
            SIGNAL(SignalItemRemove(HierarchyItem*)),
            m_editorWindow->GetHierarchy(),
            SLOT(HandleItemRemove(HierarchyItem*)));
    }

    // Add to the entity map for quick lookup.
    //
    // IMPORTANT: This MUST be done BEFORE changing the
    // behavior and look of this class.
    SignalItemAdd(this);

    // Behavior and look.
    //
    // IMPORTANT: This MUST be done AFTER SignalItemAdd().
    {
        setFlags(flags() |
            Qt::ItemIsEditable |
            Qt::ItemIsDragEnabled |
            Qt::ItemIsDropEnabled);

        UpdateIcon();
    }
}

HierarchyItem::~HierarchyItem()
{
    DeleteElement();

    // Remove from quick lookup entity map.
    SignalItemRemove(this);
}

void HierarchyItem::DeleteElement()
{
    // IMPORTANT: DeleteElement() can be called from ~HierarchyItem().
    // Parent HierarchyItem are destroyed BEFORE their children.
    // When a parent HierarchyItem is destroyed, all its AZ::Entity
    // children are destroyed. Therefore, it's NECESSARY to use
    // SAFE_DELETE(). That's because our AZ::Entity might have already
    // been deleted. In which case GetElement() will return nullptr.
    // ~HierarchyItem() is the ONLY place where GetElement() is allowed
    // return nullptr.
    EBUS_EVENT_ID(m_elementId, UiElementBus, DestroyElement);
}

AZ::Entity* HierarchyItem::GetElement() const
{
    // IMPORTANT: "element" will NEVER be nullptr, EXCEPT in ~HierarchyItem().
    // In the ~HierarchyItem(), deleting the parent of our element will cause
    // our own element to be destroyed. ~HierarchyItem() is the ONLY place
    // where we CAN'T assume that our m_elementId is always valid.
    return EntityHelpers::GetEntity(m_elementId);
}

AZ::EntityId HierarchyItem::GetEntityId() const
{
    return m_elementId;
}

void HierarchyItem::ClearEntityId()
{
    m_elementId.SetInvalid();
}

void HierarchyItem::SetMouseIsHovering(bool isHovering)
{
    m_mouseIsHovering = isHovering;

    UpdateIcon();
}

void HierarchyItem::SetIsExpanded(bool isExpanded)
{
    // Runtime-side.
    EBUS_EVENT_ID(m_elementId, UiEditorBus, SetIsExpanded, isExpanded);

    // Editor-side.
    setExpanded(isExpanded);
}

void HierarchyItem::ApplyElementIsExpanded()
{
    bool isExpanded = false;
    EBUS_EVENT_ID_RESULT(isExpanded, m_elementId, UiEditorBus, GetIsExpanded);

    setExpanded(isExpanded);
}

void HierarchyItem::SetIsSelectable(bool isSelectable)
{
    // Runtime-side.
    EBUS_EVENT_ID(m_elementId, UiEditorBus, SetIsSelectable, isSelectable);

    // Editor-side.
    UpdateIcon();
    UpdateChildIcon();
    m_editorWindow->GetViewport()->Refresh();
}

void HierarchyItem::SetIsSelected(bool isSelected)
{
    // Runtime-side.
    EBUS_EVENT_ID(m_elementId, UiEditorBus, SetIsSelected, isSelected);

    // Editor-side.
    setSelected(isSelected);
    UpdateIcon();
    m_editorWindow->GetViewport()->Refresh();
}

void HierarchyItem::SetIsVisible(bool isVisible)
{
    // Runtime-side.
    EBUS_EVENT_ID(m_elementId, UiEditorBus, SetIsVisible, isVisible);

    // Editor-side.
    UpdateIcon();
    UpdateChildIcon();
    m_editorWindow->GetViewport()->Refresh();
}

void HierarchyItem::UpdateIcon()
{
    // Eye icon.
    {
        const char* textureName = nullptr;

        bool isVisible = false;
        EBUS_EVENT_ID_RESULT(isVisible, m_elementId, UiEditorBus, GetIsVisible);

        if (isVisible)
        {
            // This item is visible.

            bool areAllAncestorsVisible = true;
            EBUS_EVENT_ID_RESULT(areAllAncestorsVisible, m_elementId, UiEditorBus, AreAllAncestorsVisible);

            textureName = (m_mouseIsHovering ? UICANVASEDITOR_HIERARCHY_ICON_OPEN_HOVER : (areAllAncestorsVisible ? UICANVASEDITOR_HIERARCHY_ICON_OPEN : UICANVASEDITOR_HIERARCHY_ICON_OPEN_HIDDEN));
        }
        else
        {
            // This item is NOT visible.
            textureName = (m_mouseIsHovering ? UICANVASEDITOR_HIERARCHY_ICON_OPEN_HIDDEN : "");
        }

        setIcon(kHierarchyColumnIsVisible, QIcon(textureName).pixmap(UICANVASEDITOR_HIERARCHY_HEADER_ICON_SIZE, UICANVASEDITOR_HIERARCHY_HEADER_ICON_SIZE));
    }

    // Padlock icon.
    {
        const char* textureName = nullptr;

        bool isSelectable = false;
        EBUS_EVENT_ID_RESULT(isSelectable, m_elementId, UiEditorBus, GetIsSelectable);

        if (isSelectable)
        {
            // This item is NOT locked.
            textureName = (m_mouseIsHovering ? UICANVASEDITOR_HIERARCHY_ICON_PADLOCK_ENABLED : "");
        }
        else
        {
            // This item is locked.
            textureName = (m_mouseIsHovering ? UICANVASEDITOR_HIERARCHY_ICON_PADLOCK_ENABLED_HOVER : UICANVASEDITOR_HIERARCHY_ICON_PADLOCK_ENABLED);
        }

        setIcon(kHierarchyColumnIsSelectable, QIcon(textureName).pixmap(UICANVASEDITOR_HIERARCHY_HEADER_ICON_SIZE, UICANVASEDITOR_HIERARCHY_HEADER_ICON_SIZE));
    }
}

void HierarchyItem::UpdateChildIcon()
{
    // Seed the list.
    HierarchyItemRawPtrList items;
    HierarchyHelpers::AppendAllChildrenToEndOfList(this, items);

    // Update child icons.
    HierarchyHelpers::TraverseListAndAllChildren(items,
        [](HierarchyItem* childItem)
        {
            childItem->UpdateIcon();
        });
}

HierarchyItem* HierarchyItem::Parent() const
{
    // It's ok to return a nullptr.
    // nullptr normally happens when we've reached the invisibleRootItem(),
    // We DON'T consider the invisibleRootItem() the parent of a HierarchyItem.
    return dynamic_cast<HierarchyItem*>(QTreeWidgetItem::parent());
}

HierarchyItem* HierarchyItem::Child(int i) const
{
    HierarchyItem* item = dynamic_cast<HierarchyItem*>(QTreeWidgetItem::child(i));
    AZ_Assert(item, "There's an item in the Hierarchy that isn't a HierarchyItem.");

    return item;
}

void HierarchyItem::SetMark(bool m)
{
    m_mark = m;
}

bool HierarchyItem::GetMark()
{
    return m_mark;
}

void HierarchyItem::SetPreMove(AZ::EntityId parentId, int childRow)
{
    m_preMoveParentId = parentId;
    m_preMoveChildRow = childRow;
}

AZ::EntityId HierarchyItem::GetPreMoveParentId()
{
    return m_preMoveParentId;
}

int HierarchyItem::GetPreMoveChildRow()
{
    return m_preMoveChildRow;
}

void HierarchyItem::ReplaceElement(const AZStd::string& xml, const AZStd::unordered_set<AZ::Data::AssetId>& referencedSliceAssets)
{
    AZ_Assert(!xml.empty(), "XML is empty");

    AZ::Entity* parentEntity = Parent() ? Parent()->GetElement() : nullptr;
    AZ::Entity* replaceEntity = GetElement();

    // find the element after the one to be replaced
    AZ::Entity* insertBeforeEntity = nullptr;
    {
        LyShine::EntityArray childElements;
        if (parentEntity)
        {
            EBUS_EVENT_ID_RESULT(childElements, parentEntity->GetId(), UiElementBus, GetChildElements);
        }
        else
        {
            EBUS_EVENT_ID_RESULT(childElements, m_editorWindow->GetCanvas(), UiCanvasBus, GetChildElements);
        }

        // find the enity we are replacing in the list (it must exist)
        auto iter = std::find(childElements.begin(), childElements.end(), replaceEntity);
        AZ_Assert(iter != childElements.end(), "Entity not found");

        // if there is an element after the replace element then that is the one to insert before
        if (++iter != childElements.end())
        {
            insertBeforeEntity = *iter;
        }
    }

    // If restoring to a slice, keep a reference to the slice asset so it isn't released when the entity
    // is deleted, only to immediately reload upon restoring.
    AZStd::vector<AZ::Data::Asset<AZ::SliceAsset>> preservedAssetsRefs;
    for (auto assetId : referencedSliceAssets)
    {
        preservedAssetsRefs.push_back(AZ::Data::AssetManager::Instance().FindAsset(assetId));
    }

    // Discard the old element.
    DeleteElement();

    // Load the new element.
    SerializeHelpers::RestoreSerializedElements(m_editorWindow->GetCanvas(),
        parentEntity,
        insertBeforeEntity,
        m_editorWindow->GetEntityContext(),
        xml,
        false,
        nullptr);
}

void HierarchyItem::SetNonSnappedOffsets(UiTransform2dInterface::Offsets offsets)
{
    m_nonSnappedOffsets = offsets;
}

UiTransform2dInterface::Offsets HierarchyItem::GetNonSnappedOffsets()
{
    return m_nonSnappedOffsets;
}

void HierarchyItem::SetNonSnappedZRotation(float rotation)
{
    m_nonSnappedZRotation = rotation;
}

float HierarchyItem::GetNonSnappedZRotation()
{
    return m_nonSnappedZRotation;
}

#include <HierarchyItem.moc>
