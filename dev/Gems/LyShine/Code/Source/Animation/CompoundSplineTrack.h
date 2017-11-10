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

#pragma once

#define MAX_SUBTRACKS 4

#include <LyShine/Animation/IUiAnimation.h>
#include <AzCore/std/smart_ptr/intrusive_ptr.h>
#include <AzCore/std/containers/array.h>

//////////////////////////////////////////////////////////////////////////
class UiCompoundSplineTrack
    : public IUiAnimTrack
{
public:
    AZ_CLASS_ALLOCATOR(UiCompoundSplineTrack, AZ::SystemAllocator, 0)
    AZ_RTTI(UiCompoundSplineTrack, "{91947B8B-65B7-451D-9D04-0C821C82014E}", IUiAnimTrack);

    UiCompoundSplineTrack(int nDims, EUiAnimValue inValueType, CUiAnimParamType subTrackParamTypes[MAX_SUBTRACKS]);
    UiCompoundSplineTrack();

    void add_ref() { ++m_refCount; }
    void release()
    {
        if (--m_refCount <= 0)
        {
            delete this;
        }
    }

    virtual int GetSubTrackCount() const { return m_nDimensions; };
    virtual IUiAnimTrack* GetSubTrack(int nIndex) const;
    virtual const char* GetSubTrackName(int nIndex) const;
    virtual void SetSubTrackName(int nIndex, const char* name);

    virtual EUiAnimCurveType GetCurveType() { return eUiAnimCurveType_BezierFloat; };
    virtual EUiAnimValue GetValueType() { return m_valueType; };

    virtual CUiAnimParamType  GetParameterType() const { return m_nParamType; };
    virtual void SetParameterType(CUiAnimParamType type) { m_nParamType = type; }

    virtual const UiAnimParamData& GetParamData() const { return m_componentParamData; }
    virtual void SetParamData(const UiAnimParamData& param) { m_componentParamData = param; }

    virtual int GetNumKeys() const;
    virtual void SetNumKeys(int numKeys) { assert(0); };
    virtual bool HasKeys() const;
    virtual void RemoveKey(int num);

    virtual void GetKeyInfo(int key, const char*& description, float& duration);
    virtual int CreateKey(float time) { assert(0); return 0; };
    virtual int CloneKey(int fromKey) { assert(0); return 0; };
    virtual int CopyKey(IUiAnimTrack* pFromTrack, int nFromKey) { assert(0); return 0; };
    virtual void GetKey(int index, IKey* key) const { assert(0); };
    virtual float GetKeyTime(int index) const;
    virtual int FindKey(float time) { assert(0); return 0; };
    virtual int GetKeyFlags(int index) { assert(0); return 0; };
    virtual void SetKey(int index, IKey* key) { assert(0); };
    virtual void SetKeyTime(int index, float time);
    virtual void SetKeyFlags(int index, int flags) { assert(0); };
    virtual void SortKeys() { assert(0); };

    virtual bool IsKeySelected(int key) const;
    virtual void SelectKey(int key, bool select);

    virtual int GetFlags() { return m_flags; };
    virtual bool IsMasked(const uint32 mask) const { return false; }
    virtual void SetFlags(int flags) { m_flags = flags; };

    //////////////////////////////////////////////////////////////////////////
    // Get track value at specified time.
    // Interpolates keys if needed.
    //////////////////////////////////////////////////////////////////////////
    virtual void GetValue(float time, float& value);
    virtual void GetValue(float time, Vec3& value);
    virtual void GetValue(float time, Vec4& value);
    virtual void GetValue(float time, Quat& value);
    virtual void GetValue(float time, AZ::Vector2& value);
    virtual void GetValue(float time, AZ::Vector3& value);
    virtual void GetValue(float time, AZ::Vector4& value);
    virtual void GetValue(float time, AZ::Color& value);
    virtual void GetValue(float time, bool& value) { assert(0); };

    //////////////////////////////////////////////////////////////////////////
    // Set track value at specified time.
    // Adds new keys if required.
    //////////////////////////////////////////////////////////////////////////
    virtual void SetValue(float time, const float& value, bool bDefault = false);
    virtual void SetValue(float time, const Vec3& value, bool bDefault = false);
    void SetValue(float time, const Vec4& value, bool bDefault = false);
    virtual void SetValue(float time, const Quat& value, bool bDefault = false);
    virtual void SetValue(float time, const bool& value, bool bDefault = false) { assert(0); };
    virtual void SetValue(float time, const AZ::Vector2& value, bool bDefault = false);
    virtual void SetValue(float time, const AZ::Vector3& value, bool bDefault = false);
    virtual void SetValue(float time, const AZ::Vector4& value, bool bDefault = false);
    virtual void SetValue(float time, const AZ::Color& value, bool bDefault = false);

    virtual void OffsetKeyPosition(const Vec3& value);

    virtual void SetTimeRange(const Range& timeRange);

    virtual bool Serialize(IUiAnimationSystem* uiAnimationSystem, XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true);

    virtual bool SerializeSelection(XmlNodeRef& xmlNode, bool bLoading, bool bCopySelected = false, float fTimeOffset = 0);

    virtual int NextKeyByTime(int key) const;

    void SetSubTrackName(const int i, const string& name) { assert (i < MAX_SUBTRACKS); m_subTrackNames[i] = name; }

#ifdef UI_ANIMATION_SYSTEM_SUPPORT_EDITING
    virtual ColorB GetCustomColor() const
    { return m_customColor; }
    virtual void SetCustomColor(ColorB color)
    {
        m_customColor = color;
        m_bCustomColorSet = true;
    }
    virtual bool HasCustomColor() const
    { return m_bCustomColorSet; }
    virtual void ClearCustomColor()
    { m_bCustomColorSet = false; }
#endif

    virtual void GetKeyValueRange(float& fMin, float& fMax) const
    {
        if (GetSubTrackCount() > 0)
        {
            m_subTracks[0]->GetKeyValueRange(fMin, fMax);
        }
    };
    virtual void SetKeyValueRange(float fMin, float fMax)
    {
        for (int i = 0; i < m_nDimensions; ++i)
        {
            m_subTracks[i]->SetKeyValueRange(fMin, fMax);
        }
    };

    static void Reflect(AZ::SerializeContext* serializeContext);

protected:
    int m_refCount;
    EUiAnimValue m_valueType;
    int m_nDimensions;
    AZStd::array<AZStd::intrusive_ptr<IUiAnimTrack>, MAX_SUBTRACKS> m_subTracks;
    int m_flags;
    CUiAnimParamType m_nParamType;
    AZStd::array<AZStd::string, MAX_SUBTRACKS> m_subTrackNames;

    UiAnimParamData m_componentParamData;

#ifdef UI_ANIMATION_SYSTEM_SUPPORT_EDITING
    ColorB m_customColor;
    bool m_bCustomColorSet;
#endif

    void PrepareNodeForSubTrackSerialization(XmlNodeRef& subTrackNode, XmlNodeRef& xmlNode, int i, bool bLoading);
    float PreferShortestRotPath(float degree, float degree0) const;
    int GetSubTrackIndex(int& key) const;
};
