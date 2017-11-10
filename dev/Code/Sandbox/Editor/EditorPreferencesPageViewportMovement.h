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
#pragma once

#include "Include/IPreferencesPage.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/RTTI.h>


class CEditorPreferencesPage_ViewportMovement
    : public IPreferencesPage
{
public:
    AZ_RTTI(CEditorPreferencesPage_ViewportMovement, "{BC593332-7EAF-4171-8A35-1C5DE5B40909}", IPreferencesPage)

    static void Reflect(AZ::SerializeContext& serialize);

    CEditorPreferencesPage_ViewportMovement();

    virtual const char* GetCategory() override { return "Viewports"; }
    virtual const char* GetTitle() override { return "Movement"; }
    virtual void OnApply() override;
    virtual void OnCancel() override {}
    virtual bool OnQueryCancel() override { return true; }

private:
    void InitializeSettings();

    struct CameraMovementSettings
    {
        AZ_TYPE_INFO(CameraMovementSettings, "{60B8C07E-5F48-4171-A50B-F45558B5CCA1}")

        float m_moveSpeed;
        float m_rotateSpeed;
        float m_fastMoveSpeed;
        float m_wheelZoomSpeed;
        bool m_invertYRotation;
        bool m_invertPan;
    };

    CameraMovementSettings m_cameraMovementSettings;
};


