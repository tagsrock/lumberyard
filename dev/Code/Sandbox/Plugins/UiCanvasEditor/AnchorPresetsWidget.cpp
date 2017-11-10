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
#include "stdafx.h"
#include "PresetButton.h"
#include "AnchorPresets.h"
#include "AnchorPresetsWidget.h"

#include <AzToolsFramework/UI/PropertyEditor/PropertyQTConstants.h>

#define UICANVASEDITOR_ANCHOR_ICON_PATH_DEFAULT(presetIndex) (QString(":/Icons/AnchorIcon%1Default.tif").arg(presetIndex, 2, 10, QChar('0')))
#define UICANVASEDITOR_ANCHOR_ICON_PATH_HOVER(presetIndex) (QString(":/Icons/AnchorIcon%1Hover.tif").arg(presetIndex, 2, 10, QChar('0')))
#define UICANVASEDITOR_ANCHOR_ICON_PATH_SELECTED(presetIndex) (QString(":/Icons/AnchorIcon%1Selected.tif").arg(presetIndex, 2, 10, QChar('0')))

#define UICANVASEDITOR_ANCHOR_WIDGET_FIXED_SIZE             (106)
#define UICANVASEDITOR_ANCHOR_BUTTON_AND_ICON_FIXED_SIZE    (20)

AnchorPresetsWidget::AnchorPresetsWidget(int defaultPresetIndex,
    PresetChanger presetChanger,
    QWidget* parent)
    : QWidget(parent)
    , m_presetIndex(defaultPresetIndex)
    , m_buttons(AnchorPresets::PresetIndexCount, nullptr)
{
    setFixedSize(UICANVASEDITOR_ANCHOR_WIDGET_FIXED_SIZE, UICANVASEDITOR_ANCHOR_WIDGET_FIXED_SIZE);

    // The layout.
    QGridLayout* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    // Preset buttons.
    {
        for (int presetIndex = 0; presetIndex < AnchorPresets::PresetIndexCount; ++presetIndex)
        {
            PresetButton* button = new PresetButton(UICANVASEDITOR_ANCHOR_ICON_PATH_DEFAULT(presetIndex),
                    UICANVASEDITOR_ANCHOR_ICON_PATH_HOVER(presetIndex),
                    UICANVASEDITOR_ANCHOR_ICON_PATH_SELECTED(presetIndex),
                    QSize(UICANVASEDITOR_ANCHOR_BUTTON_AND_ICON_FIXED_SIZE, UICANVASEDITOR_ANCHOR_BUTTON_AND_ICON_FIXED_SIZE),
                    "",
                    [ this, presetChanger, presetIndex ](bool checked)
                    {
                        SetPresetSelection(presetIndex);

                        presetChanger(presetIndex);
                    },
                    this);

            grid->addWidget(button, (presetIndex / 4), (presetIndex % 4));

            m_buttons[ presetIndex ] = button;
        }
    }
}

void AnchorPresetsWidget::SetPresetSelection(int presetIndex)
{
    if (m_presetIndex != -1)
    {
        // Clear the old selection.
        m_buttons[ m_presetIndex ]->setChecked(false);
    }

    if (presetIndex != -1)
    {
        // Set the new selection.
        m_buttons[ presetIndex ]->setChecked(true);
    }

    m_presetIndex = presetIndex;
}

#include <AnchorPresetsWidget.moc>
