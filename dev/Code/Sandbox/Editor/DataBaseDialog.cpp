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

#include "StdAfx.h"

#include "Material/Material.h"

#include "DataBaseDialog.h"
#include "EntityProtLibDialog.h"
#include "Particles/ParticleDialog.h"
#include "Prefabs/PrefabDialog.h"
#include "GameTokens/GameTokenDialog.h"
#include "ShadersDialog.h"
#include "VegetationDataBasePage.h"
#include "IViewPane.h"
#include "QtViewPaneManager.h"

#include "QtUtil.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QDebug>

#include "QtUtil.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QDebug>

#include <LyMetricsProducer/LyMetricsAPI.h>
#include "ShortcutDispatcher.h"

#define IDC_TABCTRL 1

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
void CDataBaseDialog::RegisterViewClass()
{
    QtViewOptions options;
    options.paneRect = QRect(200, 200, 1000, 800);
    options.canHaveMultipleInstances = true;
    options.sendViewPaneNameBackToAmazonAnalyticsServers = true;
    RegisterQtViewPane<CDataBaseDialog>(GetIEditor(), LyViewPane::DatabaseView, LyViewPane::CategoryOther, options);

    GetIEditor()->GetSettingsManager()->AddToolVersion(LyViewPane::DatabaseView, DATABASE_VIEW_VER);
}

const GUID& CDataBaseDialog::GetClassID()
{
    static const GUID guid =
    {
        0x20b02723, 0xfbb3, 0x4421, { 0x88, 0x88, 0xbc, 0x94, 0x93, 0x86, 0x87, 0xd2 }
    };
    return guid;
}

CDataBaseDialog::CDataBaseDialog(QWidget* pParent)
    : QWidget(pParent)
{
    m_tabCtrl = new QTabWidget(this);

    connect(m_tabCtrl, &QTabWidget::currentChanged, this, QtUtil::Select<int>::OverloadOf(&CDataBaseDialog::Activate));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabCtrl);

    OnInitDialog();
    SEventLog toolEvent(LyViewPane::DatabaseView, "", DATABASE_VIEW_VER);
    GetIEditor()->GetSettingsManager()->RegisterEvent(toolEvent);
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialog::~CDataBaseDialog()
{
    SEventLog toolEvent(LyViewPane::DatabaseView, "", DATABASE_VIEW_VER);
    GetIEditor()->GetSettingsManager()->UnregisterEvent(toolEvent);
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::PostNcDestroy()
{
    delete this;
}

BOOL CDataBaseDialog::OnInitDialog()
{
    m_tabCtrl->addTab(new CEntityProtLibDialog(m_tabCtrl), tr("Entity Library"));
    m_tabCtrl->addTab(new CPrefabDialog(m_tabCtrl), tr("Prefabs Library"));
    m_tabCtrl->addTab(new CVegetationDataBasePage(m_tabCtrl), tr("Vegetation"));
    m_tabCtrl->addTab(new CParticleDialog(m_tabCtrl), tr("Particles"));
    m_tabCtrl->addTab(new CGameTokenDialog(m_tabCtrl), tr("GameTokens"));

    return TRUE;
}

void CDataBaseDialog::Activate(CDataBaseDialogPage* dlg, bool bActive)
{
    dlg->SetActive(bActive);
}

void CDataBaseDialog::Activate(int index)
{
    for (int i = 0; i < m_tabCtrl->count(); ++i)
    {
        Activate(qobject_cast<CDataBaseDialogPage*>(m_tabCtrl->widget(i)), i == index);

        if (m_isReady && (i == index))
        {
            QString tabName = m_tabCtrl->tabText(i);

            // Send metrics events for tabs in the Database View
            ShortcutDispatcher::SubmitMetricsEvent(tabName.toUtf8());
        }
    }

    m_isReady = true;
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialogPage* CDataBaseDialog::SelectDialog(EDataBaseItemType type, IDataBaseItem* pItem)
{
    switch (type)
    {
    case EDB_TYPE_MATERIAL:
        return 0;
        break;
    case EDB_TYPE_ENTITY_ARCHETYPE:
        Select(0);
        break;
    case EDB_TYPE_PREFAB:
        Select(1);
        break;
    //case EDB_TYPE_VEGETATION: // this one is not defined yet
    //  Select(2);
    //  break;
    case EDB_TYPE_PARTICLE:
        Select(3);
        break;
    case EDB_TYPE_FLARE:
        Select(4);
        break;
    case EDB_TYPE_MUSIC:
        Select(5);
        break;
    case EDB_TYPE_EAXPRESET:
        Select(6);
        break;
    case EDB_TYPE_SOUNDMOOD:
        Select(7);
        break;
    case EDB_TYPE_GAMETOKEN:
        Select(8);
        break;
    default:
        return 0;
    }
    CDataBaseDialogPage* pPage = GetCurrent();
    CBaseLibraryDialog* dlg = qobject_cast<CBaseLibraryDialog*>(pPage);
    if (pItem && dlg)
    {
        if (dlg->CanSelectItem((CBaseLibraryItem*)pItem))
        {
            dlg->SelectItem((CBaseLibraryItem*)pItem);
        }
    }
    return pPage;
}

void CDataBaseDialog::Select(int num)
{
    if (num == m_tabCtrl->currentIndex())
    {
        return;
    }
    int prevSelected = m_tabCtrl->currentIndex();
    if (prevSelected >= 0 && prevSelected < m_tabCtrl->count())
    {
        Activate(qobject_cast<CDataBaseDialogPage*>(m_tabCtrl->widget(prevSelected)), false);
    }
    m_tabCtrl->setCurrentIndex(num);
    Activate(qobject_cast<CDataBaseDialogPage*>(m_tabCtrl->currentWidget()), true);
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialogPage* CDataBaseDialog::GetPage(int num)
{
    return qobject_cast<CDataBaseDialogPage*>(m_tabCtrl->widget(num));
}

int CDataBaseDialog::GetSelection() const
{
    return m_tabCtrl->currentIndex();
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialogPage* CDataBaseDialog::GetCurrent()
{
    return qobject_cast<CDataBaseDialogPage*>(m_tabCtrl->currentWidget());
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::Update()
{
    if (GetCurrent())
    {
        GetCurrent()->Update();
    }
}
#include <DataBaseDialog.moc>
