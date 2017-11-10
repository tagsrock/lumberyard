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

// Description : A dialog for customizing track colors


#include "StdAfx.h"
#include <algorithm>
#include "TVCustomizeTrackColorsDlg.h"
#include "TrackViewDialog.h"

#include <TrackView/ui_TVCustomizeTrackColorsDialog.h>

#include <QLabel>
#include <QMessageBox>

#include "QtUI/ColorButton.h"

#include <Plugins/EditorCommon/QtUtilWin.h>


#define TRACKCOLOR_ENTRY_PREFIX ("TrackColor")
#define TRACKCOLOR_FOR_OTHERS_ENTRY ("TrackColorForOthers")
#define TRACKCOLOR_FOR_DISABLED_ENTRY ("TrackColorForDisabled")
#define TRACKCOLOR_FOR_MUTED_ENTRY ("TrackColorForMuted")

struct STrackEntry
{
    CAnimParamType paramType;
    QString name;
    QColor defaultColor;
};

namespace
{
    const STrackEntry g_trackEntries[] = {
        // Color for tracks
        { eAnimParamType_FOV, "FOV", QColor(220, 220, 220) },
        { eAnimParamType_Position, "Pos", QColor(90, 150, 90) },
        { eAnimParamType_Rotation, "Rot", QColor(90, 150, 90) },
        { eAnimParamType_Scale, "Scale", QColor(90, 150, 90) },
        { eAnimParamType_Event, "Event", QColor(220, 220, 220) },
        { eAnimParamType_Visibility, "Visibility", QColor(220, 220, 220) },
        { eAnimParamType_Camera, "Camera", QColor(220, 220, 220) },
        { eAnimParamType_Sound, "Sound", QColor(220, 220, 220) },
        { eAnimParamType_Animation, "Animation", QColor(220, 220, 220) },
        { eAnimParamType_Sequence, "Sequence", QColor(220, 220, 220) },
        { eAnimParamType_Console, "Console", QColor(220, 220, 220) },
        { eAnimParamType_Music, "Music", QColor(220, 220, 220) },
        { eAnimParamType_LookAt, "LookAt", QColor(220, 220, 220) },
        { eAnimParamType_TrackEvent, "TrackEvent", QColor(220, 220, 220) },
        { eAnimParamType_ShakeMultiplier, "ShakeMult", QColor(90, 150, 90) },
        { eAnimParamType_TransformNoise, "Noise", QColor(90, 150, 90) },
        { eAnimParamType_TimeWarp, "Timewarp", QColor(220, 220, 220) },
        { eAnimParamType_FixedTimeStep, "FixedTimeStep", QColor(220, 220, 220) },
        { eAnimParamType_DepthOfField, "DepthOfField", QColor(90, 150, 90) },
        { eAnimParamType_CommentText, "CommentText", QColor(220, 220, 220) },
        { eAnimParamType_ScreenFader, "ScreenFader", QColor(220, 220, 220) },
        { eAnimParamType_LightDiffuse, "LightDiffuseColor", QColor(90, 150, 90) },
        { eAnimParamType_LightRadius, "LightRadius", QColor(220, 220, 220) },
        { eAnimParamType_LightDiffuseMult, "LightDiffuseMult", QColor(220, 220, 220) },
        { eAnimParamType_LightHDRDynamic, "LightHDRDynamic", QColor(220, 220, 220) },
        { eAnimParamType_LightSpecularMult, "LightSpecularMult", QColor(220, 220, 220) },
        { eAnimParamType_LightSpecPercentage, "LightSpecularPercent", QColor(220, 220, 220) },
        { eAnimParamType_FocusDistance, "FocusDistance", QColor(220, 220, 220) },
        { eAnimParamType_FocusRange, "FocusRange", QColor(220, 220, 220) },
        { eAnimParamType_BlurAmount, "BlurAmount", QColor(220, 220, 220) },
        { eAnimParamType_PositionX, "PosX", QColor(220, 220, 220) },
        { eAnimParamType_PositionY, "PosY", QColor(220, 220, 220) },
        { eAnimParamType_PositionZ, "PosZ", QColor(220, 220, 220) },
        { eAnimParamType_RotationX, "RotX", QColor(220, 220, 220) },
        { eAnimParamType_RotationY, "RotY", QColor(220, 220, 220) },
        { eAnimParamType_RotationZ, "RotZ", QColor(220, 220, 220) },
        { eAnimParamType_ScaleX, "ScaleX", QColor(220, 220, 220) },
        { eAnimParamType_ScaleY, "ScaleY", QColor(220, 220, 220) },
        { eAnimParamType_ScaleZ, "ScaleZ", QColor(220, 220, 220) },
        { eAnimParamType_ShakeAmpAMult, "ShakeMultAmpA", QColor(220, 220, 220) },
        { eAnimParamType_ShakeAmpBMult, "ShakeMultAmpB", QColor(220, 220, 220) },
        { eAnimParamType_ShakeFreqAMult, "ShakeMultFreqA", QColor(220, 220, 220) },
        { eAnimParamType_ShakeFreqBMult, "ShakeMultFreqB", QColor(220, 220, 220) },
        { eAnimParamType_ColorR, "ColorR", QColor(220, 220, 220) },
        { eAnimParamType_ColorG, "ColorG", QColor(220, 220, 220) },
        { eAnimParamType_ColorB, "ColorB", QColor(220, 220, 220) },
        { eAnimParamType_MaterialOpacity, "MaterialOpacity", QColor(220, 220, 220) },
        { eAnimParamType_MaterialSmoothness, "MaterialGlossiness", QColor(220, 220, 220) },
        { eAnimParamType_MaterialEmissive, "MaterialEmission", QColor(220, 220, 220) },
        { eAnimParamType_MaterialEmissiveIntensity, "MaterialEmissionIntensity", QColor(220, 220, 220) },
        { eAnimParamType_NearZ, "NearZ", QColor(220, 220, 220) },

        { eAnimParamType_User, "", QColor(0, 0, 0) }, // An empty string means a separator row.

        // Misc colors for special states of a track
        { eAnimParamType_User, "Others", QColor(220, 220, 220) },
        { eAnimParamType_User, "Disabled/Inactive", QColor(255, 224, 224) },
        { eAnimParamType_User, "Muted", QColor(255, 224, 224) },
    };

    const int kButtonsIdBase = 0x7fff;
    const int kMaxRows = 20;
    const int kColumnWidth = 300;
    const int kRowHeight = 24;

    const int kOthersEntryIndex = arraysize(g_trackEntries) - 3;
    const int kDisabledEntryIndex = arraysize(g_trackEntries) - 2;
    const int kMutedEntryIndex = arraysize(g_trackEntries) - 1;
}

std::map<CAnimParamType, QColor> CTVCustomizeTrackColorsDlg::s_trackColors;
QColor CTVCustomizeTrackColorsDlg::s_colorForDisabled;
QColor CTVCustomizeTrackColorsDlg::s_colorForMuted;
QColor CTVCustomizeTrackColorsDlg::s_colorForOthers;

CTVCustomizeTrackColorsDlg::CTVCustomizeTrackColorsDlg(QWidget* pParent)
    : QDialog(pParent)
    , m_aLabels(arraysize(g_trackEntries))
    , m_colorButtons(arraysize(g_trackEntries))
    , m_ui(new Ui::TVCustomizeTrackColorsDialog)
{
    OnInitDialog();
}

CTVCustomizeTrackColorsDlg::~CTVCustomizeTrackColorsDlg()
{
}

void CTVCustomizeTrackColorsDlg::OnInitDialog()
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &CTVCustomizeTrackColorsDlg::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &CTVCustomizeTrackColorsDlg::reject);
    connect(m_ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &CTVCustomizeTrackColorsDlg::OnApply);
    connect(m_ui->buttonResetAll, &QPushButton::clicked, this, &CTVCustomizeTrackColorsDlg::OnResetAll);
    connect(m_ui->buttonExport, &QPushButton::clicked, this, &CTVCustomizeTrackColorsDlg::OnExport);
    connect(m_ui->buttonImport, &QPushButton::clicked, this, &CTVCustomizeTrackColorsDlg::OnImport);


    QRect labelRect(QPoint(30, 30), QPoint(150, 50));
    QRect buttonRect(QPoint(180, 30), QPoint(280, 50));
    // Create a label and a color button for each track.
    int col = 0, i = 0;
    std::for_each(g_trackEntries, g_trackEntries + arraysize(g_trackEntries), [&](const STrackEntry& entry)
	{
		const QString labelText = entry.name;

		if(!labelText.isEmpty())
		{
			m_aLabels[i] = new QLabel(m_ui->frame);
			m_aLabels[i]->setGeometry(labelRect);
			m_aLabels[i]->setText(labelText);

                m_colorButtons[i] = new ColorButton(m_ui->frame);
			m_colorButtons[i]->setGeometry(buttonRect);

			if(entry.paramType.GetType() == eAnimParamType_User)
			{
				assert(kOthersEntryIndex <= i);
				if (i == kOthersEntryIndex)
                    {
					m_colorButtons[i]->SetColor(s_colorForOthers);
                    }
				else if(i == kDisabledEntryIndex)
                    {
					m_colorButtons[i]->SetColor(s_colorForDisabled);
                    }
				else if(i == kMutedEntryIndex)
                    {
					m_colorButtons[i]->SetColor(s_colorForMuted);
                    }
                }
			else
			{
				m_colorButtons[i]->SetColor(s_trackColors[entry.paramType]);
			}
		}

		if(i % kMaxRows == kMaxRows - 1)
		{
			++col;
			labelRect.moveTopLeft(QPoint(30+kColumnWidth*col, 30));
			buttonRect.moveTopLeft(QPoint(180+kColumnWidth*col, 30));
		}
		else
		{
			labelRect.translate(0, kRowHeight);
			buttonRect.translate(0, kRowHeight);
		}
		++i;
	});

    // Resize this dialog to fit the contents.
    const QSize size(60 + kColumnWidth * (col + 1), 100 + kMaxRows * kRowHeight);
    m_ui->frame->setFixedSize(size);
    setFixedSize(sizeHint());
}

void CTVCustomizeTrackColorsDlg::accept()
{
    OnApply();
    QDialog::accept();
}

void CTVCustomizeTrackColorsDlg::OnApply()
{
    int i = 0;
    std::for_each(g_trackEntries, g_trackEntries + arraysize(g_trackEntries), [&](const STrackEntry& entry)
	{
		if(entry.paramType.GetType() != eAnimParamType_User)
		{
                s_trackColors[entry.paramType] = m_colorButtons[i]->Color();
		}
		++i;
	});

    s_colorForOthers = m_colorButtons[kOthersEntryIndex]->Color();
    s_colorForDisabled = m_colorButtons[kDisabledEntryIndex]->Color();
    s_colorForMuted = m_colorButtons[kMutedEntryIndex]->Color();

    CTrackViewDialog::GetCurrentInstance()->InvalidateDopeSheet();
}

void CTVCustomizeTrackColorsDlg::OnResetAll()
{
    int i = 0;
    std::for_each(g_trackEntries, g_trackEntries + arraysize(g_trackEntries), [&](const STrackEntry& entry)
	{
		const QString labelText = entry.name;
		if(!labelText.isEmpty())
		{
			m_colorButtons[i]->SetColor(entry.defaultColor);
		}
		++i;
	});
}

void CTVCustomizeTrackColorsDlg::SaveColors(const char* sectionName)
{
    QSettings settings;
    for (auto g : QString(sectionName).split('\\'))
    {
        settings.beginGroup(g);
    }
    std::for_each(begin(s_trackColors), end(s_trackColors),
    [&](const std::pair<CAnimParamType, QColor>& pair)
    {
        const QString trackColorEntry = QString::fromLatin1("%1%2").arg(TRACKCOLOR_ENTRY_PREFIX).arg(static_cast<int>(pair.first.GetType()));
        settings.setValue(trackColorEntry, pair.second.rgb());
    });

    settings.setValue(TRACKCOLOR_FOR_OTHERS_ENTRY, s_colorForOthers.rgb());
    settings.setValue(TRACKCOLOR_FOR_DISABLED_ENTRY, s_colorForDisabled.rgb());
    settings.setValue(TRACKCOLOR_FOR_MUTED_ENTRY, s_colorForMuted.rgb());
}

void CTVCustomizeTrackColorsDlg::LoadColors(const char* sectionName)
{
    QSettings settings;
    for (auto g : QString(sectionName).split('\\'))
    {
        settings.beginGroup(g);
    }
    std::for_each(g_trackEntries, g_trackEntries + arraysize(g_trackEntries), [&](const STrackEntry& entry)
    {
        if (entry.paramType.GetType() != eAnimParamType_User)
        {
            s_trackColors[entry.paramType] = QColor::fromRgb(settings.value(QStringLiteral("%2%3").arg(TRACKCOLOR_ENTRY_PREFIX).arg(static_cast<int>(entry.paramType.GetType())), entry.defaultColor.rgb()).toInt());
        }
    });

    s_colorForOthers = QColor::fromRgb(settings.value(TRACKCOLOR_FOR_OTHERS_ENTRY, g_trackEntries[kOthersEntryIndex].defaultColor.rgb()).toInt());
    s_colorForDisabled = QColor::fromRgb(settings.value(TRACKCOLOR_FOR_DISABLED_ENTRY, g_trackEntries[kDisabledEntryIndex].defaultColor.rgb()).toInt());
    s_colorForMuted = QColor::fromRgb(settings.value(TRACKCOLOR_FOR_MUTED_ENTRY, g_trackEntries[kMutedEntryIndex].defaultColor.rgb()).toInt());
}

void CTVCustomizeTrackColorsDlg::OnExport()
{
    QString savePath;
    if (CFileUtil::SelectSaveFile("Custom Track Colors Files (*.ctc)", "ctc",
            Path::GetUserSandboxFolder(), savePath))
    {
        Export(savePath);
    }
}

void CTVCustomizeTrackColorsDlg::OnImport()
{
    QString loadPath;
    if (CFileUtil::SelectFile("Custom Track Colors Files (*.ctc)",
            Path::GetUserSandboxFolder(), loadPath))
    {
        if (Import(loadPath))
        {
            // since the user is explicitly pressing 'Import', we assume he or she wants to apply this import
            // to see the result immediately, based on a customer feedback sample of one
            OnApply();
        }
        else
        {
            QMessageBox::critical(this, tr("Cannot import"), tr("The file format is invalid!"));
        }
    }
}

void CTVCustomizeTrackColorsDlg::Export(const QString& fullPath) const
{
    XmlNodeRef customTrackColorsNode = XmlHelpers::CreateXmlNode("customtrackcolors");

    int i = 0;
    std::for_each(g_trackEntries, g_trackEntries + arraysize(g_trackEntries), [&](const STrackEntry& entry)
	{
		if(entry.paramType.GetType() != eAnimParamType_User)
		{
			XmlNodeRef entryNode = customTrackColorsNode->newChild("entry");

			// Serialization is const safe
			CAnimParamType &paramType = const_cast<CAnimParamType&>( entry.paramType );
			paramType.Serialize( entryNode, false );
                entryNode->setAttr("color", m_colorButtons[i]->Color().rgb());
		}
		++i;
	});

    XmlNodeRef othersNode = customTrackColorsNode->newChild("others");
    othersNode->setAttr("color", m_colorButtons[kOthersEntryIndex]->Color().rgb());
    XmlNodeRef disabledNode = customTrackColorsNode->newChild("disabled");
    disabledNode->setAttr("color", m_colorButtons[kDisabledEntryIndex]->Color().rgb());
    XmlNodeRef mutedNode = customTrackColorsNode->newChild("muted");
    mutedNode->setAttr("color", m_colorButtons[kMutedEntryIndex]->Color().rgb());

    XmlHelpers::SaveXmlNode(GetIEditor()->GetFileUtil(), customTrackColorsNode, fullPath.toStdString().c_str());
}

bool CTVCustomizeTrackColorsDlg::Import(const QString& fullPath)
{
    XmlNodeRef customTrackColorsNode = XmlHelpers::LoadXmlFromFile(fullPath.toStdString().c_str());
    if (customTrackColorsNode == NULL)
    {
        return false;
    }

    QColor color;
    for (int i = 0; i < customTrackColorsNode->getChildCount(); ++i)
    {
        XmlNodeRef childNode = customTrackColorsNode->getChild(i);
        if (QString(childNode->getTag()) != "entry")
        {
            continue;
        }

        CAnimParamType paramType;
        paramType.Serialize(childNode, true);

        // Get the entry index for this param type.
        const STrackEntry* pEntry = std::find_if(g_trackEntries, g_trackEntries + arraysize(g_trackEntries),
                [=](const STrackEntry& entry)
		{ 
			return entry.paramType == paramType;
		});
        int entryIndex = pEntry - g_trackEntries;
        if (entryIndex >= arraysize(g_trackEntries)) // If not found, skip this.
        {
            continue;
        }
        GetQColorFromXmlNode(color, childNode);
        m_colorButtons[entryIndex]->SetColor(color);
    }

    XmlNodeRef othersNode = customTrackColorsNode->findChild("others");
    if (othersNode)
    {
        GetQColorFromXmlNode(color, othersNode);
        m_colorButtons[kOthersEntryIndex]->SetColor(color);
    }

    XmlNodeRef disabledNode = customTrackColorsNode->findChild("disabled");
    if (disabledNode)
    {
        GetQColorFromXmlNode(color, disabledNode);
        m_colorButtons[kDisabledEntryIndex]->SetColor(color);
    }

    XmlNodeRef mutedNode = customTrackColorsNode->findChild("muted");
    if (mutedNode)
    {
        GetQColorFromXmlNode(color, mutedNode);
        m_colorButtons[kMutedEntryIndex]->SetColor(color);
    }

    return true;
}

#include <TrackView/TVCustomizeTrackColorsDlg.moc>
