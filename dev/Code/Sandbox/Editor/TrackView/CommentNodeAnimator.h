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

// Description : Comment node animator class

/*
    CCommentContext stores information about comment track.
    The Comment Track is activated only in the editor.
*/



#ifndef CRYINCLUDE_EDITOR_TRACKVIEW_COMMENTNODEANIMATOR_H
#define CRYINCLUDE_EDITOR_TRACKVIEW_COMMENTNODEANIMATOR_H
#pragma once

#include "TrackViewAnimNode.h"

class CTrackViewTrack;

struct CCommentContext
{
    CCommentContext()
        : m_nLastActiveKeyIndex(-1)
        , m_strComment(0)
        , m_size(1.0f)
        , m_align(0)
    {
        sprintf_s(m_strFont, sizeof(m_strFont), "default");
        m_unitPos = Vec2(0.f, 0.f);
        m_color = Vec3(0.f, 0.f, 0.f);
    }

    int m_nLastActiveKeyIndex;

    const char* m_strComment;
    char m_strFont[64];
    Vec2 m_unitPos;
    Vec3 m_color;
    float m_size;
    int m_align;
};

class CCommentNodeAnimator
    : public IAnimNodeAnimator
{
public:
    CCommentNodeAnimator(CTrackViewAnimNode* pCommentNode);
    virtual void Animate(CTrackViewAnimNode* pNode, const SAnimContext& ac);
    virtual void Render(CTrackViewAnimNode* pNode, const SAnimContext& ac);

private:
    virtual ~CCommentNodeAnimator();

    void AnimateCommentTextTrack(CTrackViewTrack* pTrack, const SAnimContext& ac);
    CTrackViewKeyHandle GetActiveKeyHandle(CTrackViewTrack* pTrack, float fTime);
    Vec2 GetScreenPosFromNormalizedPos(const Vec2& unitPos);
    void DrawText(const char* szFontName, float fSize, const Vec2& unitPos, const ColorF col, const char* szText, int align);

    CTrackViewAnimNode* m_pCommentNode;
    CCommentContext m_commentContext;
};
#endif // CRYINCLUDE_EDITOR_TRACKVIEW_COMMENTNODEANIMATOR_H
