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
#include "StdAfx.h"
#include "Resource.h"
#include "LevelEditorMenuHandler.h"
#include "MainWindow.h"
#include "QtViewPaneManager.h"
#include "QMenuBar"
#include "QTimer"
#include "Toolbox.h"
#include "Viewport.h"
#include "Controls/RollupBar.h"
#include "Controls/ConsoleSCB.h"

#include <QSettings>
#include <NetPromoterScore/NetPromoterScoreDialog.h>
#include <AzToolsFramework/UI/AssetEditor/AssetEditorWidget.hxx>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>
#include <QMap>

using namespace AZ;

static const char* g_menuSwitchSettingName = "MainMenuMode";
static const char* g_LUAEditorName = "Lua Editor";

static const char* g_netPromoterScore = "NetPromoterScore";
static const char* g_shortTimeInterval = "debug";

static bool CompareLayoutNames(const QString& name1, const QString& name2)
{
    return name1.compare(name2, Qt::CaseInsensitive) > 0;
}

LevelEditorMenuHandler::LevelEditorMenuHandler(MainWindow* mainWindow, QtViewPaneManager* const viewPaneManager, QSettings& settings)
: m_mainWindow(mainWindow)
    , m_viewPaneManager(viewPaneManager)
    , m_actionManager(mainWindow->GetActionManager())
    , m_settings(settings)
{
#if defined(AZ_PLATFORM_APPLE_OSX)
    // Hide the non-native toolbar, then setNativeMenuBar to ensure it is always visible on macOS.
    m_mainWindow->menuBar()->hide();
    m_mainWindow->menuBar()->setNativeMenuBar(true);
#endif
}

void LevelEditorMenuHandler::Initialize()
{
    // make sure we can fix the view menus
    connect(m_viewPaneManager, &QtViewPaneManager::registeredPanesChanged, this, &LevelEditorMenuHandler::ResetToolsMenus);

    m_topLevelMenus << CreateFileMenu();
    m_topLevelMenus << CreateEditMenu();
    m_topLevelMenus << CreateGameMenu();
    m_topLevelMenus << CreateToolsMenu();
    m_topLevelMenus << CreateViewMenu();
    m_topLevelMenus << CreateAWSMenu();
    m_topLevelMenus << CreateHelpMenu();

    // have to do this after creating the AWS Menu for the first time
    ResetToolsMenus();
}

void LevelEditorMenuHandler::ShowMenus()
{
    ShowMenus(true);
}

void LevelEditorMenuHandler::ShowMenus(bool updateRegistryKey)
{
    QMenuBar* menuBar = m_mainWindow->menuBar();

    menuBar->clear();

    for (QMenu* menu : m_topLevelMenus)
    {
        menuBar->addMenu(menu);
    }

    if (updateRegistryKey)
    {
        m_mainWindow->m_settings.setValue(GetSwitchMenuSettingName(), 1);
    }
}

bool LevelEditorMenuHandler::MRUEntryIsValid(const QString& entry, const QString& gameFolderPath)
{
    if (entry.isEmpty())
    {
        return false;
    }

    QFileInfo info(entry);
    if (!info.exists())
    {
        return false;
    }

    return info.absolutePath().startsWith(gameFolderPath);
}

const char* LevelEditorMenuHandler::GetSwitchMenuSettingName()
{
    return g_menuSwitchSettingName;
}

void LevelEditorMenuHandler::IncrementViewPaneVersion()
{
    m_viewPaneVersion++;
}

int LevelEditorMenuHandler::GetViewPaneVersion() const
{
    return m_viewPaneVersion;
}

void LevelEditorMenuHandler::UpdateViewLayoutsMenu(ActionManager::MenuWrapper& layoutsMenu)
{
    if (layoutsMenu.isNull())
    {
        return;
    }

    QStringList layoutNames = m_viewPaneManager->LayoutNames();
    qSort(layoutNames.begin(), layoutNames.end(), CompareLayoutNames);
    layoutsMenu->clear();
    const int MAX_LAYOUTS = ID_VIEW_LAYOUT_LAST - ID_VIEW_LAYOUT_FIRST;

    QAction* componentLayoutAction = layoutsMenu->addAction(tr("Component Entity Layout"));
    connect(componentLayoutAction, &QAction::triggered, this, &LevelEditorMenuHandler::LoadComponentLayout);

    // Load Legacy Layout
    QAction* legacyLayoutAction = layoutsMenu->addAction(tr("Legacy Layout"));
    connect(legacyLayoutAction, &QAction::triggered, this, &LevelEditorMenuHandler::LoadLegacyLayout);

    layoutsMenu.AddSeparator();

    for (int i = 0; i < layoutNames.size() && i <= MAX_LAYOUTS; i++)
    {
        const QString layoutName = layoutNames[i];
        QAction* action = layoutsMenu->addAction(layoutName);
        QMenu* subSubMenu = new QMenu();

        QAction* subSubAction = subSubMenu->addAction(tr("Load"));
        connect(subSubAction, &QAction::triggered, [layoutName, this]() { m_mainWindow->ViewLoadPaneLayout(layoutName); });

        subSubAction = subSubMenu->addAction(tr("Save"));
        connect(subSubAction, &QAction::triggered, [layoutName, this]() { m_mainWindow->ViewSavePaneLayout(layoutName); });

        subSubAction = subSubMenu->addAction(tr("Rename..."));
        connect(subSubAction, &QAction::triggered, [layoutName, this]() { m_mainWindow->ViewRenamePaneLayout(layoutName); });

        subSubAction = subSubMenu->addAction(tr("Delete"));
        connect(subSubAction, &QAction::triggered, [layoutName, this]() { m_mainWindow->ViewDeletePaneLayout(layoutName); });

        action->setMenu(subSubMenu);
    }

    layoutsMenu.AddAction(ID_VIEW_SAVELAYOUT);

    layoutsMenu.AddAction(ID_VIEW_LAYOUT_LOAD_DEFAULT);
}

void LevelEditorMenuHandler::ResetToolsMenus()
{
    if (!m_toolsMenu->isEmpty())
    {
        // Clear everything from the Tools menu
        m_toolsMenu->clear();
    }

    if (!m_cloudMenu->isEmpty())
    {
        m_cloudMenu->clear();
    }

    QtViewPanes allRegisteredViewPanes = QtViewPaneManager::instance()->GetRegisteredPanes();

    QMap<QString, QList<QtViewPane*>> menuMap;

    CreateMenuMap(menuMap, allRegisteredViewPanes);

    CreateMenuOptions(&menuMap, m_toolsMenu, LyViewPane::CategoryTools);

    m_toolsMenu.AddSeparator();

    // Other
    auto otherSubMenu = m_toolsMenu.AddMenu(QObject::tr("Other"));

    CreateMenuOptions(&menuMap, otherSubMenu, LyViewPane::CategoryOther);

    // Plug-Ins
    auto plugInsMenu = m_toolsMenu.AddMenu(QObject::tr("Plug-Ins"));

    CreateMenuOptions(&menuMap, plugInsMenu, LyViewPane::CategoryPlugIns);

    m_toolsMenu.AddSeparator();

    // set up the cloud canvas menu, which is slightly different than the other menus because it
    // goes somewhere else
    CopyActionWithoutIcon(m_cloudMenu, ID_AWS_ACTIVE_DEPLOYMENT, "Select a Deployment", false);
    CreateMenuOptions(&menuMap, m_cloudMenu, LyViewPane::CategoryCloudCanvas);

    // Optional Sub Menus
    if (!menuMap.isEmpty())
    {
        QMap<QString, QList<QtViewPane*>>::iterator it = menuMap.begin();

        while (it != menuMap.end())
        {
            auto currentSubMenu = m_toolsMenu.AddMenu(it.key());
            CreateMenuOptions(&menuMap, currentSubMenu, it.key().toStdString().c_str());

            it = menuMap.begin();
        }
    }
}

QMenu* LevelEditorMenuHandler::CreateFileMenu()
{
    auto fileMenu = m_actionManager->AddMenu(tr("&File"));
    connect(fileMenu, &QMenu::aboutToShow, this, &LevelEditorMenuHandler::OnUpdateOpenRecent);

    // New
    fileMenu.AddAction(ID_FILE_NEW);
    // Open...
    fileMenu.AddAction(ID_FILE_OPEN_LEVEL);

    // Open Recent
    m_mostRecentLevelsMenu = fileMenu.AddMenu(tr("Open Recent"));
    connect(m_mostRecentLevelsMenu, &QMenu::aboutToShow, this, &LevelEditorMenuHandler::UpdateMRUFiles);

    OnUpdateOpenRecent();


    fileMenu.AddSeparator();

    // Save
    fileMenu.AddAction(ID_FILE_SAVE_LEVEL);

    // Save As...
    CopyActionWithoutIcon(fileMenu, ID_FILE_SAVE_AS, "Save as...");

    // Save Level Resources...
    fileMenu.AddAction(ID_FILE_SAVELEVELRESOURCES);

    // Save Level Statistics
    fileMenu.AddAction(ID_TOOLS_LOGMEMORYUSAGE);

    // Save Modified External Layers
    fileMenu.AddAction(ID_PANEL_LAYERS_SAVE_EXTERNAL_LAYERS);

    fileMenu.AddSeparator();

    // NEWMENUS: NEEDS IMPLEMENTATION
    //m_mostRecentProjectsMenu = fileMenu.AddMenu(tr("Recent Projects"));
    //connect(m_mostRecentProjectsMenu, &QMenu::aboutToShow, this, &LevelEditorMenuHandler::UpdateMRUProjects);
    //fileMenu.AddSeparator();

    // Project Settings
    auto projectSettingMenu = fileMenu.AddMenu(tr("Project Settings"));

    // Switch Projects
    projectSettingMenu.AddAction(ID_PROJECT_CONFIGURATOR_PROJECTSELECTION);

    // Configure Gems
    auto configureGemSubMenu = projectSettingMenu.Get()->addAction(tr("Configure Gems"));
    connect(configureGemSubMenu, &QAction::triggered, this, &LevelEditorMenuHandler::ActivateGemConfiguration);

    // Input Mapping
    auto inputMappingMenu = projectSettingMenu.Get()->addAction(tr("Input Mapping"));
    connect(inputMappingMenu, &QAction::triggered, this, &LevelEditorMenuHandler::OnOpenAssetEditor);

    fileMenu.AddSeparator();

    // NEWMENUS: NEEDS IMPLEMENTATION
    // should have the equivalent of a Close here; it should be close the current slice, but the editor isn't slice based right now
    // so that won't work.
    // instead, it should be Close of the level, but we don't have that either. I'm leaving it here so that it's obvious where UX intended it
    // to go
    //fileMenu.AddAction(ID_FILE_CLOSE);

    // Show Log File
    fileMenu.AddAction(ID_FILE_EDITLOGFILE);

    // Quit - Mac for quit, Window for exit
#if defined(AZ_PLATFORM_WINDOWS)
    fileMenu.AddAction(ID_APP_EXIT);
#elif defined(AZ_PLATFORM_APPLE)
    QAction* quitAction = CopyActionWithoutIcon(fileMenu, ID_APP_EXIT, "&Quit", false);
#endif

    return fileMenu;
}

QMenu* LevelEditorMenuHandler::CreateEditMenu()
{
    auto editMenu = m_actionManager->AddMenu(tr("&Edit"));

    // Undo
    CopyActionWithoutIcon(editMenu, ID_UNDO, "Undo");

    // Redo
    CopyActionWithoutIcon(editMenu, ID_REDO, "Redo");

    editMenu.AddSeparator();

    // NEWMENUS: NEEDS IMPLEMENTATION
    // Not quite ready for these yet. Have to register them with the ActionManager in MainWindow.cpp when we're ready
    // editMenu->addAction(ID_EDIT_CUT);
    // editMenu->addAction(ID_EDIT_COPY);
    // editMenu->addAction(ID_EDIT_PASTE);
    // editMenu.AddSeparator();

    // Duplicate
    editMenu.AddAction(ID_EDIT_CLONE);

    // Delete
    editMenu.AddAction(ID_EDIT_DELETE);

    editMenu.AddSeparator();

    // Select All
    CopyActionWithoutIcon(editMenu, ID_EDIT_SELECTALL, "Select &All");

    // Deselect All
    CopyActionWithoutIcon(editMenu, ID_EDIT_SELECTNONE, "Deselect All");

    // Next Selection Mask
    editMenu.AddAction(ID_EDIT_NEXTSELECTIONMASK);

    // Invert Selection
    CopyActionWithoutIcon(editMenu, ID_EDIT_INVERTSELECTION, "Invert Selection");

    editMenu.AddSeparator();

    // Hide Selection
    CopyActionWithoutIcon(editMenu, ID_EDIT_HIDE, "Hide Selection");

    // Show Selection
    auto showSelectionMenu = editMenu.Get()->addAction(tr("Show Selection"));
    connect(showSelectionMenu, &QAction::triggered, [this]() {ToggleSelection(false); });

    // Show Last Hidden
    CopyActionWithoutIcon(editMenu, ID_EDIT_SHOW_LAST_HIDDEN, "Show Last Hidden");

    // Unhide All
    CopyActionWithoutIcon(editMenu, ID_EDIT_UNHIDEALL, "Unhide All");

    /*
     * The following block of code is part of the feature "Isolation Mode" and is temporarily 
     * disabled for 1.10 release.
     * Jira: https://jira.agscollab.com/browse/LY-49532

    // Isolate Selected
    QAction* isolateSelectedAction = editMenu->addAction(tr("Isolate Selected"));
    connect(isolateSelectedAction, &QAction::triggered, this, []() {
        AzToolsFramework::ToolsApplicationRequestBus::Broadcast(&AzToolsFramework::ToolsApplicationRequestBus::Events::EnterEditorIsolationMode);
    });

    // Exit Isolation
    QAction* exitIsolationAction = editMenu->addAction(tr("Exit Isolation"));
    connect(exitIsolationAction, &QAction::triggered, this, []() {
        AzToolsFramework::ToolsApplicationRequestBus::Broadcast(&AzToolsFramework::ToolsApplicationRequestBus::Events::ExitEditorIsolationMode);
    });

    connect(editMenu, &QMenu::aboutToShow, this, [isolateSelectedAction, exitIsolationAction]() {
        bool isInIsolationMode = false;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(isInIsolationMode, &AzToolsFramework::ToolsApplicationRequestBus::Events::IsEditorInIsolationMode);
        if (isInIsolationMode)
        {
            isolateSelectedAction->setDisabled(true);
            exitIsolationAction->setDisabled(false);
        }
        else
        {
            isolateSelectedAction->setDisabled(false);
            exitIsolationAction->setDisabled(true);
        }
    });

    */

    editMenu.AddSeparator();

    // Group Sub Menu
    auto groupSubMenu = editMenu.AddMenu(tr("Group"));

    // Group
    groupSubMenu.AddAction(ID_GROUP_MAKE);

    // Ungroup
    groupSubMenu.AddAction(ID_GROUP_UNGROUP);

    // Open Group
    groupSubMenu.AddAction(ID_GROUP_OPEN);

    // Close Group
    groupSubMenu.AddAction(ID_GROUP_CLOSE);

    // Attach to Group
    groupSubMenu.AddAction(ID_GROUP_ATTACH);

    // Detach from Group
    groupSubMenu.AddAction(ID_GROUP_DETACH);

    groupSubMenu.AddSeparator();

    // Hold
    groupSubMenu.AddAction(ID_EDIT_HOLD);

    // Fetch
    groupSubMenu.AddAction(ID_EDIT_FETCH);

    // Modify Menu
    auto modifyMenu = editMenu.AddMenu(tr("&Modify"));
    modifyMenu.AddAction(ID_MODIFY_LINK);
    modifyMenu.AddAction(ID_MODIFY_UNLINK);
    modifyMenu.AddSeparator();

    auto alignMenu = modifyMenu.AddMenu(tr("Align"));
    alignMenu.AddAction(ID_OBJECTMODIFY_ALIGNTOGRID);
    alignMenu.AddAction(ID_OBJECTMODIFY_ALIGN);
    alignMenu.AddAction(ID_MODIFY_ALIGNOBJTOSURF);

    auto constrainMenu = modifyMenu.AddMenu(tr("Constrain"));
    constrainMenu.AddAction(ID_SELECT_AXIS_X);
    constrainMenu.AddAction(ID_SELECT_AXIS_Y);
    constrainMenu.AddAction(ID_SELECT_AXIS_Z);
    constrainMenu.AddAction(ID_SELECT_AXIS_XY);
    constrainMenu.AddAction(ID_SELECT_AXIS_TERRAIN);

    auto snapMenu = modifyMenu.AddMenu(tr("Snap"));
    snapMenu.AddAction(ID_SNAP_TO_GRID);
    snapMenu.AddAction(ID_SNAPANGLE);

    auto transformModeMenu = modifyMenu.AddMenu(tr("Transform Mode"));
    transformModeMenu.AddAction(ID_EDITMODE_SELECT);
    transformModeMenu.AddAction(ID_EDITMODE_MOVE);
    transformModeMenu.AddAction(ID_EDITMODE_ROTATE);
    transformModeMenu.AddAction(ID_EDITMODE_SCALE);
    transformModeMenu.AddAction(ID_EDITMODE_SELECTAREA);

    auto convertToMenu = modifyMenu.AddMenu(tr("Convert to"));
    convertToMenu.AddAction(ID_CONVERTSELECTION_TOBRUSHES);
    convertToMenu.AddAction(ID_CONVERTSELECTION_TOSIMPLEENTITY);
    convertToMenu.AddAction(ID_CONVERTSELECTION_TODESIGNEROBJECT);
    convertToMenu.AddAction(ID_CONVERTSELECTION_TOSTATICENTITY);
    convertToMenu.AddAction(ID_CONVERTSELECTION_TOGAMEVOLUME);
    convertToMenu.AddAction(ID_CONVERTSELECTION_TOCOMPONENTENTITY);

    auto fastRotateMenu = modifyMenu.AddMenu(tr("Fast Rotate"));
    fastRotateMenu.AddAction(ID_ROTATESELECTION_XAXIS);
    fastRotateMenu.AddAction(ID_ROTATESELECTION_YAXIS);
    fastRotateMenu.AddAction(ID_ROTATESELECTION_ZAXIS);
    fastRotateMenu.AddAction(ID_ROTATESELECTION_ROTATEANGLE);

    auto subObjectModeMenu = modifyMenu.AddMenu(tr("Sub Object Mode"));
    subObjectModeMenu.AddAction(ID_SUBOBJECTMODE_EDGE);
    subObjectModeMenu.AddAction(ID_SUBOBJECTMODE_FACE);
    subObjectModeMenu.AddAction(ID_SUBOBJECTMODE_PIVOT);
    subObjectModeMenu.AddAction(ID_SUBOBJECTMODE_VERTEX);

    modifyMenu.AddSeparator();

    modifyMenu.AddAction(ID_SELECTION_SAVE);
    modifyMenu.AddAction(ID_SELECTION_LOAD);
    modifyMenu.AddSeparator();

    modifyMenu.AddAction(ID_TOOLS_UPDATEPROCEDURALVEGETATION);

    editMenu.AddSeparator();

    // Lock Selection
    CopyActionWithoutIcon(editMenu, ID_EDIT_FREEZE, "Lock Selection", false);

    // NEWMENUS: NEEDS IMPLEMENTATION
    //// Unlock Selection 
    //auto unlockSelectionMenu = editMenu.Get()->addAction(tr("Unlock Selection"));

    //// Unlock Last Locked
    //auto unlockLastLockedMenu = editMenu.Get()->addAction(tr("Unlock Last Locked"));

    // Unlock All 
    CopyActionWithoutIcon(editMenu, ID_EDIT_UNFREEZEALL, "Unlock All", false);

    // Rename Object(s)...
    editMenu.AddAction(ID_EDIT_RENAMEOBJECT);

    // Set Object(s) Height...
    editMenu.AddAction(ID_MODIFY_OBJECT_HEIGHT);

    editMenu.AddSeparator();

    // Editor Settings
    auto editorSettingsMenu = editMenu.AddMenu(tr("Editor Settings"));

    // Global Preferences...
    CopyActionWithoutIcon(editorSettingsMenu, ID_TOOLS_PREFERENCES, "Global Preferences...", false);

    // Graphics Performance
    auto graphicPerformanceSubMenu = editorSettingsMenu.AddMenu(QObject::tr("Graphics Performance"));
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEVERYHIGHSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEHIGHSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEMEDIUMSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLELOWSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEDURANGOSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEORBISSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEANDROIDSPEC);
    graphicPerformanceSubMenu.AddAction(ID_GAME_ENABLEIOSSPEC);

    // Keyboard Customization
    auto keyboardCustomizationMenu = editorSettingsMenu.AddMenu(tr("Keyboard Customization"));
    keyboardCustomizationMenu.AddAction(ID_TOOLS_CUSTOMIZEKEYBOARD);
    keyboardCustomizationMenu.AddAction(ID_TOOLS_EXPORT_SHORTCUTS);
    keyboardCustomizationMenu.AddAction(ID_TOOLS_IMPORT_SHORTCUTS);

    return editMenu;
}

QMenu* LevelEditorMenuHandler::CreateGameMenu()
{
    auto gameMenu = m_actionManager->AddMenu(tr("&Game"));

    // update the Toolbox Macros List
    connect(gameMenu, &QMenu::aboutToShow, this, &LevelEditorMenuHandler::OnUpdateMacrosMenu);

    // Play Game
    CopyActionWithoutIcon(gameMenu, ID_VIEW_SWITCHTOGAME, "Play Game");

    // Enable Physics/AI
    gameMenu.AddAction(ID_SWITCH_PHYSICS);
    gameMenu.AddSeparator();

    // Export to Engine
    gameMenu.AddAction(ID_FILE_EXPORTTOGAMENOSURFACETEXTURE);

    // Export Selected Objects
    gameMenu.AddAction(ID_FILE_EXPORT_SELECTEDOBJECTS);

    // Export Occlusion Mesh
    gameMenu.AddAction(ID_FILE_EXPORTOCCLUSIONMESH);

    gameMenu.AddSeparator();

    // Terrain Collision
    gameMenu.AddAction(ID_TERRAIN_COLLISION);

    // Edit Equipment-Packs...
    gameMenu.AddAction(ID_TOOLS_EQUIPPACKSEDIT);

    // Toggle SP/MP GameRules
    gameMenu.AddAction(ID_TOGGLE_MULTIPLAYER);

    // Synchronize Player with Camera
    gameMenu.AddAction(ID_GAME_SYNCPLAYER);

    //AI
    auto aiMenu = gameMenu.AddMenu(tr("AI"));

    // Generate All AI
    aiMenu.AddAction(ID_AI_GENERATEALL);

    // Generate Triangulation
    aiMenu.AddAction(ID_AI_GENERATETRIANGULATION);

    // Generate 3D Navigation Volumes
    aiMenu.AddAction(ID_AI_GENERATE3DVOLUMES);

    // Generate Flight Navigation
    aiMenu.AddAction(ID_AI_GENERATEFLIGHTNAVIGATION);

    // Generate Waypoints
    aiMenu.AddAction(ID_AI_GENERATEWAYPOINT);

    // Validate Navigation
    aiMenu.AddAction(ID_AI_VALIDATENAVIGATION);

    // Clear All Navigation
    aiMenu.AddAction(ID_AI_CLEARALLNAVIGATION);

    // Generate Spawner Entity Code
    aiMenu.AddAction(ID_AI_GENERATESPAWNERS);

    // Generate 3D Debug Voxels
    aiMenu.AddAction(ID_AI_GENERATE3DDEBUGVOXELS);

    // Create New Navigation Area
    aiMenu.AddAction(ID_AI_NAVIGATION_NEW_AREA);

    // Request a Full MNM Rebuild
    aiMenu.AddAction(ID_AI_NAVIGATION_TRIGGER_FULL_REBUILD);

    // Show Navigation Areas
    aiMenu.AddAction(ID_AI_NAVIGATION_SHOW_AREAS);

    // Add Navigation Seed
    aiMenu.AddAction(ID_AI_NAVIGATION_ADD_SEED);

    // Continuous Update
    aiMenu.AddAction(ID_AI_NAVIGATION_ENABLE_CONTINUOUS_UPDATE);

    // Visualize Navigation Accessibility
    aiMenu.AddAction(ID_AI_NAVIGATION_VISUALIZE_ACCESSIBILITY);

    // Medium Sized Characters (Use the old version for now)
    aiMenu.AddAction(ID_AI_NAVIGATION_DISPLAY_AGENT);

    // Generate Cover Surfaces
    aiMenu.AddAction(ID_AI_GENERATECOVERSURFACES);

    // AIPoint Pick Link
    aiMenu.AddAction(ID_MODIFY_AIPOINT_PICKLINK);

    // AIPoint Pick Impass Link
    aiMenu.AddAction(ID_MODIFY_AIPOINT_PICKIMPASSLINK);

    gameMenu.AddSeparator();

    // Audio 
    auto audioMenu = gameMenu.AddMenu(tr("Audio"));

    // Stop All Sounds
    audioMenu.AddAction(ID_SOUND_STOPALLSOUNDS);

    // Refresh Audio
    audioMenu.AddAction(ID_AUDIO_REFRESH_AUDIO_SYSTEM);

    gameMenu.AddSeparator();

    // Clouds
    auto cloudsMenu = gameMenu.AddMenu(tr("Clouds"));

    // Create
    cloudsMenu.AddAction(ID_CLOUDS_CREATE);

    // Destroy
    cloudsMenu.AddAction(ID_CLOUDS_DESTROY);
    cloudsMenu.AddSeparator();

    // Open
    cloudsMenu.AddAction(ID_CLOUDS_OPEN);

    // Close
    cloudsMenu.AddAction(ID_CLOUDS_CLOSE);

    gameMenu.AddSeparator();

    // Physics
    auto physicsMenu = gameMenu.AddMenu(tr("Physics"));

    // Get Physics State
    physicsMenu.AddAction(ID_PHYSICS_GETPHYSICSSTATE);

    // Reset Physics State
    physicsMenu.AddAction(ID_PHYSICS_RESETPHYSICSSTATE);

    // Simulate Objects
    physicsMenu.AddAction(ID_PHYSICS_SIMULATEOBJECTS);

    gameMenu.AddSeparator();

    // Prefabs
    auto prefabsMenu = gameMenu.AddMenu(tr("Prefabs"));

    prefabsMenu.AddAction(ID_PREFABS_MAKEFROMSELECTION);
    prefabsMenu.AddAction(ID_PREFABS_ADDSELECTIONTOPREFAB);
    prefabsMenu.AddSeparator();
    prefabsMenu.AddAction(ID_PREFABS_CLONESELECTIONFROMPREFAB);
    prefabsMenu.AddAction(ID_PREFABS_EXTRACTSELECTIONFROMPREFAB);
    prefabsMenu.AddSeparator();
    prefabsMenu.AddAction(ID_PREFABS_OPENALL);
    prefabsMenu.AddAction(ID_PREFABS_CLOSEALL);
    prefabsMenu.AddSeparator();
    prefabsMenu.AddAction(ID_PREFABS_REFRESHALL);

    gameMenu.AddSeparator();

    // Terrain
    auto terrainMenu = gameMenu.AddMenu(tr("&Terrain"));

    terrainMenu.AddAction(ID_FILE_GENERATETERRAINTEXTURE);
    terrainMenu.AddAction(ID_FILE_GENERATETERRAIN);
    terrainMenu.AddSeparator();
    terrainMenu.AddAction(ID_TERRAIN);

    terrainMenu.AddAction(ID_TERRAIN_TEXTURE_EXPORT);

    terrainMenu.AddSeparator();

    terrainMenu.AddAction(ID_TERRAIN_EXPORTBLOCK);
    terrainMenu.AddAction(ID_TERRAIN_IMPORTBLOCK);
    terrainMenu.AddAction(ID_TERRAIN_RESIZE);
    terrainMenu.AddSeparator();

    auto terrainModifyMenu = terrainMenu.AddMenu(tr("Terrain Modify"));
    terrainModifyMenu.AddAction(ID_TOOLTERRAINMODIFY_SMOOTH);
    terrainModifyMenu.AddAction(ID_TERRAINMODIFY_SMOOTH);

    terrainMenu.AddAction(ID_TERRAIN_VEGETATION);
    terrainMenu.AddAction(ID_TERRAIN_PAINTLAYERS);
    terrainMenu.AddAction(ID_TERRAIN_REFINETERRAINTEXTURETILES);
    terrainMenu.AddSeparator();
    terrainMenu.AddAction(ID_FILE_EXPORT_TERRAINAREA);
    terrainMenu.AddAction(ID_FILE_EXPORT_TERRAINAREAWITHOBJECTS);

    gameMenu.AddSeparator();

    CreateDebuggingSubMenu(gameMenu);

    return gameMenu;
}

QMenu* LevelEditorMenuHandler::CreateToolsMenu()
{
    // Tools
    m_toolsMenu = m_actionManager->AddMenu(tr("&Tools"));

    return m_toolsMenu;
}

QMenu* LevelEditorMenuHandler::CreateAWSMenu()
{
    // AWS
    auto awsMenu = m_actionManager->AddMenu(tr("&AWS"));
    awsMenu.AddAction(ID_AWS_CREDENTIAL_MGR);

    // Cloud Canvas
    m_cloudMenu = awsMenu.AddMenu(tr("Cloud Canvas"));

    // Commerce
    auto commerceMenu = awsMenu.AddMenu(tr("Commerce"));
    commerceMenu.AddAction(ID_COMMERCE_MERCH);
    commerceMenu.AddAction(ID_COMMERCE_PUBLISH);

    // GameLift
    auto awsGameLiftMenu = awsMenu.AddMenu(tr("GameLift"));
    awsGameLiftMenu.AddAction(ID_AWS_GAMELIFT_LEARN);
    awsGameLiftMenu.AddAction(ID_AWS_GAMELIFT_CONSOLE);
    awsGameLiftMenu.AddAction(ID_AWS_GAMELIFT_GETSTARTED);
    awsGameLiftMenu.AddAction(ID_AWS_GAMELIFT_TRIALWIZARD);

    // Open AWS Console
    auto awsConsoleMenu = awsMenu.AddMenu(tr("Open AWS Console"));
    awsConsoleMenu.AddAction(ID_AWS_LAUNCH);
    awsConsoleMenu.AddAction(ID_AWS_COGNITO_CONSOLE);
    awsConsoleMenu.AddAction(ID_AWS_DYNAMODB_CONSOLE);
    awsConsoleMenu.AddAction(ID_AWS_S3_CONSOLE);
    awsConsoleMenu.AddAction(ID_AWS_LAMBDA_CONSOLE);

    // Cloud Gem Portal
    awsMenu.AddSeparator();
    awsMenu.AddAction(ID_CGP_CONSOLE);

    return awsMenu;
}

QMenu* LevelEditorMenuHandler::CreateViewMenu()
{
    auto viewMenu = m_actionManager->AddMenu(tr("&View"));

    // NEWMENUS: NEEDS IMPLEMENTATION
    // minimize window - Ctrl+M
    // Zoom - Ctrl+Plus(+) -> Need the inverse too?

    // Cycle Viewports
    viewMenu.AddAction(ID_VIEW_CYCLE2DVIEWPORT);

    // Center on Selection
    CopyActionWithoutIcon(viewMenu, ID_MODIFY_GOTO_SELECTION, "Center on Selection");

    // Show Quick Access Bar
    CopyActionWithoutIcon(viewMenu, ID_OPEN_QUICK_ACCESS_BAR, "Show Quick Access Bar");

    // Enter Full Screen Mode
    CopyActionWithoutIcon(viewMenu, ID_DISPLAY_TOGGLEFULLSCREENMAINWINDOW, "Enter Full Screen Mode", false);

    // Layouts
    m_layoutsMenu = viewMenu.AddMenu(tr("Layouts"));
    connect(m_viewPaneManager, &QtViewPaneManager::savedLayoutsChanged, this, [this]()
    {
        UpdateViewLayoutsMenu(m_layoutsMenu);
    });

    UpdateViewLayoutsMenu(m_layoutsMenu);

    // Viewport
    auto viewportViewsMenuWrapper = viewMenu.AddMenu(tr("Viewport"));
    auto viewportTypesMenuWrapper = viewportViewsMenuWrapper.AddMenu(tr("Viewport Type"));

    m_viewportViewsMenu = viewportViewsMenuWrapper;
    connect(viewportTypesMenuWrapper, &QMenu::aboutToShow, this, &LevelEditorMenuHandler::UpdateOpenViewPaneMenu);

    InitializeViewPaneMenu(m_actionManager, viewportTypesMenuWrapper, [](const QtViewPane& view)
    {
        return view.IsViewportPane();
    });
    viewportViewsMenuWrapper.AddAction(ID_WIREFRAME);

    viewportViewsMenuWrapper.AddSeparator();

    // Ruler
    CopyActionWithoutIcon(viewportViewsMenuWrapper, ID_RULER, "Ruler", false);

    viewportViewsMenuWrapper.AddAction(ID_VIEW_GRIDSETTINGS);
    viewportViewsMenuWrapper.AddSeparator();

    viewportViewsMenuWrapper.AddAction(ID_VIEW_CONFIGURELAYOUT);
    viewportViewsMenuWrapper.AddSeparator();

    viewportViewsMenuWrapper.AddAction(ID_DISPLAY_GOTOPOSITION);
    viewportViewsMenuWrapper.AddAction(ID_MODIFY_GOTO_SELECTION);

    auto gotoLocationMenu = viewportViewsMenuWrapper.AddMenu(tr("Goto Location"));
    gotoLocationMenu.AddAction(ID_GOTO_LOC1);
    gotoLocationMenu.AddAction(ID_GOTO_LOC2);
    gotoLocationMenu.AddAction(ID_GOTO_LOC3);
    gotoLocationMenu.AddAction(ID_GOTO_LOC4);
    gotoLocationMenu.AddAction(ID_GOTO_LOC5);
    gotoLocationMenu.AddAction(ID_GOTO_LOC6);
    gotoLocationMenu.AddAction(ID_GOTO_LOC7);
    gotoLocationMenu.AddAction(ID_GOTO_LOC8);
    gotoLocationMenu.AddAction(ID_GOTO_LOC9);
    gotoLocationMenu.AddAction(ID_GOTO_LOC10);
    gotoLocationMenu.AddAction(ID_GOTO_LOC11);
    gotoLocationMenu.AddAction(ID_GOTO_LOC12);

    auto rememberLocationMenu = viewportViewsMenuWrapper.AddMenu(tr("Remember Location"));
    rememberLocationMenu.AddAction(ID_TAG_LOC1);
    rememberLocationMenu.AddAction(ID_TAG_LOC2);
    rememberLocationMenu.AddAction(ID_TAG_LOC3);
    rememberLocationMenu.AddAction(ID_TAG_LOC4);
    rememberLocationMenu.AddAction(ID_TAG_LOC5);
    rememberLocationMenu.AddAction(ID_TAG_LOC6);
    rememberLocationMenu.AddAction(ID_TAG_LOC7);
    rememberLocationMenu.AddAction(ID_TAG_LOC8);
    rememberLocationMenu.AddAction(ID_TAG_LOC9);
    rememberLocationMenu.AddAction(ID_TAG_LOC10);
    rememberLocationMenu.AddAction(ID_TAG_LOC11);
    rememberLocationMenu.AddAction(ID_TAG_LOC12);

    viewportViewsMenuWrapper.AddSeparator();

    auto changeMoveSpeedMenu = viewportViewsMenuWrapper.AddMenu(tr("Change Move Speed"));
    changeMoveSpeedMenu.AddAction(ID_CHANGEMOVESPEED_INCREASE);
    changeMoveSpeedMenu.AddAction(ID_CHANGEMOVESPEED_DECREASE);
    changeMoveSpeedMenu.AddAction(ID_CHANGEMOVESPEED_CHANGESTEP);

    auto switchCameraMenu = viewportViewsMenuWrapper.AddMenu(tr("Switch Camera"));
    switchCameraMenu.AddAction(ID_SWITCHCAMERA_DEFAULTCAMERA);
    switchCameraMenu.AddAction(ID_SWITCHCAMERA_SEQUENCECAMERA);
    switchCameraMenu.AddAction(ID_SWITCHCAMERA_SELECTEDCAMERA);
    switchCameraMenu.AddAction(ID_SWITCHCAMERA_NEXT);

    // NEWMENUS:
    // MISSING AVIRECORDER

    viewportViewsMenuWrapper.AddSeparator();
    viewportViewsMenuWrapper.AddAction(ID_DISPLAY_SHOWHELPERS);

    // Refresh Style
    viewMenu.AddAction(ID_SKINS_REFRESH);

    return viewMenu;
}

QMenu* LevelEditorMenuHandler::CreateHelpMenu()
{
    // Help
    auto helpMenu = m_actionManager->AddMenu(tr("&Help"));

    // Getting Started
    CopyActionWithoutIcon(helpMenu, ID_DOCUMENTATION_GETTINGSTARTEDGUIDE, "Getting Started", false);

    // Tutorials
    helpMenu.AddAction(ID_DOCUMENTATION_TUTORIALS);

    // Documentation
    auto documentationMenu = helpMenu.AddMenu(tr("Documentation"));

    // Glossary
    documentationMenu.AddAction(ID_DOCUMENTATION_GLOSSARY);

    // Lumberyard Documentation
    documentationMenu.AddAction(ID_DOCUMENTATION_LUMBERYARD);

    // GameLift Documentation
    documentationMenu.AddAction(ID_DOCUMENTATION_GAMELIFT);

    // Release Notes
    documentationMenu.AddAction(ID_DOCUMENTATION_RELEASENOTES);

    // GameDev Resources
    auto gameDevResourceMenu = helpMenu.AddMenu(tr("GameDev Resources"));

    // Game Dev Blog
    gameDevResourceMenu.AddAction(ID_DOCUMENTATION_GAMEDEVBLOG);

    // GameDev Twitch Channel
    gameDevResourceMenu.AddAction(ID_DOCUMENTATION_TWITCHCHANNEL);

    // Forums
    gameDevResourceMenu.AddAction(ID_DOCUMENTATION_FORUMS);

    // AWS Support
    gameDevResourceMenu.AddAction(ID_DOCUMENTATION_AWSSUPPORT);

    helpMenu.AddSeparator();

    // Give Us Feedback
    helpMenu.AddAction(ID_DOCUMENTATION_FEEDBACK);

    // Report a Bug???
    // auto reportBugMenu = helpMenu.Get()->addAction(tr("Report a Bug"));

    // About Lumberyard
    helpMenu.AddAction(ID_APP_ABOUT);

    // NEWMENUS: Remove after sufficient QA/Usertesting and feedback
    /* helpMenu->addSeparator();
    QAction* switchMenus = helpMenu->addAction("Switch to Old Menus");
    connect(switchMenus, &QAction::triggered, m_mainWindow, &MainWindow::ShowOldMenus);*/

    LoadNetPromoterScoreDialog(helpMenu);

    return helpMenu;
}

QAction* LevelEditorMenuHandler::CopyActionWithoutIcon(ActionManager::MenuWrapper& menu, QAction* originalAction, const char* menuOptionName, bool copyShortcut /*= true*/)
{
    QAction* newAction = menu.Get()->addAction(menuOptionName);

    if (copyShortcut)
    {
        newAction->setShortcut(originalAction->shortcut());

        // Remove the shortcut on the original action once it's copied so that
        // it doesn't remain functional after being removed or re-assigned from
        // the wrapper action
        originalAction->setShortcut(QKeySequence());
    }

    connect(newAction, &QAction::triggered, originalAction, &QAction::trigger);

    return newAction;
}

QAction* LevelEditorMenuHandler::CopyActionWithoutIcon(ActionManager::MenuWrapper& menu, int actionId, const char* menuOptionName, bool copyShortcut /*= true*/)
{
    QAction* originalAction = m_actionManager->GetAction(actionId);
    return CopyActionWithoutIcon(menu, originalAction, menuOptionName, copyShortcut);
}

QAction* LevelEditorMenuHandler::CreateViewPaneAction(const QtViewPane* view)
{
    QAction* action = m_actionManager->HasAction(view->m_id) ? m_actionManager->GetAction(view->m_id) : nullptr;

    if (!action)
    {
        action = new QAction(view->m_name, this);
        action->setObjectName(view->m_name);
        action->setCheckable(view->IsViewportPane());
        m_actionManager->AddAction(view->m_id, action);

        // copy this so that it's still valid when the lambda executes
        QString viewPaneName = view->m_name;

        QObject::connect(action, &QAction::triggered, QtViewPaneManager::instance(), [action, viewPaneName]()
        {
            // If this action is checkable and was just unchecked, then we
            // should close the view pane
            if (action->isCheckable() && !action->isChecked())
            {
                QtViewPaneManager::instance()->ClosePane(viewPaneName);
            }
            // Otherwise, this action should open the view pane
            else
            {
                QtViewPaneManager::instance()->OpenPane(viewPaneName);
            }
        }, Qt::UniqueConnection);

        if (view->m_options.sendViewPaneNameBackToAmazonAnalyticsServers)
        {
            AzToolsFramework::EditorMetricsEventsBus::Broadcast(&AzToolsFramework::EditorMetricsEventsBus::Events::RegisterAction, action, QString("ViewPaneMenu %1").arg(viewPaneName));
        }
    }

    return action;
}

// Function used to show menu options without its icon and be able to toggle shortcut visibility in the new menu layout
// This is a work around for the fact that setting the shortcut on the original action isn't working for some reasons
// and we need to investigate it further in the future.
QAction* LevelEditorMenuHandler::CreateViewPaneMenuItem(ActionManager* actionManager, ActionManager::MenuWrapper& menu, const QtViewPane* view)
{
    QAction* action = CreateViewPaneAction(view);

    if (!view->m_options.shortcut.isEmpty())
    {
        QAction* wrappedAction = CopyActionWithoutIcon(menu, action, view->m_name.toUtf8(), true);
        wrappedAction->setShortcut(view->m_options.shortcut);

        // Remove the shortcut from the original action being wrapped
        action->setShortcut(QKeySequence());
    }
    else
    {
        menu->addAction(action);
    }

    return action;
}

void LevelEditorMenuHandler::InitializeViewPaneMenu(ActionManager* actionManager, ActionManager::MenuWrapper& menu, AZStd::function < bool(const QtViewPane& view)> functor)
{
    QtViewPanes views = QtViewPaneManager::instance()->GetRegisteredPanes();

    for (auto it = views.cbegin(), end = views.cend(); it != end; ++it)
    {
        const QtViewPane& view = *it;
        if (!functor(view))
        {
            continue;
        }

        CreateViewPaneMenuItem(actionManager, menu, it);
    }
}

void LevelEditorMenuHandler::LoadComponentLayout()
{
    m_viewPaneManager->RestoreDefaultLayout();
}

void LevelEditorMenuHandler::LoadLegacyLayout()
{
    m_viewPaneManager->RestoreLegacyLayout();
}

void LevelEditorMenuHandler::LoadNetPromoterScoreDialog(ActionManager::MenuWrapper& menu)
{
    m_settings.beginGroup(g_netPromoterScore);

    if (!m_settings.value(g_shortTimeInterval).isNull())
    {
        auto showNetPromoterDialog = menu.Get()->addAction(tr("Show Net Promoter Score Dialog"));
        connect(showNetPromoterDialog, &QAction::triggered, [this]() {
            NetPromoterScoreDialog p(m_mainWindow);
            p.exec();
        });
    }
    m_settings.endGroup();
}

QMap<QString, QList<QtViewPane*>> LevelEditorMenuHandler::CreateMenuMap(QMap<QString, QList<QtViewPane*>>& menuMap, QtViewPanes& allRegisteredViewPanes)
{
    // set up view panes to each category
    for (QtViewPane& viewpane : allRegisteredViewPanes)
    {
        // only store the view panes that should be shown in the menu
        if (!viewpane.IsViewportPane())
        {
            menuMap[viewpane.m_category].push_back(&viewpane);
        }
    }

    return menuMap;
}

// sort the menu option name in alphabetically order
struct CaseInsensitiveStringCompare
{
    bool operator() (const QString& left, const QString& right) const
    {
        return left.toLower() < right.toLower();
    }
};

void LevelEditorMenuHandler::CreateMenuOptions(QMap<QString, QList<QtViewPane*>>* menuMap, ActionManager::MenuWrapper& menu, const char * category)
{
    // list in the menu and remove this menu category from the menuMap
    QList<QtViewPane*> menuList = menuMap->take(category);

    std::map<QString, std::function<void()>, CaseInsensitiveStringCompare> sortMenuMap;

    // store menu options into the map
    // name as a key, functionality as a value
    for (QtViewPane* viewpane : menuList)
    {
        // Console  
        if (viewpane->m_options.builtInActionId != LyViewPane::NO_BUILTIN_ACTION)
        {
            sortMenuMap[viewpane->m_name] = [&menu, viewpane]() -> void
            {
                menu.AddAction(viewpane->m_options.builtInActionId);
            };
        }
        else
        {
            sortMenuMap[viewpane->m_name] = [this, viewpane, &menu]() -> void
            {
                CreateViewPaneMenuItem(m_actionManager, menu, viewpane);
            };
        }
    }

    if (category == LyViewPane::CategoryTools)
    {
        // add LUA Editor into the tools map
        sortMenuMap[g_LUAEditorName] = [this, &menu]()->void
        {
            auto luaEditormenu = menu->addAction(tr(g_LUAEditorName));

            connect(luaEditormenu, &QAction::triggered, this, []() {
                EBUS_EVENT(AzToolsFramework::EditorRequests::Bus, LaunchLuaEditor, nullptr);
            });
        };
    }

    // add each menu option into the menu
    std::map<QString, std::function<void()>, CaseInsensitiveStringCompare>::iterator iter;
    for (iter = sortMenuMap.begin(); iter != sortMenuMap.end(); ++iter)
    {
        iter->second();
    }
}

void LevelEditorMenuHandler::CreateDebuggingSubMenu(ActionManager::MenuWrapper gameMenu)
{
    // DebuggingSubMenu
    auto debuggingSubMenu = gameMenu.AddMenu(QObject::tr("Debugging"));

    // Reload Script
    auto reloadScriptsMenu = debuggingSubMenu.AddMenu(tr("Reload Scripts (LEGACY)"));
    reloadScriptsMenu.AddAction(ID_RELOAD_ALL_SCRIPTS);
    reloadScriptsMenu.AddSeparator();
    reloadScriptsMenu.AddAction(ID_RELOAD_ACTOR_SCRIPTS);
    reloadScriptsMenu.AddAction(ID_RELOAD_AI_SCRIPTS);
    reloadScriptsMenu.AddAction(ID_RELOAD_ENTITY_SCRIPTS);
    reloadScriptsMenu.AddAction(ID_RELOAD_ITEM_SCRIPTS);
    reloadScriptsMenu.AddAction(ID_RELOAD_UI_SCRIPTS);

    // Reload Textures/Shaders
    debuggingSubMenu.AddAction(ID_RELOAD_TEXTURES);

    // Reload Geometry
    debuggingSubMenu.AddAction(ID_RELOAD_GEOMETRY);

    // Reload Terrain
    debuggingSubMenu.AddAction(ID_RELOAD_TERRAIN);

    // Resolve Missing Objects/Materials
    CopyActionWithoutIcon(debuggingSubMenu, ID_TOOLS_RESOLVEMISSINGOBJECTS, "Resolve Missing Objects/Materials", false);

    // Enable File Change Monitoring
    CopyActionWithoutIcon(debuggingSubMenu, ID_TOOLS_ENABLEFILECHANGEMONITORING, "Enable File Change Monitoring", false);

    // Check Object Positions
    debuggingSubMenu.AddAction(ID_TOOLS_VALIDATEOBJECTPOSITIONS);

    // Clear Registry Data
    debuggingSubMenu.AddAction(ID_CLEAR_REGISTRY);

    // Check Level for Errors
    debuggingSubMenu.AddAction(ID_VALIDATELEVEL);

    // Save Level Statistics
    debuggingSubMenu.AddAction(ID_TOOLS_LOGMEMORYUSAGE);

    // Compile Scripts
    debuggingSubMenu.AddAction(ID_SCRIPT_COMPILESCRIPT);

    // Reduce Working Set
    debuggingSubMenu.AddAction(ID_RESOURCES_REDUCEWORKINGSET);

    // Update Procedural Vegetation
    debuggingSubMenu.AddAction(ID_TOOLS_UPDATEPROCEDURALVEGETATION);

    // Configure Toolbox Marcos
    CopyActionWithoutIcon(debuggingSubMenu, ID_TOOLS_CONFIGURETOOLS, "Configure ToolBox Macros", false);

    // Toolbox Macros
    m_macrosMenu = debuggingSubMenu.AddMenu(tr("ToolBox Macros"));
    connect(m_macrosMenu, &QMenu::aboutToShow, this, &LevelEditorMenuHandler::UpdateMacrosMenu, Qt::UniqueConnection);

    // Script Help
    debuggingSubMenu.AddAction(ID_TOOLS_SCRIPTHELP);
}

void LevelEditorMenuHandler::UpdateMRUFiles()
{
    auto cryEdit = CCryEditApp::instance();
    RecentFileList* mruList = cryEdit->GetRecentFileList();
    const int numMru = mruList->GetSize();

    if (!m_mostRecentLevelsMenu)
    {
        return;
    }

    static QString s_lastMru;
    QString currentMru = numMru > 0 ? (*mruList)[0] : QString();
    if (s_lastMru == currentMru) // Protect against flickering if we're updating the menu every time
    {
        return;
    }

    s_lastMru = currentMru;

    // Remove most recent items
    m_mostRecentLevelsMenu->clear();

    // Insert mrus
    QString sCurDir = Path::GetEditingGameDataFolder().c_str() + QDir::separator().toLatin1();

    QFileInfo gameDir(sCurDir); // Pass it through QFileInfo so it comes out normalized
    const QString gameDirPath = gameDir.absolutePath();

    for (int i = 0; i < numMru; ++i)
    {
        if (!MRUEntryIsValid((*mruList)[i], gameDirPath))
        {
            continue;
        }

        QString displayName;
        mruList->GetDisplayName(displayName, i, sCurDir);

        QString entry = QString("%1 %2").arg(i + 1).arg(displayName);
        QAction* action = m_actionManager->GetAction(ID_FILE_MRU_FILE1 + i);
        action->setText(entry);

        m_actionManager->RegisterActionHandler(ID_FILE_MRU_FILE1 + i, [i]() {
            auto cryEdit = CCryEditApp::instance();
            RecentFileList* mruList = cryEdit->GetRecentFileList();
            cryEdit->OpenDocumentFile((*mruList)[i].toLatin1().data());
        });

        m_mostRecentLevelsMenu->addAction(action);
    }

    // Used when disabling the "Open Recent" menu options
    OnUpdateOpenRecent();

    m_mostRecentLevelsMenu->addSeparator();

    // Clear All
    auto clearAllMenu = m_mostRecentLevelsMenu->addAction(tr("Clear All"));
    connect(clearAllMenu, &QAction::triggered, this, &LevelEditorMenuHandler::ClearAll);
}

void LevelEditorMenuHandler::ActivateGemConfiguration()
{
    CCryEditApp::instance()->OnOpenProjectConfiguratorGems();
}

void LevelEditorMenuHandler::ClearAll()
{
    RecentFileList* mruList = CCryEditApp::instance()->GetRecentFileList();

    // remove everything from the mru list
    for (int i = mruList->GetSize(); i > 0; i--)
    {
        mruList->Remove(i - 1);
    }

    // save the settings immediately to the registry
    mruList->WriteList();

    // re-update the menus
    UpdateMRUFiles();
}

void LevelEditorMenuHandler::ToggleSelection(bool hide)
{
    CCryEditApp::instance()->OnToggleSelection(hide);
}

// Used for showing last hidden objects
void LevelEditorMenuHandler::ShowLastHidden()
{
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        CUndo undo("Show Last Hidden");
        GetIEditor()->GetObjectManager()->ShowLastHiddenObject();
    }
}

// Used for disabling "Open Recent" menu option
void LevelEditorMenuHandler::OnUpdateOpenRecent()
{
    RecentFileList* mruList = CCryEditApp::instance()->GetRecentFileList();
    const int numMru = mruList->GetSize();
    QString currentMru = numMru > 0 ? (*mruList)[0] : QString();

    if (!currentMru.isEmpty())
    {
        m_mostRecentLevelsMenu->setEnabled(true);
    }
    else
    {
        m_mostRecentLevelsMenu->setEnabled(false);
    }
}

void LevelEditorMenuHandler::OnOpenAssetEditor()
{
    AZ::SerializeContext* serializeContext = nullptr;
    EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);

    AzToolsFramework::AssetEditorDialog* dialog = new AzToolsFramework::AssetEditorDialog(m_mainWindow, serializeContext);
    
    connect(dialog, &QDialog::finished, this, [dialog]() {dialog->deleteLater(); });
    dialog->show();
}

void LevelEditorMenuHandler::OnUpdateMacrosMenu()
{
    auto tools = GetIEditor()->GetToolBoxManager();
    const int macroCount = tools->GetMacroCount(true);

    if (macroCount <= 0)
    {
        m_macrosMenu->setEnabled(false);
    }
    else
    {
        m_macrosMenu->setEnabled(true);
    }
}

void LevelEditorMenuHandler::UpdateMacrosMenu()
{
    m_macrosMenu->clear();

    auto tools = GetIEditor()->GetToolBoxManager();
    const int macroCount = tools->GetMacroCount(true);

    for (int i = 0; i < macroCount; ++i)
    {
        auto macro = tools->GetMacro(i, true);
        const int toolbarId = macro->GetToolbarId();
        if (toolbarId == -1 || toolbarId == ID_TOOLS_TOOL1)
        {
            m_macrosMenu->addAction(macro->action());
        }
    }
}

void LevelEditorMenuHandler::UpdateOpenViewPaneMenu()
{
    // This function goes through all the viewport menu actions (top, left, perspective...)
    // and adds a checkmark on the viewport that has focus

    QtViewport* viewport = m_mainWindow->GetActiveViewport();
    QString activeViewportName = viewport ? viewport->GetName() : QString();

    for (QAction* action : m_viewportViewsMenu->actions())
    {
        action->setChecked(action->objectName() == activeViewportName);
    }
}
