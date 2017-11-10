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

// Description : Animationes environment parameters


#include "StdAfx.h"
#include "AnimEnvironmentNode.h"
#include "AnimLightNode.h"
#include "AnimSplineTrack.h"
#include <ITimeOfDay.h>

namespace AnimEnvironmentNode
{
    bool s_environmentNodeParamsInit = false;
    std::vector<CAnimNode::SParamInfo> s_environmentNodeParams;

    void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
    {
        CAnimNode::SParamInfo param;
        param.name = sName;
        param.paramType = paramId;
        param.valueType = valueType;
        s_environmentNodeParams.push_back(param);
    }
}

CAnimEnvironmentNode::CAnimEnvironmentNode(const int id)
    : CAnimNode(id)
    , m_oldSunLongitude(0.0f)
    , m_oldSunLatitude(0.0f)
{
    CAnimEnvironmentNode::Initialize();
    StoreCelestialPositions();
}

void CAnimEnvironmentNode::Initialize()
{
    if (!AnimEnvironmentNode::s_environmentNodeParamsInit)
    {
        AnimEnvironmentNode::s_environmentNodeParamsInit = true;
        AnimEnvironmentNode::s_environmentNodeParams.reserve(2);
        AnimEnvironmentNode::AddSupportedParam("Sun Longitude", eAnimParamType_SunLongitude, eAnimValue_Float);
        AnimEnvironmentNode::AddSupportedParam("Sun Latitude", eAnimParamType_SunLatitude, eAnimValue_Float);
        AnimEnvironmentNode::AddSupportedParam("Moon Longitude", eAnimParamType_MoonLongitude, eAnimValue_Float);
        AnimEnvironmentNode::AddSupportedParam("Moon Latitude", eAnimParamType_MoonLatitude, eAnimValue_Float);
    }
}

void CAnimEnvironmentNode::Animate(SAnimContext& ac)
{
    ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();

    float sunLongitude = pTimeOfDay->GetSunLongitude();
    float sunLatitude = pTimeOfDay->GetSunLatitude();

    Vec3 v;
    gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
    float& moonLongitude = v.y;
    float& moonLatitude = v.x;

    IAnimTrack* pSunLongitudeTrack = GetTrackForParameter(eAnimParamType_SunLongitude);
    IAnimTrack* pSunLatitudeTrack = GetTrackForParameter(eAnimParamType_SunLatitude);
    IAnimTrack* pMoonLongitudeTrack = GetTrackForParameter(eAnimParamType_MoonLongitude);
    IAnimTrack* pMoonLatitudeTrack = GetTrackForParameter(eAnimParamType_MoonLatitude);

    bool bUpdateSun = false;
    bool bUpdateMoon = false;

    if (pSunLongitudeTrack && pSunLongitudeTrack->GetNumKeys() > 0)
    {
        bUpdateSun = true;
        pSunLongitudeTrack->GetValue(ac.time, sunLongitude);
    }

    if (pSunLatitudeTrack && pSunLatitudeTrack->GetNumKeys() > 0)
    {
        bUpdateSun = true;
        pSunLatitudeTrack->GetValue(ac.time, sunLatitude);
    }

    if (pMoonLongitudeTrack && pMoonLongitudeTrack->GetNumKeys() > 0)
    {
        bUpdateMoon = true;
        pMoonLongitudeTrack->GetValue(ac.time, moonLongitude);
    }

    if (pMoonLatitudeTrack && pMoonLatitudeTrack->GetNumKeys() > 0)
    {
        bUpdateMoon = true;
        pMoonLatitudeTrack->GetValue(ac.time, moonLatitude);
    }

    if (bUpdateSun)
    {
        pTimeOfDay->SetSunPos(sunLongitude, sunLatitude);
    }

    if (bUpdateMoon)
    {
        gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
    }

    if (bUpdateSun || bUpdateMoon)
    {
        pTimeOfDay->Update(true, false);
    }
}

void CAnimEnvironmentNode::CreateDefaultTracks()
{
    CreateTrack(eAnimParamType_SunLatitude);
    CreateTrack(eAnimParamType_SunLongitude);
}

void CAnimEnvironmentNode::Activate(bool bActivate)
{
    if (bActivate)
    {
        StoreCelestialPositions();
    }
    else
    {
        RestoreCelestialPositions();
    }
}

unsigned int CAnimEnvironmentNode::GetParamCount() const
{
    return AnimEnvironmentNode::s_environmentNodeParams.size();
}

CAnimParamType CAnimEnvironmentNode::GetParamType(unsigned int nIndex) const
{
    if (nIndex >= 0 && nIndex < (int)AnimEnvironmentNode::s_environmentNodeParams.size())
    {
        return AnimEnvironmentNode::s_environmentNodeParams[nIndex].paramType;
    }

    return eAnimParamType_Invalid;
}

bool CAnimEnvironmentNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
    for (size_t i = 0; i < AnimEnvironmentNode::s_environmentNodeParams.size(); ++i)
    {
        if (AnimEnvironmentNode::s_environmentNodeParams[i].paramType == paramId)
        {
            info = AnimEnvironmentNode::s_environmentNodeParams[i];
            return true;
        }
    }
    return false;
}

void CAnimEnvironmentNode::InitializeTrack(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
    ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
    const float sunLongitude = pTimeOfDay->GetSunLongitude();
    const float sunLatitude = pTimeOfDay->GetSunLatitude();

    Vec3 v;
    gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
    const float moonLongitude = v.y;
    const float moonLatitude = v.x;

    C2DSplineTrack* pFloatTrack = static_cast<C2DSplineTrack*>(pTrack);

    if (pFloatTrack)
    {
        if (paramType == eAnimParamType_SunLongitude)
        {
            pFloatTrack->SetDefaultValue(Vec2(0.0f, sunLongitude));
        }
        else if (paramType == eAnimParamType_SunLatitude)
        {
            pFloatTrack->SetDefaultValue(Vec2(0.0f, sunLatitude));
        }
        else if (paramType == eAnimParamType_MoonLongitude)
        {
            pFloatTrack->SetDefaultValue(Vec2(0.0f, moonLongitude));
        }
        else if (paramType == eAnimParamType_MoonLatitude)
        {
            pFloatTrack->SetDefaultValue(Vec2(0.0f, moonLatitude));
        }
    }
}

void CAnimEnvironmentNode::StoreCelestialPositions()
{
    ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
    m_oldSunLongitude = pTimeOfDay->GetSunLongitude();
    m_oldSunLatitude = pTimeOfDay->GetSunLatitude();

    Vec3 v;
    gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
    m_oldMoonLongitude = v.y;
    m_oldMoonLatitude = v.x;
}

void CAnimEnvironmentNode::RestoreCelestialPositions()
{
    ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
    pTimeOfDay->SetSunPos(m_oldSunLongitude, m_oldSunLatitude);

    Vec3 v;
    v.y = m_oldMoonLongitude;
    v.x = m_oldMoonLatitude;
    gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);

    pTimeOfDay->Update(true, false);
}
