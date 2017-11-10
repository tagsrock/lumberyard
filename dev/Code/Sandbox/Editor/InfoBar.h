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

#ifndef CRYINCLUDE_EDITOR_INFOBAR_H
#define CRYINCLUDE_EDITOR_INFOBAR_H

#pragma once
// InfoBar.h : header file
//

#include <IAudioSystem.h>
#include <HMDBus.h>

/////////////////////////////////////////////////////////////////////////////
// CInfoBar dialog

namespace Ui {
    class CInfoBar;
}

class CInfoBar
    : public QWidget
    , public IEditorNotifyListener
    , public AZ::VR::VREventBus::Handler
{
    Q_OBJECT

    // Construction
public:
    CInfoBar(QWidget* parent = nullptr);
    ~CInfoBar();

    // Toggle the mute audio button
    void ToggleAudio() { OnBnClickedMuteAudio(); }

Q_SIGNALS:
    void ActionTriggered(int command);

    // Implementation
protected:
    void IdleUpdate();
    virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

    virtual void OnOK() {};
    virtual void OnCancel() {};

    void OnVectorUpdate(bool followTerrain);

    // these get called by drag/spinner style updates
    void OnVectorUpdateX();
    void OnVectorUpdateY();
    void OnVectorUpdateZ();

    // this gets called by stepper or text edit changes
    void OnVectorChanged();

    void SetVector(const Vec3& v);
    void SetVectorRange(float min, float max);
    Vec3 GetVector();
    void EnableVector(bool enable);

    void SetVectorLock(bool bVectorLock);

    void OnBnClickedSyncplayer();
    void OnBnClickedGotoPosition();
    void OnBnClickedSpeed01();
    void OnBnClickedSpeed1();
    void OnBnClickedSpeed10();
    void OnBeginVectorUpdate();
    void OnEndVectorUpdate();

    void OnVectorLock();
    void OnLockSelection();
    void OnBnClickedSetVector();
    void OnUpdateMoveSpeed();
    void OnBnClickedTerrainCollision();
    void OnBnClickedPhysics();
    void OnBnClickedSingleStepPhys();
    void OnBnClickedDoStepPhys();
    void OnBnClickedMuteAudio();
    void OnBnClickedEnableVR();
    void OnInitDialog();

    //////////////////////////////////////////////////////////////////////////
    /// VR Event Bus Implementation
    //////////////////////////////////////////////////////////////////////////
    void OnHMDInitialized() override;
    void OnHMDShutdown() override;
    //////////////////////////////////////////////////////////////////////////

    bool m_enabledVector;

    float m_width, m_height;
    //int m_heightMapX,m_heightMapY;

    int m_prevEditMode;
    int m_numSelected;
    float m_prevMoveSpeed;

    bool m_bVectorLock;
    bool m_bSelectionLocked;
    bool m_bSelectionChanged;

    bool m_bDragMode;
    QString m_sLastText;

    CEditTool* m_editTool;
    Vec3 m_lastValue;
    Vec3 m_currValue;
    float m_oldMasterVolume;

    Audio::SAudioRequest m_oMuteAudioRequest;
    Audio::SAudioManagerRequestData<Audio::eAMRT_MUTE_ALL> m_oMuteAudioRequestData;
    Audio::SAudioRequest m_oUnmuteAudioRequest;
    Audio::SAudioManagerRequestData<Audio::eAMRT_UNMUTE_ALL> m_oUnmuteAudioRequestData;

    QScopedPointer<Ui::CInfoBar> ui;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // CRYINCLUDE_EDITOR_INFOBAR_H
