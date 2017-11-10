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
#ifndef QBITMAPPREVIEWDIALOG_H
#define QBITMAPPREVIEWDIALOG_H

#include <QWidget>
#include <QPixmap>
#include <QImage>

class QLabel;

namespace Ui {
    class QBitmapTooltip;
}

class QBitmapPreviewDialog
    : public QWidget
{
    Q_OBJECT

    struct ImageData
    {
        QByteArray  m_buffer;
        QImage      m_image;

        void setRgba8888(const void* buffer, const int& w, const int& h);
    };

public:
    explicit QBitmapPreviewDialog(QWidget* parent = 0);
    virtual ~QBitmapPreviewDialog();
    QSize GetCurrentBitmapSize();
    QSize GetOriginalImageSize();

protected:
    void setImageRgba8888(const void* buffer, const int& w, const int& h, const QString& info);
    void setSize(QString _value);
    void setMips(QString _value);
    void setMean(QString _value);
    void setMedian(QString _value);
    void setStdDev(QString _value);
    QRect getHistogramArea();
    void setFullSize(const bool& fullSize);

    void paintEvent(QPaintEvent* e) override;

private:
    void drawImageData(const QRect& rect, const ImageData& imgData);

protected:
    Ui::QBitmapTooltip* ui;
    QSize       m_initialSize;
    ImageData   m_checker;
    ImageData   m_imageMain;
};

#endif // QBITMAPPREVIEWDIALOG_H
