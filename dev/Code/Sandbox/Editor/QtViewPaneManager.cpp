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
#include "QtViewPaneManager.h"
#include "Controls/ConsoleSCB.h"
#include "Controls/RollupBar.h"
#include "fancydocking.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QDataStream>
#include <QDebug>
#include <QCloseEvent>
#include <QApplication>
#include <QRect>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QRubberBand>
#include <QCursor>
#include <QTimer>
#include <QStackedWidget>
#include "MainWindow.h"

#include <algorithm>
#include <QScopedValueRollback>

#include "DockWidgetUtils.h"

#include <AzAssetBrowser/AzAssetBrowserWindow.h>
#include <AzQtComponents/Utilities/AutoSettingsGroup.h>

struct ViewLayoutState
{
    QVector<QString> viewPanes;
    QByteArray mainWindowState;
};
Q_DECLARE_METATYPE(ViewLayoutState)

static QDataStream &operator<<(QDataStream & out, const ViewLayoutState&myObj)
{
    out << myObj.viewPanes << myObj.mainWindowState;
    return out;
}

static QDataStream& operator>>(QDataStream& in, ViewLayoutState& myObj)
{
    in >> myObj.viewPanes;
    in >> myObj.mainWindowState;
    return in;
}



// All settings keys for stored layouts are in the form "layouts/<name>"
// When starting up, "layouts/last" is loaded
static QLatin1String s_lastLayoutName = QLatin1String("last");

static QString GetViewPaneStateGroupName()
{
    return QString("%1/%2").arg("Editor").arg("mainWindowLayouts");
}

static QString GetFancyViewPaneStateGroupName()
{
    return QString("%1/%2").arg("Editor").arg("fancyWindowLayouts");
}

Q_GLOBAL_STATIC(QtViewPaneManager, s_instance)


/**
 * Check if this dock widget is tabbed in our custom dock tab widget
 */
bool QtViewPane::IsTabbed() const
{
    // If our dock widget is tabbed, it will have a valid tab widget parent
    return ParentTabWidget();
}

/**
 * Return the tab widget holding this dock widget if it is a tab, otherwise nullptr
 */
AzQtComponents::DockTabWidget* QtViewPane::ParentTabWidget() const
{
    if (m_dockWidget)
    {
        // If our dock widget is tabbed, it will be parented to a QStackedWidget that is parented to
        // our dock tab widget
        QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(m_dockWidget->parentWidget());
        if (stackedWidget)
        {
            AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(stackedWidget->parentWidget());
            return tabWidget;
        }
    }

    return nullptr;
}

bool QtViewPane::Close(QtViewPane::CloseModes closeModes)
{
    if (!IsConstructed())
    {
        return true;
    }

    bool canClose = true;
    bool destroy = closeModes & CloseMode::Destroy;

    // Console is not deletable, so always hide it instead of destroying
    if (!m_options.isDeletable)
    {
        destroy = false;
    }

    if (!(closeModes & CloseMode::Force))
    {
        QCloseEvent closeEvent;

        // Prevent closing view pane if it has modal dialog open, as modal dialogs
        // are often constructed on stack and will not finish properly when the view
        // pane is destroyed.
        QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        const int numTopLevel = topLevelWidgets.size();
        for (size_t i = 0; i < numTopLevel; ++i)
        {
            QWidget* widget = topLevelWidgets[i];
            if (widget->isModal() && widget->isVisible())
            {
                widget->activateWindow();
                return false;
            }
        }

        // Check if embedded QWidget allows view pane to be closed.
        QCoreApplication::sendEvent(Widget(), &closeEvent);
        // If widget accepted the close event, we delete the dockwidget, which will also delete the child widget in case it doesn't have Qt::WA_DeleteOnClose
        if (!closeEvent.isAccepted())
        {
            // Widget doesn't want to close
            canClose = false;
        }
    }

    if (canClose)
    {
        if (destroy)
        {
            //important to set parent to null otherwise docking code will still find it while restoring since that happens before the delete.
            m_dockWidget->setParent(nullptr);
            m_dockWidget->deleteLater();

            //clear dockwidget pointer otherwise if we open this pane before the delete happens we'll think it's already there, then it gets deleted on us.
            m_dockWidget.clear();
        }
        else
        {
            // If the dock widget is tabbed, then just remove it from the tab widget
            AzQtComponents::DockTabWidget* tabWidget = ParentTabWidget();
            if (tabWidget)
            {
                tabWidget->removeTab(m_dockWidget);
            }
            // Otherwise just hide the widget
            else
            {
                m_dockWidget->hide();
            }
        }
    }

    return canClose;
}

DockWidget::DockWidget(QWidget* widget, QtViewPane* pane, QSettings* settings, QMainWindow* parent, FancyDocking* advancedDockManager)
    : AzQtComponents::StyledDockWidget(pane->m_name, parent)
    , m_settings(settings)
    , m_mainWindow(parent)
    , m_pane(pane)
    , m_advancedDockManager(advancedDockManager)
{
    if (pane->m_options.isDeletable)
    {
        setAttribute(Qt::WA_DeleteOnClose);
    }

    setObjectName(pane->m_name);

    setWidget(widget);
    setFocusPolicy(Qt::StrongFocus);

    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
}

bool DockWidget::event(QEvent* qtEvent)
{
    // this accounts for a difference in behavior where we want all floating windows to be always parented to the main window instead of to each other, so that
    // they don't overlap in odd ways - for example, if you tear off a floating window from another floating window, under Qt's system its technically still a child of that window
    // so that window can't ever be placed on top of it.  This is not what we want.  We want you to be able to then take that window and drag it into this new one.
    // (Qt's original behavior is like that so if you double click on a floating widget it docks back into the parent which it came from - we don't use this functionality)
    if (qtEvent->type() == QEvent::WindowActivate)
    {
        reparentToMainWindowFix();
    }

    return QDockWidget::event(qtEvent);
}

void DockWidget::reparentToMainWindowFix()
{
    if (!isFloating() || !DockWidgetUtils::isDockWidgetWindowGroup(parentWidget()))
    {
        return;
    }

    if (qApp->mouseButtons() & Qt::LeftButton)
    {
        // We're still dragging, lets try later
        QTimer::singleShot(200, this, &DockWidget::reparentToMainWindowFix);
        return;
    }

    // bump it up and to the left by the size of its frame, to account for the reparenting operation;
    QPoint framePos = pos();
    QPoint contentPos = mapToGlobal(QPoint(0, 0));
    move(framePos.x() - (contentPos.x() - framePos.x()), framePos.y() - (contentPos.y() - framePos.y()));

    // we have to dock this to the mainwindow, even if we're floating, so that the mainwindow knows about it.
    // if the preferred area is valid, use that. Otherwise, arbitrarily toss it in the left.
    // This is relevant because it will determine where the widget goes if the title bar is double clicked
    // after it's been detached from a QDockWidgetGroupWindow
    auto dockArea = (m_pane->m_options.preferedDockingArea != Qt::DockWidgetArea::NoDockWidgetArea) ? m_pane->m_options.preferedDockingArea : Qt::LeftDockWidgetArea;

    setParent(m_mainWindow);
    m_mainWindow->addDockWidget(dockArea, this);
    setFloating(true);
}

QString DockWidget::PaneName() const
{
    return m_pane->m_name;
}

void DockWidget::RestoreState(bool forceDefault)
{
    // check if we can get the main window to do all the work for us first
    // (which is also the proper way to do this)
    if (!forceDefault)
    {
        // If the advanced docking is enabled, let it try to restore the dock widget
        bool restored = false;
        if (m_advancedDockManager)
        {
            restored = m_advancedDockManager->restoreDockWidget(this);
        }
        // Otherwise, let our main window do it directly
        else
        {
            restored = m_mainWindow->restoreDockWidget(this);
        }

        if (restored)
        {
            DockWidgetUtils::correctVisibility(this);
            return;
        }
    }

    // can't rely on the main window; fall back to our preferences
    auto dockingArea = m_pane->m_options.preferedDockingArea;
    auto paneRect = m_pane->m_options.paneRect;

    // make sure we're sized properly before we dock
    if (paneRect.isValid())
    {
        resize(paneRect.size());
    }

    // check if we should force floating
    bool floatWidget = (dockingArea == Qt::NoDockWidgetArea);

    // if we're floating, we need to move and resize again, because the act of docking may have moved us
    if (floatWidget)
    {
        // in order for saving and restoring state to work properly in Qt,
        // along with docking widgets within other floating widgets, the widget
        // must be added at least once to the main window, with a VALID area,
        // before we set it to floating.
        auto arbitraryDockingArea = Qt::LeftDockWidgetArea;
        m_mainWindow->addDockWidget(arbitraryDockingArea, this);

        // If we are using the fancy docking, let it handle making the dock
        // widget floating, or else the titlebar will be missing, since
        // floating widgets are actually contained in a floating main
        // window container
        if (m_advancedDockManager)
        {
            m_advancedDockManager->makeDockWidgetFloating(this, paneRect);
        }
        // Otherwise, we can make the dock widget floating directly and move it
        else
        {
            setFloating(true);

            // Not using setGeometry() since it excludes the frame when positioning
            if (paneRect.isValid())
            {
                resize(paneRect.size());
                move(paneRect.topLeft());
            }
        }
    }
    else
    {
        m_mainWindow->addDockWidget(dockingArea, this);
    }
}

QRect DockWidget::ProperGeometry() const
{
    Qt::DockWidgetArea area = isFloating() ? Qt::NoDockWidgetArea : m_mainWindow->dockWidgetArea(const_cast<DockWidget*>(this));

    QRect myGeom(pos(), size());

    // we need this state in global coordinates, but if we're parented to one of those group dock windows, there is a problem, it will be local coords.
    if (!isFloating())
    {
        if (parentWidget() && (strcmp(parentWidget()->metaObject()->className(), "QDockWidgetGroupWindow") == 0))
        {
            myGeom = QRect(parentWidget()->pos(), parentWidget()->size());
        }
    }

    return myGeom;
}

QString DockWidget::settingsKey() const
{
    return settingsKey(m_pane->m_name);
}

QString DockWidget::settingsKey(const QString& paneName)
{
    return QStringLiteral("ViewPane-") + paneName;
}

QtViewPaneManager::QtViewPaneManager(QObject* parent)
    : QObject(parent)
    , m_mainWindow(nullptr)
    , m_settings(nullptr)
    , m_restoreInProgress(false)
    , m_useNewDocking(false)
    , m_advancedDockManager(nullptr)
{
    qRegisterMetaTypeStreamOperators<ViewLayoutState>("ViewLayoutState");
    qRegisterMetaTypeStreamOperators<QVector<QString> >("QVector<QString>");
}

QtViewPaneManager::~QtViewPaneManager()
{
}

static bool lessThan(const QtViewPane& v1, const QtViewPane& v2)
{
    if (v1.IsViewportPane() && v2.IsViewportPane())
    {
        // Registration order (Top, Front, Left ...)
        return v1.m_id < v2.m_id;
    }
    else if (!v1.IsViewportPane() && !v2.IsViewportPane())
    {
        // Sort by name
        return v1.m_name.compare(v2.m_name, Qt::CaseInsensitive) < 0;
    }
    else
    {
        // viewports on top of non-viewports
        return v1.IsViewportPane();
    }
}

void QtViewPaneManager::RegisterPane(const QString& name, const QString& category, ViewPaneFactory factory, const QtViewOptions& options)
{
    QtViewPane view = { NextAvailableId(), name, category, factory, nullptr, options };

    // Sorted insert
    auto it = std::upper_bound(m_registeredPanes.begin(), m_registeredPanes.end(), view, lessThan);
    m_registeredPanes.insert(it, view);

    emit registeredPanesChanged();
}

void QtViewPaneManager::UnregisterPane(const QString& name)
{
    auto it = std::find_if(m_registeredPanes.begin(), m_registeredPanes.end(),
            [name](const QtViewPane& pane) { return name == pane.m_name; });

    if (it != m_registeredPanes.end())
    {
        const QtViewPane& pane = *it;
        m_knownIdsSet.removeOne(pane.m_id);
        m_registeredPanes.erase(it);
        emit registeredPanesChanged();
    }
}

QtViewPaneManager* QtViewPaneManager::instance()
{
    return s_instance();
}

void QtViewPaneManager::SetMainWindow(QMainWindow* mainWindow, QSettings* settings, const QByteArray& lastMainWindowState, bool useNewDocking)
{
    Q_ASSERT(mainWindow && !m_mainWindow && settings && !m_settings);
    m_mainWindow = mainWindow;
    m_settings = settings;
    m_useNewDocking = useNewDocking;
    if (m_useNewDocking)
    {
        m_advancedDockManager = new FancyDocking(mainWindow);
    }

    m_defaultMainWindowState = mainWindow->saveState();
    m_loadedMainWindowState = lastMainWindowState;
}

const QtViewPane* QtViewPaneManager::OpenPane(const QString& name, QtViewPane::OpenModes modes)
{
    QtViewPane* pane = GetPane(name);
    if (!pane || !pane->IsValid())
    {
        qWarning() << Q_FUNC_INFO << "Could not find pane with name" << name;
        return nullptr;
    }

    // this multi-pane code is a bit of an hack to support more than one view of the same class
    // All views are single pane, except for one in Maglev Control plugin
    // Save/Restore support of the duplicates will only be implemented if required.

    const bool isMultiPane = modes & QtViewPane::OpenMode::MultiplePanes;

    if (!pane->IsVisible() || isMultiPane)
    {
        if (!pane->IsConstructed() || isMultiPane)
        {
            QWidget* w = pane->m_factoryFunc();
            w->setProperty("restored", (modes & QtViewPane::OpenMode::RestoreLayout) != 0); 
            DockWidget* dock = new DockWidget(w, pane, m_settings, m_mainWindow, m_advancedDockManager);
            pane->m_dockWidget = dock;
            pane->m_dockWidget->setVisible(true);

            // If this pane isn't dockable, set the allowed areas to none on the
            // dock widget so the fancy docking knows to prevent it from docking
            if (!pane->m_options.isDockable)
            {
                dock->setAllowedAreas(Qt::NoDockWidgetArea);
            }

            emit viewPaneCreated(pane);
        }
        else if (!pane->IsTabbed())
        {
            pane->m_dockWidget->setVisible(true);
        }

        if (modes & QtViewPane::OpenMode::UseDefaultState)
        {
            const bool forceToDefault = true;
            pane->m_dockWidget->RestoreState(forceToDefault);
        }
        else if (!pane->IsTabbed() && !(modes & QtViewPane::OpenMode::OnlyOpen))
        {
            pane->m_dockWidget->RestoreState();
        }
    }

    // If the dock widget is off screen (e.g. second monitor was disconnected),
    // restore its default state
    if (QApplication::desktop()->screenNumber(pane->m_dockWidget) == -1)
    {
        const bool forceToDefault = true;
        pane->m_dockWidget->RestoreState(forceToDefault);
    }

    if (pane->IsVisible())
    {
        if (!modes.testFlag(QtViewPane::OpenMode::RestoreLayout))
        {
            pane->m_dockWidget->setFocus();
        }
    }
    else
    {
        // If the dock widget is tabbed, then set it as the active tab
        AzQtComponents::DockTabWidget* tabWidget = pane->ParentTabWidget();
        if (tabWidget)
        {
            int index = tabWidget->indexOf(pane->m_dockWidget);
            tabWidget->setCurrentIndex(index);
        }
        // Otherwise just show the widget
        else
        {
            pane->m_dockWidget->show();
        }
    }

    // When a user opens a pane, if it is docked in a floating window, make sure
    // it isn't hidden behind other floating windows or the Editor main window
    if (modes.testFlag(QtViewPane::OpenMode::None))
    {
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(pane->m_dockWidget->parentWidget());
        if (!mainWindow)
        {
            // If the parent of our dock widgets isn't a QMainWindow, then it
            // might be tabbed, so try to find the tab container dock widget
            // and then get the QMainWindow from that.
            AzQtComponents::DockTabWidget* tabWidget = pane->ParentTabWidget();
            if (tabWidget)
            {
                QDockWidget* tabDockContainer = qobject_cast<QDockWidget*>(tabWidget->parentWidget());
                if (tabDockContainer)
                {
                    mainWindow = qobject_cast<QMainWindow*>(tabDockContainer->parentWidget());
                }
            }
        }

        if (mainWindow)
        {
            // If our pane is part of a floating window, then the parent of its
            // QMainWindow will be another dock widget container that is floating.
            // If this is the case, then raise it to the front so it won't be
            // hidden behind other floating windows (or the Editor main window)
            QDockWidget* parentDockWidget = qobject_cast<QDockWidget*>(mainWindow->parentWidget());
            if (parentDockWidget && parentDockWidget->isFloating())
            {
                parentDockWidget->raise();
            }
        }
    }

    return pane;
}

bool QtViewPaneManager::ClosePane(const QString& name, QtViewPane::CloseModes closeModes)
{
    if (QtViewPane* p = GetPane(name))
    {
        p->Close(closeModes | QtViewPane::CloseMode::Force);
        return true;
    }

    return false;
}

bool QtViewPaneManager::CloseAllPanes()
{
    for (QtViewPane& p : m_registeredPanes)
    {
        if (!p.Close())
        {
            return false; // Abort closing
        }
    }
    return true;
}

void QtViewPaneManager::CloseAllNonStandardPanes()
{
    for (QtViewPane& p : m_registeredPanes)
    {
        if (!p.m_options.isStandard)
        {
            p.Close(QtViewPane::CloseMode::Force);
        }
    }
}

void QtViewPaneManager::TogglePane(const QString& name)
{
    QtViewPane* pane = GetPane(name);
    if (!pane)
    {
        Q_ASSERT(false);
        return;
    }

    if (pane->IsVisible())
    {
        ClosePane(name);
    }
    else
    {
        OpenPane(name);
    }
}

QWidget* QtViewPaneManager::CreateWidget(const QString& paneName)
{
    QtViewPane* pane = GetPane(paneName);
    if (!pane)
    {
        qWarning() << Q_FUNC_INFO << "Couldn't find pane" << paneName << "; paneCount=" << m_registeredPanes.size();
        return nullptr;
    }

    QWidget* w = pane->m_factoryFunc();
    w->setWindowTitle(paneName);

    return w;
}

void QtViewPaneManager::SaveLayout()
{
    SaveLayout(s_lastLayoutName);
}

void QtViewPaneManager::RestoreLayout()
{
    bool restored = RestoreLayout(s_lastLayoutName);
    if (!restored)
    {
        // Nothing is saved in settings, restore default layout
        RestoreDefaultLayout();
    }
}

bool QtViewPaneManager::ClosePanesWithRollback(const QVector<QString>& panesToKeepOpen)
{
    QVector<QString> closedPanes;

    // try to close all panes that aren't remaining open after relayout
    bool rollback = false;
    for (QtViewPane& p : m_registeredPanes)
    {
        // Only close the panes that aren't remaining open and are currently
        // visible (which has to include a check if the pane is tabbed since
        // it could be hidden if its not the active tab)
        if (panesToKeepOpen.contains(p.m_name) || (!p.IsVisible() && !p.IsTabbed()))
        {
            continue;
        }

        // attempt to close this pane; if Close returns false, then the close event
        // was intercepted and the pane doesn't want to close, so we should cancel the whole thing
        // and rollback
        if (!p.Close())
        {
            rollback = true;
            break;
        }

        // keep track of the panes that we closed, so we can rollback later and reopen them
        closedPanes.push_back(p.m_name);
    }

    // check if we cancelled and need to roll everything back
    if (rollback)
    {
        for (const QString& paneName : closedPanes)
        {
            // append this to the end of the event loop with a zero length timer, so that
            // all of the close/hide events above are entirely processed
            QTimer::singleShot(0, this, [paneName, this]()
                {
                    OpenPane(paneName, QtViewPane::OpenMode::RestoreLayout);
                });
        }

        return false;
    }

    return true;
}

/**
 * Restore the default layout (also known as component entity layout)
 */
void QtViewPaneManager::RestoreDefaultLayout(bool resetSettings)
{
    if (resetSettings)
    {
        // We're going to do something destructive (removing all of the viewpane settings). Better confirm with the user
        auto buttonPressed = QMessageBox::warning(m_mainWindow, tr("Restore Default Layout"), tr("Are you sure you'd like to restore to the default layout? This will reset all of your view related settings."), QMessageBox::Cancel | QMessageBox::RestoreDefaults, QMessageBox::RestoreDefaults);
        if (buttonPressed != QMessageBox::RestoreDefaults)
        {
            return;
        }
    }

    // First, close all the open panes
    if (!ClosePanesWithRollback(QVector<QString>()))
    {
        return;
    }

    // Disable updates while we restore the layout to avoid temporary glitches
    // as the panes are moved around
    m_mainWindow->setUpdatesEnabled(false);

    // Reset all of the settings, or windows opened outside of RestoreDefaultLayout won't be reset at all.
    // Also ensure that this is done after CloseAllPanes, because settings will be saved in CloseAllPanes
    if (resetSettings)
    {
        ViewLayoutState state;

        state.viewPanes.push_back(LyViewPane::EntityOutliner);
        state.viewPanes.push_back(LyViewPane::EntityInspector);
        state.viewPanes.push_back(LyViewPane::AssetBrowser);

        state.viewPanes.push_back(LyViewPane::Console);
        state.viewPanes.push_back(LyViewPane::LegacyRollupBar);

        state.mainWindowState = m_defaultMainWindowState;

        {
            AzQtComponents::AutoSettingsGroup settingsGroupGuard(m_settings, m_useNewDocking ? GetFancyViewPaneStateGroupName() : GetViewPaneStateGroupName());
            m_settings->setValue(s_lastLayoutName, QVariant::fromValue<ViewLayoutState>(state));
        }

        m_settings->sync();

        // Let anything listening know to reset as well (*cough*CLayoutWnd*cough*)
        emit layoutReset();

        // Ensure that the main window knows it's new state
        // otherwise when we load view panes that haven't been loaded,
        // the main window will attempt to position them where they were last, not in their default spot
        m_mainWindow->restoreState(m_defaultMainWindowState);
    }

    // Reset the default view panes to be opened. Used for restoring default layout and component entity layout.
    const QtViewPane* entityOutlinerViewPane = OpenPane(LyViewPane::EntityOutliner, QtViewPane::OpenMode::UseDefaultState);
    const QtViewPane* assetBrowserViewPane = OpenPane(LyViewPane::AssetBrowser, QtViewPane::OpenMode::UseDefaultState);
    const QtViewPane* entityInspectorViewPane = OpenPane(LyViewPane::EntityInspector, QtViewPane::OpenMode::UseDefaultState);
    const QtViewPane* rollupBarViewPane = OpenPane(LyViewPane::LegacyRollupBar, QtViewPane::OpenMode::UseDefaultState);
    const QtViewPane* consoleViewPane = OpenPane(LyViewPane::Console, QtViewPane::OpenMode::UseDefaultState);

    // This class does all kinds of behind the scenes magic to make docking / restore work, especially with groups
    // so instead of doing our special default layout attach / docking right now, we want to make it happen
    // after all of the other events have been processed.
    QTimer::singleShot(0, [=]
    {
        // If we are using the new docking, set the right dock area to be absolute
        // so that the inspector/rollupbar tab widget will be to the right of the
        // viewport and console
        if (m_useNewDocking)
        {
            m_advancedDockManager->setAbsoluteCornersForDockArea(m_mainWindow, Qt::RightDockWidgetArea);
        }

        // Retrieve the width of the screen that our main window is on so we can
        // use it later for resizing our panes. The main window ends up being maximized
        // when we restore the default layout, but even if we maximize the main window
        // before doing anything else, its width won't update until after this has all
        // been processed, so we need to resize the panes based on what the main window
        // width WILL be after maximized
        int screenWidth = QApplication::desktop()->screenGeometry(m_mainWindow).width();

        if (assetBrowserViewPane && entityOutlinerViewPane)
        {
            m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, entityOutlinerViewPane->m_dockWidget);
            entityOutlinerViewPane->m_dockWidget->setFloating(false);

            m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, assetBrowserViewPane->m_dockWidget);
            assetBrowserViewPane->m_dockWidget->setFloating(false);

            if (m_useNewDocking)
            {
                m_advancedDockManager->splitDockWidget(m_mainWindow, entityOutlinerViewPane->m_dockWidget, assetBrowserViewPane->m_dockWidget, Qt::Vertical);

                // Resize our entity outliner (and by proxy the asset browser split with it)
                // so that they get an appropriate default width since the minimum sizes have
                // been removed from these widgets
                static const float entityOutlinerWidthPercentage = 0.15f;
                int newWidth = (float)screenWidth * entityOutlinerWidthPercentage;
                m_mainWindow->resizeDocks({ entityOutlinerViewPane->m_dockWidget }, { newWidth }, Qt::Horizontal);
            }
            else
            {
                m_mainWindow->splitDockWidget(entityOutlinerViewPane->m_dockWidget, assetBrowserViewPane->m_dockWidget, Qt::Vertical);
            }
        }

        m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, rollupBarViewPane->m_dockWidget);
        rollupBarViewPane->m_dockWidget->setFloating(false);

        if (entityInspectorViewPane)
        {
            // Only need to add the entity inspector with the old docking, since its about
            // to be tabbed anyway
            if (!m_useNewDocking)
            {
                m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, entityInspectorViewPane->m_dockWidget);
            }
            entityInspectorViewPane->m_dockWidget->setFloating(false);

            if (m_useNewDocking)
            {
                // Tab the entity inspector with the rollupbar so that when they are
                // tabbed they will be given the rollupbar's default width which
                // is more appropriate, and move the entity inspector to be the
                // first tab on the left and active
                AzQtComponents::DockTabWidget* tabWidget = m_advancedDockManager->tabifyDockWidget(rollupBarViewPane->m_dockWidget, entityInspectorViewPane->m_dockWidget, m_mainWindow);
                if (tabWidget)
                {
                    tabWidget->moveTab(1, 0);
                    tabWidget->setCurrentWidget(entityInspectorViewPane->m_dockWidget);

                    // Resize our tabbed entity inspector and rollup bar dock widget
                    // so that it takes up an appropriate amount of space (with the
                    // minimum sizes removed, it was being shrunk too small by default)
                    static const float tabWidgetWidthPercentage = 0.2f;
                    QDockWidget* tabWidgetParent = qobject_cast<QDockWidget*>(tabWidget->parentWidget());
                    int newWidth = (float)screenWidth * tabWidgetWidthPercentage;
                    m_mainWindow->resizeDocks({ tabWidgetParent }, { newWidth }, Qt::Horizontal);
                }
            }
            else
            {
                m_mainWindow->tabifyDockWidget(rollupBarViewPane->m_dockWidget, entityInspectorViewPane->m_dockWidget);
            }
        }

        m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, consoleViewPane->m_dockWidget);
        consoleViewPane->m_dockWidget->setFloating(false);

        // Re-enable updates now that we've finished restoring the layout
        m_mainWindow->setUpdatesEnabled(true);

        // Default layout should always be maximized
        // (use window() because the MainWindow may be wrapped in another window
        // like a WindowDecoratorWrapper or another QMainWindow for various layout reasons)
        m_mainWindow->window()->showMaximized();
    });
}

/**
 * Restore the legacy layout (rollupbar, console, and viewport)
 */
void QtViewPaneManager::RestoreLegacyLayout()
{
    // First, close all the open panes
    if (!ClosePanesWithRollback(QVector<QString>()))
    {
        return;
    }

    // Reset the default view panes to be opened so we can restore them to the legacy layout
    const QtViewPane* rollupBarViewPane = OpenPane(LyViewPane::LegacyRollupBar, QtViewPane::OpenMode::UseDefaultState);
    const QtViewPane* consoleViewPane = OpenPane(LyViewPane::Console, QtViewPane::OpenMode::UseDefaultState);

    // This class does all kinds of behind the scenes magic to make docking / restore work, especially with groups
    // so instead of doing our special default layout attach / docking right now, we want to make it happen
    // after all of the other events have been processed.
    QTimer::singleShot(0, [=]
    {
        // If we are using the new docking, set the right dock area to be absolute
        // so that the rollupbar will be to the right of the viewport and console
        if (m_useNewDocking)
        {
            m_advancedDockManager->setAbsoluteCornersForDockArea(m_mainWindow, Qt::RightDockWidgetArea);
        }

        m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, rollupBarViewPane->m_dockWidget);
        rollupBarViewPane->m_dockWidget->setFloating(false);

        m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, consoleViewPane->m_dockWidget);
        consoleViewPane->m_dockWidget->setFloating(false);
    });
}

void QtViewPaneManager::SaveLayout(QString layoutName)
{
    if (!m_mainWindow || m_restoreInProgress)
    {
        return;
    }

    layoutName = layoutName.trimmed();
    const bool isNew = !HasLayout(layoutName);

    ViewLayoutState state;
    foreach(const QtViewPane &pane, m_registeredPanes)
    {
        // Include all visible and tabbed panes in our layout, since tabbed panes
        // won't be visible if they aren't the active tab, but still need to be
        // retained in the layout
        if (pane.IsVisible() || pane.IsTabbed())
        {
            state.viewPanes.push_back(pane.m_dockWidget->PaneName());
        }
    }

    if (m_useNewDocking)
    {
        state.mainWindowState = m_advancedDockManager->saveState();
    }
    else
    {
        state.mainWindowState = m_mainWindow->saveState();
    }

    {
        AzQtComponents::AutoSettingsGroup settingsGroupGuard(m_settings, m_useNewDocking ? GetFancyViewPaneStateGroupName() : GetViewPaneStateGroupName());
        m_settings->setValue(layoutName, QVariant::fromValue<ViewLayoutState>(state));
    }

    m_settings->sync();

    if (isNew)
    {
        emit savedLayoutsChanged();
    }
}


void QtViewPaneManager::SerializeLayout(XmlNodeRef& parentNode) const
{
    ViewLayoutState state = GetLayout();

    XmlNodeRef paneListNode = XmlHelpers::CreateXmlNode("ViewPanes");
    parentNode->addChild(paneListNode);

    for (const QString& paneName : state.viewPanes)
    {
        XmlNodeRef paneNode = XmlHelpers::CreateXmlNode("ViewPane");
        paneNode->setContent(paneName.toLatin1().data());

        paneListNode->addChild(paneNode);
    }

    XmlNodeRef windowStateNode = XmlHelpers::CreateXmlNode("WindowState");
    windowStateNode->setContent(state.mainWindowState.toHex().data());

    parentNode->addChild(windowStateNode);
}

bool QtViewPaneManager::DeserializeLayout(const XmlNodeRef& parentNode)
{
    ViewLayoutState state;

    XmlNodeRef paneListNode = parentNode->findChild("ViewPanes");
    if (!paneListNode)
        return false;

    for (int i = 0; i < paneListNode->getChildCount(); ++i)
    {
        XmlNodeRef paneNode = paneListNode->getChild(i);
        state.viewPanes.push_back(QString(paneNode->getContent()));
    }

    XmlNodeRef windowStateNode = parentNode->findChild("WindowState");
    if (!windowStateNode)
        return false;

    state.mainWindowState = QByteArray::fromHex(windowStateNode->getContent());

    return RestoreLayout(state);
}

ViewLayoutState QtViewPaneManager::GetLayout() const
{
    ViewLayoutState state;

    foreach(const QtViewPane &pane, m_registeredPanes)
    {
        // Include all visible and tabbed panes in our layout, since tabbed panes
        // won't be visible if they aren't the active tab, but still need to be
        // retained in the layout
        if (pane.IsVisible() || pane.IsTabbed())
        {
            state.viewPanes.push_back(pane.m_dockWidget->PaneName());
        }
    }

    if (m_useNewDocking)
    {
        state.mainWindowState = m_advancedDockManager->saveState();
    }
    else
    {
        state.mainWindowState = m_mainWindow->saveState();
    }

    return state;
}


bool QtViewPaneManager::RestoreLayout(QString layoutName)
{
    if (m_restoreInProgress) // Against re-entrancy
    {
        return true;
    }

    QScopedValueRollback<bool> recursionGuard(m_restoreInProgress);
    m_restoreInProgress = true;

    layoutName = layoutName.trimmed();
    if (layoutName.isEmpty())
    {
        return false;
    }

    AzQtComponents::AutoSettingsGroup settingsGroupGuard(m_settings, m_useNewDocking ? GetFancyViewPaneStateGroupName() : GetViewPaneStateGroupName());

    if (!m_settings->contains(layoutName))
    {
        return false;
    }

    ViewLayoutState state = m_settings->value(layoutName).value<ViewLayoutState>();

    if (!ClosePanesWithRollback(state.viewPanes))
    {
        return false;
    }

    if (!m_useNewDocking)
    {
        bool restoreSuccess = m_mainWindow->restoreState(m_defaultMainWindowState);
        if (!restoreSuccess)
        {
            return false;
        }

        DockWidgetUtils::deleteWindowGroups(m_mainWindow);
    }

    for (const QString& paneName : state.viewPanes)
    {
        const QtViewPane* pane = OpenPane(paneName, QtViewPane::OpenMode::OnlyOpen);

        // Currently opened panes don't get closed when restoring a layout,
        // so if one of those panes is currently tabbed, it won't be restored
        // properly when using the new docking since it is parented to our
        // custom tab widget instead of the main editor window, so remove the
        // pane as a tab before proceeding with the restore
        if (m_useNewDocking && pane && pane->IsTabbed())
        {
            AzQtComponents::DockTabWidget* tabWidget = pane->ParentTabWidget();
            if (tabWidget)
            {
                tabWidget->removeTab(pane->m_dockWidget);
            }
        }
    }

    // must do this after opening all of the panes!
    if (m_useNewDocking)
    {
        m_advancedDockManager->restoreState(state.mainWindowState);
    }
    else
    {
        m_mainWindow->restoreState(state.mainWindowState);

        // Delete bogus empty QDockWidgetGroupWindow that appear
        DockWidgetUtils::deleteWindowGroups(m_mainWindow, /*onlyGhosts=*/true);
    }

    // In case of a crash it might happen that the QMainWindow state gets out of sync with the
    // QtViewPaneManager state, which would result in we opening dock widgets that QMainWindow
    // didn't know how to restore.
    // Check if that happened and return false indicating the restore failed and giving caller
    // a chance to restore the default layout.
    if (DockWidgetUtils::hasInvalidDockWidgets(m_mainWindow))
    {
        return false;
    }

    return true;
}

bool QtViewPaneManager::RestoreLayout(const ViewLayoutState& state)
{
    if (!ClosePanesWithRollback(state.viewPanes))
    {
        return false;
    }

    if (!m_useNewDocking)
    {
        //qDebug() << "Starting restore default";
        const bool restoreSuccess = m_mainWindow->restoreState(m_defaultMainWindowState);
        if (!restoreSuccess)
        {
            return false;
        }
        //qDebug() << "Restore default finished. success=" << restoreSuccess;
        DockWidgetUtils::deleteWindowGroups(m_mainWindow);
        //DockWidgetUtils::dumpDockWidgets(m_mainWindow);
    }

    for (const QString& paneName : state.viewPanes)
    {
        OpenPane(paneName, QtViewPane::OpenMode::OnlyOpen);
    }

    // must do this after opening all of the panes!
    if (m_useNewDocking)
    {
        m_advancedDockManager->restoreState(state.mainWindowState);
    }
    else
    {
        //QStringList dockwidgets;
        //DockWidgetUtils::processSavedState(state.mainWindowState, dockwidgets);
        m_mainWindow->restoreState(state.mainWindowState);

        // Delete bogus empty QDockWidgetGroupWindow that appear
        DockWidgetUtils::deleteWindowGroups(m_mainWindow, /*onlyGhosts=*/true);

        //DockWidgetUtils::dumpDockWidgets(m_mainWindow);
    }

    return true;
}


void QtViewPaneManager::RenameLayout(QString name, QString newName)
{
    name = name.trimmed();
    newName = newName.trimmed();
    if (name == newName || newName.isEmpty() || name.isEmpty())
    {
        return;
    }

    {
        AzQtComponents::AutoSettingsGroup settingsGroupGuard(m_settings, m_useNewDocking ? GetFancyViewPaneStateGroupName() : GetViewPaneStateGroupName());

        m_settings->setValue(newName, m_settings->value(name));
        m_settings->remove(name);
    }

    m_settings->sync();
    emit savedLayoutsChanged();
}

void QtViewPaneManager::RemoveLayout(QString layoutName)
{
    layoutName = layoutName.trimmed();
    if (layoutName.isEmpty())
    {
        return;
    }

    {
        AzQtComponents::AutoSettingsGroup settingsGroupGuard(m_settings, m_useNewDocking ? GetFancyViewPaneStateGroupName() : GetViewPaneStateGroupName());
        m_settings->remove(layoutName.trimmed());
    }

    m_settings->sync();
    emit savedLayoutsChanged();
}

bool QtViewPaneManager::HasLayout(const QString& name) const
{
    return LayoutNames().contains(name.trimmed(), Qt::CaseInsensitive);
}

QStringList QtViewPaneManager::LayoutNames(bool userLayoutsOnly) const
{
    QStringList layouts;

    AzQtComponents::AutoSettingsGroup settingsGroupGuard(m_settings, m_useNewDocking ? GetFancyViewPaneStateGroupName() : GetViewPaneStateGroupName());
    layouts = m_settings->childKeys();

    if (userLayoutsOnly)
    {
        layouts.removeOne(s_lastLayoutName); // "last" is internal
    }
    return layouts;
}

QtViewPanes QtViewPaneManager::GetRegisteredPanes(bool viewPaneMenuOnly) const
{
    if (!viewPaneMenuOnly)
    {
        return m_registeredPanes;
    }

    QtViewPanes panes;
    panes.reserve(30); // approximate
    std::copy_if(m_registeredPanes.cbegin(), m_registeredPanes.cend(), std::back_inserter(panes), [](QtViewPane pane)
        {
            return pane.m_options.showInMenu;
        });

    return panes;
}

QtViewPanes QtViewPaneManager::GetRegisteredMultiInstancePanes(bool viewPaneMenuOnly) const
{
    QtViewPanes panes;
    panes.reserve(30); // approximate

    if (viewPaneMenuOnly)
    {
        std::copy_if(m_registeredPanes.cbegin(), m_registeredPanes.cend(), std::back_inserter(panes), [](QtViewPane pane)
            {
                return pane.m_options.showInMenu && pane.m_options.canHaveMultipleInstances;
            });
    }
    else
    {
        std::copy_if(m_registeredPanes.cbegin(), m_registeredPanes.cend(), std::back_inserter(panes), [](QtViewPane pane)
            {
                return pane.m_options.canHaveMultipleInstances;
            });
    }

    return panes;
}

QtViewPanes QtViewPaneManager::GetRegisteredViewportPanes() const
{
    QtViewPanes viewportPanes;
    viewportPanes.reserve(5); // approximate
    std::copy_if(m_registeredPanes.cbegin(), m_registeredPanes.cend(), std::back_inserter(viewportPanes), [](QtViewPane pane)
        {
            return pane.IsViewportPane();
        });

    return viewportPanes;
}

int QtViewPaneManager::NextAvailableId()
{
    for (int candidate = ID_VIEW_OPENPANE_FIRST; candidate <= ID_VIEW_OPENPANE_LAST; ++candidate)
    {
        if (!m_knownIdsSet.contains(candidate))
        {
            m_knownIdsSet.push_back(candidate);
            return candidate;
        }
    }

    return -1;
}

QtViewPane* QtViewPaneManager::GetPane(int id)
{
    auto it = std::find_if(m_registeredPanes.begin(), m_registeredPanes.end(),
            [id](const QtViewPane& pane) { return id == pane.m_id; });

    return it == m_registeredPanes.end() ? nullptr : it;
}

QtViewPane* QtViewPaneManager::GetPane(const QString& name)
{
    auto it = std::find_if(m_registeredPanes.begin(), m_registeredPanes.end(),
            [name](const QtViewPane& pane) { return name == pane.m_name; });

    return it == m_registeredPanes.end() ? nullptr : it;
}

QtViewPane* QtViewPaneManager::GetViewportPane(int viewportType)
{
    auto it = std::find_if(m_registeredPanes.begin(), m_registeredPanes.end(),
            [viewportType](const QtViewPane& pane) { return viewportType == pane.m_options.viewportType; });

    return it == m_registeredPanes.end() ? nullptr : it;
}

QDockWidget* QtViewPaneManager::GetView(const QString& name)
{
    QtViewPane* pane = GetPane(name);
    return pane ? pane->m_dockWidget : nullptr;
}

bool QtViewPaneManager::IsVisible(const QString& name)
{
    QtViewPane* view = GetPane(name);
    return view && view->IsVisible();
}

#include <QtViewPaneManager.moc>
