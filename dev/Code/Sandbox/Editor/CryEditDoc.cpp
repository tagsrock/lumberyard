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
#include "CryEditDoc.h"
#include "PluginManager.h"
#include "Mission.h"
#include "EquipPackLib.h"
#include "VegetationMap.h"
#include "ViewManager.h"
#include "DisplaySettings.h"
#include "GameEngine.h"
#include "MissionSelectDialog.h"
#include "Terrain/SurfaceType.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/Clouds.h"
#include "Util/PakFile.h"
#include "Util/FileUtil.h"
#include "Objects/BaseObject.h"
#include "EntityPrototypeManager.h"
#include "Material/MaterialManager.h"
#include "Particles/ParticleManager.h"
#include "Prefabs/PrefabManager.h"
#include "GameTokens/GameTokenManager.h"
#include "LensFlareEditor/LensFlareManager.h"
#include "ErrorReportDialog.h"
#include "Undo/Undo.h"
#include "SurfaceTypeValidator.h"
#include "ShaderCache.h"
#include "Util/AutoLogTime.h"
#include "Util/BoostPythonHelpers.h"
#include "Objects/ObjectLayerManager.h"
#include "ICryPak.h"
#include "Objects/BrushObject.h"
#include "CheckOutDialog.h"
#include <IGameFramework.h>
#include "IEditor.h"
#include "IEditorGame.h"
#include "GameExporter.h"
#include "MainWindow.h"
#include "ITimeOfDay.h"
#include <IAudioSystem.h>
#include "LevelFileDialog.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/parallel/thread.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QDateTime>
#include <QMessageBox>
#include <QTimer>
#include <QDir>

#include "QtUtil.h"

//#define PROFILE_LOADING_WITH_VTUNE

// profilers api.
//#include "pure.h"
#ifdef PROFILE_LOADING_WITH_VTUNE
#include "C:\Program Files\Intel\Vtune\Analyzer\Include\VTuneApi.h"
#pragma comment(lib,"C:\\Program Files\\Intel\\Vtune\\Analyzer\\Lib\\VTuneApi.lib")
#endif

static const char* kAutoBackupFolder = "_autobackup";
static const char* kHoldFolder = "_hold";
static const char* kSaveBackupFolder = "_savebackup";
static const char* kResizeTempFolder = "_tmpresize";

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc construction/destruction

CCryEditDoc::CCryEditDoc()
    : doc_validate_surface_types(0)
    , m_modifiedModuleFlags(eModifiedNothing)
    // It assumes loaded levels have already been exported. Can be a big fat lie, though.
    // The right way would require us to save to the level folder the export status of the
    // level.
    , m_boLevelExported(true)
    , m_mission(NULL)
    , m_modified(false)
{
    ////////////////////////////////////////////////////////////////////////
    // Set member variables to initial values
    ////////////////////////////////////////////////////////////////////////
    m_bLoadFailed = false;
    m_waterColor = QColor(0, 0, 255);
    m_pClouds = new CClouds();
    m_fogTemplate = GetIEditor()->FindTemplate("Fog");
    m_environmentTemplate = GetIEditor()->FindTemplate("Environment");

    if (m_environmentTemplate)
    {
        m_fogTemplate = m_environmentTemplate->findChild("Fog");
    }
    else
    {
        m_environmentTemplate = XmlHelpers::CreateXmlNode("Environment");
    }

    m_pLevelShaderCache = new CLevelShaderCache;
    m_bDocumentReady = false;
    GetIEditor()->SetDocument(this);
    CLogFile::WriteLine("Document created");
    m_pTmpXmlArchHack = 0;
    RegisterConsoleVariables();

    MainWindow::instance()->GetActionManager()->RegisterActionHandler(ID_FILE_SAVE_AS, this, &CCryEditDoc::OnFileSaveAs);
}

CCryEditDoc::~CCryEditDoc()
{
    GetIEditor()->SetDocument(nullptr);
    ClearMissions();
    GetIEditor()->GetTerrainManager()->ClearLayers();
    delete m_pLevelShaderCache;
    SAFE_DELETE(m_pClouds);
    CLogFile::WriteLine("Document destroyed");
}

bool CCryEditDoc::IsModified() const
{
    return m_modified;
}

void CCryEditDoc::SetModifiedFlag(bool modified)
{
    m_modified = modified;
}

QString CCryEditDoc::GetPathName() const
{
    return m_pathName;
}

void CCryEditDoc::SetPathName(const QString& pathName)
{
    m_pathName = pathName;
    SetTitle(pathName.isEmpty() ? tr("Untitle") : PathUtil::GetFileName(pathName.toLatin1().data()).c_str());
}

QString CCryEditDoc::GetTitle() const
{
    return m_title;
}

void CCryEditDoc::SetTitle(const QString& title)
{
    m_title = title;
}

bool CCryEditDoc::DoSave(const QString& pathName, bool replace)
{
    if (!OnSaveDocument(pathName.isEmpty() ? GetPathName() : pathName))
    {
        return false;
    }

    if (replace)
    {
        SetPathName(pathName);
    }

    return true;
}

bool CCryEditDoc::Save()
{
    return OnSaveDocument(GetPathName().toLatin1().data()) == TRUE;
}

void CCryEditDoc::ChangeMission()
{
    GetIEditor()->Notify(eNotify_OnMissionChange);

    // Notify listeners.
    for (std::list<IDocListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
    {
        (*it)->OnMissionChange();
    }
}

void CCryEditDoc::DeleteContents()
{
    SetDocumentReady(false);

    GetIEditor()->Notify(eNotify_OnCloseScene);

    EBUS_EVENT(AzToolsFramework::EditorEntityContextRequestBus, ResetEditorContext);

    GetIEditor()->SetEditTool(0); // Turn off any active edit tools.
    GetIEditor()->SetEditMode(eEditModeSelect);

    //////////////////////////////////////////////////////////////////////////
    // Clear all undo info.
    //////////////////////////////////////////////////////////////////////////
    GetIEditor()->FlushUndo();

    // Notify listeners.
    for (std::list<IDocListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
    {
        (*it)->OnCloseDocument();
    }

    GetIEditor()->GetVegetationMap()->ClearObjects();
    GetIEditor()->GetTerrainManager()->ClearLayers();
    m_pClouds->GetLastParam()->bValid = false;
    GetIEditor()->ResetViews();

    // Delete all objects from Object Manager.
    GetIEditor()->GetObjectManager()->DeleteAllObjects();
    GetIEditor()->GetObjectManager()->GetLayersManager()->ClearLayers();
    GetIEditor()->GetTerrainManager()->RemoveAllSurfaceTypes();
    ClearMissions();

    GetIEditor()->GetGameEngine()->ResetResources();

    // Load scripts data
    SetModifiedFlag(FALSE);
    SetModifiedModules(eModifiedNothing);
    // Clear error reports if open.
    CErrorReportDialog::Clear();

    // Unload level specific audio binary data.
    Audio::SAudioManagerRequestData<Audio::eAMRT_UNLOAD_AFCM_DATA_BY_SCOPE> oAMData(Audio::eADS_LEVEL_SPECIFIC);
    Audio::SAudioRequest oAudioRequestData;
    oAudioRequestData.nFlags = (Audio::eARF_PRIORITY_HIGH | Audio::eARF_EXECUTE_BLOCKING);
    oAudioRequestData.pData = &oAMData;
    Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);

    // Now unload level specific audio config data.
    Audio::SAudioManagerRequestData<Audio::eAMRT_CLEAR_CONTROLS_DATA> oAMData2(Audio::eADS_LEVEL_SPECIFIC);
    oAudioRequestData.pData = &oAMData2;
    Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);

    Audio::SAudioManagerRequestData<Audio::eAMRT_CLEAR_PRELOADS_DATA> oAMData3(Audio::eADS_LEVEL_SPECIFIC);
    oAudioRequestData.pData = &oAMData3;
    Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);
}


void CCryEditDoc::Save(CXmlArchive& xmlAr)
{
    TDocMultiArchive arrXmlAr;
    FillXmlArArray(arrXmlAr, &xmlAr);
    Save(arrXmlAr);
}

void CCryEditDoc::Save(TDocMultiArchive& arrXmlAr)
{
    m_pTmpXmlArchHack = arrXmlAr[DMAS_GENERAL];
    CAutoDocNotReady autoDocNotReady;
    QString currentMissionName;

    if (arrXmlAr[DMAS_GENERAL] != NULL)
    {
        (*arrXmlAr[DMAS_GENERAL]).root = XmlHelpers::CreateXmlNode("Level");
        (*arrXmlAr[DMAS_GENERAL]).root->setAttr("WaterColor", m_waterColor);

        char version[50];
        GetIEditor()->GetFileVersion().ToString(version, AZ_ARRAY_SIZE(version));
        (*arrXmlAr[DMAS_GENERAL]).root->setAttr("SandboxVersion", version);

        SerializeViewSettings((*arrXmlAr[DMAS_GENERAL]));
        // Cloud parameters ////////////////////////////////////////////////////
        m_pClouds->Serialize((*arrXmlAr[DMAS_GENERAL]));
        // Fog settings  ///////////////////////////////////////////////////////
        SerializeFogSettings((*arrXmlAr[DMAS_GENERAL]));
        // Serialize Missions //////////////////////////////////////////////////
        SerializeMissions(arrXmlAr, currentMissionName, false);
        //! Serialize entity prototype manager.
        GetIEditor()->GetEntityProtManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        //! Serialize prefabs manager.
        GetIEditor()->GetPrefabManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        //! Serialize material manager.
        GetIEditor()->GetMaterialManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        //! Serialize particles manager.
        GetIEditor()->GetParticleManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        //! Serialize game tokens manager.
        GetIEditor()->GetGameTokenManager()->Save();
        //! Serialize LensFlare manager.
        GetIEditor()->GetLensFlareManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);

        SerializeShaderCache((*arrXmlAr[DMAS_GENERAL_NAMED_DATA]));
        SerializeNameSelection((*arrXmlAr[DMAS_GENERAL]));
    }
    AfterSave();
    m_pTmpXmlArchHack = 0;
}


void CCryEditDoc::Load(CXmlArchive& xmlAr, const QString& szFilename)
{
    TDocMultiArchive arrXmlAr;
    FillXmlArArray(arrXmlAr, &xmlAr);
    CCryEditDoc::Load(arrXmlAr, szFilename);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::Load(TDocMultiArchive& arrXmlAr, const QString& szFilename)
{
    // Register a unique load event
    QString fileName = Path::GetFileName(szFilename);
    QString levelHash = GetIEditor()->GetSettingsManager()->GenerateContentHash(arrXmlAr[DMAS_GENERAL]->root, fileName);
    SEventLog loadEvent("Level_" + Path::GetFileName(fileName), "", levelHash);

    // Register this level and its content hash as version
    GetIEditor()->GetSettingsManager()->AddToolVersion(fileName, levelHash);
    GetIEditor()->GetSettingsManager()->RegisterEvent(loadEvent);
    LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);
    m_pTmpXmlArchHack = arrXmlAr[DMAS_GENERAL];
    CAutoDocNotReady autoDocNotReady;

    HEAP_CHECK

    CLogFile::FormatLine("Loading from %s...", szFilename.toLatin1().data());
    QString currentMissionName;
    QString szLevelPath = Path::GetPath(szFilename);

    {
        // Set game g_levelname variable to the name of current level.
        QString szGameLevelName = Path::GetFileName(szFilename);
        ICVar* sv_map = gEnv->pConsole->GetCVar("sv_map");
        if (sv_map)
        {
            sv_map->Set(szGameLevelName.toLatin1().data());
        }
    }

    GetIEditor()->Notify(eNotify_OnBeginSceneOpen);
    GetIEditor()->GetMovieSystem()->RemoveAllSequences();

    {
        // Start recording errors
        const ICVar* pShowErrorDialogOnLoad = gEnv->pConsole->GetCVar("ed_showErrorDialogOnLoad");
        CErrorsRecorder errorsRecorder(pShowErrorDialogOnLoad && (pShowErrorDialogOnLoad->GetIVal() != 0));
        AZStd::string levelPakPath;
        if (AzFramework::StringFunc::Path::ConstructFull(gEnv->pFileIO->GetAlias("@assets@"), szLevelPath.toLatin1().data(), "level", "pak", levelPakPath, true))
        {
            //Check whether level.pak is present
            if (!gEnv->pFileIO->Exists(levelPakPath.c_str()))
            {
                CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "level.pak is missing.  This will cause other errors.  To fix this, re-export the level.");
            }
        }

        int t0 = GetTickCount();

#ifdef PROFILE_LOADING_WITH_VTUNE
        VTResume();
#endif
        // Parse level specific config data.
        const char* controlsPath = nullptr;
        Audio::AudioSystemRequestBus::BroadcastResult(controlsPath, &Audio::AudioSystemRequestBus::Events::GetControlsPath);
        QString sAudioLevelPath(controlsPath);
        sAudioLevelPath += "levels/";
        string const sLevelNameOnly = PathUtil::GetFileName(fileName.toLatin1().data());
        sAudioLevelPath += sLevelNameOnly;
        QByteArray path = sAudioLevelPath.toLatin1();
        Audio::SAudioManagerRequestData<Audio::eAMRT_PARSE_CONTROLS_DATA> oAMData(path, Audio::eADS_LEVEL_SPECIFIC);
        Audio::SAudioRequest oAudioRequestData;
        oAudioRequestData.nFlags = (Audio::eARF_PRIORITY_HIGH | Audio::eARF_EXECUTE_BLOCKING); // Needs to be blocking so data is available for next preloading request!
        oAudioRequestData.pData = &oAMData;
        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);

        Audio::SAudioManagerRequestData<Audio::eAMRT_PARSE_PRELOADS_DATA> oAMData2(path, Audio::eADS_LEVEL_SPECIFIC);
        oAudioRequestData.pData = &oAMData2;
        Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);

        Audio::TAudioPreloadRequestID nPreloadRequestID = INVALID_AUDIO_PRELOAD_REQUEST_ID;
        Audio::AudioSystemRequestBus::BroadcastResult(nPreloadRequestID, &Audio::AudioSystemRequestBus::Events::GetAudioPreloadRequestID, sLevelNameOnly.c_str());
        if (nPreloadRequestID != INVALID_AUDIO_PRELOAD_REQUEST_ID)
        {
            Audio::SAudioManagerRequestData<Audio::eAMRT_PRELOAD_SINGLE_REQUEST> oAMData2(nPreloadRequestID);
            oAudioRequestData.pData = &oAMData2;
            Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);
        }

        HEAP_CHECK

        SerializeMissions(arrXmlAr, currentMissionName, false);

        // If multiple missions, select specific mission to load.
        if (GetMissionCount() > 1)
        {
            CMissionSelectDialog dlg;
            if (dlg.exec() == QDialog::Accepted)
            {
                currentMissionName = dlg.GetSelected();
            }
        }

        HEAP_CHECK

        {
            CAutoLogTime logtime("Load Terrain");

            if (!GetIEditor()->GetTerrainManager()->Load())
            {
                GetIEditor()->GetTerrainManager()->SerializeTerrain(arrXmlAr); // load old version
            }
            if (!GetIEditor()->GetTerrainManager()->LoadTexture())
            {
                GetIEditor()->GetTerrainManager()->SerializeTexture((*arrXmlAr[DMAS_GENERAL]));  // load old version
            }
            GetIEditor()->GetHeightmap()->InitTerrain();
            GetIEditor()->GetHeightmap()->UpdateEngineTerrain();
        }

        {
            CAutoLogTime logtime("Game Engine level load");
            GetIEditor()->GetGameEngine()->LoadLevel(szLevelPath, currentMissionName, true, true);
        }

        //////////////////////////////////////////////////////////////////////////
        // Load water color.
        //////////////////////////////////////////////////////////////////////////
        (*arrXmlAr[DMAS_GENERAL]).root->getAttr("WaterColor", m_waterColor);

        //////////////////////////////////////////////////////////////////////////
        // Load materials.
        //////////////////////////////////////////////////////////////////////////
        {
            CAutoLogTime logtime("Load MaterialManager");
            GetIEditor()->GetMaterialManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        }

        //////////////////////////////////////////////////////////////////////////
        // Load Particles.
        //////////////////////////////////////////////////////////////////////////
        {
            CAutoLogTime logtime("Load Particles");
            GetIEditor()->GetParticleManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        }

        //////////////////////////////////////////////////////////////////////////
        // Load LensFlares.
        //////////////////////////////////////////////////////////////////////////
        {
            CAutoLogTime logtime("Load Flares");
            GetIEditor()->GetLensFlareManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        }

        //////////////////////////////////////////////////////////////////////////
        // Load GameTokensManager.
        //////////////////////////////////////////////////////////////////////////
        {
            CAutoLogTime logtime("Load GameTokens");

            if (!GetIEditor()->GetGameTokenManager()->Load())
            {
                GetIEditor()->GetGameTokenManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);    // load old version
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // Load View Settings
        //////////////////////////////////////////////////////////////////////////
        SerializeViewSettings((*arrXmlAr[DMAS_GENERAL]));

        CVegetationMap* pVegetationMap = GetIEditor()->GetVegetationMap();

        if (pVegetationMap)
        {
            CAutoLogTime logtime("Load Vegetation");

            if (!pVegetationMap->Load())
            {
                pVegetationMap->Serialize((*arrXmlAr[DMAS_VEGETATION])); // old version
            }
        }

        // Reposition Vegetation.
        RepositionVegetation();

        // update surf types because layers info only now is available in vegetation groups
        {
            CAutoLogTime logtime("Updating Surface Types");
            GetIEditor()->GetTerrainManager()->ReloadSurfaceTypes(false);
        }

        //////////////////////////////////////////////////////////////////////////
        // Fog settings
        //////////////////////////////////////////////////////////////////////////
        SerializeFogSettings((*arrXmlAr[DMAS_GENERAL]));

        //! Serialize entity prototype manager.
        if (gEnv->pGame)
        {
            CAutoLogTime logtime("Load Entity Archetypes Database");
            GetIEditor()->GetEntityProtManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        }

        //! Serialize prefabs manager.
        {
            CAutoLogTime logtime("Load Prefabs Database");
            GetIEditor()->GetPrefabManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
        }

        {
            QByteArray str;
            str = tr("Activating Mission %1").arg(currentMissionName).toLatin1();

            CAutoLogTime logtime(str.data());

            // Select current mission.
            m_mission = FindMission(currentMissionName);

            if (m_mission)
            {
                SyncCurrentMissionContent(true);
            }
            else
            {
                GetCurrentMission();
            }
        }

        ForceSkyUpdate();

        // Serialize Shader Cache.
        {
            CAutoLogTime logtime("Load Level Shader Cache");
            SerializeShaderCache((*arrXmlAr[DMAS_GENERAL_NAMED_DATA]));
        }

        {
            // support old version of sequences
            IMovieSystem* pMs = GetIEditor()->GetMovieSystem();

            if (pMs)
            {
                for (int k = 0; k < pMs->GetNumSequences(); ++k)
                {
                    IAnimSequence* seq = pMs->GetSequence(k);
                    QString fullname = seq->GetName();
                    CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(fullname);

                    if (!pObj)
                    {
                        pObj = GetIEditor()->GetObjectManager()->NewObject("SequenceObject", 0, fullname);
                    }
                }
            }
        }

        // Name Selection groups
        SerializeNameSelection((*arrXmlAr[DMAS_GENERAL]));

        {
            CAutoLogTime logtime("Post Load");

            // Notify listeners.
            for (std::list<IDocListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
            {
                (*it)->OnLoadDocument();
            }
        }

        CSurfaceTypeValidator().Validate();

#ifdef PROFILE_LOADING_WITH_VTUNE
        VTPause();
#endif

        LogLoadTime(GetTickCount() - t0);
        GetIEditor()->CommitLevelErrorReport();
        m_pTmpXmlArchHack = 0;
        // Loaded with success, remove event from log file
        GetIEditor()->GetSettingsManager()->UnregisterEvent(loadEvent);
    }

    GetIEditor()->Notify(eNotify_OnEndSceneOpen);
}

void CCryEditDoc::AfterSave()
{
    // When saving level also save editor settings
    // Save settings
    gSettings.Save();
    GetIEditor()->GetDisplaySettings()->SaveRegistry();
    MainWindow::instance()->SaveConfig();
}

void CCryEditDoc::SerializeViewSettings(CXmlArchive& xmlAr)
{
    // Load or restore the viewer settings from an XML
    if (xmlAr.bLoading)
    {
        // Loading
        CLogFile::WriteLine("Loading View settings...");

        Vec3 vp(0.0f, 0.0f, 256.0f);
        Ang3 va(ZERO);

        XmlNodeRef view = xmlAr.root->findChild("View");
        if (view)
        {
            view->getAttr("ViewerPos", vp);
            view->getAttr("ViewerAngles", va);
        }

        CViewport* pVP = GetIEditor()->GetViewManager()->GetGameViewport();

        if (pVP)
        {
            Matrix34 tm = Matrix34::CreateRotationXYZ(va);
            tm.SetTranslation(vp);
            pVP->SetViewTM(tm);
        }

        // Load grid.
        XmlNodeRef gridNode = xmlAr.root->findChild("Grid");

        if (gridNode)
        {
            GetIEditor()->GetViewManager()->GetGrid()->Serialize(gridNode, xmlAr.bLoading);
        }
    }
    else
    {
        // Storing
        CLogFile::WriteLine("Storing View settings...");

        XmlNodeRef view = xmlAr.root->newChild("View");
        CViewport* pVP = GetIEditor()->GetViewManager()->GetGameViewport();

        if (pVP)
        {
            Vec3 pos = pVP->GetViewTM().GetTranslation();
            Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(pVP->GetViewTM()));
            view->setAttr("ViewerPos", pos);
            view->setAttr("ViewerAngles", angles);
        }

        // Save grid.
        XmlNodeRef gridNode = xmlAr.root->newChild("Grid");
        GetIEditor()->GetViewManager()->GetGrid()->Serialize(gridNode, xmlAr.bLoading);
    }
}

void CCryEditDoc::SerializeFogSettings(CXmlArchive& xmlAr)
{
    if (xmlAr.bLoading)
    {
        CLogFile::WriteLine("Loading Fog settings...");

        XmlNodeRef fog = xmlAr.root->findChild("Fog");

        if (!fog)
        {
            return;
        }

        if (m_fogTemplate)
        {
            CXmlTemplate::GetValues(m_fogTemplate, fog);
        }
    }
    else
    {
        CLogFile::WriteLine("Storing Fog settings...");

        XmlNodeRef fog = xmlAr.root->newChild("Fog");

        if (m_fogTemplate)
        {
            CXmlTemplate::SetValues(m_fogTemplate, fog);
        }
    }
}

void CCryEditDoc::SerializeMissions(TDocMultiArchive& arrXmlAr, QString& currentMissionName, bool bPartsInXml)
{
    bool bLoading = IsLoadingXmlArArray(arrXmlAr);

    if (bLoading)
    {
        // Loading
        CLogFile::WriteLine("Loading missions...");
        // Clear old layers
        ClearMissions();
        // Load shared objects and layers.
        XmlNodeRef objectsNode = arrXmlAr[DMAS_GENERAL]->root->findChild("Objects");
        XmlNodeRef objectLayersNode = arrXmlAr[DMAS_GENERAL]->root->findChild("ObjectLayers");
        // Load the layer count
        XmlNodeRef node = arrXmlAr[DMAS_GENERAL]->root->findChild("Missions");

        if (!node)
        {
            return;
        }

        QString current;
        node->getAttr("Current", current);
        currentMissionName = current;

        // Read all node
        for (int i = 0; i < node->getChildCount(); i++)
        {
            CXmlArchive ar(*arrXmlAr[DMAS_GENERAL]);
            ar.root = node->getChild(i);
            CMission* mission = new CMission(this);
            mission->Serialize(ar);
            if (bPartsInXml)
            {
                mission->SerializeTimeOfDay(*arrXmlAr[DMAS_TIME_OF_DAY]);
                mission->SerializeEnvironment(*arrXmlAr[DMAS_ENVIRONMENT]);
            }
            else
            {
                mission->LoadParts();
            }

            // Timur[9/11/2002] For backward compatibility with shared objects
            if (objectsNode)
            {
                mission->AddObjectsNode(objectsNode);
            }
            if (objectLayersNode)
            {
                mission->SetLayersNode(objectLayersNode);
            }

            AddMission(mission);
        }
    }
    else
    {
        // Storing
        CLogFile::WriteLine("Storing missions...");
        // Save contents of current mission.
        SyncCurrentMissionContent(false);

        XmlNodeRef node = arrXmlAr[DMAS_GENERAL]->root->newChild("Missions");

        //! Store current mission name.
        currentMissionName = GetCurrentMission()->GetName();
        node->setAttr("Current", currentMissionName.toLatin1().data());

        // Write all surface types.
        for (int i = 0; i < m_missions.size(); i++)
        {
            CXmlArchive ar(*arrXmlAr[DMAS_GENERAL]);
            ar.root = node->newChild("Mission");
            m_missions[i]->Serialize(ar, false);
            if (bPartsInXml)
            {
                m_missions[i]->SerializeTimeOfDay(*arrXmlAr[DMAS_TIME_OF_DAY]);
                m_missions[i]->SerializeEnvironment(*arrXmlAr[DMAS_ENVIRONMENT]);
            }
            else
            {
                m_missions[i]->SaveParts();
            }
        }
        CLogFile::WriteString("Done");
    }
}

void CCryEditDoc::SerializeShaderCache(CXmlArchive& xmlAr)
{
    if (xmlAr.bLoading)
    {
        void* pData = 0;
        int nSize = 0;

        if (xmlAr.pNamedData->GetDataBlock("ShaderCache", pData, nSize))
        {
            if (nSize <= 0)
            {
                return;
            }

            QByteArray str(nSize + 1, 0);
            memcpy(str.data(), pData, nSize);
            str[nSize] = 0;
            m_pLevelShaderCache->LoadBuffer(str);
        }
    }
    else
    {
        QString buf;

        m_pLevelShaderCache->SaveBuffer(buf);

        if (!buf.isEmpty())
        {
            xmlAr.pNamedData->AddDataBlock("ShaderCache", buf.toLatin1().data(), buf.toLatin1().count());
        }
    }
}

void CCryEditDoc::SerializeNameSelection(CXmlArchive& xmlAr)
{
    IObjectManager* pObjManager = GetIEditor()->GetObjectManager();

    if (pObjManager)
    {
        pObjManager->SerializeNameSelection(xmlAr.root, xmlAr.bLoading);
    }
}

void CCryEditDoc::SetModifiedModules(EModifiedModule eModifiedModule, bool boSet)
{
    if (!boSet)
    {
        m_modifiedModuleFlags &= ~eModifiedModule;
    }
    else
    {
        if (eModifiedModule == eModifiedNothing)
        {
            m_modifiedModuleFlags = eModifiedNothing;
        }
        else
        {
            m_modifiedModuleFlags |= eModifiedModule;
        }
    }
}

int CCryEditDoc::GetModifiedModule()
{
    return m_modifiedModuleFlags;
}

BOOL CCryEditDoc::CanCloseFrame(CFrameWnd* pFrame)
{
    // Ask the base class to ask for saving, which also includes the save
    // status of the plugins. Additionaly we query if all the plugins can exit
    // now. A reason for a failure might be that one of the plugins isn't
    // currently processing data or has other unsaved information which
    // are not serialized in the project file
    if (!SaveModified())
    {
        return FALSE;
    }

    if (!GetIEditor()->GetPluginManager()->CanAllPluginsExitNow())
    {
        return FALSE;
    }

    // If there is an export in process, exiting will corrupt it
    if (CGameExporter::GetCurrentExporter() != nullptr)
    {
        return FALSE;
    }

    return TRUE;
}

bool CCryEditDoc::SaveModified()
{
    if (!IsModified())
        return true;

    auto button = QMessageBox::question(QApplication::activeWindow(), QString(), tr("Save changes to %1?").arg(GetTitle()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    switch (button)
    {
    case QMessageBox::Cancel:
        return false;
    case QMessageBox::Yes:
        return DoFileSave();
    case QMessageBox::No:
        SetModifiedFlag(false);
        return true;
    }
    Q_UNREACHABLE();
}

void CCryEditDoc::OnFileSaveAs()
{
    CLevelFileDialog levelFileDialog(false);
    if (levelFileDialog.exec() == QDialog::Accepted)
    {
        OnSaveDocument(levelFileDialog.GetFileName());
    }
}

bool CCryEditDoc::OnOpenDocument(const QString& lpszPathName)
{
    TOpenDocContext context;
    if (!BeforeOpenDocument(lpszPathName, context))
    {
        return FALSE;
    }
    return DoOpenDocument(lpszPathName, context);
}

bool CCryEditDoc::BeforeOpenDocument(const QString& lpszPathName, TOpenDocContext& context)
{
    CTimeValue loading_start_time = gEnv->pTimer->GetAsyncTime();
    //ensure we close any open packs
    if (!GetIEditor()->GetLevelFolder().isEmpty())
    {
        GetIEditor()->GetSystem()->GetIPak()->ClosePack((GetIEditor()->GetLevelFolder() + "\\level.pak").toLatin1().data());
    }

    // restore directory to root.
    QDir::setCurrent(GetIEditor()->GetMasterCDFolder());

    QString absoluteLevelPath = lpszPathName;
    QString friendlyDisplayName = Path::GetRelativePath(absoluteLevelPath, true);
    CLogFile::FormatLine("Opening document %s", friendlyDisplayName.toLatin1().data());

    absoluteLevelPath = Path::GamePathToFullPath(friendlyDisplayName);

    context.loading_start_time = loading_start_time;
    context.absoluteLevelPath = absoluteLevelPath;
    return TRUE;
}

bool CCryEditDoc::DoOpenDocument(const QString& lpszPathName, TOpenDocContext& context)
{
    CTimeValue& loading_start_time = context.loading_start_time;
    QString& absoluteLevelCryFilePath = context.absoluteLevelPath;

    // write the full filename and path to the log
    m_bLoadFailed = false;

    ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();
    QString levelPath = Path::GetPath(absoluteLevelCryFilePath);
    QString relativeLevelCryFilePath = Path::GetRelativePath(absoluteLevelCryFilePath, true);
    QString relativeLevelPath = Path::GetRelativePath(levelPath, true);

    // if the level pack exists, open that, too.
    QString levelPackPath = levelPath + "level.pak";

    // load the pack if available.  Note that it is okay for it to be missing at this point
    // we may still be generating it.  Note that it mounts it in "@assets@" so that game code continues functioning,
    // even though it lives in the dev folder.

    pIPak->OpenPack((QString("@assets@/") + relativeLevelPath).toLatin1().data(), levelPackPath.toLatin1().data());

    TDocMultiArchive arrXmlAr = {};
    if (!LoadXmlArchiveArray(arrXmlAr, absoluteLevelCryFilePath, levelPath))
    {
        return FALSE;
    }

    IGameFramework* pGameframework = GetISystem()->GetIGame() ? GetISystem()->GetIGame()->GetIGameFramework() : NULL;

    if (pGameframework)
    {
        string level = absoluteLevelCryFilePath.toLatin1().data();
        pGameframework->SetEditorLevel(PathUtil::GetFileName(level).c_str(), PathUtil::GetPath(level).c_str());
    }
    LoadLevel(arrXmlAr, absoluteLevelCryFilePath);
    ReleaseXmlArchiveArray(arrXmlAr);

    // Load AZ entities for the editor.
    LoadEntities(absoluteLevelCryFilePath);

    if (m_bLoadFailed)
    {
        return FALSE;
    }

    StartStreamingLoad();

    CTimeValue loading_end_time = gEnv->pTimer->GetAsyncTime();

    CLogFile::FormatLine("-----------------------------------------------------------");
    CLogFile::FormatLine("Successfully opened document %s", levelPath.toLatin1().data());
    CLogFile::FormatLine("Level loading time: %.2f seconds", (loading_end_time - loading_start_time).GetSeconds());
    CLogFile::FormatLine("-----------------------------------------------------------");

    // It assumes loaded levels have already been exported. Can be a big fat lie, though.
    // The right way would require us to save to the level folder the export status of the
    // level.
    SetLevelExported(true);

    return TRUE;
}

bool CCryEditDoc::OnNewDocument()
{
    DeleteContents();
    m_pathName.clear();
    SetModifiedFlag(false);
    return true;
}

bool CCryEditDoc::OnSaveDocument(const QString& lpszPathName)
{
    bool saveSuccess = false;
    if (gEnv->IsEditorSimulationMode())
    {
        // Don't allow saving in AI/Physics mode.
        // Prompt the user to exit Simulation Mode (aka AI/Phyics mode) before saving.
        QWidget* mainWindow = nullptr;
        EBUS_EVENT_RESULT(mainWindow, AzToolsFramework::EditorRequests::Bus, GetMainWindow);

        QMessageBox msgBox(mainWindow);
        msgBox.setText(mainWindow->tr("You must exit AI/Physics mode before saving."));
        msgBox.setInformativeText(mainWindow->tr("The level will not be saved."));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
    else
    {
        TSaveDocContext context;
        if (BeforeSaveDocument(lpszPathName, context))
        {
            DoSaveDocument(lpszPathName, context);
            saveSuccess = AfterSaveDocument(lpszPathName, context);
        }     
    }

    return saveSuccess;
}

bool CCryEditDoc::BeforeSaveDocument(const QString& lpszPathName, TSaveDocContext& context)
{
    // Restore directory to root.
    QDir::setCurrent(GetIEditor()->GetMasterCDFolder());

    // If we do not have a level loaded, we will also have an empty path, and that will
    // cause problems later in the save process.  Early out here if that's the case
    QString levelPath = Path::ToUnixPath(Path::GetRelativePath(lpszPathName));
    if (levelPath.isEmpty())
    {
        return false;
    }

    CryLog("Saving to %s...", levelPath.toLatin1().data());
    GetIEditor()->Notify(eNotify_OnBeginSceneSave);

    bool bSaved(true);

    context.bSaved = bSaved;
    return TRUE;
}

bool CCryEditDoc::DoSaveDocument(const QString& filename, TSaveDocContext& context)
{
    bool& bSaved = context.bSaved;
    if (bSaved)
    {
        // Paranoia - we shouldn't get this far into the save routine without a level loaded (empty levelPath)
        // If nothing is loaded, we don't need to save anything
        QString levelPath = Path::ToUnixPath(Path::GetRelativePath(filename, true));
        if (levelPath.isEmpty())
        {
            bSaved = false;
        }
        else
        {
            // Save Tag Point locations to file if auto save of tag points disabled
            if (!gSettings.bAutoSaveTagPoints)
            {
                CCryEditApp::instance()->SaveTagLocations();
            }

            bSaved = SaveLevel(levelPath);

            // Changes filename for this document.
            SetPathName(filename);
        }
    }

    return bSaved;
}

bool CCryEditDoc::AfterSaveDocument(const QString& lpszPathName, TSaveDocContext& context, bool bShowPrompt)
{
    bool& bSaved = context.bSaved;

    GetIEditor()->Notify(eNotify_OnEndSceneSave);

    if (!bSaved)
    {
        if (bShowPrompt)
        {
            QMessageBox::warning(QApplication::activeWindow(), QString(), QObject::tr("Save Failed"), QMessageBox::Ok);
        }
        CLogFile::WriteLine("$4Level saving has failed.");
    }
    else
    {
        CLogFile::WriteLine("$3Level successfully saved");
        SetModifiedFlag(FALSE);
        SetModifiedModules(eModifiedNothing);
        MainWindow::instance()->ResetAutoSaveTimers();
    }

    return bSaved;
}


static void GetUserSettingsFile(const QString& levelFolder, QString& userSettings)
{
    const char* pUserName = GetISystem()->GetUserName();
    QString fileName;
    fileName = QStringLiteral("%1_usersettings.editor_xml").arg(pUserName);
    userSettings = Path::Make(levelFolder, fileName);
}


bool CCryEditDoc::SaveLevel(const QString& filename)
{
    QWaitCursor wait;

    CAutoCheckOutDialogEnableForAll enableForAll;

    QString fullPathName = filename;
    if (QFileInfo(filename).isRelative())
    {
        // Resolving the path through resolvepath would normalize and lowcase it, and in this case, we don't want that.
        fullPathName = QStringLiteral("%1/%2").arg(gEnv->pFileIO->GetAlias("@devassets@")).arg(filename);
    }

    if (!CFileUtil::OverwriteFile(fullPathName))
    {
        return false;
    }

    BackupBeforeSave();

    QString levelAbsoluteFolder = Path::GetPath(fullPathName);
    CFileUtil::CreateDirectory(levelAbsoluteFolder.toLatin1().data());
    GetIEditor()->GetGameEngine()->SetLevelPath(levelAbsoluteFolder);

    // need to copy existing level data before saving to different folder
    const QString oldLevelRelativePath = Path::ToUnixPath(Path::GetRelativePath(GetPathName()));
    const QString oldLevelRelativeFolder = Path::GetPath(oldLevelRelativePath);


    // is it the same folder?
    QString currentLevelRelativeFolder = Path::ToUnixPath(Path::GetRelativePath(levelAbsoluteFolder));

    if (oldLevelRelativeFolder.compare(currentLevelRelativeFolder, Qt::CaseInsensitive) != 0)
    {
        const QString oldLevelAbsoluteFolder = Path::GetPath(GetPathName());
        // if we're saving to a new folder, we need to copy the old folder tree.
        ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();
        pIPak->Lock();

        const QString oldLevelPattern = oldLevelAbsoluteFolder + "*.*";
        const QString oldLevelName = Path::GetFile(GetPathName());
        const QString oldLevelXml = Path::ReplaceExtension(oldLevelName, "xml");
        _finddata_t findData;
        intptr_t findHandle = pIPak->FindFirst(oldLevelPattern.toLatin1().data(), &findData, 0, true);
        if (findHandle >= 0)
        {
            do
            {
                const QString sourceName(findData.name);
                // copy all subdirectories that aren't filtered out.
                if (findData.attrib & _A_SUBDIR)
                {
                    bool skipDir = sourceName == "." || sourceName == "..";
                    skipDir |= sourceName == kSaveBackupFolder || sourceName == kAutoBackupFolder || sourceName == kHoldFolder;   // ignore backups / autosaves
                    skipDir |= (sourceName.compare(kResizeTempFolder, Qt::CaseInsensitive) == 0); // skip resize temp folder
                    skipDir |= sourceName == "Layers"; // layers folder will be created and written out as part of saving
                    if (!skipDir)
                    {
                        CFileUtil::CreateDirectory(Path::AddSlash(levelAbsoluteFolder + sourceName).toLatin1().data());
                        CFileUtil::CopyTree((oldLevelAbsoluteFolder + sourceName).toLatin1().data(), Path::AddSlash(levelAbsoluteFolder + sourceName).toLatin1().data());
                    }
                    continue;
                }

                bool skipFile = sourceName.contains(".cry"); // .cry file will be written out by saving, ignore the source one
                if (skipFile)
                {
                    continue;
                }

                // close any paks in the source folder so that when the paks are re-opened there is
                // no stale cached metadata in the pak system
                if (sourceName.contains(".pak"))
                {
                    pIPak->ClosePack(sourceName.toLatin1().data());
                }

                QString destName = sourceName;
                // copy oldLevel.xml -> newLevel.xml
                if (sourceName.compare(oldLevelXml, Qt::CaseInsensitive) == 0)
                {
                    destName = Path::ReplaceExtension(Path::GetFile(fullPathName), "xml");
                }

                const QString sourceFile = oldLevelAbsoluteFolder + "/" + sourceName;
                const QString destFile = levelAbsoluteFolder + "/" + destName;
                CFileUtil::CopyFile(sourceFile, destFile);
            } while (pIPak->FindNext(findHandle, &findData) >= 0);
            pIPak->FindClose(findHandle);
            // ensure that copied files are not read-only
            CFileUtil::ForEach(levelAbsoluteFolder, [](const QString& filePath)
            {
                QFile(filePath).setPermissions(QFile::ReadOther | QFile::WriteOther);
            });
        }

        pIPak->Unlock();
    }

    // Save level to XML archive.
    CXmlArchive xmlAr;
    Save(xmlAr);

    QString tempSaveFile = Path::ReplaceExtension(fullPathName, "tmp");
    QFile(tempSaveFile).setPermissions(QFile::ReadOther | QFile::WriteOther);
    QFile::remove(tempSaveFile);

    CPakFile pakFile;

    if (!pakFile.Open(tempSaveFile.toLatin1().data(), false))
    {
        gEnv->pLog->LogWarning("Unable to open pack file %s for writing", tempSaveFile.toLatin1().data());
        return false;
    }

    // Save AZ entities to the editor level pak.
    bool savedEntities = false;
    AZStd::vector<char> entitySaveBuffer;
    AZ::IO::ByteContainerStream<AZStd::vector<char> > entitySaveStream(&entitySaveBuffer);
    EBUS_EVENT_RESULT(savedEntities,
        AzToolsFramework::EditorEntityContextRequestBus,
        SaveToStreamForEditor, entitySaveStream);

    if (savedEntities)
    {
        pakFile.UpdateFile("LevelEntities.editor_xml", entitySaveBuffer.begin(), entitySaveBuffer.size());
    }

    // Save XML archive to pak file.
    bool bSaved = xmlAr.SaveToPak(Path::GetPath(tempSaveFile), pakFile);
    pakFile.Close();

    if (!bSaved)
    {
        QFile::remove(tempSaveFile);
        gEnv->pLog->LogWarning("Unable to write the level data to file %s", tempSaveFile.toLatin1().data());
        return false;
    }
    
    QFile(fullPathName).setPermissions(QFile::ReadOther | QFile::WriteOther);
    QFile::remove(fullPathName);

    // try a few times, something can lock the file (such as virus scanner, etc).
    bool succeeded = false;
    for (int attempts = 0; attempts < 10; attempts++)
    {
        if (!QFile::rename(tempSaveFile, fullPathName))
        {
            AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100));
        }
        else
        {
            succeeded = true;
            break;
        }
    }

    if (!succeeded)
    {
        gEnv->pLog->LogWarning("Unable to move file %s to %s when saving", tempSaveFile.toLatin1().data(), fullPathName.toLatin1().data());
        return false;
    }

    // Save Heightmap and terrain data
    GetIEditor()->GetTerrainManager()->Save();
    // Save TerrainTexture
    GetIEditor()->GetTerrainManager()->SaveTexture();

    // Save vegetation
    if (GetIEditor()->GetVegetationMap())
    {
        GetIEditor()->GetVegetationMap()->Save();
    }

    if (GetIEditor()->GetGameEngine()->GetIEditorGame())
    {
        GetIEditor()->GetGameEngine()->GetIEditorGame()->OnAfterLevelSave();
    }

    // Commit changes to the disk.
    _flushall();
    return true;
}

bool CCryEditDoc::LoadEntities(const QString& levelPakFile)
{
    bool loadedSuccessfully = false;

    ICryPak* pakSystem = GetIEditor()->GetSystem()->GetIPak();
    bool pakOpened = pakSystem->OpenPack(levelPakFile.toLatin1().data());
    if (pakOpened)
    {
        const QString entityFilename = Path::GetPath(levelPakFile) + "LevelEntities.editor_xml";

        CCryFile entitiesFile;
        if (entitiesFile.Open(entityFilename.toLatin1().data(), "rt"))
        {
            AZStd::vector<char> fileBuffer;
            fileBuffer.resize(entitiesFile.GetLength());
            if (fileBuffer.size() > 0)
            {
                if (fileBuffer.size() == entitiesFile.ReadRaw(fileBuffer.begin(), fileBuffer.size()))
                {
                    AZ::IO::ByteContainerStream<AZStd::vector<char> > fileStream(&fileBuffer);

                    EBUS_EVENT_RESULT(loadedSuccessfully, AzToolsFramework::EditorEntityContextRequestBus, LoadFromStream, fileStream);
                }
                else
                {
                    AZ_Error("Editor", "Failed to load level entities because the file \"%s\" could not be read.", entityFilename.toLatin1().data());
                }
            }
            else
            {
                AZ_Error("Editor", "Failed to load level entities because the file \"%s\" is empty.", entityFilename.toLatin1().data());
            }

            entitiesFile.Close();
        }
        else
        {
            AZ_Error("Editor", "Failed to load level entities because the file \"%s\" was not found.", entityFilename.toLatin1().data());
        }

        pakSystem->ClosePack(levelPakFile.toLatin1().data());
    }

    return loadedSuccessfully;
}

bool CCryEditDoc::LoadLevel(TDocMultiArchive& arrXmlAr, const QString& absoluteCryFilePath)
{
    ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();

    QString relativeFilePath = Path::GetRelativePath(absoluteCryFilePath);
    QString relativeFolder = Path::GetPath(relativeFilePath);

    GetIEditor()->GetGameEngine()->SetLevelPath(Path::GetPath(absoluteCryFilePath));
    OnStartLevelResourceList();

    // Load next level resource list.
    pIPak->GetResourceList(ICryPak::RFOM_NextLevel)->Load(Path::Make(relativeFolder, "resourcelist.txt").toLatin1().data());
    GetIEditor()->Notify(eNotify_OnBeginLoad);
    //GetISystem()->GetISystemEventDispatcher()->OnSystemEvent( ESYSTEM_EVENT_LEVEL_LOAD_START,0,0 );
    DeleteContents();
    SetModifiedFlag(TRUE);  // dirty during de-serialize
    SetModifiedModules(eModifiedAll);
    Load(arrXmlAr, relativeFilePath);

    GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
    // We don't need next level resource list anymore.
    pIPak->GetResourceList(ICryPak::RFOM_NextLevel)->Clear();
    SetModifiedFlag(FALSE); // start off with unmodified
    SetModifiedModules(eModifiedNothing);
    SetDocumentReady(true);
    GetIEditor()->Notify(eNotify_OnEndLoad);

    return true;
}

void CCryEditDoc::Hold(const QString& holdName)
{
    if (!IsDocumentReady())
    {
        return;
    }

    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    QString holdPath = levelPath + "/" + holdName + "/";
    QString holdFilename = holdPath + holdName + ".cry";

    // never auto-backup while we're trying to hold.
    bool oldBackup = gSettings.bBackupOnSave;
    gSettings.bBackupOnSave = false;
    SaveLevel(holdFilename);
    gSettings.bBackupOnSave = oldBackup;

    GetIEditor()->GetGameEngine()->SetLevelPath(levelPath);
}

void CCryEditDoc::Fetch(const QString& holdName, bool bShowMessages, bool bDelHoldFolder)
{
    if (!IsDocumentReady())
    {
        return;
    }

    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    QString holdPath = levelPath + "/" + holdName + "/";
    QString holdFilename = holdPath + holdName + ".cry";

    {
        QFile cFile(holdFilename);
        // Open the file for writing, create it if needed
        if (!cFile.open(QFile::ReadOnly))
        {
            if (bShowMessages)
            {
                QMessageBox::information(QApplication::activeWindow(), QString(), QObject::tr("You have to use 'Hold' before you can fetch!"));
            }
            return;
        }
    }

    // Does the document contain unsaved data ?
    if (bShowMessages && IsModified() &&
        QMessageBox::question(QApplication::activeWindow(), QString(), QObject::tr("The document contains unsaved data, it will be lost if fetched.\r\nReally fetch old state?")) != QMessageBox::Yes)
    {
        return;
    }

    GetIEditor()->FlushUndo();

    TDocMultiArchive arrXmlAr = {};
    if (!LoadXmlArchiveArray(arrXmlAr, holdFilename, holdPath))
    {
        return;
    }
    
    // Load the state
    LoadLevel(arrXmlAr, holdFilename);

    // Load AZ entities for the editor.
    LoadEntities(holdFilename);

    GetIEditor()->GetGameEngine()->SetLevelPath(levelPath);
    GetIEditor()->GetTerrainManager()->GetRGBLayer()->ClosePakForLoading(); //TODO: Support hold/fetch for terrain texture and remove this line
    GetIEditor()->FlushUndo();

    if (bDelHoldFolder)
    {
        CFileUtil::Deltree(holdPath.toLatin1().data(), true);
    }
}



//////////////////////////////////////////////////////////////////////////
namespace {
    struct SFolderTime
    {
        QString folder;
        time_t creationTime;
    };

    bool SortByCreationTime(SFolderTime& a, SFolderTime& b)
    {
        return a.creationTime < b.creationTime;
    }

    // This function, given a source folder to scan, returns all folders within that folder
    // non-recursively.  They will be sorted by time, with most recent first, and oldest last.
    void CollectAllFoldersByTime(const char* sourceFolder, std::vector<SFolderTime>& outputFolders)
    {
        QString folderMask(sourceFolder);
        _finddata_t fileinfo;
        intptr_t handle = gEnv->pCryPak->FindFirst((folderMask + "/*.*").toLatin1().data(), &fileinfo);
        if (handle != -1)
        {
            do
            {
                if (fileinfo.name[0] == '.')
                {
                    continue;
                }

                if (fileinfo.attrib & _A_SUBDIR)
                {
                    SFolderTime ft;
                    ft.folder = fileinfo.name;
                    ft.creationTime = fileinfo.time_create;
                    outputFolders.push_back(ft);
                }
            } while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);
        }
        std::sort(outputFolders.begin(), outputFolders.end(), SortByCreationTime);
    }
}

bool CCryEditDoc::BackupBeforeSave(bool force)
{
    // This function will copy the contents of an entire level folder to a backup folder
    // and delete older ones based on user preferences.
    if (!force && !gSettings.bBackupOnSave)
    {
        return true; // not an error
    }

    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    if (levelPath.isEmpty())
    {
        return false;
    }

    QWaitCursor wait;

    QString saveBackupPath = levelPath + "/" + kSaveBackupFolder;

    std::vector<SFolderTime> folders;
    CollectAllFoldersByTime(saveBackupPath.toLatin1().data(), folders);

    for (int i = int(folders.size()) - gSettings.backupOnSaveMaxCount; i >= 0; --i)
    {
        CFileUtil::Deltree(QStringLiteral("%1/%2/").arg(saveBackupPath, folders[i].folder).toLatin1().data(), true);
    }

    QDateTime theTime = QDateTime::currentDateTime();
    QString subFolder = theTime.toString("yyyy-MM-dd [HH.mm.ss]");

    QString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
    QString backupPath = saveBackupPath + "/" + subFolder + "/";
    gEnv->pCryPak->MakeDir(backupPath.toLatin1().data());

    QString sourcePath = levelPath + "/";
    QString ignoredFiles(kAutoBackupFolder);
    ignoredFiles += "|";
    ignoredFiles += kSaveBackupFolder;
    ignoredFiles += "|";
    ignoredFiles += kHoldFolder;

    // copy that whole tree:
    if (IFileUtil::ETREECOPYOK != CFileUtil::CopyTree(sourcePath, backupPath, true, false, ignoredFiles.toLatin1().data()))
    {
        gEnv->pLog->LogWarning("Attempting to save backup to %s before saving, but could not write all files.", backupPath.toLatin1().data());
        return false;
    }
    return true;
}

void CCryEditDoc::SaveAutoBackup(bool bForce)
{
    if (!bForce && (!gSettings.autoBackupEnabled || GetIEditor()->IsInGameMode()))
    {
        return;
    }

    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    if (levelPath.isEmpty())
    {
        return;
    }

    static bool isInProgress = false;
    if (isInProgress)
    {
        return;
    }

    isInProgress = true;

    QWaitCursor wait;

    QString autoBackupPath = levelPath + "/" + kAutoBackupFolder;

    // collect all subfolders
    std::vector<SFolderTime> folders;

    CollectAllFoldersByTime(autoBackupPath.toLatin1().data(), folders);

    for (int i = int(folders.size()) - gSettings.autoBackupMaxCount; i >= 0; --i)
    {
        CFileUtil::Deltree(QStringLiteral("%1/%2/").arg(autoBackupPath, folders[i].folder).toLatin1().data(), true);
    }

    // save new backup
    QDateTime theTime = QDateTime::currentDateTime();
    QString subFolder = theTime.toString(QStringLiteral("yyyy-MM-dd [HH.mm.ss]"));

    QString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
    QString filename = autoBackupPath + "/" + subFolder + "/" + levelName + "/" + levelName + ".cry";
    SaveLevel(filename);
    GetIEditor()->GetGameEngine()->SetLevelPath(levelPath);

    isInProgress = false;
}


CMission*   CCryEditDoc::GetCurrentMission(bool bSkipLoadingAIWhenSyncingContent /* = false */)
{
    if (m_mission)
    {
        return m_mission;
    }

    if (!m_missions.empty())
    {
        // Choose first available mission.
        SetCurrentMission(m_missions[0]);
        return m_mission;
    }

    // Create initial mission.
    m_mission = new CMission(this);
    m_mission->SetName("Mission0");
    AddMission(m_mission);
    m_mission->SyncContent(true, false, bSkipLoadingAIWhenSyncingContent);
    return m_mission;
}

void CCryEditDoc::SetCurrentMission(CMission* mission)
{
    if (mission != m_mission)
    {
        QWaitCursor wait;

        if (m_mission)
        {
            m_mission->SyncContent(false, false);
        }

        m_mission = mission;
        m_mission->SyncContent(true, false);

        GetIEditor()->GetGameEngine()->LoadMission(m_mission->GetName());
    }
}

void CCryEditDoc::ClearMissions()
{
    for (int i = 0; i < m_missions.size(); i++)
    {
        delete m_missions[i];
    }

    m_missions.clear();
    m_mission = 0;
}

bool CCryEditDoc::IsLevelExported() const
{
    return m_boLevelExported;
}

void CCryEditDoc::SetLevelExported(bool boExported)
{
    m_boLevelExported = boExported;
}

CMission*   CCryEditDoc::FindMission(const QString& name) const
{
    for (int i = 0; i < m_missions.size(); i++)
    {
        if (QString::compare(name.toLatin1().data(), m_missions[i]->GetName(), Qt::CaseInsensitive) == 0)
        {
            return m_missions[i];
        }
    }
    return 0;
}

void CCryEditDoc::AddMission(CMission* mission)
{
    assert(std::find(m_missions.begin(), m_missions.end(), mission) == m_missions.end());
    m_missions.push_back(mission);
    GetIEditor()->Notify(eNotify_OnInvalidateControls);
}

void CCryEditDoc::RemoveMission(CMission* mission)
{
    // if deleting current mission.
    if (mission == m_mission)
    {
        m_mission = 0;
    }

    m_missions.erase(std::find(m_missions.begin(), m_missions.end(), mission));
    GetIEditor()->Notify(eNotify_OnInvalidateControls);
}

LightingSettings* CCryEditDoc::GetLighting()
{
    return GetCurrentMission()->GetLighting();
}

void CCryEditDoc::RegisterListener(IDocListener* listener)
{
    if (listener == nullptr)
    {
        return;
    }

    if (std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end())
    {
        m_listeners.push_back(listener);
    }
}

void CCryEditDoc::UnregisterListener(IDocListener* listener)
{
    m_listeners.remove(listener);
}

void CCryEditDoc::LogLoadTime(int time)
{
    QString appFilePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QString exePath = Path::GetPath(appFilePath);
    QString filename = Path::Make(exePath, "LevelLoadTime.log");
    QString level = GetIEditor()->GetGameEngine()->GetLevelPath();

    CLogFile::FormatLine("[LevelLoadTime] Level %s loaded in %d seconds", level.toLatin1().data(), time / 1000);
#if defined(AZ_PLATFORM_WINDOWS)
    SetFileAttributes(filename.toLatin1().data(), FILE_ATTRIBUTE_ARCHIVE);
#endif

    FILE* file = fopen(filename.toLatin1().data(), "at");

    if (file)
    {
        char version[50];
        GetIEditor()->GetFileVersion().ToShortString(version, AZ_ARRAY_SIZE(version));

        QString text;

        time = time / 1000;
        text = QStringLiteral("\n[%1] Level %2 loaded in %3 seconds").arg(version, level).arg(time);
        fwrite(text.toLatin1().data(), text.toLatin1().length(), 1, file);
        fclose(file);
    }
}

void CCryEditDoc::SetDocumentReady(bool bReady)
{
    m_bDocumentReady = bReady;
}

void CCryEditDoc::GetMemoryUsage(ICrySizer* pSizer)
{
    {
        SIZER_COMPONENT_NAME(pSizer, "UndoManager(estimate)");
        GetIEditor()->GetUndoManager()->GetMemoryUsage(pSizer);
    }

    pSizer->Add(*this);
    GetIEditor()->GetTerrainManager()->GetTerrainMemoryUsage(pSizer);
}

void CCryEditDoc::RegisterConsoleVariables()
{
    doc_validate_surface_types = gEnv->pConsole->GetCVar("doc_validate_surface_types");

    if (!doc_validate_surface_types)
    {
        doc_validate_surface_types = REGISTER_INT_CB("doc_validate_surface_types", 0, 0,
            "Flag indicating whether icons are displayed on the animation graph.\n"
            "Default is 1.\n",
            OnValidateSurfaceTypesChanged);
    }
}

void CCryEditDoc::OnValidateSurfaceTypesChanged(ICVar*)
{
    CErrorsRecorder errorsRecorder(GetIEditor());
    CSurfaceTypeValidator().Validate();
}

void CCryEditDoc::OnStartLevelResourceList()
{
    // after loading another level we clear the RFOM_Level list, the first time the list should be empty
    static bool bFirstTime = true;

    if (bFirstTime)
    {
        const char* pResFilename = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level)->GetFirst();

        while (pResFilename)
        {
            // This should be fixed because ExecuteCommandLine is executed right after engine init as we assume the
            // engine already has all data loaded an is initialized to process commands. Loading data afterwards means
            // some init was done later which can cause problems when running in the engine batch mode (executing console commands).
            gEnv->pLog->LogError("'%s' was loaded after engine init but before level load/new (should be fixed)", pResFilename);
            pResFilename = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level)->GetNext();
        }

        bFirstTime = false;
    }

    gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level)->Clear();
}

void CCryEditDoc::ForceSkyUpdate()
{
    ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
    CMission* pCurMission = GetIEditor()->GetDocument()->GetCurrentMission();

    if (pTimeOfDay && pCurMission)
    {
        pTimeOfDay->SetTime(pCurMission->GetTime(), gSettings.bForceSkyUpdate);
        pCurMission->SetTime(pCurMission->GetTime());
        GetIEditor()->Notify(eNotify_OnTimeOfDayChange);
    }
}

BOOL CCryEditDoc::DoFileSave()
{
    // If the file to save is the temporary level it should 'save as' since temporary levels will get deleted
    const char* temporaryLevelName = GetTemporaryLevelName();
    if (QString::compare(GetIEditor()->GetLevelName(), temporaryLevelName) == 0)
    {
        QString filename;
        if (CCryEditApp::instance()->GetDocManager()->DoPromptFileName(filename, ID_FILE_SAVE_AS, 0, false, nullptr)
            && !filename.isEmpty() && !QFileInfo(filename).exists())
        {
            if (SaveLevel(filename))
            {
                DeleteTemporaryLevel();
                QString newLevelPath = filename.left(filename.lastIndexOf('/') + 1);
                GetIEditor()->GetDocument()->SetPathName(filename);
                GetIEditor()->GetGameEngine()->SetLevelPath(newLevelPath);
                return TRUE;
            }
        }
        return FALSE;
    }
    if (!IsDocumentReady())
    {
        return FALSE;
    }

    if (GetIEditor()->GetCommandManager()->Execute("general.save_level") == "true")
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

const char* CCryEditDoc::GetTemporaryLevelName() const
{
    return gEnv->pConsole->GetCVar("g_TemporaryLevelName")->GetString();
}

void CCryEditDoc::DeleteTemporaryLevel()
{
    QString tempLevelPath = (Path::GetEditingGameDataFolder() + "/Levels/" + GetTemporaryLevelName()).c_str();
    GetIEditor()->GetSystem()->GetIPak()->ClosePacks(tempLevelPath.toLatin1().data(), ICryPak::EPathResolutionRules::FLAGS_ADD_TRAILING_SLASH);
    CFileUtil::Deltree(tempLevelPath.toLatin1().data(), true);
}

void CCryEditDoc::InitEmptyLevel(int resolution, int unitSize, bool bUseTerrain)
{
    GetIEditor()->SetStatusText("Initializing Level...");

    OnStartLevelResourceList();

    GetIEditor()->Notify(eNotify_OnBeginNewScene);
    CLogFile::WriteLine("Preparing new document...");

    ////////////////////////////////////////////////////////////////////////
    // Reset heightmap (water level, etc) to default
    ////////////////////////////////////////////////////////////////////////
    GetIEditor()->GetTerrainManager()->ResetHeightMap();
    GetIEditor()->GetTerrainManager()->SetUseTerrain(bUseTerrain);

    // If possible set terrain to correct size here, this will help with initial camera placement in new levels
    if (bUseTerrain)
    {
        GetIEditor()->GetTerrainManager()->SetTerrainSize(resolution, unitSize);
    }

    ////////////////////////////////////////////////////////////////////////
    // Reset the terrain texture of the top render window
    ////////////////////////////////////////////////////////////////////////

    //cleanup resources!
    GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_POST_UNLOAD, 0, 0);

    //////////////////////////////////////////////////////////////////////////
    // Initialize defaults.
    //////////////////////////////////////////////////////////////////////////
    if (!GetIEditor()->IsInPreviewMode())
    {
        GetIEditor()->GetTerrainManager()->CreateDefaultLayer();

        // Make new mission.
        GetIEditor()->ReloadTemplates();
        m_environmentTemplate = GetIEditor()->FindTemplate("Environment");

        GetCurrentMission(true);    // true = skip loading the AI in case the content needs to get synchronized (otherwise it would attempt to load AI stuff from the previously loaded level (!) which might give confusing warnings)
        GetIEditor()->GetGameEngine()->SetMissionName(GetCurrentMission()->GetName());
        GetIEditor()->GetGameEngine()->SetLevelCreated(true);
        GetIEditor()->GetGameEngine()->ReloadEnvironment();
        GetIEditor()->GetGameEngine()->SetLevelCreated(false);

        // Default time of day.
        XmlNodeRef root = GetISystem()->LoadXmlFromFile("@devroot@/Editor/default_time_of_day.xml");
        if (root)
        {
            ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
            pTimeOfDay->Serialize(root, true);
            pTimeOfDay->SetTime(13.5f, true);  // Set to 1:30pm for new level
        }
    }

    GetIEditor()->GetObjectManager()->GetLayersManager()->CreateMainLayer();

    {
        // Notify listeners.
        std::list<IDocListener*> listeners = m_listeners;
        std::list<IDocListener*>::iterator it, next;
        for (it = listeners.begin(); it != listeners.end(); it = next)
        {
            next = it;
            next++;
            (*it)->OnNewDocument();
        }
    }

    // Tell the system that the level has been created/loaded.
    GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);

    GetIEditor()->Notify(eNotify_OnEndNewScene);
    SetModifiedFlag(FALSE);
    SetLevelExported(false);
    SetModifiedModules(eModifiedNothing);

    GetIEditor()->SetStatusText("Ready");
}

void CCryEditDoc::OnEnvironmentPropertyChanged(IVariable* pVar)
{
    if (pVar == NULL)
    {
        return;
    }

    XmlNodeRef node = GetEnvironmentTemplate();
    if (node == NULL)
    {
        return;
    }

    // QVariant will not convert a void * to int, so do it manually.
    int nKey = reinterpret_cast<intptr_t>(pVar->GetUserData().value<void*>());

    int nGroup = (nKey & 0xFFFF0000) >> 16;
    int nChild = (nKey & 0x0000FFFF);

    if (nGroup < 0 || nGroup >= node->getChildCount())
    {
        return;
    }

    XmlNodeRef groupNode = node->getChild(nGroup);

    if (groupNode == NULL)
    {
        return;
    }

    if (nChild < 0 || nChild >= groupNode->getChildCount())
    {
        return;
    }

    XmlNodeRef childNode = groupNode->getChild(nChild);
    if (childNode == NULL)
    {
        return;
    }

    if (pVar->GetDataType() == IVariable::DT_COLOR)
    {
        Vec3 value;
        pVar->Get(value);
        QString buff;
        QColor gammaColor = ColorLinearToGamma(ColorF(value.x, value.y, value.z));
        buff = QStringLiteral("%1,%2,%3").arg(gammaColor.red()).arg(gammaColor.green()).arg(gammaColor.blue());
        childNode->setAttr("value", buff.toLatin1().data());
    }
    else
    {
        QString value;
        pVar->Get(value);
        childNode->setAttr("value", value.toLatin1().data());
    }

    GetIEditor()->GetGameEngine()->ReloadEnvironment();
}

QString CCryEditDoc::GetCryIndexPath(const LPCTSTR levelFilePath)
{
    QString levelPath = Path::GetPath(levelFilePath);
    QString levelName = Path::GetFileName(levelFilePath);
    return Path::AddPathSlash(levelPath + levelName + "_editor");
}

BOOL CCryEditDoc::LoadXmlArchiveArray(TDocMultiArchive& arrXmlAr, const QString& absoluteLevelPath, const QString& levelPath)
{
    ICryPak* pIPak = GetIEditor()->GetSystem()->GetIPak();

    //if (m_pSWDoc->IsNull())
    {
        CXmlArchive* pXmlAr = new CXmlArchive();
        if (!pXmlAr)
        {
            return FALSE;
        }

        CXmlArchive& xmlAr = *pXmlAr;
        xmlAr.bLoading = true;
        QString relPath = Path::GetRelativePath(absoluteLevelPath, true);

        // bound to the level folder, as if it were the assets folder.
        // this mounts (whateverlevelname.cry) as @assets@/Levels/whateverlevelname/ and thus it works...
        QString bindRootRel = Path::GetPath(relPath);
        bool openLevelPakFileSuccess = pIPak->OpenPack((QString("@assets@/") + bindRootRel).toLatin1().data(), absoluteLevelPath.toLatin1().data());
        if (!openLevelPakFileSuccess)
        {
            Q_UNREACHABLE();
        }

        CPakFile pakFile;
        bool loadFromPakSuccess;
        loadFromPakSuccess = xmlAr.LoadFromPak(bindRootRel, pakFile);
        pIPak->ClosePack(absoluteLevelPath.toLatin1().data());
        if (!loadFromPakSuccess)
        {
            return FALSE;
        }

        FillXmlArArray(arrXmlAr, &xmlAr);
    }

    return TRUE;
}

void CCryEditDoc::ReleaseXmlArchiveArray(TDocMultiArchive& arrXmlAr)
{
    SAFE_DELETE(arrXmlAr[0]);
}


void CCryEditDoc::SyncCurrentMissionContent(bool bRetrieve)
{
    GetCurrentMission()->SyncContent(bRetrieve, false);
}

void CCryEditDoc::RepositionVegetation()
{
    CAutoLogTime logtime("Reposition Vegetation");
    CVegetationMap* vegMap = GetIEditor()->GetVegetationMap();
    if (vegMap)
    {
        vegMap->PlaceObjectsOnTerrain();
    }
}


namespace
{
    bool PySaveLevel()
    {
        if (!GetIEditor()->GetDocument()->DoSave(GetIEditor()->GetDocument()->GetPathName(), TRUE))
        {
            return false;
        }

        return true;
    }
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySaveLevel, general, save_level,
    "Saves the current level.",
    "general.save_level()");

#include <CryEditDoc.moc>
