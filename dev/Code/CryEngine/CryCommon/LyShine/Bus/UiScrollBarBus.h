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
#include <LyShine/UiBase.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiScrollBarInterface
    : public AZ::ComponentBus
{
public: // member functions

    virtual ~UiScrollBarInterface() {}

    //! Get the size of the handle relative to the scrollbar (0 - 1)
    virtual float GetHandleSize() = 0;

    //! Set the size of the handle relative to the scrollbar (0 - 1)
    virtual void SetHandleSize(float size) = 0;

    //! Get the minimum size of the handle in pixels
    virtual float GetMinHandlePixelSize() = 0;

    //! Set the minimum size of the handle in pixels
    virtual void SetMinHandlePixelSize(float size) = 0;

    //! Get the handle entity
    virtual AZ::EntityId GetHandleEntity() = 0;

    //! Set the handle entity
    virtual void SetHandleEntity(AZ::EntityId entityId) = 0;

public: // static member data

    //! Only one component on a entity can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
};

typedef AZ::EBus<UiScrollBarInterface> UiScrollBarBus;
