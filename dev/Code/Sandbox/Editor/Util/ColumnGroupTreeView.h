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
#ifndef COLUMNGROUPTREEVIEW_H
#define COLUMNGROUPTREEVIEW_H

#include <QTreeView>
#include <QPainter>

class ColumnGroupProxyModel;
class ColumnGroupHeaderView;

class ColumnGroupTreeView
    : public QTreeView
{
    Q_OBJECT

public:
    ColumnGroupTreeView(QWidget* parent = 0);

    void setModel(QAbstractItemModel* model) override;

    bool IsGroupsShown() const;

    QModelIndex mapToSource(const QModelIndex& proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex& sourceModel) const;

public slots:
    void ShowGroups(bool showGroups);

    void Sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    void ToggleSortOrder(int column);

    void AddGroup(int column);
    void RemoveGroup(int column);
    void SetGroups(const QVector<int>& columns);
    void ClearGroups();
    QVector<int> Groups() const;

protected:
    void paintEvent(QPaintEvent* event)
    {
        if (model() && model()->rowCount() > 0)
        {
            QTreeView::paintEvent(event);
        }
        else
        {
            const QMargins margins(2, 2, 2, 2);
            QPainter painter(viewport());
            QString text(tr("There are no items to show."));
            QRect textRect = painter.fontMetrics().boundingRect(text).marginsAdded(margins);
            textRect.moveCenter(viewport()->rect().center());
            textRect.moveTop(viewport()->rect().top());
            painter.drawText(textRect, Qt::AlignCenter, text);
        }
    }

private slots:
    void SaveOpenState();
    void RestoreOpenState();
    void SpanGroups(const QModelIndex& index = QModelIndex());

private:
    ColumnGroupHeaderView* m_header;
    ColumnGroupProxyModel* m_groupModel;
    QSet<QString> m_openNodes;
    bool m_showGroups;
};

#endif // COLUMNGROUPTREEVIEW_H
