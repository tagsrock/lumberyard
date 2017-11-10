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
#include <AzQtComponents/Components/ToolButtonComboBox.h>
#include <QComboBox>

namespace AzQtComponents
{
    ToolButtonComboBox::ToolButtonComboBox(QWidget* parent)
        : ToolButtonWithWidget(new QComboBox(), parent)
        , m_combo(static_cast<QComboBox*>(widget()))
    {
        m_combo->setEditable(true);
    }

    QComboBox* ToolButtonComboBox::comboBox() const
    {
        return m_combo;
    }

#include <Components/ToolButtonComboBox.moc>
} // namespace AzQtComponents
