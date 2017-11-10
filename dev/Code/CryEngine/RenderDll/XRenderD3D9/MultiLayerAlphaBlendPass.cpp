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

#include "MultiLayerAlphaBlendPass.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

const uint32 MultiLayerAlphaBlendPass::MAX_LAYERS = 8;  // definition
static uint32_t s_UavBindLocation = 5; // @TODO: This should be moved to a system that manages UAV bind locations when such a system exists.
MultiLayerAlphaBlendPass* MultiLayerAlphaBlendPass::s_pInstance = nullptr;

/*static*/ void MultiLayerAlphaBlendPass::InstallInstance()
{
    if (s_pInstance == nullptr)
    {
        s_pInstance = new MultiLayerAlphaBlendPass();
    }
}

/*static*/ void MultiLayerAlphaBlendPass::ReleaseInstance()
{
    delete s_pInstance;
    s_pInstance = nullptr;
}

/*static*/ MultiLayerAlphaBlendPass& MultiLayerAlphaBlendPass::GetInstance()
{
    AZ_Assert(s_pInstance != nullptr, "MultiLayerAlphaBlendPass instance being retrieved before install.");
    return *s_pInstance;
}

MultiLayerAlphaBlendPass::MultiLayerAlphaBlendPass()
    : m_alphaLayersBuffer()
    , m_layerCount(0)
    , m_supported(SupportLevel::UNKNOWN)
{
}

MultiLayerAlphaBlendPass::~MultiLayerAlphaBlendPass()
{
    //@TODO: Destroy buffers if created
}

bool MultiLayerAlphaBlendPass::IsSupported()
{
    if (m_supported == SupportLevel::UNKNOWN)
    {
        #if SUPPORTS_WINDOWS_10_SDK
        D3D11_FEATURE_DATA_D3D11_OPTIONS2 featureData;
        HRESULT result = gcpRendD3D->GetDevice().CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &featureData, sizeof(featureData));
        if (result == S_OK && featureData.ROVsSupported)
        {
            m_supported = SupportLevel::SUPPORTED;
        }
        else
        #endif
        {
            m_supported = SupportLevel::NOT_SUPPORTED;
            AZ_Warning("Rendering", false, "Multi-Layer Alpha Blending is not supported on this device.");
        }
    }

    return m_supported == SupportLevel::SUPPORTED;

}

bool MultiLayerAlphaBlendPass::SetLayerCount(uint32_t count)
{
    if (count > 0 && IsSupported())
    {
        AZ_Warning("Rendering", count <= MAX_LAYERS, "Too many layers - Setting number of alpha blend layers to the maximum of %u.", MAX_LAYERS);
        m_layerCount = min<uint32_t>(count, MAX_LAYERS);
        return true;
    }
    m_layerCount = 0;
    return false;
}

uint32_t MultiLayerAlphaBlendPass::GetLayerCount()
{
    return m_layerCount;
}

void MultiLayerAlphaBlendPass::ConfigureShaderFlags(uint64& flags)
{
    if (m_layerCount == 0)
    {
        return;
    }
    flags |= g_HWSR_MaskBit[HWSR_MULTI_LAYER_ALPHA_BLEND];
}

void MultiLayerAlphaBlendPass::Resolve(CD3D9Renderer& renderer)
{
    if (m_layerCount == 0)
    {
        return;
    }

    PROFILE_LABEL_SCOPE("MLAB_RESOLVE");

    // @TODO: Only copy the regions where there are transparent draws
    PostProcessUtils().CopyScreenToTexture(CTexture::s_ptexCurrSceneTarget);

    ConfigureShaderFlags(renderer->m_RP.m_FlagsShader_RT);
    static const CCryNameTSCRC pTechName("MultiLayerAlphaBlendResolve");
    PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    PostProcessUtils().SetTexture(CTexture::s_ptexCurrSceneTarget, 0, FILTER_NONE);

    BindResources();
    renderer.FX_SetState(GS_NODEPTHTEST);
    PostProcessUtils().DrawFullScreenTri(renderer.GetWidth(), renderer.GetHeight());
    PostProcessUtils().ShEndPass();
    UnBindResources();
}

void MultiLayerAlphaBlendPass::BindResources()
{
    if (m_layerCount == 0)
    {
        return;
    }

    // Create / resize alpha layer buffer if necessary
    uint32 width = gRenDev->GetWidth();
    uint32 height = gRenDev->GetHeight();
    uint32 numElements = width * height * m_layerCount;

    // Release if the wrong size
    if (m_alphaLayersBuffer.m_pBuffer != nullptr && m_alphaLayersBuffer.m_numElements != numElements)
    {
	    m_alphaLayersBuffer.Release();
    }

    if (!m_alphaLayersBuffer.m_pBuffer)
    {
	    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	    uint32 stride = 16; // float4 = (4 bytes per float) * 4;

	    m_alphaLayersBuffer.Create(numElements, stride, format, DX11BUF_BIND_UAV | DX11BUF_STRUCTURED, NULL);
    }

    uint32 count = 1;
    gcpRendD3D->m_DevMan.BindUAV(eHWSC_Pixel, m_alphaLayersBuffer.m_pUAV, &count, s_UavBindLocation, count);

}

void MultiLayerAlphaBlendPass::UnBindResources()
{
    if (m_layerCount == 0)
    {
        return;
    }
    gcpRendD3D->m_DevMan.BindUAV(eHWSC_Pixel, nullptr, 1, s_UavBindLocation);
}
