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
#include "stdafx.h"

#include "EditorCommon.h"

#include "EditorDefs.h"
#include "Settings.h"
#include <IInput.h>
#include <AzCore/std/containers/map.h>

#define UICANVASEDITOR_SETTINGS_VIEWPORTWIDGET_DRAW_ELEMENT_BORDERS_KEY         "ViewportWidget::m_drawElementBordersFlags"
#define UICANVASEDITOR_SETTINGS_VIEWPORTWIDGET_DRAW_ELEMENT_BORDERS_DEFAULT     ( ViewportWidget::DrawElementBorders_Unselected )

namespace
{
    // Persistence.
    uint32 GetDrawElementBordersFlags()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, AZ_QCOREAPPLICATION_SETTINGS_ORGANIZATION_NAME);

        settings.beginGroup(UICANVASEDITOR_NAME_SHORT);

        uint32 result = settings.value(UICANVASEDITOR_SETTINGS_VIEWPORTWIDGET_DRAW_ELEMENT_BORDERS_KEY,
                UICANVASEDITOR_SETTINGS_VIEWPORTWIDGET_DRAW_ELEMENT_BORDERS_DEFAULT).toInt();

        settings.endGroup();

        return result;
    }

    // Persistence.
    void SetDrawElementBordersFlags(uint32 flags)
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, AZ_QCOREAPPLICATION_SETTINGS_ORGANIZATION_NAME);

        settings.beginGroup(UICANVASEDITOR_NAME_SHORT);

        settings.setValue(UICANVASEDITOR_SETTINGS_VIEWPORTWIDGET_DRAW_ELEMENT_BORDERS_KEY,
            flags);

        settings.endGroup();
    }

    // Map Qt event key codes to the game input system keyboard codes
    EKeyId MapQtKeyToGameInputKey(int qtKey)
    {
        // The UI runtime only cares about a few special keys
        static AZStd::map<Qt::Key, EKeyId> qtKeyToEKeyIdMap
        {
            {
                Qt::Key_Tab, eKI_Tab
            },
            {
                Qt::Key_Backspace, eKI_Backspace
            },
            {
                Qt::Key_Return, eKI_Enter
            },
            {
                Qt::Key_Enter, eKI_Enter
            },
            {
                Qt::Key_Delete, eKI_Delete
            },
            {
                Qt::Key_Left, eKI_Left
            },
            {
                Qt::Key_Up, eKI_Up
            },
            {
                Qt::Key_Right, eKI_Right
            },
            {
                Qt::Key_Down, eKI_Down
            },
            {
                Qt::Key_Home, eKI_Home
            },
            {
                Qt::Key_End, eKI_End
            },
        };

        // if no match we will return unknown
        EKeyId result = eKI_Unknown;

        auto iter = qtKeyToEKeyIdMap.find(static_cast<Qt::Key>(qtKey));
        if (iter != qtKeyToEKeyIdMap.end())
        {
            result = iter->second;
        }
        return result;
    }

    // Map Qt event modifiers to the game input system modifiers
    int MapQtModifiersToGameInputModifiers(Qt::KeyboardModifiers qtMods)
    {
        int gameModifiers = 0;

        if (qtMods & Qt::ShiftModifier)
        {
            gameModifiers |= eMM_Shift;
        }

        if (qtMods & Qt::ControlModifier)
        {
            gameModifiers |= eMM_Ctrl;
        }

        if (qtMods & Qt::AltModifier)
        {
            gameModifiers |= eMM_Alt;
        }

        return gameModifiers;
    }

    bool HandleCanvasInputEvent(AZ::EntityId canvasEntityId, const SInputEvent& gameEvent)
    {
        bool handled = false;
        EBUS_EVENT_ID_RESULT(handled, canvasEntityId, UiCanvasBus, HandleInputEvent, gameEvent);

        // Execute events that have been queued during the input event handler
        gEnv->pLyShine->ExecuteQueuedEvents();

        return handled;
    }

} // anonymous namespace.

ViewportWidget::ViewportWidget(EditorWindow* parent)
    : QViewport(parent)
    , m_editorWindow(parent)
    , m_viewportInteraction(new ViewportInteraction(m_editorWindow))
    , m_viewportAnchor(new ViewportAnchor())
    , m_viewportHighlight(new ViewportHighlight())
    , m_viewportBackground(new ViewportCanvasBackground())
    , m_viewportPivot(new ViewportPivot())
    , m_drawElementBordersFlags(GetDrawElementBordersFlags())
    , m_refreshRequested(true)
    , m_canvasRenderIsDisabled(false)
    , m_updateTimer(this)
    , m_previewCanvasScale(1.0f)
{
    QObject::connect(this,
        SIGNAL(SignalRender(const SRenderContext&)),
        SLOT(HandleSignalRender(const SRenderContext&)));

    // Turn off all fancy visuals in the viewport.
    {
        SViewportSettings tweakedSettings = GetSettings();

        tweakedSettings.grid.showGrid = false;
        tweakedSettings.grid.origin = false;
        tweakedSettings.rendering.fps = false;
        tweakedSettings.rendering.wireframe = false;
        tweakedSettings.lighting.m_brightness = 0.0f;
        tweakedSettings.camera.showViewportOrientation = false;

        SetSettings(tweakedSettings);
    }

    UpdateViewportBackground();

    // Setup a timer for the maximum refresh rate we want.
    // Refresh is actually triggered by interaction events and by the IdleUpdate. This avoids the UI
    // Editor slowing down the main editor when no UI interaction is occurring.
    QObject::connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(RefreshTick()));
    const int kUpdateIntervalInMillseconds = 1000 / 60;   // 60 Hz
    m_updateTimer.start(kUpdateIntervalInMillseconds);
}

ViewportInteraction* ViewportWidget::GetViewportInteraction()
{
    return m_viewportInteraction.get();
}

bool ViewportWidget::IsDrawingElementBorders(uint32 flags) const
{
    return (m_drawElementBordersFlags & flags) ? true : false;
}

void ViewportWidget::ToggleDrawElementBorders(uint32 flags)
{
    m_drawElementBordersFlags ^= flags;

    // Persistence.
    SetDrawElementBordersFlags(m_drawElementBordersFlags);
}

void ViewportWidget::UpdateViewportBackground()
{
    SViewportSettings tweakedSettings = GetSettings();
    ColorB backgroundColor(ViewportHelpers::backgroundColorDark.GetR8(),
        ViewportHelpers::backgroundColorDark.GetG8(),
        ViewportHelpers::backgroundColorDark.GetB8(),
        ViewportHelpers::backgroundColorDark.GetA8());
    tweakedSettings.background.useGradient = false;
    tweakedSettings.background.topColor = backgroundColor;
    tweakedSettings.background.bottomColor = backgroundColor;
    tweakedSettings.lighting.m_ambientColor = backgroundColor;

    SetSettings(tweakedSettings);
}

void ViewportWidget::Refresh()
{
    m_refreshRequested = true;
}

void ViewportWidget::ClearUntilSafeToRedraw()
{
    // set flag so that Update will just clear the screen rather than rendering canvas
    m_canvasRenderIsDisabled = true;

    // Force an update
    Refresh();
    RefreshTick();

    // Schedule a timer to clear the m_canvasRenderIsDisabled flag
    // using a time of zero just waits until there is nothing on the event queue
    QTimer::singleShot(0, this, SLOT(EnableCanvasRender()));
}

void ViewportWidget::contextMenuEvent(QContextMenuEvent* e)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        // The context menu.
        const QPoint pos = e->pos();
        HierarchyMenu contextMenu(m_editorWindow->GetHierarchy(),
            HierarchyMenu::Show::kCutCopyPaste |
            HierarchyMenu::Show::kSavePrefab |
            HierarchyMenu::Show::kNew_EmptyElement |
            HierarchyMenu::Show::kNew_ElementFromPrefabs |
            HierarchyMenu::Show::kDeleteElement |
            HierarchyMenu::Show::kNewSlice |
            HierarchyMenu::Show::kNew_InstantiateSlice |
            HierarchyMenu::Show::kPushToSlice,
            true,
            nullptr,
            &pos);

        contextMenu.exec(e->globalPos());
    }

    QViewport::contextMenuEvent(e);
}

void ViewportWidget::HandleSignalRender(const SRenderContext& context)
{
    if (!m_canvasRenderIsDisabled)
    {
        gEnv->pRenderer->SetSrgbWrite(true);

        UiEditorMode editorMode = m_editorWindow->GetEditorMode();

        if (editorMode == UiEditorMode::Edit)
        {
            RenderEditMode();
        }
        else // if (editorMode == UiEditorMode::Preview)
        {
            RenderPreviewMode();
        }
    }
}

void ViewportWidget::UserSelectionChanged(HierarchyItemRawPtrList* items)
{
    Refresh();

    if (!items)
    {
        m_viewportInteraction->ClearInteraction();
    }
}

void ViewportWidget::EnableCanvasRender()
{
    m_canvasRenderIsDisabled = false;

    // force a redraw
    Refresh();
    RefreshTick();
}

void ViewportWidget::RefreshTick()
{
    if (m_refreshRequested)
    {
        Update();
        m_refreshRequested = false;

        // in case we were called manually, reset the timer
        m_updateTimer.start();
    }
}

void ViewportWidget::mousePressEvent(QMouseEvent* ev)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        // in Edit mode just send input to ViewportInteraction
        m_viewportInteraction->MousePressEvent(ev);
    }
    else // if (editorMode == UiEditorMode::Preview)
    {
        // In Preview mode convert the event into a game input event and send to canvas
        AZ::EntityId canvasEntityId = m_editorWindow->GetPreviewModeCanvas();
        if (canvasEntityId.IsValid())
        {
            if (ev->button() == Qt::LeftButton)
            {
                // Send event to this canvas
                SInputEvent gameEvent;
                gameEvent.deviceType = eIDT_Mouse;
                gameEvent.screenPosition = Vec2(ev->x(), ev->y());
                gameEvent.keyId = eKI_Mouse1;
                gameEvent.state = eIS_Pressed;
                HandleCanvasInputEvent(canvasEntityId, gameEvent);
            }
        }
    }

    // Note: do not propagate this event to parent QViewport, otherwise
    // it will manipulate the mouse position in unexpected ways.

    Refresh();
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* ev)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();

    if (editorMode == UiEditorMode::Edit)
    {
        // in Edit mode just send input to ViewportInteraction
        m_viewportInteraction->MouseMoveEvent(ev,
            m_editorWindow->GetHierarchy()->selectedItems());
    }
    else // if (editorMode == UiEditorMode::Preview)
    {
        // In Preview mode convert the event into a game input event and send to canvas
        AZ::EntityId canvasEntityId = m_editorWindow->GetPreviewModeCanvas();
        if (canvasEntityId.IsValid())
        {
            // Send event to this canvas
            SInputEvent gameEvent;
            gameEvent.deviceType = eIDT_Mouse;
            gameEvent.screenPosition = Vec2(ev->x(), ev->y());
            gameEvent.keyId = eKI_Mouse1;
            if (ev->buttons() & Qt::LeftButton)
            {
                gameEvent.state = eIS_Down;
            }
            HandleCanvasInputEvent(canvasEntityId, gameEvent);
        }
    }

    // Note: do not propagate this event to parent QViewport, otherwise
    // it will manipulate the mouse position in unexpected ways.

    Refresh();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        // in Edit mode just send input to ViewportInteraction
        m_viewportInteraction->MouseReleaseEvent(ev,
            m_editorWindow->GetHierarchy()->selectedItems());
    }
    else // if (editorMode == UiEditorMode::Preview)
    {
        // In Preview mode convert the event into a game input event and send to canvas
        AZ::EntityId canvasEntityId = m_editorWindow->GetPreviewModeCanvas();
        if (canvasEntityId.IsValid())
        {
            if (ev->button() == Qt::LeftButton)
            {
                // Send event to this canvas
                SInputEvent gameEvent;
                gameEvent.deviceType = eIDT_Mouse;
                gameEvent.screenPosition = Vec2(ev->x(), ev->y());
                gameEvent.keyId = eKI_Mouse1;
                gameEvent.state = eIS_Released;
                HandleCanvasInputEvent(canvasEntityId, gameEvent);
            }
        }
    }

    // Note: do not propagate this event to parent QViewport, otherwise
    // it will manipulate the mouse position in unexpected ways.

    Refresh();
}

void ViewportWidget::wheelEvent(QWheelEvent* ev)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        // in Edit mode just send input to ViewportInteraction
        m_viewportInteraction->MouseWheelEvent(ev);
    }

    QViewport::wheelEvent(ev);

    Refresh();
}

bool ViewportWidget::event(QEvent* ev)
{
    bool result = QWidget::event(ev);

    if (ev->type() == QEvent::ShortcutOverride)
    {
        // When a shortcut is matched, Qt's event processing sends out a shortcut override event
        // to allow other systems to override it. If it's not overridden, then the key events
        // get processed as a shortcut, even if the widget that's the target has a keyPress event
        // handler. In our case this causes a problem in preview mode for the Key_Delete event.
        // So, if we are preview mode avoid treating Key_Delete as a shortcut.

        UiEditorMode editorMode = m_editorWindow->GetEditorMode();
        if (editorMode == UiEditorMode::Preview)
        {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            if (keyEvent->key() == Qt::Key_Delete)
            {
                ev->accept();
                return true;
            }
        }
    }

    return result;
}

void ViewportWidget::keyPressEvent(QKeyEvent* event)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        // in Edit mode just send input to ViewportInteraction
        m_viewportInteraction->KeyPressEvent(event);
    }
    else // if (editorMode == UiEditorMode::Preview)
    {
        // In Preview mode convert the event into a game input event and send to canvas
        // unless it is the escape key which quits preview mode
        if (event->key() == Qt::Key_Escape)
        {
            m_editorWindow->ToggleEditorMode();
            return;
        }

        AZ::EntityId canvasEntityId = m_editorWindow->GetPreviewModeCanvas();
        if (canvasEntityId.IsValid())
        {
            // Send event to this canvas
            SInputEvent gameEvent;
            gameEvent.keyId = MapQtKeyToGameInputKey(event->key());
            gameEvent.modifiers = MapQtModifiersToGameInputModifiers(event->modifiers());
            if (gameEvent.keyId != eKI_Unknown)
            {
                gameEvent.deviceType = eIDT_Keyboard;
                gameEvent.screenPosition = Vec2(0.0f, 0.0f);
                gameEvent.state = eIS_Pressed;
                HandleCanvasInputEvent(canvasEntityId, gameEvent);
            }
        }
    }
}

void ViewportWidget::focusOutEvent(QFocusEvent* ev)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        m_viewportInteraction->ClearInteraction();
    }
}

void ViewportWidget::keyReleaseEvent(QKeyEvent* event)
{
    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        // in Edit mode just send input to ViewportInteraction
        m_viewportInteraction->KeyReleaseEvent(event);
    }
    else if (editorMode == UiEditorMode::Preview)
    {
        AZ::EntityId canvasEntityId = m_editorWindow->GetPreviewModeCanvas();
        if (canvasEntityId.IsValid())
        {
            bool handled = false;

            // Send event to this canvas
            SInputEvent gameEvent;
            gameEvent.keyId = MapQtKeyToGameInputKey(event->key());
            gameEvent.modifiers = MapQtModifiersToGameInputModifiers(event->modifiers());
            if (gameEvent.keyId != eKI_Unknown)
            {
                gameEvent.deviceType = eIDT_Keyboard;
                gameEvent.screenPosition = Vec2(0.0f, 0.0f);
                gameEvent.state = eIS_Released;
                handled = HandleCanvasInputEvent(canvasEntityId, gameEvent);
            }

            QString string = event->text();
            if (string.length() != 0 && !handled)
            {
                wchar_t inputChars[10];
                int wcharCount = string.toWCharArray(inputChars);
                if (wcharCount > 0)
                {
                    wchar_t inputChar = inputChars[0];
                    gameEvent.inputChar = inputChar;
                    gameEvent.state = eIS_UI;
                    HandleCanvasInputEvent(canvasEntityId, gameEvent);
                }
            }
        }
    }
    else
    {
        AZ_Assert(0, "Invalid editorMode: %d", editorMode);
    }
}

void ViewportWidget::resizeEvent(QResizeEvent* ev)
{
    m_editorWindow->GetPreviewToolbar()->ViewportHasResized(ev);

    QViewport::resizeEvent(ev);

    QMetaObject::invokeMethod(this, "RenderInternal", Qt::QueuedConnection);

    UiEditorMode editorMode = m_editorWindow->GetEditorMode();
    if (editorMode == UiEditorMode::Edit)
    {
        if (m_viewportInteraction->ShouldScaleToFitOnViewportResize())
        {
            m_viewportInteraction->CenterCanvasInViewport();
            Refresh();
            RefreshTick();
        }
    }
    else // if (editorMode == UiEditorMode::Preview)
    {
        // force a redraw immediately to get as close to smooth canvas redraw as possible
        Refresh();
        RefreshTick();
    }
}

void ViewportWidget::RenderEditMode()
{
    AZ::EntityId canvasEntityId = m_editorWindow->GetCanvas();
    if (!canvasEntityId.IsValid())
    {
        return; // this can happen if a render happens during a restart
    }

    Draw2dHelper draw2d;    // sets and resets 2D draw mode in constructor/destructor

    QTreeWidgetItemRawPtrQList selection = m_editorWindow->GetHierarchy()->selectedItems();

    AZ::Vector2 canvasSize;
    EBUS_EVENT_ID_RESULT(canvasSize, canvasEntityId, UiCanvasBus, GetCanvasSize);

    m_viewportBackground->Draw(draw2d,
        canvasSize,
        m_viewportInteraction->GetCanvasToViewportScale(),
        m_viewportInteraction->GetCanvasToViewportTranslation());

    AZ::Vector2 viewportSize(size().width(), size().height());

    // clear the stencil buffer before rendering each canvas - required for masking
    // NOTE: the FRT_CLEAR_IMMEDIATE is required since we will not be setting the render target
    ColorF viewportBackgroundColor(0, 0, 0, 0); // if clearing color we want to set alpha to zero also
    gEnv->pRenderer->ClearTargetsImmediately(FRT_CLEAR_STENCIL, viewportBackgroundColor);

    // Set the target size of the canvas
    EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, SetTargetCanvasSize, false, canvasSize);

    // Update this canvas (must be done after SetTargetCanvasSize)
    EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, UpdateCanvas, GetLastFrameTime(), false);

    // Render this canvas
    EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, RenderCanvas, false, viewportSize, false);

    // Draw borders around selected and unselected UI elements in the viewport
    // depending on the flags in m_drawElementBordersFlags
    HierarchyItemRawPtrList selectedItems = SelectionHelpers::GetSelectedHierarchyItems(m_editorWindow->GetHierarchy(), selection);
    m_viewportHighlight->Draw(draw2d,
        m_editorWindow->GetHierarchy()->invisibleRootItem(),
        selectedItems,
        m_drawElementBordersFlags);

    // Draw primary gizmos
    m_viewportInteraction->Draw(draw2d, selection);

    // Draw secondary gizmos
    if (ViewportInteraction::InteractionMode::ROTATE == m_viewportInteraction->GetMode())
    {
        // Draw the pivots and degrees only in Rotate mode
        LyShine::EntityArray selectedElements = SelectionHelpers::GetTopLevelSelectedElements(m_editorWindow->GetHierarchy(), selection);
        for (auto element : selectedElements)
        {
            bool isHighlighted = (m_viewportInteraction->GetActiveElement() == element) &&
                (m_viewportInteraction->GetInteractionType() == ViewportInteraction::InteractionType::PIVOT);
            m_viewportPivot->Draw(draw2d, element, isHighlighted);

            ViewportHelpers::DrawRotationValue(element, m_viewportInteraction.get(), m_viewportPivot.get(), draw2d);
        }
    }
    else if (selectedItems.size() == 1 &&
             ViewportInteraction::InteractionMode::MOVE == m_viewportInteraction->GetMode())
    {
        // Draw the anchors only if there is exactly one selected element and we're in Move mode
        auto selectedElement = selectedItems.front()->GetElement();
        bool leftButtonIsActive = m_viewportInteraction->GetLeftButtonIsActive();
        bool spaceBarIsActive = m_viewportInteraction->GetSpaceBarIsActive();
        ViewportHelpers::SelectedAnchors highlightedAnchors = m_viewportInteraction->GetGrabbedAnchors();
        m_viewportAnchor->Draw(draw2d,
            selectedElement,
            leftButtonIsActive,
            leftButtonIsActive && !spaceBarIsActive,
            highlightedAnchors);
    }
}

void ViewportWidget::RenderPreviewMode()
{
    AZ::EntityId canvasEntityId = m_editorWindow->GetPreviewModeCanvas();

    // Rather than scaling to exactly fit we try to draw at one of these preset scale factors
    // to make it it bit more obvious that the canvas size is changing
    float zoomScales[] = {
        1.00f,
        0.75f,
        0.50f,
        0.25f,
        0.10f,
        0.05f
    };

    if (canvasEntityId.IsValid())
    {
        // Get the canvas size
        AZ::Vector2 viewportSize(size().width(), size().height());
        AZ::Vector2 canvasSize = m_editorWindow->GetPreviewCanvasSize();
        if (canvasSize.GetX() == 0.0f && canvasSize.GetY() == 0.0f)
        {
            // special value of (0,0) means use the viewport size
            canvasSize = viewportSize;
        }

        // work out what scale to use for the canvasToViewport matrix
        float scale = 1.0f;
        if (canvasSize.GetX() > viewportSize.GetX())
        {
            if (canvasSize.GetX() >= 1.0f)   // avoid divide by zero
            {
                scale = viewportSize.GetX() / canvasSize.GetX();
            }
        }
        if (canvasSize.GetY() > viewportSize.GetY())
        {
            if (canvasSize.GetY() >= 1.0f)   // avoid divide by zero
            {
                float scaleY = viewportSize.GetY() / canvasSize.GetY();
                if (scaleY < scale)
                {
                    scale = scaleY;
                }
            }
        }

        // Set the target size of the canvas
        EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, SetTargetCanvasSize, true, canvasSize);

        // Update this canvas (must be done after SetTargetCanvasSize)
        EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, UpdateCanvas, GetLastFrameTime(), true);

        // Execute events that have been queued during the canvas update
        gEnv->pLyShine->ExecuteQueuedEvents();

        // match scale to one of the predefined scales. If the scale is so small
        // that it is less than the smallest scale then leave it as it is
        for (int i = 0; i < AZ_ARRAY_SIZE(zoomScales); ++i)
        {
            if (scale >= zoomScales[i])
            {
                scale = zoomScales[i];
                break;
            }
        }

        // Update the toolbar to show the current scale
        if (scale != m_previewCanvasScale)
        {
            m_previewCanvasScale = scale;
            m_editorWindow->GetPreviewToolbar()->UpdatePreviewCanvasScale(scale);
        }

        // Set up the canvasToViewportMatrix
        AZ::Vector3 scale3(scale, scale, 1.0f);
        AZ::Vector3 translation((viewportSize.GetX() - (canvasSize.GetX() * scale)) * 0.5f,
            (viewportSize.GetY() - (canvasSize.GetY() * scale)) * 0.5f, 0.0f);
        AZ::Matrix4x4 canvasToViewportMatrix = AZ::Matrix4x4::CreateScale(scale3);
        canvasToViewportMatrix.SetTranslation(translation);
        EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, SetCanvasToViewportMatrix, canvasToViewportMatrix);

        // clear the stencil buffer before rendering each canvas - required for masking
        // NOTE: the FRT_CLEAR_IMMEDIATE is required since we will not be setting the render target
        ColorF viewportBackgroundColor(0, 0, 0, 0); // if clearing color we want to set alpha to zero also
        gEnv->pRenderer->ClearTargetsImmediately(FRT_CLEAR_STENCIL, viewportBackgroundColor);

        // Render this canvas
        // NOTE: the displayBounds param is always false. If we wanted a debug option to display the bounds
        // in preview mode we would need to render the deferred primitives after this call so that they
        // show up in the correct viewport
        EBUS_EVENT_ID(canvasEntityId, UiCanvasBus, RenderCanvas, true, viewportSize, false);
    }
}

#include <ViewportWidget.moc>
