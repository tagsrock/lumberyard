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

#include "Stdafx.h"
#include "TrackViewKeyPropertiesDlg.h"
#include "TrackViewTrack.h"
#include "TrackViewUndo.h"

//////////////////////////////////////////////////////////////////////////
class CCommentKeyUIControls
    : public CTrackViewKeyUIControls
{
public:
    CSmartVariableArray mv_table;
    CSmartVariable<QString> mv_comment;
    CSmartVariable<float> mv_duration;
    CSmartVariable<float> mv_size;
    CSmartVariable<Vec3> mv_color;
    CSmartVariableEnum<int> mv_align;
    CSmartVariableEnum<QString> mv_font;


    virtual void OnCreateVars()
    {
        AddVariable(mv_table, "Key Properties");
        AddVariable(mv_table, mv_comment, "Comment");
        AddVariable(mv_table, mv_duration, "Duration");

        mv_size->SetLimits(1.f, 10.f);
        AddVariable(mv_table, mv_size, "Size");

        AddVariable(mv_table, mv_color, "Color", IVariable::DT_COLOR);

        mv_align->SetEnumList(NULL);
        mv_align->AddEnumItem("Left", ICommentKey::eTA_Left);
        mv_align->AddEnumItem("Center", ICommentKey::eTA_Center);
        mv_align->AddEnumItem("Right", ICommentKey::eTA_Right);
        AddVariable(mv_table, mv_align, "Align");

        mv_font->SetEnumList(NULL);
        IFileUtil::FileArray fa;
        CFileUtil::ScanDirectory((Path::GetEditingGameDataFolder() + "/Fonts/").c_str(), "*.xml", fa, true);
        for (size_t i = 0; i < fa.size(); ++i)
        {
            string name = fa[i].filename.toLatin1().data();
            PathUtil::RemoveExtension(name);
            mv_font->AddEnumItem(name.c_str(), name.c_str());
        }
        AddVariable(mv_table, mv_font, "Font");
    }
    bool SupportTrackType(const CAnimParamType& paramType, EAnimCurveType trackType, EAnimValue valueType) const
    {
        return paramType == eAnimParamType_CommentText;
    }
    virtual bool OnKeySelectionChange(CTrackViewKeyBundle& selectedKeys);
    virtual void OnUIChange(IVariable* pVar, CTrackViewKeyBundle& selectedKeys);

    virtual unsigned int GetPriority() const { return 1; }

    static const GUID& GetClassID()
    {
        // {FA250B8B-FC2A-43b1-AF7A-8C3B6672B49D}
        static const GUID guid =
        {
            0xfa250b8b, 0xfc2a, 0x43b1, { 0xaf, 0x7a, 0x8c, 0x3b, 0x66, 0x72, 0xb4, 0x9d }
        };
        return guid;
    }
};

//////////////////////////////////////////////////////////////////////////
bool CCommentKeyUIControls::OnKeySelectionChange(CTrackViewKeyBundle& selectedKeys)
{
    if (!selectedKeys.AreAllKeysOfSameType())
    {
        return false;
    }

    bool bAssigned = false;
    if (selectedKeys.GetKeyCount() == 1)
    {
        const CTrackViewKeyHandle& keyHandle = selectedKeys.GetKey(0);

        CAnimParamType paramType = keyHandle.GetTrack()->GetParameterType();
        if (paramType == eAnimParamType_CommentText)
        {
            ICommentKey commentKey;
            keyHandle.GetKey(&commentKey);

            mv_comment = commentKey.m_strComment.c_str();
            mv_duration = commentKey.m_duration;
            mv_size = commentKey.m_size;
            mv_font = commentKey.m_strFont;
            mv_color = commentKey.m_color;
            mv_align = commentKey.m_align;

            bAssigned = true;
        }
    }
    return bAssigned;
}
//////////////////////////////////////////////////////////////////////////
// Called when UI variable changes.
void CCommentKeyUIControls::OnUIChange(IVariable* pVar, CTrackViewKeyBundle& selectedKeys)
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    if (!pSequence || !selectedKeys.AreAllKeysOfSameType())
    {
        return;
    }

    for (size_t keyIndex = 0, num = selectedKeys.GetKeyCount(); keyIndex < num; keyIndex++)
    {
        CTrackViewKeyHandle keyHandle = selectedKeys.GetKey(keyIndex);

        CAnimParamType paramType = keyHandle.GetTrack()->GetParameterType();
        if (paramType == eAnimParamType_CommentText)
        {
            ICommentKey commentKey;
            keyHandle.GetKey(&commentKey);

            if (!pVar || pVar == mv_comment.GetVar())
            {
                commentKey.m_strComment = ((QString)mv_comment).toLatin1().data();
            }

            if (!pVar || pVar == mv_font.GetVar())
            {
                QString sFont = mv_font;
                cry_strcpy(commentKey.m_strFont, sFont.toLatin1().data());
            }

            if (!pVar || pVar == mv_align.GetVar())
            {
                commentKey.m_align = (ICommentKey::ETextAlign)((int)mv_align);
            }

            SyncValue(mv_duration, commentKey.m_duration, false, pVar);
            SyncValue(mv_color, commentKey.m_color, false, pVar);
            SyncValue(mv_size, commentKey.m_size, false, pVar);

            CUndo::Record(new CUndoTrackObject(keyHandle.GetTrack()));
            keyHandle.SetKey(&commentKey);
        }
    }
}

REGISTER_QT_CLASS_DESC(CCommentKeyUIControls, "TrackView.KeyUI.Comment", "TrackViewKeyUI");
