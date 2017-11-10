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

class CoordinateSystemToolbarSection
    : public QObject
{
    Q_OBJECT

public:

    explicit CoordinateSystemToolbarSection(QToolBar* parent, bool addSeparator);

    void SetIsEnabled(bool enabled);

    void SetCurrentIndex(int index);

    void SetSnapToGridIsChecked(bool checked);

private slots:

    //! Triggered by keyboard shortcuts.
    //@{
    void HandleCoordinateSystemCycle();
    void HandleSnapToGridToggle();
    //@}

private:

    int CycleSelectedItem();

    EditorWindow* m_editorWindow;
    QComboBox* m_combobox;
    QCheckBox* m_snapCheckbox;
};
