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

// Description : CryMovie animation node for shadow settings

#include "StdAfx.h"
#include "ShadowsSetupNode.h"

//////////////////////////////////////////////////////////////////////////
namespace ShadowSetupNode
{
    bool s_shadowSetupParamsInit = false;
    std::vector<CAnimNode::SParamInfo> s_shadowSetupParams;

    void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
    {
        CAnimNode::SParamInfo param;
        param.name = sName;
        param.paramType = paramId;
        param.valueType = valueType;
        s_shadowSetupParams.push_back(param);
    }
};

//-----------------------------------------------------------------------------
CShadowsSetupNode::CShadowsSetupNode(const int id)
    : CAnimNode(id)
{
    CShadowsSetupNode::Initialize();
}

//-----------------------------------------------------------------------------
void CShadowsSetupNode::Initialize()
{
    if (!ShadowSetupNode::s_shadowSetupParamsInit)
    {
        ShadowSetupNode::s_shadowSetupParamsInit = true;
        ShadowSetupNode::s_shadowSetupParams.reserve(1);
        ShadowSetupNode::AddSupportedParam("GSMCache", eAnimParamType_GSMCache, eAnimValue_Bool);
    }
}

//-----------------------------------------------------------------------------
void CShadowsSetupNode::Animate(SAnimContext& ac)
{
    IAnimTrack* pGsmCache = GetTrackForParameter(eAnimParamType_GSMCache);
    if (pGsmCache && (pGsmCache->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) == 0)
    {
        bool val(false);
        pGsmCache->GetValue(ac.time, val);
        gEnv->p3DEngine->SetShadowsGSMCache(val);
    }
}

//-----------------------------------------------------------------------------
void CShadowsSetupNode::CreateDefaultTracks()
{
    CreateTrack(eAnimParamType_GSMCache);
}

//-----------------------------------------------------------------------------
void CShadowsSetupNode::OnReset()
{
    gEnv->p3DEngine->SetShadowsGSMCache(false);
}

//-----------------------------------------------------------------------------
unsigned int CShadowsSetupNode::GetParamCount() const
{
    return ShadowSetupNode::s_shadowSetupParams.size();
}

//-----------------------------------------------------------------------------
CAnimParamType CShadowsSetupNode::GetParamType(unsigned int nIndex) const
{
    if (nIndex >= 0 && nIndex < (int)ShadowSetupNode::s_shadowSetupParams.size())
    {
        return ShadowSetupNode::s_shadowSetupParams[nIndex].paramType;
    }

    return eAnimParamType_Invalid;
}

//-----------------------------------------------------------------------------
bool CShadowsSetupNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
    for (size_t i = 0; i < ShadowSetupNode::s_shadowSetupParams.size(); ++i)
    {
        if (ShadowSetupNode::s_shadowSetupParams[i].paramType == paramId)
        {
            info = ShadowSetupNode::s_shadowSetupParams[i];
            return true;
        }
    }
    return false;
}
