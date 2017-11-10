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

#include <QtWidgets/QFrame>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace AzToolsFramework
{
    /**
     * Header bar for component editor info and indicators.
     * The widgets are hidden by default and will show once they're configured
     * via the appropriate setter (ex: SetIcon causes the icon widget to appear).
     */
    class ComponentEditorHeader
        : public QFrame
    {
        Q_OBJECT;
    public:
        ComponentEditorHeader(QWidget* parent = nullptr);

        /// Set a title. Passing an empty string will hide the widget.
        void SetTitle(const QString& title);

        /// Set an icon. Passing a null icon will hide the widget.
        void SetIcon(const QIcon& icon);

        /// Set whether the header has an expand/contract button.
        /// Note that the header itself will not change size or hide, it
        /// simply causes the OnExpanderChanged signal to fire.
        void SetExpandable(bool expandable);
        bool IsExpandable() const;

        void SetExpanded(bool expanded);
        bool IsExpanded() const;

        void SetSelected(bool selected);
        bool IsSelected() const;

        void SetWarningIcon(const QIcon& icon);
        void SetWarning(bool warning);
        bool IsWarning() const;

        void SetReadOnly(bool readOnly);
        bool IsReadOnly() const;

        /// Set whether the header has a context menu widget.
        /// This widget can fire the OnContextMenuClicked signal.
        /// This widget will also display any menu set via SetContextMenu(QMenu*).
        void SetHasContextMenu(bool showContextMenu);

    Q_SIGNALS:
        void OnContextMenuClicked(const QPoint& position);
        void OnExpanderChanged(bool expanded);

    private:
        void mousePressEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;
        void TriggerContextMenuUnderButton();
        void UpdateStyleSheets();

        // Widgets in header
        QVBoxLayout* m_mainLayout = nullptr;
        QHBoxLayout* m_backgroundLayout = nullptr;
        QFrame* m_backgroundFrame = nullptr;
        QPushButton* m_expanderButton = nullptr;
        QLabel* m_iconLabel = nullptr;
        QLabel* m_titleLabel = nullptr;
        QLabel* m_warningLabel = nullptr;
        QPushButton* m_contextMenuButton = nullptr;
        bool m_selected = false;
        bool m_warning = false;
        bool m_readOnly = false;
    };
}