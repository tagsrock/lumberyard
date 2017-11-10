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
#include "DecalRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "terrain.h"

int CDecalRenderNode::m_nFillBigDecalIndicesCounter = 0;

CDecalRenderNode::CDecalRenderNode()
    : m_pos(0, 0, 0)
    , m_localBounds(Vec3(-1, -1, -1), Vec3(1, 1, 1))
    , m_pMaterial(NULL)
    , m_updateRequested(false)
    , m_decalProperties()
    , m_decals()
    , m_nLastRenderedFrameId(0)
    , m_nLayerId(0)
{
    m_Matrix.SetIdentity();
}


CDecalRenderNode::~CDecalRenderNode()
{
    DeleteDecals();
    Get3DEngine()->FreeRenderNodeState(this);
}


const SDecalProperties* CDecalRenderNode::GetDecalProperties() const
{
    return &m_decalProperties;
}

void CDecalRenderNode::DeleteDecals()
{
    for (size_t i(0); i < m_decals.size(); ++i)
    {
        delete m_decals[ i ];
    }

    m_decals.resize(0);
}


void CDecalRenderNode::CreatePlanarDecal()
{
    CryEngineDecalInfo decalInfo;

    // necessary params
    decalInfo.vPos = m_decalProperties.m_pos;
    decalInfo.vNormal = m_decalProperties.m_normal;
    decalInfo.fSize = m_decalProperties.m_radius;
    decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
    strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
    decalInfo.sortPrio = m_decalProperties.m_sortPrio;

    // default for all other
    decalInfo.pIStatObj = NULL;
    decalInfo.ownerInfo.pRenderNode = NULL;
    decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
    decalInfo.vHitDirection = Vec3(0, 0, 0);
    decalInfo.fGrowTime = 0;
    decalInfo.preventDecalOnGround = true;
    decalInfo.fAngle = 0;

    CDecal* pDecal(new CDecal);
    if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
    {
        m_decals.push_back(pDecal);
    }
    else
    {
        delete pDecal;
    }
}

void CDecalRenderNode::CreateDecalOnTerrain()
{
    float terrainHeight(GetTerrain()->GetBilinearZ(m_decalProperties.m_pos.x, m_decalProperties.m_pos.y));
    float terrainDelta(m_decalProperties.m_pos.z - terrainHeight);
    if (terrainDelta < m_decalProperties.m_radius && terrainDelta > -0.5f)
    {
        CryEngineDecalInfo decalInfo;

        // necessary params
        decalInfo.vPos = Vec3(m_decalProperties.m_pos.x, m_decalProperties.m_pos.y, terrainHeight);
        decalInfo.vNormal = Vec3(0, 0, 1);
        decalInfo.vHitDirection = Vec3(0, 0, -1);
        decalInfo.fSize = m_decalProperties.m_radius;// - terrainDelta;
        decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
        strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
        decalInfo.sortPrio = m_decalProperties.m_sortPrio;

        // default for all other
        decalInfo.pIStatObj = NULL;
        decalInfo.ownerInfo.pRenderNode = 0;
        decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
        decalInfo.fGrowTime = 0;
        decalInfo.preventDecalOnGround = false;
        decalInfo.fAngle = 0;

        CDecal* pDecal(new CDecal);
        if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
        {
            m_decals.push_back(pDecal);
        }
        else
        {
            delete pDecal;
        }
    }
}


void CDecalRenderNode::CreateDecals()
{
    DeleteDecals();

    if (m_decalProperties.m_deferred)
    {
        return;
    }

    _smart_ptr<IMaterial> pMaterial(GetMaterial());

    assert(0 != pMaterial && "CDecalRenderNode::CreateDecals() -- Invalid Material!");
    if (!pMaterial)
    {
        return;
    }

    switch (m_decalProperties.m_projectionType)
    {
    case SDecalProperties::ePlanar:
    {
        CreatePlanarDecal();
        break;
    }
    case SDecalProperties::eProjectOnTerrain:
    {
        CreateDecalOnTerrain();
        break;
    }
    default:
    {
        assert(!"CDecalRenderNode::CreateDecals() : Unsupported decal projection type!");
        break;
    }
    }
}


void CDecalRenderNode::ProcessUpdateRequest()
{
    if (!m_updateRequested || m_nFillBigDecalIndicesCounter >= GetCVars()->e_DecalsMaxUpdatesPerFrame)
    {
        return;
    }

    CreateDecals();
    m_updateRequested = false;
}

void CDecalRenderNode::UpdateAABBFromRenderMeshes()
{
    if (m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrain)
    {
        AABB WSBBox;
        WSBBox.Reset();
        for (size_t i(0); i < m_decals.size(); ++i)
        {
            CDecal* pDecal(m_decals[ i ]);
            if (pDecal && pDecal->m_pRenderMesh && pDecal->m_eDecalType != eDecalType_OS_OwnersVerticesUsed)
            {
                AABB aabb;
                pDecal->m_pRenderMesh->GetBBox(aabb.min, aabb.max);
                if (pDecal->m_eDecalType == eDecalType_WS_Merged || pDecal->m_eDecalType == eDecalType_WS_OnTheGround)
                {
                    aabb.min += pDecal->m_vPos;
                    aabb.max += pDecal->m_vPos;
                }
                WSBBox.Add(aabb);
            }
        }

        if (!WSBBox.IsReset())
        {
            m_WSBBox = WSBBox;
        }
    }
}

//special check for def decals forcing
bool CDecalRenderNode::CheckForceDeferred()
{
    if (m_pMaterial != NULL)
    {
        SShaderItem& sItem = m_pMaterial->GetShaderItem(0);
        if (sItem.m_pShaderResources != NULL)
        {
            float fCosA =  m_decalProperties.m_normal.GetNormalized().Dot(Vec3(0, 0, 1));
            if (fCosA > 0.5f)
            {
                return false;
            }

            if (SEfResTexture* pEnvRes0 = sItem.m_pShaderResources->GetTexture(EFTT_ENV))
            {
                if (pEnvRes0->m_Sampler.m_pITex == NULL)
                {
                    m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
                    m_decalProperties.m_deferred = true;
                    return true;
                }
            }
            else
            {
                m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
                m_decalProperties.m_deferred = true;
                return true;
            }
        }
    }
    return false;
}

void CDecalRenderNode::SetDecalProperties(const SDecalProperties& properties)
{
    // update bounds
    m_localBounds = AABB(-properties.m_radius * Vec3(1, 1, 1), properties.m_radius * Vec3(1, 1, 1));

    // register material
    m_pMaterial = GetMatMan()->LoadMaterial(properties.m_pMaterialName, false);

    // copy decal properties
    m_decalProperties = properties;
    m_decalProperties.m_pMaterialName = 0; // reset this as it's assumed to be a temporary pointer only, refer to m_materialID to get material

    // request update
    m_updateRequested = true;

    bool bForced = 0;

    if (properties.m_deferred || (GetCVars()->e_DecalsDefferedStatic && (m_decalProperties.m_projectionType != SDecalProperties::ePlanar && m_decalProperties.m_projectionType != SDecalProperties::eProjectOnTerrain)))
    {
        m_decalProperties.m_deferred = true;
    }

    if (GetCVars()->e_DecalsForceDeferred)
    {
        if (CheckForceDeferred())
        {
            bForced = true;
        }
    }

    // set normal just in case; normal direction will be determined by m_explicitRightUpFront
    m_decalProperties.m_normal = properties.m_normal;

    m_fWSMaxViewDist = properties.m_maxViewDist;

    // set matrix
    m_Matrix.SetRotation33(m_decalProperties.m_explicitRightUpFront);
    Matrix33 matScale;
    if (bForced && !properties.m_deferred)
    {
        matScale.SetScale(Vec3(properties.m_radius, properties.m_radius, properties.m_radius * 0.05f));
    }
    else
    {
        matScale.SetScale(Vec3(properties.m_radius, properties.m_radius, properties.m_radius * properties.m_depth));
    }

    m_Matrix = m_Matrix * matScale;
    m_Matrix.SetTranslation(properties.m_pos);
}

IRenderNode* CDecalRenderNode::Clone() const
{
    CDecalRenderNode* pDestDecal = new CDecalRenderNode();

    // CDecalRenderNode member vars
    pDestDecal->m_pos = m_pos;
    pDestDecal->m_localBounds = m_localBounds;
    pDestDecal->m_pMaterial = m_pMaterial;
    pDestDecal->m_updateRequested = true;
    pDestDecal->m_decalProperties = m_decalProperties;
    pDestDecal->m_WSBBox = m_WSBBox;
    pDestDecal->m_Matrix = m_Matrix;
    pDestDecal->m_nLayerId = m_nLayerId;

    //IRenderNode member vars
    //  We cannot just copy over due to issues with the linked list of IRenderNode objects
    CopyIRenderNodeData(pDestDecal);

    return pDestDecal;
}

void CDecalRenderNode::SetMatrix(const Matrix34& mat)
{
    m_pos = mat.GetTranslation();

    if (m_decalProperties.m_projectionType == SDecalProperties::ePlanar)
    {
        m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 0.5f), Vec3(1, 1, 0.5f)));
    }
    else
    {
        m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 1), Vec3(1, 1, 1)));
    }

    Get3DEngine()->RegisterEntity(this);
}

void CDecalRenderNode::SetMatrixFull(const Matrix34& mat)
{
    m_Matrix = mat;
    m_pos = mat.GetTranslation();

    if (m_decalProperties.m_projectionType == SDecalProperties::ePlanar)
    {
        m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 0.5f), Vec3(1, 1, 0.5f)));
    }
    else
    {
        m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 1), Vec3(1, 1, 1)));
    }
}

const char* CDecalRenderNode::GetEntityClassName() const
{
    return "Decal";
}


const char* CDecalRenderNode::GetName() const
{
    return "Decal";
}


void CDecalRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
    FUNCTION_PROFILER_3DENGINE;

    if (!passInfo.RenderDecals())
    {
        return; // false;
    }

    float distFading = SATURATE((1.f - rParam.fDistance / m_fWSMaxViewDist) * DIST_FADING_FACTOR);

    if (m_decalProperties.m_deferred)
    {
        if (passInfo.IsShadowPass())
        {
            return; // otherwise causing flickering with GI
        }
        SDeferredDecal newItem;
        newItem.fAlpha = m_decalProperties.m_opacity;
        newItem.pMaterial = m_pMaterial;
        newItem.projMatrix = m_Matrix;
        newItem.nSortOrder = m_decalProperties.m_sortPrio;
        newItem.nFlags = DECAL_STATIC;
        GetRenderer()->EF_AddDeferredDecal(newItem);
        return;
    }

    // update last rendered frame id
    m_nLastRenderedFrameId = passInfo.GetMainFrameID();

    bool bUpdateAABB = m_updateRequested;

    if (passInfo.IsGeneralPass())
    {
        ProcessUpdateRequest();
    }

    float waterLevel(m_p3DEngine->GetWaterLevel());
    for (size_t i(0); i < m_decals.size(); ++i)
    {
        CDecal* pDecal(m_decals[i]);
        if (pDecal && 0 != pDecal->m_pMaterial)
        {
            pDecal->m_vAmbient.x = rParam.AmbientColor.r;
            pDecal->m_vAmbient.y = rParam.AmbientColor.g;
            pDecal->m_vAmbient.z = rParam.AmbientColor.b;
            bool bAfterWater = CObjManager::IsAfterWater(pDecal->m_vWSPos, passInfo.GetCamera().GetPosition(), passInfo, waterLevel);
            pDecal->Render(0, bAfterWater, distFading, rParam.fDistance, passInfo, SRendItemSorter(rParam.rendItemSorter));
        }
    }

    // terrain decal meshes are created only during rendering so only after that bbox can be computed
    if (bUpdateAABB)
    {
        UpdateAABBFromRenderMeshes();
    }
}


IPhysicalEntity* CDecalRenderNode::GetPhysics() const
{
    return 0;
}


void CDecalRenderNode::SetPhysics(IPhysicalEntity*)
{
}


void CDecalRenderNode::SetMaterial(_smart_ptr<IMaterial> pMat)
{
    for (size_t i(0); i < m_decals.size(); ++i)
    {
        CDecal* pDecal(m_decals[ i ]);
        if (pDecal)
        {
            pDecal->m_pMaterial = pMat;
        }
    }

    m_pMaterial = pMat;

    //special check for def decals forcing
    if (GetCVars()->e_DecalsForceDeferred)
    {
        CheckForceDeferred();
    }
}



void CDecalRenderNode::Precache()
{
    ProcessUpdateRequest();
}


void CDecalRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
    SIZER_COMPONENT_NAME(pSizer, "DecalNode");
    pSizer->AddObject(this, sizeof(*this));
    pSizer->AddObject(m_decals);
}

void CDecalRenderNode::CleanUpOldDecals()
{
    if (m_nLastRenderedFrameId != 0 && // was rendered at least once
        (int)GetRenderer()->GetFrameID(false) > (int)m_nLastRenderedFrameId + GetCVars()->e_DecalsMaxValidFrames)
    {
        DeleteDecals();
        m_nLastRenderedFrameId = 0;
        m_updateRequested = true; // make sure if rendered again, that the decal is recreated
    }
}

void CDecalRenderNode::OffsetPosition(const Vec3& delta)
{
    if (m_pRNTmpData)
    {
        m_pRNTmpData->OffsetPosition(delta);
    }
    m_pos += delta;
    m_WSBBox.Move(delta);
    m_Matrix.SetTranslation(m_Matrix.GetTranslation() + delta);
}
