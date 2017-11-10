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

#ifndef CRYINCLUDE_EDITOR_CONTROLS_WNDGRIDHELPER_H
#define CRYINCLUDE_EDITOR_CONTROLS_WNDGRIDHELPER_H
#pragma once

//////////////////////////////////////////////////////////////////////////
class CWndGridHelper
{
public:
    Vec2 zoom;
    Vec2 origin;
    Vec2 step;
    Vec2 pixelsPerGrid;
    int nMajorLines;
    QRect rect;
    QPoint nMinPixelsPerGrid;
    QPoint nMaxPixelsPerGrid;
    //////////////////////////////////////////////////////////////////////////
    QPoint firstGridLine;
    QPoint numGridLines;

    //////////////////////////////////////////////////////////////////////////
    CWndGridHelper()
    {
        zoom = Vec2(1, 1);
        step = Vec2(10, 10);
        pixelsPerGrid = Vec2(10, 10);
        origin = Vec2(0, 0);
        nMajorLines = 10;
        nMinPixelsPerGrid = QPoint(50, 10);
        nMaxPixelsPerGrid = QPoint(100, 20);
        firstGridLine = QPoint(0, 0);
        numGridLines = QPoint(0, 0);
    }

    //////////////////////////////////////////////////////////////////////////
    Vec2 ClientToWorld(const QPoint& point)
    {
        Vec2 v;
        v.x = (point.x() - rect.left()) / zoom.x + origin.x;
        v.y = (point.y() - rect.top()) / zoom.y + origin.y;
        return v;
    }
    //////////////////////////////////////////////////////////////////////////
   QPoint WorldToClient(Vec2 v)
    {
        QPoint p(floor((v.x - origin.x) * zoom.x + 0.5f) + rect.left(),
            floor((v.y - origin.y) * zoom.y + 0.5f) + rect.top());
        return p;
    }

    void SetOrigin(Vec2 neworigin)
    {
        origin = neworigin;
    }
    void SetZoom(Vec2 newzoom)
    {
        zoom = newzoom;
    }
    //////////////////////////////////////////////////////////////////////////
    void SetZoom(Vec2 newzoom, const QPoint& center)
    {
        if (newzoom.x < 0.01f)
        {
            newzoom.x = 0.01f;
        }
        if (newzoom.y < 0.01f)
        {
            newzoom.y = 0.01f;
        }

        Vec2 prevz = zoom;

        // Zoom to mouse position.
        float ofsx = origin.x;
        float ofsy = origin.y;

        Vec2 z1 = zoom;
        Vec2 z2 = newzoom;

        zoom = newzoom;

        // Calculate new offset to center zoom on mouse.
        float x2 = center.x() - rect.left();
        float y2 = center.y() - rect.top();
        ofsx = -(x2 / z2.x - x2 / z1.x - ofsx);
        ofsy = -(y2 / z2.y - y2 / z1.y - ofsy);
        origin.x = ofsx;
        origin.y = ofsy;
    }
    void CalculateGridLines()
    {
        pixelsPerGrid.x = zoom.x;
        pixelsPerGrid.y = zoom.y;

        step = Vec2(1.00f, 1.00f);
        nMajorLines = 2;

        int griditers;
        if (pixelsPerGrid.x <= nMinPixelsPerGrid.x())
        {
            griditers = 0;
            while (pixelsPerGrid.x <= nMinPixelsPerGrid.x() && griditers++ < 1000)
            {
                step.x = step.x * nMajorLines;
                pixelsPerGrid.x = step.x * zoom.x;
            }
        }
        else
        {
            griditers = 0;
            while (pixelsPerGrid.x >= nMaxPixelsPerGrid.x() && griditers++ < 1000)
            {
                step.x = step.x / nMajorLines;
                pixelsPerGrid.x = step.x * zoom.x;
            }
        }

        if (pixelsPerGrid.y <= nMinPixelsPerGrid.y())
        {
            griditers = 0;
            while (pixelsPerGrid.y <= nMinPixelsPerGrid.y() && griditers++ < 1000)
            {
                step.y = step.y * nMajorLines;
                pixelsPerGrid.y = step.y * zoom.y;
            }
        }
        else
        {
            griditers = 0;
            while (pixelsPerGrid.y >= nMaxPixelsPerGrid.y() && griditers++ < 1000)
            {
                step.y = step.y / nMajorLines;
                pixelsPerGrid.y = step.y * zoom.y;
            }
        }

        firstGridLine.rx() = origin.x / step.x;
        firstGridLine.ry() = origin.y / step.y;

        numGridLines.rx() = (rect.width() / zoom.x) / step.x + 1;
        numGridLines.ry() = (rect.height() / zoom.y) / step.y + 1;
    }
    int GetGridLineX(int nGridLineX) const
    {
        return floor((nGridLineX * step.x - origin.x) * zoom.x + 0.5f);
    }
    int GetGridLineY(int nGridLineY) const
    {
        return floor((nGridLineY * step.y - origin.y) * zoom.y + 0.5f);
    }
    float GetGridLineXValue(int nGridLineX) const
    {
        return (nGridLineX * step.x);
    }
    float GetGridLineYValue(int nGridLineY) const
    {
        return (nGridLineY * step.y);
    }
};

#endif // CRYINCLUDE_EDITOR_CONTROLS_WNDGRIDHELPER_H
