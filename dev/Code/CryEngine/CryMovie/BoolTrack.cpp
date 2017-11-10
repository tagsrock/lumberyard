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
#include "BoolTrack.h"

//////////////////////////////////////////////////////////////////////////
CBoolTrack::CBoolTrack()
    : m_bDefaultValue(true)
{
}

//////////////////////////////////////////////////////////////////////////
void CBoolTrack::GetKeyInfo(int index, const char*& description, float& duration)
{
    description = 0;
    duration = 0;
}

//////////////////////////////////////////////////////////////////////////
void CBoolTrack::GetValue(float time, bool& value)
{
    value = m_bDefaultValue;

    CheckValid();

    int nkeys = m_keys.size();
    if (nkeys < 1)
    {
        return;
    }

    int key = 0;
    while ((key < nkeys) && (time >= m_keys[key].time))
    {
        key++;
    }

    if (m_bDefaultValue)
    {
        value = !(key & 1); // True if even key.
    }
    else
    {
        value = (key & 1);  // False if even key.
    }
}

//////////////////////////////////////////////////////////////////////////
void CBoolTrack::SetValue(float time, const bool& value, bool bDefault)
{
    if (bDefault)
    {
        SetDefaultValue(value);
    }
    Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CBoolTrack::SetDefaultValue(const bool bDefaultValue)
{
    m_bDefaultValue = bDefaultValue;
}

bool CBoolTrack::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
    bool retVal = TAnimTrack<IBoolKey>::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
    if (bLoading)
    {
        xmlNode->getAttr("DefaultValue", m_bDefaultValue);
    }
    else
    {
        //saving        
        xmlNode->setAttr("DefaultValue", m_bDefaultValue);
    }
    return retVal;
}
