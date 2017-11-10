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
#include "LayerNode.h"

//////////////////////////////////////////////////////////////////////////
namespace
{
    bool s_nodeParamsInitialized = false;
    std::vector<CAnimNode::SParamInfo> s_nodeParams;

    void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
    {
        CAnimNode::SParamInfo param;
        param.name = sName;
        param.paramType = paramId;
        param.valueType = valueType;
        s_nodeParams.push_back(param);
    }
};

//-----------------------------------------------------------------------------
CLayerNode::CLayerNode(const int id)
    : CAnimNode(id)
    , m_bInit(false)
    , m_bPreVisibility(true)
{
    CLayerNode::Initialize();
}

//-----------------------------------------------------------------------------
void CLayerNode::Initialize()
{
    if (!s_nodeParamsInitialized)
    {
        s_nodeParamsInitialized = true;
        s_nodeParams.reserve(1);
        AddSupportedParam("Visibility", eAnimParamType_Visibility, eAnimValue_Bool);
    }
}

//-----------------------------------------------------------------------------
void CLayerNode::Animate(SAnimContext& ec)
{
    bool bVisibilityModified = false;

    int trackCount = NumTracks();
    for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
    {
        CAnimParamType paramType = m_tracks[paramIndex]->GetParameterType();
        IAnimTrack* pTrack = m_tracks[paramIndex];
        if (pTrack->GetNumKeys() == 0)
        {
            continue;
        }

        if (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled)
        {
            continue;
        }

        if (pTrack->IsMasked(ec.trackMask))
        {
            continue;
        }

        switch (paramType.GetType())
        {
        case eAnimParamType_Visibility:
            if (!ec.bResetting)
            {
                IAnimTrack* visTrack = pTrack;
                bool visible = true;
                visTrack->GetValue(ec.time, visible);

                if (m_bInit)
                {
                    if (visible != m_bPreVisibility)
                    {
                        m_bPreVisibility = visible;
                        bVisibilityModified = true;
                    }
                }
                else
                {
                    m_bInit = true;
                    m_bPreVisibility = visible;
                    bVisibilityModified = true;
                }
            }
            break;
        }

        // Layer entities visibility control
        if (bVisibilityModified)
        {
            // This is for game mode, in case of the layer data being exported.
            if (gEnv->pEntitySystem)
            {
                gEnv->pEntitySystem->EnableLayer(GetName(), m_bPreVisibility);
            }
        }
    }
}

//-----------------------------------------------------------------------------
void CLayerNode::CreateDefaultTracks()
{
    CreateTrack(eAnimParamType_Visibility);
}

//-----------------------------------------------------------------------------
void CLayerNode::OnReset()
{
    m_bInit = false;
}

//-----------------------------------------------------------------------------
void CLayerNode::Activate(bool bActivate)
{
    CAnimNode::Activate(bActivate);
}

//-----------------------------------------------------------------------------
void CLayerNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
    CAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);

    //Nothing to be serialized at this moment.
}

//-----------------------------------------------------------------------------
unsigned int CLayerNode::GetParamCount() const
{
    return s_nodeParams.size();
}

//-----------------------------------------------------------------------------
CAnimParamType CLayerNode::GetParamType(unsigned int nIndex) const
{
    if (nIndex >= 0 && nIndex < (int)s_nodeParams.size())
    {
        return s_nodeParams[nIndex].paramType;
    }

    return eAnimParamType_Invalid;
}

//-----------------------------------------------------------------------------
bool CLayerNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
    for (unsigned int i = 0; i < s_nodeParams.size(); i++)
    {
        if (s_nodeParams[i].paramType == paramId)
        {
            info = s_nodeParams[i];
            return true;
        }
    }
    return false;
}
