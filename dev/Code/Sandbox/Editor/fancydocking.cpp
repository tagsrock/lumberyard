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
#include "fancydocking.h"
#include <AzQtComponents/Components/DockBar.h>
#include <AzQtComponents/Components/DockMainWindow.h>
#include <AzQtComponents/Components/EditorProxyStyle.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzQtComponents/Components/Titlebar.h>
#include <AzQtComponents/Utilities/QtWindowUtilities.h>

#include <QAbstractButton>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QEvent>
#include <QTimer>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScopedValueRollback>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <AzQtComponents/Components/FancyDockingGhostWidget.h>
#include <AzQtComponents/Components/FancyDockingDropZoneWidget.h>

static AzQtComponents::FancyDockingDropZoneConstants g_FancyDockingConstants;

// Constants for our floating window and tab container object name prefixes
static const char* g_floatingWindowPrefix = "_fancydocking_";
static const char* g_tabContainerPrefix = "_fancydockingtabcontainer_";

static Qt::DockWidgetArea opposite(Qt::DockWidgetArea area)
{
    switch (area)
    {
    default:
    case Qt::BottomDockWidgetArea:
        return Qt::TopDockWidgetArea;
    case Qt::TopDockWidgetArea:
        return Qt::BottomDockWidgetArea;
    case Qt::LeftDockWidgetArea:
        return Qt::RightDockWidgetArea;
    case Qt::RightDockWidgetArea:
        return Qt::LeftDockWidgetArea;
    }
}

static Qt::Orientation orientation(Qt::DockWidgetArea area)
{
    switch (area)
    {
    default:
    case Qt::BottomDockWidgetArea:
    case Qt::TopDockWidgetArea:
        return Qt::Vertical;
    case Qt::LeftDockWidgetArea:
    case Qt::RightDockWidgetArea:
        return Qt::Horizontal;
    }
}

#ifdef KDAB_MAC_PORT

/**
 * Stream operator for writing out the TabContainerType to a data stream
 */
static QDataStream& operator<<(QDataStream& out, const FancyDocking::TabContainerType& myObj)
{
    out << myObj.floatingDockName << myObj.tabNames << myObj.currentIndex;
    return out;
}

/**
 * Stream operator for reading in a TabContainerType from a data stream
 */
static QDataStream& operator>>(QDataStream& in, FancyDocking::TabContainerType& myObj)
{
    in >> myObj.floatingDockName;
    in >> myObj.tabNames;
    in >> myObj.currentIndex;
    return in;
}
#endif



/**
 * Create our fancy docking widget
 */
FancyDocking::FancyDocking(QMainWindow* mainWindow)
    : QWidget(mainWindow, Qt::ToolTip | Qt::BypassWindowManagerHint | Qt::FramelessWindowHint)
    , m_mainWindow(mainWindow)
    , m_desktopWidget(QApplication::desktop())
    , m_emptyWidget(new QWidget(this))
    , m_dropZoneHoverFadeInTimer(new QTimer(this))
    , m_ghostWidget(new AzQtComponents::FancyDockingGhostWidget(mainWindow))
    , m_dropZoneWidgets()
{
    m_dropZoneState.dropZoneColorOnHover = AzQtComponents::EditorProxyStyle::dropZoneColorOnHover();

    // Register our TabContainerType stream operators so that they will be used
    // when reading/writing from/to data streams
#ifdef KDAB_MAC_PORT
    qRegisterMetaTypeStreamOperators<FancyDocking::TabContainerType>("FancyDocking::TabContainerType");
#endif
    mainWindow->installEventFilter(this);
    mainWindow->setProperty("fancydocking_owner", QVariant::fromValue(this));
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    // Make sure our placeholder empty widget is hidden by default
    m_emptyWidget->hide();

    // Update our docking overlay geometry, and listen for any changes to the
    // desktop screens being resized or added/removed so we can recalculate
    // our docking overlay
    updateDockingGeometry();
    QObject::connect(m_desktopWidget, &QDesktopWidget::resized, this, &FancyDocking::updateDockingGeometry);
    QObject::connect(m_desktopWidget, &QDesktopWidget::screenCountChanged, this, &FancyDocking::updateDockingGeometry);

    // Timer for updating our hovered drop zone opacity
    QObject::connect(m_dropZoneHoverFadeInTimer, &QTimer::timeout, this, &FancyDocking::onDropZoneHoverFadeInUpdate);
    m_dropZoneHoverFadeInTimer->setInterval(g_FancyDockingConstants.dropZoneHoverFadeUpdateIntervalMS);
}

FancyDocking::~FancyDocking()
{
    for (auto i = m_dropZoneWidgets.begin(); i != m_dropZoneWidgets.end(); i++)
    {
        delete i.value();
    }
}

/**
 * Create a new QDockWidget whose main widget will be a DockMainWindow. It will be created floating
 * with the given geometry. The QDockWidget will be named with the given name
 */
QMainWindow* FancyDocking::createFloatingMainWindow(const QString& name, const QRect& geometry)
{
    auto dockWidget = new AzQtComponents::StyledDockWidget(m_mainWindow);
    dockWidget->setObjectName(name);
    if (!restoreDockWidget(dockWidget))
    {
        m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
    }
    dockWidget->setFloating(true);
    if (!geometry.isNull())
    {
        dockWidget->setGeometry(geometry);
    }

    // Make sure the floating dock container is deleted when closed so that
    // its children can be restored properly when re-opened (otherwise they
    // will try to show up on a floating dock widget that is invisible)
    dockWidget->setAttribute(Qt::WA_DeleteOnClose);

    // Stack this floating dock widget name on the top of our z-ordered list
    // since it was just created
    m_orderedFloatingDockWidgetNames.prepend(name);

    // Hide the title bar when the group is docked
    //commented out because our styled dockwidget takes care of this
    //connect(dockWidget, &QDockWidget::topLevelChanged, [dockWidget](bool fl)
    //    { if (!fl) dockWidget->setTitleBarWidget(new QWidget()); });
    AzQtComponents::DockMainWindow* mainWindow = new AzQtComponents::DockMainWindow(dockWidget);
    mainWindow->setProperty("fancydocking_owner", QVariant::fromValue(this));
    mainWindow->setWindowFlags(Qt::Widget);
    mainWindow->installEventFilter(this);
    dockWidget->setWidget(mainWindow);
    dockWidget->show();
    return mainWindow;
}

/**
 * Create a new tab widget and a dock widget container to hold it
 */
AzQtComponents::DockTabWidget* FancyDocking::createTabWidget(QMainWindow* mainWindow, QDockWidget* widgetToReplace, QString name)
{
    // If a name wasn't provided, then generate a random one
    if (name.isEmpty())
    {
        name = getUniqueDockWidgetName(g_tabContainerPrefix);
    }

    // Create a container dock widget for our tab widget
    AzQtComponents::StyledDockWidget* tabWidgetContainer = new AzQtComponents::StyledDockWidget(mainWindow);
    tabWidgetContainer->setObjectName(name);
    tabWidgetContainer->setFloating(false);

    // Set an empty QWidget as the custom title bar to hide it, since our tab widget will drive it's own custom tab bar
    // that will replace it (the empty QWidget is parented to the dock widget, so it will be cleaned up whenever the dock widget is deleted)
    tabWidgetContainer->setTitleBarWidget(new QWidget());

    // Create our new tab widget and listen for tab pressed, inserted, count changed, and undock events
    AzQtComponents::DockTabWidget* tabWidget = new AzQtComponents::DockTabWidget(m_mainWindow, mainWindow);
    QObject::connect(tabWidget, &AzQtComponents::DockTabWidget::tabIndexPressed, this, &FancyDocking::onTabIndexPressed);
    QObject::connect(tabWidget, &AzQtComponents::DockTabWidget::tabWidgetInserted, this, &FancyDocking::onTabWidgetInserted);
    QObject::connect(tabWidget, &AzQtComponents::DockTabWidget::tabCountChanged, this, &FancyDocking::onTabCountChanged);
    QObject::connect(tabWidget, &AzQtComponents::DockTabWidget::undockTab, this, &FancyDocking::onUndockTab);

    // Set our tab widget as the widget for our tab container docking widget
    tabWidgetContainer->setWidget(tabWidget);

    // There isn't a way to replace a dock widget in a layout, so we have to place our tab container dock widget
    // split next to our replaced widget, and then remove our replaced widget from the layout.  The replaced widget
    // will then be moved to our tab widget, so it effectively will remain in the same spot, but now it will be tabbed
    // instead of a standalone dock widget.
    if (widgetToReplace)
    {
        splitDockWidget(mainWindow, widgetToReplace, tabWidgetContainer, Qt::Horizontal);
        mainWindow->removeDockWidget(widgetToReplace);
        tabWidget->addTab(widgetToReplace);
    }

    return tabWidget;
}

/**
 * Return a unique object name with the specified prefix that doesn't collide with any QDockWidget children of our main window
 */
QString FancyDocking::getUniqueDockWidgetName(const char* prefix)
{
    QString name;
    do
    {
        name = QLatin1String(prefix) + QString::number(qrand(), 16);
    } while (m_mainWindow->findChild<QDockWidget*>(name));

    return name;
}

/**
 * Update the geometry of our docking overlay to be a union of all the screen
 * rects for each desktop monitor
 */
void FancyDocking::updateDockingGeometry()
{
    QRect totalScreenRect;
    int numScreens = m_desktopWidget->screenCount();
    for (int i = 0; i < numScreens; ++i)
    {
        totalScreenRect = totalScreenRect.united(m_desktopWidget->screenGeometry(i));
    }

    setGeometry(totalScreenRect);

    // Update our list of screens whenever screens are added/removed so that we
    // don't have to query them every time
    m_desktopScreens = qApp->screens();
}

/**
 * Called on a timer interval to update the hovered drop zone opacity to make it
 * fade in with a set delay
 */
void FancyDocking::onDropZoneHoverFadeInUpdate()
{
    m_dropZoneState.dropZoneHoverOpacity += g_FancyDockingConstants.dropZoneHoverFadeIncrement;

    // Once we've reached the full drop zone opacity, cut it off in case we
    // went over and stop the timer
    if (m_dropZoneState.dropZoneHoverOpacity >= g_FancyDockingConstants.dropZoneOpacity)
    {
        m_dropZoneState.dropZoneHoverOpacity = g_FancyDockingConstants.dropZoneOpacity;
        m_dropZoneHoverFadeInTimer->stop();
    }

    // Trigger a re-paint so the opacity will update
    RepaintFloatingIndicators();
}

/**
 * Return the number of visible dock widget children for the specified main window
 */
int FancyDocking::NumVisibleDockWidgets(QMainWindow* mainWindow)
{
    if (!mainWindow)
    {
        return -1;
    }

    // Count the number of visible dock widgets for our main window
    const QList<QDockWidget*> list = mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    int count = std::count_if(list.cbegin(), list.cend(), [](QDockWidget* dockWidget) {
        return dockWidget->isVisible();
    });

    return count;
}

/**
 * Destroy a floating main window if it no longer contains any QDockWidgets
 */
void FancyDocking::destroyIfUseless(QMainWindow* mainWindow)
{
    // Ignore if this was triggered on our main window, or if this is triggered
    // during a tabify action, during which the dock widgets may be hidden
    // so it ends up deleting the floating main window
    if (!mainWindow || mainWindow == m_mainWindow || m_state.tabifyInProgress)
    {
        return;
    }

    // Remove the container main window if there are no more visible QDockWidgets
    int count = NumVisibleDockWidgets(mainWindow);
    if (count > 0)
    {
        return;
    }

    // Avoid a recursion
    mainWindow->removeEventFilter(this);

    // Save the state of this floating dock widget that's about to be destroyed
    // so that we can re-create it if necessary when restoring any panes whose
    // last location was in this floating dock widget
    QDockWidget* floatingDockWidget = static_cast<QDockWidget*>(mainWindow->parentWidget());
    QString floatingDockWidgetName = floatingDockWidget->objectName();
    if (!floatingDockWidgetName.isEmpty())
    {
        m_restoreFloatings[floatingDockWidgetName] = qMakePair(mainWindow->saveState(), floatingDockWidget->geometry());
    }

    // Any dock widgets left in our floating main window were hidden, so
    // reparent them to the editor main window and make sure they remain
    // hidden.  This is so they will be restored properly the next time
    // someone tries to open them, because otherwise, it would try to
    // open them on the floating main window that no longer exists.
    for (QDockWidget* dockWidget : mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
    {
        dockWidget->setParent(m_mainWindow);
        dockWidget->setVisible(false);
    }

    // Remove this floating dock widget from our z-ordered list of dock widget names
    m_orderedFloatingDockWidgetNames.removeAll(floatingDockWidgetName);

    // Lastly, delete our empty floating dock widget container, which will
    // also delete the floating main window since it is a child.
    floatingDockWidget->deleteLater();
}

/**
 * Return an absolute drop zone (if applicable) for the given drop target
 */
QRect FancyDocking::getAbsoluteDropZone(QWidget* dock, Qt::DockWidgetArea& area, const QPoint& globalPos)
{
    QRect absoluteDropZoneRect;
    if (!dock)
    {
        return absoluteDropZoneRect;
    }

    // Check if we are trying to drop onto a main window, and if not, get the
    // main window from the drop target parent
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dock);
    bool dropTargetIsMainWindow = true;
    if (!mainWindow)
    {
        dropTargetIsMainWindow = false;
        mainWindow = qobject_cast<QMainWindow*>(dock->parentWidget());
    }

    // If we still couldn't find a valid main window, then bail out
    if (!mainWindow)
    {
        return absoluteDropZoneRect;
    }

    // Don't allow the dragged dock widget to be docked as absolute
    // if it's already in the target main window and there is only
    // one other widget alongside it
    if (mainWindow != m_mainWindow)
    {
        const auto childDockWidgets = mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        if (childDockWidgets.size() <= 2 && childDockWidgets.contains(m_state.dock))
        {
            return absoluteDropZoneRect;
        }
    }

    // Setup the possible absolute drop zones for the given main window
    QRect mainWindowRect(mainWindow->rect());
    QPoint mainWindowTopLeft = mapFromGlobal(mainWindow->mapToGlobal(mainWindowRect.topLeft()));
    QPoint mainWindowTopRight = mapFromGlobal(mainWindow->mapToGlobal(mainWindowRect.topRight()));
    QPoint mainWindowBottomLeft = mapFromGlobal(mainWindow->mapToGlobal(mainWindowRect.bottomLeft()));
    QSize absoluteLeftRightSize(g_FancyDockingConstants.absoluteDropZoneSizeInPixels, mainWindowRect.height());
    QRect absoluteLeftDropZone(mainWindowTopLeft, absoluteLeftRightSize);
    QRect absoluteRightDropZone(mainWindowTopRight - QPoint(g_FancyDockingConstants.absoluteDropZoneSizeInPixels, 0), absoluteLeftRightSize);
    QSize absoluteTopBottomSize(mainWindowRect.width(), g_FancyDockingConstants.absoluteDropZoneSizeInPixels);
    QRect absoluteTopDropZone(mainWindowTopLeft, absoluteTopBottomSize);
    QRect absoluteBottomDropZone(mainWindowBottomLeft - QPoint(0, g_FancyDockingConstants.absoluteDropZoneSizeInPixels), absoluteTopBottomSize);

    // If the drop target is a main window, then we will only show the absolute
    // drop zone if the cursor is in that zone already
    if (dropTargetIsMainWindow)
    {
        QPoint localPos = mapFromGlobal(globalPos);

        if (absoluteLeftDropZone.contains(localPos))
        {
            absoluteDropZoneRect = absoluteLeftDropZone;
            area = Qt::LeftDockWidgetArea;
        }
        else if (absoluteRightDropZone.contains(localPos))
        {
            absoluteDropZoneRect = absoluteRightDropZone;
            area = Qt::RightDockWidgetArea;
        }
        else if (absoluteTopDropZone.contains(localPos))
        {
            absoluteDropZoneRect = absoluteTopDropZone;
            area = Qt::TopDockWidgetArea;
        }
        else if (absoluteBottomDropZone.contains(localPos))
        {
            absoluteDropZoneRect = absoluteBottomDropZone;
            area = Qt::BottomDockWidgetArea;
        }
    }
    // Otherwise if the drop target is just a normal dock widget, then we will
    // show the absolute drop zone once a normal drop zone sharing that edge
    // is activated
    else
    {
        const QRect& dockRect = dock->rect();
        QPoint dockTopLeft = mapFromGlobal(dock->mapToGlobal(dockRect.topLeft()));
        QPoint dockBottomRight = mapFromGlobal(dock->mapToGlobal(dockRect.bottomRight()));
        area = m_dropZoneState.dropArea;

        // If the hovered over drop zone shares a side with an absolute edge, then we need to setup
        // an absolute drop zone for that area (if absolute drop zones are allowed for this target)
        switch (m_dropZoneState.dropArea)
        {
        case Qt::LeftDockWidgetArea:
            if (dockTopLeft.x() == mainWindowTopLeft.x())
            {
                absoluteDropZoneRect = absoluteLeftDropZone;
            }
            break;
        case Qt::RightDockWidgetArea:
            if (dockBottomRight.x() == mainWindowTopRight.x())
            {
                absoluteDropZoneRect = absoluteRightDropZone;
            }
            break;
        case Qt::TopDockWidgetArea:
            if (dockTopLeft.y() == mainWindowTopLeft.y())
            {
                absoluteDropZoneRect = absoluteTopDropZone;
            }
            break;
        case Qt::BottomDockWidgetArea:
            if (dockBottomRight.y() == mainWindowBottomLeft.y())
            {
                absoluteDropZoneRect = absoluteBottomDropZone;
            }
            break;
        }
    }

    return absoluteDropZoneRect;
}

/**
 * Set m_dropZoneState.dropOnto and the m_dropZoneState.dropZones as to drop within the specified dock.
 */
void FancyDocking::setupDropZones(QWidget* dock, const QPoint& globalPos)
{
    // If there is no dock widget, then reset our drop zones and return
    if (!dock)
    {
        m_dropZoneState.dropOnto = dock;
        m_dropZoneState.dropZones.clear();
        m_dropZoneState.dockDropZoneRect = QRect();
        m_dropZoneState.innerDropZoneRect = QRect();
        m_dropZoneState.absoluteDropZoneArea = Qt::NoDockWidgetArea;
        m_dropZoneState.absoluteDropZoneRect = QRect();
        return;
    }

    // If the drop widget is a QMainWindow, then we won't show the normal drop zones
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dock);
    bool normalDropZonesAllowed = !mainWindow;

    // Figure out if we need to recalculate the drop zones
    QRect dockRect = dock->rect();
    if (m_dropZoneState.dropOnto == dock)
    {
        if (mainWindow)
        {
            // If the drop target is a main window, this means the mouse is
            // hovered over a dead zone margin, the central widget (viewport),
            // or the widget that is being dragged, so we will need to setup
            // an absolute drop zone based on the mouse position
            if (m_dropZoneState.onAbsoluteDropZone)
            {
                // If we're already hovered on the applicable absolute
                // drop zone, then we don't need to re-calculate
                return;
            }
            else
            {
                Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
                m_dropZoneState.absoluteDropZoneRect = getAbsoluteDropZone(dock, area, globalPos);
                m_dropZoneState.absoluteDropZoneArea = area;
            }
        }
        else if (m_dropZoneState.dropArea == Qt::NoDockWidgetArea || m_dropZoneState.dropArea == Qt::AllDockWidgetAreas)
        {
            // If we're hovered over the dead zone or the center tab, then reset the absolute drop
            // zone if there is one so we can recalculate the drop zones
            if (m_dropZoneState.absoluteDropZoneRect.isValid())
            {
                m_dropZoneState.absoluteDropZoneArea = Qt::NoDockWidgetArea;
                m_dropZoneState.absoluteDropZoneRect = QRect();
            }
            // Otherwise the drop zones don't need to be updated, so return
            else
            {
                return;
            }
        }
        else
        {
            // If we're still hovered over the same area, no need to re-calculate the absolute drop zones
            if (m_dropZoneState.absoluteDropZoneArea == m_dropZoneState.dropArea)
            {
                return;
            }

            // Find the main window for the dock widget we are hovered over
            QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dock->parentWidget());
            if (!mainWindow)
            {
                mainWindow = m_mainWindow;
            }

            // Try to setup an absolute drop zone based on the dock widget
            Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
            QRect absoluteDropZoneRect = getAbsoluteDropZone(dock, area);

            // If we setup an absolute drop zone, then cache it
            if (absoluteDropZoneRect.isValid())
            {
                m_dropZoneState.absoluteDropZoneRect = absoluteDropZoneRect;
                m_dropZoneState.absoluteDropZoneArea = area;
            }
            // If the current area doesn't need an absolute drop zone, and we didn't have an absolute drop zone previously,
            // then we don't need to make any changes so return
            else if (!m_dropZoneState.absoluteDropZoneRect.isValid())
            {
                return;
            }
            // Otherwise clear out our cached absolute drop zone so we can reset everything
            else
            {
                m_dropZoneState.absoluteDropZoneArea = Qt::NoDockWidgetArea;
                m_dropZoneState.absoluteDropZoneRect = QRect();
            }
        }
    }
    // We switched drop widgets; clear out the absolute drop zone data
    else
    {
        m_dropZoneState.absoluteDropZoneArea = Qt::NoDockWidgetArea;
        m_dropZoneState.absoluteDropZoneRect = QRect();
    }

    // We need to recalculate the drop zones, so clear them and proceed
    m_dropZoneState.dropOnto = dock;
    m_dropZoneState.dropZones.clear();
    m_dropZoneState.innerDropZoneRect = QRect();
    StartDropZone(m_dropZoneState.dropOnto, globalPos);

    // Don't setup the normal drop zones if our drop target is a QMainWindow
    if (!normalDropZonesAllowed)
    {
        raiseDockWidgets();
        return;
    }

    // If there is a valid absolute drop zone, adjust our outer dock widget rectangle accordingly to make room for it
    switch (m_dropZoneState.absoluteDropZoneArea)
    {
    case Qt::LeftDockWidgetArea:
        dockRect.setX(dockRect.x() + g_FancyDockingConstants.absoluteDropZoneSizeInPixels);
        break;
    case Qt::RightDockWidgetArea:
        dockRect.setWidth(dockRect.width() - g_FancyDockingConstants.absoluteDropZoneSizeInPixels);
        break;
    case Qt::TopDockWidgetArea:
        dockRect.setY(dockRect.y() + g_FancyDockingConstants.absoluteDropZoneSizeInPixels);
        break;
    case Qt::BottomDockWidgetArea:
        dockRect.setHeight(dockRect.height() - g_FancyDockingConstants.absoluteDropZoneSizeInPixels);
        break;
    }

    // Store our potentially adjusted outer dock widget rectangle and retrieve its corner points for later calculations
    m_dropZoneState.dockDropZoneRect = dockRect;
    const QPoint topLeft = mapFromGlobal(dock->mapToGlobal(dockRect.topLeft()));
    const QPoint topRight = mapFromGlobal(dock->mapToGlobal(dockRect.topRight()));
    const QPoint bottomLeft = mapFromGlobal(dock->mapToGlobal(dockRect.bottomLeft()));
    const QPoint bottomRight = mapFromGlobal(dock->mapToGlobal(dockRect.bottomRight()));

    /*
        The normal drop zones for left/right/top/bottom of a dock widget are trapezoids with the longer
        side on the edges of the widget, and the shorter side towards the middle of the widget. Here
        is a rough depiction:
         _______________________
        |\                     /|
        | \                   / |
        |  \_________________/  |
        |   |               |   |
        |   |               |   |
        |   |               |   |
        |   |_______________|   |
        |  /                 \  |
        | /                   \ |
        |/_____________________\|
        The drop zones are constructed using polygons with the appropriate points from the dock widget
        and the calculated inner points.
    */
    int dockWidth = dockRect.width();
    int dockHeight = dockRect.height();
    int topLeftX = topLeft.x();
    int topLeftY = topLeft.y();
    int topRightX = topRight.x();
    int bottomLeftY = bottomLeft.y();
    
    // Set the drop zone width/height to the default, but if the dock widget
    // width and/or height is below the threshold, then switch to scaling them
    // down accordingly
    int dropZoneWidth = g_FancyDockingConstants.dropZoneSizeInPixels;
    if (dockWidth < g_FancyDockingConstants.minDockSizeBeforeDropZoneScalingInPixels)
    {
        dropZoneWidth = dockWidth * g_FancyDockingConstants.dropZoneScaleFactor;
    }
    int dropZoneHeight = g_FancyDockingConstants.dropZoneSizeInPixels;
    if (dockHeight < g_FancyDockingConstants.minDockSizeBeforeDropZoneScalingInPixels)
    {
        dropZoneHeight = dockHeight * g_FancyDockingConstants.dropZoneScaleFactor;
    }

    // Calculate the inner corners to be used when constructing the drop zone polygons
    QPoint innerTopLeft(topLeftX + dropZoneWidth, topLeftY + dropZoneHeight);
    QPoint innerTopRight(topRightX - dropZoneWidth, topLeftY + dropZoneHeight);
    QPoint innerBottomLeft(topLeftX + dropZoneWidth, bottomLeftY - dropZoneHeight);
    QPoint innerBottomRight(topRightX - dropZoneWidth, bottomLeftY - dropZoneHeight);
    m_dropZoneState.innerDropZoneRect = QRect(innerTopLeft, innerBottomRight);

    // Setup the left/right/top/bottom drop zones using our calculated points
    QPolygon leftDropZone, rightDropZone, topDropZone, bottomDropZone;
    leftDropZone << topLeft << innerTopLeft << innerBottomLeft << bottomLeft;
    rightDropZone << topRight << bottomRight << innerBottomRight << innerTopRight;
    topDropZone << topLeft << topRight << innerTopRight << innerTopLeft;
    bottomDropZone << bottomLeft << innerBottomLeft << innerBottomRight << bottomRight;
    m_dropZoneState.dropZones[Qt::LeftDockWidgetArea] = leftDropZone;
    m_dropZoneState.dropZones[Qt::RightDockWidgetArea] = rightDropZone;
    m_dropZoneState.dropZones[Qt::TopDockWidgetArea] = topDropZone;
    m_dropZoneState.dropZones[Qt::BottomDockWidgetArea] = bottomDropZone;

    // Add the center drop zone for docking as a tab. The drop zone will be
    // stored as a polygon, although it will actually be drawn/evaluated
    // as a circle. The center drop zone size will be whichever is smaller
    // between the inner drop zone width vs height, and scaled accordingly
    int innerDropZoneWidth = m_dropZoneState.innerDropZoneRect.width();
    int innerDropZoneHeight = m_dropZoneState.innerDropZoneRect.height();
    int centerDropZoneDiameter = (innerDropZoneWidth < innerDropZoneHeight) ? innerDropZoneWidth : innerDropZoneHeight;
    centerDropZoneDiameter *= g_FancyDockingConstants.centerTabDropZoneScale;

    // Setup our center tab drop zone
    const QSize centerDropZoneSize(centerDropZoneDiameter, centerDropZoneDiameter);
    const QRect centerDropZoneRect(m_dropZoneState.innerDropZoneRect.center() - QPoint(centerDropZoneDiameter, centerDropZoneDiameter) / 2, centerDropZoneSize);
    m_dropZoneState.dropZones[Qt::AllDockWidgetAreas] = QPolygon(centerDropZoneRect, true);     // AllDockWidgetAreas means we want tab

    // Make sure the drop zones don't overlap with floating dock windows in the foreground
    raiseDockWidgets();
}

/**
 * Raise the appropriate dock widgets given the current widget to be dropped on
 * so that the drop zones don't overlap with floating dock windows in the foreground
 */
void FancyDocking::raiseDockWidgets()
{
    if (!m_dropZoneState.dropOnto)
    {
        return;
    }

    // If our drop target isn't a main window, then retrieve the main window
    // from the dock widget parent
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(m_dropZoneState.dropOnto);
    if (!mainWindow)
    {
        mainWindow = qobject_cast<QMainWindow*>(m_dropZoneState.dropOnto->parentWidget());
    }

    if (mainWindow && mainWindow != m_mainWindow)
    {
        // If our dock widget is part of a floating main window, then we need
        // to retrieve its container dock widget to raise that to the
        // foreground and then raise our docking overlay on top
        QDockWidget* containerDockWidget = qobject_cast<QDockWidget*>(mainWindow->parentWidget());
        if (containerDockWidget)
        {
            containerDockWidget->raise();
        }
    }

    if (m_activeDropZoneWidgets.size())
    {
        // the floating dropzone indicators clip against everything above them
        // so they should always be on top of everything else
        for (AzQtComponents::FancyDockingDropZoneWidget* dropZoneWidget : m_activeDropZoneWidgets)
        {
            dropZoneWidget->raise();
        }
    }

    // floating pixmap is always on top; it'll clip what it's supposed to
    m_ghostWidget->raise();
}

/*!
  Return on which dockArea should we drop something depending on the global position of the cursor
  */
Qt::DockWidgetArea FancyDocking::dockAreaForPos(const QPoint& globalPos)
{
    m_dropZoneState.onAbsoluteDropZone = false;
    if (!m_dropZoneState.dropOnto)
    {
        return Qt::NoDockWidgetArea;
    }
    const QPoint& pos = mapFromGlobal(globalPos);

    // First, check if we are hovered over an absolute drop zone
    if (m_dropZoneState.absoluteDropZoneRect.isValid() && m_dropZoneState.absoluteDropZoneRect.contains(pos))
    {
        m_dropZoneState.onAbsoluteDropZone = true;
        return m_dropZoneState.absoluteDropZoneArea;
    }

    // Then, check all of the default drop zones
    for (auto it = m_dropZoneState.dropZones.cbegin(); it != m_dropZoneState.dropZones.cend(); ++it)
    {
        const Qt::DockWidgetArea area = it.key();
        const QPolygon& dropZoneShape = it.value();

        // For the center tab drop zone, we need to translate the shape into a circle before we
        // check if the mouse position is inside the shape.
        if (area == Qt::AllDockWidgetAreas)
        {
            QRegion circleRegion(dropZoneShape.boundingRect(), QRegion::Ellipse);
            if (circleRegion.contains(pos))
            {
                return area;
            }
        }
        // For the left/right/top/bottom drop zones we can use the default polygon check if the mouse
        // position is inside the shape
        else
        {
            if (dropZoneShape.containsPoint(pos, Qt::OddEvenFill))
            {
                return area;
            }
        }
    }
 
    return Qt::NoDockWidgetArea;
}

/**
 * For a given widget, determine if it is a valid drop target and return the
 * valid drop target if applicable. If the drop target is excluded (e.g. we
 * are dragging this widget), then its parent main window will be returned.
 */
QWidget* FancyDocking::dropTargetForWidget(QWidget* widget, const QPoint& globalPos, QWidget* exclude) const
{
    auto isExcluded = [&](const QWidget* x)
    {
        for (auto i = x; i; i = i->parentWidget())
        {
            if (i == exclude)
            {
                return true;
            }
        }
        return false;
    };

    if (!widget || widget->isHidden())
    {
        return nullptr;
    }

    if (isExcluded(widget))
    {
        // If the mouse is over our excluded widget, then return its parent
        // instead so we can still evaluate for absolute drop zones
        if (widget->rect().contains(widget->mapFromGlobal(globalPos)))
        {
            return qobject_cast<QMainWindow*>(widget->parentWidget());
        }
        else
        {
            return nullptr;
        }
    }

    if (widget->rect().contains(widget->mapFromGlobal(globalPos)))
    {
        return widget;
    }

    return nullptr;
}

/**
 * Given a position in global coordinates, returns a QDockWidget, or a QMainWindow onto which
 * one can drop a widget.  This exclude the 'exclude' widget and all it's children.
 */
QWidget* FancyDocking::dropWidgetUnderMouse(const QPoint& globalPos, QWidget* exclude) const
{
    // After this logic block, this will hold a valid QMainWindow reference if
    // our current drop target is on a floating main window
    QMainWindow* dropOntoFloatingMainWindow = nullptr;
    if (qobject_cast<QDockWidget*>(m_dropZoneState.dropOnto))
    {
        // If our drop target is a dock widget, then check if its parent is
        // a floating main window
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(m_dropZoneState.dropOnto->parentWidget());
        if (mainWindow != m_mainWindow)
        {
            // If we're still hovered over the same dock widget, this shortcuts
            // all the logic below
            if (m_dropZoneState.dropOnto->rect().contains(m_dropZoneState.dropOnto->mapFromGlobal(globalPos)))
            {
                return m_dropZoneState.dropOnto;
            }
            // Otherwise, our mouse still be hovered over the same floating
            // window, so we need to give it precedence over other floating
            // main windows and the main editor window
            else
            {
                dropOntoFloatingMainWindow = mainWindow;
            }
        }
    }
    else if (m_dropZoneState.dropOnto && m_dropZoneState.dropOnto != m_mainWindow)
    {
        // If we have a valid drop target and it wasn't a dock widget, then
        // it's a QMainWindow so we need to flag it if it's floating
        dropOntoFloatingMainWindow = qobject_cast<QMainWindow*>(m_dropZoneState.dropOnto);
    }

    // Create a list of our floating drop targets separate from the dock widgets
    // on our main editor window so we can give precedence to the floating targets
    // We iterate through our floating drop targets by our z-ordered list of
    // floating dock widgets that we maintain ourselves since we can't retrieve
    // a z-ordered list from Qt, and we need to guarantee that dock widgets
    // in the front have precedence over widgets that are lower
    QList<QWidget*> floatingDropTargets;
    for (QString name : m_orderedFloatingDockWidgetNames)
    {
        QDockWidget* dockWidget = m_mainWindow->findChild<QDockWidget*>(name, Qt::FindDirectChildrenOnly);
        if (!dockWidget)
        {
            continue;
        }

        // Make sure this is a floating dock widget container
        // We need to add its dock widget children and floating main window as
        // drop targets
        if (!dockWidget->isFloating())
        {
            continue;
        }

        // Ignore this floating container if it is hidden, which means it
        // is a single pane floating window that is the one being dragged
        // so it is currently hidden
        if (dockWidget->isHidden())
        {
            continue;
        }

        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->widget());
        if (!mainWindow)
        {
            continue;
        }

        // If our current drop target lives in this floating main window,
        // then we need to add it to the front of the list so that it will
        // get precedence over other floating windows, but we need to do this
        // first so that the dock widgets of this main window will be prepended
        // in front of it
        if (mainWindow == dropOntoFloatingMainWindow)
        {
            floatingDropTargets.prepend(mainWindow);
        }

        // Add all of the child dock widgets in this floating main window
        // to our list of floating drop targets
        bool shouldAddFloatingMainWindow = true;
        for (QDockWidget* floatingDockWidget : mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
        {
            // Don't allow dock widgets that have no allowed areas to be
            // drop targets, and also prevent this floating main window
            // from being added as a drop target as well if it contains
            // a dock widget that has docking disabled
            if (floatingDockWidget->allowedAreas() == Qt::NoDockWidgetArea)
            {
                shouldAddFloatingMainWindow = false;
                continue;
            }

            // If our current drop target lives in this floating main window,
            // then put these dock widgets on the front of our list so they
            // get precedence over other floating drop targets
            if (mainWindow == dropOntoFloatingMainWindow)
            {
                floatingDropTargets.prepend(floatingDockWidget);
            }
            // Otherwise just add them to the list of other floating drop targets
            else
            {
                floatingDropTargets.append(floatingDockWidget);
            }
        }

        // If our current drop target does not live in this floating main
        // window, then store this floating main window in our list of
        // floating drop targets after its dock widgets so that they will
        // be found first
        if (shouldAddFloatingMainWindow && mainWindow != dropOntoFloatingMainWindow)
        {
            floatingDropTargets.append(mainWindow);
        }
    }

    // Then, find the normal dock widgets on the main editor window and add
    // them to the end of list so the floating widgets have priority
    QList<QDockWidget*> mainWindowDockWidgets;
    for (QDockWidget* dockWidget : m_mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
    {
        if (!dockWidget->isFloating())
        {
            mainWindowDockWidgets.append(dockWidget);
        }
    }

    // Next, check all of the floating drop targets. This includes the floating
    // dock widgets, and the floating main windows themselves so we catch the
    // absolute drop zones when hovered over the dead zone margins or the excluded
    // target (widget being dragged).
    for (QWidget* widget : floatingDropTargets)
    {
        QWidget* dropTarget = dropTargetForWidget(widget, globalPos, exclude);
        if (dropTarget)
        {
            return dropTarget;
        }
    }

    // Then, check all the dock widgets on the main window
    for (QDockWidget* dockWidget : mainWindowDockWidgets)
    {
        QWidget* dropTarget = dropTargetForWidget(dockWidget, globalPos, exclude);
        if (dropTarget)
        {
            return dropTarget;
        }
    }

    // Fallback to check if the mouse is inside our main window, which will cover
    // both the central widget (viewport) and the dead zone margins between
    // dock widgets on the main window
    if (m_mainWindow->rect().contains(m_mainWindow->mapFromGlobal(globalPos)))
    {
        return m_mainWindow;
    }

    return nullptr;
}

/**
 * Handle a mouse move event.
 */
bool FancyDocking::dockMouseMoveEvent(QDockWidget* dock, QMouseEvent* event)
{
    if (!m_state.dock)
    {
        return false;
    }

    // If we are dragging a floating dock widget, then we need to use the
    // actual dock widget child as our reference
    if (m_state.floatingDockContainer && m_state.floatingDockContainer == dock)
    {
        dock = m_state.dock;
    }

    if (m_state.dock != dock)
    {
        return false;
    }

    // use QCursor::pos(); in scenarios with multiple screens and different scale factors,
    // it's much more reliable about actually reporting a global position than
    // using event->globalPos();
    QPoint globalPos = QCursor::pos();

    if (!m_dropZoneState.dragging)
    {
        // Check if we should start dragging if the user has pressed and dragged
        // the mouse beyond the drag distance threshold, taking into account the
        // title bar height if we are dragging by the floating title bar
        QPoint dragDifference = globalPos - dock->mapToGlobal(m_state.pressPos);
        if (m_state.floatingDockContainer)
        {
            dragDifference.ry() += dock->titleBarWidget()->height();
        }
        bool shouldStartDrag = dragDifference.manhattanLength() > QApplication::startDragDistance();

        // Only initiate the tab re-ordering logic for tab widgets that have
        // multiple tabs
        if (m_state.tabWidget && m_state.tabWidget->count() > 1)
        {
            // If we are dragging a tab, we shouldn't rip the tab out until the
            // mouse leaves the tab header area
            QTabBar* tabBar = m_state.tabWidget->tabBar();
            shouldStartDrag = !tabBar->rect().contains(tabBar->mapFromGlobal(globalPos));

            // If the tab has been ripped out, we need to reset the tab widget's
            // internal drag state and update our tab index to the current
            // active tab because the initially pressed index could have changed
            // by now if the user dragged the tab inside the tab header,
            // resulting in the tabs being re-ordered
            if (shouldStartDrag)
            {
                m_state.tabWidget->finishDrag();
                m_state.tabIndex = m_state.tabWidget->currentIndex();
            }
            // Otherwise, the mouse is still being dragged inside the tab header
            // area, so pass the mouse event along to the tab widget so it can
            // use it for internally dragging the tabs to re-order them, and
            // bail out since the tab widget will handle this mouse event
            else
            {
                m_state.tabWidget->mouseMoveEvent(event);
                return true;
            }
        }

        // If we shouldn't start the drag, then bail out, otherwise we will
        // rip out the dock widget and start the dragging process
        if (!shouldStartDrag)
        {
            return false;
        }

        m_ghostWidget->show();

        // We need to explicitly grab the mouse/keyboard on our main window when
        // we start dragging a dock widget so that only our custom docking logic
        // will be executed, instead of qt's default docking.  This also allows
        // us to hide the dock widget if it's floating and still receive the events
        // since otherwise they would be lost if the widget was hidden.
        m_mainWindow->grabMouse();
        m_mainWindow->grabKeyboard();

        // If we're dragging a dock widget that is the only widget in a floating
        // window, let's hide the floating window so it doesn't get in the way.
        // If the dock widget is a tab container, then we will only hide it if
        // it only has one tab.
        QDockWidget* singleFloatingDockWidget = nullptr;
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dock->parentWidget());
        if (mainWindow && mainWindow != m_mainWindow)
        {
            QDockWidget* containerDockWidget = qobject_cast<QDockWidget*>(mainWindow->parentWidget());
            if (containerDockWidget && containerDockWidget->isFloating())
            {
                int numVisibleDockWidgets = 0;
                for (QDockWidget* dockWidget : mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
                {
                    if (dockWidget->isVisible())
                    {
                        // If this is a tab widget, then we need to count each
                        // of the tabs
                        if (dockWidget == dock && m_state.tabWidget)
                        {
                            numVisibleDockWidgets += m_state.tabWidget->count();
                        }
                        // Otherwise just count the single dock widget
                        else
                        {
                            ++numVisibleDockWidgets;
                        }
                    }
                }

                if (numVisibleDockWidgets == 1)
                {
                    singleFloatingDockWidget = containerDockWidget;
                }
            }
        }
        if (singleFloatingDockWidget)
        {
            singleFloatingDockWidget->hide();
        }
        // Otherwise, we need to hide the original widget while we are dragging
        // around the placeholder. Actual hiding it would minimize the dock
        // window, so instead we need to replace it with an empty QWidget.
        else
        {
            // If the dock widget is tabbed, then we need to grab the dock widget
            // from the tab widget
            QDockWidget* draggedDockWidget = nullptr;
            if (m_state.tabWidget && m_state.tabIndex != -1)
            {
                draggedDockWidget = qobject_cast<AzQtComponents::StyledDockWidget*>(m_state.tabWidget->widget(m_state.tabIndex));
            }
            // Otherwise, dock (same as m_state.dock) will be the actual dock
            // widget that is being dragged, so use that
            else
            {
                draggedDockWidget = dock;
            }

            // Hide the dock widgets contents, and save its content widget
            // so we can restore it later
            if (draggedDockWidget)
            {
                m_state.draggedDockWidget = draggedDockWidget;
                m_state.draggedWidget = draggedDockWidget->widget();
                draggedDockWidget->setWidget(m_emptyWidget);
                m_emptyWidget->show();
            }
        }

        m_dropZoneState.dragging = true;
    }

    if (m_dropZoneState.dragging)
    {
        // Setup the drop zones if there is a valid drop target under the mouse
        QWidget* underMouse = dropWidgetUnderMouse(globalPos, dock);
        setupDropZones(underMouse, globalPos);

        // Store the previous flag for whether or not the cursor is currently
        // over an absolute drop zone so we can compare it later
        bool previousOnAbsoluteDropZone = m_dropZoneState.onAbsoluteDropZone;

        // Check if the mouse is hovered over one of our drop zones
        Qt::DockWidgetArea area = dockAreaForPos(globalPos);

        // If we've hovered over a new drop zone, start our timer to fade in
        // the opacity of the drop zone, which also makes it inactive until
        // the max opacity has been reached
        if (area != Qt::NoDockWidgetArea && (area != m_dropZoneState.dropArea || previousOnAbsoluteDropZone != m_dropZoneState.onAbsoluteDropZone))
        {
            m_dropZoneState.dropZoneHoverOpacity = 0;
            m_dropZoneHoverFadeInTimer->start();
        }

        SetFloatingPixmapClipping(m_dropZoneState.dropOnto, area);

        // Save the drop zone area in our drag state
        m_dropZoneState.dropArea = area;

        // Calculate the placeholder rectangle based on the drag position
        QRect dockGeometry = dock->geometry();
        QRect placeholder(dockGeometry
            .translated(globalPos - dock->mapToGlobal(m_state.pressPos))
            .translated(dock->isWindow() ? QPoint() : dock->parentWidget()->mapToGlobal(QPoint())));

        QWidget* draggedDockWidget = m_state.dock;
        int offsetForMissingTitleBar = 0;

        if (m_state.tabWidget)
        {
            QWidget* widget = m_state.tabWidget->widget(m_state.tabIndex);
            if (widget)
            {
                draggedDockWidget = widget;
                offsetForMissingTitleBar = AzQtComponents::DockBar::Height;
            }
        }

        // If we restored the last floating screen grab for this dock widget,
        // then we need to change the placeholder size and update the X coordinate
        // to account for the extrapolated mouse press position
        if (m_lastFloatingScreenGrab.contains(draggedDockWidget->objectName()))
        {
            QSize lastFloatingSize = m_state.dockWidgetScreenGrab.size;
            int pressPosX = m_state.pressPos.x();
            int relativeX = (int)(((float)pressPosX / (float)dockGeometry.width()) * ((float)lastFloatingSize.width()));

            placeholder.setSize(lastFloatingSize);
            placeholder.translate(pressPosX - relativeX, 0);
        }

        int screenIndex = m_desktopWidget->screenNumber(globalPos);
        m_state.setPlaceholder(placeholder, screenIndex);

        m_ghostWidget->Enable();
        RepaintFloatingIndicators();
    }

    return m_dropZoneState.dragging;
}

void FancyDocking::RepaintFloatingIndicators()
{
    updateFloatingPixmap();

    if (m_activeDropZoneWidgets.size())
    {
        for (AzQtComponents::FancyDockingDropZoneWidget* dropZoneWidget : m_activeDropZoneWidgets)
        {
            dropZoneWidget->update();
        }
    }

    m_ghostWidget->update();
}

void FancyDocking::SetFloatingPixmapClipping(QWidget* dropOnto, Qt::DockWidgetArea area)
{
    // If our drop target isn't a main window, then retrieve the main window
    // from the dock widget parent
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(m_dropZoneState.dropOnto);
    if (!mainWindow && m_dropZoneState.dropOnto)
    {
        mainWindow = qobject_cast<QMainWindow*>(m_dropZoneState.dropOnto->parentWidget());
    }

    if ((mainWindow == m_mainWindow) && (area != Qt::NoDockWidgetArea) && m_dropZoneState.dropOnto)
    {
        m_ghostWidget->EnableClippingToDockWidgets();
    }
    else
    {
        m_ghostWidget->DisableClippingToDockWidgets();
    }
}

/**
 * Handle a mouse press event.
 */
bool FancyDocking::dockMousePressEvent(QDockWidget* dock, QMouseEvent* event)
{
    QPoint pressPos = event->pos();
    if (event->button() != Qt::LeftButton || !canDragDockWidget(dock, pressPos))
    {
        return false;
    }

    if (m_state.dock)
    {
        qWarning() << "Press event without the previous being a release?" << dock << m_state.dock;
        return true;
    }

    startDraggingWidget(dock, pressPos);

    // Show the floating pixmap, but don't start it rendering
    // It will early out in it's paint event, but then there
    // won't be any delay when the user has dragged far enough
    // to trigger dragging
    m_ghostWidget->show();

    return true;
}

/*
 * Initialize the dragging state for the specified dock widget.  The tabIndex will be -1
 * if you are dragging a regular panel by the title bar, and will be set to a valid index
 * if you are dragging a tab of a DockTabWidget
 */
void FancyDocking::startDraggingWidget(QDockWidget* dock, const QPoint& pressPos, int tabIndex)
{
    if (!dock)
    {
        return;
    }

    // If we are dragging a floating window, we need to grab a reference to its
    // actual single visible child dock widget to use as our target
    if (dock->isFloating())
    {
        QDockWidget* childDockWidget = nullptr;
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dock->widget());
        if (mainWindow)
        {
            for (QDockWidget* dockWidget : mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
            {
                if (dockWidget->isVisible())
                {
                    childDockWidget = dockWidget;
                    break;
                }
            }
        }

        if (!childDockWidget)
        {
            return;
        }

        // Use the visible child as our drag target going forward, and keep a
        // reference to the floating container for decision making later
        m_state.floatingDockContainer = dock;
        dock = childDockWidget;
    }

    QWidget* draggedWidget = dock;
    m_state.dock = dock;

    // If we are dragging a tab widget, then get a reference to the appropriate widget
    // so we can get the screen grab of just that tab
    if (tabIndex != -1 && m_state.tabWidget)
    {
        QWidget* widget = m_state.tabWidget->widget(tabIndex);
        if (widget)
        {
            draggedWidget = widget;
        }
    }

    // If we have cached the last floating screen grab for this dock widget,
    // then retrieve it here, otherwise retrieve a screen grab from the dock
    // widget itself
    QString paneName = draggedWidget->objectName();
    if (m_lastFloatingScreenGrab.contains(paneName))
    {
        m_state.dockWidgetScreenGrab = m_lastFloatingScreenGrab[paneName];
    }
    else
    {
        m_state.dockWidgetScreenGrab = { draggedWidget->grab(), draggedWidget->size() };
    }

    m_state.tabIndex = tabIndex;
    m_state.pressPos = pressPos;
    m_dropZoneState.dragging = false;
    setupDropZones(nullptr);
}

bool FancyDocking::dockMouseReleaseEvent(QDockWidget* dock, QMouseEvent* event)
{
    if (!m_state.dock || event->button() != Qt::LeftButton)
    {
        return false;
    }

    // If we are dragging a floating dock widget, then we need to use the
    // actual dock widget child as our reference
    if (m_state.floatingDockContainer && m_state.floatingDockContainer == dock)
    {
        dock = m_state.dock;
    }

    if (m_dropZoneState.dragging)
    {
        Qt::DockWidgetArea area = m_dropZoneState.dropArea;

        // If the modifier key is pressed, or the hovered drop zone opacity
        // hasn't faded in all the way yet, then ignore the drop zone area
        // which will make the widget floating
        bool modifiedKeyPressed = AzQtComponents::FancyDockingDropZoneWidget::CheckModifierKey();
        if (modifiedKeyPressed || m_dropZoneState.dropZoneHoverOpacity != g_FancyDockingConstants.dropZoneOpacity)
        {
            area = Qt::NoDockWidgetArea;
        }

        dropDockWidget(dock, m_dropZoneState.dropOnto, area);
    }
    else
    {
        // Pass the mouse release event to the tab widget (if applicable) since
        // we grab the mouse/keyboard from it
        if (m_state.tabWidget)
        {
            m_state.tabWidget->mouseReleaseEvent(event);
        }

        clearDraggingState();
    }

    return true;
}

/*
 * Handle tab index presses from our DockTabWidgets
 */
void FancyDocking::onTabIndexPressed(int index)
{
    if (index == -1)
    {
        return;
    }

    AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(sender());
    if (!tabWidget)
    {
        return;
    }

    QDockWidget* dockWidget = qobject_cast<QDockWidget*>(tabWidget->parent());
    if (!dockWidget)
    {
        return;
    }

    // Initialize our drag state with the dock widget that contains our tab widget
    QPoint pressPos = dockWidget->mapFromGlobal(QCursor::pos());
    m_state.tabWidget = tabWidget;
    startDraggingWidget(dockWidget, pressPos, index);

    // We need to grab the mouse and keyboard immediately because the QTabBar that is part of our
    // DockTabWidget overrides the mouse/key press/move/release events
    m_mainWindow->grabMouse();
    m_mainWindow->grabKeyboard();
}

/**
 * Handle tab index presses from our DockTabWidgets so we can delete the tab coutainer if all the tabs are removed
 */
void FancyDocking::onTabCountChanged(int count)
{
    // We only care if there are no tabs left
    if (count != 0)
    {
        return;
    }

    // Retrieve the dock widget container for our tab widget
    QDockWidget* dockWidget = getTabWidgetContainer(sender());
    if (!dockWidget)
    {
        return;
    }

    // Retrieve the main window that our dock widget container lives in
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->parent());
    if (!mainWindow)
    {
        return;
    }

    // Remove the dock widget tab container from the main window and then delete it since
    // it is no longer needed (this will also delete the dock tab widget since it is a child)
    mainWindow->removeDockWidget(dockWidget);
    dockWidget->setParent(nullptr);
    dockWidget->deleteLater();

    // If this tab widget was on a floating window, run the check if this main
    // window needs to be destroyed (if this tab widget was the only thing
    // left in this floating window)
    if (mainWindow != m_mainWindow)
    {
        destroyIfUseless(mainWindow);
    }
}

/**
 * Whenever widgets are inserted as tabs, cache the tab container they were
 * added to so that if they are closed, we can restore them to the last tab
 * container they were in
 */
void FancyDocking::onTabWidgetInserted(QWidget* widget)
{
    if (!widget)
    {
        return;
    }

    // Retrieve the dock widget container for our tab widget
    QDockWidget* dockWidget = getTabWidgetContainer(sender());
    if (!dockWidget)
    {
        return;
    }

    m_lastTabContainerForDockWidget[widget->objectName()] = dockWidget->objectName();
}

/**
 * Handle request to undock a tab from a tab group, or undock the entire tab
 * group from its main window
 */
void FancyDocking::onUndockTab(int index)
{
    QDockWidget* tabWidgetContainer = getTabWidgetContainer(sender());
    if (!tabWidgetContainer)
    {
        return;
    }

    // If the index given is -1, then we are going to undock the entire tab
    // group, so grab the tab widget container as our target dock widget
    QDockWidget* dockWidget = nullptr;
    if (index == -1)
    {
        dockWidget = tabWidgetContainer;
    }
    // Otherwise, grab the specific dock widget from the tab widget using
    // the specified tab index
    else
    {
        AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(sender());
        if (!tabWidget)
        {
            return;
        }

        // Set the necessary drag state parameters so that we can undock the
        // given dock widget from the tab widget
        m_state.tabWidget = tabWidget;
        m_state.tabIndex = index;
        dockWidget = qobject_cast<QDockWidget*>(tabWidget->widget(index));
    }

    undockDockWidget(dockWidget, tabWidgetContainer);
}

/**
 * Handle request from a dock widget to be undocked from its main window
 */
void FancyDocking::onUndockDockWidget()
{
    QDockWidget* dockWidget = qobject_cast<QDockWidget*>(sender());
    undockDockWidget(dockWidget);
}

/**
 * Undock the specified dock widget
 */
void FancyDocking::undockDockWidget(QDockWidget* dockWidget, QDockWidget* placeholder)
{
    if (!dockWidget)
    {
        return;
    }

    // Offset the geometry that the undocked dock widget will be given from the
    // placeholder geometry with the height of our title dock bar so that it isn't
    // undocked directly above its current position
    int offset = AzQtComponents::DockBar::Height;

    // The placeholder is an optional parameter to provide a different reference
    // geometry with which to undock the dock widget, so if it isn't provided,
    // then just use our dock widget for reference
    // In practice, if the reference geometry is not provided, that means it's not
    // untabbifying, which means that the title bar will get re-added and/or the size
    // doesn't take it into account, so we need to below otherwise the widget gets smaller
    QSize newSize;
    QPoint newPosition;
    if (!placeholder)
    {
        newSize = dockWidget->size();
        newSize.setHeight(newSize.height() + AzQtComponents::DockBar::Height);
        newPosition = dockWidget->mapToGlobal(QPoint(offset, offset));
    }
    else
    {
        newSize = placeholder->size();
        newPosition = placeholder->mapToGlobal(QPoint(offset, offset));
    }

    // Setup the new placeholder using the screen of its new position
    int screenIndex = m_desktopWidget->screenNumber(newPosition);
    QScreen* screen = m_desktopScreens[screenIndex];
    m_state.setPlaceholder(QRect(newPosition, newSize), screen);
    updateFloatingPixmap();

    // Undock the dock widget
    dropDockWidget(dockWidget, nullptr, Qt::NoDockWidgetArea);
}

/**
 * If the specified object is our custom dock tab widget, then return its QDockWidget
 * parent container, otherwise return nullptr
 */
QDockWidget* FancyDocking::getTabWidgetContainer(QObject* obj)
{
    // Check if the object is our custom dock tab widget
    AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(obj);
    if (!tabWidget)
    {
        return nullptr;
    }

    // Retrieve the dock widget container for our tab widget
    return qobject_cast<QDockWidget*>(tabWidget->parent());
}

/*
 * Determine whether or not you can drag the specified dock widget based on if the mouse
 * position is inside the title bar
 */
bool FancyDocking::canDragDockWidget(QDockWidget* dock, QPoint mousePos)
{
    if (!dock)
    {
        return false;
    }

    // Disable dragging a dock widget if it has no dockable areas allowed
    if (dock->allowedAreas() == Qt::NoDockWidgetArea)
    {
        return false;
    }

    QWidget* title = dock->titleBarWidget();
    if (title)
    {
        return title->geometry().contains(mousePos);
    }

    // Some dock widgets don't have a title bar (DockTabWidget and the viewport)
    return false;
}

/**
 * Make a dock widget floating by creating a new floating main window containt
 * for it and adding it as the only dock widget
 */
void FancyDocking::makeDockWidgetFloating(QDockWidget* dock, const QRect& geometry)
{
    if (!dock)
    {
        return;
    }

    // Create a floating window container for this dock widget
    QMainWindow* mainWindow = createFloatingMainWindow(getUniqueDockWidgetName(g_floatingWindowPrefix), geometry);
    dock->setParent(mainWindow);
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);
    dock->show();
}

/**
 * Safe version of the QMainWindow splitDockWidget method to workaround an odd Qt bug
 */
void FancyDocking::splitDockWidget(QMainWindow* mainWindow, QDockWidget* target, QDockWidget* dropped, Qt::Orientation orientation)
{
    if (!mainWindow || !target || !dropped)
    {
        return;
    }

    // Calculate the split width (or height) so that our target and dropped
    // widgets can be resized to share the space
    int splitSize = 0;
    if (orientation == Qt::Horizontal)
    {
        splitSize = target->width() / 2;
    }
    else
    {
        splitSize = target->height() / 2;
    }

    // As detailed in LY-42497, there is an odd Qt bug where if dock widget A is
    // already split with dock widget B, and you try to split B with A in the
    // opposite orientation after restoring the QMainWindow state, you will end
    // up with what looks like an empty dock widget in the old location of A,
    // but it's actually a ghost copy in the main window layout of A, which
    // you can tell because it will flicker sometimes and you can see the contents
    // of A.  So to fix, we need to remove the widget being dropped from the main
    // window layout before we split it with the target, and show it afterwards
    // since removing it will also hide it.  This eliminates the ghost copy of
    // the dropped widget that gets left in the main window layout.
    mainWindow->removeDockWidget(dropped);
    mainWindow->splitDockWidget(target, dropped, orientation);
    dropped->show();

    // Resize the target and dropped widgets so they evenly split the space
    // in the orientation that they were split
    mainWindow->resizeDocks({ target, dropped }, { splitSize, splitSize }, orientation);
}

/**
 * Dock a QDockWidget onto a QDockWidget or a QMainWindow
 * NOTE: This method is responsible for calling clearDraggingState() when it has
 * completed its actions
 */
void FancyDocking::dropDockWidget(QDockWidget* dock, QWidget* onto, Qt::DockWidgetArea area)
{
    // If the dock widget we are dropping is currently a tab, we need to retrieve it from
    // the tab widget, and remove it as a tab.  We also need to remove its item from our
    // cache of widget <-> tab container since we are moving it somewhere else.
    if (m_state.tabWidget)
    {
        int index = m_state.tabIndex;
        AzQtComponents::StyledDockWidget* dockWidget = qobject_cast<AzQtComponents::StyledDockWidget*>(m_state.tabWidget->widget(index));
        m_lastTabContainerForDockWidget.remove(dockWidget->objectName());
        m_state.tabWidget->removeTab(index);
        dock = dockWidget;
    }

    if (area == Qt::NoDockWidgetArea)
    {
        // Make this dock widget floating, since it has been dropped on no dock area
        // We need to adjust the geometry based on the title bar height offset
        QRect titleBarAdjustedGeometry = m_state.placeholder().adjusted(0, -dock->titleBarWidget()->height(), 0, 0);
        makeDockWidgetFloating(dock, titleBarAdjustedGeometry);
        clearDraggingState();

        // We can remove any cached floating screen grab for this dock widget
        // now that it's been undocked as floating, since it will be cached
        // whenever it is docked into a main window in the future
        m_lastFloatingScreenGrab.remove(dock->objectName());
    }
    else
    {
        // If we are docking a dock widget that is currently the only dock widget
        // in a floating main window, then cache its screen grab so that we can
        // restore its last floating size when undocking it later in the future
        if (qobject_cast<AzQtComponents::StyledDockWidget*>(dock)->isSingleFloatingChild())
        {
            m_lastFloatingScreenGrab[dock->objectName()] = m_state.dockWidgetScreenGrab;
        }

        // do the rest after the show has been fully processed, just to be sure
        QTimer::singleShot(0, [=]()
        {
            // Ensure that the dock window is shown, because we may have hidden it when the drag started
            dock->show();

            // Handle an absolute drop zone
            QMainWindow* mainWindow = qobject_cast<QMainWindow*>(onto);
            if (m_dropZoneState.onAbsoluteDropZone)
            {
                // Find the main window for the drop target (if it's not a main window),
                // since we will use it instead of the drop target itself for docking on the
                // absolute edge
                if (!mainWindow)
                {
                    mainWindow = qobject_cast<QMainWindow*>(onto->parentWidget());
                }

                // Fallback to the editor main window if we couldn't find one
                if (!mainWindow)
                {
                    mainWindow = m_mainWindow;
                }

                // Set the absolute drop zone corners properly for this
                // main window
                setAbsoluteCornersForDockArea(mainWindow, area);
            }

            if (mainWindow)
            {
                dock->setParent(onto);
                if (area == Qt::AllDockWidgetAreas)
                {
                    // should not happen
                }
                else
                {
                    // LY-43595 (similar to LY-42497), there is a bug in Qt where
                    // re-docking a dock widget to different areas in the main
                    // window layout if it was already split in a different
                    // part in the layout results in the dock widget being
                    // duplicated in the layout
                    // We have to show the dock widget after adding it because
                    // the call to removeDockWidget hides the dock widget
                    mainWindow->removeDockWidget(dock);
                    mainWindow->addDockWidget(area, dock, orientation(area));
                    dock->show();
                }
            }
            else
            {
                QDockWidget* dockWidget = qobject_cast<QDockWidget*>(onto);
                if (dockWidget)
                {
                    mainWindow = static_cast<QMainWindow*>(dockWidget->parentWidget());
                    dock->setParent(mainWindow);
                    if (area == Qt::AllDockWidgetAreas)
                    {
                        tabifyDockWidget(dockWidget, dock, mainWindow, &m_state.dockWidgetScreenGrab);
                    }
                    else
                    {
                        splitDockWidget(mainWindow, dockWidget, dock, orientation(area));
                        if (area == Qt::LeftDockWidgetArea || area == Qt::TopDockWidgetArea)
                        {
                            // Is was actually the other way around that we needed to do.
                            // But we needed the first call so the dock is in the right area.
                            splitDockWidget(mainWindow, dock, dockWidget, orientation(area));
                        }
                    }
                }
            }

            clearDraggingState();
        });
    }
}

/**
 * Dock the dropped dock widget into our custom tab system on the drop target,
 * and return a reference to the tab widget
 */
AzQtComponents::DockTabWidget* FancyDocking::tabifyDockWidget(QDockWidget* dropTarget, QDockWidget* dropped, QMainWindow* mainWindow, FancyDocking::WidgetGrab* droppedGrab)
{
    if (!dropTarget || !dropped || !mainWindow)
    {
        return nullptr;
    }

    // Flag that we have a tabify action in progress so that we can ignore our
    // destroyIfUseless cleanup method that gets inadvertantly triggered
    // while we are tabifying
    QScopedValueRollback<bool> rollback(m_state.tabifyInProgress, true);

    // Check if the drop target is already one of our custom tab widgets
    AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(dropTarget->widget());

    QString saveGrabName;
    if (!tabWidget)
    {
        saveGrabName = dropTarget->objectName();
    }
    else if (tabWidget->count() == 1)
    {
        saveGrabName = tabWidget->tabText(0);
    }

    // Special case this one: if we're dropping onto an untabbed widget, save it's state so that it resizes properly
    // when torn off
    // Should be cleared again when the widget goes back to being a single tab
    if (!m_lastFloatingScreenGrab.contains(saveGrabName))
    {
        m_lastFloatingScreenGrab[saveGrabName] = { dropTarget->grab(), dropTarget->size() };
    }

    if (!tabWidget)
    {
        // The drop target wasn't already a custom tab widget, so create one and replace the drop target
        // with the tab widget (with the drop target as the initial tab)
        tabWidget = createTabWidget(mainWindow, dropTarget);
    }

    // Special case this one: if a widget gets tabbified, when it's untabbified, it won't render properly
    // for the floating pixmap. So we force it to store the state here, if it isn't already
    // It's only if it isn't already, because if it was dragged from a tabgroup and into another tabgroup
    // then we shouldn't be saving it (because it's already been saved)
    QString droppedName = dropped->objectName();
    if (!m_lastFloatingScreenGrab.contains(droppedName) && (droppedGrab != nullptr))
    {
        m_lastFloatingScreenGrab[droppedName] = *droppedGrab;
    }

    // If our dropped widget is also a tab widget (e.g. we dragged a floating tab container),
    // then we need to move the tabs into our drop target tab widget
    int newActiveIndex = 0;
    if (m_state.floatingDockContainer && droppedName.startsWith(g_tabContainerPrefix))
    {
        AzQtComponents::DockTabWidget* oldTabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(dropped->widget());
        if (!oldTabWidget)
        {
            return tabWidget;
        }

        // Calculate the new active tab index based on adding the tabs to our
        // drop target
        int numOldTabs = oldTabWidget->count();
        newActiveIndex = tabWidget->count() + oldTabWidget->currentIndex();

        // Remove our dropped tabs from their existing tab widget and add them to
        // the drop target tab widget
        for (int i = 0; i < numOldTabs; ++i)
        {
            QDockWidget* dockWidget = qobject_cast<QDockWidget*>(oldTabWidget->widget(0));
            m_lastTabContainerForDockWidget.remove(dockWidget->objectName());
            oldTabWidget->removeTab(0);
            tabWidget->addTab(dockWidget);
        }
    }
    // Otherwise, the dropped widget is a normal dock widget so just add it as
    // a new tab
    else
    {
        newActiveIndex = tabWidget->addTab(dropped);
    }

    // Set the dropped widget as the active tab (or the active tab of the dropped
    // tab widget)
    tabWidget->setCurrentIndex(newActiveIndex);

    return tabWidget;
}

/**
 * Reserve the absolute corners for the specified drop zone area for this
 * main window so that any widget docked to that area will take the absolute edge
 */
void FancyDocking::setAbsoluteCornersForDockArea(QMainWindow* mainWindow, Qt::DockWidgetArea area)
{
    if (!mainWindow)
    {
        return;
    }

    // Since a widget is being docked on an absolute drop zone,
    // we need to reserve the corners for the absolute drop
    // area so that it will take precedence over other widgets
    // that may already be docked in absolute positions
    switch (area)
    {
    case Qt::LeftDockWidgetArea:
        mainWindow->setCorner(Qt::TopLeftCorner, area);
        mainWindow->setCorner(Qt::BottomLeftCorner, area);
        break;
    case Qt::RightDockWidgetArea:
        mainWindow->setCorner(Qt::TopRightCorner, area);
        mainWindow->setCorner(Qt::BottomRightCorner, area);
        break;
    case Qt::TopDockWidgetArea:
        mainWindow->setCorner(Qt::TopLeftCorner, area);
        mainWindow->setCorner(Qt::TopRightCorner, area);
        break;
    case Qt::BottomDockWidgetArea:
        mainWindow->setCorner(Qt::BottomLeftCorner, area);
        mainWindow->setCorner(Qt::BottomRightCorner, area);
        break;
    }
}

bool FancyDocking::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_mainWindow)
    {
        AzQtComponents::StyledDockWidget* dockWidget = nullptr;
        switch (event->type())
        {
        case QEvent::ChildPolished:
            dockWidget = qobject_cast<AzQtComponents::StyledDockWidget*>(static_cast<QChildEvent*>(event)->child());
            if (dockWidget)
            {
                dockWidget->installEventFilter(this);
                // Remove the movable feature because we will handle that ourselves
                dockWidget->setFeatures(dockWidget->features() & ~(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable));

                // Connect to undock requests from this dock widget
                // MUST BE A UNIQUE CONNECTION! Otherwise, every time through this method will connect to the signal again
                QObject::connect(dockWidget, &AzQtComponents::StyledDockWidget::undock, this, &FancyDocking::onUndockDockWidget, Qt::UniqueConnection);
            }
            break;
        case QEvent::MouseMove:
            if (m_state.dock && dockMouseMoveEvent(m_state.dock, static_cast<QMouseEvent*>(event)))
            {
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (m_state.dock && dockMouseReleaseEvent(m_state.dock, static_cast<QMouseEvent*>(event)))
            {
                return true;
            }
            break;
        case QEvent::KeyPress:
        case QEvent::ShortcutOverride:
            if (m_dropZoneState.dragging)
            {
                // Cancel the dragging state when the Escape key is pressed
                QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
                if (keyEvent->key() == Qt::Key_Escape)
                {
                    clearDraggingState();
                }
                else
                {
                    // modifier keys can affect things, so do a redraw
                    RepaintFloatingIndicators();
                }
            }
            break;
        case QEvent::KeyRelease:
            if (m_dropZoneState.dragging)
            {
                // modifier keys can affect things, so do a redraw
                RepaintFloatingIndicators();
            }
            break;
        case QEvent::WindowDeactivate:
            // If our main window is deactivated while we are in the middle of
            // a docking drag operation (e.g. popup dialog for new level), we
            // should cancel our drag operation because the mouse release event
            // will be lost since we lost focus
            if (m_dropZoneState.dragging)
            {
                clearDraggingState();
            }
            break;
        }
    }
    else
    {
        QDockWidget* dockWidget = qobject_cast<QDockWidget*>(watched);
        if (dockWidget)
        {
            QString dockWidgetName = dockWidget->objectName();
            switch (event->type())
            {
            case QEvent::MouseButtonPress:
                if (dockMousePressEvent(dockWidget, static_cast<QMouseEvent*>(event)))
                {
                    return true;
                }
                break;
            case QEvent::MouseMove:
                if (dockMouseMoveEvent(dockWidget, static_cast<QMouseEvent*>(event)))
                {
                    return true;
                }
                break;
            case QEvent::MouseButtonRelease:
                if (dockMouseReleaseEvent(dockWidget, static_cast<QMouseEvent*>(event)))
                {
                    return true;
                }
                break;
            case QEvent::HideToParent:
                {
                    // The dockwidget was hidden, we the parent floating mainwindow might need to be
                    // destroyed. But delay the call to destroyIfUseless to the next iteration of the
                    // event loop, as the it might only be temporarily hidden (e.g. reparenting).
                    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->parent());
                    QTimer::singleShot(0, mainWindow, [this, mainWindow] {
                        destroyIfUseless(mainWindow);
                    });
                    break;
                }
            case QEvent::Close:
                // If the user tries to close an entire floating window using
                // the top title bar, we need to handle the close ourselves
                if (dockWidgetName.startsWith(g_floatingWindowPrefix))
                {
                    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->widget());
                    if (mainWindow)
                    {
                        // Close the child dock widgets in our floating main window individually so
                        // that they will eventually trigger our destroyIfUseless method, which will
                        // properly save the floating window state in our m_restoreFloatings before
                        // deleting the floating main window, so the next time any of these child
                        // panes are opened, we can re-create the floating main window and restore
                        // them properly
                        for (QDockWidget* childDockWidget : mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
                        {
                            if (childDockWidget->isVisible() && !childDockWidget->close())
                            {
                                // If the child dock widget rejected the close,
                                // then no need to continue trying to close the
                                // other children, we can just stop now and ignore
                                // the close event
                                static_cast<QCloseEvent*>(event)->ignore();
                                break;
                            }
                        }
                        return true;
                    }
                }
                break;
            case QEvent::WindowActivate:
            case QEvent::ZOrderChange:
                // Whenever a floating dock widget is raised to the front, we need
                // to move it to the front of our z-order list of floating dock widget
                // names, since Qt doesn't have a way of retrieving the z-order of our
                // floating dock widgets. The raise can either occur when the user clicks
                // inside a floating dock widget (WindowActivate), or if the raise() method
                // is called manually when dragging a dock widget on top of the floating
                // dock widget (ZOrderChange)
                if (dockWidgetName.startsWith(g_floatingWindowPrefix))
                {
                    m_orderedFloatingDockWidgetNames.removeAll(dockWidgetName);
                    m_orderedFloatingDockWidgetNames.prepend(dockWidgetName);
                }
                break;
            }
        }
        else
        {
            QPointer<QMainWindow> mainWindow = qobject_cast<QMainWindow*>(watched);
            if (mainWindow)
            {
                QDockWidget* dockWidget = nullptr;
                switch (+event->type())
                {
                case QEvent::ChildRemoved:
                    SetDragOrDockOnFloatingMainWindow(mainWindow);
                    destroyIfUseless(mainWindow);
                    dockWidget = qobject_cast<QDockWidget*>(static_cast<QChildEvent*>(event)->child());
                    if (dockWidget)
                    {
                        // If the dock was deleted, the qobject_cast would fail. So this mean the widget will
                        // be added somewhere else
                        if (!dockWidget->objectName().isEmpty())
                        {
                            m_placeholders.remove(dockWidget->objectName());
                        }
                    }
                    break;
                case QEvent::ChildPolished:
                    // Queue this call since the dock widget won't be visible yet
                    QTimer::singleShot(0, this, [this, mainWindow] {
                        if (mainWindow)
                        {
                            SetDragOrDockOnFloatingMainWindow(mainWindow);
                        }
                    });

                    dockWidget = qobject_cast<QDockWidget*>(static_cast<QChildEvent*>(event)->child());
                    if (dockWidget)
                    {
                        if (!dockWidget->objectName().isEmpty())
                        {
                            m_placeholders[dockWidget->objectName()] = watched->parent()->objectName();
                        }
                    }
                    break;
                }
            }
        }
    }

    return false;
}

/**
 * If a floating main window has multiple dock widgets, its top title bar should
 * be used for just dragging around to re-position, but if there's only a single
 * dock widget (or single tab widget), then the top title bar should allow
 * the single dock widget to be docked
 */
void FancyDocking::SetDragOrDockOnFloatingMainWindow(QMainWindow* mainWindow)
{
    if (!mainWindow)
    {
        return;
    }

    int count = NumVisibleDockWidgets(mainWindow);
    AzQtComponents::StyledDockWidget* floatingDockWidget = qobject_cast<AzQtComponents::StyledDockWidget*>(mainWindow->parentWidget());
    if (floatingDockWidget)
    {
        AzQtComponents::TitleBar* titleBar = floatingDockWidget->customTitleBar();
        if (titleBar)
        {
            bool dragEnabled = (count > 1);

            // If there is only a single dock widget in this floating main window
            // and it has no allowed dockable areas, then set the top title bar
            // be used for dragging to reposition instead of docking
            if (count == 1)
            {
                QDockWidget* singleDockWidget = mainWindow->findChild<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
                if (singleDockWidget && singleDockWidget->allowedAreas() == Qt::NoDockWidgetArea)
                {
                    dragEnabled = true;
                }
            }

            titleBar->setDragEnabled(dragEnabled);
        }
    }
}

void FancyDocking::updateFloatingPixmap()
{
    if (m_dropZoneState.dragging && m_state.placeholder().isValid())
    {
        bool modifiedKeyPressed = AzQtComponents::FancyDockingDropZoneWidget::CheckModifierKey();

        m_ghostWidget->setWindowOpacity(modifiedKeyPressed ? 1.0f : g_FancyDockingConstants.draggingDockWidgetOpacity);
        m_ghostWidget->setPixmap(m_state.dockWidgetScreenGrab.screenGrab, m_state.placeholder(), m_state.placeholderScreen());
    }
}

void FancyDocking::StartDropZone(QWidget* dropZoneContainer, const QPoint& globalPos)
{
    // Find any screens that the drop zone container is on
    QList<QScreen*> dropZoneScreens;
    if (dropZoneContainer)
    {
        QRect dropTargetRect = dropZoneContainer->geometry();
        QWidget* dropTargetParent = dropZoneContainer->parentWidget();
        if (dropTargetParent)
        {
            dropTargetRect.moveTopLeft(dropTargetParent->mapToGlobal(dropTargetRect.topLeft()));
        }
        for (QScreen* screen : m_desktopScreens)
        {
            if (dropTargetRect.intersects(screen->geometry()))
            {
                dropZoneScreens.append(screen);
            }
        }
    }

    // If there's no drop zone target or couldn't find the screen the drop zone
    // target is on, then pick the screen the mouse is currently on so we can
    // have that drop zone widget warmed up
    if (dropZoneScreens.isEmpty())
    {
        for (QScreen* screen : m_desktopScreens)
        {
            if (screen->geometry().contains(globalPos))
            {
                dropZoneScreens.append(screen);
                break;
            }
        }
    }

    // Raise any current active drop zone widgets that should still be active
    // and stop any that should no longer be active
    int numActiveDropZoneWidgets = m_activeDropZoneWidgets.size();
    for (int i = 0; i < numActiveDropZoneWidgets; ++i)
    {
        AzQtComponents::FancyDockingDropZoneWidget* dropZoneWidget = m_activeDropZoneWidgets.takeFirst();
        QScreen* dropZoneScreen = dropZoneWidget->GetScreen();
        if (dropZoneScreens.contains(dropZoneScreen))
        {
            // This screen is already active, so remove it from our list of
            // drop zone screens that need to be activated and raise it
            dropZoneScreens.removeAll(dropZoneScreen);
            dropZoneWidget->raise();

            // Put this drop zone widget back on the end of our active list
            // since we've already processed it
            m_activeDropZoneWidgets.append(dropZoneWidget);
        }
        else
        {
            // Stop this active drop zone widget since it's no longer needed
            dropZoneWidget->Stop();
        }
    }

    // Any screens left aren't active already, so they need to be created if
    // they haven't been already and started
    for (QScreen* screen : dropZoneScreens)
    {
        // Create this drop zone widget if it doesn't already exist, and add
        // it to our list of active drop zone widgets
        AzQtComponents::FancyDockingDropZoneWidget* dropZoneWidget = m_dropZoneWidgets[screen];
        if (!dropZoneWidget)
        {
            m_dropZoneWidgets[screen] = dropZoneWidget = new AzQtComponents::FancyDockingDropZoneWidget(m_mainWindow, this, screen, &m_dropZoneState);
        }
        m_activeDropZoneWidgets.append(dropZoneWidget);

        // Start and raise this drop zone widget
        dropZoneWidget->Start();
        dropZoneWidget->raise();
    }

    // floating pixmap is always on top; it'll clip what it's supposed to
    m_ghostWidget->raise();
}

void FancyDocking::StopDropZone()
{
    if (m_activeDropZoneWidgets.size())
    {
        // we have to ensure that we force a repaint, so that there isn't
        // one frame of junk the next time we show the floating drop zones
        for (AzQtComponents::FancyDockingDropZoneWidget* dropZoneWidget : m_activeDropZoneWidgets)
        {
            dropZoneWidget->repaint();
            dropZoneWidget->Stop();
        }
        m_activeDropZoneWidgets.clear();
    }
}

/**
 * Analog to QMainWindow::saveState(). The state can be restored with FancyDocking::restoreState()
 */
QByteArray FancyDocking::saveState()
{
#ifdef KDAB_MAC_PORT
    SerializedMapType map;
    for (QDockWidget* dockWidget : m_mainWindow->findChildren<QDockWidget*>(
            QRegularExpression(QString("%1.*").arg(g_floatingWindowPrefix)), Qt::FindChildrenRecursively))
    {
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->widget());
        if (!mainWindow)
        {
            continue;
        }
        const auto subs = mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);

        // Don't persist any floating windows that have no dock widgets
        if (subs.size() == 0)
        {
            continue;
        }

        QStringList names;
        std::transform(subs.begin(), subs.end(), std::back_inserter(names),
            [](QDockWidget* o) { return o->objectName(); });
        map[dockWidget->objectName()] = qMakePair(names, mainWindow->saveState());
    }

    // Find all of our tab container dock widgets that hold our dock tab widgets
    SerializedTabType tabContainers;
    for (QDockWidget* dockWidget : m_mainWindow->findChildren<QDockWidget*>(
        QRegularExpression(QString("%1.*").arg(g_tabContainerPrefix)), Qt::FindChildrenRecursively))
    {
        AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(dockWidget->widget());
        if (!tabWidget)
        {
            continue;
        }

        // Retrieve the names of all the tabs, which correspond to their dock widget object names (view pane names)
        QStringList tabNames;
        int numTabs = tabWidget->count();
        for (int i = 0; i < numTabs; ++i)
        {
            tabNames.append(tabWidget->tabText(i));
        }

        // Retrieve the main window for the tab widget so that we can see if it
        // is docked in our main window, or in a floating window
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->parentWidget());
        if (!mainWindow)
        {
            continue;
        }

        // If the tab container is docked in our main window, we will store the
        // floatingDockName as empty.  Otherwise, we need to retrieve the name
        // of the floating dock widget so we can restore the tab container
        // to the appropriate main window.
        QString floatingDockName;
        if (mainWindow != m_mainWindow)
        {
            QDockWidget* floatingDockWidget = qobject_cast<QDockWidget*>(mainWindow->parentWidget());
            if (floatingDockWidget)
            {
                floatingDockName = floatingDockWidget->objectName();
            }
        }

        // Store this tab container state
        TabContainerType state;
        state.floatingDockName = floatingDockName;
        state.tabNames = tabNames;
        state.currentIndex = tabWidget->currentIndex();
        tabContainers[dockWidget->objectName()] = state;
    }

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << quint32(VersionMarker) << m_mainWindow->saveState() << map
    << m_placeholders << m_restoreFloatings << tabContainers;
    return data;
#else
    return {};
#endif
}

/**
 * Analog to QMainWindow::restoreState(). The state must be created with FancyDocking::saveState()
 */
bool FancyDocking::restoreState(const QByteArray& state)
{
    if (state.isEmpty())
    {
        return false;
    }
#ifdef KDAB_MAC_PORT
    QByteArray stateData = state;
    QDataStream stream(&stateData, QIODevice::ReadOnly);

    quint32 version;
    SerializedMapType map;
    SerializedTabType tabContainers;
    QByteArray mainState;
    stream >> version;
    if (stream.status() != QDataStream::Ok || version != VersionMarker)
    {
        return false;
    }
    stream >> mainState >> map;
    if (stream.status() != QDataStream::Ok)
    {
        return false;
    }

    stream >> m_placeholders >> m_restoreFloatings >> tabContainers;

    // First, delete all the current floating window
    for (QDockWidget* dockWidget : m_mainWindow->findChildren<QDockWidget*>(
            QRegularExpression(QString("%1.*").arg(g_floatingWindowPrefix)), Qt::FindChildrenRecursively))
    {
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->widget());
        if (!mainWindow)
        {
            continue;
        }
        for (QDockWidget* subDockWidget : mainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
        {
            subDockWidget->setParent(m_mainWindow);
            if (!m_mainWindow->restoreDockWidget(subDockWidget))
            {
                m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, subDockWidget);
            }
        }
        delete dockWidget;
    }

    // Restore the floating windows
    QMap<QMainWindow*, QByteArray> floatingMainWindows;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        QString floatingDockName = it.key();
        QStringList childDockNames = it->first;
        QByteArray floatingState = it->second;

        // Don't restore any floating windows that have no cached dock widgets
        if (childDockNames.size() == 0)
        {
            continue;
        }

        QMainWindow* mainWindow = createFloatingMainWindow(floatingDockName, QRect());
        for (const QString &childName : childDockNames)
        {
            QDockWidget* child = m_mainWindow->findChild<QDockWidget*>(childName, Qt::FindDirectChildrenOnly);
            if (!child)
            {
                continue;
            }
            child->setParent(mainWindow);
            mainWindow->addDockWidget(Qt::LeftDockWidgetArea, child);
        }

        // Store the floating main window with its state so we can restore them
        // after the tab containers have been restored
        floatingMainWindows[mainWindow] = floatingState;
    }

    // Restore our tab containers (need to set our tabifyInProgress flag here
    // as well or floating windows that contain tab containers will get
    // deleted inadvertently)
    QScopedValueRollback<bool> rollback(m_state.tabifyInProgress, true);
    for (auto it = tabContainers.begin(); it != tabContainers.end(); ++it)
    {
        QString tabContainerName = it.key();
        TabContainerType tabState = it.value();
        QString floatingDockName = tabState.floatingDockName;
        QStringList tabNames = tabState.tabNames;
        int currentIndex = tabState.currentIndex;

        // If the floatingDockName is empty, then this tab container is meant
        // for our main window
        QMainWindow* mainWindow = nullptr;
        if (floatingDockName.isEmpty())
        {
            mainWindow = m_mainWindow;
        }
        // Otherwise, we need to find the floating dock widget that was
        // restored previously so we can get a reference to its main window
        else
        {
            QDockWidget* floatingDockWidget = m_mainWindow->findChild<QDockWidget*>(floatingDockName, Qt::FindDirectChildrenOnly);
            if (!floatingDockWidget)
            {
                continue;
            }

            mainWindow = qobject_cast<QMainWindow*>(floatingDockWidget->widget());
            if (!mainWindow)
            {
                continue;
            }
        }

        // Create a new tab container and tab widget with the same name as the cached tab container
        // so it will be restored in the same spot in the appropriate main window layout
        AzQtComponents::DockTabWidget* tabWidget = createTabWidget(mainWindow, nullptr, tabContainerName);

        // Move the dock widgets for the specified tabs into our tab widget
        for (QString name : tabNames)
        {
            // The dock widgets will be restored with the same name in the main window, they just won't
            // be in the proper layout since we have our own custom tab system
            QDockWidget* dockWidget = m_mainWindow->findChild<QDockWidget*>(name);
            if (!dockWidget)
            {
                continue;
            }

            // Move the dock widget into our tab widget
            tabWidget->addTab(dockWidget);
        }

        // Restore the cached active tab index
        tabWidget->setCurrentIndex(currentIndex);
    }

    // Restore the state of our floating main windows after the tab containers have
    // been restored, so that their place in the floating main window layouts will
    // be restored properly.  Also keep track if any of our main window restore
    // calls fail so we can report back our status.
    bool ok = true;
    for (auto it = floatingMainWindows.begin(); it != floatingMainWindows.end(); ++it)
    {
        QMainWindow* mainWindow = it.key();
        QByteArray floatingState = it.value();
        if (!mainWindow->restoreState(floatingState))
        {
            ok = false;
        }
    }

    // Restore the main layout
    if (!m_mainWindow->restoreState(mainState))
    {
        ok = false;
    }

    return ok;
#else
    return true;
#endif
}

/**
 * Same as QMainWindow::restoreDockWidget, but extended to checking if it was
 * last in one of our custom tab widgets or floating windows
 */
bool FancyDocking::restoreDockWidget(QDockWidget* dock)
{
    if (!dock)
    {
        return false;
    }

    // First, check if this dock widget was last in a tab container
    QString dockObjectName = dock->objectName();
    if (m_lastTabContainerForDockWidget.contains(dockObjectName))
    {
        QString tabDockWidgetName = m_lastTabContainerForDockWidget[dockObjectName];
        QDockWidget* dockWidget = m_mainWindow->findChild<QDockWidget*>(tabDockWidgetName);
        if (dockWidget)
        {
            AzQtComponents::DockTabWidget* tabWidget = qobject_cast<AzQtComponents::DockTabWidget*>(dockWidget->widget());
            if (tabWidget)
            {
                tabWidget->addTab(dock);
                return true;
            }
        }
    }

    // Then, check if it was last in a floating window
    auto it = m_placeholders.find(dockObjectName);
    if (it != m_placeholders.end())
    {
        // The DockWidget we try to restore was last seen in a floating QMainWindow.
        QString floatingDockWidgetName = *it;
        QDockWidget* dockWidget = m_mainWindow->findChild<QDockWidget*>(floatingDockWidgetName);
        if (dockWidget)
        {
            // That floating QMainWindow still exist.
            QMainWindow* mainWindow = qobject_cast<QMainWindow*>(dockWidget->widget());
            if (mainWindow)
            {
                dock->setParent(mainWindow);
                return mainWindow->restoreDockWidget(dock);
            }
        }
        else
        {
            // It no longer exists, so we need to re-create the floating main
            // window before restoring the dock widget
            auto it2 = m_restoreFloatings.find(floatingDockWidgetName);
            if (it2 != m_restoreFloatings.end())
            {
                QMainWindow* mainWindow = createFloatingMainWindow(floatingDockWidgetName, it2->second);
                mainWindow->restoreState(it2->first);
                dock->setParent(mainWindow);
                m_restoreFloatings.erase(it2);
                return mainWindow->restoreDockWidget(dock);
            }
        }
        m_placeholders.erase(it);
    }

    // Fallback to letting our main window try to restore it
    return m_mainWindow->restoreDockWidget(dock);
}

/**
 * Clear our dragging state and remove the any drop zones that have been setup
 */
void FancyDocking::clearDraggingState()
{
    m_ghostWidget->hide();

    // Release the mouse and keyboard from our main window since we grab them when we start dragging
    m_mainWindow->releaseMouse();
    m_mainWindow->releaseKeyboard();

    // Restore the dragged widget to its dock widget, and reparent our empty
    // placeholder widget to ourselves so that it will get cleaned up properly
    // We do this outside of the check for m_state.dock since there is a case
    // where the m_state.dock could no longer exist if you had ripped out a
    // single tab which would result in the tab container being destroyed
    if (m_state.draggedDockWidget)
    {
        m_state.draggedDockWidget->setWidget(m_state.draggedWidget);
        m_state.draggedDockWidget = nullptr;
        m_state.draggedWidget = nullptr;
        m_emptyWidget->hide();
        m_emptyWidget->setParent(this);
    }

    // If we hid the floating container of the dragged widget because it was
    // the only visible one, then we need to show it again
    if (m_state.dock)
    {
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(m_state.dock->parentWidget());
        if (mainWindow && mainWindow != m_mainWindow)
        {
            QDockWidget* containerDockWidget = qobject_cast<QDockWidget*>(mainWindow->parentWidget());
            if (containerDockWidget && containerDockWidget->isFloating() && !containerDockWidget->isVisible())
            {
                containerDockWidget->show();
            }
        }
    }

    // If we were dragging from a tab widget, make sure to reset its drag state
    if (m_state.tabWidget)
    {
        m_state.tabWidget->finishDrag();
    }

    m_state.dock = nullptr;
    m_dropZoneState.dragging = false;
    m_state.tabWidget = nullptr;
    m_state.tabIndex = -1;
    m_state.setPlaceholder(QRect(), nullptr);
    m_state.floatingDockContainer = nullptr;

    StopDropZone();
    setupDropZones(nullptr);

    m_ghostWidget->Disable();
}

#include <fancydocking.moc>
