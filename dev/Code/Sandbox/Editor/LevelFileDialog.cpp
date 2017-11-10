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
#include "LevelFileDialog.h"
#include "LevelTreeModel.h"
#include "Dialogs/Generic/UserOptions.h"
#include "CryEditDoc.h"

#include <QLabel>
#include <QLineEdit>
#include <QTreeView>
#include <QPushButton>
#include <QShowEvent>
#include <QtUtil.h>
#include <QtUtilWin.h>
#include <QMessageBox>
#include <QInputDialog>
#include <qdebug.h>

#include <ui_LevelFileDialog.h>

static const char lastLoadPathFilename[] = "lastLoadPath.preset";

// File name extension for the main level file
static const char kLevelExtension[] = "cry";

// Folder in which levels are stored
static const char kLevelsFolder[] = "Levels";

// List of folder names that are used to detect a level folder
static const char* kLevelFolderNames[] =
{
    "Layers",
    "Minimap",
    "LevelData"
};

// List of files that are used to detect a level folder
static const char* kLevelFileNames[] =
{
    "level.pak",
    "terraintexture.pak",
    "filelist.xml",
    "levelshadercache.pak",
    "terrain\\cover.ctc"
};

CLevelFileDialog::CLevelFileDialog(bool openDialog, QWidget* parent)
    : QDialog(parent)
    , m_bOpenDialog(openDialog)
    , ui(new Ui::Dialog())
    , m_model(new LevelTreeModel(this))
    , m_filterModel(new LevelTreeModelFilter(this))
{
    ui->setupUi(this);
    ui->treeView->header()->close();
    m_filterModel->setSourceModel(m_model);
    ui->treeView->setModel(m_filterModel);

    connect(ui->treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &CLevelFileDialog::OnTreeSelectionChanged);

    connect(ui->treeView, &QTreeView::doubleClicked, this, [this]()
        {
            if (m_bOpenDialog && !IsValidLevelSelected())
            {
                return;
            }

            OnOK();
        });

    connect(ui->filterLineEdit, &QLineEdit::textChanged, this, &CLevelFileDialog::OnFilterChanged);
    connect(ui->cancelButton, &QPushButton::clicked, this, &CLevelFileDialog::OnCancel);
    connect(ui->okButton, &QPushButton::clicked, this, &CLevelFileDialog::OnOK);
    connect(ui->newFolderButton, &QPushButton::clicked, this, &CLevelFileDialog::OnNewFolder);

    if (m_bOpenDialog)
    {
        setWindowTitle(tr("Open Level"));
        ui->newFolderButton->setVisible(false);
        ui->okButton->setText(tr("Open"));
    }
    else
    {
        setWindowTitle(tr("Save Level As "));
        ui->okButton->setText(tr("Save"));
    }

    ReloadTree();
    ui->filterLineEdit->setFocus(Qt::OtherFocusReason);
    LoadLastUsedLevelPath();
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

CLevelFileDialog::~CLevelFileDialog()
{
}

QString CLevelFileDialog::GetFileName() const
{
    return m_fileName;
}

void CLevelFileDialog::OnCancel()
{
    close();
}

void CLevelFileDialog::OnOK()
{
    if (m_bOpenDialog)
    {
        // For Open button
        if (!IsValidLevelSelected())
        {
            QMessageBox box(this);
            box.setText(tr("Please enter a valid level name"));
            box.setIcon(QMessageBox::Critical);
            box.exec();
            return;
        }
    }
    else
    {
        QString enteredPath = GetEnteredPath();
        QString levelPath = GetLevelPath();

        // Make sure that this folder can be used as a level folder:
        // - It is a valid level path
        // - It isn't already a file
        // - There are no other level folders under it
        if (!CryStringUtils::IsValidFileName(Path::GetFileName(levelPath).toLatin1().data()))
        {
            QMessageBox box(this);
            box.setText(tr("Please enter a valid level name (standard English alphanumeric characters only)"));
            box.setIcon(QMessageBox::Critical);
            box.exec();
            return;
        }

        //Verify that we are not using the temporary level name
        const char* temporaryLevelName = GetIEditor()->GetDocument()->GetTemporaryLevelName();
        if (QString::compare(Path::GetFileName(levelPath), temporaryLevelName) == 0)
        {
            QMessageBox::warning(this, tr("Error"), tr("Please enter a level name that is different from the temporary name"));
            return;
        }

        if (!ValidateLevelPath(enteredPath))
        {
            QMessageBox::warning(this, tr("Error"), tr("Please enter a valid level location.\nYou cannot save levels inside levels."));
            return;
        }

        if (CFileUtil::FileExists(levelPath))
        {
            QMessageBox box(this);
            box.setText(tr("A file with that name already exists"));
            box.setIcon(QMessageBox::Critical);
            box.exec();
            return;
        }

        if (CheckSubFoldersForLevelsRec(levelPath))
        {
            QMessageBox box(this);
            box.setText(tr("You cannot save a level in a folder with sub\nfolders that contain levels"));
            box.setIcon(QMessageBox::Critical);
            box.exec();
            return;
        }

        // Check if there is already a level folder at that
        // location, if so ask before for overwriting it
        if (CheckLevelFolder(levelPath))
        {
            QMessageBox box(this);
            box.setText(tr("Do you really want to overwrite '%1'?").arg(enteredPath));
            box.setIcon(QMessageBox::Warning);
            box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            if (box.exec() != QMessageBox::Yes)
            {
                return;
            }
        }

        m_fileName = levelPath + "/" + Path::GetFileName(levelPath) + "." + kLevelExtension;
    }

    SaveLastUsedLevelPath();
    accept();
}

QString CLevelFileDialog::NameForIndex(const QModelIndex& index) const
{
    QStringList tokens;
    QModelIndex idx = index;

    while (idx.isValid() && idx.parent().isValid())   // the root one doesn't count
    {
        tokens.push_front(idx.data(Qt::DisplayRole).toString());
        idx = idx.parent();
    }

    QString text = tokens.join('/');
    const bool isLevelFolder = index.data(LevelTreeModel::IsLevelFolderRole).toBool();
    if (!isLevelFolder && !text.isEmpty())
    {
        text += "/";
    }

    return text;
}

bool CLevelFileDialog::IsValidLevelSelected()
{
    QString levelPath = GetLevelPath();
    m_fileName = GetFileName(levelPath);

    bool isInvalidFileExtension = Path::GetExt(m_fileName) != kLevelExtension;

    if (!isInvalidFileExtension && CFileUtil::FileExists(m_fileName))
    {
        return true;
    }
    else
    {
        return false;
    }
}

QString CLevelFileDialog::GetLevelPath()
{
    const QString enteredPath = GetEnteredPath();
    const QString levelPath = QString("%1/%2/%3").arg(Path::GetEditingGameDataFolder().c_str()).arg(kLevelsFolder).arg(enteredPath);
    return levelPath;
}

QString CLevelFileDialog::GetEnteredPath()
{
    QString enteredPath = ui->nameLineEdit->text();
    enteredPath = enteredPath.trimmed();
    enteredPath = Path::RemoveBackslash(enteredPath);

    return enteredPath;
}

QString CLevelFileDialog::GetFileName(QString levelPath)
{
    QStringList levelFiles;
    QString fileName;

    if (CheckLevelFolder(levelPath, &levelFiles) && levelFiles.size() >= 1)
    {
        // A level folder was entered. Prefer the .cry file with the
        // folder name, otherwise pick the first one in the list
        QString needle = Path::GetFileName(levelPath) + "." + kLevelExtension;
        auto iter = std::find(levelFiles.begin(), levelFiles.end(), needle);

        if (iter != levelFiles.end())
        {
            fileName = levelPath + "/" + *iter;
        }
        else
        {
            fileName = levelPath + "/" + levelFiles[0];
        }
    }
    else
    {
        // Otherwise try to directly load the specified file (backward compatibility)
        fileName = levelPath;
    }

    return fileName;
}

void CLevelFileDialog::OnTreeSelectionChanged()
{
    const QModelIndexList indexes = ui->treeView->selectionModel()->selectedIndexes();
    if (!indexes.isEmpty())
    {
        ui->nameLineEdit->setText(NameForIndex(indexes.first()));
    }
}

void CLevelFileDialog::OnNewFolder()
{
    const QModelIndexList indexes = ui->treeView->selectionModel()->selectedIndexes();

    if (indexes.isEmpty())
    {
        QMessageBox box(this);
        box.setText(tr("Please select a folder first"));
        box.setIcon(QMessageBox::Critical);
        box.exec();
        return;
    }

    const QModelIndex index = indexes.first();
    const bool isLevelFolder = index.data(LevelTreeModel::IsLevelFolderRole).toBool();

    // Creating folders is not allowed in level folders
    if (!isLevelFolder && index.isValid())
    {
        const QString parentFullPath = index.data(LevelTreeModel::FullPathRole).toString();
        bool ok = false;
        QInputDialog inputDlg(this);
        inputDlg.setLabelText(tr("Please select a folder name"));

        if (inputDlg.exec() == QDialog::Accepted && !inputDlg.textValue().isEmpty())
        {
            const QString newFolderName = inputDlg.textValue();
            const QString newFolderPath = parentFullPath + "/" + newFolderName;

            if (!CryStringUtils::IsValidFileName(newFolderName.toLatin1().data()))
            {
                QMessageBox box(this);
                box.setText(tr("Please enter a single, valid folder name(standard English alphanumeric characters only)"));
                box.setIcon(QMessageBox::Critical);
                box.exec();
                return;
            }

            if (CFileUtil::PathExists(newFolderPath))
            {
                QMessageBox box(this);
                box.setText(tr("Folder already exists"));
                box.setIcon(QMessageBox::Critical);
                box.exec();
                return;
            }

            // The trailing / is important, otherwise CreatePath doesn't work
            if (!CFileUtil::CreatePath(newFolderPath + "/"))
            {
                QMessageBox box(this);
                box.setText(tr("Could not create folder"));
                box.setIcon(QMessageBox::Critical);
                box.exec();
                return;
            }

            m_model->AddItem(newFolderName, m_filterModel->mapToSource(index));
            ui->treeView->expand(index);
        }
    }
    else
    {
        QMessageBox box(this);
        box.setText(tr("Please select a folder first"));
        box.setIcon(QMessageBox::Critical);
        box.exec();
        return;
    }
}

void CLevelFileDialog::OnFilterChanged()
{
    m_filterModel->setFilterText(ui->filterLineEdit->text().toLower());
}

void CLevelFileDialog::ReloadTree()
{
    m_model->ReloadTree(m_bOpenDialog);
}

//////////////////////////////////////////////////////////////////////////
// Heuristic to detect a level folder, also returns all .cry files in it
//////////////////////////////////////////////////////////////////////////
bool CLevelFileDialog::CheckLevelFolder(const QString folder, QStringList* levelFiles)
{
    CFileEnum fileEnum;
    QFileInfo fileData;
    bool bIsLevelFolder = false;

    for (bool bFoundFile = fileEnum.StartEnumeration(folder, "*", &fileData);
         bFoundFile; bFoundFile = fileEnum.GetNextFile(&fileData))
    {
        const QString fileName = fileData.fileName();

        // Have we found a folder?
        if (fileData.isDir())
        {
            // Skip the parent folder entries
            if (fileName == "." || fileName == "..")
            {
                continue;
            }

            for (unsigned int i = 0; i < sizeof(kLevelFolderNames) / sizeof(char*); ++i)
            {
                if (fileName == kLevelFolderNames[i])
                {
                    bIsLevelFolder = true;
                }
            }
        }
        else
        {
            QString ext = Path::GetExt(fileName);

            if (ext == kLevelExtension)
            {
                bIsLevelFolder = true;

                if (levelFiles)
                {
                    levelFiles->push_back(fileName);
                }
            }

            for (unsigned int i = 0; i < sizeof(kLevelFileNames) / sizeof(char*); ++i)
            {
                if (fileName == kLevelFileNames[i])
                {
                    bIsLevelFolder = true;
                }
            }
        }
    }

    return bIsLevelFolder;
}


//////////////////////////////////////////////////////////////////////////
// Checks if there are levels in the sub folders of a folder
//////////////////////////////////////////////////////////////////////////
bool CLevelFileDialog::CheckSubFoldersForLevelsRec(const QString folder, bool bRoot)
{
    if (!bRoot && CheckLevelFolder(folder))
    {
        return true;
    }

    CFileEnum fileEnum;
    QFileInfo fileData;
    bool bIsLevelFolder = false;

    for (bool bFoundFile = fileEnum.StartEnumeration(folder, "*", &fileData);
         bFoundFile; bFoundFile = fileEnum.GetNextFile(&fileData))
    {
        const QString fileName = fileData.fileName();

        // Have we found a folder?
        if (fileData.isDir())
        {
            // Skip the parent folder entries
            if (fileName == "." || fileName == "..")
            {
                continue;
            }

            // Check if this sub folder contains a level
            if (CheckSubFoldersForLevelsRec(folder + "/" + fileName, false))
            {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
// Checks if a given path is a valid level path
//////////////////////////////////////////////////////////////////////////
bool CLevelFileDialog::ValidateLevelPath(const QString& levelPath)
{
    if (levelPath.isEmpty() || Path::GetExt(levelPath) != "")
    {
        return false;
    }

    // Split path
    QStringList splittedPath = levelPath.split(QRegularExpression(QStringLiteral(R"([\\/])")), QString::SkipEmptyParts);

    // This shouldn't happen, but be careful
    if (splittedPath.empty())
    {
        return false;
    }

    // Make sure that no folder before the last in the name contains a level
    if (splittedPath.size() > 1)
    {
        QString currentPath = (Path::GetEditingGameDataFolder() + "/" + kLevelsFolder).c_str();
        for (size_t i = 0; i < splittedPath.size() - 1; ++i)
        {
            currentPath += "/" + splittedPath[i];

            if (CheckLevelFolder(currentPath))
            {
                return false;
            }
        }
    }

    return true;
}

void CLevelFileDialog::SaveLastUsedLevelPath()
{
    const QString settingPath = QString(GetIEditor()->GetUserFolder()) + lastLoadPathFilename;

    XmlNodeRef lastUsedLevelPathNode = XmlHelpers::CreateXmlNode("lastusedlevelpath");
    lastUsedLevelPathNode->setAttr("path", ui->nameLineEdit->text().toLatin1().data());

    XmlHelpers::SaveXmlNode(GetIEditor()->GetFileUtil(), lastUsedLevelPathNode, settingPath.toLatin1().data());
}

void CLevelFileDialog::LoadLastUsedLevelPath()
{
    const QString settingPath = QString(GetIEditor()->GetUserFolder()) + lastLoadPathFilename;
    if (!QFile::exists(settingPath))
    {
        return;
    }

    XmlNodeRef lastUsedLevelPathNode = XmlHelpers::LoadXmlFromFile(settingPath.toLatin1().data());
    if (lastUsedLevelPathNode == nullptr)
    {
        return;
    }

    QString lastLoadedFileName;
    lastUsedLevelPathNode->getAttr("path", lastLoadedFileName);

    if (m_filterModel->rowCount() < 1)
    {
        // Defensive, doesn't happen
        return;
    }

    QModelIndex currentIndex = m_filterModel->index(0, 0); // Start with "Levels/" node
    QStringList segments = Path::SplitIntoSegments(lastLoadedFileName);
    for (auto it = segments.cbegin(), end = segments.cend(); it != end; ++it)
    {
        const int numChildren = m_filterModel->rowCount(currentIndex);
        for (int i = 0; i < numChildren; ++i)
        {
            const QModelIndex subIndex = m_filterModel->index(i, 0, currentIndex);
            if (*it == subIndex.data(Qt::DisplayRole).toString())
            {
                ui->treeView->expand(currentIndex);
                currentIndex = subIndex;
                break;
            }
        }
    }

    if (currentIndex.isValid())
    {
        ui->treeView->selectionModel()->select(currentIndex, QItemSelectionModel::Select);
    }

    ui->nameLineEdit->setText(lastLoadedFileName);
}

#include <LevelFileDialog.moc>
