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

#include "PropertyHandlerOffset.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtWidgets/QWidget>

#include <LyShine/Bus/UiTransform2dBus.h>

Q_DECLARE_METATYPE(UiTransform2dInterface::Anchors);

void PropertyHandlerOffset::WriteGUIValuesIntoProperty(size_t index, AzToolsFramework::PropertyVectorCtrl* GUI, UiTransform2dInterface::Offsets& instance, AzToolsFramework::InstanceDataNode* node)
{
    AZ::EntityId id = GetParentEntityId(node, index);

    // Check if this element is being controlled by its parent
    bool isControlledByParent = false;
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, id, UiElementBus, GetParent);
    if (parentElement)
    {
        EBUS_EVENT_ID_RESULT(isControlledByParent, parentElement->GetId(), UiLayoutBus, IsControllingChild, id);
    }

    if (isControlledByParent)
    {
        return;
    }

    UiTransform2dInterface::Anchors anchors;
    EBUS_EVENT_ID_RESULT(anchors, id, UiTransform2dBus, GetAnchors);

    AZ::Vector2 pivot;
    EBUS_EVENT_ID_RESULT(pivot, id, UiTransformBus, GetPivot);

    AZStd::string labels[4];
    GetLabels(anchors, labels);

    AzToolsFramework::VectorElement** elements = GUI->getElements();

    UiTransform2dInterface::Offsets guiDisplayedOffset = ExtractValuesFromGUI(GUI);

    // Set the new display offsets for the element being edited
    UiTransform2dInterface::Offsets newDisplayedOffset = InternalOffsetToDisplayedOffset(instance, anchors, pivot);
    int idx = 0;
    if (elements[idx]->WasValueEditedByUser())
    {
        QLabel* label = elements[idx]->GetLabel();
        if (label && (label->text() == labels[idx].c_str()))
        {
            newDisplayedOffset.m_left = guiDisplayedOffset.m_left;
        }
    }
    idx++;
    if (elements[idx]->WasValueEditedByUser())
    {
        QLabel* label = elements[idx]->GetLabel();
        if (label && (label->text() == labels[idx].c_str()))
        {
            newDisplayedOffset.m_top = guiDisplayedOffset.m_top;
        }
    }
    idx++;
    if (elements[idx]->WasValueEditedByUser())
    {
        QLabel* label = elements[idx]->GetLabel();
        if (label && (label->text() == labels[idx].c_str()))
        {
            newDisplayedOffset.m_right = guiDisplayedOffset.m_right;
        }
    }
    idx++;
    if (elements[idx]->WasValueEditedByUser())
    {
        QLabel* label = elements[idx]->GetLabel();
        if (label && (label->text() == labels[idx].c_str()))
        {
            newDisplayedOffset.m_bottom = guiDisplayedOffset.m_bottom;
        }
    }

    UiTransform2dInterface::Offsets newInternalOffset;
    newInternalOffset = DisplayedOffsetToInternalOffset(newDisplayedOffset, anchors, pivot);

    // IMPORTANT: This will indirectly update "instance".
    EBUS_EVENT_ID(id, UiTransform2dBus, SetOffsets, newInternalOffset);
}

bool PropertyHandlerOffset::ReadValuesIntoGUI(size_t index, AzToolsFramework::PropertyVectorCtrl* GUI, const UiTransform2dInterface::Offsets& instance, AzToolsFramework::InstanceDataNode* node)
{
    (int)index;

    // IMPORTANT: We DON'T need to do validation of data here because that's
    // done for us BEFORE we get here. We DO need to set the labels here.

    AZ::EntityId id = GetParentEntityId(node, index);

    UiTransform2dInterface::Anchors anchors;
    EBUS_EVENT_ID_RESULT(anchors, id, UiTransform2dBus, GetAnchors);

    // Set the labels.
    {
        SetLabels(GUI, anchors);
    }

    GUI->blockSignals(true);
    {
        AZ::Vector2 pivot;
        EBUS_EVENT_ID_RESULT(pivot, id, UiTransformBus, GetPivot);

        UiTransform2dInterface::Offsets displayedOffset = InternalOffsetToDisplayedOffset(instance, anchors, pivot);
        InsertValuesIntoGUI(GUI, displayedOffset);
    }
    GUI->blockSignals(false);

    return false;
}

void PropertyHandlerOffset::GetLabels(UiTransform2dInterface::Anchors& anchors, AZStd::string* labelsOut)
{
    labelsOut[0] = "Left";
    labelsOut[1] = "Top";
    labelsOut[2] = "Right";
    labelsOut[3] = "Bottom";

    // If the left and right anchors are the same, allow editing x position and width
    if (anchors.m_left == anchors.m_right)
    {
        labelsOut[0] = "X Pos";
        labelsOut[2] = "Width";
    }

    // If the top and bottom anchors are the same, allow editing y position and height
    if (anchors.m_top == anchors.m_bottom)
    {
        labelsOut[1] = "Y Pos";
        labelsOut[3] = "Height";
    }
}

void PropertyHandlerOffset::SetLabels(AzToolsFramework::PropertyVectorCtrl* ctrl,
    UiTransform2dInterface::Anchors& anchors)
{
    AZStd::string labels[4];
    GetLabels(anchors, labels);

    for (int i = 0; i < 4; i++)
    {
        ctrl->setLabel(i, labels[i]);
    }
}

AZ::EntityId PropertyHandlerOffset::GetParentEntityId(AzToolsFramework::InstanceDataNode* node, size_t index)
{
    while (node)
    {
        if ((node->GetClassMetadata()) && (node->GetClassMetadata()->m_azRtti))
        {
            if (node->GetClassMetadata()->m_azRtti->IsTypeOf(AZ::Component::RTTI_Type()))
            {
                return static_cast<AZ::Component*>(node->GetInstance(index))->GetEntityId();
            }
        }
        node = node->GetParent();
    }

    return AZ::EntityId();
}

UiTransform2dInterface::Offsets PropertyHandlerOffset::InternalOffsetToDisplayedOffset(UiTransform2dInterface::Offsets internalOffset,
    const UiTransform2dInterface::Anchors& anchors,
    const AZ::Vector2& pivot)
{
    // This is complex because the X offsets can be displayed
    // as either left & right or as xpos & width and the Y offsets can be displayed
    // as either top & bottom or as ypos and height.

    UiTransform2dInterface::Offsets displayedOffset = internalOffset;

    // If the left and right anchors are the same, allow editing x position and width
    if (anchors.m_left == anchors.m_right)
    {
        float width = internalOffset.m_right - internalOffset.m_left;

        // width
        displayedOffset.m_right = width;

        // X Pos
        displayedOffset.m_left = internalOffset.m_left + pivot.GetX() * width;
    }

    // If the top and bottom anchors are the same, allow editing y position and height
    if (anchors.m_top == anchors.m_bottom)
    {
        float height = internalOffset.m_bottom - internalOffset.m_top;

        // height
        displayedOffset.m_bottom = height;

        // Y Pos
        displayedOffset.m_top = internalOffset.m_top + pivot.GetY() * height;
    }

    return displayedOffset;
}

UiTransform2dInterface::Offsets PropertyHandlerOffset::DisplayedOffsetToInternalOffset(UiTransform2dInterface::Offsets displayedOffset,
    const UiTransform2dInterface::Anchors& anchors,
    const AZ::Vector2& pivot)
{
    UiTransform2dInterface::Offsets internalOffset = displayedOffset;

    if (anchors.m_left == anchors.m_right)
    {
        // flipping of offsets is not allowed, so if width is negative make it zero
        float xPos = displayedOffset.m_left;
        float width = AZ::GetMax(0.0f, displayedOffset.m_right);

        internalOffset.m_left = xPos - pivot.GetX() * width;
        internalOffset.m_right = internalOffset.m_left + width;
    }

    if (anchors.m_top == anchors.m_bottom)
    {
        // flipping of offsets is not allowed, so if height is negative make it zero
        float yPos = displayedOffset.m_top;
        float height = AZ::GetMax(0.0f, displayedOffset.m_bottom);

        internalOffset.m_top = yPos - pivot.GetY() * height;
        internalOffset.m_bottom = internalOffset.m_top + height;
    }

    return internalOffset;
}

void PropertyHandlerOffset::Register()
{
    qRegisterMetaType<UiTransform2dInterface::Anchors>("UiTransform2dInterface::Anchors");
    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew PropertyHandlerOffset());
}

#include <PropertyHandlerOffset.moc>
