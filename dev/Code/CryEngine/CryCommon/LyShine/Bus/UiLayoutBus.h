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
#pragma once

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Vector2.h>
#include <LyShine/Bus/UiTransformBus.h>
#include <LyShine/IDraw2d.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiLayoutInterface
    : public AZ::ComponentBus
{
public: // types

    //! Horizontal order used by layout components
    enum class HorizontalOrder
    {
        LeftToRight,
        RightToLeft
    };

    //! Vertical order used by layout components
    enum class VerticalOrder
    {
        TopToBottom,
        BottomToTop
    };

    //! Padding (in pixels) inside the edges of an element
    struct Padding
    {
        AZ_TYPE_INFO(Padding, "{DE5C18B0-4214-4A37-B590-8D45CC450A96}")

        Padding()
            : m_left(0)
            , m_top(0)
            , m_right(0)
            , m_bottom(0) {}

        int m_left;
        int m_right;
        int m_top;
        int m_bottom;
    };

public: // member functions

    virtual ~UiLayoutInterface() {}

    //! Set the child elements' width transform properties
    virtual void ApplyLayoutWidth() = 0;

    //! Set the child elements' height transform properties
    virtual void ApplyLayoutHeight() = 0;

    //! Get whether this layout component uses layout cells to calculate its layout
    virtual bool IsUsingLayoutCellsToCalculateLayout() = 0;

    //! Get whether this layout component should bypass the default layout cell values calculated by its children
    virtual bool GetIgnoreDefaultLayoutCells() = 0;

    //! Set whether this layout component should bypass the default layout cell values calculated by its children
    virtual void SetIgnoreDefaultLayoutCells(bool ignoreDefaultLayoutCells) = 0;

    //! Get the horizontal child alignment
    virtual IDraw2d::HAlign GetHorizontalChildAlignment() = 0;

    //! Set the horizontal child alignment
    virtual void SetHorizontalChildAlignment(IDraw2d::HAlign alignment) = 0;

    //! Get the vertical child alignment
    virtual IDraw2d::VAlign GetVerticalChildAlignment() = 0;

    //! Set the vertical child alignment
    virtual void SetVerticalChildAlignment(IDraw2d::VAlign alignment) = 0;

    //! Find out whether this layout component is currently overriding the transform of the specified element.
    virtual bool IsControllingChild(AZ::EntityId childId) = 0;

    //! Get the size the element needs to be to fit a specified number of child elements of a certain size
    virtual AZ::Vector2 GetSizeToFitChildElements(const AZ::Vector2& childElementSize, int numChildElements) = 0;

public: // static member data

    //! Only one component on a entity can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
};

typedef AZ::EBus<UiLayoutInterface> UiLayoutBus;
