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

#include "StdAfx.h"
#include "UiInteractableState.h"

#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/sort.h>
#include <AzFramework/API/ApplicationAPI.h>

#include <LyShine/ISprite.h>
#include <LyShine/UiSerializeHelpers.h>

#include <LyShine/Bus/UiCanvasBus.h>
#include <LyShine/Bus/UiElementBus.h>
#include <LyShine/Bus/UiVisualBus.h>

#include <IRenderer.h>
#include "Sprite.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// UiInteractableStateAction class
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateAction::SetInteractableEntity(AZ::EntityId interactableEntityId)
{
    m_interactableEntity = interactableEntityId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateAction::Init(AZ::EntityId interactableEntityId)
{
    m_interactableEntity = interactableEntityId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateAction::EntityComboBoxVec UiInteractableStateAction::PopulateTargetEntityList()
{
    EntityComboBoxVec result;

    // add a first entry for "None"
    result.push_back(AZStd::make_pair(m_interactableEntity, "<This element>"));

    // Get a list of all child elements
    LyShine::EntityArray matchingElements;
    EBUS_EVENT_ID(m_interactableEntity, UiElementBus, FindDescendantElements,
        [](const AZ::Entity* entity) { return true; },
        matchingElements);

    // add their names to the StringList and their IDs to the id list
    for (auto childEntity : matchingElements)
    {
        result.push_back(AZStd::make_pair(childEntity->GetId(), childEntity->GetName()));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// UiInteractableStateColor class
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateColor::UiInteractableStateColor()
    : m_color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateColor::UiInteractableStateColor(AZ::EntityId target, AZ::Color color)
    : m_targetEntity(target)
    , m_color(color)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateColor::Init(AZ::EntityId interactableEntityId)
{
    UiInteractableStateAction::Init(interactableEntityId);

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = interactableEntityId;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateColor::ApplyState()
{
    EBUS_EVENT_ID(m_targetEntity, UiVisualBus, SetOverrideColor, m_color);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateColor::SetInteractableEntity(AZ::EntityId interactableEntityId)
{
    m_interactableEntity = interactableEntityId;

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = m_interactableEntity;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateAction::EntityComboBoxVec UiInteractableStateColor::PopulateTargetEntityList()
{
    return UiInteractableStateAction::PopulateTargetEntityList();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateColor::Reflect(AZ::ReflectContext* context)
{
    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

    if (serializeContext)
    {
        serializeContext->Class<UiInteractableStateColor, UiInteractableStateAction>()
            ->Version(2, &VersionConverter)
            ->Field("TargetEntity", &UiInteractableStateColor::m_targetEntity)
            ->Field("Color", &UiInteractableStateColor::m_color);

        AZ::EditContext* ec = serializeContext->GetEditContext();
        if (ec)
        {
            auto editInfo = ec->Class<UiInteractableStateColor>("Color", "Overrides the color tint on the target element.");

            editInfo->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement("ComboBox", &UiInteractableStateColor::m_targetEntity, "Target", "The target element.")
                ->Attribute("EnumValues", &UiInteractableStateColor::PopulateTargetEntityList)
                ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::UISliceFlags::PushableEvenIfInvisible);
            editInfo->DataElement("Color", &UiInteractableStateColor::m_color, "Color", "The color tint.");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiInteractableStateColor::VersionConverter(AZ::SerializeContext& context,
    AZ::SerializeContext::DataElementNode& classElement)
{
    // conversion from version 1 to current:
    // - Need to convert AZ::Vector3 to AZ::Color
    if (classElement.GetVersion() <= 1)
    {
        if (!LyShine::ConvertSubElementFromVector3ToAzColor(context, classElement, "Color"))
        {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// UiInteractableStateAlpha class
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateAlpha::UiInteractableStateAlpha()
    : m_alpha(1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateAlpha::UiInteractableStateAlpha(AZ::EntityId target, float alpha)
    : m_targetEntity(target)
    , m_alpha(alpha)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateAlpha::Init(AZ::EntityId interactableEntityId)
{
    UiInteractableStateAction::Init(interactableEntityId);

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = interactableEntityId;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateAlpha::ApplyState()
{
    EBUS_EVENT_ID(m_targetEntity, UiVisualBus, SetOverrideAlpha, m_alpha);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateAlpha::SetInteractableEntity(AZ::EntityId interactableEntityId)
{
    m_interactableEntity = interactableEntityId;

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = m_interactableEntity;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateAlpha::EntityComboBoxVec UiInteractableStateAlpha::PopulateTargetEntityList()
{
    return UiInteractableStateAction::PopulateTargetEntityList();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateAlpha::Reflect(AZ::ReflectContext* context)
{
    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

    if (serializeContext)
    {
        serializeContext->Class<UiInteractableStateAlpha, UiInteractableStateAction>()
            ->Version(1)
            ->Field("TargetEntity", &UiInteractableStateAlpha::m_targetEntity)
            ->Field("Alpha", &UiInteractableStateAlpha::m_alpha);

        AZ::EditContext* ec = serializeContext->GetEditContext();
        if (ec)
        {
            auto editInfo = ec->Class<UiInteractableStateAlpha>("Alpha", "Overrides the alpha on the target element.");

            editInfo->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement("ComboBox", &UiInteractableStateAlpha::m_targetEntity, "Target", "The target element.")
                ->Attribute("EnumValues", &UiInteractableStateAlpha::PopulateTargetEntityList)
                ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::UISliceFlags::PushableEvenIfInvisible);
            editInfo->DataElement("Slider", &UiInteractableStateAlpha::m_alpha, "Alpha", "The opacity.");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// UiInteractableStateSprite class
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateSprite::UiInteractableStateSprite()
    : m_sprite(nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateSprite::UiInteractableStateSprite(AZ::EntityId target, ISprite* sprite)
    : m_targetEntity(target)
    , m_sprite(sprite)
{
    m_sprite->AddRef();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateSprite::UiInteractableStateSprite(AZ::EntityId target, const AZStd::string& spritePath)
    : m_targetEntity(target)
    , m_sprite(nullptr)
{
    m_spritePathname.SetAssetPath(spritePath.c_str());

    if (!m_spritePathname.GetAssetPath().empty())
    {
        m_sprite = gEnv->pLyShine->LoadSprite(m_spritePathname.GetAssetPath().c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateSprite::~UiInteractableStateSprite()
{
    SAFE_RELEASE(m_sprite);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::Init(AZ::EntityId interactableEntityId)
{
    UiInteractableStateAction::Init(interactableEntityId);

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = interactableEntityId;
    }

    // If this is called from RC.exe for example these pointers will not be set. In that case
    // we only need to be able to load, init and save the component. It will never be
    // activated.
    if (!(gEnv && gEnv->pLyShine))
    {
        return;
    }

    // for the case of serializing from disk, if we have sprite pathnames but the sprites
    // are not loaded then load them
    if (!m_sprite && !m_spritePathname.GetAssetPath().empty())
    {
        m_sprite = gEnv->pLyShine->LoadSprite(m_spritePathname.GetAssetPath().c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::ApplyState()
{
    EBUS_EVENT_ID(m_targetEntity, UiVisualBus, SetOverrideSprite, m_sprite);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::SetInteractableEntity(AZ::EntityId interactableEntityId)
{
    m_interactableEntity = interactableEntityId;

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = m_interactableEntity;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::SetSprite(ISprite* sprite)
{
    CSprite::ReplaceSprite(&m_sprite, sprite);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZStd::string UiInteractableStateSprite::GetSpritePathname()
{
    return m_spritePathname.GetAssetPath();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::SetSpritePathname(const AZStd::string& spritePath)
{
    m_spritePathname.SetAssetPath(spritePath.c_str());

    OnSpritePathnameChange();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateSprite::EntityComboBoxVec UiInteractableStateSprite::PopulateTargetEntityList()
{
    return UiInteractableStateAction::PopulateTargetEntityList();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::OnSpritePathnameChange()
{
    ISprite* newSprite = nullptr;

    if (!m_spritePathname.GetAssetPath().empty())
    {
        // Load the new texture.
        newSprite = gEnv->pLyShine->LoadSprite(m_spritePathname.GetAssetPath().c_str());
    }

    SAFE_RELEASE(m_sprite);

    m_sprite = newSprite;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateSprite::Reflect(AZ::ReflectContext* context)
{
    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

    if (serializeContext)
    {
        serializeContext->Class<UiInteractableStateSprite, UiInteractableStateAction>()
            ->Version(1)
            ->Field("TargetEntity", &UiInteractableStateSprite::m_targetEntity)
            ->Field("Sprite", &UiInteractableStateSprite::m_spritePathname);

        AZ::EditContext* ec = serializeContext->GetEditContext();
        if (ec)
        {
            auto editInfo = ec->Class<UiInteractableStateSprite>("Sprite", "Overrides the sprite on the target element.");

            editInfo->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement("ComboBox", &UiInteractableStateSprite::m_targetEntity, "Target", "The target element.")
                ->Attribute("EnumValues", &UiInteractableStateSprite::PopulateTargetEntityList)
                ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::UISliceFlags::PushableEvenIfInvisible);
            editInfo->DataElement("Sprite", &UiInteractableStateSprite::m_spritePathname, "Sprite", "The sprite.")
                ->Attribute("ChangeNotify", &UiInteractableStateSprite::OnSpritePathnameChange);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// UiInteractableStateFont class
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateFont::UiInteractableStateFont()
    : m_fontFamily(nullptr)
    , m_fontEffectIndex(0)
{
    SetFontPathname("default-ui");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateFont::UiInteractableStateFont(AZ::EntityId target, const AZStd::string& pathname, unsigned int fontEffectIndex)
    : m_targetEntity(target)
    , m_fontFamily(nullptr)
    , m_fontEffectIndex(fontEffectIndex)
{
    SetFontPathname(pathname);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateFont::~UiInteractableStateFont()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateFont::Init(AZ::EntityId interactableEntityId)
{
    UiInteractableStateAction::Init(interactableEntityId);

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = interactableEntityId;
    }

    // This will load the font if needed
    SetFontPathname(m_fontFilename.GetAssetPath().c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateFont::ApplyState()
{
    EBUS_EVENT_ID(m_targetEntity, UiVisualBus, SetOverrideFont, m_fontFamily);
    EBUS_EVENT_ID(m_targetEntity, UiVisualBus, SetOverrideFontEffect, m_fontEffectIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateFont::SetInteractableEntity(AZ::EntityId interactableEntityId)
{
    m_interactableEntity = interactableEntityId;

    if (!m_targetEntity.IsValid())
    {
        m_targetEntity = m_interactableEntity;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateFont::SetFontPathname(const AZStd::string& pathname)
{
    // Just to be safe we make sure is normalized
    AZStd::string fontPath = pathname;
    EBUS_EVENT(AzFramework::ApplicationRequests::Bus, NormalizePath, fontPath);
    m_fontFilename.SetAssetPath(fontPath.c_str());

    // We should minimize what is done in constructors and Init since components may be constructed
    // in RC or other tools. Currrently this method is called from the constructor and Init.
    if (gEnv && gEnv->pCryFont &&
        (!m_fontFamily || gEnv->pCryFont->GetFontFamily(fontPath.c_str()) != m_fontFamily))
    {
        AZStd::string fileName = fontPath;
        if (fileName.empty())
        {
            fileName = "default-ui";
        }

        FontFamilyPtr fontFamily = gEnv->pCryFont->GetFontFamily(fileName.c_str());
        if (!fontFamily)
        {
            fontFamily = gEnv->pCryFont->LoadFontFamily(fileName.c_str());
            if (!fontFamily)
            {
                string errorMsg = "Error loading a font from ";
                errorMsg += fileName.c_str();
                errorMsg += ".";
                CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, errorMsg.c_str());
            }
        }
        
        if (fontFamily)
        {
            m_fontFamily = fontFamily;
            // we know that the input path is a root relative and normalized pathname
            m_fontFilename.SetAssetPath(fileName.c_str());

            // the font has changed so check that the font effect is valid
            unsigned int numEffects = m_fontFamily ? m_fontFamily->normal->GetNumEffects() : 0;
            if (m_fontEffectIndex >= numEffects)
            {
                m_fontEffectIndex = 0;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateFont::EntityComboBoxVec UiInteractableStateFont::PopulateTargetEntityList()
{
    return UiInteractableStateAction::PopulateTargetEntityList();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiInteractableStateFont::FontEffectComboBoxVec UiInteractableStateFont::PopulateFontEffectList()
{
    FontEffectComboBoxVec result;
    AZStd::vector<AZ::EntityId> entityIdList;

    // there is always a valid font since we default to "default-ui"
    // so just get the effects from the font and add their names to the result list
    // NOTE: Curently, in order for this to work, when the font is changed we need to do
    // "RefreshEntireTree" to get the combo box list refreshed.
    unsigned int numEffects = m_fontFamily ? m_fontFamily->normal->GetNumEffects() : 0;
    for (int i = 0; i < numEffects; ++i)
    {
        const char* name = m_fontFamily->normal->GetEffectName(i);
        result.push_back(AZStd::make_pair(i, name));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateFont::OnFontPathnameChange()
{
    AZStd::string fontPath = m_fontFilename.GetAssetPath();
    SetFontPathname(fontPath);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiInteractableStateFont::Reflect(AZ::ReflectContext* context)
{
    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

    if (serializeContext)
    {
        serializeContext->Class<UiInteractableStateFont, UiInteractableStateAction>()
            ->Version(1)
            ->Field("TargetEntity", &UiInteractableStateFont::m_targetEntity)
            ->Field("FontFileName", &UiInteractableStateFont::m_fontFilename)
            ->Field("EffectIndex", &UiInteractableStateFont::m_fontEffectIndex);

        AZ::EditContext* ec = serializeContext->GetEditContext();
        if (ec)
        {
            auto editInfo = ec->Class<UiInteractableStateFont>("Font", "Overrides the font on the target element.");

            editInfo->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement("ComboBox", &UiInteractableStateFont::m_targetEntity, "Target", "The target element.")
                ->Attribute("EnumValues", &UiInteractableStateFont::PopulateTargetEntityList)
                ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::UISliceFlags::PushableEvenIfInvisible);
            editInfo->DataElement("SimpleAssetRef", &UiInteractableStateFont::m_fontFilename, "Font path", "The font asset pathname.")
                ->Attribute("ChangeNotify", &UiInteractableStateFont::OnFontPathnameChange)
                ->Attribute("ChangeNotify", AZ_CRC("RefreshEntireTree", 0xefbc823c));
            editInfo->DataElement("ComboBox", &UiInteractableStateFont::m_fontEffectIndex, "Font effect", "The font effect (from font file).")
                ->Attribute("EnumValues", &UiInteractableStateFont::PopulateFontEffectList);
        }
    }
}
