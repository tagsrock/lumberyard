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
#include "EventNode.h"
#include "AnimTrack.h"
#include "TrackEventTrack.h"

#include <ISystem.h>

//////////////////////////////////////////////////////////////////////////
CAnimEventNode::CAnimEventNode(const int id)
    : CAnimNode(id)
{
    SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
    m_lastEventKey = -1;
}

//////////////////////////////////////////////////////////////////////////
void CAnimEventNode::CreateDefaultTracks()
{
    CreateTrack(eAnimParamType_TrackEvent);
}

//////////////////////////////////////////////////////////////////////////
unsigned int CAnimEventNode::GetParamCount() const
{
    return 1;
}

//////////////////////////////////////////////////////////////////////////
CAnimParamType CAnimEventNode::GetParamType(unsigned int nIndex) const
{
    if (nIndex == 0)
    {
        return eAnimParamType_TrackEvent;
    }

    return eAnimParamType_Invalid;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimEventNode::GetParamInfoFromType(const CAnimParamType& animParamType, SParamInfo& info) const
{
    if (animParamType.GetType() == eAnimParamType_TrackEvent)
    {
        info.flags = IAnimNode::ESupportedParamFlags(0);
        info.name = "Track Event";
        info.paramType = eAnimParamType_TrackEvent;
        info.valueType = eAnimValue_Unknown;
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
void CAnimEventNode::Animate(SAnimContext& ec)
{
    // Get track event
    int trackCount = NumTracks();
    for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
    {
        CAnimParamType trackType = m_tracks[paramIndex]->GetParameterType();
        IAnimTrack* pTrack = m_tracks[paramIndex];

        if (pTrack && pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled)
        {
            continue;
        }

        // Check for fire
        if (CTrackEventTrack* pEventTrack = (CTrackEventTrack*)pTrack)
        {
            IEventKey key;
            int nEventKey = pEventTrack->GetActiveKey(ec.time, &key);
            if (nEventKey != m_lastEventKey && nEventKey >= 0)
            {
                bool bKeyAfterStartTime = key.time >= ec.startTime;
                if (bKeyAfterStartTime)
                {
                    ec.pSequence->TriggerTrackEvent(key.event, key.eventValue);
                }
            }
            m_lastEventKey = nEventKey;
        }
    }
}

void CAnimEventNode::OnReset()
{
    m_lastEventKey = -1;
}
