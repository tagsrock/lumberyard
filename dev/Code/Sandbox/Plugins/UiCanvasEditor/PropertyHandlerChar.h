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

#include <AzToolsFramework/UI/PropertyEditor/PropertyStringLineEditCtrl.hxx>

class PropertyHandlerChar
    : private QObject
    , public AzToolsFramework::PropertyHandler<char, AzToolsFramework::PropertyStringLineEditCtrl>
{
    // this is a Qt Object purely so it can connect to slots with context.  This is the only reason its in this header.
    Q_OBJECT
public:
    AZ_CLASS_ALLOCATOR(PropertyHandlerChar, AZ::SystemAllocator, 0);

    AZ::u32 GetHandlerName(void) const override  { return AZ_CRC("Char", 0x8cfe579f); }
    bool IsDefaultHandler() const override { return true; }
    QWidget* CreateGUI(QWidget* pParent) override;
    void ConsumeAttribute(AzToolsFramework::PropertyStringLineEditCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
    void WriteGUIValuesIntoProperty(size_t index, AzToolsFramework::PropertyStringLineEditCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
    bool ReadValuesIntoGUI(size_t index, AzToolsFramework::PropertyStringLineEditCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)  override;

    static void Register();
};
