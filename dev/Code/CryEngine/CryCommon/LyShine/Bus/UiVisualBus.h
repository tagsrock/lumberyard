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
#include <AzCore/Math/Color.h>
#include <IFont.h>

// Forward declarations
class ISprite;

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiVisualInterface
    : public AZ::ComponentBus
{
public: // member functions

    virtual ~UiVisualInterface() {}

    //! Reset the overrides, used when setting interactable states
    virtual void ResetOverrides() = 0;

    //! Set the override color, used for interactable states
    virtual void SetOverrideColor(const AZ::Color& color) = 0;

    //! Set the override alpha, used for interactable states
    virtual void SetOverrideAlpha(float alpha) = 0;

    //! Set the override sprite, if this visual component uses a sprite this will override it
    //! \param sprite   If null the sprite on the visual component will not be overridden
    virtual void SetOverrideSprite(ISprite* sprite) {};

    //! Set the override font, if this visual component uses a font this will override it
    //! \param font   If null the font on the visual component will not be overridden
    virtual void SetOverrideFont(FontFamilyPtr fontFamily) {};

    //! Set the override font effect, if this visual component uses a font this will override it
    virtual void SetOverrideFontEffect(unsigned int fontEffectIndex) {};

public: // static member data

    //! Only one component on a entity can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
};

typedef AZ::EBus<UiVisualInterface> UiVisualBus;
