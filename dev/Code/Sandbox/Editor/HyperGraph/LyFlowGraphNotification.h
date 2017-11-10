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

#include <QDialog>
#include <QCheckBox>

#include <AzCore/UserSettings/UserSettings.h>

#include <AzQtComponents/Components/StyledDialog.h>

class FlowGraphNotificationDialog : public AzQtComponents::StyledDialog
{
    Q_OBJECT

public:

    AZ_CLASS_ALLOCATOR(FlowGraphNotificationDialog, AZ::SystemAllocator, 0);
    FlowGraphNotificationDialog(QWidget *parent = nullptr);

    void Show();

protected:

    void closeEvent(QCloseEvent* evt) override;

    QCheckBox* m_dontShowAgainCheckbox;

};
