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

// Description : implementation file


#include "StdAfx.h"
#include "MatEditMainDlg.h"

#include "Material/MaterialDialog.h"
#include "Material/MaterialManager.h"

#include "MaterialSender.h"
#include "UserMessageDefines.h"

#include <QTimer>
#include <QAbstractEventDispatcher>
#include <QEventLoop>
#include <QVBoxLayout>


CMatEditMainDlg::CMatEditMainDlg(const QString& title, QWidget* pParent /*=NULL*/)
    : QWidget(pParent)
{
    resize(1000, 600);

    setWindowTitle(title);

    QTimer* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &CMatEditMainDlg::OnKickIdle);
    t->start(250);

    m_materialDialog = new CMaterialDialog(); // must be created after the timer
    auto layout = new QVBoxLayout(this);
    layout->addWidget(m_materialDialog);

#ifdef Q_OS_WIN
    if (auto aed = QAbstractEventDispatcher::instance())
    {
        aed->installNativeEventFilter(this);
    }
#endif
}

CMatEditMainDlg::~CMatEditMainDlg()
{
#ifdef Q_OS_WIN
    if (auto aed = QAbstractEventDispatcher::instance())
    {
        aed->removeNativeEventFilter(this);
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CMatEditMainDlg message handlers

void CMatEditMainDlg::showEvent(QShowEvent*)
{
    if (QWindow *win = window()->windowHandle())
    {
        // Make sure our top-level window decorator wrapper set this exact title
        // 3ds Max Exporter will use ::FindWindow with this name
        win->setTitle("Material Editor");
    }
}

#ifdef Q_OS_WIN
bool CMatEditMainDlg::nativeEventFilter(const QByteArray&, void* message, long*)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_MATEDITSEND)
    {
        OnMatEditSend(msg->wParam);
        return true;
    }

    return false;
}
#endif

void CMatEditMainDlg::closeEvent(QCloseEvent* event)
{
    QWidget::closeEvent(event);
    qApp->exit();
}

void CMatEditMainDlg::OnKickIdle()
{
    GetIEditor()->Notify(eNotify_OnIdleUpdate);
}

void CMatEditMainDlg::OnMatEditSend(int param)
{
    if (param != eMSM_Init)
    {
        GetIEditor()->GetMaterialManager()->SyncMaterialEditor();
    }
}

#include <MatEditMainDlg.moc>