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
#include "MatMan.h"
#include <IRenderer.h>
#include "VisAreas.h"

DEFINE_INTRUSIVE_LINKED_LIST(CMatInfo)

//////////////////////////////////////////////////////////////////////////
void CMaterialLayer::SetShaderItem(const _smart_ptr<IMaterial> pParentMtl, const SShaderItem& pShaderItem)
{
    assert(pParentMtl && "CMaterialLayer::SetShaderItem invalid material");

    if (pShaderItem.m_pShader)
    {
        pShaderItem.m_pShader->AddRef();
    }

    if (pShaderItem.m_pShaderResources)
    {
        pShaderItem.m_pShaderResources->AddRef();
        CMatInfo* pParentMatInfo = (CMatInfo*)(pParentMtl.get());
        pShaderItem.m_pShaderResources->SetMaterialName(pParentMatInfo->m_sUniqueMaterialName);
    }

    gEnv->pRenderer->ClearShaderItem(&m_pShaderItem);
    SAFE_RELEASE(m_pShaderItem.m_pShader);
    SAFE_RELEASE(m_pShaderItem.m_pShaderResources);

    m_pShaderItem = pShaderItem;
    gEnv->pRenderer->UpdateShaderItem(&m_pShaderItem);
}
//////////////////////////////////////////////////////////////////////////
void CMaterialLayer::GetMemoryUsage(ICrySizer* pSizer)
{
    SIZER_COMPONENT_NAME(pSizer, "MaterialLayer");
    pSizer->AddObject(this, sizeof(*this));
}

//////////////////////////////////////////////////////////////////////////
size_t CMaterialLayer::GetResourceMemoryUsage(ICrySizer* pSizer)
{
    size_t nResourceMemory(0);
    // Pushing some context to use the Macro.
    {
        SIZER_COMPONENT_NAME(pSizer, "Textures");

        IRenderShaderResources* piResources(m_pShaderItem.m_pShaderResources);
        if (piResources)
        {
            //////////////////////////////////////////////////////////////////////////
            // Texture
            for (size_t nCount = 0; nCount < EFTT_MAX; ++nCount)
            {
                SEfResTexture*  piTextureResource(piResources->GetTexture(nCount));
                if (piTextureResource)
                {
                    ITexture* piTexture = piTextureResource->m_Sampler.m_pITex;
                    if (piTexture)
                    {
                        SIZER_COMPONENT_NAME(pSizer, "MemoryTexture");
                        size_t nCurrentResourceMemoryUsage = piTexture->GetDataSize();
                        nResourceMemory += nCurrentResourceMemoryUsage;
                        pSizer->AddObject(piTexture, nCurrentResourceMemoryUsage);

                        IResourceCollector* pColl = pSizer->GetResourceCollector();
                        if (pColl)
                        {
                            pColl->AddResource(piTexture->GetName(), nCurrentResourceMemoryUsage);
                        }
                    }
                }
            }
            //////////////////////////////////////////////////////////////////////////
        }
    }
    return nResourceMemory;
}

//////////////////////////////////////////////////////////////////////////
CMatInfo::CMatInfo()
{
    m_nRefCount = 0;
    m_Flags = 0;

    m_nSurfaceTypeId = 0;

    m_pMaterialLayers = 0;

    m_ucDefautMappingAxis = 0;
    m_fDefautMappingScale = 1.f;

#ifdef SUPPORT_MATERIAL_SKETCH
    m_pPreSketchShader = 0;
    m_nPreSketchTechnique = 0;
#endif

#ifdef SUPPORT_MATERIAL_EDITING
    m_pUserData = NULL;
#endif

    m_pActiveLayer = NULL;

    m_shaderName = "Non-Initialized Shader name";

    ZeroStruct(m_streamZoneInfo);

#ifdef TRACE_MATERIAL_LEAKS
    m_sLoadingCallstack = GetSystem()->GetLoadingProfilerCallstack();
#endif
}

//////////////////////////////////////////////////////////////////////////
CMatInfo::~CMatInfo()
{
    ShutDown();
}

void CMatInfo::AddRef() 
{
    CryInterlockedIncrement(&m_nRefCount);
}
//////////////////////////////////////////////////////////////////////////
void CMatInfo::Release()
{
    if (CryInterlockedDecrement(&m_nRefCount) <= 0)
    {
        delete this;
    }
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::ShutDown()
{
    SAFE_DELETE(m_pMaterialLayers);

    if (m_shaderItem.m_pShaderResources)
    {
        CCamera* pC = m_shaderItem.m_pShaderResources->GetCamera();
        if (pC)
        {
            delete pC;
        }
    }
    
    ReleaseCurrentShaderItem();
    m_subMtls.clear();
}

//////////////////////////////////////////////////////////////////////////
IMaterialHelpers& CMatInfo::GetMaterialHelpers()
{
    return GetMatMan()->s_materialHelpers;
}

IMaterialManager* CMatInfo::GetMaterialManager()
{
    return GetMatMan();
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetName(const char* sName)
{
    m_sMaterialName = sName;
    m_sUniqueMaterialName = m_sMaterialName;
    if (m_shaderItem.m_pShaderResources)
    {
        m_shaderItem.m_pShaderResources->SetMaterialName(m_sUniqueMaterialName); // Only for correct warning message purposes.
    }
    if (m_Flags & MTL_FLAG_MULTI_SUBMTL)
    {
        for (int i = 0, num = m_subMtls.size(); i < num; i++)
        {
            if (m_subMtls[i] && (m_subMtls[i]->m_Flags & MTL_FLAG_PURE_CHILD))
            {
                m_subMtls[i]->m_sUniqueMaterialName = m_sMaterialName;
                if (m_subMtls[i]->m_shaderItem.m_pShaderResources)
                {
                    m_subMtls[i]->m_shaderItem.m_pShaderResources->SetMaterialName(m_sUniqueMaterialName);
                }
            }

            if (m_subMtls[i] && strstr(m_subMtls[i]->m_sUniqueMaterialName, MTL_SPECIAL_NAME_RAYCAST_PROXY) != 0)
            {
                m_subMtls[i]->m_Flags |= MTL_FLAG_RAYCAST_PROXY;
                m_subMtls[i]->m_Flags |= MTL_FLAG_NODRAW;
            }
        }
    }

    if (strstr(sName, MTL_SPECIAL_NAME_COLLISION_PROXY) != 0 || strstr(sName, MTL_SPECIAL_NAME_COLLISION_PROXY_VEHICLE) != 0)
    {
        m_Flags |= MTL_FLAG_COLLISION_PROXY;
    }
    else if (strstr(sName, MTL_SPECIAL_NAME_RAYCAST_PROXY) != 0)
    {
        m_Flags |= MTL_FLAG_RAYCAST_PROXY;
        m_Flags |= MTL_FLAG_NODRAW;
    }
}

//////////////////////////////////////////////////////////////////////////
bool CMatInfo::IsDefault()
{
    return this == GetMatMan()->GetDefaultMaterial();
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::UpdateMaterialFlags()
{
    m_Flags &= ~(MTL_FLAG_REQUIRE_FORWARD_RENDERING | MTL_FLAG_REQUIRE_NEAREST_CUBEMAP);

    if (m_shaderItem.m_pShader)
    {
        IRenderShaderResources* pRendShaderResources = m_shaderItem.m_pShaderResources;

        const bool bAlphaBlended = (m_shaderItem.m_pShader->GetFlags() & (EF_NODRAW | EF_DECAL)) || (pRendShaderResources && pRendShaderResources->IsTransparent());
        const bool bIsHair = (m_shaderItem.m_pShader->GetFlags2() & EF2_HAIR) != 0;
        const bool bIsGlass = m_shaderItem.m_pShader->GetShaderType() == eST_Glass;
        const bool bIsWater = m_shaderItem.m_pShader->GetShaderType() == eST_Water;
        const bool bIsEye = strcmp(m_shaderItem.m_pShader->GetName(), "Eye") == 0;

        if (bAlphaBlended && !(m_shaderItem.m_pShader->GetFlags2() & EF2_NODRAW) && !(m_shaderItem.m_pShader->GetFlags() & EF_DECAL))
        {
            m_Flags |= MTL_FLAG_REQUIRE_FORWARD_RENDERING;
        }
        else if (bIsHair || bIsGlass)
        {
            m_Flags |= MTL_FLAG_REQUIRE_FORWARD_RENDERING;
        }

        if ((bAlphaBlended || bIsHair || bIsGlass || bIsWater || bIsEye) &&
            (pRendShaderResources) &&
            (pRendShaderResources->GetTexture(EFTT_ENV)) &&
            (pRendShaderResources->GetTexture(EFTT_ENV)->m_Sampler.m_eTexType == eTT_NearestCube))
        {
            m_Flags |= MTL_FLAG_REQUIRE_NEAREST_CUBEMAP;
        }

        // Make sure to refresh sectors
        static int nLastUpdateFrameId = 0;
        if (gEnv->IsEditing() && GetTerrain() && GetVisAreaManager() && nLastUpdateFrameId != GetRenderer()->GetFrameID())
        {
            GetTerrain()->MarkAllSectorsAsUncompiled();
            GetVisAreaManager()->MarkAllSectorsAsUncompiled();
            nLastUpdateFrameId = GetRenderer()->GetFrameID();
        }
    }
}

void CMatInfo::ReleaseCurrentShaderItem()
{
    // Clear the renderer's shader resources to nullptr before releasing the shader item so there are no dangling pointers
    gEnv->pRenderer->ClearShaderItem(&m_shaderItem);
    SAFE_RELEASE(m_shaderItem.m_pShader);
    SAFE_RELEASE(m_shaderItem.m_pShaderResources);
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetShaderItem(const SShaderItem& _ShaderItem)
{
    if (_ShaderItem.m_pShader)
    {
        _ShaderItem.m_pShader->AddRef();
    }
    if (_ShaderItem.m_pShaderResources)
    {
        _ShaderItem.m_pShaderResources->AddRef();
        _ShaderItem.m_pShaderResources->SetMaterialName(m_sUniqueMaterialName);
    }

    ReleaseCurrentShaderItem();

    m_shaderItem = _ShaderItem;
    gEnv->pRenderer->UpdateShaderItem(&m_shaderItem);

    UpdateMaterialFlags();

    int sketchMode = m_pMatMan->GetSketchMode();
    if (sketchMode)
    {
        SetSketchMode(sketchMode);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::AssignShaderItem(const SShaderItem& _ShaderItem)
{
    //if (_ShaderItem.m_pShader)
    //  _ShaderItem.m_pShader->AddRef();
    if (_ShaderItem.m_pShaderResources)
    {
        _ShaderItem.m_pShaderResources->SetMaterialName(m_sUniqueMaterialName);
    }

    ReleaseCurrentShaderItem();

    m_shaderItem = _ShaderItem;
    gEnv->pRenderer->UpdateShaderItem(&m_shaderItem);

    UpdateMaterialFlags();
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetSurfaceType(const char* sSurfaceTypeName)
{
    m_nSurfaceTypeId = 0;

    ISurfaceType* pSurfaceType = GetMatMan()->GetSurfaceTypeByName(sSurfaceTypeName, m_sMaterialName);
    if (pSurfaceType)
    {
        m_nSurfaceTypeId = pSurfaceType->GetId();
    }
}

ISurfaceType* CMatInfo::GetSurfaceType()
{
    return GetMatMan()->GetSurfaceType(m_nSurfaceTypeId, m_sMaterialName);
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetSubMtlCount(int numSubMtl)
{
    AUTO_LOCK(GetSubMaterialResizeLock());
    m_Flags |= MTL_FLAG_MULTI_SUBMTL;
    m_subMtls.resize(numSubMtl);
}

//////////////////////////////////////////////////////////////////////////
bool CMatInfo::IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES], IRenderMesh* pRenderMesh) const
{
    if (pRenderMesh)
    {
        TRenderChunkArray* pChunks = &pRenderMesh->GetChunks();

        if (pChunks != NULL)
        {
            for (uint32 i = 0, size = pChunks->size(); i < size; i++)
            {
                if (!AreChunkTexturesStreamedIn(&(*pChunks)[i], nMinPrecacheRoundIds))
                {
                    return false;
                }
            }
        }

        pChunks = &pRenderMesh->GetChunksSkinned();
        for (uint32 i = 0, size = pChunks->size(); i < size; i++)
        {
            if (!AreChunkTexturesStreamedIn(&(*pChunks)[i], nMinPrecacheRoundIds))
            {
                return false;
            }
        }
    }
    else
    {
        if (!AreChunkTexturesStreamedIn(NULL, nMinPrecacheRoundIds))
        {
            return false;
        }
    }

    return true;
}

bool CMatInfo::AreChunkTexturesStreamedIn(CRenderChunk* pRenderChunk, const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const
{
    const CMatInfo* pMaterial = this;

    if (pRenderChunk && pRenderChunk->pRE && pRenderChunk->nNumIndices && pRenderChunk->nNumVerts)
    { // chunk is defined and have valid geometry
        if (pRenderChunk->m_nMatID < (uint16)m_subMtls.size())
        {
            pMaterial = (CMatInfo*)m_subMtls[pRenderChunk->m_nMatID];
        }

        if (pMaterial != NULL)
        {
            return pMaterial->AreTexturesStreamedIn(nMinPrecacheRoundIds);
        }
    }
    else if (!pRenderChunk)
    {
        if (!AreTexturesStreamedIn(nMinPrecacheRoundIds))
        {
            return false;
        }

        for (int nSubMtl = 0; nSubMtl < (int)m_subMtls.size(); ++nSubMtl)
        {
            pMaterial = m_subMtls[nSubMtl];

            if (pMaterial != NULL)
            {
                if (!pMaterial->AreTexturesStreamedIn(nMinPrecacheRoundIds))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool CMatInfo::AreTexturesStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const
{
    // Check this material
    if (IRenderShaderResources* pShaderResources = GetShaderItem().m_pShaderResources)
    {
        for (int t = 0; t < EFTT_MAX; t++)
        {
            if (SEfResTexture* pTextureResource = pShaderResources->GetTexture(t))
            {
                if (ITexture*   pTexture = pTextureResource->m_Sampler.m_pITex)
                {
                    if (!pTexture->IsStreamedIn(nMinPrecacheRoundIds))
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetSubMtl(int nSlot, _smart_ptr<IMaterial> pMtl)
{
    assert(nSlot >= 0 && nSlot < (int)m_subMtls.size());
    m_subMtls[nSlot] = (CMatInfo*)pMtl.get();
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetLayerCount(uint32 nCount)
{
    if (!m_pMaterialLayers)
    {
        m_pMaterialLayers = new MatLayers;
    }

    m_pMaterialLayers->resize(nCount);
}

//////////////////////////////////////////////////////////////////////////
uint32 CMatInfo::GetLayerCount() const
{
    if (m_pMaterialLayers)
    {
        return (uint32) m_pMaterialLayers->size();
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetLayer(uint32 nSlot, IMaterialLayer* pLayer)
{
    assert(m_pMaterialLayers);
    assert(nSlot < (uint32) m_pMaterialLayers->size());

    if (m_pMaterialLayers && pLayer && nSlot < (uint32) m_pMaterialLayers->size())
    {
        (*m_pMaterialLayers)[nSlot] = static_cast< CMaterialLayer*>(pLayer);
    }
}

//////////////////////////////////////////////////////////////////////////

const IMaterialLayer* CMatInfo::GetLayer(uint8 nLayersMask, uint8 nLayersUsageMask) const
{
    if (m_pMaterialLayers && nLayersMask)
    {
        int nSlotCount = m_pMaterialLayers->size();
        for (int nSlot = 0; nSlot < nSlotCount; ++nSlot)
        {
            CMaterialLayer* pLayer = static_cast< CMaterialLayer*>((*m_pMaterialLayers)[nSlot]);

            if (nLayersMask & (1 << nSlot))
            {
                if (pLayer)
                {
                    m_pActiveLayer = pLayer;
                    return m_pActiveLayer;
                }

                m_pActiveLayer = 0;

                return 0;
            }
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
const IMaterialLayer* CMatInfo::GetLayer(uint32 nSlot) const
{
    if (m_pMaterialLayers && nSlot < (uint32) m_pMaterialLayers->size())
    {
        return static_cast< CMaterialLayer*>((*m_pMaterialLayers)[nSlot]);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
IMaterialLayer* CMatInfo::CreateLayer()
{
    return new CMaterialLayer;
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetUserData(void* pUserData)
{
#ifdef SUPPORT_MATERIAL_EDITING
    m_pUserData = pUserData;
#endif
}

//////////////////////////////////////////////////////////////////////////
void* CMatInfo::GetUserData() const
{
#ifdef SUPPORT_MATERIAL_EDITING
    return m_pUserData;
#else
    CryFatalError("CMatInfo::GetUserData not supported on this platform");
    return NULL;
#endif
}

//////////////////////////////////////////////////////////////////////////
int CMatInfo::FillSurfaceTypeIds(int pSurfaceIdsTable[])
{
    if (m_subMtls.empty() || !(m_Flags & MTL_FLAG_MULTI_SUBMTL))
    {
        pSurfaceIdsTable[0] = m_nSurfaceTypeId;
        return 1; // Not Multi material.
    }
    for (int i = 0; i < (int)m_subMtls.size(); i++)
    {
        if (m_subMtls[i] != 0)
        {
            pSurfaceIdsTable[i] = m_subMtls[i]->m_nSurfaceTypeId;
        }
        else
        {
            pSurfaceIdsTable[i] = 0;
        }
    }
    return m_subMtls.size();
}


void CMatInfo::Copy(_smart_ptr<IMaterial> pMtlDest, EMaterialCopyFlags flags)
{
    CMatInfo* pMatInfo = static_cast<CMatInfo*>(pMtlDest.get());
    if (flags & MTL_COPY_NAME)
    {
        pMatInfo->m_sMaterialName = m_sMaterialName;
        pMatInfo->m_sUniqueMaterialName = m_sUniqueMaterialName;
    }
    pMatInfo->m_nSurfaceTypeId = m_nSurfaceTypeId;
    pMatInfo->m_Flags = m_Flags;

    if (GetShaderItem().m_pShaderResources)
    {
        //
        SShaderItem& siSrc(GetShaderItem());
        SInputShaderResources isr(siSrc.m_pShaderResources);
        //
        SShaderItem& siDstTex(pMatInfo->GetShaderItem());
        SInputShaderResources idsTex(siDstTex.m_pShaderResources);
        if (!(flags & MTL_COPY_TEXTURES))
        {
            for (int n = 0; n < EFTT_MAX; n++)
            {
                isr.m_Textures[n] = idsTex.m_Textures[n];
            }
        }
        SShaderItem siDst(GetRenderer()->EF_LoadShaderItem(siSrc.m_pShader->GetName(), false, 0, &isr, siSrc.m_pShader->GetGenerationMask()));
        pMatInfo->AssignShaderItem(siDst);
        siDst.m_pShaderResources->CloneConstants(siSrc.m_pShaderResources);
    }
}


//////////////////////////////////////////////////////////////////////////
CMatInfo* CMatInfo::Clone()
{
    CMatInfo* pMatInfo = new CMatInfo;

    pMatInfo->m_sMaterialName = m_sMaterialName;
    pMatInfo->m_sUniqueMaterialName = m_sUniqueMaterialName;
    pMatInfo->m_nSurfaceTypeId = m_nSurfaceTypeId;
    pMatInfo->m_Flags = m_Flags;

    SShaderItem& siSrc(GetShaderItem());
    SInputShaderResources isr(siSrc.m_pShaderResources);

    SShaderItem siDst(GetRenderer()->EF_LoadShaderItem(siSrc.m_pShader->GetName(), false, 0, &isr, siSrc.m_pShader->GetGenerationMask()));
    pMatInfo->AssignShaderItem(siDst);
    siDst.m_pShaderResources->CloneConstants(siSrc.m_pShaderResources);

    // Necessary to delete all the data allocated in the renderer
    GetRenderer()->EF_ReleaseInputShaderResource(&isr);

    return pMatInfo;
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::GetMemoryUsage(ICrySizer* pSizer) const
{
    SIZER_COMPONENT_NAME(pSizer, "Material");
    pSizer->AddObject(this, sizeof(*this));

    if (m_pMaterialLayers)
    {
        int n = m_pMaterialLayers->size();
        for (int i = 0; i < n; i++)
        {
            CMaterialLayer* pMaterialLayer = (*m_pMaterialLayers)[i];
            if (pMaterialLayer)
            {
                pMaterialLayer->GetMemoryUsage(pSizer);
            }
        }
    }

    // all sub material
    {
        pSizer->AddObject(m_subMtls);
    }
}
//////////////////////////////////////////////////////////////////////////
size_t CMatInfo::GetResourceMemoryUsage(ICrySizer* pSizer)
{
    size_t  nTotalResourceMemoryUsage(0);

    if (m_pMaterialLayers)
    {
        int n = m_pMaterialLayers->size();
        for (int i = 0; i < n; i++)
        {
            CMaterialLayer* pMaterialLayer = (*m_pMaterialLayers)[i];
            if (pMaterialLayer)
            {
                nTotalResourceMemoryUsage += pMaterialLayer->GetResourceMemoryUsage(pSizer);
            }
        }
    }

    if (m_shaderItem.m_pShaderResources)
    {
        nTotalResourceMemoryUsage += m_shaderItem.m_pShaderResources->GetResourceMemoryUsage(pSizer);
    }

    // all sub material
    {
        int iCnt = GetSubMtlCount();
        for (int i = 0; i < iCnt; ++i)
        {
            _smart_ptr<IMaterial>  piMaterial(GetSubMtl(i));
            if (piMaterial)
            {
                nTotalResourceMemoryUsage += piMaterial->GetResourceMemoryUsage(pSizer);
            }
        }
    }
    return nTotalResourceMemoryUsage;
}

//////////////////////////////////////////////////////////////////////////
bool CMatInfo::SetGetMaterialParamFloat(const char* sParamName, float& v, bool bGet)
{
    bool bEmissive = m_shaderItem.m_pShaderResources->IsEmissive();
    bool bOk = m_shaderItem.m_pShaderResources && GetMaterialHelpers().SetGetMaterialParamFloat(*m_shaderItem.m_pShaderResources, sParamName, v, bGet);
    if (bOk && m_shaderItem.m_pShader && !bGet)
    {
        // since "glow" is a post effect it needs to be updated here
        if (bEmissive != m_shaderItem.m_pShaderResources->IsEmissive())
        {
            GetRenderer()->ForceUpdateShaderItem(&m_shaderItem);
        }

        m_shaderItem.m_pShaderResources->UpdateConstants(m_shaderItem.m_pShader);
    }

    return bOk;
}

bool CMatInfo::SetGetMaterialParamVec3(const char* sParamName, Vec3& v, bool bGet)
{
    bool bOk = m_shaderItem.m_pShaderResources && GetMaterialHelpers().SetGetMaterialParamVec3(*m_shaderItem.m_pShaderResources, sParamName, v, bGet);
    if (bOk && m_shaderItem.m_pShader && !bGet)
    {
        m_shaderItem.m_pShaderResources->UpdateConstants(m_shaderItem.m_pShader);
    }

    return bOk;
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetCamera(CCamera& cam)
{
    if (!m_shaderItem.m_pShaderResources)
    {
        return;
    }

    CCamera* pC = m_shaderItem.m_pShaderResources->GetCamera();
    if (!pC)
    {
        pC = new CCamera;
    }
    m_shaderItem.m_pShaderResources->SetCamera(pC);
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetSketchMode(int mode)
{
#ifdef SUPPORT_MATERIAL_SKETCH
    if (mode == 0)
    {
        if (m_pPreSketchShader)
        {
            m_shaderItem.m_pShader = m_pPreSketchShader;
            m_shaderItem.m_nTechnique = m_nPreSketchTechnique;
            m_pPreSketchShader = 0;
            m_nPreSketchTechnique = 0;
        }
    }
    else
    {
        if (m_shaderItem.m_pShader && m_shaderItem.m_pShader != m_pPreSketchShader)
        {
            EShaderType shaderType = m_shaderItem.m_pShader->GetShaderType();

            //      nGenerationMask = (uint32)m_shaderItem.m_pShader->GetGenerationMask();

            // Do not replace this shader types.
            switch (shaderType)
            {
            case eST_Terrain:
            case eST_Shadow:
            case eST_Water:
            case eST_FX:
            case eST_PostProcess:
            case eST_HDR:
            case eST_Sky:
            case eST_Particle:
                // For this shaders do not replace them.
                return;
            case eST_Vegetation:
            {
                // in low spec mode also skip vegetation - we have low spec vegetation shader
                if (mode == 3)
                {
                    return;
                }
            }
            }
        }

        if (!m_pPreSketchShader)
        {
            m_pPreSketchShader = m_shaderItem.m_pShader;
            m_nPreSketchTechnique = m_shaderItem.m_nTechnique;
        }

        //m_shaderItem.m_pShader = ((CMatMan*)GetMatMan())->GetDefaultHelperMaterial()->GetShaderItem().m_pShader;
        if (mode == 1)
        {
            m_shaderItem.m_pShader = gEnv->pRenderer->EF_LoadShader("Sketch");
            m_shaderItem.m_nTechnique = 0;
        }
        else if (mode == 2)
        {
            m_shaderItem.m_pShader = gEnv->pRenderer->EF_LoadShader("Sketch.Fast");
            m_shaderItem.m_nTechnique = 0;
        }
        else if (mode == 4)
        {
            SShaderItem tmp = gEnv->pRenderer->EF_LoadShaderItem("Sketch.TexelsPerMeter", false);
            m_shaderItem.m_pShader = tmp.m_pShader;
            m_shaderItem.m_nTechnique = tmp.m_nTechnique;
        }

        if (m_shaderItem.m_pShader)
        {
            m_shaderItem.m_pShader->AddRef();
        }
    }
    for (int i = 0; i < (int)m_subMtls.size(); i++)
    {
        if (m_subMtls[i])
        {
            m_subMtls[i]->SetSketchMode(mode);
        }
    }
#endif
}

void CMatInfo::SetTexelDensityDebug(int mode)
{
#ifdef SUPPORT_MATERIAL_SKETCH
    if (m_shaderItem.m_pShader)
    {
        EShaderType shaderType = m_pPreSketchShader ? m_pPreSketchShader->GetShaderType() : m_shaderItem.m_pShader->GetShaderType();

        switch (shaderType)
        {
        case eST_Terrain:
            if (mode == 3 || mode == 4)
            {
                if (m_nSurfaceTypeId == 0)
                {
                    mode = 0;
                }

                break;
            }
        case eST_Shadow:
        case eST_Water:
        case eST_FX:
        case eST_PostProcess:
        case eST_HDR:
        case eST_Sky:
        case eST_Particle:
            // For this shaders do not replace them.
            mode = 0;
            break;
        default:
            if (mode == 1 || mode == 2)
            {
                break;
            }
            mode = 0;
            break;
        }

        if (mode == 0)
        {
            if (m_pPreSketchShader)
            {
                m_shaderItem.m_pShader = m_pPreSketchShader;
                m_shaderItem.m_nTechnique = m_nPreSketchTechnique;
                m_pPreSketchShader = 0;
                m_nPreSketchTechnique = 0;
            }
        }
        else
        {
            if (!m_pPreSketchShader)
            {
                m_pPreSketchShader = m_shaderItem.m_pShader;
                m_nPreSketchTechnique = m_shaderItem.m_nTechnique;
            }

            SShaderItem tmp;
            if (mode == 3 || mode == 4)
            {
                tmp = gEnv->pRenderer->EF_LoadShaderItem("SketchTerrain.TexelDensityTerrainLayer", false);
            }
            else
            {
                tmp = gEnv->pRenderer->EF_LoadShaderItem("Sketch.TexelDensity", false);
            }
            m_shaderItem.m_pShader = tmp.m_pShader;
            m_shaderItem.m_nTechnique = tmp.m_nTechnique;
        }
    }

    for (int i = 0; i < (int)m_subMtls.size(); i++)
    {
        if (m_subMtls[i])
        {
            m_subMtls[i]->SetTexelDensityDebug(mode);
        }
    }
#endif
}

const char* CMatInfo::GetLoadingCallstack()
{
#ifdef TRACE_MATERIAL_LEAKS
    return m_sLoadingCallstack.c_str();
#else
    return "";
#endif
}

void CMatInfo::PrecacheMaterial(const float _fEntDistance, IRenderMesh* pRenderMesh, bool bFullUpdate, bool bDrawNear)
{
    //  FUNCTION_PROFILER_3DENGINE;
    LOADING_TIME_PROFILE_SECTION;

    int nFlags = 0;
    float fEntDistance;
    if (bDrawNear)
    {
        fEntDistance = _fEntDistance;
        nFlags |= FPR_HIGHPRIORITY;
    }
    else
    {
        fEntDistance = max(GetFloatCVar(e_StreamPredictionMinReportDistance), _fEntDistance);
    }

    float fMipFactor = fEntDistance * fEntDistance;

    // updating texture streaming distances
    if (pRenderMesh)
    {
        TRenderChunkArray* pChunks = &pRenderMesh->GetChunks();

        if (pChunks != NULL)
        {
            for (uint32 i = 0, size = pChunks->size(); i < size; i++)
            {
                PrecacheChunkTextures(fMipFactor, nFlags, &(*pChunks)[i], bFullUpdate);
            }
        }

        pChunks = &pRenderMesh->GetChunksSkinned();
        for (uint32 i = 0, size = pChunks->size(); i < size; i++)
        {
            PrecacheChunkTextures(fMipFactor, nFlags, &(*pChunks)[i], bFullUpdate);
        }
    }
    else
    {
        PrecacheChunkTextures(fMipFactor, nFlags, NULL, bFullUpdate);
    }
}

void CMatInfo::DisableTextureStreaming()
{
    const int numSubMaterials = max(GetSubMtlCount(), 1);
    for (int subMaterialId = 0; subMaterialId < numSubMaterials; ++subMaterialId)
    {
        _smart_ptr<IMaterial> subMaterial = GetSafeSubMtl(subMaterialId);
        if (subMaterial)
        {
            const SShaderItem& shaderItem = subMaterial->GetShaderItem();
            if (shaderItem.m_pShaderResources)
            {
                // Iterate through each texture in the material
                for (int textureSlot = 0; textureSlot < EFTT_MAX; ++textureSlot)
                {
                    SEfResTexture* shaderResourceTexture = shaderItem.m_pShaderResources->GetTexture(textureSlot);
                    if (shaderResourceTexture)
                    {
                        uint32 textureFlags = FT_DONT_STREAM;

                        if (textureSlot == EFTT_SMOOTHNESS || textureSlot == EFTT_SECOND_SMOOTHNESS)
                        {
                            textureFlags |= FT_ALPHA;
                        }

                        // Calling load texture here will not actually re-create/re-load an existing texture. It will simply toggle streaming off
                        ITexture* texture = gEnv->pRenderer->EF_LoadTexture(shaderResourceTexture->m_Name, textureFlags);
                        // Call release to decrement the ref count, otherwise the texture will leak when switching between maps
                        if (texture)
                        {
                            texture->Release();
                        }
                    }
                }
            }
        }
    }
}

void CMatInfo::RequestTexturesLoading(const float fMipFactor)
{
    PrecacheTextures(fMipFactor, FPR_STARTLOADING, false);
}

void CMatInfo::PrecacheTextures(const float fMipFactor, const int nFlags, bool bFullUpdate)
{
    SStreamingPredictionZone& rZone = m_streamZoneInfo[bFullUpdate ? 1 : 0];
    int bHighPriority = (nFlags & FPR_HIGHPRIORITY) != 0;

    rZone.fMinMipFactor = min(rZone.fMinMipFactor, fMipFactor);
    rZone.bHighPriority |= bHighPriority;

    int nRoundId = bFullUpdate ? GetObjManager()->m_nUpdateStreamingPrioriryRoundIdFast : GetObjManager()->m_nUpdateStreamingPrioriryRoundId;

    // TODO: fix fast update
    if (rZone.nRoundId != nRoundId)
    {
        int nCurrentFlags = Get3DEngine()->IsShadersSyncLoad() ? FPR_SYNCRONOUS : 0;
        nCurrentFlags |= bFullUpdate ? FPR_SINGLE_FRAME_PRIORITY_UPDATE : 0;

        SShaderItem& rSI = m_shaderItem;
        if (rSI.m_pShader && rSI.m_pShaderResources && !(rSI.m_pShader->GetFlags() & EF_NODRAW))
        {
            if (rZone.nRoundId == (nRoundId - 1))
            {
                nCurrentFlags |= rZone.bHighPriority ? FPR_HIGHPRIORITY : 0;
                GetRenderer()->EF_PrecacheResource(&rSI, rZone.fMinMipFactor, 0, nCurrentFlags, nRoundId, 1); // accumulated value is valid
            }
            else
            {
                nCurrentFlags |= (nFlags & FPR_HIGHPRIORITY);
                GetRenderer()->EF_PrecacheResource(&rSI, fMipFactor, 0, nCurrentFlags, nRoundId, 1); // accumulated value is not valid, pass current value
            }
        }

        rZone.nRoundId = nRoundId;
        rZone.fMinMipFactor = fMipFactor;
        rZone.bHighPriority = bHighPriority;
    }
}

void CMatInfo::PrecacheChunkTextures(const float fMipFactorDef, const int nFlags, CRenderChunk* pRenderChunk, bool bFullUpdate)
{
    //  FUNCTION_PROFILER_3DENGINE;

    CMatInfo* pMaterial = this;

    if (pRenderChunk && pRenderChunk->pRE && pRenderChunk->nNumIndices && pRenderChunk->nNumVerts)
    { // chunk is defined and have valid geometry
        if (pRenderChunk->m_nMatID < (uint16)m_subMtls.size())
        {
            pMaterial = (CMatInfo*)m_subMtls[pRenderChunk->m_nMatID];
        }

        if (pMaterial != NULL)
        {
            float fMipFactor = GetCVars()->e_StreamPredictionTexelDensity ? (fMipFactorDef * pRenderChunk->m_texelAreaDensity) : fMipFactorDef;
            pMaterial->PrecacheTextures(fMipFactor, nFlags, bFullUpdate);
        }
    }
    else if (!pRenderChunk)
    { // chunk is not set - load all sub-materials
        float fMipFactor = fMipFactorDef;

        PrecacheTextures(fMipFactor, nFlags, bFullUpdate);

        for (int nSubMtl = 0; nSubMtl < (int)m_subMtls.size(); ++nSubMtl)
        {
            pMaterial = m_subMtls[nSubMtl];

            if (pMaterial != NULL)
            {
                pMaterial->PrecacheTextures(fMipFactor, nFlags, bFullUpdate);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
int CMatInfo::GetTextureMemoryUsage(ICrySizer* pSizer, int nSubMtlSlot)
{
    int textureSize = 0;
    std::set<ITexture*> used;

    int nSlotStart = 0;
    int nSlotEnd = (int)m_subMtls.size();

    if (nSubMtlSlot >= 0)
    {
        nSlotStart = nSubMtlSlot;
        nSlotEnd = nSubMtlSlot + 1;
    }
    if (nSlotEnd >= (int)m_subMtls.size())
    {
        nSlotEnd = (int)m_subMtls.size();
    }

    if (nSlotEnd == 0)
    {
        nSlotEnd = 1;
    }

    for (int i = nSlotStart; i < nSlotEnd; i++)
    {
        IRenderShaderResources* pRes = m_shaderItem.m_pShaderResources;
        if (i < (int)m_subMtls.size() && m_subMtls[i] != NULL && (m_Flags & MTL_FLAG_MULTI_SUBMTL))
        {
            SShaderItem shaderItem = m_subMtls[i]->m_shaderItem;
            if (!shaderItem.m_pShaderResources)
            {
                continue;
            }
            pRes = shaderItem.m_pShaderResources;
        }
        if (!pRes)
        {
            continue;
        }

        for (int j = 0; j < EFTT_MAX; j++)
        {
            SEfResTexture* pResTexure = pRes->GetTexture(j);
            if (!pResTexure || !pResTexure->m_Sampler.m_pITex)
            {
                continue;
            }

            ITexture* pTexture = pResTexure->m_Sampler.m_pITex;
            if (!pTexture)
            {
                continue;
            }

            if (used.find(pTexture) != used.end()) // Already used in size calculation.
            {
                continue;
            }
            used.insert(pTexture);

            int nTexSize = pTexture->GetDataSize();
            textureSize += nTexSize;

            if (pSizer)
            {
                pSizer->AddObject(pTexture, nTexSize);
            }
        }
    }

    return textureSize;
}

void CMatInfo::SetKeepLowResSysCopyForDiffTex()
{
    int nSlotStart = 0;
    int nSlotEnd = (int)m_subMtls.size();

    if (nSlotEnd == 0)
    {
        nSlotEnd = 1;
    }

    for (int i = nSlotStart; i < nSlotEnd; i++)
    {
        IRenderShaderResources* pRes = m_shaderItem.m_pShaderResources;

        if (i < (int)m_subMtls.size() && m_subMtls[i] != NULL && (m_Flags & MTL_FLAG_MULTI_SUBMTL))
        {
            SShaderItem shaderItem = m_subMtls[i]->m_shaderItem;
            if (!shaderItem.m_pShaderResources)
            {
                continue;
            }
            pRes = shaderItem.m_pShaderResources;
        }

        if (!pRes)
        {
            continue;
        }

        int j = EFTT_DIFFUSE;
        {
            SEfResTexture* pResTexure = pRes->GetTexture(j);
            if (!pResTexure || !pResTexure->m_Sampler.m_pITex)
            {
                continue;
            }
            ITexture* pTexture = pResTexure->m_Sampler.m_pITex;
            if (!pTexture)
            {
                continue;
            }

            pTexture->SetKeepSystemCopy(true);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::SetMaterialLinkName(const char* name)
{
#ifdef SUPPORT_MATERIAL_EDITING
    if (name)
    {
        m_sMaterialLinkName = name;
    }
    else
    {
        m_sMaterialLinkName.clear();
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
const char* CMatInfo::GetMaterialLinkName() const
{
#ifdef SUPPORT_MATERIAL_EDITING
    return m_sMaterialLinkName.c_str();
#else
    CryFatalError("CMatInfo::GetMaterialLinkName not supported on this platform");
    return "";
#endif
}

//////////////////////////////////////////////////////////////////////////
CryCriticalSection& CMatInfo::GetSubMaterialResizeLock()
{
    static CryCriticalSection s_sSubMaterialResizeLock;
    return s_sSubMaterialResizeLock;
}

//////////////////////////////////////////////////////////////////////////
void CMatInfo::UpdateShaderItems()
{
    IRenderer* pRenderer = gEnv->pRenderer;
    pRenderer->UpdateShaderItem(&m_shaderItem);

    for (int  i = 0; i < (int)m_subMtls.size(); ++i)
    {
        CMatInfo* pSubMaterial = m_subMtls[i];
        if (pSubMaterial)
        {
            pRenderer->UpdateShaderItem(&pSubMaterial->m_shaderItem);
        }
    }
}

void CMatInfo::RefreshShaderResourceConstants()
{
    IRenderer* pRenderer = gEnv->pRenderer;
    pRenderer->RefreshShaderResourceConstants(&m_shaderItem);

    for (int  i = 0; i < (int)m_subMtls.size(); ++i)
    {
        CMatInfo* pSubMaterial = m_subMtls[i];
        if (pSubMaterial)
        {
            pRenderer->RefreshShaderResourceConstants(&pSubMaterial->m_shaderItem);
        }
    }
}

