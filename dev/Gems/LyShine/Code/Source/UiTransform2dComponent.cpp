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
#include "UiTransform2dComponent.h"

#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include <IRenderer.h>

#include <LyShine/IDraw2d.h>
#include <LyShine/Bus/UiCanvasBus.h>
#include <LyShine/Bus/UiElementBus.h>
#include <LyShine/Bus/UiLayoutBus.h>

#include "UiSerialize.h"

namespace
{
    bool AxisAlignedBoxesIntersect(const AZ::Vector2& minA, const AZ::Vector2& maxA, const AZ::Vector2& minB, const AZ::Vector2& maxB)
    {
        bool boxesIntersect = true;

        if (maxA.GetX() < minB.GetX() || // a is left of b
            minA.GetX() > maxB.GetX() || // a is right of b
            maxA.GetY() < minB.GetY() || // a is above b
            minA.GetY() > maxB.GetY())   // a is below b
        {
            boxesIntersect = false;   // no overlap
        }

        return boxesIntersect;
    }

    void GetInverseTransform(const AZ::Vector2& pivot, const AZ::Vector2& scale, float rotation, AZ::Matrix4x4& mat)
    {
        AZ::Vector3 pivot3(pivot.GetX(), pivot.GetY(), 0);

        float rotRad = DEG2RAD(-rotation);            // inverse rotation

        // Avoid a divide by zero. We could compare with 0.0f here and that would avoid a divide
        // by zero. However comparing with FLT_EPSILON also avoids the rare case of an overflow.
        // FLT_EPSILON is small enough to be considered equivalent to zero in this application.
        float inverseScaleX = (fabsf(scale.GetX()) > FLT_EPSILON) ? 1.0f / scale.GetX() : 0;
        float inverseScaleY = (fabsf(scale.GetY()) > FLT_EPSILON) ? 1.0f / scale.GetY() : 0;

        AZ::Vector3 scale3(inverseScaleX, inverseScaleY, 0); // inverse scale

        AZ::Matrix4x4 moveToPivotSpaceMat = AZ::Matrix4x4::CreateTranslation(-pivot3);
        AZ::Matrix4x4 scaleMat = AZ::Matrix4x4::CreateScale(scale3);
        AZ::Matrix4x4 rotMat = AZ::Matrix4x4::CreateRotationZ(rotRad);
        AZ::Matrix4x4 moveFromPivotSpaceMat = AZ::Matrix4x4::CreateTranslation(pivot3);

        mat = moveFromPivotSpaceMat * scaleMat * rotMat * moveToPivotSpaceMat;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
UiTransform2dComponent::UiTransform2dComponent()
    : m_pivot(AZ::Vector2(0.5f, 0.5f))
    , m_rotation(0.0f)
    , m_scale(AZ::Vector2(1.0f, 1.0f))
    , m_scaleToDevice(false)
    , m_recomputeTransform(true)
    , m_recomputeCanvasSpaceRect(true)
    , m_rectInitialized(false)
    , m_rectChangedByInitialization(false)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiTransform2dComponent::~UiTransform2dComponent()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float UiTransform2dComponent::GetZRotation()
{
    return m_rotation;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetZRotation(float rotation)
{
    m_rotation = rotation;
    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetScale()
{
    return AZ::Vector2(m_scale.GetX(), m_scale.GetY());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetScale(AZ::Vector2 scale)
{
    m_scale = AZ::Vector2(scale.GetX(), scale.GetY());
    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetPivot()
{
    return m_pivot;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetPivot(AZ::Vector2 pivot)
{
    m_pivot = pivot;
    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::GetScaleToDevice()
{
    return m_scaleToDevice;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetScaleToDevice(bool scaleToDevice)
{
    m_scaleToDevice = scaleToDevice;
    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetViewportSpacePoints(RectPoints& points)
{
    GetCanvasSpacePointsNoScaleRotate(points);
    RotateAndScalePoints(points);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetViewportSpacePivot()
{
    // this function is primarily used for drawing the pivot in the editor. Since we snap the pivot
    // icon to the nearest pixel, if the X position is something like 20.5 it will snap different ways
    // depending on rounding errors. We don't want this to happen while rotating an element. So, make
    // sure the ViewportSpacePivot is calculated in a way that is independent of this element's
    // scale and rotation.
    AZ::Vector2 canvasSpacePivot = GetCanvasSpacePivotNoScaleRotate();
    AZ::Vector3 point3(canvasSpacePivot.GetX(), canvasSpacePivot.GetY(), 0.0f);

    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        AZ::Matrix4x4 transform;
        EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetTransformToViewport, transform);

        point3 = transform * point3;
    }

    return AZ::Vector2(point3.GetX(), point3.GetY());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetTransformToViewport(AZ::Matrix4x4& mat)
{
    // if we already computed the transform, don't recompute.
    if (!m_recomputeTransform)
    {
        mat = m_transformToViewport;
        return;
    }

    // first get the transform to canvas space
    GetTransformToCanvasSpace(mat);

    // then get the transform from canvas to viewport space
    AZ::Matrix4x4 canvasToViewportMatrix;
    EBUS_EVENT_ID_RESULT(canvasToViewportMatrix, GetCanvasEntityId(), UiCanvasBus, GetCanvasToViewportMatrix);

    // add the transform to viewport space to the matrix
    mat = canvasToViewportMatrix * mat;

    m_transformToViewport = mat;
    m_recomputeTransform = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetTransformFromViewport(AZ::Matrix4x4& mat)
{
    // first get the transform from canvas space
    GetTransformFromCanvasSpace(mat);

    // then get the transform from viewport to canvas space
    AZ::Matrix4x4 viewportToCanvasMatrix;
    EBUS_EVENT_ID(GetCanvasEntityId(), UiCanvasBus, GetViewportToCanvasMatrix, viewportToCanvasMatrix);

    // add the transform from viewport space to canvas space to the transform matrix
    mat = mat * viewportToCanvasMatrix;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::RotateAndScalePoints(RectPoints& points)
{
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        AZ::Matrix4x4 transform;
        GetTransformToViewport(transform);

        points = points.Transform(transform);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetCanvasSpacePoints(RectPoints& points)
{
    GetCanvasSpacePointsNoScaleRotate(points);

    // apply the transfrom to canvas space
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        AZ::Matrix4x4 transform;
        GetTransformToCanvasSpace(transform);

        points = points.Transform(transform);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetCanvasSpacePivot()
{
    AZ::Vector2 canvasSpacePivot = GetCanvasSpacePivotNoScaleRotate();
    AZ::Vector3 point3(canvasSpacePivot.GetX(), canvasSpacePivot.GetY(), 0.0f);

    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        AZ::Matrix4x4 transform;
        EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetTransformToCanvasSpace, transform);

        point3 = transform * point3;
    }

    return AZ::Vector2(point3.GetX(), point3.GetY());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetTransformToCanvasSpace(AZ::Matrix4x4& mat)
{
    // this takes a matrix and builds the concatenation of this elements rotate and scale about the pivot
    // with the transforms for all parent elements into one 3x4 matrix.
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetTransformToCanvasSpace, mat);

        AZ::Matrix4x4 transformToParent;
        GetLocalTransform(transformToParent);

        mat = mat * transformToParent;
    }
    else
    {
        mat = AZ::Matrix4x4::CreateIdentity();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetTransformFromCanvasSpace(AZ::Matrix4x4& mat)
{
    // this takes a matrix and builds the concatenation of this elements rotate and scale about the pivot
    // with the transforms for all parent elements into one 3x4 matrix.
    // The result is an inverse transform that can be used to map from transformed space to non-transformed
    // space
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetTransformFromCanvasSpace, mat);

        AZ::Matrix4x4 transformFromParent;
        GetLocalInverseTransform(transformFromParent);

        mat = transformFromParent * mat;
    }
    else
    {
        mat = AZ::Matrix4x4::CreateIdentity();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetCanvasSpaceRectNoScaleRotate(Rect& rect)
{
    CalculateCanvasSpaceRect();
    rect = m_rect;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetCanvasSpacePointsNoScaleRotate(RectPoints& points)
{
    Rect rect;
    GetCanvasSpaceRectNoScaleRotate(rect);
    points.SetAxisAligned(rect.left, rect.right, rect.top, rect.bottom);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetCanvasSpaceSizeNoScaleRotate()
{
    Rect rect;
    GetCanvasSpaceRectNoScaleRotate(rect);
    return rect.GetSize();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetCanvasSpacePivotNoScaleRotate()
{
    Rect rect;
    GetCanvasSpaceRectNoScaleRotate(rect);

    AZ::Vector2 size = rect.GetSize();

    float x =  rect.left + size.GetX() * m_pivot.GetX();
    float y =  rect.top + size.GetY() * m_pivot.GetY();

    return AZ::Vector2(x, y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetLocalTransform(AZ::Matrix4x4& mat)
{
    // this takes a matrix and builds the concatenation of this element's rotate and scale about the pivot
    AZ::Vector2 pivot = GetCanvasSpacePivotNoScaleRotate();
    AZ::Vector3 pivot3(pivot.GetX(), pivot.GetY(), 0);

    float rotRad = DEG2RAD(m_rotation);     // rotation

    AZ::Vector2 scale = GetScaleAdjustedForDevice();
    AZ::Vector3 scale3(scale.GetX(), scale.GetY(), 1);   // scale

    AZ::Matrix4x4 moveToPivotSpaceMat = AZ::Matrix4x4::CreateTranslation(-pivot3);
    AZ::Matrix4x4 scaleMat = AZ::Matrix4x4::CreateScale(scale3);
    AZ::Matrix4x4 rotMat = AZ::Matrix4x4::CreateRotationZ(rotRad);
    AZ::Matrix4x4 moveFromPivotSpaceMat = AZ::Matrix4x4::CreateTranslation(pivot3);

    mat = moveFromPivotSpaceMat * rotMat * scaleMat * moveToPivotSpaceMat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::GetLocalInverseTransform(AZ::Matrix4x4& mat)
{
    // this takes a matrix and builds the concatenation of this element's rotate and scale about the pivot
    // The result is an inverse transform that can be used to map from parent space to non-transformed
    // space
    AZ::Vector2 pivot = GetCanvasSpacePivotNoScaleRotate();
    AZ::Vector2 scale = GetScaleAdjustedForDevice();
    GetInverseTransform(pivot, scale, m_rotation, mat);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::HasScaleOrRotation()
{
    return !(m_scale.GetX() == 1.0f && m_scale.GetY() == 1.0f && m_rotation == 0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetViewportPosition()
{
    return GetViewportSpacePivot();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetViewportPosition(const AZ::Vector2& position)
{
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (!parentElement)
    {
        return; // this is the root element
    }

    AZ::Vector2 curCanvasSpacePosition = GetCanvasSpacePivotNoScaleRotate();

    AZ::Matrix4x4 transform;
    EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetTransformFromViewport, transform);

    AZ::Vector3 point3(position.GetX(), position.GetY(), 0.0f);
    point3 = transform * point3;
    AZ::Vector2 canvasSpacePosition(point3.GetX(), point3.GetY());

    m_offsets += canvasSpacePosition - curCanvasSpacePosition;

    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetCanvasPosition()
{
    return GetCanvasSpacePivot();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetCanvasPosition(const AZ::Vector2& position)
{
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (!parentElement)
    {
        return; // this is the root element
    }

    AZ::Vector2 curCanvasSpacePosition = GetCanvasSpacePivotNoScaleRotate();

    AZ::Matrix4x4 transform;
    EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetTransformFromCanvasSpace, transform);

    AZ::Vector3 point3(position.GetX(), position.GetY(), 0.0f);
    point3 = transform * point3;
    AZ::Vector2 canvasSpacePosition(point3.GetX(), point3.GetY());

    m_offsets += canvasSpacePosition - curCanvasSpacePosition;

    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetLocalPosition()
{
    AZ::Vector2 position = GetCanvasSpacePivotNoScaleRotate() - GetCanvasSpaceAnchorsCenterNoScaleRotate();
    return position;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetLocalPosition(const AZ::Vector2& position)
{
    AZ::Vector2 curPosition = GetLocalPosition();
    m_offsets += position - curPosition;

    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::MoveViewportPositionBy(const AZ::Vector2& offset)
{
    SetViewportPosition(GetViewportPosition() + offset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::MoveCanvasPositionBy(const AZ::Vector2& offset)
{
    SetCanvasPosition(GetCanvasPosition() + offset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::MoveLocalPositionBy(const AZ::Vector2& offset)
{
    SetLocalPosition(GetLocalPosition() + offset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::IsPointInRect(AZ::Vector2 point)
{
    // get point in the no scale/rotate canvas space for this element
    AZ::Matrix4x4 transform;
    GetTransformFromViewport(transform);
    AZ::Vector3 point3(point.GetX(), point.GetY(), 0.0f);
    point3 = transform * point3;

    // get the rect for this element in the same space
    Rect rect;
    GetCanvasSpaceRectNoScaleRotate(rect);

    float left   = rect.left;
    float right  = rect.right;
    float top    = rect.top;
    float bottom = rect.bottom;

    // allow for "flipped" rects
    if (left > right)
    {
        std::swap(left, right);
    }
    if (top > bottom)
    {
        std::swap(top, bottom);
    }

    // point is in rect if it is within rect or exactly on edge
    if (point3.GetX() >= left &&
        point3.GetX() <= right &&
        point3.GetY() >= top &&
        point3.GetY() <= bottom)
    {
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::BoundsAreOverlappingRect(const AZ::Vector2& bound0, const AZ::Vector2& bound1)
{
    // Get the element points in viewport space
    RectPoints points;
    GetViewportSpacePoints(points);

    // If the element is axis aligned we can just do an AAAB to AABB intersection test.
    // This is by far the most common case in UI canvases
    if (points.TopLeft().GetY() == points.TopRight().GetY() && points.TopLeft().GetX() <= points.TopRight().GetX() &&
        points.TopLeft().GetX() == points.BottomLeft().GetX() && points.TopLeft().GetY() <= points.BottomLeft().GetY())
    {
        // the element has no rotation and is not flipped so use AABB test
        return AxisAlignedBoxesIntersect(bound0, bound1, points.TopLeft(), points.BottomRight());
    }

    // IMPORTANT: This collision detection algorithm is based on the
    // Separating Axis Theorem, but is optimized for this context.
    // This ISN'T a generalized implementation. We DISCOURAGE using
    // this implementation elsewhere.
    //
    // Reference:
    // http://en.wikipedia.org/wiki/Hyperplane_separation_theorem

    // Vertices from shape A (input shape, which is axis-aligned).
    std::list<AZ::Vector2> vertsA;
    {
        // bound0
        //        A----B
        //        |    |
        //        D----C
        //               bound1

        vertsA.push_back(bound0); // A.
        vertsA.push_back(AZ::Vector2(bound1.GetX(), bound0.GetY())); // B.
        vertsA.push_back(bound1); // C.
        vertsA.push_back(AZ::Vector2(bound0.GetX(), bound1.GetY())); // D.
    }

    // Vertices from shape B (our shape, which ISN'T axis-aligned).
    RectPoints vertsB = points;

    // Normals from shape A (input shape, which is axis-aligned).
    // IMPORTANT: This ISN'T thread-safe.
    static std::list<AZ::Vector2> edgeNormalsA;
    if (edgeNormalsA.empty())
    {
        edgeNormalsA.push_back(AZ::Vector2(0.0f, 1.0f));
        edgeNormalsA.push_back(AZ::Vector2(1.0f, 0.0f));
        edgeNormalsA.push_back(AZ::Vector2(0.0f, -1.0f));
        edgeNormalsA.push_back(AZ::Vector2(-1.0f, 0.0f));
    }

    // All edge normals.
    std::list<AZ::Vector2> edgeNormals(edgeNormalsA);

    // Normals from shape B (our rect shape, which ISN'T axis-aligned).
    {
        // A----B
        // |    |
        // D----C
        const AZ::Vector2& A = vertsB.TopLeft();
        const AZ::Vector2& B = vertsB.TopRight();
        const AZ::Vector2& C = vertsB.BottomRight();
        const AZ::Vector2& D = vertsB.BottomLeft();

        AZ::Vector2 normAB((B - A).GetNormalized().GetPerpendicular());
        AZ::Vector2 normBC((C - B).GetNormalized().GetPerpendicular());
        AZ::Vector2 normCD((D - C).GetNormalized().GetPerpendicular());
        AZ::Vector2 normDA((A - D).GetNormalized().GetPerpendicular());

        edgeNormals.push_back(normAB);
        edgeNormals.push_back(normBC);
        edgeNormals.push_back(normCD);
        edgeNormals.push_back(normDA);
    }

    // A collision occurs only when we CAN'T find any gaps.
    // To find a gap, we project all vertices against all normals.
    for (auto && n : edgeNormals)
    {
        std::set<float> vertsAdot;
        std::set<float> vertsBdot;

        for (auto && v : vertsA)
        {
            vertsAdot.insert(n.Dot(v));
        }

        for (auto && v : vertsB.pt)
        {
            vertsBdot.insert(n.Dot(v));
        }

        float minA = *vertsAdot.begin();
        float maxA = *vertsAdot.rbegin();
        float minB = *vertsBdot.begin();
        float maxB = *vertsBdot.rbegin();

        // Two intervals overlap if:
        //
        // ( ( A.min < B.max ) &&
        //   ( A.max > B.min ) )
        //
        // Visual reference:
        // http://silentmatt.com/rectangle-intersection/
        if (!(minA < maxB) && (maxA > minB))
        {
            // Stop as soon as we find a gap.
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetRecomputeTransformFlag()
{
    int numChildren = 0;
    EBUS_EVENT_ID_RESULT(numChildren, GetEntityId(), UiElementBus, GetNumChildElements);
    for (int i = 0; i < numChildren; i++)
    {
        AZ::Entity* childElement = nullptr;
        EBUS_EVENT_ID_RESULT(childElement, GetEntityId(), UiElementBus, GetChildElement, i);
        if (childElement)
        {
            EBUS_EVENT_ID(childElement->GetId(), UiTransformBus, SetRecomputeTransformFlag);
        }
    }

    m_recomputeTransform = true;
    m_recomputeCanvasSpaceRect = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::HasCanvasSpaceRectChanged()
{
    CalculateCanvasSpaceRect();

    return (HasCanvasSpaceRectChangedByInitialization() || m_rect != m_prevRect);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::HasCanvasSpaceSizeChanged()
{
    if (HasCanvasSpaceRectChanged())
    {
        static const float sizeChangeTolerance = 0.05f;

        // If old rect equals new rect, size changed due to initialization
        return (HasCanvasSpaceRectChangedByInitialization() || !m_prevRect.GetSize().IsClose(m_rect.GetSize(), sizeChangeTolerance));
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::HasCanvasSpaceRectChangedByInitialization()
{
    return m_rectChangedByInitialization;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::NotifyAndResetCanvasSpaceRectChange()
{
    if (HasCanvasSpaceRectChanged())
    {
        // Reset before sending the notification because the notification could trigger a new rect change
        Rect prevRect = m_prevRect;
        m_prevRect = m_rect;
        m_rectChangedByInitialization = false;
        EBUS_EVENT_ID(GetEntityId(), UiTransformChangeNotificationBus, OnCanvasSpaceRectChanged, GetEntityId(), prevRect, m_rect);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiTransform2dComponent::Anchors UiTransform2dComponent::GetAnchors()
{
    return m_anchors;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetAnchors(Anchors anchors, bool adjustOffsets, bool allowPush)
{
    // First adjust the input structure to be valid.
    // If either pair of anchors is flipped then set them to be the same.
    // To avoid changing one anchor "pushing" the other we check which one changed and correct that
    // unless allowPush is set in which case we do the opposite
    if (anchors.m_right < anchors.m_left)
    {
        if (anchors.m_right != m_anchors.m_right)
        {
            // right anchor changed
            if (allowPush)
            {
                anchors.m_left = anchors.m_right;   // push left to match right
            }
            else
            {
                anchors.m_right = anchors.m_left;   // clamp to right to equal left
            }
        }
        else
        {
            // left changed or both changed
            if (allowPush)
            {
                anchors.m_right = anchors.m_left;   // push right to match left
            }
            else
            {
                anchors.m_left = anchors.m_right;   // clamp left to equal right
            }
        }
    }

    if (anchors.m_bottom < anchors.m_top)
    {
        if (anchors.m_bottom != m_anchors.m_bottom)
        {
            // bottom anchor changed
            if (allowPush)
            {
                anchors.m_top = anchors.m_bottom;   // push top to match bottom
            }
            else
            {
                anchors.m_bottom = anchors.m_top;   // clamp bottom to equal top
            }
        }
        else
        {
            // top changed or both changed
            if (allowPush)
            {
                anchors.m_bottom = anchors.m_top;   // push bottom to match top
            }
            else
            {
                anchors.m_top = anchors.m_bottom;   // clamp top to equal bottom
            }
        }
    }

    if (adjustOffsets)
    {
        // now we need to adjust the offsets
        AZ::Entity* parentElement = nullptr;
        EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
        if (parentElement)
        {
            AZ::Vector2 parentSize;
            EBUS_EVENT_ID_RESULT(parentSize, parentElement->GetId(), UiTransformBus, GetCanvasSpaceSizeNoScaleRotate);

            m_offsets.m_left    -= parentSize.GetX() * (anchors.m_left - m_anchors.m_left);
            m_offsets.m_right   -= parentSize.GetX() * (anchors.m_right - m_anchors.m_right);
            m_offsets.m_top     -= parentSize.GetY() * (anchors.m_top - m_anchors.m_top);
            m_offsets.m_bottom  -= parentSize.GetY() * (anchors.m_bottom - m_anchors.m_bottom);
        }
    }

    // now actually change the anchors
    m_anchors = anchors;

    // now, if the anchors are the same in a dimension we check that the offsets are not flipped in that dimension
    // if they are we set them to be zero apart. This is a rule when the anchors are together in order to prevent
    // displaying a negative width or height
    if (m_anchors.m_left == m_anchors.m_right && m_offsets.m_left > m_offsets.m_right)
    {
        // left and right offsets are flipped, set to their midpoint
        m_offsets.m_left = m_offsets.m_right = (m_offsets.m_left + m_offsets.m_right) * 0.5f;
    }
    if (m_anchors.m_top == m_anchors.m_bottom && m_offsets.m_top > m_offsets.m_bottom)
    {
        // top and bottom offsets are flipped, set to their midpoint
        m_offsets.m_top = m_offsets.m_bottom = (m_offsets.m_top + m_offsets.m_bottom) * 0.5f;
    }

    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiTransform2dComponent::Offsets UiTransform2dComponent::GetOffsets()
{
    return m_offsets;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetOffsets(Offsets offsets)
{
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (!parentElement)
    {
        return; // cannot set offsets on the root element
    }

    // first adjust the input structure to be valid
    // if either pair of offsets is flipped then set them to be the same
    // to avoid changing one offset "pushing" the other we check which one changed and correct that
    // NOTE: To see if an offset is flipped we have to take into account all the parents, the calculation
    // below is based on the calculation in GetCanvasSpaceRectNoScaleRotate but needs to be able to do
    // it in reverse also
    // NOTE: if a parent changes size this can cause offsets to flip and this is OK - we treat it as a zero
    // rect in that dimension in GetCanvasSpaceRectNoScaleRotate. But if the offsets on this element are
    // being changed then we do enforce the "no flipping" rule.

    Rect parentRect;
    EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetCanvasSpaceRectNoScaleRotate, parentRect);

    AZ::Vector2 parentSize = parentRect.GetSize();

    float left   = parentRect.left + parentSize.GetX() * m_anchors.m_left   + offsets.m_left;
    float right  = parentRect.left + parentSize.GetX() * m_anchors.m_right  + offsets.m_right;
    float top    = parentRect.top  + parentSize.GetY() * m_anchors.m_top    + offsets.m_top;
    float bottom = parentRect.top  + parentSize.GetY() * m_anchors.m_bottom + offsets.m_bottom;

    if (left > right)
    {
        // left/right offsets are flipped
        bool leftChanged = offsets.m_left != m_offsets.m_left;
        bool rightChanged = offsets.m_right != m_offsets.m_right;

        if (leftChanged && rightChanged)
        {
            // Both changed. This usually happens when resizing by gizmo, which is about the pivot.
            // So rather than taking the midpoint (which the below calculation effectively does for the normal
            // case of pivot.GetX() = 0.5f) we take the point between the two values using the pivot as a ratio.
            // This makes sense even if not resizing by gizmo. When the width is zero the pivot position
            // is always co-incident with the left and right edges. So this calculation moves the two points
            // together without moving the pivot position.
            float newValue = left * (1.0f - m_pivot.GetX()) + right * m_pivot.GetX();
            offsets.m_left  = newValue - (parentRect.left + parentSize.GetX() * m_anchors.m_left);
            offsets.m_right = newValue - (parentRect.left + parentSize.GetX() * m_anchors.m_right);
        }
        else if (rightChanged)
        {
            // the right offset changed, correct that one
            offsets.m_right = left - (parentRect.left + parentSize.GetX() * m_anchors.m_right);
        }
        else if (leftChanged)
        {
            // the left offset changed, correct that one
            offsets.m_left = right - (parentRect.left + parentSize.GetX() * m_anchors.m_left);
        }
    }

    if (top > bottom)
    {
        // top/bottom offsets are flipped
        bool topChanged = offsets.m_top != m_offsets.m_top;
        bool bottomChanged = offsets.m_bottom != m_offsets.m_bottom;

        if (topChanged && bottomChanged)
        {
            // Both changed. This usually happens when resizing by gizmo, which is about the pivot.
            // So rather than taking the midpoint (which the below calculation effectively does for the normal
            // case of pivot.GetY() = 0.5f) we take the point between the two values using the pivot as a ratio.
            float newValue = top * (1.0f - m_pivot.GetY()) + bottom * m_pivot.GetY();
            offsets.m_top    = newValue - (parentRect.top + parentSize.GetY() * m_anchors.m_top);
            offsets.m_bottom = newValue - (parentRect.top + parentSize.GetY() * m_anchors.m_bottom);
        }
        else if (bottomChanged)
        {
            // the bottom offset changed, correct that one
            offsets.m_bottom = top - (parentRect.top + parentSize.GetY() * m_anchors.m_bottom);
        }
        else if (topChanged)
        {
            // the top offset changed, correct that one
            offsets.m_top = bottom - (parentRect.top + parentSize.GetY() * m_anchors.m_top);
        }
    }

    m_offsets = offsets;

    SetRecomputeTransformFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::SetPivotAndAdjustOffsets(AZ::Vector2 pivot)
{
    // if the element has local rotation or scale then we have to modify the offsets to keep the rect from moving
    // in transformed space.
    if (HasScaleOrRotation())
    {
        // Get the untransformed canvas space rect
        RectPoints oldCanvasSpacePoints;
        GetCanvasSpacePointsNoScaleRotate(oldCanvasSpacePoints);

        // apply just this elements rotate and scale (must be done before changing pivot)
        // NOTE: this element's pivot only affects the local transformation so that is no need to apply all the
        // transforms up the hierarchy.
        AZ::Matrix4x4 localTransform;
        GetLocalTransform(localTransform);
        RectPoints localTransformedPoints = oldCanvasSpacePoints.Transform(localTransform);

        // Set the new pivot
        SetPivot(pivot);

        // Now the pivot has changed we want to get the inverse local transform which will rotate/scale around new pivot
        // to get back to a new untransformed canvas space rect - which we can then use to calculate the new offsets.
        // However we cannot use GetLocalInverseTransform because that works out the canvas space pivot using the
        // existing untransformed rect. The input pivot point is a ratio between the transformed points.
        // So this code below is a modification of GetLocalInverseTransform that allows for that.
        AZ::Matrix4x4 localInverseTransform;
        {
            // Get the pivot point using the transformed rect.
            AZ::Vector2 rightVec = localTransformedPoints.TopRight() - localTransformedPoints.TopLeft();
            AZ::Vector2 downVec = localTransformedPoints.BottomLeft() - localTransformedPoints.TopLeft();
            AZ::Vector2 canvasSpacePivot = localTransformedPoints.TopLeft() + pivot.GetX() * rightVec + pivot.GetY() * downVec;

            AZ::Vector2 scale = GetScaleAdjustedForDevice();

            GetInverseTransform(canvasSpacePivot, scale, m_rotation, localInverseTransform);
        }

        // get the new untransformed canvas space points
        RectPoints newCanvasSpacePoints = localTransformedPoints.Transform(localInverseTransform);

        // we could work out the offsets using the reverse of the calculation in GetCanvasSpacePointsNoScaleRotate
        // but is easier to just use the delta between the old untransformed points and the new ones
        m_offsets.m_left += newCanvasSpacePoints.TopLeft().GetX() - oldCanvasSpacePoints.TopLeft().GetX();
        m_offsets.m_right += newCanvasSpacePoints.BottomRight().GetX() - oldCanvasSpacePoints.BottomRight().GetX();
        m_offsets.m_top += newCanvasSpacePoints.TopLeft().GetY() - oldCanvasSpacePoints.TopLeft().GetY();
        m_offsets.m_bottom += newCanvasSpacePoints.BottomRight().GetY() - oldCanvasSpacePoints.BottomRight().GetY();

        SetRecomputeTransformFlag();
    }
    else
    {
        // no scale or rotation, just set the pivot
        SetPivot(pivot);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC STATIC MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

void UiTransform2dComponent::Reflect(AZ::ReflectContext* context)
{
    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

    if (serializeContext)
    {
        serializeContext->Class<UiTransform2dComponent, AZ::Component>()
            ->Version(2, &VersionConverter)
            ->Field("Anchors", &UiTransform2dComponent::m_anchors)
            ->Field("Offsets", &UiTransform2dComponent::m_offsets)
            ->Field("Pivot", &UiTransform2dComponent::m_pivot)
            ->Field("Rotation", &UiTransform2dComponent::m_rotation)
            ->Field("Scale", &UiTransform2dComponent::m_scale)
            ->Field("ScaleToDevice", &UiTransform2dComponent::m_scaleToDevice);

        // EditContext. Note that the Transform component is unusual in that we want to hide the
        // properties when the transform is controlled by the parent. There is not a standard
        // way to hide all the properties and replace them by a message. We could hide them all
        // using the "Visibility" attribute, but then the component name itself is not even shown.
        // We really want to be able to display a message indicating why the properties are not shown.
        // Alternatively we could make them all read-only using the "ReadOnly" property. Again this
        // doesn't tell the user why.
        // So the approach we use is:
        // - Hide all of the properties excepth Anchors using the "Visibility" property
        // - Set the Anchors property to ReadOnly and change the ProertyHandler for Anchors to
        //   display a message in this case (and have a different tooltip)
        // - Dynanically change the property name of the Anchors property using the
        //   "NameLabelOverride" attribute.
        AZ::EditContext* ec = serializeContext->GetEditContext();
        if (ec)
        {
            auto editInfo = ec->Class<UiTransform2dComponent>("Transform2D",
                    "All 2D UI elements have this component.\n"
                    "It controls the placement of the element's rectangle relative to its parent");

            editInfo->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/UiTransform2d.png")
                ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/UiTransform2d.png")
                ->Attribute(AZ::Edit::Attributes::AddableByUser, false)     // Cannot be added or removed by user
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement("Anchor", &UiTransform2dComponent::m_anchors, "Anchors",
                "The anchors specify proportional positions within the parent element's rectangle.\n"
                "If the anchors are together (e.g. left = right or top = bottom) then, in that dimension,\n"
                "there is a single anchor point that the element is offset from.\n"
                "If they are apart, then there are two anchor points and as the parent changes size\n"
                "this element will change size also")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshValues", 0x28e720d4))
                ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                ->Attribute(AZ::Edit::Attributes::Max, 100.0f)
                ->Attribute(AZ::Edit::Attributes::Step, 1.0f)
                ->Attribute(AZ::Edit::Attributes::Suffix, "%")
                ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Show) // needed because sub-elements are hidden
                ->Attribute(AZ::Edit::Attributes::ReadOnly, &UiTransform2dComponent::IsControlledByParent)
                ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &UiTransform2dComponent::GetAnchorPropertyLabel);

            editInfo->DataElement("Offset", &UiTransform2dComponent::m_offsets, "Offsets",
                "The offsets (in pixels) from the anchors.\n"
                "When anchors are together, the offset to the pivot plus the size is displayed.\n"
                "When they are apart, the offsets to each edge of the element's rect are displayed")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshValues", 0x28e720d4))
                ->Attribute(AZ::Edit::Attributes::Visibility, &UiTransform2dComponent::IsNotControlledByParent);

            editInfo->DataElement("Pivot", &UiTransform2dComponent::m_pivot, "Pivot",
                "Rotation and scaling happens around the pivot point.\n"
                "If the anchors are together then the offsets specify the offset from the anchor to the pivot")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshValues", 0x28e720d4))
                ->Attribute(AZ::Edit::Attributes::Step, 0.1f);

            editInfo->DataElement(AZ::Edit::UIHandlers::SpinBox, &UiTransform2dComponent::m_rotation, "Rotation",
                "The rotation in degrees about the pivot point")
                ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                ->Attribute(AZ::Edit::Attributes::Suffix, " degrees")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, &UiTransform2dComponent::SetRecomputeTransformFlag);

            editInfo->DataElement(0, &UiTransform2dComponent::m_scale, "Scale",
                "The X and Y scale around the pivot point")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, &UiTransform2dComponent::SetRecomputeTransformFlag);

            editInfo->DataElement("CheckBox", &UiTransform2dComponent::m_scaleToDevice, "Scale to device",
                "If checked, at runtime, this element and all its children will be scaled to allow for\n"
                "the difference between the authored canvas size and the actual viewport size")
                ->Attribute(AZ::Edit::Attributes::Visibility, &UiTransform2dComponent::IsNotControlledByParent);
        }
    }

    AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
    if (behaviorContext)
    {
        behaviorContext->EBus<UiTransformBus>("UiTransformBus")
            ->Event("GetZRotation", &UiTransformBus::Events::GetZRotation)
            ->Event("SetZRotation", &UiTransformBus::Events::SetZRotation)
            ->Event("GetScale", &UiTransformBus::Events::GetScale)
            ->Event("SetScale", &UiTransformBus::Events::SetScale)
            ->Event("GetPivot", &UiTransformBus::Events::GetPivot)
            ->Event("SetPivot", &UiTransformBus::Events::SetPivot)
            ->Event("GetScaleToDevice", &UiTransformBus::Events::GetScaleToDevice)
            ->Event("SetScaleToDevice", &UiTransformBus::Events::SetScaleToDevice)
            ->Event("GetViewportPosition", &UiTransformBus::Events::GetViewportPosition)
            ->Event("SetViewportPosition", &UiTransformBus::Events::SetViewportPosition)
            ->Event("GetCanvasPosition", &UiTransformBus::Events::GetCanvasPosition)
            ->Event("SetCanvasPosition", &UiTransformBus::Events::SetCanvasPosition)
            ->Event("GetLocalPosition", &UiTransformBus::Events::GetLocalPosition)
            ->Event("SetLocalPosition", &UiTransformBus::Events::SetLocalPosition)
            ->Event("MoveViewportPositionBy", &UiTransformBus::Events::MoveViewportPositionBy)
            ->Event("MoveCanvasPositionBy", &UiTransformBus::Events::MoveCanvasPositionBy)
            ->Event("MoveLocalPositionBy", &UiTransformBus::Events::MoveLocalPositionBy);

        behaviorContext->EBus<UiTransform2dBus>("UiTransform2dBus")
            ->Event("GetAnchors", &UiTransform2dBus::Events::GetAnchors)
            ->Event("SetAnchors", &UiTransform2dBus::Events::SetAnchors)
            ->Event("GetOffsets", &UiTransform2dBus::Events::GetOffsets)
            ->Event("SetOffsets", &UiTransform2dBus::Events::SetOffsets)
            ->Event("SetPivotAndAdjustOffsets", &UiTransform2dBus::Events::SetPivotAndAdjustOffsets);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PROTECTED MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::Activate()
{
    UiTransformBus::Handler::BusConnect(m_entity->GetId());
    UiTransform2dBus::Handler::BusConnect(m_entity->GetId());
    UiAnimateEntityBus::Handler::BusConnect(m_entity->GetId());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::Deactivate()
{
    UiTransformBus::Handler::BusDisconnect();
    UiTransform2dBus::Handler::BusDisconnect();
    UiAnimateEntityBus::Handler::BusDisconnect();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::IsControlledByParent() const
{
    bool isControlledByParent = false;

    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        EBUS_EVENT_ID_RESULT(isControlledByParent, parentElement->GetId(), UiLayoutBus, IsControllingChild, GetEntityId());
    }

    return isControlledByParent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::IsNotControlledByParent() const
{
    return !IsControlledByParent();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* UiTransform2dComponent::GetAnchorPropertyLabel() const
{
    const char* label = "Anchors";

    if (IsControlledByParent())
    {
        label = "Disabled";
    }

    return label;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiTransform2dComponent::GetCanvasEntityId()
{
    AZ::EntityId canvasEntityId;
    EBUS_EVENT_ID_RESULT(canvasEntityId, GetEntityId(), UiElementBus, GetCanvasEntityId);
    return canvasEntityId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetScaleAdjustedForDevice()
{
    AZ::Vector2 scale = m_scale;

    if (m_scaleToDevice)
    {
        float uniformDeviceScale = 1.0f;
        EBUS_EVENT_ID_RESULT(uniformDeviceScale, GetCanvasEntityId(), UiCanvasBus, GetUniformDeviceScale);
        scale *= AZ::Vector2(uniformDeviceScale, uniformDeviceScale);
    }

    return scale;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::CalculateCanvasSpaceRect()
{
    if (!m_recomputeCanvasSpaceRect)
    {
        return;
    }

    Rect rect;

    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, GetEntityId(), UiElementBus, GetParent);
    if (parentElement)
    {
        Rect parentRect;

        EBUS_EVENT_ID(parentElement->GetId(), UiTransformBus, GetCanvasSpaceRectNoScaleRotate, parentRect);

        AZ::Vector2 parentSize = parentRect.GetSize();

        float left = parentRect.left + parentSize.GetX() * m_anchors.m_left + m_offsets.m_left;
        float right = parentRect.left + parentSize.GetX() * m_anchors.m_right + m_offsets.m_right;
        float top = parentRect.top + parentSize.GetY() * m_anchors.m_top + m_offsets.m_top;
        float bottom = parentRect.top + parentSize.GetY() * m_anchors.m_bottom + m_offsets.m_bottom;

        rect.Set(left, right, top, bottom);
    }
    else
    {
        // this is the root element, it's offset and anchors are ignored
        AZ::Vector2 size;
        EBUS_EVENT_ID_RESULT(size, GetCanvasEntityId(), UiCanvasBus, GetCanvasSize);

        rect.Set(0.0f, size.GetX(), 0.0f, size.GetY());
    }

    // we never return a "flipped" rect. I.e. left is always less than right, top is always less than bottom
    // if it is flipped in a dimension then we make it zero size in that dimension
    if (rect.left > rect.right)
    {
        rect.left = rect.right = rect.GetCenterX();
    }
    if (rect.top > rect.bottom)
    {
        rect.top = rect.bottom = rect.GetCenterY();
    }

    m_rect = rect;
    if (!m_rectInitialized)
    {
        m_prevRect = m_rect;
        m_rectChangedByInitialization = true;
        m_rectInitialized = true;
    }
    else
    {
        // If the rect is being changed after it was initialized, but before the first
        // update, keep prev rect in sync with current rect. On a canvas space rect
        // change callback, prev rect and current rect can be used to determine whether
        // the canvas rect size has changed. Equal rects implies a change due to initialization 
        if (m_rectChangedByInitialization)
        {
            m_prevRect = m_rect;
        }
    }
    m_recomputeCanvasSpaceRect = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiTransform2dComponent::GetCanvasSpaceAnchorsCenterNoScaleRotate()
{
    // Get the position of the element's anchors in canvas space
    AZ::Entity* parentEntity = nullptr;
    EBUS_EVENT_ID_RESULT(parentEntity, GetEntityId(), UiElementBus, GetParent);

    if (!parentEntity)
    {
        return AZ::Vector2(0.0f, 0.0f); // this is the root element
    }

    // Get parent's rect in canvas space
    UiTransformInterface::Rect parentRect;
    EBUS_EVENT_ID(parentEntity->GetId(), UiTransformBus, GetCanvasSpaceRectNoScaleRotate, parentRect);

    // Get the anchor center in canvas space
    UiTransformInterface::Rect anchorRect;
    anchorRect.left = parentRect.left + m_anchors.m_left * parentRect.GetWidth();
    anchorRect.right = parentRect.left + m_anchors.m_right * parentRect.GetWidth();
    anchorRect.top = parentRect.top + m_anchors.m_top * parentRect.GetHeight();
    anchorRect.bottom = parentRect.top + m_anchors.m_bottom * parentRect.GetHeight();

    return anchorRect.GetCenter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiTransform2dComponent::PropertyValuesChanged()
{
    EBUS_EVENT_ID(GetEntityId(), UiTransformBus, SetRecomputeTransformFlag);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiTransform2dComponent::VersionConverter(AZ::SerializeContext& context,
    AZ::SerializeContext::DataElementNode& classElement)
{
    // conversion from version 1:
    // - Need to convert Vec2 to AZ::Vector2
    if (classElement.GetVersion() <= 1)
    {
        if (!LyShine::ConvertSubElementFromVec2ToVector2(context, classElement, "Pivot"))
        {
            return false;
        }

        if (!LyShine::ConvertSubElementFromVec2ToVector2(context, classElement, "Scale"))
        {
            return false;
        }
    }

    return true;
}
