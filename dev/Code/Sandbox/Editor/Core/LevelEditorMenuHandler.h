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
#ifndef LEVELEDITORMENUHANDLER_H
#define LEVELEDITORMENUHANDLER_H

#include <QObject>
#include <QString>
#include <QMenu>
#include <QPointer>
#include "ActionManager.h"

class MainWindow;
class QtViewPaneManager;
class NetPromoterScoreDialog;
class QSettings;

class LevelEditorMenuHandler
    : public QObject
{
public:
    LevelEditorMenuHandler(MainWindow* mainWindow, QtViewPaneManager* const viewPaneManager, QSettings& settings);

    void Initialize();
    void ShowMenus();
    void ShowMenus(bool updateRegistryKey);

    static bool MRUEntryIsValid(const QString& entry, const QString& gameFolderPath);
    static const char* GetSwitchMenuSettingName();

    void IncrementViewPaneVersion();
    int GetViewPaneVersion() const;

    void UpdateViewLayoutsMenu(ActionManager::MenuWrapper& layoutsMenu);

    void ResetToolsMenus();

    QAction* CreateViewPaneAction(const QtViewPane* view);

private:
    QMenu* CreateFileMenu();
    QMenu* CreateEditMenu();
    QMenu* CreateGameMenu();
    QMenu* CreateToolsMenu();
    QMenu* CreateAWSMenu();
    QMenu* CreateViewMenu();
    QMenu* CreateHelpMenu();

    QMap<QString, QList<QtViewPane*>> CreateMenuMap(QMap<QString, QList<QtViewPane*>>& menuMap, QtViewPanes& allRegisteredViewPanes);
    void CreateMenuOptions(QMap<QString, QList<QtViewPane*>>* menuMap, ActionManager::MenuWrapper& menu, const char* category);

    void CreateDebuggingSubMenu(ActionManager::MenuWrapper gameMenu);

    void UpdateMRUFiles();
    //void UpdateMRUProjects();

    void ActivateGemConfiguration();
    void ClearAll();
    void ToggleSelection(bool hide);
    void ShowLastHidden();
    void OnUpdateOpenRecent();
    void OnOpenAssetEditor();

    void OnUpdateMacrosMenu();
    
    void UpdateOpenViewPaneMenu();

    void CreateDisabledPlaceholderAction(ActionManager::MenuWrapper& menu, int id);

    QAction* CopyActionWithoutIcon(ActionManager::MenuWrapper& menu, QAction* originalAction, const char* menuOptionName, bool copyShortcut /*= true*/);
    QAction* CopyActionWithoutIcon(ActionManager::MenuWrapper& menu, int actionId, const char* menuOptionName, bool copyShortcut = true);

    QAction* CreateViewPaneMenuItem(ActionManager* actionManager, ActionManager::MenuWrapper& menu, const QtViewPane* view);

    void InitializeViewPaneMenu(ActionManager* actionManager, ActionManager::MenuWrapper& menu, AZStd::function < bool(const QtViewPane& view)> functor);

    void LoadComponentLayout();
    void LoadLegacyLayout();

    // It's used when users update the Tool Box Macro list in the Configure Tool Box Macro dialog
    void UpdateMacrosMenu();

    void LoadNetPromoterScoreDialog(ActionManager::MenuWrapper& menu);
    
    MainWindow* m_mainWindow;
    ActionManager* m_actionManager;
    QtViewPaneManager* m_viewPaneManager;

    QPointer<QMenu> m_viewportViewsMenu;

    ActionManager::MenuWrapper m_toolsMenu;

    QMenu* m_mostRecentLevelsMenu = nullptr;
    QMenu* m_mostRecentProjectsMenu = nullptr;
    ActionManager::MenuWrapper m_cloudMenu;

    ActionManager::MenuWrapper m_viewPanesMenu;
    ActionManager::MenuWrapper m_layoutsMenu;
    ActionManager::MenuWrapper m_macrosMenu;

    int m_viewPaneVersion = 0;

    QList<QMenu*> m_topLevelMenus;
    QSettings& m_settings;
};


#endif // LEVELEDITORMENUHANDLER_H
