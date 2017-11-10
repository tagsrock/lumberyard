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
#ifndef QBITMAPPREVIEWDIALOG_IMP_H
#define QBITMAPPREVIEWDIALOG_IMP_H

#include "QBitmapPreviewDialog.h"
#include <Util/ImageHistogram.h>

class CImageEx;

class QBitmapPreviewDialogImp
    : public QBitmapPreviewDialog
{
    Q_OBJECT;
public:

    enum EUIStyle
    {
        EUISTYLE_IMAGE_ONLY,
        EUISTYLE_IMAGE_HISTOGRAM,
        EUISTYLE_NumModes
    };

    enum EShowMode
    {
        ESHOW_RGB = 0,
        ESHOW_ALPHA,
        ESHOW_RGBA,
        ESHOW_RGB_ALPHA,
        ESHOW_RGBE,
        ESHOW_NumModes,
    };

    enum EHistogramMode
    {
        eHistogramMode_Luminosity,
        eHistogramMode_OverlappedRGB,
        eHistogramMode_SplitRGB,
        eHistogramMode_RedChannel,
        eHistogramMode_GreenChannel,
        eHistogramMode_BlueChannel,
        eHistogramMode_AlphaChannel,
        eHistogramMode_NumModes,
    };

    explicit QBitmapPreviewDialogImp(QWidget* parent = 0);
    virtual ~QBitmapPreviewDialogImp();

    void setImage(const QString path);

    void setShowMode(EShowMode mode);
    void toggleShowMode();
    void setUIStyleMode(EUIStyle mode);
    const EShowMode& getShowMode() const;

    void setHistogramMode(EHistogramMode mode);
    void toggleHistrogramMode();
    const EHistogramMode& getHistogramMode() const;

    void setOriginalSize(bool value);
    void toggleOriginalSize();

    bool isSizeSmallerThanDefault();
    void paintEvent(QPaintEvent* e) override;

protected:
    void refreshData();

private:
    const char* GetShowModeDescription(EShowMode eShowMode, bool bShowInOriginalSize) const;

private:
    CImageEx*       m_image;
    QString         m_path;
    CImageHistogram m_histogram;
    bool            m_showOriginalSize;
    EShowMode       m_showMode;
    EHistogramMode  m_histrogramMode;
    EUIStyle        m_uiStyle;
};

#endif // QBITMAPPREVIEWDIALOG_IMP_H
