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

// Description : Tooltip that displays bitmap.


#ifndef CRYINCLUDE_EDITOR_CONTROLS_BITMAPTOOLTIP_H
#define CRYINCLUDE_EDITOR_CONTROLS_BITMAPTOOLTIP_H
#pragma once


#include "Controls/ImageHistogramCtrl.h"

#include <QLabel>
#include <QTimer>

//////////////////////////////////////////////////////////////////////////
class CBitmapToolTip
    : public QWidget
{
    Q_OBJECT
    // Construction
public:

    enum EShowMode
    {
        ESHOW_RGB = 0,
        ESHOW_ALPHA,
        ESHOW_RGBA,
        ESHOW_RGB_ALPHA,
        ESHOW_RGBE
    };

    CBitmapToolTip(QWidget* parent = nullptr);
    virtual ~CBitmapToolTip();

    BOOL Create(const RECT& rect);

    // Attributes
public:

    // Operations
public:
    void RefreshLayout();
    void RefreshViewmode();

    bool LoadImage(const QString& imageFilename);
    void SetTool(QWidget* pWnd, const QRect& rect);
    void CorrectPosition();

    // Generated message map functions
protected:
    void OnTimer();

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void GetShowMode(EShowMode& showMode, bool& showInOriginalSize) const;
    const char* GetShowModeDescription(EShowMode showMode, bool showInOriginalSize) const;

    QLabel m_staticBitmap;
    QLabel m_staticText;
    QString m_filename;
    QPixmap m_previewBitmap;
    bool        m_bShowHistogram;
    EShowMode m_eShowMode;
    bool    m_bShowFullsize;
    bool    m_bHasAlpha;
    bool    m_bIsLimitedHDR;
    CImageHistogramCtrl m_rgbaHistogram, m_alphaChannelHistogram;
    int m_nTimer;
    QWidget* m_hToolWnd;
    QRect m_toolRect;
    QTimer m_timer;
};


#endif // CRYINCLUDE_EDITOR_CONTROLS_BITMAPTOOLTIP_H
