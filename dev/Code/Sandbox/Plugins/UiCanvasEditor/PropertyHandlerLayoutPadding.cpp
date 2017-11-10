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

#include "PropertyHandlerLayoutPadding.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtWidgets/QWidget>

QWidget* PropertyHandlerLayoutPadding::CreateGUI(QWidget* pParent)
{
    AzToolsFramework::PropertyVectorCtrl* ctrl = m_common.ConstructGUI(pParent);
    ctrl->setLabel(0, "Left");
    ctrl->setLabel(1, "Top");
    ctrl->setLabel(2, "Right");
    ctrl->setLabel(3, "Bottom");

    return ctrl;
}

void PropertyHandlerLayoutPadding::Register()
{
    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew PropertyHandlerLayoutPadding());
}

#include <PropertyHandlerLayoutPadding.moc>
