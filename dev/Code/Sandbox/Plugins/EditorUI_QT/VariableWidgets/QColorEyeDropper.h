#ifndef CRYINCLUDE_EDITORUI_QT_QColorEyeDropper_H
#define CRYINCLUDE_EDITORUI_QT_QColorEyeDropper_H
#pragma once
#include "qwidget.h"
#include "qlabel.h"
#include "qdialog.h"
#include "qelapsedtimer.h"

class QColorEyeDropper
    : public QWidget
{
    Q_OBJECT
public:
    QColorEyeDropper(QWidget*);
    ~QColorEyeDropper();

    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

    void StartEyeDropperMode();
    void EndEyeDropperMode();
    
    bool EyeDropperIsActive();

    void RegisterExceptionWidget(QVector<QWidget*> widgets);
    void UnregisterExceptionWidget(QVector<QWidget*> widgets);
    void RegisterExceptionWidget(QWidget* widget);
    void UnregisterExceptionWidget(QWidget* widget);

signals:
    void SignalEyeDropperUpdating();
    void SignalEyeDropperColorPicked(const QColor& color);
    void SignalEndEyeDropper();

private:
    QColor m_centerColor;
    QPixmap m_mouseMask;
    QPixmap m_borderMap;
    QLabel* m_colorDescriptor;
    QLayout* layout;
    QPoint m_cursorPos;
    QPixmap m_sample;
    QTimer* timer;

    void UpdateColor();
    //Paints the eyedropper widget and returns the selected color(center color)
    QColor PaintWidget(const QPoint& mousePosition);
    QColor GrabScreenColor(const QPoint& pos);

    bool m_EyeDropperActive;

    // If the color picker is moved on these exception widget, it will turn back to original fucntionality
    QVector<QWidget*> m_exceptionWidgets;
    bool m_isMouseInException;
    QWidget* m_currentExceptionWidget;
};

#endif