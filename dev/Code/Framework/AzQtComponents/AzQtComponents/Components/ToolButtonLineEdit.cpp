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
#include <AzQtComponents/Components/ToolButtonLineEdit.h>
#include <QLineEdit>

namespace AzQtComponents
{
    ToolButtonLineEdit::ToolButtonLineEdit(QWidget* parent)
        : ToolButtonWithWidget(new QLineEdit(), parent)
        , m_lineEdit(static_cast<QLineEdit*>(widget()))
    {
        m_lineEdit->setProperty("class", "ToolButtonLineEdit");
    }

    void ToolButtonLineEdit::clear()
    {
        m_lineEdit->clear();
    }

    QString ToolButtonLineEdit::text() const
    {
        return m_lineEdit->text();
    }

    void ToolButtonLineEdit::setText(const QString& text)
    {
        m_lineEdit->setText(text);
    }

    void ToolButtonLineEdit::setPlaceholderText(const QString& text)
    {
        m_lineEdit->setPlaceholderText(text);
    }

    QLineEdit* ToolButtonLineEdit::lineEdit() const
    {
        return m_lineEdit;
    }

#include <Components/ToolButtonLineEdit.moc>
} // namespace AzQtComponents
