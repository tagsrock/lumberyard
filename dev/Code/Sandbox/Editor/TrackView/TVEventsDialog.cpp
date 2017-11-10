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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "TVEventsDialog.h"
#include <TrackView/ui_TVEventsDialog.h>
#include "StringDlg.h"
#include "TrackViewSequence.h"
#include "AnimationContext.h"
#include <limits>


// CTVEventsDialog dialog

namespace
{
    const int kCountSubItemIndex = 1;
    const int kTimeSubItemIndex = 2;
}

class TVEventsModel
    : public QAbstractTableModel
{
public:
    TVEventsModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid())
        {
            return 0;
        }
        CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
        assert(pSequence);
        return pSequence->GetTrackEventsCount();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 3;
    }

    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override
    {
        if (parent.isValid())
        {
            return false;
        }

        for (int r = row; r < row + count; ++r)
        {
            const QString eventName = index(r, 0).data().toString();
            beginRemoveRows(QModelIndex(), r, r);
            CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
            assert(pSequence);
            pSequence->RemoveTrackEvent(eventName.toLatin1().data());
            endRemoveRows();
        }
        return true;
    }

    bool addRow(const QString& name)
    {
        CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
        assert(pSequence);
        const int index = rowCount();
        beginInsertRows(QModelIndex(), index, index);
        const bool result = pSequence->AddTrackEvent(name.toLatin1().data());
        endInsertRows();
        if (!result)
        {
            beginRemoveRows(QModelIndex(), index, index);
            endRemoveRows();
        }
        return result;
    }

    bool moveRow(const QModelIndex& index, bool up)
    {
        CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
        assert(pSequence);
        if (!index.isValid() || (up && index.row() == 0) || (!up && index.row() == rowCount() - 1))
        {
            return false;
        }
        if (up)
        {
            beginMoveRows(QModelIndex(), index.row(), index.row(), QModelIndex(), index.row() - 1);
            pSequence->MoveUpTrackEvent(index.sibling(index.row(), 0).data().toString().toLatin1().data());
        }
        else
        {
            beginMoveRows(QModelIndex(), index.row() + 1, index.row() + 1, QModelIndex(), index.row());
            pSequence->MoveDownTrackEvent(index.sibling(index.row(), 0).data().toString().toLatin1().data());
        }
        endMoveRows();
        return true;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
        assert(pSequence);
        if (role != Qt::DisplayRole)
        {
            return QVariant();
        }

        float timeFirstUsed;
        int usageCount = GetNumberOfUsageAndFirstTimeUsed(pSequence->GetTrackEvent(index.row()), timeFirstUsed);

        switch (index.column())
        {
        case 0:
            return QString::fromLatin1(pSequence->GetTrackEvent(index.row()));
        case 1:
            return usageCount;
        case 2:
            return usageCount > 0 ? QString::number(timeFirstUsed, 'f', 3) : QString();
        default:
            return QVariant();
        }
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
    {
        CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
        assert(pSequence);
        if (role != Qt::DisplayRole && role != Qt::EditRole)
        {
            return false;
        }
        if (index.column() != 0 || value.toString().isEmpty())
        {
            return false;
        }

        const QString oldName = index.data().toString();
        const QString newName = value.toString();
        pSequence->RenameTrackEvent(oldName.toLatin1().data(), newName.toLatin1().data());
        emit dataChanged(index, index);
        return true;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        {
            return QVariant();
        }

        switch (section)
        {
        case 0:
            return tr("Event");
        case 1:
            return tr("# of use");
        case 2:
            return tr("Time of first usage");
        default:
            return QVariant();
        }
    }

    int GetNumberOfUsageAndFirstTimeUsed(const char* eventName, float& timeFirstUsed) const;
};

CTVEventsDialog::CTVEventsDialog(QWidget* pParent /*=NULL*/)
    : QDialog(pParent)
    , m_ui(new Ui::TVEventsDialog)
{
    m_ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    OnInitDialog();

    connect(m_ui->buttonAddEvent, &QPushButton::clicked, this, &CTVEventsDialog::OnBnClickedButtonAddEvent);
    connect(m_ui->buttonRemoveEvent, &QPushButton::clicked, this, &CTVEventsDialog::OnBnClickedButtonRemoveEvent);
    connect(m_ui->buttonRenameEvent, &QPushButton::clicked, this, &CTVEventsDialog::OnBnClickedButtonRenameEvent);
    connect(m_ui->buttonUpEvent, &QPushButton::clicked, this, &CTVEventsDialog::OnBnClickedButtonUpEvent);
    connect(m_ui->buttonDownEvent, &QPushButton::clicked, this, &CTVEventsDialog::OnBnClickedButtonDownEvent);
    connect(m_ui->m_List->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CTVEventsDialog::OnListItemChanged);
}

CTVEventsDialog::~CTVEventsDialog()
{
}

// CTVEventsDialog message handlers

void CTVEventsDialog::OnBnClickedButtonAddEvent()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    const QString add = QInputDialog::getText(this, tr("Track Event Name"), QString());
    if (!add.isEmpty() && static_cast<TVEventsModel*>(m_ui->m_List->model())->addRow(add))
    {
        m_ui->m_List->setCurrentIndex(m_ui->m_List->model()->index(m_ui->m_List->model()->rowCount() - 1, 0));
    }
    m_ui->m_List->setFocus();
}

void CTVEventsDialog::OnBnClickedButtonRemoveEvent()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    QList<QPersistentModelIndex> indexes;
    for (auto index : m_ui->m_List->selectionModel()->selectedRows())
    {
        indexes.push_back(index);
    }

    for (auto index : indexes)
    {
        if (QMessageBox::warning(this, tr("Remove Event"), tr("This removal might cause some link breakages in Flow Graph.\nStill continue?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            m_ui->m_List->model()->removeRow(index.row());
        }
    }
    m_ui->m_List->setFocus();
}

void CTVEventsDialog::OnBnClickedButtonRenameEvent()
{
    const QModelIndex index = m_ui->m_List->currentIndex();

    if (index.isValid())
    {
        const QString newName = QInputDialog::getText(this, tr("Track Event Name"), QString());
        if (!newName.isEmpty())
        {
            m_ui->m_List->model()->setData(index.sibling(index.row(), 0), newName);
        }
    }
    m_ui->m_List->setFocus();
}

void CTVEventsDialog::OnBnClickedButtonUpEvent()
{
    static_cast<TVEventsModel*>(m_ui->m_List->model())->moveRow(m_ui->m_List->currentIndex(), true);
    UpdateButtons();
    m_ui->m_List->setFocus();
}

void CTVEventsDialog::OnBnClickedButtonDownEvent()
{
    static_cast<TVEventsModel*>(m_ui->m_List->model())->moveRow(m_ui->m_List->currentIndex(), false);
    UpdateButtons();
    m_ui->m_List->setFocus();
}

void CTVEventsDialog::OnInitDialog()
{
    m_ui->m_List->setModel(new TVEventsModel(this));
    m_ui->m_List->header()->resizeSections(QHeaderView::ResizeToContents);

    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
    assert(pSequence);

    UpdateButtons();
}

void CTVEventsDialog::OnListItemChanged()
{
    UpdateButtons();
}

void CTVEventsDialog::UpdateButtons()
{
    bool bRemove = false, bRename = false, bUp = false, bDown = false;

    int nSelected = m_ui->m_List->selectionModel()->selectedRows().count();
    if (nSelected > 1)
    {
        bRemove = true;
        bRename = false;
    }
    else if (nSelected > 0)
    {
        bRemove = bRename = true;

        const QModelIndex index = m_ui->m_List->selectionModel()->selectedRows().first();
        if (index.row() > 0)
        {
            bUp = true;
        }
        if (index.row() < m_ui->m_List->model()->rowCount() - 1)
        {
            bDown = true;
        }
    }

    m_ui->buttonRemoveEvent->setEnabled(bRemove);
    m_ui->buttonRenameEvent->setEnabled(bRename);
    m_ui->buttonUpEvent->setEnabled(bUp);
    m_ui->buttonDownEvent->setEnabled(bDown);
}

int TVEventsModel::GetNumberOfUsageAndFirstTimeUsed(const char* eventName, float& timeFirstUsed) const
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    int usageCount = 0;
    float firstTime = std::numeric_limits<float>::max();

    CTrackViewAnimNodeBundle nodeBundle = pSequence->GetAnimNodesByType(eAnimNodeType_Event);
    const unsigned int numNodes = nodeBundle.GetCount();

    for (unsigned int currentNode = 0; currentNode < numNodes; ++currentNode)
    {
        CTrackViewAnimNode* pCurrentNode = nodeBundle.GetNode(currentNode);

        CTrackViewTrackBundle tracks = pCurrentNode->GetTracksByParam(eAnimParamType_TrackEvent);
        const unsigned int numTracks = tracks.GetCount();

        for (unsigned int currentTrack = 0; currentTrack < numTracks; ++currentTrack)
        {
            CTrackViewTrack* pTrack = tracks.GetTrack(currentTrack);

            for (int currentKey = 0; currentKey < pTrack->GetKeyCount(); ++currentKey)
            {
                CTrackViewKeyHandle keyHandle = pTrack->GetKey(currentKey);

                IEventKey key;
                keyHandle.GetKey(&key);

                if (strcmp(key.event, eventName) == 0) // If it has a key with the specified event set
                {
                    ++usageCount;
                    if (key.time < firstTime)
                    {
                        firstTime = key.time;
                    }
                }
            }
        }
    }

    if (usageCount > 0)
    {
        timeFirstUsed = firstTime;
    }
    return usageCount;
}

#include <TrackView/TVEventsDialog.moc>
