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

#ifndef CRYINCLUDE_CRYMOVIE_BOOLTRACK_H
#define CRYINCLUDE_CRYMOVIE_BOOLTRACK_H

#pragma once

#include "IMovieSystem.h"
#include "AnimTrack.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/** Boolean track, every key on this track negates boolean value.
*/
class CBoolTrack
    : public TAnimTrack<IBoolKey>
{
public:
    CBoolTrack();

    virtual EAnimValue GetValueType() { return eAnimValue_Bool; };


    virtual void GetValue(float time, bool& value);
    virtual void SetValue(float time, const bool& value, bool bDefault = false);

    void SerializeKey(IBoolKey& key, XmlNodeRef& keyNode, bool bLoading) {};
    void GetKeyInfo(int key, const char*& description, float& duration);

    void SetDefaultValue(const bool bDefaultValue);

    virtual void GetMemoryUsage(ICrySizer* pSizer) const
    {
        pSizer->AddObject(this, sizeof(*this));
    }

    bool Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true) override;

private:
    bool m_bDefaultValue;
};

#endif // CRYINCLUDE_CRYMOVIE_BOOLTRACK_H
