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
#include "AlembicCompiler.h"
#include "AlembicCompileDialog.h"
#include "Util/FileUtil.h"

// Attempt to add the file to source control if it is available
bool TryAddFileToSourceControl(const QString& filename)
{
    if (!CFileUtil::CheckoutFile(filename.toUtf8().data(), nullptr))
    {
        CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to add file %s to the source control provider", filename);
        return false;
    }

    return true;
}

bool CAlembicCompiler::CompileAlembic(const QString& fullPath)
{
    bool compileConfigFileSaved = false;
    const QString configPath = Path::ReplaceExtension(fullPath, "cbc");
    XmlNodeRef config = XmlHelpers::LoadXmlFromFile(configPath.toLatin1().data());

    CAlembicCompileDialog dialog(config);

    if (dialog.exec() == QDialog::Accepted)
    {
        bool          configChanged = false;
        const QString upAxis = dialog.GetUpAxis();
        const QString playbackFromMemory = dialog.GetPlaybackFromMemory();
        const QString blockCompressionFormat = dialog.GetBlockCompressionFormat();
        const QString meshPrediction = dialog.GetMeshPrediction();
        const QString useBFrames = dialog.GetUseBFrames();
        const uint    indexFrameDistance = dialog.GetIndexFrameDistance();
        const double  positionPrecision = dialog.GetPositionPrecision();
        const float   uvMax = dialog.GetUVmax();

        if (!config)
        {
            CryLog("Build configuration file not found, writing new one");
            config = XmlHelpers::CreateXmlNode("CacheBuildConfiguration");
            configChanged = true;
        }

        if (strcmp(config->getAttr("UpAxis"), upAxis.toLatin1().data()) != 0)
        {
            config->setAttr("UpAxis", upAxis.toLatin1().data());
            configChanged = true;
        }
        if (strcmp(config->getAttr("MeshPrediction"), meshPrediction.toLatin1().data()) != 0)
        {
            config->setAttr("MeshPrediction", meshPrediction.toLatin1().data());
            configChanged = true;
        }
        if (strcmp(config->getAttr("UseBFrames"), useBFrames.toLatin1().data()) != 0)
        {
            config->setAttr("UseBFrames", useBFrames.toLatin1().data());
            configChanged = true;
        }
        if (atoi(config->getAttr("IndexFrameDistance")) != indexFrameDistance)
        {
            config->setAttr("IndexFrameDistance", indexFrameDistance);
            configChanged = true;
        }
        if (strcmp(config->getAttr("BlockCompressionFormat"), blockCompressionFormat.toLatin1().data()) != 0)
        {
            config->setAttr("BlockCompressionFormat", blockCompressionFormat.toLatin1().data());
            configChanged = true;
        }
        if (strcmp(config->getAttr("PlaybackFromMemory"), playbackFromMemory.toLatin1().data()) != 0)
        {
            config->setAttr("PlaybackFromMemory", playbackFromMemory.toLatin1().data());
            configChanged = true;
        }
        if (atof(config->getAttr("PositionPrecision")) != positionPrecision)
        {
            config->setAttr("PositionPrecision", positionPrecision);
            configChanged = true;
        }
        if (atof(config->getAttr("UVmax")) != uvMax)
        {
            config->setAttr("UVmax", uvMax);
            configChanged = true;
        }

        if (configChanged)
        {
            compileConfigFileSaved = XmlHelpers::SaveXmlNode(GetIEditor()->GetFileUtil(), config, configPath.toUtf8().data());
            if (compileConfigFileSaved)
            {
                // If we just created the file above, or the cbc file was not previously managed, attempt to add it to perforce now.
                // Note that XmlHelpers::SaveXmlNode will prompt the user to checkout or overwrite the file
                TryAddFileToSourceControl(configPath);
            }
        }       
    }

    return compileConfigFileSaved;
}
