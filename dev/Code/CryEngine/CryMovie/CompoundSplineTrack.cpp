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
#include "CompoundSplineTrack.h"
#include "AnimSplineTrack.h"

CCompoundSplineTrack::CCompoundSplineTrack(int nDims, EAnimValue inValueType, CAnimParamType subTrackParamTypes[MAX_SUBTRACKS])
{
    assert(nDims > 0 && nDims <= MAX_SUBTRACKS);
    m_node = nullptr;
    m_nDimensions = nDims;
    m_valueType = inValueType;

    m_nParamType = eAnimNodeType_Invalid;
    m_flags = 0;

    ZeroStruct(m_subTracks);

    for (int i = 0; i < m_nDimensions; i++)
    {
        m_subTracks[i] = new C2DSplineTrack();
        m_subTracks[i]->SetParameterType(subTrackParamTypes[i]);

        if (inValueType == eAnimValue_RGB)
        {
            m_subTracks[i]->SetKeyValueRange(0.0f, 255.f);
        }
    }

    m_subTrackNames[0] = "X";
    m_subTrackNames[1] = "Y";
    m_subTrackNames[2] = "Z";
    m_subTrackNames[3] = "W";

#ifdef MOVIESYSTEM_SUPPORT_EDITING
    m_bCustomColorSet = false;
#endif
}

void CCompoundSplineTrack::SetNode(IAnimNode* node)
{
    m_node = node;
    for (int i = 0; i < m_nDimensions; i++)
    {
        m_subTracks[i]->SetNode(node);
    }
}
//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::SetTimeRange(const Range& timeRange)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        m_subTracks[i]->SetTimeRange(timeRange);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::PrepareNodeForSubTrackSerialization(XmlNodeRef& subTrackNode, XmlNodeRef& xmlNode, int i, bool bLoading)
{
    assert(!bLoading || xmlNode->getChildCount() == m_nDimensions);

    if (bLoading)
    {
        subTrackNode = xmlNode->getChild(i);
        // First, check its version.
        if (strcmp(subTrackNode->getTag(), "SubTrack") == 0)
        // So, it's an old format.
        {
            CAnimParamType paramType = m_subTracks[i]->GetParameterType();
            // Recreate sub tracks as the old format.
            m_subTracks[i] = new CTcbFloatTrack;
            m_subTracks[i]->SetParameterType(paramType);
        }
    }
    else
    {
        if (m_subTracks[i]->GetCurveType() == eAnimCurveType_BezierFloat)
        {
            // It's a new 2D Bezier curve.
            subTrackNode = xmlNode->newChild("NewSubTrack");
        }
        else
        // Old TCB spline
        {
            assert(m_subTracks[i]->GetCurveType() == eAnimCurveType_TCBFloat);
            subTrackNode = xmlNode->newChild("SubTrack");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
bool CCompoundSplineTrack::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks /*=true */)
{
#ifdef MOVIESYSTEM_SUPPORT_EDITING
    if (bLoading)
    {
        int flags = m_flags;
        xmlNode->getAttr("Flags", flags);
        SetFlags(flags);
        xmlNode->getAttr("HasCustomColor", m_bCustomColorSet);
        if (m_bCustomColorSet)
        {
            unsigned int abgr;
            xmlNode->getAttr("CustomColor", abgr);
            m_customColor = ColorB(abgr);
        }
    }
    else
    {
        int flags = GetFlags();
        xmlNode->setAttr("Flags", flags);
        xmlNode->setAttr("HasCustomColor", m_bCustomColorSet);
        if (m_bCustomColorSet)
        {
            xmlNode->setAttr("CustomColor", m_customColor.pack_abgr8888());
        }
    }
#endif

    for (int i = 0; i < m_nDimensions; i++)
    {
        XmlNodeRef subTrackNode;
        PrepareNodeForSubTrackSerialization(subTrackNode, xmlNode, i, bLoading);
        m_subTracks[i]->Serialize(subTrackNode, bLoading, bLoadEmptyTracks);
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CCompoundSplineTrack::SerializeSelection(XmlNodeRef& xmlNode, bool bLoading, bool bCopySelected /*=false*/, float fTimeOffset /*=0*/)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        XmlNodeRef subTrackNode;
        PrepareNodeForSubTrackSerialization(subTrackNode, xmlNode, i, bLoading);
        m_subTracks[i]->SerializeSelection(subTrackNode, bLoading, bCopySelected, fTimeOffset);
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::GetValue(float time, float& value, bool applyMultiplier)
{
    for (int i = 0; i < 1 && i < m_nDimensions; i++)
    {
        m_subTracks[i]->GetValue(time, value, applyMultiplier);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::GetValue(float time, Vec3& value, bool applyMultiplier)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        float v = value[i];
        m_subTracks[i]->GetValue(time, v, applyMultiplier);
        value[i] = v;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::GetValue(float time, Vec4& value, bool applyMultiplier)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        float v = value[i];
        m_subTracks[i]->GetValue(time, v, applyMultiplier);
        value[i] = v;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::GetValue(float time, Quat& value)
{
    if (m_nDimensions == 3)
    {
        // Assume Euler Angles XYZ
        float angles[3] = {0, 0, 0};
        for (int i = 0; i < m_nDimensions; i++)
        {
            m_subTracks[i]->GetValue(time, angles[i]);
        }
        value = Quat::CreateRotationXYZ(Ang3(DEG2RAD(angles[0]), DEG2RAD(angles[1]), DEG2RAD(angles[2])));
    }
    else
    {
        assert(0);
        value.SetIdentity();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::SetValue(float time, const float& value, bool bDefault, bool applyMultiplier)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        m_subTracks[i]->SetValue(time, value, bDefault, applyMultiplier);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::SetValue(float time, const Vec3& value, bool bDefault, bool applyMultiplier)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        m_subTracks[i]->SetValue(time, value[i], bDefault, applyMultiplier);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::SetValue(float time, const Vec4& value, bool bDefault, bool applyMultiplier)
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        m_subTracks[i]->SetValue(time, value[i], bDefault, applyMultiplier);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::SetValue(float time, const Quat& value, bool bDefault)
{
    if (m_nDimensions == 3)
    {
        // Assume Euler Angles XYZ
        Ang3 angles = Ang3::GetAnglesXYZ(value);
        for (int i = 0; i < 3; i++)
        {
            float degree = RAD2DEG(angles[i]);
            if (false == bDefault)
            {
                // Try to prefer the shortest path of rotation.
                float degree0 = 0.0f;
                m_subTracks[i]->GetValue(time, degree0);
                degree = PreferShortestRotPath(degree, degree0);
            }
            m_subTracks[i]->SetValue(time, degree, bDefault);
        }
    }
    else
    {
        assert(0);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::OffsetKeyPosition(const Vec3& offset)
{
    if (m_nDimensions == 3)
    {
        for (int i = 0; i < 3; i++)
        {
            IAnimTrack* pSubTrack = m_subTracks[i];
            // Iterate over all keys.
            for (int k = 0, num = pSubTrack->GetNumKeys(); k < num; k++)
            {
                // Offset each key.
                float time = pSubTrack->GetKeyTime(k);
                float value = 0;
                pSubTrack->GetValue(time, value);
                value = value + offset[i];
                pSubTrack->SetValue(time, value);
            }
        }
    }
    else
    {
        assert(0);
    }
}

//////////////////////////////////////////////////////////////////////////
IAnimTrack* CCompoundSplineTrack::GetSubTrack(int nIndex) const
{
    assert(nIndex >= 0 && nIndex < m_nDimensions);
    return m_subTracks[nIndex];
}

//////////////////////////////////////////////////////////////////////////
const char* CCompoundSplineTrack::GetSubTrackName(int nIndex) const
{
    assert(nIndex >= 0 && nIndex < m_nDimensions);
    return m_subTrackNames[nIndex];
}


//////////////////////////////////////////////////////////////////////////
void CCompoundSplineTrack::SetSubTrackName(int nIndex, const char* name)
{
    assert(nIndex >= 0 && nIndex < m_nDimensions);
    assert(name);
    m_subTrackNames[nIndex] = name;
}

//////////////////////////////////////////////////////////////////////////
int CCompoundSplineTrack::GetNumKeys() const
{
    int nKeys = 0;
    for (int i = 0; i < m_nDimensions; i++)
    {
        nKeys += m_subTracks[i]->GetNumKeys();
    }
    return nKeys;
}

//////////////////////////////////////////////////////////////////////////
bool CCompoundSplineTrack::HasKeys() const
{
    for (int i = 0; i < m_nDimensions; i++)
    {
        if (m_subTracks[i]->GetNumKeys())
        {
            return true;
        }
    }
    return false;
}

float CCompoundSplineTrack::PreferShortestRotPath(float degree, float degree0) const
{
    // Assumes the degree is in (-PI, PI).
    assert(-181.0f < degree && degree < 181.0f);
    float degree00 = degree0;
    degree0 = fmod_tpl(degree0, 360.0f);
    float n = (degree00 - degree0) / 360.0f;
    float degreeAlt;
    if (degree >= 0)
    {
        degreeAlt = degree - 360.0f;
    }
    else
    {
        degreeAlt = degree + 360.0f;
    }
    if (fabs(degreeAlt - degree0) < fabs(degree - degree0))
    {
        return degreeAlt + n * 360.0f;
    }
    else
    {
        return degree + n * 360.0f;
    }
}

int CCompoundSplineTrack::GetSubTrackIndex(int& key) const
{
    assert(key >= 0 && key < GetNumKeys());
    int count = 0;
    for (int i = 0; i < m_nDimensions; i++)
    {
        if (key < count + m_subTracks[i]->GetNumKeys())
        {
            key = key - count;
            return i;
        }
        count += m_subTracks[i]->GetNumKeys();
    }
    return -1;
}

void CCompoundSplineTrack::RemoveKey(int num)
{
    assert(num >= 0 && num < GetNumKeys());
    int i = GetSubTrackIndex(num);
    assert(i >= 0);
    if (i < 0)
    {
        return;
    }
    m_subTracks[i]->RemoveKey(num);
}

void CCompoundSplineTrack::GetKeyInfo(int key, const char*& description, float& duration)
{
    static char str[64];
    duration = 0;
    description = str;
    const char* subDesc = NULL;
    float time = GetKeyTime(key);
    int m = 0;
    /// Using the time obtained, combine descriptions from keys of the same time
    /// in sub-tracks if any into one compound description.
    str[0] = 0;
    // A head case
    for (m = 0; m < m_subTracks[0]->GetNumKeys(); ++m)
    {
        if (m_subTracks[0]->GetKeyTime(m) == time)
        {
            float dummy;
            m_subTracks[0]->GetKeyInfo(m, subDesc, dummy);
            cry_strcat(str, subDesc);
            break;
        }
    }
    if (m == m_subTracks[0]->GetNumKeys())
    {
        cry_strcat(str, m_subTrackNames[0]);
    }
    // Tail cases
    for (int i = 1; i < GetSubTrackCount(); ++i)
    {
        cry_strcat(str, ",");
        for (m = 0; m < m_subTracks[i]->GetNumKeys(); ++m)
        {
            if (m_subTracks[i]->GetKeyTime(m) == time)
            {
                float dummy;
                m_subTracks[i]->GetKeyInfo(m, subDesc, dummy);
                cry_strcat(str, subDesc);
                break;
            }
        }
        if (m == m_subTracks[i]->GetNumKeys())
        {
            cry_strcat(str, m_subTrackNames[i]);
        }
    }
}

float CCompoundSplineTrack::GetKeyTime(int index) const
{
    assert(index >= 0 && index < GetNumKeys());
    int i = GetSubTrackIndex(index);
    assert(i >= 0);
    if (i < 0)
    {
        return 0;
    }
    return m_subTracks[i]->GetKeyTime(index);
}

void CCompoundSplineTrack::SetKeyTime(int index, float time)
{
    assert(index >= 0 && index < GetNumKeys());
    int i = GetSubTrackIndex(index);
    assert(i >= 0);
    if (i < 0)
    {
        return;
    }
    m_subTracks[i]->SetKeyTime(index, time);
}

bool CCompoundSplineTrack::IsKeySelected(int key) const
{
    assert(key >= 0 && key < GetNumKeys());
    int i = GetSubTrackIndex(key);
    assert(i >= 0);
    if (i < 0)
    {
        return false;
    }
    return m_subTracks[i]->IsKeySelected(key);
}

void CCompoundSplineTrack::SelectKey(int key, bool select)
{
    assert(key >= 0 && key < GetNumKeys());
    int i = GetSubTrackIndex(key);
    assert(i >= 0);
    if (i < 0)
    {
        return;
    }
    float keyTime = m_subTracks[i]->GetKeyTime(key);
    // In the case of compound tracks, animators want to
    // select all keys of the same time in the sub-tracks together.
    const float timeEpsilon = 0.001f;
    for (int k = 0; k < m_nDimensions; ++k)
    {
        for (int m = 0; m < m_subTracks[k]->GetNumKeys(); ++m)
        {
            if (fabs(m_subTracks[k]->GetKeyTime(m) - keyTime) < timeEpsilon)
            {
                m_subTracks[k]->SelectKey(m, select);
                break;
            }
        }
    }
}

int CCompoundSplineTrack::NextKeyByTime(int key) const
{
    assert(key >= 0 && key < GetNumKeys());
    float time = GetKeyTime(key);
    int count = 0, result = -1;
    float timeNext = FLT_MAX;
    for (int i = 0; i < GetSubTrackCount(); ++i)
    {
        for (int k = 0; k < m_subTracks[i]->GetNumKeys(); ++k)
        {
            float t = m_subTracks[i]->GetKeyTime(k);
            if (t > time)
            {
                if (t < timeNext)
                {
                    timeNext = t;
                    result = count + k;
                }
                break;
            }
        }
        count += m_subTracks[i]->GetNumKeys();
    }
    return result;
}
