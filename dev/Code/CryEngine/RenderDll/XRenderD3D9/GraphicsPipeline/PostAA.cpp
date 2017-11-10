/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "PostAA.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#include "DepthOfField.h"

struct TemporalAAParameters
{
    TemporalAAParameters() {}

    Matrix44 m_reprojection;

    // Index ordering
    // 5  2  6
    // 1  0  3
    // 7  4  8
    float m_beckmannHarrisFilter[9];
    float m_sharpeningFactor;
    float m_useAntiFlickerFilter;
    float m_clampingFactor;
    float m_newFrameWeight;
};

bool CPostAA::Preprocess()
{
    // Disable PostAA for Dolby mode.
    static ICVar* DolbyCvar = gEnv->pConsole->GetCVar("r_HDRDolby");
    int DolbyCvarValue = DolbyCvar ? DolbyCvar->GetIVal() : eDVM_Disabled;
    return DolbyCvarValue == eDVM_Disabled;
}

void CPostAA::Render()
{
    gcpRendD3D->GetGraphicsPipeline().RenderPostAA();
}

void PostAAPass::Init()
{
    m_TextureAreaSMAA = CTexture::ForName("EngineAssets/ScreenSpace/AreaTex.dds", FT_DONT_STREAM, eTF_Unknown);
    m_TextureSearchSMAA = CTexture::ForName("EngineAssets/ScreenSpace/SearchTex.dds", FT_DONT_STREAM, eTF_Unknown);
}

void PostAAPass::Shutdown()
{
    SAFE_RELEASE(m_TextureAreaSMAA);
    SAFE_RELEASE(m_TextureSearchSMAA);
}

void PostAAPass::Reset()
{
    
}

static bool IsTemporalRestartNeeded()
{
    // When we are activating a new viewport.
    static AZ::s32 s_LastViewportId = -1;
    if (gRenDev->m_CurViewportID != s_LastViewportId)
    {
        s_LastViewportId = gRenDev->m_CurViewportID;
        return true;
    }

    const AZ::s32 StaleFrameThresholdCount = 10;

    // When we exceed N frames without rendering TAA (e.g. we toggle it on and off).
    static AZ::s32 s_LastFrameCounter = 0;
    const bool bIsStale = (GetUtils().m_iFrameCounter - s_LastFrameCounter) > StaleFrameThresholdCount;
    s_LastFrameCounter = GetUtils().m_iFrameCounter;

    return bIsStale;
}

static float BlackmanHarris(Vec2 uv)
{
    return expf(-2.29f * (uv.x * uv.x + uv.y * uv.y));
}

static void BuildTemporalParameters(TemporalAAParameters& temporalAAParameters)
{
    Matrix44_tpl<f64> reprojection64;

    {
        Matrix44_tpl<f64> currViewProjMatrixInverse = Matrix44_tpl<f64>(gRenDev->m_ViewProjNoJitterMatrix).GetInverted();
        Matrix44_tpl<f64> prevViewProjMatrix = gRenDev->GetPreviousFrameMatrixSet().m_ViewProjMatrix;

        reprojection64 = currViewProjMatrixInverse * prevViewProjMatrix;

        Matrix44_tpl<f64> scaleBias1 = Matrix44_tpl<f64>(
            0.5, 0, 0, 0,
            0, -0.5, 0, 0,
            0, 0, 1, 0,
            0.5, 0.5, 0, 1);

        Matrix44_tpl<f64> scaleBias2 = Matrix44_tpl<f64>(
            2.0, 0, 0, 0,
            0, -2.0, 0, 0,
            0, 0, 1, 0,
            -1.0, 1.0, 0, 1);

        reprojection64 = scaleBias2 * reprojection64 * scaleBias1;
    }

    const size_t FILTER_WEIGHT_COUNT = 9;
    Vec2 filterWeights[FILTER_WEIGHT_COUNT] =
    {
        Vec2{  0.0f,  0.0f },
        Vec2{ -1.0f,  0.0f },
        Vec2{  0.0f, -1.0f },
        Vec2{  1.0f,  0.0f },
        Vec2{  0.0f,  1.0f },
        Vec2{ -1.0f, -1.0f },
        Vec2{  1.0f, -1.0f },
        Vec2{ -1.0f,  1.0f },
        Vec2{  1.0f,  1.0f }
    };

    const Vec2 temporalJitterOffset(gRenDev->m_TemporalJitterClipSpace.x * 0.5f, gRenDev->m_TemporalJitterClipSpace.y * 0.5f);
    for (size_t i = 0; i < FILTER_WEIGHT_COUNT; ++i)
    {
        temporalAAParameters.m_beckmannHarrisFilter[i] = BlackmanHarris((filterWeights[i] - temporalJitterOffset));
    }

    temporalAAParameters.m_reprojection = reprojection64;
    temporalAAParameters.m_sharpeningFactor = max(CRenderer::CV_r_AntialiasingTAASharpening + 1.0f, 1.0f);
    temporalAAParameters.m_useAntiFlickerFilter = (float)CRenderer::CV_r_AntialiasingTAAUseAntiFlickerFilter;
    temporalAAParameters.m_clampingFactor = CRenderer::CV_r_AntialiasingTAAClampingFactor;
    temporalAAParameters.m_newFrameWeight = 1.0f - expf(-CRenderer::GetElapsedTime() / max(gRenDev->CV_r_AntialiasingTAANewFrameFalloff, FLT_EPSILON));
}

void PostAAPass::RenderTemporalAA(
    CTexture* sourceTexture,
    CTexture* outputTarget,
    const DepthOfFieldParameters& depthOfFieldParameters)
{
    CShader* pShader = CShaderMan::s_shPostAA;
    PROFILE_LABEL_SCOPE("TAA");

    uint64 saveFlags_RT = gRenDev->m_RP.m_FlagsShader_RT;
    gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3]);

    if (CRenderer::CV_r_AntialiasingTAAUseVarianceClamping)
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
    }

    if (CRenderer::CV_r_HDREyeAdaptationMode == 2)
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
    }

    // Filter the CoC's when depth of field is enabled.
    if (depthOfFieldParameters.m_bEnabled)
    {
        gcpRendD3D->FX_PushRenderTarget(2, GetUtils().GetCoCCurrentTarget(), nullptr);

        GetUtils().SetTexture(GetUtils().GetCoCHistoryTarget(), 4, FILTER_LINEAR);

        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
    }

    if (IsTemporalRestartNeeded())
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];
    }

    CTexture* currentTarget = GetUtils().GetTemporalCurrentTarget();
    CTexture* historyTarget = GetUtils().GetTemporalHistoryTarget();

    gcpRendD3D->FX_PushRenderTarget(0, outputTarget, nullptr);
    gcpRendD3D->FX_PushRenderTarget(1, currentTarget, nullptr);

    static const CCryNameTSCRC TechNameTAA("TAA");
    GetUtils().ShBeginPass(pShader, TechNameTAA, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    Vec4 vHDRSetupParams[5];
    gEnv->p3DEngine->GetHDRSetupParams(vHDRSetupParams);

    {
        TemporalAAParameters temporalAAParameters;
        BuildTemporalParameters(temporalAAParameters);

        {
            const float sharpening = max(0.5f + CRenderer::CV_r_AntialiasingTAASharpening, 0.5f); // Catmull-rom sharpening baseline is 0.5.
            const float motionDifferenceMaxInverse = (float)outputTarget->GetWidth() / max(CRenderer::CV_r_AntialiasingTAAMotionDifferenceMax, FLT_EPSILON);
            const float motionDifferenceMaxWeight = clamp_tpl(CRenderer::CV_r_AntialiasingTAAMotionDifferenceMaxWeight, 0.0f, 1.0f);
            const float luminanceMax = max(CRenderer::CV_r_AntialiasingTAALuminanceMax, 0.0f);

            static CCryNameR paramName("TemporalParams");
            Vec4 temporalParams[4];
            temporalParams[0] = Vec4(
                temporalAAParameters.m_useAntiFlickerFilter,
                temporalAAParameters.m_clampingFactor,
                temporalAAParameters.m_newFrameWeight,
                sharpening);

            temporalParams[1] = Vec4(
                motionDifferenceMaxInverse,
                motionDifferenceMaxWeight,
                luminanceMax,
                temporalAAParameters.m_beckmannHarrisFilter[0]);

            temporalParams[2] = Vec4(
                temporalAAParameters.m_beckmannHarrisFilter[1],
                temporalAAParameters.m_beckmannHarrisFilter[2],
                temporalAAParameters.m_beckmannHarrisFilter[3],
                temporalAAParameters.m_beckmannHarrisFilter[4]);

            temporalParams[3] = Vec4(
                temporalAAParameters.m_beckmannHarrisFilter[5],
                temporalAAParameters.m_beckmannHarrisFilter[6],
                temporalAAParameters.m_beckmannHarrisFilter[7],
                temporalAAParameters.m_beckmannHarrisFilter[8]);

            pShader->FXSetPSFloat(paramName, (const Vec4*)temporalParams, 4);
        }

        static CCryNameR szReprojMatrix("ReprojectionMatrix");
        pShader->FXSetPSFloat(szReprojMatrix, (Vec4*)temporalAAParameters.m_reprojection.GetData(), 4);

        static CCryNameR HDREyeAdaptation("HDREyeAdaptation");
        pShader->FXSetPSFloat(HDREyeAdaptation, CRenderer::CV_r_HDREyeAdaptationMode == 2 ? &vHDRSetupParams[4] : &vHDRSetupParams[3], 1);

        static CCryNameR DOF_FocusParams0("DOF_FocusParams0");
        pShader->FXSetPSFloat(DOF_FocusParams0, &depthOfFieldParameters.m_FocusParams0, 1);

        static CCryNameR DOF_FocusParams1("DOF_FocusParams1");
        pShader->FXSetPSFloat(DOF_FocusParams1, &depthOfFieldParameters.m_FocusParams1, 1);
    }

    GetUtils().SetTexture(sourceTexture, 0, FILTER_POINT);
    GetUtils().SetTexture(historyTarget, 1, FILTER_LINEAR);

    if (CTexture::s_ptexCurLumTexture)
    {
        if (!gRenDev->m_CurViewportID)
        {
            GetUtils().SetTexture(CTexture::s_ptexCurLumTexture, 2, FILTER_LINEAR);
        }
        else
        {
            GetUtils().SetTexture(CTexture::s_ptexHDRToneMaps[0], 2, FILTER_LINEAR);
        }
    }

    GetUtils().SetTexture(GetUtils().GetVelocityObjectRT(), 3, FILTER_POINT);
    GetUtils().SetTexture(CTexture::s_ptexZTarget, 5, FILTER_POINT);

    D3DShaderResourceView* depthSRV[1] = { gcpRendD3D->m_pZBufferDepthReadOnlySRV };
    gcpRendD3D->m_DevMan.BindSRV(eHWSC_Pixel, depthSRV, 16, 1);
    gcpRendD3D->FX_Commit();

    SD3DPostEffectsUtils::DrawFullScreenTri(outputTarget->GetWidth(), outputTarget->GetHeight());

    depthSRV[0] = nullptr;
    gcpRendD3D->m_DevMan.BindSRV(eHWSC_Pixel, depthSRV, 16, 1);
    gcpRendD3D->FX_Commit();

    GetUtils().ShEndPass();

    gcpRendD3D->FX_PopRenderTarget(0);
    gcpRendD3D->FX_PopRenderTarget(1);

    if (depthOfFieldParameters.m_bEnabled)
    {
        gcpRendD3D->FX_PopRenderTarget(2);
    }

    gcpRendD3D->m_RP.m_PersFlags2 |= RBPF2_NOPOSTAA;
    gRenDev->m_RP.m_FlagsShader_RT = saveFlags_RT;
}

void PostAAPass::Execute()
{
    PROFILE_LABEL_SCOPE("POST_AA");
    PROFILE_SHADER_SCOPE;

    uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
    gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3]);

    CTexture* inOutBuffer = CTexture::s_ptexSceneSpecular;
    GetUtils().CopyScreenToTexture(inOutBuffer);

    switch (CRenderer::CV_r_AntialiasingMode)
    {
    case eAT_SMAA1TX:
        RenderSMAA(inOutBuffer, &inOutBuffer);
        break;
    case eAT_FXAA:
        RenderFXAA(inOutBuffer);
        break;
    case eAT_NOAA:
        break;
    }

    RenderComposites(inOutBuffer);

    gcpRendD3D->m_RP.m_PersFlags2 |= RBPF2_NOPOSTAA;
    CTexture::s_ptexBackBuffer->SetResolved(true);

    gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

void PostAAPass::RenderSMAA(CTexture* sourceTexture, CTexture** outputTexture)
{
    CTexture* pEdgesTex = CTexture::s_ptexSceneNormalsMap; // Reusing esram resident target
    CTexture* pBlendTex = CTexture::s_ptexSceneDiffuse;    // Reusing esram resident target (note that we access this FP16 RT using point filtering - full rate on GCN)

    CShader* pShader = CShaderMan::s_shPostAA;

    if (pEdgesTex && pBlendTex)
    {
        PROFILE_LABEL_SCOPE("SMAA1tx");
        const int iWidth = gcpRendD3D->GetWidth();
        const int iHeight = gcpRendD3D->GetHeight();

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // 1st pass: generate edges texture
        {
            PROFILE_LABEL_SCOPE("Edge Generation");
            gcpRendD3D->FX_ClearTarget(pEdgesTex, Clr_Transparent);
            gcpRendD3D->FX_PushRenderTarget(0, pEdgesTex, &gcpRendD3D->m_DepthBufferOrig);
            gcpRendD3D->FX_SetActiveRenderTargets();

            static CCryNameTSCRC pszLumaEdgeDetectTechName("LumaEdgeDetectionSMAA");
            static const CCryNameR pPostAAParams("PostAAParams");

            gcpRendD3D->RT_SetViewport(0, 0, iWidth, iHeight);

            GetUtils().ShBeginPass(pShader, pszLumaEdgeDetectTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

            gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
            GetUtils().BeginStencilPrePass(false, true);

            GetUtils().SetTexture(sourceTexture, 0, FILTER_POINT);
            SD3DPostEffectsUtils::DrawFullScreenTriWPOS(iWidth, iHeight);

            GetUtils().ShEndPass();

            GetUtils().EndStencilPrePass();

            gcpRendD3D->FX_PopRenderTarget(0);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // 2nd pass: generate blend texture
        {
            PROFILE_LABEL_SCOPE("Blend Weight Generation");
            gcpRendD3D->FX_ClearTarget(pBlendTex, Clr_Transparent);
            gcpRendD3D->FX_PushRenderTarget(0, pBlendTex, &gcpRendD3D->m_DepthBufferOrig);
            gcpRendD3D->FX_SetActiveRenderTargets();

            static CCryNameTSCRC pszBlendWeightTechName("BlendWeightSMAA");
            GetUtils().ShBeginPass(pShader, pszBlendWeightTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

            gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
            gcpRendD3D->FX_StencilTestCurRef(true, false);

            GetUtils().SetTexture(pEdgesTex, 0, FILTER_LINEAR);
            GetUtils().SetTexture(m_TextureAreaSMAA, 1, FILTER_LINEAR);
            GetUtils().SetTexture(m_TextureSearchSMAA, 2, FILTER_POINT);

            SD3DPostEffectsUtils::DrawFullScreenTriWPOS(iWidth, iHeight);

            GetUtils().ShEndPass();

            gcpRendD3D->FX_PopRenderTarget(0);
        }

        CTexture* pDstRT = CTexture::s_ptexSceneNormalsMap;

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // Final pass - blend neighborhood pixels
        {
            PROFILE_LABEL_SCOPE("Composite");
            gcpRendD3D->FX_PushRenderTarget(0, pDstRT, NULL);
            gcpRendD3D->FX_SetActiveRenderTargets();

            gcpRendD3D->FX_StencilTestCurRef(false);

            static CCryNameTSCRC pszBlendNeighborhoodTechName("NeighborhoodBlendingSMAA");
            GetUtils().ShBeginPass(pShader, pszBlendNeighborhoodTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

            gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
            GetUtils().SetTexture(pBlendTex, 0, FILTER_POINT);
            GetUtils().SetTexture(sourceTexture, 1, FILTER_LINEAR);

            SD3DPostEffectsUtils::DrawFullScreenTriWPOS(iWidth, iHeight);

            GetUtils().ShEndPass();

            gcpRendD3D->FX_PopRenderTarget(0);
        }

        //////////////////////////////////////////////////////////////////////////
        // TEMPORAL SMAA 1TX
        {
            PROFILE_LABEL_SCOPE("TAA");
            CTexture* currentTarget = GetUtils().GetTemporalCurrentTarget();
            CTexture* historyTarget = GetUtils().GetTemporalHistoryTarget();

            gcpRendD3D->FX_PushRenderTarget(0, currentTarget, nullptr);

            static CCryNameTSCRC TechNameTAA("SMAA_TAA");
            GetUtils().ShBeginPass(pShader, TechNameTAA, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

            {
                TemporalAAParameters temporalAAParameters;
                BuildTemporalParameters(temporalAAParameters);

                static CCryNameR szReprojMatrix("ReprojectionMatrix");
                pShader->FXSetPSFloat(szReprojMatrix, (Vec4*)temporalAAParameters.m_reprojection.GetData(), 4);

                Vec4 temporalParams(
                    temporalAAParameters.m_useAntiFlickerFilter,
                    temporalAAParameters.m_clampingFactor,
                    temporalAAParameters.m_newFrameWeight,
                    temporalAAParameters.m_sharpeningFactor);

                static CCryNameR paramName("TemporalParams");
                pShader->FXSetPSFloat(paramName, &temporalParams, 1);
            }

            GetUtils().SetTexture(pDstRT, 0, FILTER_POINT);
            GetUtils().SetTexture(historyTarget, 1, FILTER_LINEAR);
            GetUtils().SetTexture(GetUtils().GetVelocityObjectRT(), 3, FILTER_POINT);
            GetUtils().SetTexture(CTexture::s_ptexZTarget, 5, FILTER_POINT);

            D3DShaderResourceView* depthSRV[1] = { gcpRendD3D->m_pZBufferDepthReadOnlySRV };
            gcpRendD3D->m_DevMan.BindSRV(eHWSC_Pixel, depthSRV, 16, 1);
            gcpRendD3D->FX_Commit();

            SD3DPostEffectsUtils::DrawFullScreenTriWPOS(currentTarget->GetWidth(), currentTarget->GetHeight());

            depthSRV[0] = nullptr;
            gcpRendD3D->m_DevMan.BindSRV(eHWSC_Pixel, depthSRV, 16, 1);
            gcpRendD3D->FX_Commit();

            GetUtils().ShEndPass();

            gcpRendD3D->FX_PopRenderTarget(0);

            *outputTexture = currentTarget;
        }
    }
}

void PostAAPass::RenderFXAA(CTexture* sourceTexture)
{
    PROFILE_LABEL_SCOPE("FXAA");

    CShader* pShader = CShaderMan::s_shPostAA;
    const f32 fWidthRcp = 1.0f / (float)gcpRendD3D->GetWidth();
    const f32 fHeightRcp = 1.0f / (float)gcpRendD3D->GetHeight();

    static CCryNameTSCRC TechNameFXAA("FXAA");
    GetUtils().ShBeginPass(pShader, TechNameFXAA, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    const Vec4 vRcpFrameOpt(-0.33f * fWidthRcp, -0.33f * fHeightRcp, 0.33f * fWidthRcp, 0.33f * fHeightRcp);// (1.0/sz.xy) * -0.33, (1.0/sz.xy) * 0.33. 0.5 -> softer
    const Vec4 vRcpFrameOpt2(-2.0f * fWidthRcp, -2.0f * fHeightRcp, 2.0f * fWidthRcp, 2.0f  * fHeightRcp);// (1.0/sz.xy) * -2.0, (1.0/sz.xy) * 2.0
    static CCryNameR pRcpFrameOptParam("RcpFrameOpt");
    pShader->FXSetPSFloat(pRcpFrameOptParam, &vRcpFrameOpt, 1);
    static CCryNameR pRcpFrameOpt2Param("RcpFrameOpt2");
    pShader->FXSetPSFloat(pRcpFrameOpt2Param, &vRcpFrameOpt2, 1);

    GetUtils().SetTexture(sourceTexture, 0, FILTER_LINEAR);

    SD3DPostEffectsUtils::DrawFullScreenTriWPOS(sourceTexture->GetWidth(), sourceTexture->GetHeight());
    gcpRendD3D->FX_Commit();

    GetUtils().ShEndPass();
}

void PostAAPass::RenderComposites(CTexture* sourceTexture)
{
    PROFILE_LABEL_SCOPE("FLARES, GRAIN");

    gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3]);
    if ((gcpRendD3D->FX_GetAntialiasingType() & eAT_TEMPORAL_MASK) == 0)
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
    }

    if (gcpRendD3D->m_RP.m_PersFlags2 & RBPF2_LENS_OPTICS_COMPOSITE)
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
        if (CRenderer::CV_r_FlaresChromaShift > 0.5f / (float)gcpRendD3D->GetWidth())  // only relevant if bigger than half pixel
        {
            gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];
        }
    }

    if (CRenderer::CV_r_colorRangeCompression)
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
    }
    else
    {
        gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE4];
    }

    static const CCryNameTSCRC TechNameComposites("PostAAComposites");
    static const CCryNameTSCRC TechNameDebugMotion("PostAADebugMotion");

    CCryNameTSCRC techName = TechNameComposites;
    if (CRenderer::CV_r_MotionVectorsDebug)
    {
        techName = TechNameDebugMotion;
    }

    GetUtils().ShBeginPass(CShaderMan::s_shPostAA, techName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    {
        STexState texStateLinerSRGB(FILTER_LINEAR, true);
        texStateLinerSRGB.m_bSRGBLookup = true;

        bool bResolutionScaling = false;

#if defined(CRY_USE_METAL) || defined(ANDROID)
        {
            const Vec2& vDownscaleFactor = gcpRendD3D->m_RP.m_CurDownscaleFactor;
            bResolutionScaling = (vDownscaleFactor.x < .999999f) || (vDownscaleFactor.y < .999999f) == false;
            gcpRendD3D->SetCurDownscaleFactor(Vec2(1, 1));
        }
#endif
        if (!bResolutionScaling)
        {
            texStateLinerSRGB.SetFilterMode(FILTER_POINT);
        }

        sourceTexture->Apply(0, CTexture::GetTexState(texStateLinerSRGB));
    }

    gcpRendD3D->FX_PushWireframeMode(R_SOLID_MODE);
    gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

    if (CRenderer::CV_r_MotionVectorsDebug)
    {
        TemporalAAParameters temporalAAParameters;
        BuildTemporalParameters(temporalAAParameters);

        static CCryNameR szReprojMatrix("ReprojectionMatrix");
        CShaderMan::s_shPostAA->FXSetPSFloat(szReprojMatrix, (Vec4*)temporalAAParameters.m_reprojection.GetData(), 4);

        GetUtils().SetTexture(GetUtils().GetVelocityObjectRT(), 3, FILTER_POINT);
        GetUtils().SetTexture(CTexture::s_ptexZTarget, 5, FILTER_POINT);

        D3DShaderResourceView* depthSRV[1] = { gcpRendD3D->m_pZBufferDepthReadOnlySRV };
        gcpRendD3D->m_DevMan.BindSRV(eHWSC_Pixel, depthSRV, 16, 1);
        gcpRendD3D->FX_Commit();

        SPostEffectsUtils::DrawFullScreenTri(gcpRendD3D->GetOverlayWidth(), gcpRendD3D->GetOverlayHeight());

        depthSRV[0] = nullptr;
        gcpRendD3D->m_DevMan.BindSRV(eHWSC_Pixel, depthSRV, 16, 1);
        gcpRendD3D->FX_Commit();
    }
    else
    {
        const Vec4 temporalParams(0, 0, 0, max(1.0f + CRenderer::CV_r_AntialiasingTAASharpening, 1.0f));
        static CCryNameR paramName("TemporalParams");
        CShaderMan::s_shPostAA->FXSetPSFloat(paramName, &temporalParams, 1);

        CTexture* pLensOpticsComposite = CTexture::s_ptexSceneTargetR11G11B10F[0];
        GetUtils().SetTexture(pLensOpticsComposite, 5, FILTER_POINT);
        if (gRenDev->m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_SAMPLE3])
        {
            const Vec4 vLensOptics(1.0f, 1.0f, 1.0f, CRenderer::CV_r_FlaresChromaShift);
            static CCryNameR pLensOpticsParam("vLensOpticsParams");
            CShaderMan::s_shPostAA->FXSetPSFloat(pLensOpticsParam, &vLensOptics, 1);
        }

        // Apply grain (unfortunately final luminance texture doesn't get its final value baked, so have to replicate entire hdr eye adaption)
        {
            Vec4 vHDRSetupParams[5];
            gEnv->p3DEngine->GetHDRSetupParams(vHDRSetupParams);

            CEffectParam* m_pFilterGrainAmount = PostEffectMgr()->GetByName("FilterGrain_Amount");
            CEffectParam* m_pFilterArtifactsGrain = PostEffectMgr()->GetByName("FilterArtifacts_Grain");
            const float fFiltersGrainAmount = max(m_pFilterGrainAmount->GetParam(), m_pFilterArtifactsGrain->GetParam());
            const Vec4 v = Vec4(0, 0, 0, max(fFiltersGrainAmount, max(vHDRSetupParams[1].w, CRenderer::CV_r_HDRGrainAmount)));
            static CCryNameR szHDRParam("HDRParams");
            CShaderMan::s_shPostAA->FXSetPSFloat(szHDRParam, &v, 1);
            static CCryNameR szHDREyeAdaptationParam("HDREyeAdaptation");
            CShaderMan::s_shPostAA->FXSetPSFloat(szHDREyeAdaptationParam, &vHDRSetupParams[3], 1);

            GetUtils().SetTexture(CTexture::s_ptexFilmGrainMap, 6, FILTER_POINT, 0);
            if (CTexture::s_ptexCurLumTexture)
            {
                GetUtils().SetTexture(CTexture::s_ptexCurLumTexture, 7, FILTER_POINT);
            }
#ifdef CRY_USE_METAL // Metal still expects a bound texture here!
            else
            {
                CTexture::s_ptexWhite->Apply(7, FILTER_POINT);
            }
#endif
        }

        SPostEffectsUtils::DrawFullScreenTri(gcpRendD3D->GetOverlayWidth(), gcpRendD3D->GetOverlayHeight());
    }

    gcpRendD3D->FX_PopWireframeMode();

    GetUtils().ShEndPass();
}

void PostAAPass::RenderFinalComposite(CTexture* sourceTexture)
{
    if (CShaderMan::s_shPostAA == NULL)
    {
        return;
    }

    PROFILE_LABEL_SCOPE("NATIVE_UPSCALE");
    gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
    if (sourceTexture->GetWidth() != gRenDev->GetOverlayWidth() || sourceTexture->GetHeight() != gRenDev->GetOverlayHeight())
    {
        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
    }

    gcpRendD3D->FX_PushWireframeMode(R_SOLID_MODE);
    gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

    static CCryNameTSCRC pTechName("UpscaleImage");
    SPostEffectsUtils::ShBeginPass(CShaderMan::s_shPostAA, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    STexState texStateLinerSRGB(FILTER_LINEAR, true);
    texStateLinerSRGB.m_bSRGBLookup = true;
    sourceTexture->Apply(0, CTexture::GetTexState(texStateLinerSRGB));

    SPostEffectsUtils::DrawFullScreenTri(gcpRendD3D->GetOverlayWidth(), gcpRendD3D->GetOverlayHeight());
    SPostEffectsUtils::ShEndPass();

    gcpRendD3D->FX_PopWireframeMode();
}