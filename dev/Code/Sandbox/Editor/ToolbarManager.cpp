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
#include "ToolbarManager.h"
#include "ToolBox.h"
#include "ActionManager.h"
#include "MainWindow.h"
#include "IGemManager.h"
#include "ToolBox.h"

#include <QDataStream>
#include <QDebug>
#include <QChildEvent>
#include <QToolButton>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QStyle>
#include <QToolBar>
#include <QComboBox>
#include <QWidgetAction>

static const char* SUBSTANCE_GEM_UUID = "a2f08ba9713f485a8485d7588e5b120f";
static const char* SUBSTANCE_TOOLBAR_NAME = "Substance";
static const QString TOOLBAR_SETTINGS_KEY("ToolbarSettings");


// Save out the version of the toolbars with it
// Only save a toolbar if it's not a standard or has some changes to it from the standard
// On load, add any actions that are with a newer version to it
// Check if a toolbar is the same as a default version on load

const int TOOLBAR_IDENTIFIER = 0xFFFF; // must be an int, for compatibility

enum AmazonToolbarVersions
{
    ORIGINAL_TOOLBAR_VERSION = 1,
    TOOLBARS_WITH_PLAY_GAME = 2,

    //TOOLBAR_VERSION = 1
    TOOLBAR_VERSION = TOOLBARS_WITH_PLAY_GAME
};

struct InternalAmazonToolbarList
{
    int version;
    AmazonToolbar::List toolbars;
};

Q_DECLARE_METATYPE(AmazonToolbar)
Q_DECLARE_METATYPE(AmazonToolbar::List)
Q_DECLARE_METATYPE(InternalAmazonToolbarList)

static bool ObjectIsSeparator(QObject* o)
{
    return o && o->metaObject()->className() == QStringLiteral("QToolBarSeparator");
}

static QDataStream& WriteToolbarDataStream(QDataStream& out, const AmazonToolbar& toolbar)
{
    out << toolbar.GetName();
    out << toolbar.GetTranslatedName();
    out << toolbar.ActionIds();
    out << toolbar.IsShowByDefault();

    return out;
}

static QDataStream& ReadToolbarDataStream(QDataStream& in, AmazonToolbar& toolbar, int version)
{
    QString name;
    QString translatedName;
    QVector<int> actionIds;

    bool showByDefault = true;

    in >> name;
    in >> translatedName;

    in >> actionIds;

    if (version > 0)
    {
        in >> showByDefault;
        toolbar.SetShowByDefault(showByDefault);
    }

    for (int actionId : actionIds)
    {
        toolbar.AddAction(actionId);
    }

    toolbar.SetName(name, translatedName);

    return in;
}

static QDataStream& operator<<(QDataStream& out, const InternalAmazonToolbarList& list)
{
    out << TOOLBAR_IDENTIFIER;
    out << list.version;
    out << list.toolbars.size();
    for (const AmazonToolbar& t : list.toolbars)
    {
        WriteToolbarDataStream(out, t);
    }

    return out;
}

static QDataStream& operator>>(QDataStream& in, InternalAmazonToolbarList& list)
{
    int identifier;

    in >> identifier;

    int size = 0;
    if (identifier == TOOLBAR_IDENTIFIER)
    {
        in >> list.version;
        in >> size;
    }
    else
    {
        // version 0; size is identifier
        list.version = 0;
        size = identifier;
    }

    size = std::min(size, 30); // protect against corrupt data
    list.toolbars.reserve(size);
    for (int i = 0; i < size; ++i)
    {
        AmazonToolbar t;
        ReadToolbarDataStream(in, t, list.version);
        list.toolbars.push_back(t);
    }

    return in;
}

ToolbarManager::ToolbarManager(ActionManager* actionManager, MainWindow* mainWindow)
    : m_mainWindow(mainWindow)
    , m_actionManager(actionManager)
    , m_settings("amazon", "lumberyard")
{
    // Note that we don't actually save/load from AmazonToolbar::List
    // The data saved for existing users had that name, and it can't be changed now without ignoring user's data.
    // We need to know the version stored, so we need to save/load into a different structure (InternalAmazonToolbarList)
    qRegisterMetaType<InternalAmazonToolbarList>("AmazonToolbar::List");
    qRegisterMetaTypeStreamOperators<InternalAmazonToolbarList>("AmazonToolbar::List");
}

ToolbarManager::~ToolbarManager()
{
    SaveToolbars();
}

EditableQToolBar* ToolbarManager::ToolbarParent(QObject* o) const
{
    if (!o)
    {
        return nullptr;
    }

    if (auto t = qobject_cast<EditableQToolBar*>(o))
    {
        return t;
    }

    return ToolbarParent(o->parent());
}

void ToolbarManager::LoadToolbars()
{
    InitializeStandardToolbars();

    m_settings.beginGroup(TOOLBAR_SETTINGS_KEY);
    InternalAmazonToolbarList loadedToolbarList = m_settings.value("toolbars").value<InternalAmazonToolbarList>();
    m_toolbars = loadedToolbarList.toolbars;
    m_loadedVersion = loadedToolbarList.version;
    qDebug() << "Loaded toolbars:" << m_toolbars.size();
    m_settings.endGroup();
    SanitizeToolbars();
    InstantiateToolbars();
}

static bool operator==(const AmazonToolbar& t1, const AmazonToolbar& t2)
{
    return t1.GetName() == t2.GetName();
}

static bool operator!=(const AmazonToolbar& t1, const AmazonToolbar& t2)
{
    return !operator==(t1, t2);
}

void ToolbarManager::SanitizeToolbars()
{
    // All standard toolbars must be present
    const auto stdToolbars = m_standardToolbars;
    const int numStandardToolbars = stdToolbars.size();

    // make a set of the loaded toolbars
    QMap<QString, AmazonToolbar> toolbarSet;
    for (const AmazonToolbar& toolbar : m_toolbars)
    {
        toolbarSet[toolbar.GetName()] = toolbar;
    }

    // the order is important because IsCustomToolbar() checks based on the order (which it shouldn't...)
    // so we go through the loaded toolbars and make sure that our standard ones are all in there
    // in the right order. We also remove them from the set, so that we know what's left later
    AmazonToolbar::List newToolbars;
    for (const AmazonToolbar& stdToolbar : stdToolbars)
    {
        auto customToolbarReference = toolbarSet.find(stdToolbar.GetName());
        if (customToolbarReference == toolbarSet.end())
        {
            newToolbars.push_back(stdToolbar);
        }
        else
        {
            // to be sneaky and ensure that default toolbars are laid out as intended,
            // we check if the toolbar we loaded is the same as a previous version of the standard toolbar.
            // If it is, we just use the standard one as is (because it might have newer actions
            // added after the user saved the previous version)
            if (customToolbarReference.value().IsOlderVersionOf(stdToolbar, m_loadedVersion))
            {
                newToolbars.push_back(stdToolbar);
            }
            else
            {
                AmazonToolbar& newToolbar = customToolbarReference.value();

                // make sure to add any actions added since the last time the user saved this toolbar
                newToolbar.AddActionsFromNewerVersion(stdToolbar, m_loadedVersion);

                newToolbars.push_back(newToolbar);
            }

            toolbarSet.remove(stdToolbar.GetName());
        }
    }

    // go through and add in all of the left over toolbars, in the same order now
    for (const AmazonToolbar& existingToolbar : m_toolbars)
    {
        if (toolbarSet.contains(existingToolbar.GetName()))
        {
            newToolbars.push_back(existingToolbar);
        }
    }

    // it isn't an older version of the std toolbar, but it needs to have all of the actions
    // that the newest one has, so add anything newer than what it was saved with
    // WORKS FOR THIS, BUT WHAT ABOUT FOR PLUGIN CREATOR TOOLBARS? HOW DO THEY ADD NEW BUTTONS?

    // keep the new list now
    m_toolbars = newToolbars;

    // TODO: Remove this once gems are able to control toolbars
    bool removeSubstanceToolbar = !IsGemEnabled(SUBSTANCE_GEM_UUID, { ">=1.0" });

    // Remove toolbars with invalid names (corrupted)
    m_toolbars.erase(std::remove_if(m_toolbars.begin(), m_toolbars.end(), [removeSubstanceToolbar](const AmazonToolbar& t)
    {
        return t.GetName().isEmpty() || (removeSubstanceToolbar && t.GetName() == SUBSTANCE_TOOLBAR_NAME);
    }), m_toolbars.end());
}

bool AmazonToolbar::IsOlderVersionOf(const AmazonToolbar& referenceToolbar, int versionNumber)
{
    int index = 0;
    for (auto actionData : referenceToolbar.m_actions)
    {
        if (actionData.toolbarVersionAdded <= versionNumber)
        {
            // make sure we don't go out of bounds
            if (index >= m_actions.size())
            {
                return false;
            }

            if (actionData.actionId != m_actions[index].actionId)
            {
                return false;
            }

            index++;
        }
    }

    return true;
}

void AmazonToolbar::AddActionsFromNewerVersion(const AmazonToolbar& referenceToolbar, int versionNumber)
{
    for (auto actionData : referenceToolbar.m_actions)
    {
        if (actionData.toolbarVersionAdded > versionNumber)
        {
            // new toolbar items should be visible when added to older customized toolbars
            // so instead, push to the front
            m_actions.push_front({ actionData.actionId, actionData.toolbarVersionAdded });
        }
    }
}

void ToolbarManager::SaveToolbar(EditableQToolBar* toolbar)
{
    for (AmazonToolbar& at : m_toolbars)
    {
        if (at.Toolbar() == toolbar)
        {
            at.Clear();
            for (QAction* action : toolbar->actions())
            {
                const int actionId = action->data().toInt();
                if (actionId >= 0)
                {
                    at.AddAction(actionId);
                }
                else
                {
                    qWarning() << Q_FUNC_INFO << "Invalid action id";
                }
            }

            AmazonToolbar::UpdateAllowedAreas(toolbar);
            SaveToolbars();
            return;
        }
    }

    qWarning() << Q_FUNC_INFO << "Couldn't find toolbar";
}

void ToolbarManager::SaveToolbars()
{
    m_settings.beginGroup(TOOLBAR_SETTINGS_KEY);
    InternalAmazonToolbarList savedToolbars = { TOOLBAR_VERSION, m_toolbars };
    m_settings.setValue("toolbars", QVariant::fromValue<InternalAmazonToolbarList>(savedToolbars));
    m_settings.endGroup();
}

void ToolbarManager::InitializeStandardToolbars()
{
    if (m_standardToolbars.isEmpty())
    {
        auto macroToolbars = GetIEditor()->GetToolBoxManager()->GetToolbars();

        m_standardToolbars.reserve(5 + macroToolbars.size());
        m_standardToolbars.push_back(GetEditModeToolbar());
        m_standardToolbars.push_back(GetObjectToolbar());
        m_standardToolbars.push_back(GetEditorsToolbar());

        if (IsGemEnabled("a2f08ba9713f485a8485d7588e5b120f", ">=1.0"))
        {
            m_standardToolbars.push_back(GetSubstanceToolbar());
        }

        IPlugin* pGamePlugin = GetIEditor()->GetPluginManager()->GetPluginByGUID("{71CED8AB-54E2-4739-AA78-7590A5DC5AEB}");
        IPlugin* pDescriptionEditorPlugin = GetIEditor()->GetPluginManager()->GetPluginByGUID("{4B9B7074-2D58-4AFD-BBE1-BE469D48456A}");
        if (pGamePlugin && pDescriptionEditorPlugin)
        {
            m_standardToolbars.push_back(GetMiscToolbar());
        }

        std::copy(std::begin(macroToolbars), std::end(macroToolbars), std::back_inserter(m_standardToolbars));
    }
}

void AmazonToolbar::UpdateAllowedAreas()
{
    UpdateAllowedAreas(m_toolbar);
}

void AmazonToolbar::UpdateAllowedAreas(QToolBar* toolbar)
{
    bool horizontalOnly = false;

    for (QAction* action : toolbar->actions())
    {
        if (qobject_cast<QWidgetAction*>(action))
        {
            // if it's a widget action, we assume it won't fit in the vertical toolbars
            horizontalOnly = true;
            break;
        }
    }

    if (horizontalOnly)
    {
        toolbar->setAllowedAreas(Qt::BottomToolBarArea | Qt::TopToolBarArea);
    }
    else
    {
        toolbar->setAllowedAreas(Qt::AllToolBarAreas);
    }
}

bool ToolbarManager::IsGemEnabled(const QString& uuid, const QString& version) const
{
    const auto gemId = AZ::Uuid::CreateString(uuid.toLatin1().data());
    return GetISystem()->GetGemManager()->IsGemEnabled(gemId, { version.toLatin1().data() });
}

AmazonToolbar ToolbarManager::GetEditModeToolbar() const
{
    const bool applyHoverEffect = true;
    AmazonToolbar t = AmazonToolbar("EditMode", QObject::tr("EditMode Toolbar"), applyHoverEffect);
    t.AddAction(ID_TOOLBAR_WIDGET_UNDO, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_REDO, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_SEPARATOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITTOOL_LINK, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITTOOL_UNLINK, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_SEPARATOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_SELECTION_MASK, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITMODE_SELECT, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITMODE_MOVE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITMODE_ROTATE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITMODE_SCALE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDITMODE_SELECTAREA, ORIGINAL_TOOLBAR_VERSION);

    t.AddAction(ID_VIEW_SWITCHTOGAME, TOOLBARS_WITH_PLAY_GAME);

    t.AddAction(ID_TOOLBAR_SEPARATOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_REF_COORD, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECT_AXIS_X, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECT_AXIS_Y, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECT_AXIS_Z, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECT_AXIS_XY, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECT_AXIS_TERRAIN, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECT_AXIS_SNAPTOALL, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_SNAP_GRID, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_SNAP_ANGLE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_RULER, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_SEPARATOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_SELECT_OBJECT, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECTION_DELETE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECTION_SAVE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_SELECTION_LOAD, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_SEPARATOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_WIDGET_LAYER_SELECT, ORIGINAL_TOOLBAR_VERSION);

    return t;
}
        
AmazonToolbar ToolbarManager::GetObjectToolbar() const
{
    const bool applyHoverEffect = true;
    AmazonToolbar t = AmazonToolbar("Object", QObject::tr("Object Toolbar"), applyHoverEffect);
    t.AddAction(ID_GOTO_SELECTED, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OBJECTMODIFY_ALIGN, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OBJECTMODIFY_ALIGNTOGRID, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OBJECTMODIFY_SETHEIGHT, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_MODIFY_ALIGNOBJTOSURF, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TOOLBAR_SEPARATOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDIT_FREEZE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDIT_UNFREEZEALL, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OBJECTMODIFY_VERTEXSNAPPING, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDIT_PHYS_RESET, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDIT_PHYS_GET, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_EDIT_PHYS_SIMULATE, ORIGINAL_TOOLBAR_VERSION);

    return t;
}
    
AmazonToolbar ToolbarManager::GetEditorsToolbar() const
{
    const bool applyHoverEffect = true;
    AmazonToolbar t = AmazonToolbar("Editors", QObject::tr("Editors Toolbar"), applyHoverEffect);
    t.AddAction(ID_OPEN_LAYER_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_MATERIAL_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_CHARACTER_TOOL, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_MANNEQUIN_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_FLOWGRAPH, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_AIDEBUGGER, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_TRACKVIEW, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_AUDIO_CONTROLS_BROWSER, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_TERRAIN_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_TERRAINTEXTURE_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_PARTICLE_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_TERRAIN_TIMEOFDAYBUTTON, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_GENERATORS_LIGHTING, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_DATABASE, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_UICANVASEDITOR, ORIGINAL_TOOLBAR_VERSION);

    return t;
}
        
AmazonToolbar ToolbarManager::GetSubstanceToolbar() const
{
    AmazonToolbar t = AmazonToolbar("Substance", QObject::tr("Substance Toolbar"));
    t.AddAction(ID_OPEN_SUBSTANCE_EDITOR, ORIGINAL_TOOLBAR_VERSION);
    return t;
}
        
AmazonToolbar ToolbarManager::GetMiscToolbar() const
{
    AmazonToolbar t = AmazonToolbar("Misc", QObject::tr("Misc Toolbar"));
    t.AddAction(ID_GAMEP1_AUTOGEN, ORIGINAL_TOOLBAR_VERSION);
    t.AddAction(ID_OPEN_ASSET_BROWSER, ORIGINAL_TOOLBAR_VERSION);
    return t;
}

const AmazonToolbar* ToolbarManager::FindDefaultToolbar(const QString& toolbarName) const
{
    for (const AmazonToolbar& toolbar : m_standardToolbars)
    {
        if (toolbar.GetName() == toolbarName)
        {
            return &toolbar;
        }
    }

    return nullptr;
}

AmazonToolbar* ToolbarManager::FindToolbar(const QString& toolbarName)
{
    for (AmazonToolbar& toolbar : m_toolbars)
    {
        if (toolbar.GetName() == toolbarName)
        {
            return &toolbar;
        }
    }

    return nullptr;
}

void ToolbarManager::RestoreToolbarDefaults(const QString& toolbarName)
{
    if (!IsCustomToolbar(toolbarName))
    {
        const AmazonToolbar* defaultToolbar = FindDefaultToolbar(toolbarName);
        AmazonToolbar* existingToolbar = FindToolbar(toolbarName);
        Q_ASSERT(existingToolbar != nullptr);
        const bool isInstantiated = existingToolbar->IsInstantiated();

        if (isInstantiated)
        {
            // We have a QToolBar instance, updated it too
            for (QAction* action : existingToolbar->Toolbar()->actions())
            {
                existingToolbar->Toolbar()->removeAction(action);
            }
        }

        existingToolbar->CopyActions(*defaultToolbar);

        if (isInstantiated)
        {
            existingToolbar->SetActionsOnInternalToolbar(m_actionManager);
            existingToolbar->UpdateAllowedAreas();
        }
        SaveToolbars();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Can only reset standard toolbars";
    }
}

void AmazonToolbar::SetActionsOnInternalToolbar(ActionManager* actionManager)
{
    for (auto actionData : m_actions)
    {
        int actionId = actionData.actionId;

        if (actionId == ID_TOOLBAR_SEPARATOR)
        {
            QAction* action = m_toolbar->addSeparator();
            action->setData(ID_TOOLBAR_SEPARATOR);
        }
        else
        {
            if (actionManager->HasAction(actionId))
            {
                m_toolbar->addAction(actionManager->GetAction(actionId));
            }
        }
    }
}

void ToolbarManager::InstantiateToolbars()
{
    const int numToolbars = m_toolbars.size();
    for (int i = 0; i < numToolbars; ++i)
    {
        InstantiateToolbar(i);
        if (i == 1)
        {
            // Hack. Just copying how it was
            m_mainWindow->addToolBarBreak();
        }
    }
}

void ToolbarManager::InstantiateToolbar(int index)
{
    AmazonToolbar& t = m_toolbars[index];

    // Set up all of the hover effects on the standard toolbars
    if (!IsCustomToolbar(t.GetName()))
    {
        t.SetApplyHoverEffect(true);
    }

    t.InstantiateToolbar(m_mainWindow, this); // Create the QToolbar
}

AmazonToolbar::List ToolbarManager::GetToolbars() const
{
    return m_toolbars;
}

AmazonToolbar ToolbarManager::GetToolbar(int index)
{
    if (index < 0 || index >= m_toolbars.size())
    {
        return AmazonToolbar();
    }

    return m_toolbars.at(index);
}

bool ToolbarManager::Delete(int index)
{
    if (!IsCustomToolbar(index))
    {
        qWarning() << Q_FUNC_INFO << "Won't try to delete invalid or standard toolbar" << index << m_toolbars.size();
        return false;
    }

    AmazonToolbar t = m_toolbars.takeAt(index);
    delete t.Toolbar();

    SaveToolbars();

    return true;
}


bool ToolbarManager::Rename(int index, const QString& newName)
{
    if (newName.isEmpty())
    {
        return false;
    }

    if (!IsCustomToolbar(index))
    {
        qWarning() << Q_FUNC_INFO << "Won't try to rename invalid or standard toolbar" << index << m_toolbars.size();
        return false;
    }

    AmazonToolbar& t = m_toolbars[index];
    if (t.GetName() == newName)
    {
        qWarning() << Q_FUNC_INFO << "Won't try to rename to the same name" << newName;
        return false;
    }
    t.SetName(newName, newName); // No translation for custom bars
    SaveToolbars();

    return true;
}

int ToolbarManager::Add(const QString& name)
{
    if (name.isEmpty())
    {
        return -1;
    }

    AmazonToolbar t(name, name);
    t.InstantiateToolbar(m_mainWindow, this);

    m_toolbars.push_back(t);
    SaveToolbars();
    return m_toolbars.size() - 1;
}

bool ToolbarManager::IsCustomToolbar(int index) const
{
    return IsCustomToolbar(m_toolbars[index].GetName());
}

bool ToolbarManager::IsCustomToolbar(const QString& toolbarName) const
{
    for (auto toolbar : m_standardToolbars)
    {
        if (toolbar.GetName() == toolbarName)
        {
            return false;
        }
    }
    
    return true;
}

ActionManager* ToolbarManager::GetActionManager() const
{
    return m_actionManager;
}

bool ToolbarManager::DeleteAction(QAction* action, EditableQToolBar* toolbar)
{
    if (!action)
    {
        // Doesn't happen
        qWarning() << Q_FUNC_INFO << "Null action!";
        return false;
    }

    const int actionId = action->data().toInt();
    if (actionId <= 0)
    {
        qWarning() << Q_FUNC_INFO << "Action has null id";
        return false;
    }

    if (toolbar->actions().contains(action))
    {
        toolbar->removeAction(action);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Couldnt find action to remove";
        return false;
    }

    SaveToolbar(toolbar);
    return true;
}

void ToolbarManager::SetIsEditingToolBars(bool is)
{
    m_isEditingToolBars = is;
}

bool ToolbarManager::IsEditingToolBars() const
{
    return m_isEditingToolBars;
}

void ToolbarManager::InsertAction(QAction* action, QWidget* beforeWidget, QAction* beforeAction, EditableQToolBar* toolbar)
{
    if (!action)
    {
        qWarning() << Q_FUNC_INFO << "Invalid action for id" << action;
        return;
    }

    const int actionId = action->data().toInt();
    if (actionId <= 0)
    {
        qWarning() << Q_FUNC_INFO << "Invalid action id";
        return;
    }

    const int beforeActionId = beforeAction ? beforeAction->data().toInt() : -1;
    const bool beforeIsSeparator = beforeActionId == ID_TOOLBAR_SEPARATOR;

    if (beforeIsSeparator)
    {
        beforeAction = beforeWidget->actions().first();
    }

    if (beforeAction && !toolbar->actions().contains(beforeAction))
    {
        qWarning() << Q_FUNC_INFO << "Invalid before action" << beforeAction << beforeActionId << beforeWidget->actions();
        return;
    }

    toolbar->insertAction(beforeAction, action);

    SaveToolbar(toolbar);
}

class DnDIndicator
    : public QWidget
{
public:
    DnDIndicator(EditableQToolBar* parent)
        : QWidget(parent)
        , m_toolbar(parent)
    {
        setVisible(false);
    }

    void paintEvent(QPaintEvent* ev) override
    {
        QPainter painter(this);
        painter.fillRect(QRect(0, 0, width(), height()), QBrush(QColor(217, 130, 46)));
    }

    void setLastDragPos(QPoint lastDragPos)
    {
        if (lastDragPos != m_lastDragPos)
        {
            m_lastDragPos = lastDragPos;
            if (lastDragPos.isNull())
            {
                m_dragSourceWidget = nullptr;
                setVisible(false);
            }
            else
            {
                setVisible(true);
                updatePosition();
            }
            update();
        }
    }

    void setDragSourceWidget(QWidget* w)
    {
        m_dragSourceWidget = w;
    }

    void updatePosition()
    {
        QWidget* beforeWidget = m_toolbar->insertPositionForDrop(m_lastDragPos);
        const auto widgets = m_toolbar->childWidgetsWithActions();
        QWidget* lastWidget = widgets.isEmpty() ? nullptr : widgets.last();

        if (beforeWidget && beforeWidget == m_dragSourceWidget)
        {
            // Nothing to do, user is dragging to the same place, don't indicate it as a possibility
            setVisible(false);
            return;
        }

        if (!beforeWidget && m_dragSourceWidget == lastWidget)
        {
            // Nothing to do. Don't show indicator. The widget is already at the end.
            setVisible(false);
            return;
        }

        int x = 0;
        if (beforeWidget)
        {
            x = beforeWidget->pos().x();
        }
        else
        {
            if (lastWidget)
            {
                x = lastWidget->pos().x() + lastWidget->width();
            }
            else
            {
                x = style()->pixelMetric(QStyle::PM_ToolBarHandleExtent) + style()->pixelMetric(QStyle::PM_ToolBarItemSpacing);
            }
        }

        const int w = 2;
        const int y = 5;
        const int h = m_toolbar->height() - (y * 2);
        setGeometry(x, y, w, h);
        raise();
    }

    QPoint lastDragPos() const
    {
        return m_lastDragPos;
    }

private:
    QPoint m_lastDragPos;
    QPointer<QWidget> m_dragSourceWidget = nullptr;
    EditableQToolBar* const m_toolbar;
};

EditableQToolBar::EditableQToolBar(const QString& title, ToolbarManager* manager)
    : QToolBar(title)
    , m_toolbarManager(manager)
    , m_actionManager(manager->GetActionManager())
    , m_dndIndicator(new DnDIndicator(this))
{
    setAcceptDrops(true);

    connect(this, &QToolBar::orientationChanged, [this](Qt::Orientation orientation)
    {
        for (const auto widget : findChildren<QWidget*>())
            layout()->setAlignment(widget, orientation == Qt::Horizontal ? Qt::AlignVCenter : Qt::AlignHCenter);
    });
}

QWidget* EditableQToolBar::insertPositionForDrop(QPoint mousePos)
{
    // QToolBar::actionAt() is no good here, since it sometimes returns nullptr between widgets

    const QList<QWidget*> widgets = childWidgetsWithActions();
    // Find the closest button
    QWidget* beforeWidget = nullptr;
    for (auto w : widgets)
    {
        if (w && w->pos().x() + w->width() / 2 > mousePos.x())
        {
            beforeWidget = w;
            break;
        }
    }

    return beforeWidget;
}

void EditableQToolBar::childEvent(QChildEvent* ev)
{
    QObject* child = ev->child();
    if (ev->type() == QEvent::ChildAdded && child->isWidgetType()) // we can't qobject_cast to QToolButton yet, since it's not fully constructed
    {
        child->installEventFilter(this);
    }

    QToolBar::childEvent(ev);
}

QList<QWidget*> EditableQToolBar::childWidgetsWithActions() const
{
    QList<QWidget*> widgets;
    widgets.reserve(actions().size());
    for (QAction* action : actions())
    {
        if (QWidget* w = widgetForAction(action))
        {
            if (w->actions().isEmpty() && ObjectIsSeparator(w))
            {
                // Hack around the fact that QToolBarSeparator doesn't have an action associated
                w->addAction(action);
                action->setData(ID_TOOLBAR_SEPARATOR);
            }
            widgets.push_back(w);
        }
    }

    return widgets;
}

bool EditableQToolBar::eventFilter(QObject* obj, QEvent* ev)
{
    auto type = ev->type();
    const bool isMouseEvent = type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease ||
        type == QEvent::MouseButtonDblClick || type == QEvent::MouseMove;

    auto* sourceWidget = qobject_cast<QWidget*>(obj);
    if (!m_toolbarManager->IsEditingToolBars() || !isMouseEvent || !sourceWidget)
    {
        return QToolBar::eventFilter(obj, ev);
    }

    QAction* sourceAction = ActionForWidget(sourceWidget);
    if (!sourceAction)
    {
        qWarning() << Q_FUNC_INFO << "Source widget" << sourceWidget << "doesn't have actions";
        return QToolBar::eventFilter(obj, ev);
    }

    if (ev->type() == QEvent::MouseButtonPress)
    {
        QAction* action = ActionForWidget(sourceWidget);
        const int actionId = action ? action->data().toInt() : 0;
        if (actionId <= 0)
        {
            // Doesn't happen
            qWarning() << Q_FUNC_INFO << "Invalid action id for widget" << sourceWidget << action << actionId;
            return false;
        }

        QDrag* drag = new QDrag(sourceWidget);

        { // Nested scope so painter gets deleted before we enter nested event-loop of QDrag::exec().
          // Otherwise QPainter dereferences invalid pointer because QWidget was deleted already
            QPixmap iconPixmap(sourceWidget->size());
            QPainter painter(&iconPixmap);
            sourceWidget->render(&painter);
            drag->setPixmap(iconPixmap);
        }

        QMimeData* mimeData = new QMimeData();
        mimeData->setText(action->text());
        drag->setMimeData(mimeData);

        Qt::DropAction dropAction = drag->exec();
        m_dndIndicator->setLastDragPos(QPoint());
        return true;
    }
    else if (ev->type() == QEvent::MouseButtonPress)
    {
        if (QWidget* w = qobject_cast<QWidget*>(obj))
        {
            qDebug() << w->actions();
        }
    }
    else if (isMouseEvent)
    {
        return true;
    }

    return QToolBar::eventFilter(obj, ev);
}

QAction* EditableQToolBar::actionFromDrop(QDropEvent* ev) const
{
    if (ev->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
    {
        // The drag originated in ToolbarCustomizationDialog's list view of commands, decode it
        QByteArray encoded = ev->mimeData()->data("application/x-qabstractitemmodeldatalist");
        QDataStream stream(&encoded, QIODevice::ReadOnly);

        if (!stream.atEnd())
        {
            int row, col;
            QMap<int,  QVariant> roleDataMap;
            stream >> row >> col >> roleDataMap;
            const int actionId = roleDataMap.value(ActionRole).toInt();
            if (actionId > 0)
            {
                return m_actionManager->GetAction(actionId);
            }
        }
    }
    else if (auto w = qobject_cast<QWidget*>(ev->source()))
    {
        return ActionForWidget(w);
    }

    return nullptr;
}

QAction* EditableQToolBar::ActionForWidget(QWidget* w) const
{
    EditableQToolBar* toolbar = m_toolbarManager->ToolbarParent(w);
    if (!toolbar)
    {
        qWarning() << Q_FUNC_INFO << "Couldn't find parent toolbar for widget" << w;
        return nullptr;
    }

    // Does the reverse of QToolBar::widgetForAction()
    // Useful because only QToolButtons have actions, separators and custom widgets
    // return an empty actions list.

    for (QAction* action : toolbar->actions())
    {
        if (w == toolbar->widgetForAction(action))
        {
            return action;
        }
    }

    return nullptr;
}

void EditableQToolBar::dropEvent(QDropEvent* ev)
{
    auto srcWidget = qobject_cast<QWidget*>(ev->source());
    QAction* action = actionFromDrop(ev);
    if (!action || !srcWidget)
    { // doesn't happen
        qDebug() << Q_FUNC_INFO << "null action or widget" << action << srcWidget;
        return;
    }

    const int actionId = action->data().toInt();
    QWidget* beforeWidget = insertPositionForDrop(ev->pos());
    QAction* beforeAction = beforeWidget ? ActionForWidget(beforeWidget) : nullptr;

    if (beforeAction == action)
    {// Same place, nothing to do
        m_dndIndicator->setLastDragPos(QPoint());
        return;
    }

    if (EditableQToolBar* sourceToolbar = m_toolbarManager->ToolbarParent(srcWidget)) // If we're dragging from a QToolBar (instead of the customization dialog)
    {
        if (!m_toolbarManager->DeleteAction(sourceToolbar->ActionForWidget(srcWidget), sourceToolbar))
        {
            qWarning() << Q_FUNC_INFO << "Failed to delete source widget" << srcWidget;
            return;
        }
    }
    m_toolbarManager->InsertAction(action, beforeWidget, beforeAction, this);
    m_dndIndicator->setLastDragPos(QPoint());
}

void EditableQToolBar::dragEnterEvent(QDragEnterEvent* ev)
{
    dragMoveEvent(ev); // Same code to run
}

void EditableQToolBar::dragMoveEvent(QDragMoveEvent* ev)
{
    if (!m_toolbarManager->IsEditingToolBars())
    {
        return;
    }

    // We support dragging from a QToolBar but also from ToolbarCustomizationDialog's list view of commands
    auto sourceWidget = qobject_cast<QWidget*>(ev->source());
    if (!sourceWidget)
    {
        qWarning() << Q_FUNC_INFO << "Ignoring drag, widget is null";
        return;
    }

    const bool valid = ev->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") || ActionForWidget(sourceWidget) != nullptr;
    if (valid)
    {
        m_dndIndicator->setDragSourceWidget(sourceWidget);
        m_dndIndicator->setLastDragPos(ev->pos());
        ev->accept();
        update();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Ignoring drag. Widget=" << ev->source();
        m_dndIndicator->setLastDragPos(QPoint());
        ev->ignore();
    }
}

void EditableQToolBar::dragLeaveEvent(QDragLeaveEvent* ev)
{
    if (!m_toolbarManager->IsEditingToolBars())
    {
        return;
    }

    if (!m_dndIndicator->lastDragPos().isNull())
    {
        m_dndIndicator->setLastDragPos(QPoint());
        ev->accept();
        update();
    }
    else
    {
        ev->ignore();
    }
}

AmazonToolbar::AmazonToolbar(const QString& name, const QString& translatedName, bool applyHoverEffect)
    : m_name(name)
    , m_translatedName(translatedName)
    , m_applyHoverEffect(applyHoverEffect)
{
}

void AmazonToolbar::InstantiateToolbar(QMainWindow* mainWindow, ToolbarManager* manager)
{
    Q_ASSERT(!m_toolbar);
    m_toolbar = new EditableQToolBar(m_translatedName, manager);
    m_toolbar->setObjectName(m_name);
    m_toolbar->setIconSize(QSize(32, 32));
    mainWindow->addToolBar(m_toolbar);

    // Hide custom toolbars if they've been flagged that way.
    // This only applies to toolbars the user hasn't seen already, because the saveState/restoreState on the Editor's MainWindow will
    // show/hide based on what the user did last time the editor loaded
    if (!m_showByDefault)
    {
        m_toolbar->hide();
    }

    // Our standard toolbars's icons, when hovered on, get a white color effect.
    // but for this to work we need .pngs that look good with this effect, so this only works with the standard toolbars
    // and looks very ugly for other toolbars, including toolbars loaded from XML (which just show a white rectangle)
    if (m_applyHoverEffect)
    {
        m_toolbar->setProperty("IconsHaveHoverEffect", true);
    }

    ActionManager* actionManager = manager->GetActionManager();
    actionManager->AddToolBar(m_toolbar);

    SetActionsOnInternalToolbar(actionManager);

    UpdateAllowedAreas();
}

void AmazonToolbar::AddAction(int actionId, int toolbarVersionAdded)
{
    m_actions.push_back({ actionId, toolbarVersionAdded });
}

void AmazonToolbar::Clear()
{
    m_actions.clear();
}

QVector<int> AmazonToolbar::ActionIds() const
{
    QVector<int> ret;
    ret.reserve(m_actions.size());

    for (auto actionData : m_actions)
    {
        ret.push_back(actionData.actionId);
    }

    return ret;
}

void AmazonToolbar::SetName(const QString& name, const QString& translatedName)
{
    m_name = name;
    m_translatedName = translatedName;
    if (m_toolbar)
    {
        m_toolbar->setWindowTitle(translatedName);
    }
}

void AmazonToolbar::SetApplyHoverEffect(bool applyHoverEffect)
{
    m_applyHoverEffect = applyHoverEffect;
    Q_ASSERT(m_toolbar == nullptr); // this must be called before the toolbar is instantiated
}


#include <ToolbarManager.moc>
