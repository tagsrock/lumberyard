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

class PivotPresetsWidget;

class PropertyPivotCtrl
    : public QWidget
{
    Q_OBJECT

public:

    AZ_CLASS_ALLOCATOR(PropertyPivotCtrl, AZ::SystemAllocator, 0);

    PropertyPivotCtrl(QWidget* parent = nullptr);

    void ConsumeAttribute(AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName);

    PivotPresetsWidget* GetPivotPresetsWidget();
    AzToolsFramework::PropertyVectorCtrl* GetPropertyVectorCtrl();

private:

    AzToolsFramework::VectorPropertyHandlerCommon m_common;
    AzToolsFramework::PropertyVectorCtrl* m_propertyVectorCtrl;
    PivotPresetsWidget* m_pivotPresetsWidget;
};

//-------------------------------------------------------------------------------

class PropertyHandlerPivot
    : public AzToolsFramework::PropertyHandler < AZ::Vector2, PropertyPivotCtrl >
{
public:
    AZ_CLASS_ALLOCATOR(PropertyHandlerPivot, AZ::SystemAllocator, 0);

    AZ::u32 GetHandlerName(void) const override  { return AZ_CRC("Pivot", 0x9caf79f4); }

    QWidget* CreateGUI(QWidget* pParent) override;
    void ConsumeAttribute(PropertyPivotCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
    void WriteGUIValuesIntoProperty(size_t index, PropertyPivotCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
    bool ReadValuesIntoGUI(size_t index, PropertyPivotCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)  override;

    AZ::EntityId GetParentEntityId(AzToolsFramework::InstanceDataNode* node, size_t index);

    static void Register();
};
