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
#include "GotoTrack.h"

#define MIN_TIME_PRECISION 0.01f

//////////////////////////////////////////////////////////////////////////
CGotoTrack::CGotoTrack()
{
    m_flags = 0;
    m_DefaultValue = -1.0f;
}
////////////////////////////////////////////////////////////////////////
void CGotoTrack::GetValue(float time, float& value, bool applyMultiplier)
{
    size_t nTotalKeys(m_keys.size());

    value = m_DefaultValue;

    if (nTotalKeys < 1)
    {
        return;
    }

    CheckValid();

    size_t nKey(0);
    for (nKey = 0; nKey < nTotalKeys; ++nKey)
    {
        if (time >= m_keys[nKey].time)
        {
            value = m_keys[nKey].m_fValue;
        }
        else
        {
            break;
        }
    }

    if (applyMultiplier && m_trackMultiplier != 1.0f)
    {
        value /= m_trackMultiplier;
    }
}
//////////////////////////////////////////////////////////////////////////
void CGotoTrack::SetValue(float time, const float& value, bool bDefault, bool applyMultiplier)
{
    if (!bDefault)
    {
        IDiscreteFloatKey oKey;
        if (applyMultiplier && m_trackMultiplier != 1.0f)
        {
            oKey.SetValue(value * m_trackMultiplier);
        }
        else
        {
            oKey.SetValue(value);
        }
        SetKeyAtTime(time, &oKey);
    }
    else
    {
        if (applyMultiplier && m_trackMultiplier != 1.0f)
        {
            m_DefaultValue = value * m_trackMultiplier;
        }
        else
        {
            m_DefaultValue = value;
        }
    }
}
////////////////////////////////////////////////////////////////////////
void CGotoTrack::SerializeKey(IDiscreteFloatKey& key, XmlNodeRef& keyNode, bool bLoading)
{
    if (bLoading)
    {
        keyNode->getAttr("time", key.time);
        keyNode->getAttr("value", key.m_fValue);
        //assert(key.time == key.m_fValue);

        keyNode->getAttr("flags", key.flags);
    }
    else
    {
        keyNode->setAttr("time", key.time);
        keyNode->setAttr("value", key.m_fValue);

        int flags = key.flags;
        if (flags != 0)
        {
            keyNode->setAttr("flags", flags);
        }
    }
}
//////////////////////////////////////////////////////////////////////////
void CGotoTrack::GetKeyInfo(int index, const char*& description, float& duration)
{
    static char str[64];
    description = str;
    assert(index >= 0 && index < GetNumKeys());
    float& k = m_keys[index].m_fValue;
    sprintf_s(str, "%.2f", k);
}
//////////////////////////////////////////////////////////////////////////
void CGotoTrack::SetKeyAtTime(float time, IKey* key)
{
    assert(key != 0);

    key->time = time;

    bool found = false;
    // Find key with given time.
    for (size_t i = 0; i < m_keys.size(); i++)
    {
        float keyt = m_keys[i].time;
        if (fabs(keyt - time) < MIN_TIME_PRECISION)
        {
            key->flags = m_keys[i].flags;               // Reserve the flag value.
            SetKey(i, key);
            found = true;
            break;
        }
        //if (keyt > time)
        //break;
    }
    if (!found)
    {
        // Key with this time not found.
        // Create a new one.
        int keyIndex = CreateKey(time);
        // Reserve the flag value.
        key->flags = m_keys[keyIndex].flags;        // Reserve the flag value.
        SetKey(keyIndex, key);
    }
}
//////////////////////////////////////////////////////////////////////////
