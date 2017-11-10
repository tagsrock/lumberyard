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

// Description : CEditorImpl class implementation.


#include "StdAfx.h"
#include "IEditorImpl.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "Dialogs/ErrorsDlg.h"
#include "Plugin.h"
#include "IResourceCompilerHelper.h"
#include "PluginManager.h"
#include "IconManager.h"
#include "ViewManager.h"
#include "ViewPane.h"
#include "Objects/GizmoManager.h"
#include "Objects/AxisGizmo.h"
#include "DisplaySettings.h"
#include "ShaderEnum.h"
#include "KeyboardCustomizationSettings.h"
#include "HyperGraph/FlowGraphManager.h"
#include "Export/ExportManager.h"
#include "HyperGraph/FlowGraphModuleManager.h"
#include "HyperGraph/FlowGraphDebuggerEditor.h"
#include "Material/MaterialFXGraphMan.h"
#include "EquipPackLib.h"
#include "CustomActions/CustomActionsEditorManager.h"
#include "AI/AIManager.h"
#include "Undo/Undo.h"
#include "Material/MaterialManager.h"
#include "Material/MaterialPickTool.h"
#include "EntityPrototypeManager.h"
#include "TrackView/TrackViewSequenceManager.h"
#include "AnimationContext.h"
#include "GameEngine.h"
#include "ToolBox.h"
#include "MainWindow.h"
#include "BaseLibraryDialog.h"
#include "Material/Material.h"
#include "EntityPrototype.h"
#include "Particles/ParticleManager.h"
#include "Prefabs/PrefabManager.h"
#include "GameTokens/GameTokenManager.h"
#include "LensFlareEditor/LensFlareManager.h"
#include "DataBaseDialog.h"
#include "UIEnumsDatabase.h"
#include "Util/Ruler.h"
#include "Script/ScriptEnvironment.h"
#include "RenderHelpers/AxisHelper.h"
#include "PickObjectTool.h"
#include "ObjectCreateTool.h"
#include "TerrainModifyTool.h"
#include "RotateTool.h"
#include "VegetationMap.h"
#include "VegetationTool.h"
#include "TerrainTexturePainter.h"
#include "EditMode/ObjectMode.h"
#include "EditMode/VertexMode.h"
#include "Modelling/ModellingMode.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/SurfaceType.h"
#include <I3DEngine.h>
#include <IConsole.h>
#include <IEntitySystem.h>
#include <IMovieSystem.h>
#include <ISourceControl.h>
#include <IAssetTagging.h>
#include "Util/BoostPythonHelpers.h"
#include "Objects/ObjectLayerManager.h"
#include "BackgroundTaskManager.h"
#include "BackgroundScheduleManager.h"
#include "EditorFileMonitor.h"
#include <IEditorGame.h>
#include "EditMode/VertexSnappingModeTool.h"
#include "AssetResolver/AssetResolver.h"
#include "Mission.h"
#include "MainStatusBar.h"
#include <LyMetricsProducer/LyMetricsAPI.h>
#include <aws/core/utils/memory/STL/AWSString.h>
#include <aws/core/platform/FileSystem.h>
#include <WinWidget/WinWidgetManager.h>

#include "EditorParticleUtils.h" // Leroy@Conffx
#include "IEditorParticleUtils.h" // Leroy@Conffx

#include <Serialization/Serializer.h>
#include "SettingsBlock.h"
#include "ResourceSelectorHost.h"
#include "Util/FileUtil_impl.h"
#include "Util/ImageUtil_impl.h" // Vladimir@Conffx
#include "AssetBrowser/AssetBrowserImpl.h"
#include "LogFileImpl.h"  // Vladimir@Conffx
#include "Controls/QRollupCtrl.h"
#include "Controls/RollupBar.h"

#include <AzCore/Serialization/Utils.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzFramework/Network/AssetProcessorConnection.h>
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>

#include "AssetDatabase/AssetDatabaseLocationListener.h"
#include "AzAssetBrowser/AzAssetBrowserRequestHandler.h"

#include "aws/core/utils/crypto/Factories.h"
#include <AzCore/JSON/document.h>
#include <AzCore/Math/Uuid.h>

#include <QDir>

LINK_SYSTEM_LIBRARY(version.lib)

// even in Release mode, the editor will return its heap, because there's no Profile build configuration for the editor
#ifdef _RELEASE
#undef _RELEASE
#endif
#include <CrtDebugStats.h>
#include "Settings.h"

#include "Core/QtEditorApplication.h"
#include "../Plugins/EditorCommon/EditorCommonAPI.h"
#include "QtViewPaneManager.h"
#include "../Plugins/EditorUI_QT/EditorUI_QTAPI.h"

#include <AzCore/Math/Crc.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/FileIO.h>
#include <AzFramework/IO/FileOperations.h>

static CCryEditDoc * theDocument;
#include <QMimeData>
#include <QColorDialog>
#include <QMessageBox>

#if defined(EXTERNAL_CRASH_REPORTING)
#include <CrashHandler.h>
#endif
#ifndef VERIFY
#define VERIFY(EXPRESSION) { auto e = EXPRESSION; assert(e); }
#endif

#undef GetCommandLine

namespace
{
    bool SelectionContainsComponentEntities()
    {
        bool result = false;
        CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
        if (pSelection)
        {
            CBaseObject* selectedObj = nullptr;
            for (int selectionCounter = 0; selectionCounter < pSelection->GetCount(); ++selectionCounter)
            {
                selectedObj = pSelection->GetObject(selectionCounter);
                if (selectedObj->GetType() == OBJTYPE_AZENTITY)
                {
                    result = true;
                    break;
                }
            }
        }
        return result;
    }
}

const char* CEditorImpl::m_crashLogFileName = "SessionStatus/editor_statuses.json";

CEditorImpl::CEditorImpl()
    : m_currEditMode(eEditModeSelect)
    , m_prevEditMode(eEditModeSelect)
    , m_operationMode(eOperationModeNone)
    , m_pSystem(nullptr)
    , m_pFileUtil(nullptr)
    , m_pClassFactory(nullptr)
    , m_pCommandManager(nullptr)
    , m_pObjectManager(nullptr)
    , m_pPluginManager(nullptr)
    , m_pViewManager(nullptr)
    , m_pUndoManager(nullptr)
    , m_marker(0, 0, 0)
    , m_selectedAxis(AXIS_TERRAIN)
    , m_refCoordsSys(COORDS_LOCAL)
    , m_bAxisVectorLock(false)
    , m_bUpdates(true)
    , m_bTerrainAxisIgnoreObjects(false)
    , m_pDisplaySettings(nullptr)
    , m_pShaderEnum(nullptr)
    , m_pIconManager(nullptr)
    , m_bSelectionLocked(true)
    , m_pPickTool(nullptr)
    , m_pAxisGizmo(nullptr)
    , m_pAIManager(nullptr)
    , m_pCustomActionsManager(nullptr)
    , m_pFlowGraphModuleManager(nullptr)
    , m_pMatFxGraphManager(nullptr)
    , m_pFlowGraphDebuggerEditor(nullptr)
    , m_pEquipPackLib(nullptr)
    , m_pGameEngine(nullptr)
    , m_pAnimationContext(nullptr)
    , m_pSequenceManager(nullptr)
    , m_pToolBoxManager(nullptr)
    , m_pEntityManager(nullptr)
    , m_pMaterialManager(nullptr)
    , m_particleManager(nullptr)
    , m_particleEditorUtils(nullptr)
    , m_pMusicManager(nullptr)
    , m_pPrefabManager(nullptr)
    , m_pGameTokenManager(nullptr)
    , m_pLensFlareManager(nullptr)
    , m_pErrorReport(nullptr)
    , m_pFileNameResolver(nullptr)
    , m_pLasLoadedLevelErrorReport(nullptr)
    , m_pErrorsDlg(nullptr)
    , m_pSourceControl(nullptr)
    , m_pAssetTagging(nullptr)
    , m_pFlowGraphManager(nullptr)
    , m_pSelectionTreeManager(nullptr)
    , m_pUIEnumsDatabase(nullptr)
    , m_pRuler(nullptr)
    , m_pScriptEnv(nullptr)
    , m_pConsoleSync(nullptr)
    , m_pSettingsManager(nullptr)
    , m_pLevelIndependentFileMan(nullptr)
    , m_pExportManager(nullptr)
    , m_pTerrainManager(nullptr)
    , m_pVegetationMap(nullptr)
    , m_awsResourceManager(nullptr)
    , m_bMatEditMode(false)
    , m_bShowStatusText(true)
    , m_bInitialized(false)
    , m_bExiting(false)
    , m_QtApplication(static_cast<Editor::EditorQtApplication*>(qApp))
    , m_pAssetBrowser(nullptr)
    , m_pImageUtil(nullptr)
    , m_pLogFile(nullptr)
{
    // note that this is a call into EditorCore.dll, which stores the g_pEditorPointer for all shared modules that share EditorCore.dll
    // this means that they don't need to do SetIEditor(...) themselves and its available immediately
    SetIEditor(this);

    m_pFileUtil = new CFileUtil_impl(); // Vladimir@Conffx
    m_pAssetBrowser = new CAssetBrowserImpl(); // Vladimir@Conffx
    m_pLogFile = new CLogFileImpl(); // Vladimir@Conffx
    m_pLevelIndependentFileMan = new CLevelIndependentFileMan;
    SetMasterCDFolder();
    gSettings.Load();
    m_pErrorReport = new CErrorReport;
    m_pFileNameResolver = new CMissingAssetResolver;
    m_pClassFactory = CClassFactory::Instance();
    m_pCommandManager = new CEditorCommandManager;
    CRegistrationContext regCtx;
    regCtx.pCommandManager = m_pCommandManager;
    regCtx.pClassFactory = m_pClassFactory;
    m_pEditorFileMonitor.reset(new CEditorFileMonitor());
    m_pBackgroundTaskManager.reset(new BackgroundTaskManager::CTaskManager);
    m_pBackgroundScheduleManager.reset(new BackgroundScheduleManager::CScheduleManager);
    m_pUIEnumsDatabase = new CUIEnumsDatabase;
    m_pDisplaySettings = new CDisplaySettings;
    m_pShaderEnum = new CShaderEnum;
    m_pDisplaySettings->LoadRegistry();
    m_pPluginManager = new CPluginManager;
    m_pTerrainManager = new CTerrainManager();
    m_pVegetationMap = new CVegetationMap();
    m_pObjectManager = new CObjectManager;
    m_pViewManager = new CViewManager;
    m_pIconManager = new CIconManager;
    m_pUndoManager = new CUndoManager;
    m_pAIManager = new CAIManager;
    m_pCustomActionsManager = new CCustomActionsEditorManager;
    m_pEquipPackLib = new CEquipPackLib;
    m_pToolBoxManager = new CToolBoxManager;
    m_pMaterialManager = new CMaterialManager(regCtx);
    m_pSequenceManager = new CTrackViewSequenceManager;
    m_pAnimationContext = new CAnimationContext;
    m_pEntityManager = new CEntityPrototypeManager;
    m_particleManager = new CEditorParticleManager;
    m_pPrefabManager = new CPrefabManager;
    m_pGameTokenManager = new CGameTokenManager;
    m_pFlowGraphManager = new CFlowGraphManager;

    m_pImageUtil = new CImageUtil_impl();
    m_particleEditorUtils = CreateEditorParticleUtils();
    m_pLensFlareManager = new CLensFlareManager;
    m_pFlowGraphModuleManager = new CEditorFlowGraphModuleManager;
    m_pFlowGraphDebuggerEditor  = new CFlowGraphDebuggerEditor;
    m_pMatFxGraphManager = new CMaterialFXGraphMan;
    m_pScriptEnv = new EditorScriptEnvironment();
    m_pResourceSelectorHost.reset(CreateResourceSelectorHost());
    m_pRuler = new CRuler;
    m_selectedRegion.min = Vec3(0, 0, 0);
    m_selectedRegion.max = Vec3(0, 0, 0);
    ZeroStruct(m_lastAxis);
    m_lastAxis[eEditModeSelect] = AXIS_TERRAIN;
    m_lastAxis[eEditModeSelectArea] = AXIS_TERRAIN;
    m_lastAxis[eEditModeMove] = AXIS_TERRAIN;
    m_lastAxis[eEditModeRotate] = AXIS_Z;
    m_lastAxis[eEditModeScale] = AXIS_XY;
    ZeroStruct(m_lastCoordSys);
    m_lastCoordSys[eEditModeSelect] = COORDS_LOCAL;
    m_lastCoordSys[eEditModeSelectArea] = COORDS_LOCAL;
    m_lastCoordSys[eEditModeMove] = COORDS_LOCAL;
    m_lastCoordSys[eEditModeRotate] = COORDS_LOCAL;
    m_lastCoordSys[eEditModeScale] = COORDS_LOCAL;
    DetectVersion();
    RegisterTools();

    m_winWidgetManager.reset(new WinWidget::WinWidgetManager);

    m_pAssetDatabaseLocationListener = nullptr;
    m_pAssetBrowserRequestHandler = nullptr;

    AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();

    AZ::IO::SystemFile::CreateDir("SessionStatus");
#ifdef KDAB_MAC_POR
    SetFileAttributes(m_crashLogFileName, FILE_ATTRIBUTE_NORMAL);
#endif
}

void CEditorImpl::Initialize()
{
#if defined(EXTERNAL_CRASH_REPORTING)
    InitCrashHandler("Editor", {});
#endif

    // Must be set before QApplication is initialized, so that we support HighDpi monitors, like the Retina displays
    // on Windows 10
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Activate QT immediately so that its available as soon as CEditorImpl is (and thus GetIEditor())
    InitializeEditorCommon(GetIEditor());
}

void CEditorImpl::Uninitialize()
{
    if (m_pSystem)
    {
        UninitializeEditorCommonISystem(m_pSystem);
        UninitializeEditorUIQTISystem(m_pSystem);
    }
    UninitializeEditorCommon();
    ShutdownCrashLog();
}

void CEditorImpl::UnloadPlugins()
{
    CryAutoLock<CryMutex> lock(m_pluginMutex);

    // Flush core buses. We're about to unload DLLs and need to ensure we don't have module-owned functions left behind.
    AZ::Data::AssetBus::ExecuteQueuedEvents();
    AZ::TickBus::ExecuteQueuedEvents();

    LyMetrics_Shutdown();

    // first, stop anyone from accessing plugins that provide things like source control.
    // note that m_psSourceControl is re-queried
    m_pSourceControl = nullptr;
    m_pAssetTagging = nullptr;

    // Send this message to ensure that any widgets queued for deletion will get deleted before their
    // plugin containing their vtable is unloaded. If not, access violations can occur
    QCoreApplication::sendPostedEvents(Q_NULLPTR, QEvent::DeferredDelete);

    GetPluginManager()->ReleaseAllPlugins();
    m_QtApplication->UninitializeQML(); // destroy QML first since it will hang onto memory inside the DLLs
    GetPluginManager()->UnloadAllPlugins();

    // since we mean to continue, so need to bring QML back up again in case someone needs it.
    m_QtApplication->InitializeQML();
}

void CEditorImpl::LoadPlugins()
{
    CryAutoLock<CryMutex> lock(m_pluginMutex);
    // plugins require QML, so make sure its present:

    m_QtApplication->InitializeQML();
#ifdef AZ_PLATFORM_WINDOWS
    GetPluginManager()->LoadPlugins(QDir::toNativeSeparators(qApp->applicationDirPath() + "/EditorPlugins/*.dll").toLatin1().data());
#else
    GetPluginManager()->LoadPlugins(QDir::toNativeSeparators(qApp->applicationDirPath() + "/EditorPlugins/*.dylib").toLatin1().data());
#endif

    InitMetrics();
}

QQmlEngine* CEditorImpl::GetQMLEngine() const
{
    if (!m_QtApplication)
    {
        CryFatalError("Attempt to get the QML engine when there isn't a Qt Application created.");
        return nullptr;
    }

    QQmlEngine* pEngine = m_QtApplication->GetQMLEngine();
    if (!pEngine)
    {
        CryFatalError("Attempt to get the QML engine when there isn't a QML engine in existence yet or it has already been destroyed.");
        return nullptr;
    }

    return pEngine;
}

CEditorImpl::~CEditorImpl()
{
    AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();

    gSettings.Save();
    m_bExiting = true; // Can't save level after this point (while Crash)
    SAFE_DELETE(m_pScriptEnv);
    SAFE_RELEASE(m_pSourceControl);
    SAFE_DELETE(m_pGameTokenManager);

    SAFE_DELETE(m_pMatFxGraphManager);
    SAFE_DELETE(m_pFlowGraphModuleManager);

    if (m_pFlowGraphDebuggerEditor)
    {
        m_pFlowGraphDebuggerEditor->Shutdown();
        SAFE_DELETE(m_pFlowGraphDebuggerEditor);
    }

    SAFE_DELETE(m_particleManager)
    SAFE_DELETE(m_pEntityManager)
    SAFE_DELETE(m_pMaterialManager)
    SAFE_DELETE(m_pEquipPackLib)
    SAFE_DELETE(m_pIconManager)
    SAFE_DELETE(m_pViewManager)
    SAFE_DELETE(m_pObjectManager) // relies on prefab manager
    SAFE_DELETE(m_pPrefabManager); // relies on flowgraphmanager
    SAFE_DELETE(m_pFlowGraphManager);
    SAFE_DELETE(m_pVegetationMap);
    SAFE_DELETE(m_pTerrainManager);
    // AI should be destroyed after the object manager, as the objects may
    // refer to AI components.
    SAFE_DELETE(m_pAIManager)
    SAFE_DELETE(m_pCustomActionsManager)

    // some plugins may be exporter - this must be above plugin manager delete.
    SAFE_DELETE(m_pExportManager);

    SAFE_DELETE(m_pPluginManager)
    SAFE_DELETE(m_pAnimationContext) // relies on undo manager
    SAFE_DELETE(m_pUndoManager)

    if (m_pDisplaySettings)
    {
        m_pDisplaySettings->SaveRegistry();
    }

    SAFE_DELETE(m_pDisplaySettings)
    SAFE_DELETE(m_pRuler)
    SAFE_DELETE(m_pShaderEnum)
    SAFE_DELETE(m_pToolBoxManager)
    SAFE_DELETE(m_pCommandManager)
    SAFE_DELETE(m_pClassFactory)
    SAFE_DELETE(m_pLasLoadedLevelErrorReport)
    SAFE_DELETE(m_pUIEnumsDatabase)

    SAFE_DELETE(m_pSettingsManager);

    SAFE_DELETE(m_pAssetDatabaseLocationListener);
    SAFE_DELETE(m_pAssetBrowserRequestHandler);

    // Game engine should be among the last things to be destroyed, as it
    // destroys the engine.
    SAFE_DELETE(m_pErrorsDlg);
    SAFE_DELETE(m_pLevelIndependentFileMan);
    SAFE_DELETE(m_pFileNameResolver);
    SAFE_DELETE(m_pGameEngine);
    // The error report must be destroyed after the game, as the engine
    // refers to the error report and the game destroys the engine.
    SAFE_DELETE(m_pErrorReport);

    SAFE_DELETE(m_pAssetBrowser); // Vladimir@Conffx
    SAFE_DELETE(m_pFileUtil); // Vladimir@Conffx
    SAFE_DELETE(m_pImageUtil); // Vladimir@Conffx
    SAFE_DELETE(m_particleEditorUtils); // Leroy@Conffx
    SAFE_DELETE(m_pLogFile); // Vladimir@Conffx
}

void CEditorImpl::SetMasterCDFolder()
{
    QString szFolder = qApp->applicationDirPath();

    // Remove Bin32/Bin64 folder/
    szFolder.remove(QRegularExpression(R"((\\|/)Bin32.*)"));

    szFolder.remove(QRegularExpression(QStringLiteral("(\\\\|/)%1.*").arg(QRegularExpression::escape(BINFOLDER_NAME))));


    m_masterCDFolder = QDir::toNativeSeparators(szFolder);

    if (!m_masterCDFolder.isEmpty())
    {
        if (m_masterCDFolder[m_masterCDFolder.length() - 1] != '\\')
        {
            m_masterCDFolder += '\\';
        }
    }

    QDir::setCurrent(szFolder);
}

void CEditorImpl::SetGameEngine(CGameEngine* ge)
{
    m_pAssetDatabaseLocationListener = new AssetDatabase::AssetDatabaseLocationListener();
    m_pAssetBrowserRequestHandler = new AzAssetBrowserRequestHandler();

    m_pSystem = ge->GetSystem();
    m_pGameEngine = ge;

    InitializeEditorCommonISystem(m_pSystem);
    InitializeEditorUIQTISystem(m_pSystem);

    m_templateRegistry.LoadTemplates("Editor");
    m_pObjectManager->LoadClassTemplates("Editor");

    m_pMaterialManager->Set3DEngine();
    m_pAnimationContext->Init();
}

void CEditorImpl::RegisterTools()
{
    CRegistrationContext rc;

    rc.pCommandManager = m_pCommandManager;
    rc.pClassFactory = m_pClassFactory;
    CTerrainModifyTool::RegisterTool(rc);
    CVegetationTool::RegisterTool(rc);
    CTerrainTexturePainter::RegisterTool(rc);
    CObjectMode::RegisterTool(rc);
    CSubObjectModeTool::RegisterTool(rc);
    CMaterialPickTool::RegisterTool(rc);
    CModellingModeTool::RegisterTool(rc);
    CVertexSnappingModeTool::RegisterTool(rc);
    CRotateTool::RegisterTool(rc);
}

void CEditorImpl::ExecuteCommand(const char* sCommand, ...)
{
    const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE];
    va_list args;

    buffer[BUF_SIZE - 1] = '\0';
    va_start(args, sCommand);
    vsprintf_s(buffer, BUF_SIZE, sCommand, args);
    va_end(args);
    m_pCommandManager->Execute(buffer);
}

void CEditorImpl::Update()
{
    if (!m_bUpdates)
    {
        return;
    }

    // Make sure this is not called recursively
    m_bUpdates = false;

    FUNCTION_PROFILER(GetSystem(), PROFILE_EDITOR);
    m_pRuler->Update();

    m_pFileNameResolver->PumpEvents();

    //@FIXME: Restore this latter.
    //if (GetGameEngine() && GetGameEngine()->IsLevelLoaded())
    {
        m_pObjectManager->Update();
    }
    if (IsInPreviewMode())
    {
        SetModifiedFlag(FALSE);
        SetModifiedModule(eModifiedNothing);
    }

    if (m_pGameEngine != NULL)
    {
        IEditorGame* pEditorGame = m_pGameEngine->GetIEditorGame();
        if (pEditorGame != NULL)
        {
            IEditorGame::HelpersDrawMode::EType helpersDrawMode = IEditorGame::HelpersDrawMode::Hide;
            if (m_pDisplaySettings->IsDisplayHelpers())
            {
                helpersDrawMode = IEditorGame::HelpersDrawMode::Show;
            }
            pEditorGame->UpdateHelpers(helpersDrawMode);
        }
    }

    m_bUpdates = true;
}

ISystem* CEditorImpl::GetSystem()
{
    return m_pSystem;
}

I3DEngine* CEditorImpl::Get3DEngine()
{
    if (gEnv)
    {
        return gEnv->p3DEngine;
    }
    return nullptr;
}

IRenderer*  CEditorImpl::GetRenderer()
{
    if (gEnv)
    {
        return gEnv->pRenderer;
    }
    return nullptr;
}

IGame*  CEditorImpl::GetGame()
{
    if (gEnv)
    {
        return gEnv->pGame;
    }
    return nullptr;
}

IEditorClassFactory* CEditorImpl::GetClassFactory()
{
    return m_pClassFactory;
}

CCryEditDoc* CEditorImpl::GetDocument() const
{
    return theDocument;
}

void CEditorImpl::SetDocument(CCryEditDoc* pDoc)
{
    theDocument = pDoc;
}

void CEditorImpl::SetModifiedFlag(bool modified)
{
    if (GetDocument() && GetDocument()->IsDocumentReady())
    {
        GetDocument()->SetModifiedFlag(modified);

        if (modified)
        {
            GetDocument()->SetLevelExported(false);
        }
    }
}

void CEditorImpl::SetModifiedModule(EModifiedModule eModifiedModule, bool boSet)
{
    if (GetDocument())
    {
        GetDocument()->SetModifiedModules(eModifiedModule, boSet);
    }
}

bool CEditorImpl::IsLevelExported() const
{
    CCryEditDoc* pDoc = GetDocument();

    if (pDoc)
    {
        return pDoc->IsLevelExported();
    }

    return false;
}

bool CEditorImpl::SetLevelExported(bool boExported)
{
    if (GetDocument())
    {
        GetDocument()->SetLevelExported(boExported);
        return true;
    }
    return false;
}

bool CEditorImpl::IsModified()
{
    if (GetDocument())
    {
        return GetDocument()->IsModified();
    }
    return false;
}

bool CEditorImpl::SaveDocument()
{
    if (m_bExiting)
    {
        return false;
    }

    if (GetDocument())
    {
        return GetDocument()->Save();
    }
    else
    {
        return false;
    }
}

QString CEditorImpl::GetMasterCDFolder()
{
    return m_masterCDFolder;
}

QString CEditorImpl::GetLevelFolder()
{
    return GetGameEngine()->GetLevelPath();
}

QString CEditorImpl::GetLevelName()
{
    m_levelNameBuffer = GetGameEngine()->GetLevelName();
    return m_levelNameBuffer;
}

QString CEditorImpl::GetLevelDataFolder()
{
    return Path::AddPathSlash(Path::AddPathSlash(GetGameEngine()->GetLevelPath()) + "LevelData");
}

QString CEditorImpl::GetSearchPath(EEditorPathName path)
{
    return gSettings.searchPaths[path][0];
}

QString CEditorImpl::GetUserFolder()
{
    m_userFolder = Path::GetUserSandboxFolder();
    return m_userFolder;
}

void CEditorImpl::SetDataModified()
{
    GetDocument()->SetModifiedFlag(TRUE);
}

void CEditorImpl::SetStatusText(const QString& pszString)
{
    if (m_bShowStatusText && !m_bMatEditMode && GetMainStatusBar())
    {
        GetMainStatusBar()->SetStatusText(pszString);
    }
}

IMainStatusBar* CEditorImpl::GetMainStatusBar()
{
    return MainWindow::instance()->StatusBar();
}

int CEditorImpl::SelectRollUpBar(int rollupBarId)
{
    return MainWindow::instance()->SelectRollUpBar(rollupBarId);
}

int CEditorImpl::AddRollUpPage(
    int rollbarId,
    const QString& pszCaption,
    QWidget* pwndTemplate,
    int iIndex /*= -1*/,
    bool bAutoExpand /*= true*/)
{
    if (!GetRollUpControl(rollbarId))
    {
        return 0;
    }

    // Preserve Focused window.
#ifdef KDAB_MAC_PORT
    HWND hFocusWnd = GetFocus();
#endif // KDAB_MAC_PORT
    int ndx = GetRollUpControl(rollbarId)->insertItem(iIndex, pwndTemplate, pszCaption);
    if (!bAutoExpand)
    {
        GetRollUpControl(rollbarId)->setIndexVisible(ndx, false);
    }

    int id = 1;
    assert(m_panelIds.key(pwndTemplate, -1) == -1);
    if (!m_panelIds.isEmpty())
    {
        id = m_panelIds.lastKey() + 1;
    }
    m_panelIds.insert(id, pwndTemplate);

    // Make sure focus stay in main wnd.
#ifdef KDAB_MAC_PORT
    if (hFocusWnd && GetFocus() != hFocusWnd)
    {
        SetFocus(hFocusWnd);
    }
#endif // KDAB_MAC_PORT
    return id;
}

void CEditorImpl::RemoveRollUpPage(int rollbarId, int iIndex)
{
    if (GetRollUpControl(rollbarId))
    {
        QWidget* w = m_panelIds.value(iIndex);
        m_panelIds.remove(iIndex);
        GetRollUpControl(rollbarId)->removeItem(w);
        w->deleteLater();
    }
}

void CEditorImpl::RenameRollUpPage(int rollbarId, int iIndex, const char* pNewName)
{
    if (GetRollUpControl(rollbarId))
    {
        GetRollUpControl(rollbarId)->setItemText(GetRollUpControl(rollbarId)->indexOf(m_panelIds.value(iIndex)), pNewName);
    }
}

void CEditorImpl::ExpandRollUpPage(int rollbarId, int iIndex, bool bExpand)
{
    // Preserve Focused window.
#ifdef KDAB_MAC_PORT
    HWND hFocusWnd = GetFocus();
#endif // KDAB_MAC_PORT

    if (GetRollUpControl(rollbarId))
    {
        GetRollUpControl(rollbarId)->setWidgetVisible(m_panelIds.value(iIndex), bExpand);
    }

    // Preserve Focused window.
#ifdef KDAB_MAC_PORT
    if (hFocusWnd && GetFocus() != hFocusWnd)
    {
        SetFocus(hFocusWnd);
    }
#endif // KDAB_MAC_PORT
}

void CEditorImpl::EnableRollUpPage(int rollbarId, int iIndex, bool bEnable)
{
    // Preserve Focused window.
#ifdef KDAB_MAC_PORT
    HWND hFocusWnd = GetFocus();
#endif // KDAB_MAC_PORT

    if (GetRollUpControl(rollbarId))
    {
        GetRollUpControl(rollbarId)->setItemEnabled(GetRollUpControl(rollbarId)->indexOf(m_panelIds.value(iIndex)), bEnable);
    }

    // Preserve Focused window.
#ifdef KDAB_MAC_PORT
    if (hFocusWnd && GetFocus() != hFocusWnd)
    {
        SetFocus(hFocusWnd);
    }
#endif // KDAB_MAC_PORT
}

int CEditorImpl::GetRollUpPageCount(int rollbarId)
{
    if (GetRollUpControl(rollbarId))
    {
        return GetRollUpControl(rollbarId)->count();
    }
    return 0;
}

int CEditorImpl::GetEditMode()
{
    return m_currEditMode;
}

void CEditorImpl::SetEditMode(int editMode)
{
    bool isEditorInGameMode = false;
    EBUS_EVENT_RESULT(isEditorInGameMode, AzToolsFramework::EditorEntityContextRequestBus, IsEditorRunningGame);

    if (isEditorInGameMode)
    {
        if (editMode != eEditModeSelect)
        {
            if (SelectionContainsComponentEntities())
            {
                return;
            }
        }
    }

    if ((EEditMode)editMode == eEditModeRotate)
    {
        if (GetEditTool() && GetEditTool()->IsCircleTypeRotateGizmo())
        {
            editMode = eEditModeRotateCircle;
        }
    }

    m_currEditMode = (EEditMode)editMode;
    m_prevEditMode = m_currEditMode;
    AABB box(Vec3(0, 0, 0), Vec3(0, 0, 0));
    SetSelectedRegion(box);

    if (GetEditTool() && !GetEditTool()->IsNeedMoveTool())
    {
        SetEditTool(0, true);
    }

    if (editMode == eEditModeMove || editMode == eEditModeRotate || editMode == eEditModeScale)
    {
        SetAxisConstraints(m_lastAxis[editMode]);
        SetReferenceCoordSys(m_lastCoordSys[editMode]);
    }

    if (editMode == eEditModeRotateCircle)
    {
        SetReferenceCoordSys(COORDS_LOCAL);
    }

    Notify(eNotify_OnEditModeChange);
}

void CEditorImpl::SetOperationMode(EOperationMode mode)
{
    m_operationMode = mode;
    gSettings.operationMode = mode;
}

EOperationMode CEditorImpl::GetOperationMode()
{
    return m_operationMode;
}

bool CEditorImpl::HasCorrectEditTool() const
{
    if (!m_pEditTool)
    {
        return false;
    }

    switch (m_currEditMode)
    {
    case eEditModeRotate:
        return qobject_cast<CRotateTool*>(m_pEditTool) != nullptr;
    default:
        return qobject_cast<CObjectMode*>(m_pEditTool) != nullptr && qobject_cast<CRotateTool*>(m_pEditTool) == nullptr;
    }
}

CEditTool* CEditorImpl::CreateCorrectEditTool()
{
    if (m_currEditMode == eEditModeRotate)
    {
        CBaseObject* selectedObj = nullptr;
        CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
        if (pSelection && pSelection->GetCount() > 0)
        {
            selectedObj = pSelection->GetObject(0);
        }

        return (new CRotateTool(selectedObj));
    }

    return (new CObjectMode);
}

void CEditorImpl::SetEditTool(CEditTool* tool, bool bStopCurrentTool)
{
    CViewport* pViewport = GetIEditor()->GetActiveView();
    if (pViewport)
    {
        pViewport->SetCurrentCursor(STD_CURSOR_DEFAULT);
    }

    if (!tool)
    {
        if (HasCorrectEditTool())
        {
            return;
        }
        else
        {
            tool = CreateCorrectEditTool();
        }
    }

    if (!tool->Activate(m_pEditTool))
    {
        return;
    }

    if (bStopCurrentTool)
    {
        if (m_pEditTool && m_pEditTool != tool)
        {
            m_pEditTool->EndEditParams();
            SetStatusText("Ready");
        }
    }

    m_pEditTool = tool;
    if (m_pEditTool)
    {
        m_pEditTool->BeginEditParams(this, 0);
    }

    // Make sure pick is aborted.
    if (tool != m_pPickTool)
    {
        m_pPickTool = nullptr;
    }
    Notify(eNotify_OnEditToolChange);
}

void CEditorImpl::ReinitializeEditTool()
{
    if (m_pEditTool)
    {
        m_pEditTool->EndEditParams();
        m_pEditTool->BeginEditParams(this, 0);
    }
}

void CEditorImpl::SetEditTool(const QString& sEditToolName, bool bStopCurrentTool)
{
    CEditTool* pTool = GetEditTool();
    if (pTool && pTool->GetClassDesc())
    {
        // Check if already selected.
        if (QString::compare(pTool->GetClassDesc()->ClassName(), sEditToolName, Qt::CaseInsensitive) == 0)
        {
            return;
        }
    }

    IClassDesc* pClass = GetIEditor()->GetClassFactory()->FindClass(sEditToolName.toLatin1().data());
    if (!pClass)
    {
        Warning("Editor Tool %s not registered.", sEditToolName.toLatin1().data());
        return;
    }
    if (pClass->SystemClassID() != ESYSTEM_CLASS_EDITTOOL)
    {
        Warning("Class name %s is not a valid Edit Tool class.", sEditToolName.toLatin1().data());
        return;
    }

    QScopedPointer<QObject> o(pClass->CreateQObject());
    if (CEditTool* pEditTool = qobject_cast<CEditTool*>(o.data()))
    {
        GetIEditor()->SetEditTool(pEditTool);
        o.take();
        return;
    }
    else
    {
        Warning("Class name %s is not a valid Edit Tool class.", sEditToolName.toLatin1().data());
        return;
    }
}

CEditTool* CEditorImpl::GetEditTool()
{
    return m_pEditTool;
}

ITransformManipulator* CEditorImpl::ShowTransformManipulator(bool bShow)
{
    if (bShow)
    {
        if (!m_pAxisGizmo)
        {
            m_pAxisGizmo = new CAxisGizmo;
            m_pAxisGizmo->AddRef();
            GetObjectManager()->GetGizmoManager()->AddGizmo(m_pAxisGizmo);
        }
        return m_pAxisGizmo;
    }
    else
    {
        // Hide gizmo.
        if (m_pAxisGizmo)
        {
            GetObjectManager()->GetGizmoManager()->RemoveGizmo(m_pAxisGizmo);
            m_pAxisGizmo->Release();
        }
        m_pAxisGizmo = 0;
    }
    return 0;
}

ITransformManipulator* CEditorImpl::GetTransformManipulator()
{
    return m_pAxisGizmo;
}

void CEditorImpl::SetAxisConstraints(AxisConstrains axisFlags)
{
    m_selectedAxis = axisFlags;
    m_lastAxis[m_currEditMode] = m_selectedAxis;
    m_pViewManager->SetAxisConstrain(axisFlags);
    SetTerrainAxisIgnoreObjects(false);

    // Update all views.
    UpdateViews(eUpdateObjects, NULL);
}

AxisConstrains CEditorImpl::GetAxisConstrains()
{
    return m_selectedAxis;
}

void CEditorImpl::SetTerrainAxisIgnoreObjects(bool bIgnore)
{
    m_bTerrainAxisIgnoreObjects = bIgnore;
}

bool CEditorImpl::IsTerrainAxisIgnoreObjects()
{
    return m_bTerrainAxisIgnoreObjects;
}

void CEditorImpl::SetReferenceCoordSys(RefCoordSys refCoords)
{
    m_refCoordsSys = refCoords;
    m_lastCoordSys[m_currEditMode] = m_refCoordsSys;

    // Update all views.
    UpdateViews(eUpdateObjects, NULL);

    // Update the construction plane infos.
    CViewport* pViewport = GetActiveView();
    if (pViewport)
    {
        pViewport->MakeConstructionPlane(GetIEditor()->GetAxisConstrains());
    }

    Notify(eNotify_OnRefCoordSysChange);
}

RefCoordSys CEditorImpl::GetReferenceCoordSys()
{
    return m_refCoordsSys;
}

CBaseObject* CEditorImpl::NewObject(const char* typeName, const char* fileName, const char* name, float x, float y, float z, bool modifyDoc)
{
    CUndo undo("Create new object");

    IEditor* editor = GetIEditor();
    if (modifyDoc)
    {
        editor->SetModifiedFlag();
        editor->SetModifiedModule(eModifiedBrushes);
    }
    CBaseObject* object = editor->GetObjectManager()->NewObject(typeName, 0, fileName);
    if (!object)
    {
        return nullptr;
    }
    if (name && name[0])
    {
        object->SetName(name);
    }
    object->SetPos(Vec3(x, y, z));

    return object;
}

const SGizmoParameters& CEditorImpl::GetGlobalGizmoParameters()
{
    if (!m_pGizmoParameters.get())
    {
        m_pGizmoParameters.reset(new SGizmoParameters());
    }

    m_pGizmoParameters->axisConstraint = m_selectedAxis;
    m_pGizmoParameters->referenceCoordSys = m_refCoordsSys;
    m_pGizmoParameters->axisGizmoScale = gSettings.gizmo.axisGizmoSize;
    m_pGizmoParameters->axisGizmoText = gSettings.gizmo.axisGizmoText;

    return *m_pGizmoParameters;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DeleteObject(CBaseObject* obj)
{
    SetModifiedFlag();
    GetIEditor()->SetModifiedModule(eModifiedBrushes);
    GetObjectManager()->DeleteObject(obj);
}

CBaseObject* CEditorImpl::CloneObject(CBaseObject* obj)
{
    SetModifiedFlag();
    GetIEditor()->SetModifiedModule(eModifiedBrushes);
    return GetObjectManager()->CloneObject(obj);
}

void CEditorImpl::StartObjectCreation(const QString& type, const QString& file)
{
    if (!GetDocument()->IsDocumentReady())
    {
        return;
    }

    CObjectCreateTool* pTool = new CObjectCreateTool;
    GetIEditor()->SetEditTool(pTool);
    pTool->StartCreation(type, file);
}

CBaseObject* CEditorImpl::GetSelectedObject()
{
    CBaseObject* obj = nullptr;
    if (m_pObjectManager->GetSelection()->GetCount() != 1)
    {
        return nullptr;
    }
    return m_pObjectManager->GetSelection()->GetObject(0);
}

void CEditorImpl::SelectObject(CBaseObject* obj)
{
    GetObjectManager()->SelectObject(obj);
}

IObjectManager* CEditorImpl::GetObjectManager()
{
    return m_pObjectManager;
};

CSettingsManager* CEditorImpl::GetSettingsManager()
{
    // Do not go any further before XML class is ready to use
    if (!gEnv)
    {
        return nullptr;
    }

    if (!GetISystem())
    {
        return nullptr;
    }

    if (!m_pSettingsManager)
    {
        m_pSettingsManager = new CSettingsManager(eSettingsManagerMemoryStorage);
    }

    return m_pSettingsManager;
}

CSelectionGroup* CEditorImpl::GetSelection()
{
    return m_pObjectManager->GetSelection();
}

int CEditorImpl::ClearSelection()
{
    if (GetSelection()->IsEmpty())
    {
        return 0;
    }
    string countString = GetCommandManager()->Execute("general.clear_selection");
    int count = 0;
    FromString(count, countString.c_str());
    return count;
}

void CEditorImpl::LockSelection(bool bLock)
{
    // Selection must be not empty to enable selection lock.
    if (!GetSelection()->IsEmpty())
    {
        m_bSelectionLocked = bLock;
    }
    else
    {
        m_bSelectionLocked = false;
    }
}

bool CEditorImpl::IsSelectionLocked()
{
    return m_bSelectionLocked;
}

void CEditorImpl::PickObject(IPickObjectCallback* callback, const QMetaObject* targetClass, const char* statusText, bool bMultipick)
{
    m_pPickTool = new CPickObjectTool(callback, targetClass);
    ((CPickObjectTool*)m_pPickTool)->SetMultiplePicks(bMultipick);
    if (statusText)
    {
        m_pPickTool->SetStatusText(statusText);
    }

    SetEditTool(m_pPickTool);
}

void CEditorImpl::CancelPick()
{
    SetEditTool(0);
    m_pPickTool = 0;
}

bool CEditorImpl::IsPicking()
{
    if (GetEditTool() == m_pPickTool && m_pPickTool != 0)
    {
        return true;
    }
    return false;
}

CViewManager* CEditorImpl::GetViewManager()
{
    return m_pViewManager;
}

CViewport* CEditorImpl::GetActiveView()
{
    CLayoutViewPane* viewPane = MainWindow::instance()->GetActiveView();
    if (viewPane)
    {
        return qobject_cast<QtViewport*>(viewPane->GetViewport());
    }
    return nullptr;
}

void CEditorImpl::SetActiveView(CViewport* viewport)
{
    m_pViewManager->SelectViewport(viewport);
}

void CEditorImpl::UpdateViews(int flags, const AABB* updateRegion)
{
    AABB prevRegion = m_pViewManager->GetUpdateRegion();
    if (updateRegion)
    {
        m_pViewManager->SetUpdateRegion(*updateRegion);
    }
    m_pViewManager->UpdateViews(flags);
    if (updateRegion)
    {
        m_pViewManager->SetUpdateRegion(prevRegion);
    }
}

void CEditorImpl::ReloadTrackView()
{
    Notify(eNotify_OnReloadTrackView);
}

void CEditorImpl::UpdateSequencer(bool bOnlyKeys)
{
    if (bOnlyKeys)
    {
        Notify(eNotify_OnUpdateSequencerKeys);
    }
    else
    {
        Notify(eNotify_OnUpdateSequencer);
    }
}

void CEditorImpl::ResetViews()
{
    m_pViewManager->ResetViews();

    m_pDisplaySettings->SetRenderFlags(m_pDisplaySettings->GetRenderFlags());
}

IIconManager* CEditorImpl::GetIconManager()
{
    return m_pIconManager;
}

IBackgroundTaskManager* CEditorImpl::GetBackgroundTaskManager()
{
    return m_pBackgroundTaskManager.get();
}

IBackgroundScheduleManager* CEditorImpl::GetBackgroundScheduleManager()
{
    return m_pBackgroundScheduleManager.get();
}

IEditorFileMonitor* CEditorImpl::GetFileMonitor()
{
    return m_pEditorFileMonitor.get();
}

void CEditorImpl::RegisterEventLoopHook(IEventLoopHook* pHook)
{
    CCryEditApp::instance()->RegisterEventLoopHook(pHook);
}

void CEditorImpl::UnregisterEventLoopHook(IEventLoopHook* pHook)
{
    CCryEditApp::instance()->UnregisterEventLoopHook(pHook);
}

void CEditorImpl::LaunchAWSConsole(QString destUrl)
{
    CCryEditApp::instance()->OnAWSLaunchConsolePage(destUrl.toStdString().c_str());
}

bool CEditorImpl::ToProjectConfigurator(const char* msg, const char* caption, const char* location)
{
    return CCryEditApp::instance()->ToProjectConfigurator(msg, caption, location);
}

float CEditorImpl::GetTerrainElevation(float x, float y)
{
    I3DEngine* engine = m_pSystem->GetI3DEngine();
    if (!engine)
    {
        return 0;
    }
    return engine->GetTerrainElevation(x, y);
}

CHeightmap* CEditorImpl::GetHeightmap()
{
    assert(m_pTerrainManager);
    return m_pTerrainManager->GetHeightmap();
}

CVegetationMap* CEditorImpl::GetVegetationMap()
{
    return m_pVegetationMap;
}

const QColor& CEditorImpl::GetColorByName(const QString& name)
{
    return m_QtApplication->GetColorByName(name);
}

void CEditorImpl::SetSelectedRegion(const AABB& box)
{
    m_selectedRegion = box;
}

void CEditorImpl::GetSelectedRegion(AABB& box)
{
    box = m_selectedRegion;
}

const QtViewPane* CEditorImpl::OpenView(QString sViewClassName, bool reuseOpened)
{
    auto openMode = reuseOpened ? QtViewPane::OpenMode::None : QtViewPane::OpenMode::MultiplePanes;
    return QtViewPaneManager::instance()->OpenPane(sViewClassName, openMode);
}

QWidget* CEditorImpl::OpenWinWidget(WinWidgetId openId)
{
    if (m_winWidgetManager)
    {
        return m_winWidgetManager->OpenWinWidget(openId);
    }
    return nullptr;
}

WinWidget::WinWidgetManager* CEditorImpl::GetWinWidgetManager() const
{
    return m_winWidgetManager.get();
}

QWidget* CEditorImpl::FindView(QString viewClassName)
{
    return QtViewPaneManager::instance()->GetView(viewClassName);
}

// Intended to give a window focus only if it is currently open
bool CEditorImpl::SetViewFocus(const char* sViewClassName)
{
    QWidget* findWindow = FindView(sViewClassName);
    if (findWindow)
    {
        findWindow->setFocus(Qt::OtherFocusReason);
        return true;
    }
    return false;
}

bool CEditorImpl::CloseView(const char* sViewClassName)
{
    return QtViewPaneManager::instance()->ClosePane(sViewClassName);
}

void CEditorImpl::CloseView(const GUID& classId)
{
    IClassDesc* found = GetClassFactory()->FindClass(classId);
    if (found)
    {
        CloseView(found->ClassName().toLatin1().data());
    }
}

IDataBaseManager* CEditorImpl::GetDBItemManager(EDataBaseItemType itemType)
{
    switch (itemType)
    {
    case EDB_TYPE_MATERIAL:
        return m_pMaterialManager;
    case EDB_TYPE_ENTITY_ARCHETYPE:
        return m_pEntityManager;
    case EDB_TYPE_PREFAB:
        return m_pPrefabManager;
    case EDB_TYPE_GAMETOKEN:
        return m_pGameTokenManager;
    case EDB_TYPE_PARTICLE:
        return m_particleManager;
    }
    return 0;
}

CBaseLibraryDialog* CEditorImpl::OpenDataBaseLibrary(EDataBaseItemType type, IDataBaseItem* pItem)
{
    if (pItem)
    {
        type = pItem->GetType();
    }

    const QtViewPane* pane = nullptr;
    if (type == EDB_TYPE_MATERIAL)
    {
        pane = QtViewPaneManager::instance()->OpenPane(LyViewPane::MaterialEditor);

        // This is a workaround for a timing issue where the material editor
        // gets in a bad state while it is being polished for the first time
        // while loading a material at the same time, so delay the setting
        // of the material until the next event queue check
        QTimer::singleShot(0, [this, type, pItem] {
                IDataBaseManager* pManager = GetDBItemManager(type);
                if (pManager)
                {
                    pManager->SetSelectedItem(pItem);
                }
            });
    }
    else
    {
        pane = QtViewPaneManager::instance()->OpenPane(LyViewPane::DatabaseView);

        IDataBaseManager* pManager = GetDBItemManager(type);
        if (pManager)
        {
            pManager->SetSelectedItem(pItem);
        }
    }

    if (!pane)
    {
        return nullptr;
    }

    if (auto dlgDB = qobject_cast<CDataBaseDialog*>(pane->Widget()))
    {
        CDataBaseDialogPage* pPage = dlgDB->SelectDialog(type, pItem);
        if (pPage)
        {
            return qobject_cast<CBaseLibraryDialog*>(pPage);
        }
    }
    return nullptr;
}

bool CEditorImpl::SelectColor(QColor& color, QWidget* parent)
{
    QColorDialog dlg(color, parent);
    if (dlg.exec() == QDialog::Accepted)
    {
        color = dlg.currentColor();
        return true;
    }
    return false;
}

void CEditorImpl::SetInGameMode(bool inGame)
{
    static bool bWasInSimulationMode(false);

    if (inGame)
    {
        bWasInSimulationMode = GetIEditor()->GetGameEngine()->GetSimulationMode();
        GetIEditor()->GetGameEngine()->SetSimulationMode(false);
        GetIEditor()->GetCommandManager()->Execute("general.enter_game_mode");
    }
    else
    {
        GetIEditor()->GetCommandManager()->Execute("general.exit_game_mode");
        GetIEditor()->GetGameEngine()->SetSimulationMode(bWasInSimulationMode);
    }
}

bool CEditorImpl::IsInGameMode()
{
    if (m_pGameEngine)
    {
        return m_pGameEngine->IsInGameMode();
    }
    return false;
}

bool CEditorImpl::IsInTestMode()
{
    return CCryEditApp::instance()->IsInTestMode();
}

bool CEditorImpl::IsInConsolewMode()
{
    return CCryEditApp::instance()->IsInConsoleMode();
}

bool CEditorImpl::IsInLevelLoadTestMode()
{
    return CCryEditApp::instance()->IsInLevelLoadTestMode();
}

bool CEditorImpl::IsInPreviewMode()
{
    return CCryEditApp::instance()->IsInPreviewMode();
}

void CEditorImpl::EnableAcceleratos(bool bEnable)
{
    KeyboardCustomizationSettings::EnableShortcutsGlobally(bEnable);
}

void CEditorImpl::InitMetrics()
{
    QString fileToCheck = "project.json";
    bool fullPathfound = false;

    // get the full path of the project.json
    AZStd::string fullPath;
    AZStd::string relPath(fileToCheck.toUtf8().data());
    EBUS_EVENT_RESULT(fullPathfound, AzToolsFramework::AssetSystemRequestBus, GetFullSourcePathFromRelativeProductPath, relPath, fullPath);

    QFile file(fullPath.c_str());

    QByteArray fileContents;
    AZStd::string str;
    const char* projectId = "";

    if (file.open(QIODevice::ReadOnly))
    {
        // Read the project.json file using its full path
        fileContents = file.readAll();
        file.close();

        rapidjson::Document projectCfg;
        projectCfg.Parse(fileContents);

        if (projectCfg.IsObject())
        {
            // get the project Id and project name from the project.json file
            QString projectName = projectCfg["project_name"].IsString() ? projectCfg["project_name"].GetString() : "";

            if (projectCfg.HasMember("project_id") && projectCfg["project_id"].IsString())
            {
                projectId = projectCfg["project_id"].GetString();
            }

            QFileInfo fileInfo(fullPath.c_str());
            QDir folderDirectory = fileInfo.dir();

            // get the project name from the folder directory
            QString editorProjectName = folderDirectory.dirName();

            // get the project Id generated by using the project name from the folder directory
            QByteArray editorProjectNameUtf8 = editorProjectName.toUtf8();
            AZ::Uuid id = AZ::Uuid::CreateName(editorProjectNameUtf8.constData());


            // The projects that Lumberyard ships with had their project IDs hand-generated based on the name of the level.
            // Therefore, if the UUID from the project name is the same as the UUID in the file, it's one of our projects
            // and we can therefore send the name back, making it easier for Metrics to determine which level it was.
            // We are checking to see if this is a project we ship with Lumberyard, and therefore we can unobfuscate non-customer information.
            if (projectId && editorProjectName.compare(projectName, Qt::CaseInsensitive) == 0 &&
                (id == AZ::Uuid(projectId)))
            {
                QByteArray projectNameUtf8 = projectName.toUtf8();

                str += projectId;
                str += " [";
                str += projectNameUtf8.constData();
                str += "]";
            }


            projectId = (str.length() != 0) ? str.c_str() : projectId;
        }
    }

    static char statusFilePath[_MAX_PATH + 1];
    {
        azstrcpy(statusFilePath, _MAX_PATH, AZ::IO::FileIOBase::GetInstance()->GetAlias("@devroot@"));
        azstrcat(statusFilePath, _MAX_PATH, &(Aws::FileSystem::PATH_DELIM));
        azstrcat(statusFilePath, _MAX_PATH, m_crashLogFileName);
    }

    const bool doSDKInitShutdown = false;
    Aws::Utils::Crypto::InitCrypto();
    LyMetrics_Initialize("Editor.exe", 2, doSDKInitShutdown, projectId, statusFilePath);
}

void CEditorImpl::DetectVersion()
{
#ifdef KDAB_MAC_PORT
    char exe[_MAX_PATH];
    DWORD dwHandle;
    UINT len;

    char ver[1024 * 8];

    GetModuleFileName(NULL, exe, _MAX_PATH);

    int verSize = GetFileVersionInfoSize(exe, &dwHandle);
    if (verSize > 0)
    {
        GetFileVersionInfo(exe, dwHandle, 1024 * 8, ver);
        VS_FIXEDFILEINFO* vinfo;
        VerQueryValue(ver, "\\", (void**)&vinfo, &len);

        m_fileVersion.v[0] = vinfo->dwFileVersionLS & 0xFFFF;
        m_fileVersion.v[1] = vinfo->dwFileVersionLS >> 16;
        m_fileVersion.v[2] = vinfo->dwFileVersionMS & 0xFFFF;
        m_fileVersion.v[3] = vinfo->dwFileVersionMS >> 16;

        m_productVersion.v[0] = vinfo->dwProductVersionLS & 0xFFFF;
        m_productVersion.v[1] = vinfo->dwProductVersionLS >> 16;
        m_productVersion.v[2] = vinfo->dwProductVersionMS & 0xFFFF;
        m_productVersion.v[3] = vinfo->dwProductVersionMS >> 16;
    }
#endif // KDAB_MAC_PORT
}

XmlNodeRef CEditorImpl::FindTemplate(const QString& templateName)
{
    return m_templateRegistry.FindTemplate(templateName);
}

void CEditorImpl::AddTemplate(const QString& templateName, XmlNodeRef& tmpl)
{
    m_templateRegistry.AddTemplate(templateName, tmpl);
}

CShaderEnum* CEditorImpl::GetShaderEnum()
{
    return m_pShaderEnum;
}

bool CEditorImpl::ExecuteConsoleApp(const QString& CommandLine, QString& OutputText, bool bNoTimeOut, bool bShowWindow)
{
#ifdef KDAB_MAC_PORT
    // Execute a console application and redirect its output to the console window
    SECURITY_ATTRIBUTES sa = { 0 };
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    HANDLE hPipeOutputRead = NULL;
    HANDLE hPipeOutputWrite = NULL;
    HANDLE hPipeInputRead = NULL;
    HANDLE hPipeInputWrite = NULL;
    BOOL bTest = FALSE;
    bool bReturn = true;
    DWORD dwNumberOfBytesRead = 0;
    DWORD dwStartTime = 0;
    char szCharBuffer[65];
    char szOEMBuffer[65];

    CLogFile::FormatLine("Executing console application '%s'", CommandLine.toLatin1().data());
    // Initialize the SECURITY_ATTRIBUTES structure
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    // Create a pipe for standard output redirection
    VERIFY(CreatePipe(&hPipeOutputRead, &hPipeOutputWrite, &sa, 0));
    // Create a pipe for standard inout redirection
    VERIFY(CreatePipe(&hPipeInputRead, &hPipeInputWrite, &sa, 0));
    // Make a child process useing hPipeOutputWrite as standard out. Also
    // make sure it is not shown on the screen
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW;

    if (bShowWindow == false)
    {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hPipeInputRead;
        si.hStdOutput = hPipeOutputWrite;
        si.hStdError = hPipeOutputWrite;
    }

    si.wShowWindow = bShowWindow ? SW_SHOW : SW_HIDE;
    // Save the process start time
    dwStartTime = GetTickCount();

    // Launch the console application
    char cmdLine[AZ_COMMAND_LINE_LEN];
    azstrncpy(cmdLine, AZ_COMMAND_LINE_LEN, CommandLine.toLatin1().data(), CommandLine.toLatin1().length() - 1);

    if (!CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        return false;
    }

    // If no process was spawned
    if (!pi.hProcess)
    {
        bReturn = false;
    }

    // Now that the handles have been inherited, close them
    CloseHandle(hPipeOutputWrite);
    CloseHandle(hPipeInputRead);

    if (bShowWindow == false)
    {
        // Capture the output of the console application by reading from hPipeOutputRead
        while (true)
        {
            // Read from the pipe
            bTest = ReadFile(hPipeOutputRead, &szOEMBuffer, 64, &dwNumberOfBytesRead, NULL);

            // Break when finished
            if (!bTest)
            {
                break;
            }

            // Break when timeout has been exceeded
            if (bNoTimeOut == false && GetTickCount() - dwStartTime > 5000)
            {
                break;
            }

            // Null terminate string
            szOEMBuffer[dwNumberOfBytesRead] = '\0';

            // Translate into ANSI
            VERIFY(OemToChar(szOEMBuffer, szCharBuffer));

            // Add it to the output text
            OutputText += szCharBuffer;
        }
    }

    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, bNoTimeOut ? INFINITE : 1000);

    return bReturn;
#else
    return false;
#endif // KDAB_MAC_PORT
}

void CEditorImpl::BeginUndo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Begin();
    }
}

void CEditorImpl::RestoreUndo(bool undo)
{
    if (m_pPrefabManager)
    {
        m_pPrefabManager->SetSkipPrefabUpdate(true);
    }
    if (m_pUndoManager)
    {
        m_pUndoManager->Restore(undo);
    }
    if (m_pPrefabManager)
    {
        m_pPrefabManager->SetSkipPrefabUpdate(false);
    }
}

void CEditorImpl::AcceptUndo(const QString& name)
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Accept(name);
    }
}

void CEditorImpl::CancelUndo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Cancel();
    }
}

void CEditorImpl::SuperBeginUndo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->SuperBegin();
    }
}

void CEditorImpl::SuperAcceptUndo(const QString& name)
{
    if (m_pUndoManager)
    {
        m_pUndoManager->SuperAccept(name);
    }
}

void CEditorImpl::SuperCancelUndo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->SuperCancel();
    }
}

void CEditorImpl::SuspendUndo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Suspend();
    }
}

void CEditorImpl::ResumeUndo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Resume();
    }
}

void CEditorImpl::Undo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Undo();
    }
}

void CEditorImpl::Redo()
{
    if (m_pUndoManager)
    {
        m_pUndoManager->Redo();
    }
}

bool CEditorImpl::IsUndoRecording()
{
    if (m_pUndoManager)
    {
        return m_pUndoManager->IsUndoRecording();
    }
    return false;
}

bool CEditorImpl::IsUndoSuspended()
{
    if (m_pUndoManager)
    {
        return m_pUndoManager->IsUndoSuspended();
    }
    return false;
}

void CEditorImpl::RecordUndo(IUndoObject* obj)
{
    if (m_pUndoManager)
    {
        m_pUndoManager->RecordUndo(obj);
    }
}

bool CEditorImpl::FlushUndo(bool isShowMessage)
{
    if (isShowMessage && m_pUndoManager && m_pUndoManager->IsHaveUndo() && QMessageBox::question(nullptr, QString(), QObject::tr("After this operation undo will not be available! Are you sure you want to continue?")) != QMessageBox::Yes)
    {
        return false;
    }

    if (m_pUndoManager)
    {
        m_pUndoManager->Flush();
    }
    return true;
}

void CEditorImpl::SetConsoleVar(const char* var, float value)
{
    ICVar* ivar = GetSystem()->GetIConsole()->GetCVar(var);
    if (ivar)
    {
        ivar->Set(value);
    }
}

float CEditorImpl::GetConsoleVar(const char* var)
{
    ICVar* ivar = GetSystem()->GetIConsole()->GetCVar(var);
    if (ivar)
    {
        return ivar->GetFVal();
    }
    return 0;
}

CAIManager* CEditorImpl::GetAI()
{
    return m_pAIManager;
}

CCustomActionsEditorManager* CEditorImpl::GetCustomActionManager()
{
    return m_pCustomActionsManager;
}

CAnimationContext* CEditorImpl::GetAnimation()
{
    return m_pAnimationContext;
}

CTrackViewSequenceManager* CEditorImpl::GetSequenceManager()
{
    return m_pSequenceManager;
}

ITrackViewSequenceManager* CEditorImpl::GetSequenceManagerInterface()
{
    return GetSequenceManager();
}

void CEditorImpl::RegisterDocListener(IDocListener* listener)
{
    CCryEditDoc* doc = GetDocument();
    if (doc)
    {
        doc->RegisterListener(listener);
    }
}

void CEditorImpl::UnregisterDocListener(IDocListener* listener)
{
    CCryEditDoc* doc = GetDocument();
    if (doc)
    {
        doc->UnregisterListener(listener);
    }
}

const char* GetMetricNameForEvent(EEditorNotifyEvent aEventId)
{
    static const std::unordered_map< EEditorNotifyEvent, const char* > s_eventNameMap =
    {
        {eNotify_OnInit, "OnInit"},
        {eNotify_OnBeginNewScene, "OnBeginNewScene"},
        {eNotify_OnEndNewScene, "OnEndNewScene"},
        {eNotify_OnBeginSceneOpen, "OnBeginSceneOpen"},
        {eNotify_OnEndSceneOpen, "OnEndSceneOpen"},
        {eNotify_OnBeginSceneSave, "OnBeginSceneSave"},
        {eNotify_OnBeginLayerExport, "OnBeginLayerExport"},
        {eNotify_OnEndLayerExport, "OnEndLayerExport"},
        {eNotify_OnCloseScene, "OnCloseScene"},
        {eNotify_OnMissionChange, "OnMissionChange"},
        {eNotify_OnBeginLoad, "OnBeginLoad"},
        {eNotify_OnEndLoad, "OnEndLoad"},
        {eNotify_OnExportToGame, "OnExportToGame"},
        {eNotify_OnEditModeChange, "OnEditModeChange"},
        {eNotify_OnEditToolChange, "OnEditToolChange"},
        {eNotify_OnBeginGameMode, "OnBeginGameMode"},
        {eNotify_OnEndGameMode, "OnEndGameMode"},
        {eNotify_OnEnableFlowSystemUpdate, "OnEnableFlowSystemUpdate"},
        {eNotify_OnDisableFlowSystemUpdate, "OnDisableFlowSystemUpdate"},
        {eNotify_OnSelectionChange, "OnSelectionChange"},
        {eNotify_OnPlaySequence, "OnPlaySequence"},
        {eNotify_OnStopSequence, "OnStopSequence"},
        {eNotify_OnOpenGroup, "OnOpenGroup"},
        {eNotify_OnCloseGroup, "OnCloseGroup"},
        {eNotify_OnTerrainRebuild, "OnTerrainRebuild"},
        {eNotify_OnBeginTerrainRebuild, "OnBeginTerrainRebuild"},
        {eNotify_OnEndTerrainRebuild, "OnEndTerrainRebuild"},
        {eNotify_OnDisplayRenderUpdate, "OnDisplayRenderUpdate"},
        {eNotify_OnLayerImportBegin, "OnLayerImportBegin"},
        {eNotify_OnLayerImportEnd, "OnLayerImportEnd"},
        {eNotify_OnAddAWSProfile, "OnAddAWSProfile"},
        {eNotify_OnSwitchAWSProfile, "OnSwitchAWSProfile"},
        {eNotify_OnSwitchAWSDeployment, "OnSwitchAWSDeployment"},
        {eNotify_OnFirstAWSUse, "OnFirstAWSUse"}
    };

    auto eventIter = s_eventNameMap.find(aEventId);
    if (eventIter == s_eventNameMap.end())
    {
        return nullptr;
    }

    return eventIter->second;
}

// Confetti Start: Leroy Sikkes
void CEditorImpl::Notify(EEditorNotifyEvent event)
{
    NotifyExcept(event, nullptr);
}

static const char* EDITOR_OPERATION_METRIC_EVENT_NAME = "EditorOperation";
static const char* EDITOR_OPERATION_ATTRIBUTE_NAME = "Operation";

void CEditorImpl::NotifyExcept(EEditorNotifyEvent event, IEditorNotifyListener* listener)
{
    if (m_bExiting)
    {
        return;
    }

    std::list<IEditorNotifyListener*>::iterator it = m_listeners.begin();
    while (it != m_listeners.end())
    {
        if (*it == listener)
        {
            it++;
            continue; // skip "except" listener
        }

        (*it++)->OnEditorNotifyEvent(event);
    }

    if (event == eNotify_OnSelectionChange)
    {
        bool isEditorInGameMode = false;
        EBUS_EVENT_RESULT(isEditorInGameMode, AzToolsFramework::EditorEntityContextRequestBus, IsEditorRunningGame);
        if (isEditorInGameMode)
        {
            if (SelectionContainsComponentEntities())
            {
                SetEditMode(eEditModeSelect);
            }
        }
    }

    if (event == eNotify_OnBeginNewScene)
    {
        if (m_pAxisGizmo)
        {
            m_pAxisGizmo->Release();
        }
        m_pAxisGizmo = 0;
    }
    else if (event == eNotify_OnDisplayRenderUpdate)
    {
        IEditorGame* pEditorGame = m_pGameEngine ? m_pGameEngine->GetIEditorGame() : NULL;
        if (pEditorGame != NULL)
        {
            pEditorGame->OnDisplayRenderUpdated(m_pDisplaySettings->IsDisplayHelpers());
        }
    }

    if (event == eNotify_OnInit)
    {
        REGISTER_COMMAND("py", CmdPy, 0, "Execute a Python code snippet.");
    }

    GetPluginManager()->NotifyPlugins(event);

    const char* eventMetricName = GetMetricNameForEvent(event);
    if (eventMetricName)
    {
        auto metricId = LyMetrics_CreateEvent(EDITOR_OPERATION_METRIC_EVENT_NAME);
        LyMetrics_AddAttribute(metricId, EDITOR_OPERATION_ATTRIBUTE_NAME, eventMetricName);
        LyMetrics_SubmitEvent(metricId);
    }

    if (event == eNotify_OnEndGameMode)
    {
        LogEndGameMode();
    }
}
// Confetti end: Leroy Sikkes

void CEditorImpl::RegisterNotifyListener(IEditorNotifyListener* listener)
{
    listener->m_bIsRegistered = true;
    stl::push_back_unique(m_listeners, listener);
}

void CEditorImpl::UnregisterNotifyListener(IEditorNotifyListener* listener)
{
    m_listeners.remove(listener);
    listener->m_bIsRegistered = false;
}

ISourceControl* CEditorImpl::GetSourceControl()
{
    CryAutoLock<CryMutex> lock(m_pluginMutex);

    if (m_pSourceControl)
    {
        return m_pSourceControl;
    }

    IEditorClassFactory* classFactory = GetIEditor() ? GetIEditor()->GetClassFactory() : nullptr;
    if (classFactory)
    {
        std::vector<IClassDesc*> classes;
        classFactory->GetClassesBySystemID(ESYSTEM_CLASS_SCM_PROVIDER, classes);
        for (int i = 0; i < classes.size(); i++)
        {
            IClassDesc* pClass = classes[i];
            ISourceControl* pSCM = NULL;
            HRESULT hRes = pClass->QueryInterface(__uuidof(ISourceControl), (void**)&pSCM);
            if (!FAILED(hRes) && pSCM)
            {
                m_pSourceControl = pSCM;
                return m_pSourceControl;
            }
        }
    }

    return 0;
}

bool CEditorImpl::IsSourceControlAvailable()
{
    if ((gSettings.enableSourceControl) && (GetSourceControl()))
    {
        return true;
    }

    return false;
}

bool CEditorImpl::IsSourceControlConnected()
{
    if ((gSettings.enableSourceControl) && (GetSourceControl()) && (GetSourceControl()->GetConnectivityState() == ISourceControl::Connected))
    {
        return true;
    }

    return false;
}

IAssetTagging* CEditorImpl::GetAssetTagging()
{
    CryAutoLock<CryMutex> lock(m_pluginMutex);

    if (m_pAssetTagging)
    {
        return m_pAssetTagging;
    }

    std::vector<IClassDesc*> classes;
    GetIEditor()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_ASSET_TAGGING, classes);
    for (int i = 0; i < classes.size(); i++)
    {
        IClassDesc* pClass = classes[i];
        IAssetTagging* pAssetTagging = NULL;
        HRESULT hRes = pClass->QueryInterface(__uuidof(IAssetTagging), (void**)&pAssetTagging);
        if (!FAILED(hRes) && pAssetTagging)
        {
            m_pAssetTagging = pAssetTagging;
            return m_pAssetTagging;
        }
    }

    return 0;
}

void CEditorImpl::SetMatEditMode(bool bIsMatEditMode)
{
    m_bMatEditMode = bIsMatEditMode;
}

void CEditorImpl::ShowStatusText(bool bEnable)
{
    m_bShowStatusText = bEnable;
}

void CEditorImpl::GetMemoryUsage(ICrySizer* pSizer)
{
    SIZER_COMPONENT_NAME(pSizer, "Editor");

    if (GetDocument())
    {
        SIZER_COMPONENT_NAME(pSizer, "Document");

        GetDocument()->GetMemoryUsage(pSizer);
    }

    if (m_pVegetationMap)
    {
        m_pVegetationMap->GetMemoryUsage(pSizer);
    }
}

void CEditorImpl::ReduceMemory()
{
    GetIEditor()->GetUndoManager()->ClearRedoStack();
    GetIEditor()->GetUndoManager()->ClearUndoStack();
    GetIEditor()->GetObjectManager()->SendEvent(EVENT_FREE_GAME_DATA);
    gEnv->pRenderer->FreeResources(FRR_TEXTURES);

#ifdef KDAB_MAC_PORT
    HANDLE hHeap = GetProcessHeap();

    if (hHeap)
    {
        uint64 maxsize = (uint64)HeapCompact(hHeap, 0);
        CryLogAlways("Max Free Memory Block = %I64d Kb", maxsize / 1024);
    }
#endif // KDAB_MAC_PORT
}

IExportManager* CEditorImpl::GetExportManager()
{
    if (!m_pExportManager)
    {
        m_pExportManager = new CExportManager();
    }

    return m_pExportManager;
}

void CEditorImpl::AddUIEnums()
{
    // Spec settings for shadow casting lights
    string SpecString[4];
    QStringList types;
    types.push_back("Never=0");
    SpecString[0].Format("VeryHigh Spec=%d", CONFIG_VERYHIGH_SPEC);
    types.push_back(SpecString[0].c_str());
    SpecString[1].Format("High Spec=%d", CONFIG_HIGH_SPEC);
    types.push_back(SpecString[1].c_str());
    SpecString[2].Format("Medium Spec=%d", CONFIG_MEDIUM_SPEC);
    types.push_back(SpecString[2].c_str());
    SpecString[3].Format("Low Spec=%d", CONFIG_LOW_SPEC);
    types.push_back(SpecString[3].c_str());
    m_pUIEnumsDatabase->SetEnumStrings("CastShadows", types);

    // Power-of-two percentages
    string percentStringPOT[5];
    types.clear();
    percentStringPOT[0].Format("Default=%d", 0);
    types.push_back(percentStringPOT[0].c_str());
    percentStringPOT[1].Format("12.5=%d", 1);
    types.push_back(percentStringPOT[1].c_str());
    percentStringPOT[2].Format("25=%d", 2);
    types.push_back(percentStringPOT[2].c_str());
    percentStringPOT[3].Format("50=%d", 3);
    types.push_back(percentStringPOT[3].c_str());
    percentStringPOT[4].Format("100=%d", 4);
    types.push_back(percentStringPOT[4].c_str());
    m_pUIEnumsDatabase->SetEnumStrings("ShadowMinResPercent", types);
}

void CEditorImpl::SetEditorConfigSpec(ESystemConfigSpec spec)
{
    gSettings.editorConfigSpec = spec;
    if (m_pSystem->GetConfigSpec(true) != spec)
    {
        m_pSystem->SetConfigSpec(spec, true);
        gSettings.editorConfigSpec = m_pSystem->GetConfigSpec(true);
        GetObjectManager()->SendEvent(EVENT_CONFIG_SPEC_CHANGE);
        AzToolsFramework::EditorEvents::Bus::Broadcast(&AzToolsFramework::EditorEvents::OnEditorSpecChange);
        if (m_pVegetationMap)
        {
            m_pVegetationMap->UpdateConfigSpec();
        }
    }
}

ESystemConfigSpec CEditorImpl::GetEditorConfigSpec() const
{
    return (ESystemConfigSpec)gSettings.editorConfigSpec;
}

void CEditorImpl::InitFinished()
{
    SProjectSettingsBlock::Load();

    if (!m_bInitialized)
    {
        m_bInitialized = true;
        Notify(eNotify_OnInit);

        // Let system wide listeners know about this as well.
        GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_ON_INIT, 0, 0);
    }
}

void CEditorImpl::ReloadTemplates()
{
    m_templateRegistry.LoadTemplates("Editor");
}

void CEditorImpl::AddErrorMessage(const QString& text, const QString& caption)
{
    if (!m_pErrorsDlg)
    {
        m_pErrorsDlg = new CErrorsDlg(GetEditorMainWindow());
        m_pErrorsDlg->show();
    }

    m_pErrorsDlg->AddMessage(text, caption);
}

void CEditorImpl::CmdPy(IConsoleCmdArgs* pArgs)
{
    // Execute the given script command.
    QString scriptCmd = pArgs->GetCommandLine();

    scriptCmd = scriptCmd.right(scriptCmd.length() - 2); // The part of the text after the 'py'
    scriptCmd = scriptCmd.trimmed();
    PyScript::AcquirePythonLock();
    PyRun_SimpleString(scriptCmd.toLatin1().data());
    PyErr_Print();
    PyScript::ReleasePythonLock();
}

void CEditorImpl::OnObjectContextMenuOpened(QMenu* pMenu, const CBaseObject* pObject)
{
    for (auto it : m_objectContextMenuExtensions)
    {
        it(pMenu, pObject);
    }
}

void CEditorImpl::RegisterObjectContextMenuExtension(TContextMenuExtensionFunc func)
{
    m_objectContextMenuExtensions.push_back(func);
}

void CEditorImpl::SetCurrentMissionTime(float time)
{
    if (CMission* pMission = GetIEditor()->GetDocument()->GetCurrentMission())
    {
        pMission->SetTime(time);
    }
}
// Vladimir@Conffx
SSystemGlobalEnvironment* CEditorImpl::GetEnv()
{
    assert(gEnv);
    return gEnv;
}

// Leroy@Conffx
IEditorParticleUtils* CEditorImpl::GetParticleUtils()
{
    return m_particleEditorUtils;
}

// Leroy@Conffx
SEditorSettings* CEditorImpl::GetEditorSettings()
{
    return &gSettings;
}

// Vladimir@Conffx
IAssetBrowser* CEditorImpl::GetAssetBrowser()
{
    return m_pAssetBrowser;
}

// Vladimir@Conffx
IBaseLibraryManager* CEditorImpl::GetMaterialManagerLibrary()
{
    return m_pMaterialManager;
}

// Vladimir@Conffx
IEditorMaterialManager* CEditorImpl::GetIEditorMaterialManager()
{
    return m_pMaterialManager;
}

IImageUtil* CEditorImpl::GetImageUtil()
{
    return m_pImageUtil;
}

QMimeData* CEditorImpl::CreateQMimeData() const
{
    return new QMimeData();
}

void CEditorImpl::DestroyQMimeData(QMimeData* data) const
{
    delete data;
}

CBaseObject* CEditorImpl::BaseObjectFromEntityId(EntityId id)
{
    return CEntityObject::FindFromEntityId(id);
}

void CEditorImpl::OnStartPlayInEditor()
{
    if (SelectionContainsComponentEntities())
    {
        SetEditMode(eEditModeSelect);
    }
    LogBeginGameMode();
}

void CEditorImpl::InitializeCrashLog()
{
    LyMetrics_InitializeCurrentProcessStatus(m_crashLogFileName);

#if defined(WIN32) || defined(WIN64)
    if (::IsDebuggerPresent())
    {
        LyMetrics_UpdateCurrentProcessStatus(EESS_DebuggerAttached);
    }
#endif
}

void CEditorImpl::ShutdownCrashLog()
{
    LyMetrics_UpdateCurrentProcessStatus(EESS_EditorShutdown);
}

void CEditorImpl::LogBeginGameMode()
{
    LyMetrics_UpdateCurrentProcessStatus(EESS_InGame);
}

void CEditorImpl::LogEndGameMode()
{
    EEditorSessionStatus sessionStatus = EESS_EditorOpened;

#if defined(WIN32) || defined(WIN64)
    if (::IsDebuggerPresent())
    {
        sessionStatus = EESS_DebuggerAttached;
    }
#endif

    LyMetrics_UpdateCurrentProcessStatus(sessionStatus);
}
