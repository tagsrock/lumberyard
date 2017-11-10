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
#if defined(AZ_PLATFORM_WINDOWS)
#include <InitGuid.h>
#endif
#include "ObjectMode.h"
#include "Viewport.h"
#include "ViewManager.h"
#include "./Terrain/Heightmap.h"
#include "GameEngine.h"
#include "Objects/EntityObject.h"
#include "Objects/CameraObject.h"
#include "AnimationContext.h"
#include "Objects/BrushObject.h"
#include "DeepSelection.h"
#include "SubObjectSelectionReferenceFrameCalculator.h"
#include "ITransformManipulator.h"
#include "DisplaySettings.h"
#include "Objects/AIPoint.h"
#include "Objects/ParticleEffectObject.h"
#include "Objects/PrefabObject.h"

#include <AzCore/Math/Vector2.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QMenu>
#include <QCursor>
#include <AzToolsFramework/ToolsComponents/EditorSelectionAccentingBus.h>

//////////////////////////////////////////////////////////////////////////
CObjectMode::CObjectMode(QObject* parent)
    : CEditTool(parent)
{
    m_pClassDesc = GetIEditor()->GetClassFactory()->FindClass(OBJECT_MODE_GUID);
    SetStatusText(tr("Object Selection"));

    m_openContext = false;
    m_commandMode = NothingMode;
    m_MouseOverObject = GuidUtil::NullGuid;

    m_pDeepSelection = new CDeepSelection();
    m_bMoveByFaceNormManipShown = false;
    m_pHitObject = NULL;

    m_bTransformChanged = false;
}

//////////////////////////////////////////////////////////////////////////
CObjectMode::~CObjectMode()
{
}

void CObjectMode::DrawSelectionPreview(struct DisplayContext& dc, CBaseObject* drawObject)
{
    int childColVal = 0;

    AABB bbox;
    drawObject->GetBoundBox(bbox);

    // If CGroup/CPrefabObject
    if (drawObject->GetChildCount() > 0)
    {
        // Draw object name label on top of object
        Vec3 vTopEdgeCenterPos = bbox.GetCenter();

        dc.SetColor(gSettings.objectColorSettings.groupHighlight);
        vTopEdgeCenterPos(vTopEdgeCenterPos.x, vTopEdgeCenterPos.y, bbox.max.z);
        dc.DrawTextLabel(vTopEdgeCenterPos, 1.3f, drawObject->GetName().toLatin1().data());
        // Draw bounding box wireframe
        dc.DrawWireBox(bbox.min, bbox.max);
    }
    else
    {
        dc.SetColor(Vec3(1, 1, 1));
        dc.DrawTextLabel(bbox.GetCenter(), 1.5, drawObject->GetName().toLatin1().data());
    }

    // Object Geometry Highlight
    
    const float normalizedFloatToUint8 = 255.0f;
    
    // Default, CBrush object
    ColorB selColor = ColorB(gSettings.objectColorSettings.geometryHighlightColor.red(), gSettings.objectColorSettings.geometryHighlightColor.green(), gSettings.objectColorSettings.geometryHighlightColor.blue(), gSettings.objectColorSettings.fGeomAlpha * normalizedFloatToUint8);

    // CDesignerBrushObject
    if (drawObject->GetType() == OBJTYPE_SOLID)
    {
        selColor = ColorB(gSettings.objectColorSettings.solidBrushGeometryColor.red(), gSettings.objectColorSettings.solidBrushGeometryColor.green(), gSettings.objectColorSettings.solidBrushGeometryColor.blue(), gSettings.objectColorSettings.fGeomAlpha * normalizedFloatToUint8);
    }

    // In case it is a child object, use a different alpha value
    if (drawObject->GetParent())
    {
        selColor.a = (uint8)(gSettings.objectColorSettings.fChildGeomAlpha * normalizedFloatToUint8);
    }

    // Draw geometry in custom color
    SGeometryDebugDrawInfo dd;
    dd.tm = drawObject->GetWorldTM();
    dd.color = selColor;
    dd.lineColor = selColor;
    dd.bExtrude = true;

    if (qobject_cast<CGroup*>(drawObject) || qobject_cast<CPrefabObject*>(drawObject))
    {
        CGroup* paintObj = (CGroup*)drawObject;

        dc.DepthTestOff();

        if (drawObject->metaObject() == &CPrefabObject::staticMetaObject)
        {
            dc.SetColor(gSettings.objectColorSettings.prefabHighlight, gSettings.objectColorSettings.fBBoxAlpha * normalizedFloatToUint8);
        }
        else
        {
            dc.SetColor(gSettings.objectColorSettings.groupHighlight, gSettings.objectColorSettings.fBBoxAlpha * normalizedFloatToUint8);
        }

        dc.DrawSolidBox(bbox.min, bbox.max);
        dc.DepthTestOn();
    }
    else if (qobject_cast<CBrushObject*>(drawObject))
    {
        if (!(dc.flags & DISPLAY_2D))
        {
            CBrushObject* paintObj = (CBrushObject*)drawObject;
            IStatObj* pStatObj = paintObj->GetIStatObj();
            if (pStatObj)
            {
                pStatObj->DebugDraw(dd);
            }
        }
    }
    else if (drawObject->GetType() == OBJTYPE_SOLID)
    {
        if (!(dc.flags & DISPLAY_2D))
        {
            IStatObj* pStatObj = drawObject->GetIStatObj();
            if (pStatObj)
            {
                pStatObj->DebugDraw(dd);
            }
        }
    }
    else if (qobject_cast<CEntityObject*>(drawObject))
    {
        dc.DepthTestOff();
        dc.SetColor(gSettings.objectColorSettings.entityHighlight, gSettings.objectColorSettings.fBBoxAlpha * normalizedFloatToUint8);
        dc.DrawSolidBox(bbox.min, bbox.max);
        dc.DepthTestOn();

        CEntityObject* entityObj = (CEntityObject*)drawObject;
        if (entityObj)
        {
            entityObj->DrawExtraLightInfo(dc);
        }
    }

    // Highlight also children objects if this object is opened
    if (drawObject->GetChildCount() > 0)
    {
        CGroup* group = (CGroup*)drawObject;
        if (!group->IsOpen())
        {
            return;
        }

        for (int gNo = 0; gNo < drawObject->GetChildCount(); ++gNo)
        {
            if (std::find(m_PreviewGUIDs.begin(), m_PreviewGUIDs.end(), drawObject->GetChild(gNo)->GetId()) == m_PreviewGUIDs.end())
            {
                DrawSelectionPreview(dc, drawObject->GetChild(gNo));
            }
        }
    }
}

void CObjectMode::DisplaySelectionPreview(struct DisplayContext& dc)
{
    CViewport* view = GetIEditor()->GetViewManager()->GetView(0);
    IObjectManager* objMan = GetIEditor()->GetObjectManager();

    if (!view)
    {
        return;
    }

    QRect rc = view->GetSelectionRectangle();

    if (GetCommandMode() == SelectMode)
    {
        if (rc.width() > 1 && rc.height() > 1)
        {
            GetIEditor()->GetObjectManager()->FindObjectsInRect(view, rc, m_PreviewGUIDs);

            QString selCountStr;

            // Do not include child objects in the count of object candidates
            int childNo = 0;
            for (int objNo = 0; objNo < m_PreviewGUIDs.size(); ++objNo)
            {
                if (objMan->FindObject(m_PreviewGUIDs[objNo]))
                {
                    if (objMan->FindObject(m_PreviewGUIDs[objNo])->GetParent())
                    {
                        ++childNo;
                    }
                }
            }

            selCountStr = QString::number(m_PreviewGUIDs.size() - childNo);
            GetIEditor()->SetStatusText(tr("Selection Candidates Count: %1").arg(selCountStr).toLatin1().data());

            // Draw Preview for objects
            for (size_t i = 0; i < m_PreviewGUIDs.size(); ++i)
            {
                CBaseObject* curObj = GetIEditor()->GetObjectManager()->FindObject(m_PreviewGUIDs[i]);

                if (!curObj)
                {
                    continue;
                }

                DrawSelectionPreview(dc, curObj);
            }
        }
    }
}

void CObjectMode::DisplayExtraLightInfo(struct DisplayContext& dc)
{
    if (m_MouseOverObject != GUID_NULL)
    {
        IObjectManager* objMan = GetIEditor()->GetObjectManager();

        if (objMan)
        {
            CBaseObject* hitObj = objMan->FindObject(m_MouseOverObject);

            if (hitObj)
            {
                if (objMan->IsLightClass(hitObj))
                {
                    CEntityObject* entityObj = (CEntityObject*)hitObj;
                    if (entityObj)
                    {
                        entityObj->DrawExtraLightInfo(dc);
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::EndEditParams()
{
    CBaseObject* pMouseOverObject = nullptr;
    if (!GuidUtil::IsEmpty(m_MouseOverObject))
    {
        pMouseOverObject = GetIEditor()->GetObjectManager()->FindObject(m_MouseOverObject);
    }

    if (pMouseOverObject)
    {
        pMouseOverObject->SetHighlight(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::Display(struct DisplayContext& dc)
{
    // Selection Candidates Preview
    DisplaySelectionPreview(dc);
    DisplayExtraLightInfo(dc);

    GetIEditor()->GetSelection()->IndicateSnappingVertex(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::MouseCallback(CViewport* view, EMouseEvent event, QPoint& point, int flags)
{
    switch (event)
    {
    case eMouseLDown:
        return OnLButtonDown(view, flags, point);
        break;
    case eMouseLUp:
        return OnLButtonUp(view, flags, point);
        break;
    case eMouseLDblClick:
        return OnLButtonDblClk(view, flags, point);
        break;
    case eMouseRDown:
        return OnRButtonDown(view, flags, point);
        break;
    case eMouseRUp:
        return OnRButtonUp(view, flags, point);
        break;
    case eMouseMove:
        return OnMouseMove(view, flags, point);
        break;
    case eMouseMDown:
        return OnMButtonDown(view, flags, point);
        break;
    case eMouseLeave:
        return OnMouseLeave(view);
        break;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
    if (nChar == VK_ESCAPE)
    {
        GetIEditor()->ClearSelection();
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonDown(CViewport* view, int nFlags, const QPoint& point)
{
    if (m_bMoveByFaceNormManipShown)
    {
        HideMoveByFaceNormGizmo();
    }

    // CPointF ptMarker;
    QPoint ptCoord;
    int iCurSel = -1;

    if (GetIEditor()->IsInGameMode())
    {
        // Ignore clicks while in game.
        return false;
    }

    // Allow interception of mouse clicks for custom behavior.
    bool handledExternally = false;
    EBUS_EVENT(AzToolsFramework::EditorRequests::Bus,
        HandleObjectModeSelection,
        AZ::Vector2(static_cast<float>(point.x()), static_cast<float>(point.y())),
        nFlags,
        handledExternally);
    if (handledExternally)
    {
        return true;
    }

    // Save the mouse down position
    m_cMouseDownPos = point;
    m_bDragThresholdExceeded = false;

    view->ResetSelectionRegion();

    Vec3 pos = view->SnapToGrid(view->ViewToWorld(point));

    // Swap X/Y
    int unitSize = 1;
    CHeightmap* pHeightmap = GetIEditor()->GetHeightmap();
    if (pHeightmap)
    {
        unitSize = pHeightmap->GetUnitSize();
    }
    float hx = pos.y / unitSize;
    float hy = pos.x / unitSize;
    float hz = GetIEditor()->GetTerrainElevation(pos.x, pos.y);

    char szNewStatusText[512];
    sprintf_s(szNewStatusText, "Heightmap Coordinates: HX:%g HY:%g HZ:%g", hx, hy, hz);
    GetIEditor()->SetStatusText(szNewStatusText);

    // Get control key status.
    const bool bAltClick = (Qt::AltModifier & QApplication::queryKeyboardModifiers());
    bool bCtrlClick = (nFlags & MK_CONTROL);
    bool bShiftClick = (nFlags & MK_SHIFT);

    bool bAddSelect = bCtrlClick;
    bool bUnselect = bAltClick;
    bool bNoRemoveSelection = bAddSelect || bUnselect;

    // Check deep selection mode activated
    // The Deep selection has two mode.
    // The normal mode pops the context menu, another is the cyclic selection on clinking.
    const bool bTabPressed = CheckVirtualKey(Qt::Key_Tab);
    const bool bZKeyPressed = CheckVirtualKey(Qt::Key_Z);

    CDeepSelection::EDeepSelectionMode dsMode =
        (bTabPressed ? (bZKeyPressed ? CDeepSelection::DSM_POP : CDeepSelection::DSM_CYCLE) : CDeepSelection::DSM_NONE);

    bool bLockSelection = GetIEditor()->IsSelectionLocked();

    int numUnselected = 0;
    int numSelected = 0;

    //  m_activeAxis = 0;

    HitContext hitInfo;
    hitInfo.view = view;
    if (bAddSelect || bUnselect)
    {
        // If adding or removing selection from the object, ignore hitting selection axis.
        hitInfo.bIgnoreAxis = true;
    }

    if (dsMode == CDeepSelection::DSM_POP)
    {
        m_pDeepSelection->Reset(true);
        m_pDeepSelection->SetMode(dsMode);
        hitInfo.pDeepSelection = m_pDeepSelection;
    }
    else if (dsMode == CDeepSelection::DSM_CYCLE)
    {
        if (!m_pDeepSelection->OnCycling(point))
        {
            // Start of the deep selection cycling mode.
            m_pDeepSelection->Reset(false);
            m_pDeepSelection->SetMode(dsMode);
            hitInfo.pDeepSelection = m_pDeepSelection;
        }
    }
    else
    {
        if (m_pDeepSelection->GetPreviousMode() == CDeepSelection::DSM_NONE)
        {
            m_pDeepSelection->Reset(true);
        }

        m_pDeepSelection->SetMode(CDeepSelection::DSM_NONE);
        hitInfo.pDeepSelection = 0;
    }

    if (view->HitTest(point, hitInfo))
    {
        if (hitInfo.axis != 0)
        {
            GetIEditor()->SetAxisConstraints((AxisConstrains)hitInfo.axis);
            bLockSelection = true;
        }
        if (hitInfo.axis != 0)
        {
            view->SetAxisConstrain(hitInfo.axis);
        }


        //////////////////////////////////////////////////////////////////////////
        // Deep Selection
        CheckDeepSelection(hitInfo, view);
    }

    CBaseObject* hitObj = hitInfo.object;

    int editMode = GetIEditor()->GetEditMode();

    Matrix34 userTM = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix();

    if (hitObj)
    {
        Matrix34 tm = hitInfo.object->GetWorldTM();
        tm.OrthonormalizeFast();
        view->SetConstructionMatrix(COORDS_LOCAL, tm);
        if (hitInfo.object->GetParent())
        {
            Matrix34 parentTM = hitInfo.object->GetParent()->GetWorldTM();
            parentTM.OrthonormalizeFast();
            parentTM.SetTranslation(tm.GetTranslation());
            view->SetConstructionMatrix(COORDS_PARENT, parentTM);
        }
        else
        {
            Matrix34 parentTM;
            parentTM.SetIdentity();
            parentTM.SetTranslation(tm.GetTranslation());
            view->SetConstructionMatrix(COORDS_PARENT, parentTM);
        }
        userTM.SetTranslation(tm.GetTranslation());
        view->SetConstructionMatrix(COORDS_USERDEFINED, userTM);

        Matrix34 viewTM = view->GetViewTM();
        viewTM.SetTranslation(tm.GetTranslation());
        view->SetConstructionMatrix(COORDS_VIEW, viewTM);
    }
    else
    {
        Matrix34 tm;
        tm.SetIdentity();
        tm.SetTranslation(pos);
        userTM.SetTranslation(pos);
        view->SetConstructionMatrix(COORDS_LOCAL, tm);
        view->SetConstructionMatrix(COORDS_PARENT, tm);
        view->SetConstructionMatrix(COORDS_USERDEFINED, userTM);
    }

    if (editMode != eEditModeTool)
    {
        // Check for Move to position.
        if (bCtrlClick && bShiftClick)
        {
            // Ctrl-Click on terrain will move selected objects to specified location.
            MoveSelectionToPos(view, pos, bAltClick, point);
            bLockSelection = true;
        }
    }

    if (editMode == eEditModeMove)
    {
        if (!bNoRemoveSelection)
        {
            SetCommandMode(MoveMode);
        }

        if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
        {
            bLockSelection = true;
        }
    }
    else if (editMode == eEditModeRotate)
    {
        if (!bNoRemoveSelection)
        {
            SetCommandMode(RotateMode);
        }
        if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
        {
            bLockSelection = true;
        }
    }
    else if (editMode == eEditModeScale)
    {
        if (!bNoRemoveSelection)
        {
            GetIEditor()->GetSelection()->StartScaling();
            SetCommandMode(ScaleMode);
        }

        if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
        {
            bLockSelection = true;
        }
    }
    else if (hitObj != 0 && GetIEditor()->GetSelectedObject() == hitObj && !bAddSelect && !bUnselect)
    {
        bLockSelection = true;
    }

    if (!bLockSelection)
    {
        // If not selection locked.
        view->BeginUndo();

        if (!bNoRemoveSelection)
        {
            // Current selection should be cleared
            numUnselected = GetIEditor()->GetObjectManager()->ClearSelection();
        }

        if (hitObj)
        {
            numSelected = 1;

            if (!bUnselect)
            {
                if (hitObj->IsSelected())
                {
                    bUnselect = true;
                }
            }

            if (!bUnselect)
            {
                GetIEditor()->GetObjectManager()->SelectObject(hitObj, true);
            }
            else
            {
                GetIEditor()->GetObjectManager()->UnselectObject(hitObj);
            }

            AzToolsFramework::Components::EditorSelectionAccentingRequestBus::Broadcast(&AzToolsFramework::Components::EditorSelectionAccentingRequests::ProcessQueuedSelectionAccents);
        }
        if (view->IsUndoRecording())
        {
            // When a designer object is selected, the update of the designer object can cause a change of a edit tool, which will makes this objectmode tool pointer invalid.
            // so the update of objects must run on only pure idle time.
            // view->AcceptUndo method calls the OnIdle() function in the app, which this is not a right timing to updates all, I think. - Jaesik Hwang.
            GetIEditor()->GetObjectManager()->SetSkipUpdate(true);
            view->AcceptUndo("Select Object(s)");
            GetIEditor()->GetObjectManager()->SetSkipUpdate(false);
        }

        if ((numSelected == 0 || editMode == eEditModeSelect))
        {
            // If object is not selected.
            // Capture mouse input for this window.
            SetCommandMode(SelectMode);
        }
    }

    if (GetCommandMode() == MoveMode ||
        GetCommandMode() == RotateMode ||
        GetCommandMode() == ScaleMode)
    {
        view->BeginUndo();
    }

    //////////////////////////////////////////////////////////////////////////
    // Change cursor, must be before Capture mouse.
    //////////////////////////////////////////////////////////////////////////
    SetObjectCursor(view, hitObj, true);

    //////////////////////////////////////////////////////////////////////////
    view->CaptureMouse();
    //////////////////////////////////////////////////////////////////////////

    UpdateStatusText();

    m_bTransformChanged = false;
    m_AIMoveSimulation.OnSelectionChanged();

    if (m_pDeepSelection->GetMode() == CDeepSelection::DSM_POP)
    {
        return OnLButtonUp(view, nFlags, point);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonUp(CViewport* view, int nFlags, const QPoint& point)
{
    if (GetIEditor()->IsInGameMode())
    {
        // Ignore clicks while in game.
        return true;
    }

    if (m_bTransformChanged)
    {
        CSelectionGroup* pSelection = GetIEditor()->GetSelection();
        if (pSelection)
        {
            pSelection->FinishChanges();
        }
        m_bTransformChanged = false;
    }

    if (GetCommandMode() == ScaleMode)
    {
        Vec3 scale;
        GetIEditor()->GetSelection()->FinishScaling(GetScale(view, point, scale),
            GetIEditor()->GetReferenceCoordSys());
    }

    if (GetCommandMode() == MoveMode)
    {
        m_bDragThresholdExceeded = false;
    }

    // Reset the status bar caption
    GetIEditor()->SetStatusText("Ready");

    //////////////////////////////////////////////////////////////////////////
    if (view->IsUndoRecording())
    {
        if (GetCommandMode() == MoveMode)
        {
            AzToolsFramework::ScopedUndoBatch undo("Move");
            view->AcceptUndo("Move Selection");
        }
        else if (GetCommandMode() == RotateMode)
        {
            AzToolsFramework::ScopedUndoBatch undo("Rotate");
            view->AcceptUndo("Rotate Selection");
        }
        else if (GetCommandMode() == ScaleMode)
        {
            AzToolsFramework::ScopedUndoBatch undo("Scale");
            view->AcceptUndo("Scale Selection");
        }
        else
        {
            view->CancelUndo();
        }
    }
    //////////////////////////////////////////////////////////////////////////

    if (GetCommandMode() == SelectMode && (!GetIEditor()->IsSelectionLocked()))
    {
        const bool bUnselect = (Qt::AltModifier & QApplication::queryKeyboardModifiers());
        QRect selectRect = view->GetSelectionRectangle();
        if (!selectRect.isEmpty())
        {
            // Ignore too small rectangles.
            if (selectRect.width() > 5 && selectRect.height() > 5)
            {
                GetIEditor()->GetObjectManager()->SelectObjectsInRect(view, selectRect, !bUnselect);
                UpdateStatusText();
            }
        }

        if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
        {
            AABB box;
            GetIEditor()->GetSelectedRegion(box);

            //////////////////////////////////////////////////////////////////////////
            GetIEditor()->ClearSelection();

            /*
            SEntityProximityQuery query;
            query.box = box;
            gEnv->pEntitySystem->QueryProximity( query );
            for (int i = 0; i < query.nCount; i++)
            {
                IEntity *pIEntity = query.pEntities[i];
                CEntityObject *pEntity = CEntityObject::FindFromEntityId( pIEntity->GetId() );
                if (pEntity)
                {
                    GetIEditor()->GetObjectManager()->SelectObject( pEntity );
                }
            }
            */
            //////////////////////////////////////////////////////////////////////////
            /*

            if (fabs(box.min.x-box.max.x) > 0.5f && fabs(box.min.y-box.max.y) > 0.5f)
            {
                //@FIXME: restore it later.
                //Timur[1/14/2003]
                //SelectRectangle( box,!bUnselect );
                //SelectObjectsInRect( m_selectedRect,!bUnselect );
                GetIEditor()->GetObjectManager()->SelectObjects( box,bUnselect );
                GetIEditor()->UpdateViews(eUpdateObjects);
            }
            */
        }

        m_AIMoveSimulation.OnSelectionChanged();
    }
    // Release the restriction of the cursor
    view->ReleaseMouse();

    if (GetCommandMode() == ScaleMode || GetCommandMode() == MoveMode || GetCommandMode() == RotateMode)
    {
        GetIEditor()->GetObjectManager()->GetSelection()->ObjectModified();
    }

    if (GetIEditor()->GetEditMode() != eEditModeSelectArea)
    {
        view->ResetSelectionRegion();
    }
    // Reset selected rectangle.
    view->SetSelectionRectangle(QRect());

    // Restore default editor axis constrain.
    if (GetIEditor()->GetAxisConstrains() != view->GetAxisConstrain())
    {
        view->SetAxisConstrain(GetIEditor()->GetAxisConstrains());
    }

    SetCommandMode(NothingMode);

    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonDblClk(CViewport* view, int nFlags, const QPoint& point)
{
    // If shift clicked, Move the camera to this place.
    if (nFlags & MK_SHIFT)
    {
        // Get the heightmap coordinates for the click position
        Vec3 v = view->ViewToWorld(point);
        if (!(v.x == 0 && v.y == 0 && v.z == 0))
        {
            Matrix34 tm = view->GetViewTM();
            Vec3 p = tm.GetTranslation();
            float height = p.z - GetIEditor()->GetTerrainElevation(p.x, p.y);
            if (height < 1)
            {
                height = 1;
            }
            p.x = v.x;
            p.y = v.y;
            p.z = GetIEditor()->GetTerrainElevation(p.x, p.y) + height;
            tm.SetTranslation(p);
            view->SetViewTM(tm);
        }
    }
    else
    {
        // Check if double clicked on object.
        HitContext hitInfo;
        view->HitTest(point, hitInfo);

        CBaseObject* hitObj = hitInfo.object;
        if (hitObj)
        {
            // Fire double click event on hit object.
            hitObj->OnEvent(EVENT_DBLCLICK);
        }
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnRButtonDown(CViewport* view, int nFlags, const QPoint& point)
{
    if (gSettings.viewports.bEnableContextMenu)
    {
        m_openContext = true;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnRButtonUp(CViewport* view, int nFlags, const QPoint& point)
{
    if (m_openContext)
    {
        bool selectionLocked = GetIEditor()->IsSelectionLocked();

        QMenu* menu = new QMenu(viewport_cast<QtViewport*>(view));

        // Check if right clicked on object.
        HitContext hitInfo;
        hitInfo.bIgnoreAxis = true; // ignore gizmo
        view->HitTest(point, hitInfo);

        if (selectionLocked)
        {
            if (hitInfo.object)
            {
                // Populate object context.
                hitInfo.object->OnContextMenu(menu);
            }
        }
        else
        {
            Vec3 pos = view->SnapToGrid(view->ViewToWorld(point));
            Matrix34 userTM = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix();

            if (hitInfo.object)
            {
                Matrix34 tm = hitInfo.object->GetWorldTM();
                tm.OrthonormalizeFast();
                view->SetConstructionMatrix(COORDS_LOCAL, tm);
                if (hitInfo.object->GetParent())
                {
                    Matrix34 parentTM = hitInfo.object->GetParent()->GetWorldTM();
                    parentTM.OrthonormalizeFast();
                    parentTM.SetTranslation(tm.GetTranslation());
                    view->SetConstructionMatrix(COORDS_PARENT, parentTM);
                }
                else
                {
                    Matrix34 parentTM;
                    parentTM.SetIdentity();
                    parentTM.SetTranslation(tm.GetTranslation());
                    view->SetConstructionMatrix(COORDS_PARENT, parentTM);
                }
                userTM.SetTranslation(tm.GetTranslation());
                view->SetConstructionMatrix(COORDS_USERDEFINED, userTM);

                Matrix34 viewTM = view->GetViewTM();
                viewTM.SetTranslation(tm.GetTranslation());
                view->SetConstructionMatrix(COORDS_VIEW, viewTM);

                CSelectionGroup* selections = GetIEditor()->GetObjectManager()->GetSelection();

                // hit object has not been selected
                if (!selections->IsContainObject(hitInfo.object))
                {
                    GetIEditor()->GetObjectManager()->ClearSelection();
                    GetIEditor()->GetObjectManager()->SelectObject(hitInfo.object, true);
                }

                // Populate object context.
                hitInfo.object->OnContextMenu(menu);
            }
            else
            {
                Matrix34 tm;
                tm.SetIdentity();
                tm.SetTranslation(pos);
                userTM.SetTranslation(pos);
                view->SetConstructionMatrix(COORDS_LOCAL, tm);
                view->SetConstructionMatrix(COORDS_PARENT, tm);
                view->SetConstructionMatrix(COORDS_USERDEFINED, userTM);

                GetIEditor()->GetObjectManager()->ClearSelection();
            }
        }

        // Populate global context menu.
        int contextMenuFlag = 0;
        EBUS_EVENT(AzToolsFramework::EditorEvents::Bus,
            PopulateEditorGlobalContextMenu,
            menu,
            AZ::Vector2(static_cast<float>(point.x()), static_cast<float>(point.y())),
            contextMenuFlag);

        if (menu->isEmpty())
        {
            delete menu;
        }
        else
        {
            // Don't use exec() here: CRenderViewport hides the cursor when the mouse button is pressed
            // and shows it when button is released. If we exec  then we block and the cursor stays invisible while the menu is open
            //QObject::connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);
            menu->popup(QCursor::pos());
        }
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMButtonDown(CViewport* view, int nFlags, const QPoint& point)
{
    if (GetIEditor()->GetGameEngine()->GetSimulationMode())
    {
        // Get control key status.
        const bool bCtrlClick = (Qt::ControlModifier & QApplication::queryKeyboardModifiers());

        if (bCtrlClick)
        {
            // In simulation mode awake objects under the cursor when Ctrl+MButton pressed.
            AwakeObjectAtPoint(view, point);
            return true;
        }
        else
        {
            // Update AI move simulation when not holding Ctrl down.
            return m_AIMoveSimulation.UpdateAIMoveSimulation(view, point);
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::AwakeObjectAtPoint(CViewport* view, const QPoint& point)
{
    // In simulation mode awake objects under the cursor.
    // Check if double clicked on object.
    HitContext hitInfo;
    view->HitTest(point, hitInfo);
    CBaseObject* hitObj = hitInfo.object;
    if (hitObj)
    {
        IPhysicalEntity* pent = hitObj->GetCollisionEntity();
        if (pent)
        {
            pe_action_awake pa;
            pa.bAwake = true;
            pent->Action(&pa);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::MoveSelectionToPos(CViewport* view, Vec3& pos, bool align, const QPoint& point)
{
    view->BeginUndo();
    // Find center of selection.
    Vec3 center = GetIEditor()->GetSelection()->GetCenter();
    GetIEditor()->GetSelection()->Move(pos - center, CSelectionGroup::eMS_None, true, point);

    if (align)
    {
        GetIEditor()->GetSelection()->Align();
    }

    // This will capture any entity state changes that occurred
    // during the move.
    AzToolsFramework::ScopedUndoBatch undo("Transform");

    view->AcceptUndo("Move Selection");
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMouseMove(CViewport* view, int nFlags, const QPoint& point)
{
    if (GetIEditor()->IsInGameMode())
    {
        // Ignore while in game.
        return true;
    }

    m_openContext = false;
    SetObjectCursor(view, 0);

    Vec3 pos = view->SnapToGrid(view->ViewToWorld(point));

    // get world/local coordinate system setting.
    int coordSys = GetIEditor()->GetReferenceCoordSys();

    // get current axis constrains.
    if (GetCommandMode() == MoveMode)
    {
        if (!m_bDragThresholdExceeded)
        {
            int halfLength = gSettings.viewports.nDragSquareSize / 2;
            QRect rcDrag(m_cMouseDownPos, QSize(0,0));
            rcDrag.adjust(-halfLength, -halfLength, halfLength, halfLength);

            if (!rcDrag.contains(point))
            {
                m_bDragThresholdExceeded = true;
            }
            else
            {
                return true;
            }
        }

        GetIEditor()->RestoreUndo();

        Vec3 v;
        //m_cMouseDownPos = point;
        CSelectionGroup::EMoveSelectionFlag selectionFlag = CSelectionGroup::eMS_None;
        if (view->GetAxisConstrain() == AXIS_TERRAIN)
        {
            selectionFlag = CSelectionGroup::eMS_FollowTerrain;
            Vec3 p1 = view->SnapToGrid(view->ViewToWorld(m_cMouseDownPos));
            Vec3 p2 = view->SnapToGrid(view->ViewToWorld(point));
            v = p2 - p1;
            v.z = 0;
        }
        else
        {
            Vec3 p1 = view->MapViewToCP(m_cMouseDownPos);
            Vec3 p2 = view->MapViewToCP(point);
            if (p1.IsZero() || p2.IsZero())
            {
                return true;
            }
            v = view->GetCPVector(p1, p2);

            //Matrix invParent = m_parentConstructionMatrix;
            //invParent.Invert();
            //p1 = invParent.TransformVector(p1);
            //p2 = invParent.TransformVector(p2);
            //v = p2 - p1;
        }

        if ((nFlags & MK_CONTROL) && !(nFlags & MK_SHIFT))
        {
            selectionFlag = CSelectionGroup::eMS_FollowGeometryPosNorm;
        }

        if (!v.IsEquivalent(Vec3(0, 0, 0)))
        {
            m_bTransformChanged = true;
        }

        CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
        {
            CTrackViewSequenceNoNotificationContext context(pSequence);
            GetIEditor()->GetSelection()->Move(v, selectionFlag, coordSys, point);
        }

        if (pSequence)
        {
            pSequence->OnKeysChanged();
        }

        return true;
    }
    else if (GetCommandMode() == ScaleMode)
    {
        GetIEditor()->RestoreUndo();
        Vec3 scale;
        GetIEditor()->GetSelection()->Scale(GetScale(view, point, scale), coordSys);
        if (!scale.IsEquivalent(Vec3(0, 0, 0)))
        {
            m_bTransformChanged = true;
        }
    }
    else if (GetCommandMode() == SelectMode)
    {
        // Ignore select when selection locked.
        if (GetIEditor()->IsSelectionLocked())
        {
            return true;
        }

        QRect rc(m_cMouseDownPos, point - QPoint(1, 1));
        if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
        {
            view->OnDragSelectRectangle(rc, false);
        }
        else
        {
            view->SetSelectionRectangle(rc);
        }
        //else
        //OnDragSelectRectangle( CPoint(rc.left,rc.top),CPoint(rc.right,rc.bottom),true );
    }

    if (!(nFlags & MK_RBUTTON || nFlags & MK_MBUTTON))
    {
        // Track mouse movements.
        HitContext hitInfo;
        if (view->HitTest(point, hitInfo))
        {
            SetObjectCursor(view, hitInfo.object);
        }

        HandleMoveByFaceNormal(hitInfo);
    }

    if ((nFlags & MK_MBUTTON) && GetIEditor()->GetGameEngine()->GetSimulationMode())
    {
        // Get control key status.
        const bool bCtrlClick = (Qt::ControlModifier & QApplication::queryKeyboardModifiers());

        if (bCtrlClick)
        {
            // In simulation mode awake objects under the cursor when Ctrl+MButton pressed.
            AwakeObjectAtPoint(view, point);
        }
    }

    UpdateStatusText();
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMouseLeave(CViewport* view)
{
    if (GetIEditor()->IsInGameMode())
    {
        // Ignore while in game.
        return true;
    }

    m_openContext = false;
    SetObjectCursor(view, 0);

    return true;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::SetObjectCursor(CViewport* view, CBaseObject* hitObj, bool bChangeNow)
{
    EStdCursor cursor = STD_CURSOR_DEFAULT;
    QString m_cursorStr;

    CBaseObject* pMouseOverObject = NULL;
    if (!GuidUtil::IsEmpty(m_MouseOverObject))
    {
        pMouseOverObject = GetIEditor()->GetObjectManager()->FindObject(m_MouseOverObject);
    }

    //HCURSOR hPrevCursor = m_hCurrCursor;
    if (pMouseOverObject)
    {
        pMouseOverObject->SetHighlight(false);
    }
    if (hitObj)
    {
        m_MouseOverObject = hitObj->GetId();
    }
    else
    {
        m_MouseOverObject = GUID_NULL;
    }
    pMouseOverObject = hitObj;
    bool bHitSelectedObject = false;
    if (pMouseOverObject)
    {
        if (GetCommandMode() != SelectMode && !GetIEditor()->IsSelectionLocked())
        {
            if (pMouseOverObject->CanBeHightlighted())
            {
                pMouseOverObject->SetHighlight(true);
            }

            m_cursorStr = pMouseOverObject->GetName();

            QString comment(pMouseOverObject->GetComment());
            if (!comment.isEmpty())
            {
                m_cursorStr += "\n";
                m_cursorStr += comment;
            }

            const QString triangleCountText = pMouseOverObject->GetMouseOverStatisticsText();
            if (gSettings.viewports.bShowMeshStatsOnMouseOver && !triangleCountText.isEmpty())
            {
                m_cursorStr += triangleCountText;
            }

            QString warnings(pMouseOverObject->GetWarningsText());
            if (!warnings.isEmpty())
            {
                m_cursorStr += warnings;
            }

            cursor = STD_CURSOR_HIT;
            if (pMouseOverObject->IsSelected())
            {
                bHitSelectedObject = true;
            }
        }

        QString tooltip = pMouseOverObject->GetTooltip();
        if (!tooltip.isEmpty())
        {
            m_cursorStr += "\n";
            m_cursorStr += tooltip;
        }
        ;
    }
    else
    {
        m_cursorStr = "";
        cursor = STD_CURSOR_DEFAULT;
    }
    // Get control key status.
    const auto modifiers = QApplication::queryKeyboardModifiers();
    const bool bAltClick = (Qt::AltModifier & modifiers);
    const bool bCtrlClick = (Qt::ControlModifier & modifiers);
    const bool bShiftClick = (Qt::ShiftModifier & modifiers);

    bool bAddSelect = bCtrlClick && !bShiftClick;
    bool bUnselect = bAltClick;
    bool bNoRemoveSelection = bAddSelect || bUnselect;

    bool bLockSelection = GetIEditor()->IsSelectionLocked();

    if (GetCommandMode() == SelectMode || GetCommandMode() == NothingMode)
    {
        if (bAddSelect)
        {
            cursor = STD_CURSOR_SEL_PLUS;
        }
        if (bUnselect)
        {
            cursor = STD_CURSOR_SEL_MINUS;
        }

        if ((bHitSelectedObject && !bNoRemoveSelection) || bLockSelection)
        {
            int editMode = GetIEditor()->GetEditMode();
            if (editMode == eEditModeMove)
            {
                cursor = STD_CURSOR_MOVE;
            }
            else if (editMode == eEditModeRotate)
            {
                cursor = STD_CURSOR_ROTATE;
            }
            else if (editMode == eEditModeScale)
            {
                cursor = STD_CURSOR_SCALE;
            }
        }
    }
    else if (GetCommandMode() == MoveMode)
    {
        cursor = STD_CURSOR_MOVE;
    }
    else if (GetCommandMode() == RotateMode)
    {
        cursor = STD_CURSOR_ROTATE;
    }
    else if (GetCommandMode() == ScaleMode)
    {
        cursor = STD_CURSOR_SCALE;
    }

    AZ::u32 cursorId = static_cast<AZ::u32>(cursor);
    AZStd::string cursorStr = m_cursorStr.toLatin1().data();
    EBUS_EVENT(AzToolsFramework::EditorRequests::Bus,
        UpdateObjectModeCursor,
        cursorId,
        cursorStr);
    cursor = static_cast<EStdCursor>(cursorId);
    m_cursorStr = cursorStr.c_str();

    /*
    if (bChangeNow)
    {
        if (GetCapture() == NULL)
        {
            if (m_hCurrCursor)
                SetCursor( m_hCurrCursor );
            else
                SetCursor( m_hDefaultCursor );
        }
    }
    */
    view->SetCurrentCursor(cursor, m_cursorStr);
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::RegisterTool(CRegistrationContext& rc)
{
    rc.pClassFactory->RegisterClass(new CQtViewClass<CObjectMode>("EditTool.ObjectMode", "Select", ESYSTEM_CLASS_EDITTOOL));
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::UpdateStatusText()
{
    QString str;
    int nCount = GetIEditor()->GetSelection()->GetCount();
    if (nCount > 0)
    {
        str = tr("%1 Object(s) Selected").arg(nCount);
    }
    else
    {
        str = tr("No Selection");
    }
    SetStatusText(str);
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::CheckDeepSelection(HitContext& hitContext, CViewport* view)
{
    if (hitContext.pDeepSelection)
    {
        m_pDeepSelection->CollectCandidate(hitContext.dist, gSettings.deepSelectionSettings.fRange);
    }

    if (m_pDeepSelection->GetCandidateObjectCount() > 1)
    {
        // Deep Selection Pop Mode
        if (m_pDeepSelection->GetMode() == CDeepSelection::DSM_POP)
        {
            // Show a sorted pop-up menu for selecting a bone.
            QMenu popUpDeepSelect(qobject_cast<QWidget*>(view->qobject()));

            for (int i = 0; i < m_pDeepSelection->GetCandidateObjectCount(); ++i)
            {
                QAction* action = popUpDeepSelect.addAction(QString(m_pDeepSelection->GetCandidateObject(i)->GetName()));
                action->setData(i);
            }

            QAction* userSelection = popUpDeepSelect.exec(QCursor::pos());
            if (userSelection)
            {
                int nSelect = userSelection->data().toInt();

                // Update HitContext hitInfo.
                hitContext.object = m_pDeepSelection->GetCandidateObject(nSelect);
                m_pDeepSelection->ExcludeHitTest(nSelect);
            }
        }
        else if (m_pDeepSelection->GetMode() == CDeepSelection::DSM_CYCLE)
        {
            int selPos = m_pDeepSelection->GetCurrentSelectPos();
            hitContext.object = m_pDeepSelection->GetCandidateObject(selPos + 1);
            m_pDeepSelection->ExcludeHitTest(selPos + 1);
        }
    }
}

Vec3& CObjectMode::GetScale(const CViewport* view, const QPoint& point, Vec3& OutScale)
{
    float ay = 1.0f - 0.01f * (point.y() - m_cMouseDownPos.y());

    if (ay < 0.01f)
    {
        ay = 0.01f;
    }

    Vec3 scl(ay, ay, ay);

    int axisConstrain = view->GetAxisConstrain();

    if (axisConstrain < AXIS_XYZ && GetIEditor()->IsAxisVectorLocked())
    {
        axisConstrain = AXIS_XYZ;
    }

    switch (axisConstrain)
    {
    case AXIS_X:
        scl(ay, 1, 1);
        break;
    case AXIS_Y:
        scl(1, ay, 1);
        break;
    case AXIS_Z:
        scl(1, 1, ay);
        break;
    case AXIS_XY:
        scl(ay, ay, ay);
        break;
    case AXIS_XZ:
        scl(ay, ay, ay);
        break;
    case AXIS_YZ:
        scl(ay, ay, ay);
        break;
    case AXIS_XYZ:
        scl(ay, ay, ay);
        break;
    case AXIS_TERRAIN:
        scl(ay, ay, ay);
        break;
    }
    ;

    OutScale = scl;

    return OutScale;
}

//////////////////////////////////////////////////////////////////////////
// This callback is currently called only to handle the case of the 'move by the face normal'.
// Other movements of the object are handled in the 'CObjectMode::OnMouseMove()' method.
void CObjectMode::OnManipulatorDrag(CViewport* view, ITransformManipulator* pManipulator, QPoint& point0, QPoint& point1, const Vec3& value)
{
    RefCoordSys coordSys = GetIEditor()->GetReferenceCoordSys();
    int editMode = GetIEditor()->GetEditMode();

    if (editMode == eEditModeMove)
    {
        GetIEditor()->RestoreUndo();
        CSelectionGroup* pSelGrp = GetIEditor()->GetSelection();

        CSelectionGroup::EMoveSelectionFlag selectionFlag = view->GetAxisConstrain() == AXIS_TERRAIN ? CSelectionGroup::eMS_FollowTerrain : CSelectionGroup::eMS_None;
        pSelGrp->Move(value, selectionFlag, coordSys, point0);

        if (m_pHitObject)
        {
            UpdateMoveByFaceNormGizmo(m_pHitObject);
        }
    }
}

void CObjectMode::HandleMoveByFaceNormal(HitContext& hitInfo)
{
    CBaseObject* pHitObject = hitInfo.object;
    bool bFaceNormalMovePossible = pHitObject && GetIEditor()->GetEditMode() == eEditModeMove
        && (pHitObject->GetType() == OBJTYPE_SOLID || pHitObject->GetType() == OBJTYPE_BRUSH)
        && pHitObject->IsSelected();
    const bool bNKeyPressed = CheckVirtualKey(Qt::Key_N);
    if (bFaceNormalMovePossible && bNKeyPressed)
    {
        // Test a hit for its faces.
        hitInfo.nSubObjFlags = SO_HIT_POINT | SO_HIT_SELECT | SO_HIT_NO_EDIT | SO_HIT_ELEM_FACE;
        pHitObject->SetFlags(OBJFLAG_SUBOBJ_EDITING);
        pHitObject->HitTest(hitInfo);
        pHitObject->ClearFlags(OBJFLAG_SUBOBJ_EDITING);

        UpdateMoveByFaceNormGizmo(pHitObject);
    }
    else if (m_bMoveByFaceNormManipShown && !bNKeyPressed)
    {
        HideMoveByFaceNormGizmo();
    }
}

void CObjectMode::UpdateMoveByFaceNormGizmo(CBaseObject* pHitObject)
{
    Matrix34 refFrame;
    refFrame.SetIdentity();
    SubObjectSelectionReferenceFrameCalculator calculator(SO_ELEM_FACE);
    pHitObject->CalculateSubObjectSelectionReferenceFrame(&calculator);
    if (calculator.GetFrame(refFrame) == false)
    {
        HideMoveByFaceNormGizmo();
    }
    else
    {
        ITransformManipulator* pManipulator = GetIEditor()->ShowTransformManipulator(true);
        m_bMoveByFaceNormManipShown = true;
        m_pHitObject = pHitObject;

        Matrix34 parentTM = pHitObject->GetWorldTM();
        Matrix34 userTM = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix();
        parentTM.SetTranslation(refFrame.GetTranslation());
        userTM.SetTranslation(refFrame.GetTranslation());
        pManipulator->SetTransformation(COORDS_LOCAL, refFrame);
        pManipulator->SetTransformation(COORDS_PARENT, parentTM);
        pManipulator->SetTransformation(COORDS_USERDEFINED, userTM);
        pManipulator->SetAlwaysUseLocal(true);
    }
}

void CObjectMode::HideMoveByFaceNormGizmo()
{
    GetIEditor()->ShowTransformManipulator(false);
    m_bMoveByFaceNormManipShown = false;
    m_pHitObject = NULL;
}

#include <EditMode/ObjectMode.moc>
