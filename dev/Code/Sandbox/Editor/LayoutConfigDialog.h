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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_EDITOR_LAYOUTCONFIGDIALOG_H
#define CRYINCLUDE_EDITOR_LAYOUTCONFIGDIALOG_H

#pragma once

#include "LayoutWnd.h"

#include <QDialog>

namespace Ui {
    class CLayoutConfigDialog;
}

class LayoutConfigModel;

// CLayoutConfigDialog dialog

class CLayoutConfigDialog
    : public QDialog
{
    Q_OBJECT

public:
    CLayoutConfigDialog(QWidget* pParent = nullptr);   // standard constructor
    virtual ~CLayoutConfigDialog();

    void SetLayout(EViewLayout layout);
    EViewLayout GetLayout() const { return m_layout; };

    // Dialog Data
protected:
    virtual void OnOK();

    LayoutConfigModel* m_model;
    EViewLayout m_layout;
    QScopedPointer<Ui::CLayoutConfigDialog> ui;
};

#endif // CRYINCLUDE_EDITOR_LAYOUTCONFIGDIALOG_H
