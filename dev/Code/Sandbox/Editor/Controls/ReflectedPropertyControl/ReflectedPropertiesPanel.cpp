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
#include "ReflectedPropertiesPanel.h"

/////////////////////////////////////////////////////////////////////////////
// ReflectedPropertiesPanel dialog


ReflectedPropertiesPanel::ReflectedPropertiesPanel(QWidget* pParent)
    : ReflectedPropertyControl(pParent)
{
}

//////////////////////////////////////////////////////////////////////////
void ReflectedPropertiesPanel::DeleteVars()
{
    ClearVarBlock();
    m_updateCallbacks.clear();
    m_varBlock = 0;
}

//////////////////////////////////////////////////////////////////////////
void ReflectedPropertiesPanel::SetVarBlock(class CVarBlock* vb, const ReflectedPropertyControl::UpdateVarCallback& updCallback, const char* category)
{
    assert(vb);

    m_varBlock = vb;

    RemoveAllItems();
    m_varBlock = vb;
    AddVarBlock(m_varBlock, category);

    SetUpdateCallback(functor(*this, &ReflectedPropertiesPanel::OnPropertyChanged));

    // When new object set all previous callbacks freed.
    m_updateCallbacks.clear();
    if (updCallback)
    {
        stl::push_back_unique(m_updateCallbacks, updCallback);
    }
}

//////////////////////////////////////////////////////////////////////////
void ReflectedPropertiesPanel::AddVars(CVarBlock* vb, const ReflectedPropertyControl::UpdateVarCallback& updCallback, const char* category)
{
    assert(vb);

    bool bNewBlock = false;
    // Make a clone of properties.
    if (!m_varBlock)
    {
        RemoveAllItems();
        m_varBlock = vb->Clone(true);
        AddVarBlock(m_varBlock, category);
        bNewBlock = true;
    }
    m_varBlock->Wire(vb);

    if (bNewBlock)
    {
        SetUpdateCallback(functor(*this, &ReflectedPropertiesPanel::OnPropertyChanged));

        // When new object set all previous callbacks freed.
        m_updateCallbacks.clear();
    }

    if (updCallback)
    {
        stl::push_back_unique(m_updateCallbacks, updCallback);
    }
}

void ReflectedPropertiesPanel::OnPropertyChanged(IVariable* pVar)
{
    std::list<ReflectedPropertyControl::UpdateVarCallback>::iterator iter;
    for (iter = m_updateCallbacks.begin(); iter != m_updateCallbacks.end(); ++iter)
    {
        (*iter)(pVar);
    }
}


