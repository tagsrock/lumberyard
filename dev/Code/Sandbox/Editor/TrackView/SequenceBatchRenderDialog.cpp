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

// Description : A dialog for batch-rendering sequences


#include "stdafx.h"
#include "SequenceBatchRenderDialog.h"
#include "CustomResolutionDlg.h"
#include "ViewPane.h"
#include "GameEngine.h"

#include <TrackView/ui_SequenceBatchRenderDialog.h>

#include <QtUtilWin.h>

#if defined (Q_OS_WIN)
#include <QtWinExtras/QtWin>
#endif

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringListModel>
#include <QStyle>
#include <QToolButton>

#include <QtConcurrent>

namespace
{
    const int g_useActiveViewportResolution = -1;   // reserved value to indicate the use of the active viewport resolution
    int resolutions[][2] = {
        { 1280, 720 }, { 1920, 1080 }, { 1998, 1080 }, { 2048, 858 }, { 2560, 1440 }, 
        { g_useActiveViewportResolution, g_useActiveViewportResolution }    // active viewport res must be the last element of the resolution array
    };

    // cached current active viewport resolution
    int activeViewportWidth;
    int activeViewportHeight;

    struct SFPSPair
    {
        int fps;
        const char* fpsDesc;
    };
    SFPSPair fps[] = {
        {24, "Film(24)"}, {25, "PAL(25)"}, {30, "NTSC(30)"},
        {48, "Show(48)"}, {50, "PAL Field(50)"}, {60, "NTSC Field(60)"}
    };

    // The text and ordering of these strings need to match ICaptureKey::CaptureFileFormat. These strings are used
    // both for the comboBox UI strings as well as the file extension strings
    const char* imageFormats[ICaptureKey::NumCaptureFileFormats] = { "jpg", "tga", "tif" };

    // The text and ordering of these strings need to match ICaptureKey::CaptureBufferType
    const char* buffersToCapture[ICaptureKey::NumCaptureBufferTypes] = { "Color", "Color+Alpha" };

    const char defaultPresetFilename[] = "defaultBatchRender.preset";

    const char customResFormat[] = "Custom(%1 x %2)...";

    const int kBatchRenderFileVersion = 2; // This version number should be incremented every time available options like the list of formats,
    // the list of buffers change.

    const int kWarmingUpDuration = 1000; // 1000ms == 1s

    // get the actual render width to use (substitutes active viewport width if needed)
    int getResWidth(int renderItemWidth)
    {
        return (renderItemWidth == g_useActiveViewportResolution) ? activeViewportWidth : renderItemWidth;
    }
    // get the actual render height to use (substitutes active viewport height if needed)
    int getResHeight(int renderItemHeight)
    {
        return (renderItemHeight == g_useActiveViewportResolution) ? activeViewportHeight : renderItemHeight;
    }
}

CSequenceBatchRenderDialog::CSequenceBatchRenderDialog(float fps, QWidget* pParent /* = nullptr */)
    : QDialog(pParent)
    , m_fpsForTimeToFrameConversion(fps)
    , m_customResH(0)
    , m_customResW(0)
    , m_customFPS(0)
    , m_bFFMPEGCommandAvailable(false)
    , m_ui(new Ui::SequenceBatchRenderDialog)
    , m_renderListModel(new QStringListModel(this))

{
    m_ui->setupUi(this);
    setFixedSize(size());
    m_ui->m_renderList->setModel(m_renderListModel);

    OnInitDialog();

    connect(&m_renderTimer, &QTimer::timeout, this, &CSequenceBatchRenderDialog::OnKickIdleTimout);
    m_renderTimer.setInterval(0);
    m_renderTimer.setSingleShot(true);
}

CSequenceBatchRenderDialog::~CSequenceBatchRenderDialog()
{
}

void CSequenceBatchRenderDialog::reject()
{
    if (m_renderContext.IsInRendering())
    {
        OnCancelRender();
    }
    else
    {
        QDialog::reject();
    }
}

void CSequenceBatchRenderDialog::OnInitDialog()
{
    QAction* browseAction = m_ui->m_destinationEdit->addAction(style()->standardPixmap(QStyle::SP_DirOpenIcon), QLineEdit::TrailingPosition);
    connect(browseAction, &QAction::triggered, [=]()
    {
        const QString dir = QFileDialog::getExistingDirectory(this);
        if (!dir.isEmpty())
        {
            m_ui->m_destinationEdit->setText(dir);
        }
    });

    void(QComboBox::* activated)(int) = &QComboBox::activated;
    void(QSpinBox::* editingFinished)() = &QSpinBox::editingFinished;

    connect(m_ui->BATCH_RENDER_ADD_SEQ, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnAddRenderItem);
    connect(m_ui->BATCH_RENDER_REMOVE_SEQ, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnRemoveRenderItem);
    connect(m_ui->BATCH_RENDER_CLEAR_SEQ, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnClearRenderItems);
    connect(m_ui->m_updateBtn, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnUpdateRenderItem);
    connect(m_ui->BATCH_RENDER_LOAD_PRESET, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnLoadPreset);
    connect(m_ui->BATCH_RENDER_SAVE_PRESET, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnSavePreset);
    connect(m_ui->BATCH_RENDER_LOAD_BATCH, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnLoadBatch);
    connect(m_ui->BATCH_RENDER_SAVE_BATCH, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnSaveBatch);
    connect(m_ui->m_pGoBtn, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnGo);
    connect(m_ui->CANCEL, &QPushButton::clicked, this, &CSequenceBatchRenderDialog::OnDone);
    connect(m_ui->m_sequenceCombo, activated, this, &CSequenceBatchRenderDialog::OnSequenceSelected);
    connect(m_ui->m_fpsCombo->lineEdit(), &QLineEdit::textEdited, this, &CSequenceBatchRenderDialog::OnFPSEditChange);
    connect(m_ui->m_renderList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CSequenceBatchRenderDialog::OnRenderItemSelChange);
    connect(m_ui->m_resolutionCombo, activated, this, &CSequenceBatchRenderDialog::OnResolutionSelected);
    connect(m_ui->m_buffersToCaptureCombo, activated, this, &CSequenceBatchRenderDialog::OnBuffersSelected);
    connect(m_ui->m_startFrame, editingFinished, this, &CSequenceBatchRenderDialog::OnStartFrameChange);
    connect(m_ui->m_endFrame, editingFinished, this, &CSequenceBatchRenderDialog::OnEndFrameChange);

    const float bigEnoughNumber = 1000000.0f;
    m_ui->m_startFrame->setRange(0.0f, bigEnoughNumber);

    m_ui->m_endFrame->setRange(0.0f, bigEnoughNumber);

    // Fill the sequence combo box.
    bool activeSequenceWasSet = false;
    for (int k = 0; k < GetIEditor()->GetMovieSystem()->GetNumSequences(); ++k)
    {
        IAnimSequence* pSequence = GetIEditor()->GetMovieSystem()->GetSequence(k);
        m_ui->m_sequenceCombo->addItem(pSequence->GetName());
        if (pSequence->IsActivated())
        {
            m_ui->m_sequenceCombo->setCurrentIndex(k);
            activeSequenceWasSet = true;
        }
    }
    if (!activeSequenceWasSet)
    {
        m_ui->m_sequenceCombo->setCurrentIndex(0);
    }

    m_ui->m_fpsCombo->setEditable(true);

    // Fill the shot combos and the default frame range.
    OnSequenceSelected();

    // Fill the resolution combo box.
    for (int i = 0; i < arraysize(resolutions); ++i)
    {
        if (resolutions[i][0] == g_useActiveViewportResolution  && resolutions[i][1] == g_useActiveViewportResolution)
        {
            m_ui->m_resolutionCombo->addItem(tr("Active View Resolution"));
            stashActiveViewportResolution();    // render dialog is modal, so we can stash the viewport res on init
        }
        else
        {
            m_ui->m_resolutionCombo->addItem(tr("%1 x %2").arg(resolutions[i][0]).arg(resolutions[i][1]));
        }
    }
    m_ui->m_resolutionCombo->addItem(tr("Custom..."));
    m_ui->m_resolutionCombo->setCurrentIndex(0);

    // Fill the FPS combo box.
    for (int i = 0; i < arraysize(fps); ++i)
    {
        m_ui->m_fpsCombo->addItem(fps[i].fpsDesc);
    }
    m_ui->m_fpsCombo->setCurrentIndex(0);

    // Fill the image format combo box.
    for (int i = 0; i < arraysize(imageFormats); ++i)
    {
        m_ui->m_imageFormatCombo->addItem(imageFormats[i]);
    }
    m_ui->m_imageFormatCombo->setCurrentIndex(ICaptureKey::Jpg);

    // Fill the buffers-to-capture combo box.
    for (int i = 0; i < arraysize(buffersToCapture); ++i)
    {
        m_ui->m_buffersToCaptureCombo->addItem(buffersToCapture[i]);
    }
    m_ui->m_buffersToCaptureCombo->setCurrentIndex(0);

    m_ui->BATCH_RENDER_FILE_PREFIX->setText("Frame");

    m_ui->m_progressStatusMsg->setText("Not running");

    m_ui->BATCH_RENDER_REMOVE_SEQ->setEnabled(false);
    m_ui->m_updateBtn->setEnabled(false);
    m_ui->m_pGoBtn->setEnabled(false);
    m_ui->m_pGoBtn->setIcon(QPixmap(":/Trackview/clapperboard_ready.png"));

    m_ui->m_progressBar->setRange(0, 100);

    m_ui->BATCH_RENDER_FRAME_IN_FPS->setText(tr("In %1 FPS").arg(static_cast<int>(m_fpsForTimeToFrameConversion)));

    m_bFFMPEGCommandAvailable = GetIEditor()->GetICommandManager()->IsRegistered("plugin", "ffmpeg_encode");
    m_ffmpegPluginStatusMsg = m_bFFMPEGCommandAvailable ?
        QString("") :
        tr("FFMPEG plug-in isn't found(creating a video isn't supported).");
    m_ui->BATCH_RENDER_PRESS_ESC_TO_CANCEL->setText(m_ffmpegPluginStatusMsg);

    // Load previously saved options, if any.
    QString defaultPresetPath = Path::GetUserSandboxFolder();
    defaultPresetPath += defaultPresetFilename;
    if (CFileUtil::FileExists(defaultPresetPath))
    {
        LoadOutputOptions(defaultPresetPath);
    }
}

void CSequenceBatchRenderDialog::OnRenderItemSelChange()
{
    /// Enable/disable the 'remove'/'update' button properly.
    bool bNoSelection = !m_ui->m_renderList->selectionModel()->hasSelection();
    m_ui->BATCH_RENDER_REMOVE_SEQ->setEnabled(bNoSelection ? false : true);
    m_ui->m_updateBtn->setEnabled(bNoSelection ? false : true);

    if (bNoSelection)
    {
        return;
    }

    /// Apply the settings of the selected one to the dialog.
    const SRenderItem& item = m_renderItems[m_ui->m_renderList->currentIndex().row()];
    // sequence
    for (int i = 0; i < m_ui->m_sequenceCombo->count(); ++i)
    {
        const QString sequenceName = m_ui->m_sequenceCombo->itemText(i);
        if (sequenceName == item.pSequence->GetName())
        {
            m_ui->m_sequenceCombo->setCurrentIndex(i);
            OnSequenceSelected();
            break;
        }
    }
    // director
    for (int i = 0; i < m_ui->m_shotCombo->count(); ++i)
    {
        const QString directorName = m_ui->m_shotCombo->itemText(i);
        if (directorName == item.pDirectorNode->GetName())
        {
            m_ui->m_shotCombo->setCurrentIndex(i);
            break;
        }
    }
    // frame range
    m_ui->m_startFrame->setValue(item.frameRange.start * m_fpsForTimeToFrameConversion);
    m_ui->m_endFrame->setValue(item.frameRange.end * m_fpsForTimeToFrameConversion);
    // folder
    m_ui->m_destinationEdit->setText(item.folder);
    // fps
    bool bFound = false;
    for (int i = 0; i < arraysize(fps); ++i)
    {
        if (item.fps == fps[i].fps)
        {
            m_ui->m_fpsCombo->setCurrentIndex(i);
            bFound = true;
            break;
        }
    }
    if (bFound == false)
    {
        m_customFPS = item.fps;
        m_ui->m_fpsCombo->setCurrentText(QString::number(item.fps));
    }
    // capture buffer type
    m_ui->m_buffersToCaptureCombo->setCurrentIndex(item.bufferIndex);
    // prefix
    m_ui->BATCH_RENDER_FILE_PREFIX->setText(item.prefix);
    // format
    m_ui->m_imageFormatCombo->setCurrentIndex(item.formatIndex);
    OnBuffersSelected();

    m_ui->m_disableDebugInfoCheckBox->setChecked(item.disableDebugInfo);

    // create_video
    m_ui->m_createVideoCheckBox->setChecked(item.bCreateVideo);
    // resolution
    bFound = false;
    for (int i = 0; i < arraysize(resolutions); ++i)
    {
        if (item.resW == resolutions[i][0] && item.resH == resolutions[i][1])
        {
            m_ui->m_resolutionCombo->setCurrentIndex(i);
            bFound = true;
            break;
        }
    }
    if (bFound == false)
    {
        int indexOfCustomRes = arraysize(resolutions);
        const QString resText = QString::fromLatin1(customResFormat).arg(item.resW).arg(item.resH);
        m_customResW = item.resW;
        m_customResH = item.resH;
        m_ui->m_resolutionCombo->removeItem(indexOfCustomRes);
        m_ui->m_resolutionCombo->addItem(resText);
        m_ui->m_resolutionCombo->setCurrentIndex(indexOfCustomRes);
    }
    // cvars
    QString cvarsText;
    for (size_t i = 0; i < item.cvars.size(); ++i)
    {
        cvarsText += item.cvars[i];
        cvarsText += "\r\n";
    }
    m_ui->m_cvarsEdit->setPlainText(cvarsText);

    m_ui->m_updateBtn->setEnabled(false);
}

void CSequenceBatchRenderDialog::OnAddRenderItem()
{
    // If there is no director node, it cannot be added.
    if (m_ui->m_shotCombo->count() == 0)
    {
        QMessageBox::critical(this, tr("Cannot add"), tr("No director available!"));
        return;
    }

    /// Set up a new render item.
    SRenderItem item;
    if (SetUpNewRenderItem(item) == false)
    {
        return;
    }

    /// Check a duplication before adding.
    for (size_t i = 0; i < m_renderItems.size(); ++i)
    {
        if (m_renderItems[i] == item)
        {
            QMessageBox::critical(this, tr("Cannot add"), tr("The same item already exists"));
            return;
        }
    }

    AddItem(item);
}

void CSequenceBatchRenderDialog::OnRemoveRenderItem()
{
    int index = m_ui->m_renderList->currentIndex().row();
    assert(index != CB_ERR);
    m_ui->m_renderList->model()->removeRow(index);
    m_renderItems.erase(m_renderItems.begin() + index);

    if (m_renderItems.empty())
    {
        m_ui->BATCH_RENDER_REMOVE_SEQ->setEnabled(false);
        m_ui->m_updateBtn->setEnabled(false);
        m_ui->m_pGoBtn->setEnabled(false);
    }
    else
    {
        m_ui->m_renderList->setCurrentIndex(m_ui->m_renderList->model()->index(0, 0));
        OnRenderItemSelChange();
    }
}

void CSequenceBatchRenderDialog::OnClearRenderItems()
{
    m_ui->m_renderList->model()->removeRows(0, m_ui->m_renderList->model()->rowCount());
    m_renderItems.clear();

    m_ui->BATCH_RENDER_REMOVE_SEQ->setEnabled(false);
    m_ui->m_updateBtn->setEnabled(false);
    m_ui->m_pGoBtn->setEnabled(false);
}

void CSequenceBatchRenderDialog::OnUpdateRenderItem()
{
    int index = m_ui->m_renderList->currentIndex().row();
    assert(index != -1);

    /// Set up a new render item.
    SRenderItem item;
    SetUpNewRenderItem(item);

    /// Check a duplication before updating.
    for (size_t i = 0; i < m_renderItems.size(); ++i)
    {
        if (m_renderItems[i] == item)
        {
            QMessageBox::critical(this, tr("Cannot update"), tr("The same item already exists!"));
            return;
        }
    }

    /// Update the item.
    m_renderItems[index] = item;

    /// Update the list box, too.
    m_ui->m_renderList->model()->setData(m_ui->m_renderList->model()->index(index, 0), GetCaptureItemString(item));

    m_ui->m_updateBtn->setEnabled(false);
}

void CSequenceBatchRenderDialog::OnLoadPreset()
{
    QString loadPath;
    if (CFileUtil::SelectFile("Preset Files (*.preset)", Path::GetUserSandboxFolder(), loadPath))
    {
        if (LoadOutputOptions(loadPath) == false)
        {
            QMessageBox::critical(this, tr("Cannot load"), tr("The file version is different!"));
        }
    }
}

void CSequenceBatchRenderDialog::OnSavePreset()
{
    QString savePath;
    if (CFileUtil::SelectSaveFile("Preset Files (*.preset)", "preset", Path::GetUserSandboxFolder(), savePath))
    {
        SaveOutputOptions(savePath);
    }
}

void CSequenceBatchRenderDialog::stashActiveViewportResolution()
{   
    // stash active resolution in global vars
    activeViewportWidth = resolutions[0][0];
    activeViewportHeight = resolutions[0][1];
    CViewport* activeViewport = GetIEditor()->GetActiveView();
    if (activeViewport)
    {
        activeViewport->GetDimensions(&activeViewportWidth, &activeViewportHeight);
    }  
}

void CSequenceBatchRenderDialog::OnGo()
{
    if (m_renderContext.IsInRendering())
    {
        OnCancelRender();
    }
    else
    {
        /// Start a new batch.
        m_ui->m_pGoBtn->setText("Cancel");
        m_ui->m_pGoBtn->setIcon(QPixmap(":/Trackview/clapperboard_cancel.png"));
        // Inform the movie system that it soon will be in a batch-rendering mode.
        GetIEditor()->GetMovieSystem()->EnableBatchRenderMode(true);

        // Initialize the context.
        InitializeContext();

        // Trigger the first item.
        OnMovieEvent(IMovieListener::eMovieEvent_Stopped, NULL);
    }
}

void CSequenceBatchRenderDialog::OnMovieEvent(IMovieListener::EMovieEvent event, IAnimSequence* pSequence)
{
    if (event == IMovieListener::eMovieEvent_Stopped
        || event == IMovieListener::eMovieEvent_Aborted)
    {
        /// Finalize the current one, if any.
        if (pSequence)
        {
            EndCaptureItem(pSequence);

            bool bDone = m_renderContext.currentItemIndex == m_renderItems.size() - 1;
            bool bCancelled = event == IMovieListener::eMovieEvent_Aborted;
            if (bDone || bCancelled)
            {
                // Display the final progress message.
                if (bCancelled)
                {
                    m_ui->m_progressBar->setValue(0);
                    m_ui->m_progressStatusMsg->setText(tr("Rendering cancelled"));
                }
                else
                {
                    m_ui->m_progressBar->setValue(100);
                    m_ui->m_progressStatusMsg->setText(tr("Rendering finished"));
                }
                /// End the batch.
                m_ui->m_pGoBtn->setText(tr("Start"));
                m_ui->m_pGoBtn->setIcon(QPixmap(":/Trackview/clapperboard_ready.png"));
                GetIEditor()->GetMovieSystem()->EnableBatchRenderMode(false);
                m_renderContext.currentItemIndex = -1;
                m_ui->BATCH_RENDER_PRESS_ESC_TO_CANCEL->setText(m_ffmpegPluginStatusMsg);
                return;
            }

            /// Update the context.
            m_renderContext.spentTime += m_renderContext.captureOptions.duration;
            ++m_renderContext.currentItemIndex;
        }

        /// Trigger the next item.
        StartCaptureItem();
    }
}

void CSequenceBatchRenderDialog::OnDone()
{
    if (m_renderContext.IsInRendering())
    {
        if (m_renderContext.bWarmingUpAfterResChange
            || m_renderContext.bFFMPEGProcessing) // No cancellation in these two phases
        {
            return;
        }
        /// Cancel the batch.
        GetIEditor()->GetMovieSystem()->AbortSequence(m_renderItems[m_renderContext.currentItemIndex].pSequence);
    }
    else
    {
        // Save options when closed.
        QString defaultPresetPath = Path::GetUserSandboxFolder();
        defaultPresetPath += defaultPresetFilename;
        SaveOutputOptions(defaultPresetPath);

        reject();
    }
}

void CSequenceBatchRenderDialog::OnSequenceSelected()
{
    // Get the selected sequence.
    const QString seqName = m_ui->m_sequenceCombo->currentText();
    IAnimSequence* pSequence = GetIEditor()->GetMovieSystem()->FindSequence(seqName.toLatin1().data());

    // Adjust the frame range.
    float sFrame = pSequence->GetTimeRange().start * m_fpsForTimeToFrameConversion;
    float eFrame = pSequence->GetTimeRange().end * m_fpsForTimeToFrameConversion;
    m_ui->m_startFrame->setRange(0.0f, eFrame);
    m_ui->m_endFrame->setRange(0.0f, eFrame);

    // Set the default start/end frames properly.
    m_ui->m_startFrame->setValue(sFrame);
    m_ui->m_endFrame->setValue(eFrame);

    m_ui->m_shotCombo->clear();
    // Fill the shot combo box with the names of director nodes.
    for (int i = 0; i < pSequence->GetNodeCount(); ++i)
    {
        if (pSequence->GetNode(i)->GetType() == eAnimNodeType_Director)
        {
            m_ui->m_shotCombo->addItem(pSequence->GetNode(i)->GetName());
        }
    }
    m_ui->m_shotCombo->setCurrentIndex(0);
}

void CSequenceBatchRenderDialog::OnFPSEditChange()
{
    const QString fpsText = m_ui->m_fpsCombo->currentText();
    bool ok;
    const int fps = fpsText.toInt(&ok);
    bool bInvalidInput = !ok || fps <= 0;

    if (bInvalidInput)
    {
        m_ui->m_fpsCombo->setCurrentIndex(0);
    }
    else
    {
        m_customFPS = fps;
    }
}

void CSequenceBatchRenderDialog::OnResolutionSelected()
{
    int indexOfCustomRes = arraysize(resolutions);
    if (m_ui->m_resolutionCombo->currentIndex() == indexOfCustomRes)
    {
        int defaultW;
        int defaultH;
        const QString currentCustomResText = m_ui->m_resolutionCombo->currentText();
        GetResolutionFromCustomResText(currentCustomResText.toStdString().c_str(), defaultW, defaultH);
        
        CCustomResolutionDlg resDlg(defaultW, defaultH, this);
        if (resDlg.exec() == QDialog::Accepted)
        {
            const int maxRes = GetIEditor()->GetRenderer()->GetMaxSquareRasterDimension();
            m_customResW = min(resDlg.GetWidth(), maxRes);
            m_customResH = min(resDlg.GetHeight(), maxRes);
            const QString resText = QString(customResFormat).arg(m_customResW).arg(m_customResH);
            m_ui->m_resolutionCombo->setItemText(indexOfCustomRes, resText);
            m_ui->m_resolutionCombo->setCurrentIndex(indexOfCustomRes);
        }
        else
        {
            m_ui->m_resolutionCombo->setCurrentIndex(0);
        }
    }
}

void CSequenceBatchRenderDialog::SaveOutputOptions(const QString& pathname) const
{
    XmlNodeRef batchRenderOptionsNode = XmlHelpers::CreateXmlNode("batchrenderoptions");
    batchRenderOptionsNode->setAttr("version", kBatchRenderFileVersion);

    // Resolution
    XmlNodeRef resolutionNode = batchRenderOptionsNode->newChild("resolution");
    resolutionNode->setAttr("cursel", m_ui->m_resolutionCombo->currentIndex());
    if (m_ui->m_resolutionCombo->currentIndex() == arraysize(resolutions))
    {
        const QString resText = m_ui->m_resolutionCombo->currentText();
        resolutionNode->setContent(resText.toLatin1().data());
    }

    // FPS
    XmlNodeRef fpsNode = batchRenderOptionsNode->newChild("fps");
    fpsNode->setAttr("cursel", m_ui->m_fpsCombo->currentIndex());
    const QString fpsText = m_ui->m_fpsCombo->currentText();
    if (m_ui->m_fpsCombo->currentIndex() == -1 || m_ui->m_fpsCombo->findText(fpsText) == -1)
    {
        fpsNode->setContent(fpsText.toLatin1().data());
    }

    // Capture options (format, buffer, prefix, create_video)
    XmlNodeRef imageNode = batchRenderOptionsNode->newChild("image");
    imageNode->setAttr("format", m_ui->m_imageFormatCombo->currentIndex() % arraysize(imageFormats));
    imageNode->setAttr("bufferstocapture", m_ui->m_buffersToCaptureCombo->currentIndex());
    const QString prefix = m_ui->BATCH_RENDER_FILE_PREFIX->text();
    imageNode->setAttr("prefix", prefix.toLatin1().data());
    bool disableDebugInfo = m_ui->m_disableDebugInfoCheckBox->isChecked();
    imageNode->setAttr("disabledebuginfo", disableDebugInfo);
    bool bCreateVideoOn = m_ui->m_createVideoCheckBox->isChecked();
    imageNode->setAttr("createvideo", bCreateVideoOn);

    // Custom configs
    XmlNodeRef cvarsNode = batchRenderOptionsNode->newChild("cvars");
    const QStringList lines = m_ui->m_cvarsEdit->toPlainText().split(QStringLiteral("\n"));
    for (const QString& line : lines)
    {
        cvarsNode->newChild("cvar")->setContent(line.toLatin1().data());
    }

    // Destination
    XmlNodeRef destinationNode = batchRenderOptionsNode->newChild("destination");
    const QString destinationText = m_ui->m_destinationEdit->text();
    destinationNode->setContent(destinationText.toLatin1().data());

    XmlHelpers::SaveXmlNode(GetIEditor()->GetFileUtil(), batchRenderOptionsNode, pathname.toStdString().c_str());
}

bool CSequenceBatchRenderDialog::GetResolutionFromCustomResText(const char* customResText, int& retCustomWidth, int& retCustomHeight) const
{
    // initialize to first resolution preset as default if the sscanf below doesn't scan values successfully
    retCustomWidth = resolutions[0][0];
    retCustomHeight = resolutions[0][1];

    bool    scanSuccess   = false;
    int     scannedWidth  = retCustomWidth;      // initialize with default fall-back values - they'll be overwritten in the case of a succesful sscanf below.
    int     scannedHeight = retCustomHeight;

    QString strFormat = QString::fromLatin1(customResFormat).replace(QRegularExpression(QStringLiteral("%\\d")), QStringLiteral("%d"));
    scanSuccess = (sscanf(customResText, strFormat.toStdString().c_str(), &scannedWidth, &scannedHeight) == 2);
    if (scanSuccess)
    {
        retCustomWidth = scannedWidth;
        retCustomHeight = scannedHeight;
    }
    return scanSuccess;
}

bool CSequenceBatchRenderDialog::LoadOutputOptions(const QString& pathname)
{
    XmlNodeRef batchRenderOptionsNode = XmlHelpers::LoadXmlFromFile(pathname.toStdString().c_str());
    if (batchRenderOptionsNode == NULL)
    {
        return true;
    }
    int version = 0;
    batchRenderOptionsNode->getAttr("version", version);
    if (version != kBatchRenderFileVersion)
    {
        return false;
    }

    // Resolution
    XmlNodeRef resolutionNode = batchRenderOptionsNode->findChild("resolution");
    if (resolutionNode)
    {
        int curSel = CB_ERR;
        resolutionNode->getAttr("cursel", curSel);
        if (curSel == arraysize(resolutions))
        {
            const QString customResText = resolutionNode->getContent();
            m_ui->m_resolutionCombo->setItemText(curSel, customResText);
            
            GetResolutionFromCustomResText(customResText.toStdString().c_str(), m_customResW, m_customResH);
        }
        m_ui->m_resolutionCombo->setCurrentIndex(curSel);
    }

    // FPS
    XmlNodeRef fpsNode = batchRenderOptionsNode->findChild("fps");
    if (fpsNode)
    {
        int curSel = -1;
        fpsNode->getAttr("cursel", curSel);
        if (curSel == -1)
        {
            m_ui->m_fpsCombo->setCurrentIndex(-1);
            m_ui->m_fpsCombo->setCurrentText(fpsNode->getContent());
            m_customFPS = QString::fromLatin1(fpsNode->getContent()).toInt();
        }
        else
        {
            m_ui->m_fpsCombo->setCurrentIndex(curSel);
        }
    }

    // Capture options (format, buffer, prefix, create_video)
    XmlNodeRef imageNode = batchRenderOptionsNode->findChild("image");
    if (imageNode)
    {
        int curSel = CB_ERR;
        imageNode->getAttr("format", curSel);
        m_ui->m_imageFormatCombo->setCurrentIndex(curSel);
        curSel = CB_ERR;
        imageNode->getAttr("bufferstocapture", curSel);
        m_ui->m_buffersToCaptureCombo->setCurrentIndex(curSel);
        OnBuffersSelected();
        m_ui->BATCH_RENDER_FILE_PREFIX->setText(imageNode->getAttr("prefix"));
        bool disableDebugInfo = false;
        imageNode->getAttr("disabledebuginfo", disableDebugInfo);
        m_ui->m_disableDebugInfoCheckBox->setChecked(disableDebugInfo);
        bool bCreateVideoOn = false;
        imageNode->getAttr("createvideo", bCreateVideoOn);
        m_ui->m_createVideoCheckBox->setChecked(bCreateVideoOn);
    }

    // Custom configs
    XmlNodeRef cvarsNode = batchRenderOptionsNode->findChild("cvars");
    if (cvarsNode)
    {
        QString cvarsText;
        for (int i = 0; i < cvarsNode->getChildCount(); ++i)
        {
            cvarsText += cvarsNode->getChild(i)->getContent();
            if (i < cvarsNode->getChildCount() - 1)
            {
                cvarsText += QStringLiteral("\r\n");
            }
        }
        m_ui->m_cvarsEdit->setPlainText(cvarsText);
    }

    // Destination
    XmlNodeRef destinationNode = batchRenderOptionsNode->findChild("destination");
    if (destinationNode)
    {
        m_ui->m_destinationEdit->setText(destinationNode->getContent());
    }

    return true;
}

void CSequenceBatchRenderDialog::OnStartFrameChange()
{
    if (m_ui->m_startFrame->value() >= m_ui->m_endFrame->value())
    {
        m_ui->m_endFrame->setValue(m_ui->m_startFrame->value() + 1);
    }
}

void CSequenceBatchRenderDialog::OnEndFrameChange()
{
    if (m_ui->m_startFrame->value() >= m_ui->m_endFrame->value())
    {
        m_ui->m_startFrame->setValue(m_ui->m_endFrame->value() - 1);
    }
}

void CSequenceBatchRenderDialog::InitializeContext()
{
    m_renderContext.currentItemIndex = 0;
    m_renderContext.spentTime = 0;
    m_renderContext.expectedTotalTime = 0;
    for (size_t i = 0; i < m_renderItems.size(); ++i)
    {
        Range rng = m_renderItems[i].frameRange;
        m_renderContext.expectedTotalTime += rng.end - rng.start;
    }
    m_renderContext.captureOptions.once = false;

    m_ui->BATCH_RENDER_PRESS_ESC_TO_CANCEL->setText(tr("Press ESC to cancel"));
}

void CSequenceBatchRenderDialog::StartCaptureItem()
{
    SRenderItem renderItem = m_renderItems[m_renderContext.currentItemIndex];
    IAnimSequence* pNextSequence = renderItem.pSequence;
    /// Initialize the next one for the batch rendering.
    // Set the active shot.
    m_renderContext.pActiveDirectorBU = pNextSequence->GetActiveDirector();
    pNextSequence->SetActiveDirector(renderItem.pDirectorNode);

    // Back up flags and range of the sequence.
    m_renderContext.flagBU = pNextSequence->GetFlags();
    m_renderContext.rangeBU = pNextSequence->GetTimeRange();

    // Change flags and range of the sequence so that it automatically starts
    // once the game mode kicks in with the specified range.
    pNextSequence->SetFlags(m_renderContext.flagBU | IAnimSequence::eSeqFlags_PlayOnReset);
    // A margin value to capture the precise number of frames
    const float someMargin = 2.5f / 30.0f;
    Range newRange = renderItem.frameRange;
    newRange.end += someMargin;
    pNextSequence->SetTimeRange(newRange);

    // Set up the custom config cvars for this item.
    for (size_t i = 0; i < renderItem.cvars.size(); ++i)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString(renderItem.cvars[i].toLatin1().data());
    }

    // Set specific capture options for this item.
    m_renderContext.captureOptions.timeStep = 1.0f / renderItem.fps;
    m_renderContext.captureOptions.captureBufferIndex = renderItem.bufferIndex;
    cry_strcpy(m_renderContext.captureOptions.prefix, renderItem.prefix.toLatin1().data());
    switch (renderItem.formatIndex)
    {
    case ICaptureKey::Jpg:
        m_renderContext.captureOptions.FormatJPG();
        break;
    case ICaptureKey::Tga:
        m_renderContext.captureOptions.FormatTGA();
        break;
    case ICaptureKey::Tif:
        m_renderContext.captureOptions.FormatTIF();
        break;
    default:
        // fall back to tga, the most general of the formats
        gEnv->pLog->LogWarning("Unhandled file format type detected in CSequenceBatchRenderDialog::StartCaptureItem(), using tga");
        m_renderContext.captureOptions.FormatTGA();
        break;
    }

    Range rng = pNextSequence->GetTimeRange();
    m_renderContext.captureOptions.duration = rng.end - rng.start;
    QString folder = renderItem.folder;
    QString itemText = m_ui->m_renderList->model()->index(m_renderContext.currentItemIndex, 0).data().toString();
    itemText.replace('/', '-'); // A full sequence name can have slash characters which aren't suitable for a file name.
    folder += "/";
    folder += itemText;
    QString finalFolder = folder;
    int i = 2;
    while (QFileInfo::exists(finalFolder))
    {
        finalFolder = folder;
        const QString suffix = QString::fromLatin1("_v%1").arg(i);
        finalFolder += suffix;
        ++i;
    }
    cry_strcpy(m_renderContext.captureOptions.folder, finalFolder.toLatin1().data());

    /// Change the resolution.
    const int renderWidth = getResWidth(renderItem.resW);
    const int renderHeight = getResHeight(renderItem.resH);
    ICVar* pCVarCustomResWidth = gEnv->pConsole->GetCVar("r_CustomResWidth");
    ICVar* pCVarCustomResHeight = gEnv->pConsole->GetCVar("r_CustomResHeight");
    if (pCVarCustomResWidth && pCVarCustomResHeight)
    {
        // If available, use the custom resolution cvars.
        m_renderContext.cvarCustomResWidthBU = pCVarCustomResWidth->GetIVal();
        m_renderContext.cvarCustomResHeightBU = pCVarCustomResHeight->GetIVal();
        pCVarCustomResWidth->Set(renderWidth);
        pCVarCustomResHeight->Set(renderHeight);
    }
    else
    {
        // Otherwise, try to adjust the viewport resolution accordingly.
        GetIEditor()->ExecuteCommand("general.resize_viewport %d %d", renderWidth, renderHeight);
    }

    // turn off debug info if requested
    ICVar* cvarDebugInfo = gEnv->pConsole->GetCVar("r_DisplayInfo");
    if (cvarDebugInfo)
    {
        // cache the current value to restore during EndCaptureItem()
        m_renderContext.cvarDisplayInfoBU = cvarDebugInfo->GetIVal();
        if (renderItem.disableDebugInfo && cvarDebugInfo->GetIVal())
        {
            const int DISPLAY_INFO_OFF = 0;         
            cvarDebugInfo->Set(DISPLAY_INFO_OFF);
        }
    }

    // The capturing doesn't actually start here. It just flags the warming-up and
    // once it's done, then the capturing really begins.
    // The warming-up is necessary to settle down some post-fxs after the resolution change.
    m_renderContext.bWarmingUpAfterResChange = true;
    m_renderContext.timeWarmingUpStarted = GetTickCount();
    m_renderTimer.start();
}

void CSequenceBatchRenderDialog::ReallyStartCaptureItem()
{
    SRenderItem renderItem = m_renderItems[m_renderContext.currentItemIndex];
    IAnimSequence* pNextSequence = renderItem.pSequence;

    GetIEditor()->GetMovieSystem()->StartCapture(m_renderContext.captureOptions);
    GetIEditor()->SetInGameMode(true);
    GetIEditor()->GetGameEngine()->Update();    // Update is needed because SetInGameMode() queues game mode, Update() executes it.
    GetIEditor()->GetMovieSystem()->AddMovieListener(pNextSequence, this);
}

void CSequenceBatchRenderDialog::EndCaptureItem(IAnimSequence* pSequence)
{
    GetIEditor()->GetMovieSystem()->RemoveMovieListener(pSequence, this);
    GetIEditor()->SetInGameMode(false);
    GetIEditor()->GetGameEngine()->Update();        // Update is needed because SetInGameMode() queues game mode, Update() executes it.
    GetIEditor()->GetMovieSystem()->EndCapture();
    GetIEditor()->GetMovieSystem()->ControlCapture();

    ICVar* pCVarCustomResWidth = gEnv->pConsole->GetCVar("r_CustomResWidth");
    ICVar* pCVarCustomResHeight = gEnv->pConsole->GetCVar("r_CustomResHeight");
    if (pCVarCustomResWidth && pCVarCustomResHeight)
    {
        // Restore the custom resolution cvars.
        pCVarCustomResWidth->Set(m_renderContext.cvarCustomResWidthBU);
        pCVarCustomResHeight->Set(m_renderContext.cvarCustomResHeightBU);
    }

    // Restore display debug info
    ICVar* cvarDebugInfo = gEnv->pConsole->GetCVar("r_DisplayInfo");
    if (cvarDebugInfo)
    {
        cvarDebugInfo->Set(m_renderContext.cvarDisplayInfoBU);
    }
    
    // Restore flags, range and the active director of the sequence.
    pSequence->SetFlags(m_renderContext.flagBU);
    pSequence->SetTimeRange(m_renderContext.rangeBU);
    pSequence->SetActiveDirector(m_renderContext.pActiveDirectorBU);

    SRenderItem renderItem = m_renderItems[m_renderContext.currentItemIndex];
    if (m_bFFMPEGCommandAvailable
        && renderItem.bCreateVideo)
    {
        // Create a video using the ffmpeg plug-in from captured images.
        m_renderContext.bFFMPEGProcessing = true;
        QString outputFolder = m_renderContext.captureOptions.folder;
        auto future = QtConcurrent::run(
            [&renderItem, &outputFolder]
        {
            QString inputFile, outputFile = outputFolder;
            outputFile += "\\";
            outputFile += renderItem.prefix;
            inputFile   = outputFile;
            outputFile += ".mp4";
            inputFile  += "%06d.";
            inputFile  += imageFormats[renderItem.formatIndex];
            GetIEditor()->ExecuteCommand(
                "plugin.ffmpeg_encode '%s' '%s' '%s' %d %d '-vf crop=%d:%d:0:0'",
                inputFile.toLocal8Bit().data(), outputFile.toLocal8Bit().data(), "mpeg4",
                10240, renderItem.fps, getResWidth(renderItem.resW), getResHeight(renderItem.resH));
        }
            );
        do
        {
            OnKickIdle();
        }
        while (future.isRunning());
        m_renderContext.bFFMPEGProcessing = false;
    }
}

void CSequenceBatchRenderDialog::OnKickIdleTimout()
{
    OnKickIdle();
    if (m_renderContext.IsInRendering())
    {
        m_renderTimer.start();
    }
}

void CSequenceBatchRenderDialog::OnKickIdle()
{
    static int count = 0;

    if (m_renderContext.IsInRendering())
    {
        if (m_renderContext.bWarmingUpAfterResChange)  /// A warming-up phase
        {
            const char* rotatingCursor[] =  { "|", "/", "-", "\\" };
            const QString msg = tr("Warming up %1").arg(rotatingCursor[(count++) % arraysize(rotatingCursor)]);
            m_ui->m_progressStatusMsg->setText(msg);
            GetIEditor()->GetGameEngine()->Update();
            GetIEditor()->Notify(eNotify_OnIdleUpdate);

            if (GetTickCount() > m_renderContext.timeWarmingUpStarted + kWarmingUpDuration)
            {
                // The warming-up done.
                m_renderContext.bWarmingUpAfterResChange = false;
                count = 0;
                ReallyStartCaptureItem();
            }
        }
        else if (m_renderContext.bFFMPEGProcessing)    /// A ffmpeg-processing phase
        {
            const char* rotatingCursor[] =  { "|", "/", "-", "\\" };
            const QString msg = tr("FFMPEG processing %1").arg(rotatingCursor[(count++) % arraysize(rotatingCursor)]);
            m_ui->m_progressStatusMsg->setText(msg);
            GetIEditor()->GetGameEngine()->Update();
            GetIEditor()->Notify(eNotify_OnIdleUpdate);
        }
        else                                          /// A capturing phase
        {
            // Progress bar
            IAnimSequence* pCurSeq = m_renderItems[m_renderContext.currentItemIndex].pSequence;
            Range rng = pCurSeq->GetTimeRange();
            float elapsedTime = GetIEditor()->GetMovieSystem()->GetPlayingTime(pCurSeq) - rng.start;
            int percentage
                = int(100.0f * (m_renderContext.spentTime + elapsedTime) / m_renderContext.expectedTotalTime);
            m_ui->m_progressBar->setValue(percentage);

            // Progress message
            const QString itemText = m_ui->m_renderList->model()->index(m_renderContext.currentItemIndex, 0).data().toString();
            const QString msg = tr("Rendering '%1'...(%2%)").arg(itemText).arg(static_cast<int>(100.0f * elapsedTime / (rng.end - rng.start)));
            m_ui->m_progressStatusMsg->setText(msg);

            GetIEditor()->GetGameEngine()->Update();
        }
    }
    else
    {
        bool bItemSelected = -1 != m_ui->m_renderList->currentIndex().row();
        if (bItemSelected)
        {
            // If any of settings changed, then enable the 'update button'.
            // Otherwise, disable it.
            SRenderItem item;
            bool bSettingChanged
                = SetUpNewRenderItem(item) && !(item == m_renderItems[m_ui->m_renderList->currentIndex().row()]);
            m_ui->m_updateBtn->setEnabled(bSettingChanged);
        }
    }

    qApp->processEvents();
}

void CSequenceBatchRenderDialog::OnCancelRender()
{
    // No cancellation in these two phases
    if (m_renderContext.bWarmingUpAfterResChange || m_renderContext.bFFMPEGProcessing)
    {
        return;
    }

    /// Cancel the batch.
    GetIEditor()->GetMovieSystem()->AbortSequence(m_renderItems[m_renderContext.currentItemIndex].pSequence);
}

void CSequenceBatchRenderDialog::OnLoadBatch()
{
    QString loadPath;
    if (CFileUtil::SelectFile("Render Batch Files (*.batch)",
            Path::GetUserSandboxFolder(), loadPath))
    {
        XmlNodeRef batchRenderListNode = XmlHelpers::LoadXmlFromFile(loadPath.toStdString().c_str());
        if (batchRenderListNode == NULL)
        {
            return;
        }
        int version = 0;
        batchRenderListNode->getAttr("version", version);
        if (version != kBatchRenderFileVersion)
        {
            QMessageBox::critical(this, tr("Cannot load"), tr("The file version is different!"));
            return;
        }

        OnClearRenderItems();

        for (int i = 0; i < batchRenderListNode->getChildCount(); ++i)
        {
            /// Get an item.
            SRenderItem item;
            XmlNodeRef itemNode = batchRenderListNode->getChild(i);

            // sequence
            const QString seqName = itemNode->getAttr("sequence");
            item.pSequence = GetIEditor()->GetMovieSystem()->FindSequence(seqName.toLatin1().data());
            if (item.pSequence == NULL)
            {
                QMessageBox::warning(this, tr("Sequence not found"), tr("A sequence of '%1' not found! This'll be skipped.").arg(seqName));
                continue;
            }

            // director node
            const QString directorName = itemNode->getAttr("director");
            for (int k = 0; k < item.pSequence->GetNodeCount(); ++k)
            {
                IAnimNode* pNode = item.pSequence->GetNode(k);
                if (pNode->GetType() == eAnimNodeType_Director && directorName == pNode->GetName())
                {
                    item.pDirectorNode = pNode;
                    break;
                }
            }
            if (item.pDirectorNode == NULL)
            {
                QMessageBox::warning(this, tr("Director node not found"), tr("A director node of '%1' not found in the sequence of '%2'! This'll be skipped.").arg(directorName).arg(seqName));
                continue;
            }

            // frame range
            itemNode->getAttr("startframe", item.frameRange.start);
            itemNode->getAttr("endframe", item.frameRange.end);

            // resolution
            itemNode->getAttr("width", item.resW);
            itemNode->getAttr("height", item.resH);

            // fps
            itemNode->getAttr("fps", item.fps);

            // format
            int intAttr;
            itemNode->getAttr("format", intAttr);
            item.formatIndex = (intAttr <= ICaptureKey::NumCaptureFileFormats) ? static_cast<ICaptureKey::CaptureFileFormat>(intAttr) : ICaptureKey::Jpg;

            // capture buffer type
            itemNode->getAttr("bufferstocapture", intAttr);
            item.bufferIndex = (intAttr <= ICaptureKey::NumCaptureBufferTypes) ? static_cast<ICaptureKey::CaptureBufferType>(intAttr) : ICaptureKey::Color;

            // prefix
            item.prefix = itemNode->getAttr("prefix");

            // create_video
            itemNode->getAttr("createvideo", item.bCreateVideo);

            // folder
            item.folder = itemNode->getAttr("folder");

            // cvars
            for (int k = 0; k < itemNode->getChildCount(); ++k)
            {
                const QString cvar = itemNode->getChild(k)->getContent();
                item.cvars.push_back(cvar);
            }

            AddItem(item);
        }
    }
}

void CSequenceBatchRenderDialog::OnSaveBatch()
{
    QString savePath;
    if (CFileUtil::SelectSaveFile("Render Batch Files (*.batch)", "batch",
            Path::GetUserSandboxFolder(), savePath))
    {
        XmlNodeRef batchRenderListNode = XmlHelpers::CreateXmlNode("batchrenderlist");
        batchRenderListNode->setAttr("version", kBatchRenderFileVersion);

        for (size_t i = 0; i < m_renderItems.size(); ++i)
        {
            const SRenderItem& item = m_renderItems[i];
            XmlNodeRef itemNode = batchRenderListNode->newChild("item");

            // sequence
            itemNode->setAttr("sequence", item.pSequence->GetName());

            // director node
            itemNode->setAttr("director", item.pDirectorNode->GetName());

            // frame range
            itemNode->setAttr("startframe", item.frameRange.start);
            itemNode->setAttr("endframe", item.frameRange.end);

            // resolution
            itemNode->setAttr("width", item.resW);
            itemNode->setAttr("height", item.resH);

            // fps
            itemNode->setAttr("fps", item.fps);

            // format
            itemNode->setAttr("format", item.formatIndex);

            // capture buffer type
            itemNode->setAttr("bufferstocapture", item.bufferIndex);

            // prefix
            itemNode->setAttr("prefix", item.prefix.toLatin1().data());

            // create_video
            itemNode->setAttr("createvideo", item.bCreateVideo);

            // folder
            itemNode->setAttr("folder", item.folder.toLatin1().data());

            // cvars
            for (size_t k = 0; k < item.cvars.size(); ++k)
            {
                itemNode->newChild("cvar")->setContent(item.cvars[k].toLatin1().data());
            }
        }

        XmlHelpers::SaveXmlNode(GetIEditor()->GetFileUtil(), batchRenderListNode, savePath.toStdString().c_str());
    }
}

bool CSequenceBatchRenderDialog::SetUpNewRenderItem(SRenderItem& item)
{
    const QString seqName = m_ui->m_sequenceCombo->currentText();
    const QString shotName = m_ui->m_shotCombo->currentText();
    // folder
    item.folder = m_ui->m_destinationEdit->text();
    if (item.folder.isEmpty())
    {
        QMessageBox::critical(this, tr("Cannot add"), tr("The output folder should be specified!"));
        return false;
    }
    // sequence
    item.pSequence = GetIEditor()->GetMovieSystem()->FindSequence(seqName.toLatin1().data());
    assert(item.pSequence);
    // director
    for (int i = 0; i < item.pSequence->GetNodeCount(); ++i)
    {
        IAnimNode* pNode = item.pSequence->GetNode(i);
        if (pNode->GetType() == eAnimNodeType_Director && shotName == pNode->GetName())
        {
            item.pDirectorNode = pNode;
            break;
        }
    }
    if (item.pDirectorNode == NULL)
    {
        return false;
    }
    // frame range
    item.frameRange = Range(m_ui->m_startFrame->value() / m_fpsForTimeToFrameConversion,
            m_ui->m_endFrame->value() / m_fpsForTimeToFrameConversion);
    // fps
    if (m_ui->m_fpsCombo->currentIndex() == -1 || m_ui->m_fpsCombo->currentText() != fps[m_ui->m_fpsCombo->currentIndex()].fpsDesc)
    {
        item.fps = m_customFPS;
    }
    else
    {
        item.fps = fps[m_ui->m_fpsCombo->currentIndex()].fps;
    }
    // capture buffer type
    item.bufferIndex = static_cast<ICaptureKey::CaptureBufferType>(m_ui->m_buffersToCaptureCombo->currentIndex());
    // prefix
    item.prefix = m_ui->BATCH_RENDER_FILE_PREFIX->text();
    // format
    item.formatIndex = static_cast<ICaptureKey::CaptureFileFormat>(m_ui->m_imageFormatCombo->currentIndex() % arraysize(imageFormats));
    // disable debug info
    item.disableDebugInfo = m_ui->m_disableDebugInfoCheckBox->isChecked();
    // create_video
    item.bCreateVideo = m_ui->m_createVideoCheckBox->isChecked();
    // resolution
    int curResSel = m_ui->m_resolutionCombo->currentIndex();
    if (curResSel < arraysize(resolutions))
    {
        item.resW = resolutions[curResSel][0];
        item.resH = resolutions[curResSel][1];
    }
    else
    {
        item.resW = m_customResW;
        item.resH = m_customResH;
    }
    // cvars
    const int kCVarNameMaxSize = 256;
    char buf[kCVarNameMaxSize];
    buf[kCVarNameMaxSize - 1] = 0;
    const QStringList lines = m_ui->m_cvarsEdit->toPlainText().split('\n');
    for (const QString& line : lines)
    {
        if (!line.isEmpty())
        {
            item.cvars.push_back(line);
        }
    }

    return true;
}

void CSequenceBatchRenderDialog::AddItem(const SRenderItem& item)
{
    // Add the item.
    m_renderItems.push_back(item);

    // Add it to the list box, too.
    m_renderListModel->setStringList(m_renderListModel->stringList() << GetCaptureItemString(item));

    m_ui->m_pGoBtn->setEnabled(true);
}

QString CSequenceBatchRenderDialog::GetCaptureItemString(const SRenderItem& item) const
{
    return QString::fromLatin1("%1_%2_%3-%4(%5x%6,%7,%8)%9").arg(item.pSequence->GetName())
               .arg(item.pDirectorNode->GetName())
               .arg(int(item.frameRange.start * m_fpsForTimeToFrameConversion))
               .arg(int(item.frameRange.end * m_fpsForTimeToFrameConversion))
               .arg(getResWidth(item.resW)).arg(getResHeight(item.resH)).arg(item.fps).arg(buffersToCapture[item.bufferIndex])
               .arg(item.bCreateVideo ? "[v]" : "");
}
void CSequenceBatchRenderDialog::OnBuffersSelected()
{
    int curSel = m_ui->m_buffersToCaptureCombo->currentIndex();
    const ICaptureKey::CaptureBufferType bufferType = (curSel >= ICaptureKey::NumCaptureBufferTypes ? ICaptureKey::Color : static_cast<ICaptureKey::CaptureBufferType>(curSel));

    switch (bufferType)
    {
    case ICaptureKey::Color:
        // allow any format for color buffer
        m_ui->m_imageFormatCombo->setEnabled(true);
        break;
    case ICaptureKey::ColorWithAlpha:
        // only tga supports alpha for now - set it and disable the ability to change it
        m_ui->m_imageFormatCombo->setCurrentIndex(ICaptureKey::Tga);
        m_ui->m_imageFormatCombo->setEnabled(false);
        break;
    default:
        gEnv->pLog->LogWarning("Unhandle capture buffer type used in CSequenceBatchRenderDialog::OnBuffersSelected()");
        break;
    }
}

