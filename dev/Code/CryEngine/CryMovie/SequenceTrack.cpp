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
#include "SequenceTrack.h"

//////////////////////////////////////////////////////////////////////////
void CSequenceTrack::SerializeKey(ISequenceKey& key, XmlNodeRef& keyNode, bool bLoading)
{
    if (bLoading)
    {
        const char* szSelection;
        szSelection = keyNode->getAttr("node");
        cry_strcpy(key.szSelection, szSelection);

        if (!keyNode->getAttr("overridetimes", key.bOverrideTimes))
        {
            key.bOverrideTimes = false;
        }

        if (key.bOverrideTimes == true)
        {
            if (!keyNode->getAttr("starttime", key.fStartTime))
            {
                key.fStartTime = 0.0f;
            }
            if (!keyNode->getAttr("endtime", key.fEndTime))
            {
                key.fEndTime  = 0.0f;
            }
        }
        else
        {
            key.fStartTime = 0.0f;
            key.fEndTime = 0.0f;
        }
    }
    else
    {
        keyNode->setAttr("node", key.szSelection);

        if (key.bOverrideTimes == true)
        {
            keyNode->setAttr("overridetimes", key.bOverrideTimes);
            keyNode->setAttr("starttime", key.fStartTime);
            keyNode->setAttr("endtime", key.fEndTime);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CSequenceTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
    assert(key >= 0 && key < (int)m_keys.size());
    CheckValid();
    description = 0;
    duration = m_keys[key].fDuration;
    if (strlen(m_keys[key].szSelection) > 0)
    {
        description = m_keys[key].szSelection;
    }
}