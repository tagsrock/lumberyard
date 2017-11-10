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

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>

#include <AzToolsFramework/UI/PropertyEditor/PropertyVectorCtrl.hxx>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>

#include <LyShine/UiBase.h>
#include <LyShine/Bus/UiTransform2dBus.h>

class AnchorPresetsWidget;

class PropertyAnchorCtrl
    : public QWidget
{
    Q_OBJECT

public:

    AZ_CLASS_ALLOCATOR(PropertyAnchorCtrl, AZ::SystemAllocator, 0);

    PropertyAnchorCtrl(QWidget* parent = nullptr);

    void ConsumeAttribute(AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName);

    AnchorPresetsWidget* GetAnchorPresetsWidget();
    AzToolsFramework::PropertyVectorCtrl* GetPropertyVectorCtrl();

    bool IsReadOnly() { return m_isReadOnly; }

private:

    AzToolsFramework::VectorPropertyHandlerCommon m_common;
    AzToolsFramework::PropertyVectorCtrl* m_propertyVectorCtrl;
    AnchorPresetsWidget* m_anchorPresetsWidget;
    QLabel* m_disabledLabel;
    bool m_isReadOnly;
};

//-------------------------------------------------------------------------------

class PropertyHandlerAnchor
    : public AzToolsFramework::PropertyHandler < UiTransform2dInterface::Anchors, PropertyAnchorCtrl >
{
public:
    AZ_CLASS_ALLOCATOR(PropertyHandlerAnchor, AZ::SystemAllocator, 0);

    AZ::u32 GetHandlerName(void) const override  { return AZ_CRC("Anchor", 0x6751117d); }
    bool IsDefaultHandler() const override { return true; }

    QWidget* CreateGUI(QWidget* pParent) override;
    void ConsumeAttribute(PropertyAnchorCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
    void WriteGUIValuesIntoProperty(size_t index, PropertyAnchorCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
    bool ReadValuesIntoGUI(size_t index, PropertyAnchorCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)  override;
    bool ModifyTooltip(QWidget* widget, QString& toolTipString) override;

    AZ::EntityId GetParentEntityId(AzToolsFramework::InstanceDataNode* node, size_t index);

    static void Register();
};
