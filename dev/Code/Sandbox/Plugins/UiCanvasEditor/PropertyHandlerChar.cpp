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

#include "PropertyHandlerChar.h"

QWidget* PropertyHandlerChar::CreateGUI(QWidget* pParent)
{
    AzToolsFramework::PropertyStringLineEditCtrl* ctrl = aznew AzToolsFramework::PropertyStringLineEditCtrl(pParent);
    ctrl->setMaxLen(1);
    QObject::connect(ctrl, &AzToolsFramework::PropertyStringLineEditCtrl::valueChanged, this, [ctrl]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, ctrl);
        });

    return ctrl;
}

void PropertyHandlerChar::ConsumeAttribute(AzToolsFramework::PropertyStringLineEditCtrl* /*GUI*/, AZ::u32 /*attrib*/, AzToolsFramework::PropertyAttributeReader* /*attrValue*/, const char* /*debugName*/)
{
}

void PropertyHandlerChar::WriteGUIValuesIntoProperty(size_t index, AzToolsFramework::PropertyStringLineEditCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    (int)index;
    AZStd::string str = GUI->value();
    char character = '\0';
    if (!str.empty())
    {
        character = str[0];
    }
    instance = character;
}

bool PropertyHandlerChar::ReadValuesIntoGUI(size_t index, AzToolsFramework::PropertyStringLineEditCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    (int)index;

    GUI->blockSignals(true);
    {
        AZStd::string val(1, instance);
        GUI->setValue(val);
    }
    GUI->blockSignals(false);

    return false;
}

void PropertyHandlerChar::Register()
{
    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew PropertyHandlerChar());
}

#include <PropertyHandlerChar.moc>
