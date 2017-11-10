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
#include "ImagePainter.h"
#include "Image.h"
#include "./Terrain/Heightmap.h"                // CHeightmap for mask computations
#include "./Terrain/Layer.h"                        // CLayer for mask computations

SEditorPaintBrush::SEditorPaintBrush(CHeightmap& rHeightmap, CLayer& rLayer,
    const bool bMaskByLayerSettings, const uint32 dwLayerIdMask, const bool bFlood)
    : bBlended(true)
    , m_rHeightmap(rHeightmap)
    , m_rLayer(rLayer)
    , m_cFilterColor(1, 1, 1)
    , m_dwLayerIdMask(dwLayerIdMask)
    , m_bFlood(bFlood)
{
    if (bMaskByLayerSettings)
    {
        m_fMinAltitude = m_rLayer.GetLayerStart();
        m_fMaxAltitude = m_rLayer.GetLayerEnd();
        m_fMinSlope = tan(m_rLayer.GetLayerMinSlopeAngle() / 90.1f * g_PI / 2.0f);    // 0..90 -> 0..~1/0
        m_fMaxSlope = tan(m_rLayer.GetLayerMaxSlopeAngle() / 90.1f * g_PI / 2.0f);    // 0..90 -> 0..~1/0
    }
    else
    {
        m_fMinAltitude = -FLT_MAX;
        m_fMaxAltitude = FLT_MAX;
        m_fMinSlope = 0;
        m_fMaxSlope = FLT_MAX;
    }
}

float SEditorPaintBrush::GetMask(const float fX, const float fY) const
{
    int iX = (int)(fX * (m_rHeightmap.GetWidth() - 1) + 0.5f), iY = (int)(fY * (m_rHeightmap.GetHeight() - 1) + 0.5f);

    float fAltitude = m_rHeightmap.GetZInterpolated(fX * m_rHeightmap.GetWidth(), fY * m_rHeightmap.GetHeight());

    // Check if altitude is within brush min/max altitude
    if (fAltitude < m_fMinAltitude || fAltitude > m_fMaxAltitude)
    {
        return 0;
    }

    float fSlope = m_rHeightmap.GetAccurateSlope(fX * m_rHeightmap.GetWidth(), fY * m_rHeightmap.GetHeight());

    // Check if slope is within brush min/max slope
    if (fSlope < m_fMinSlope || fSlope > m_fMaxSlope)
    {
        return 0;
    }

    //  Soft slope test
    //  float fSlopeAplha = 1.f;
    //  fSlopeAplha *= CLAMP((m_fMaxSlope-fSlope)*4 + 0.25f,0,1);
    //  fSlopeAplha *= CLAMP((fSlope-m_fMinSlope)*4 + 0.25f,0,1);

    if (m_dwLayerIdMask != 0xffffffff)
    {
        LayerWeight weight = m_rHeightmap.GetLayerWeightAt(iX, iY);

        if ((weight.PrimaryId() & CLayer::e_undefined) != m_dwLayerIdMask)
        {
            return 0;
        }
    }

    return 1;
}


//////////////////////////////////////////////////////////////////////////
void CImagePainter::PaintBrush(const float fpx, const float fpy, TImage<LayerWeight>& image, const SEditorPaintBrush& brush)
{
    float fX = fpx * image.GetWidth(), fY = fpy * image.GetHeight();

    const float fScaleX = 1.0f / image.GetWidth();
    const float fScaleY = 1.0f / image.GetHeight();

    ////////////////////////////////////////////////////////////////////////
    // Draw an attenuated spot on the map
    ////////////////////////////////////////////////////////////////////////
    float fMaxDist, fAttenuation, fYSquared;
    float fHardness = brush.hardness;

    unsigned int pos;

    LayerWeight* sourceData = image.GetData();

    // Calculate the maximum distance
    fMaxDist = brush.fRadius * image.GetWidth();

    assert(image.GetWidth() == image.GetHeight());

    int width = image.GetWidth();
    int height = image.GetHeight();

    int iMinX = (int)floor(fX - fMaxDist), iMinY = (int)floor(fY - fMaxDist);
    int iMaxX = (int)ceil(fX + fMaxDist), iMaxY = (int)ceil(fY + fMaxDist);

    for (int iPosY = iMinY; iPosY <= iMaxY; iPosY++)
    {
        // Skip invalid locations
        if (iPosY < 0 || iPosY > height - 1)
        {
            continue;
        }

        float fy = (float)iPosY - fY;

        // Precalculate
        fYSquared = (float)(fy * fy);

        for (int iPosX = iMinX; iPosX <= iMaxX; iPosX++)
        {
            float fx = (float)iPosX - fX;

            // Skip invalid locations
            if (iPosX < 0 || iPosX > width - 1)
            {
                continue;
            }

            // Only circle.
            float dist = sqrtf(fYSquared + fx * fx);
            if (!brush.m_bFlood && dist > fMaxDist)
            {
                continue;
            }

            float fMask = brush.GetMask(iPosX * fScaleX, iPosY * fScaleY);

            if (fMask < 0.5f)
            {
                continue;
            }

            // Calculate the array index
            pos = iPosX + iPosY * width;

            // Calculate attenuation factor
            fAttenuation = brush.m_bFlood ? 1.0f : 1.0f - __min(1.0f, dist / fMaxDist);

            float h = static_cast<float>(sourceData[pos].GetWeight(brush.color) / 255.0f);
            float dh = 1.0f - h;
            float fh = clamp_tpl((fAttenuation) * dh * fHardness + h, 0.0f, 1.0f);

            sourceData[pos].SetWeight(brush.color, static_cast<uint8>(fh * 255.0f));
        }
    }
}


void CImagePainter::PaintBrushWithPattern(const float fpx, const float fpy, CImageEx& outImage,
    const uint32 dwOffsetX, const uint32 dwOffsetY, const float fScaleX, const float fScaleY,
    const SEditorPaintBrush& brush, const CImageEx& imgPattern)
{
    float fX = fpx * fScaleX, fY = fpy * fScaleY;

    ////////////////////////////////////////////////////////////////////////
    // Draw an attenuated spot on the map
    ////////////////////////////////////////////////////////////////////////
    float fMaxDist, fAttenuation, fYSquared;
    float fHardness = brush.hardness;

    unsigned int pos;

    uint32* src = outImage.GetData();
    uint32* pat = imgPattern.GetData();

    int value = brush.color;

    // Calculate the maximum distance
    fMaxDist = brush.fRadius;

    int width = outImage.GetWidth();
    int height = outImage.GetHeight();

    int patwidth = imgPattern.GetWidth();
    int patheight = imgPattern.GetHeight();

    int iMinX = (int)floor(fX - fMaxDist), iMinY = (int)floor(fY - fMaxDist);
    int iMaxX = (int)ceil(fX + fMaxDist), iMaxY = (int)ceil(fY + fMaxDist);

    bool bSRGB = imgPattern.GetSRGB();

    for (int iPosY = iMinY; iPosY < iMaxY; iPosY++)
    {
        // Skip invalid locations
        if (iPosY - dwOffsetY < 0 || iPosY - dwOffsetY > height - 1)
        {
            continue;
        }

        float fy = (float)iPosY - fY;

        // Precalculate
        fYSquared = (float)(fy * fy);

        int32 iPatY = ((uint32)iPosY) % patheight;
        assert(iPatY >= 0 && iPatY < patheight);

        for (int iPosX = iMinX; iPosX < iMaxX; iPosX++)
        {
            float fx = (float)iPosX - fX;

            // Skip invalid locations
            if (iPosX - dwOffsetX < 0 || iPosX - dwOffsetX > width - 1)
            {
                continue;
            }

            // Only circle.
            float dist = sqrtf(fYSquared + fx * fx);

            if (!brush.m_bFlood && dist > fMaxDist)
            {
                continue;
            }

            // Calculate the array index
            pos = (iPosX - dwOffsetX) + (iPosY - dwOffsetY) * width;

            // Calculate attenuation factor
            fAttenuation = brush.m_bFlood ? 1.0f : 1.0f - __min(1.0f, dist / fMaxDist);
            assert(fAttenuation >= 0.0f && fAttenuation <= 1.0f);

            float fMask = brush.GetMask(iPosX / fScaleX, iPosY / fScaleX);

            uint32 cDstPix = src[pos];

            int32 iPatX = ((uint32)iPosX) % patwidth;
            assert(iPatX >= 0 && iPatX < patwidth);

            uint32 cSrcPix = pat[iPatX + iPatY * patwidth];

            float s = fAttenuation * fHardness * fMask;
            assert(s >= 0.0f && s <= 1.0f);
            if (fcmp(s, 0))
            {
                // If the blend would be entirely biased to the pixel in outImage then don't modify anything
                // (The logic below is susceptible to floating point inaccuracy and would change the pixel
                // even though it is not supposed to)
                continue;
            }

            const float fRecip255 = 1.0f / 255.0f;

            // Convert Src to Linear Space (Src is pattern texture, can be in linear or gamma space)
            ColorF cSrc = ColorF((cSrcPix & 0x0ff), (cSrcPix & 0x0ff00) >> 8, (cSrcPix & 0x0ff0000) >> 16) * fRecip255;
            if (bSRGB)
            {
                cSrc.srgb2rgb();
            }

            ColorF cMtl = brush.m_cFilterColor;
            cMtl.srgb2rgb();

            cSrc *= cMtl;
            cSrc.clamp(0.0f, 1.0f);

            // Convert Dst to Linear Space ( Dst is always in gamma space )
            ColorF cDst = ColorF((cDstPix & 0x0ff0000) >> 16, (cDstPix & 0x0ff00) >> 8, (cDstPix & 0x0ff)) * fRecip255;
            cDst.srgb2rgb();

            // Linear space blend
            ColorF cOut = cSrc * s + cDst * (1.0f - s);

            // Convert final result to gamma space and put back in [0..255] range
            cOut.rgb2srgb();
            cOut *= 255.0f;

            src[pos] = (((uint32)cOut.r) << 16) | (((uint32)cOut.g) << 8) | ((uint32)cOut.b);
        }
    }
}


void CImagePainter::FillWithPattern(CImageEx& outImage, const uint32 dwOffsetX, const uint32 dwOffsetY,
    const CImageEx& imgPattern)
{
    unsigned int pos;

    uint32* src = outImage.GetData();
    uint32* pat = imgPattern.GetData();

    int width = outImage.GetWidth();
    int height = outImage.GetHeight();

    int patwidth = imgPattern.GetWidth();
    int patheight = imgPattern.GetHeight();

    if (patheight == 0 || patwidth == 0)
    {
        return;
    }

    for (int iPosY = 0; iPosY < height; iPosY++)
    {
        int32 iPatY = ((uint32)iPosY + dwOffsetY) % patheight;
        assert(iPatY >= 0 && iPatY < patheight);

        for (int iPosX = 0; iPosX < width; iPosX++)
        {
            // Calculate the array index
            pos = iPosX + iPosY * width;

            int32 iPatX = ((uint32)iPosX + dwOffsetX) % patwidth;
            assert(iPatX >= 0 && iPatX < patwidth);

            uint32 cSrc = pat[iPatX + iPatY * patwidth];

            float SrcR = (cSrc & 0x0ff);                  // RGB flipped
            float SrcG = (cSrc & 0x0ff00) >> 8;
            float SrcB = (cSrc & 0x0ff0000) >> 16;

            uint32 r = SrcR, g = SrcG, b = SrcB;

            src[pos] = (r << 16) | (g << 8) | b;
        }
    }
}

