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
#include "TrackViewKeyPropertiesDlg.h"
#include "TrackViewDialog.h"
#include "TrackViewSequence.h"
#include "TrackViewTrack.h"
#include "TrackViewUndo.h"
#include "Controls/ReflectedPropertyControl/ReflectedPropertyCtrl.h"

#include <ISplines.h>
#include <QVBoxLayout>
#include <TrackView/ui_TrackViewTrackPropsDlg.h>

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyUIControls::OnInternalVariableChange(IVariable* pVar)
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    CTrackViewSequenceNotificationContext context(pSequence);
    CTrackViewKeyBundle keys = pSequence->GetSelectedKeys();

    bool bAlreadyRecording = CUndo::IsRecording();
    if (!bAlreadyRecording)
    {
        // Try to start undo. This can't be done if restoring undo
        GetIEditor()->BeginUndo();

        if (CUndo::IsRecording())
        {
            GetIEditor()->GetAnimation()->GetSequence()->StoreUndoForTracksWithSelectedKeys();
        }
        else
        {
            bAlreadyRecording = true;
        }
    }
    else
    {
        GetIEditor()->GetAnimation()->GetSequence()->StoreUndoForTracksWithSelectedKeys();
    }

    OnUIChange(pVar, keys);

    if (!bAlreadyRecording)
    {
        GetIEditor()->AcceptUndo("Change Keys");
    }
}

//////////////////////////////////////////////////////////////////////////
CTrackViewKeyPropertiesDlg::CTrackViewKeyPropertiesDlg(QWidget* hParentWnd)
    : QWidget(hParentWnd)
    , m_pLastTrackSelected(NULL)
{
    QVBoxLayout* l = new QVBoxLayout();
    l->setMargin(0);
    m_wndTrackProps = new CTrackViewTrackPropsDlg(this);
    l->addWidget(m_wndTrackProps);
    m_wndProps = new ReflectedPropertyControl(this);
    m_wndProps->Setup(true, 120);
    m_wndProps->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    l->addWidget(m_wndProps);

    m_wndProps->SetStoreUndoByItems(false);

    setLayout(l);

    m_pVarBlock = new CVarBlock;

    // Add key UI classes
    std::vector<IClassDesc*> classes;
    GetIEditor()->GetClassFactory()->GetClassesByCategory("TrackViewKeyUI", classes);  // BySystemID(ESYSTEM_CLASS_TRACKVIEW_KEYUI, classes);
    for (IClassDesc* iclass : classes)
    {
        if (QObject* pObj = iclass->CreateQObject())
        {
            auto keyControl = qobject_cast<CTrackViewKeyUIControls*>(pObj);
            Q_ASSERT(keyControl);
            m_keyControls.push_back(keyControl);
        }
    }

    // Sort key controls by descending priority
    std::stable_sort(m_keyControls.begin(), m_keyControls.end(),
        [](const _smart_ptr<CTrackViewKeyUIControls>& a, const _smart_ptr<CTrackViewKeyUIControls>& b)
        {
            return a->GetPriority() > b->GetPriority();
        }
        );

    CreateAllVars();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::OnVarChange(IVariable* pVar)
{
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::CreateAllVars()
{
    for (int i = 0; i < (int)m_keyControls.size(); i++)
    {
        m_keyControls[i]->SetKeyPropertiesDlg(this);
        m_keyControls[i]->OnCreateVars();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::PopulateVariables()
{
    //SetVarBlock( m_pVarBlock,functor(*this,&CTrackViewKeyPropertiesDlg::OnVarChange) );

    // Must first clear any selection in properties window.
    m_wndProps->RemoveAllItems();
    m_wndProps->AddVarBlock(m_pVarBlock);

    m_wndProps->SetUpdateCallback(functor(*this, &CTrackViewKeyPropertiesDlg::OnVarChange));
    //m_wndProps->m_props.ExpandAll();


    ReloadValues();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::PopulateVariables(ReflectedPropertyControl* propCtrl)
{
    propCtrl->RemoveAllItems();
    propCtrl->AddVarBlock(m_pVarBlock);

    propCtrl->ReloadValues();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::OnKeysChanged(CTrackViewSequence* pSequence)
{
    CTrackViewKeyBundle selectedKeys = pSequence->GetSelectedKeys();

    if (selectedKeys.GetKeyCount() > 0 && selectedKeys.AreAllKeysOfSameType())
    {
        CTrackViewTrack* pTrack = selectedKeys.GetKey(0).GetTrack();

        CAnimParamType paramType = pTrack->GetParameterType();
        EAnimCurveType trackType = pTrack->GetCurveType();
        EAnimValue valueType = pTrack->GetValueType();

        for (int i = 0; i < (int)m_keyControls.size(); i++)
        {
            if (m_keyControls[i]->SupportTrackType(paramType, trackType, valueType))
            {
                m_keyControls[i]->OnKeySelectionChange(selectedKeys);
                break;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::OnKeySelectionChanged(CTrackViewSequence* pSequence)
{
    CTrackViewKeyBundle selectedKeys = pSequence->GetSelectedKeys();

    m_wndTrackProps->OnKeySelectionChange(selectedKeys);

    bool bSelectChangedInSameTrack
        = m_pLastTrackSelected
            && selectedKeys.GetKeyCount() == 1
            && selectedKeys.GetKey(0).GetTrack() == m_pLastTrackSelected;

    if (selectedKeys.GetKeyCount() == 1)
    {
        m_pLastTrackSelected = selectedKeys.GetKey(0).GetTrack();
    }
    else
    {
        m_pLastTrackSelected = nullptr;
    }

    if (bSelectChangedInSameTrack)
    {
        m_wndProps->ClearSelection();
    }
    else
    {
        m_pVarBlock->DeleteAllVariables();
    }

    m_wndProps->setEnabled(false);
    bool bAssigned = false;
    if (selectedKeys.GetKeyCount() > 0 && selectedKeys.AreAllKeysOfSameType())
    {
        CTrackViewTrack* pTrack = selectedKeys.GetKey(0).GetTrack();

        CAnimParamType paramType = pTrack->GetParameterType();
        EAnimCurveType trackType = pTrack->GetCurveType();
        EAnimValue valueType = pTrack->GetValueType();

        for (int i = 0; i < (int)m_keyControls.size(); i++)
        {
            if (m_keyControls[i]->SupportTrackType(paramType, trackType, valueType))
            {
                if (!bSelectChangedInSameTrack)
                {
                    AddVars(m_keyControls[i]);
                }

                if (m_keyControls[i]->OnKeySelectionChange(selectedKeys))
                {
                    bAssigned = true;
                }

                break;
            }
        }

        m_wndProps->setEnabled(true);
    }
    else
    {
        m_wndProps->setEnabled(false);
    }

    if (bSelectChangedInSameTrack)
    {
        ReloadValues();
    }
    else
    {
        PopulateVariables();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::AddVars(CTrackViewKeyUIControls* pUI)
{
    CVarBlock* pVB = pUI->GetVarBlock();
    for (int i = 0, num = pVB->GetNumVariables(); i < num; i++)
    {
        IVariable* pVar = pVB->GetVariable(i);
        m_pVarBlock->AddVariable(pVar);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeyPropertiesDlg::ReloadValues()
{
    m_wndProps->ReloadValues();
}

void CTrackViewKeyPropertiesDlg::OnSequenceChanged()
{
    m_wndTrackProps->OnSequenceChanged();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CTrackViewTrackPropsDlg::CTrackViewTrackPropsDlg(QWidget* parent /* = 0 */)
    : QWidget(parent)
    , ui(new Ui::CTrackViewTrackPropsDlg)
{
    ui->setupUi(this);
    connect(ui->TIME, SIGNAL(valueChanged(double)), this, SLOT(OnUpdateTime()));
}

CTrackViewTrackPropsDlg::~CTrackViewTrackPropsDlg()
{
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewTrackPropsDlg::OnSequenceChanged()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    if (pSequence)
    {
        Range range = pSequence->GetTimeRange();
        ui->TIME->setRange(range.start, range.end);
    }
}


//////////////////////////////////////////////////////////////////////////
bool CTrackViewTrackPropsDlg::OnKeySelectionChange(CTrackViewKeyBundle& selectedKeys)
{
    m_keyHandle = CTrackViewKeyHandle();

    if (selectedKeys.GetKeyCount() == 1)
    {
        m_keyHandle = selectedKeys.GetKey(0);
    }

    if (m_keyHandle.IsValid())
    {
        ui->TIME->setValue(m_keyHandle.GetTime());
        ui->PREVNEXT->setText(QString::number(m_keyHandle.GetIndex() + 1));

        ui->PREVNEXT->setEnabled(true);
        ui->TIME->setEnabled(true);
    }
    else
    {
        ui->PREVNEXT->setEnabled(FALSE);
        ui->TIME->setEnabled(FALSE);
    }
    return true;
}

void CTrackViewTrackPropsDlg::OnUpdateTime()
{
    if (!m_keyHandle.IsValid())
    {
        return;
    }

    CUndo undo("Change key time");
    CUndo::Record(new CUndoTrackObject(m_keyHandle.GetTrack()));

    const float time = (float) ui->TIME->value();
    m_keyHandle.SetTime(time);

    CTrackViewKeyHandle newKey = m_keyHandle.GetTrack()->GetKeyByTime(time);

    if (newKey != m_keyHandle)
    {
        SetCurrKey(newKey);
    }
}

void CTrackViewTrackPropsDlg::SetCurrKey(CTrackViewKeyHandle& keyHandle)
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    if (keyHandle.IsValid())
    {
        CUndo undo("Select key");
        CUndo::Record(new CUndoAnimKeySelection(pSequence));

        m_keyHandle.Select(false);
        m_keyHandle = keyHandle;
        m_keyHandle.Select(true);
    }
}

#include <TrackView/TrackViewKeyPropertiesDlg.moc>