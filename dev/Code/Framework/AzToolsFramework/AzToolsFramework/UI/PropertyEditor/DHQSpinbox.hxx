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

#ifndef AZ_SPINBOX_HXX
#define AZ_SPINBOX_HXX

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <QtWidgets/QSpinBox>


#pragma once

namespace AzToolsFramework
{
    class DHQSpinbox
        : public QSpinBox
    {
    public:
        AZ_CLASS_ALLOCATOR(DHQSpinbox, AZ::SystemAllocator, 0);

        explicit DHQSpinbox(QWidget* parent = 0);

        QSize minimumSizeHint() const override;

        bool event(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

        // NOTE: setValue() is not virtual, but is in the base class. In order for this to work
        // YOU MUST USE A POINTER TO DHQSpinbox, NOT A POINTER TO QSpinBox
        // Needed so that we can track of the last value properly for trapping the Escape key
        void setValue(int value);

    protected:
        QPoint lastMousePosition;
        bool mouseCaptured = false;

    private:
        int m_lastValue;
    };

    class DHQDoubleSpinbox
        : public QDoubleSpinBox
    {
    public:
        AZ_CLASS_ALLOCATOR(DHQDoubleSpinbox, AZ::SystemAllocator, 0);

        explicit DHQDoubleSpinbox(QWidget* parent = 0);

        QSize minimumSizeHint() const override;

        bool event(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

        // NOTE: setValue() is not virtual, but is in the base class. In order for this to work
        // YOU MUST USE A POINTER TO DHQDoubleSpinbox, NOT A POINTER TO QSpinBox
        // Needed so that we can track of the last value properly for trapping the Escape key
        void setValue(double value);

    protected:
        QPoint lastMousePosition;
        bool mouseCaptured = false;

    private:
        double m_lastValue;
    };
}

#endif
