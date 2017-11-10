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

#ifndef CRYINCLUDE_CRY3DENGINE_DECALMANAGER_H
#define CRYINCLUDE_CRY3DENGINE_DECALMANAGER_H
#pragma once

#define DECAL_COUNT (512) // must be pow2
#define ENTITY_DECAL_DIST_FACTOR (200)
#define DIST_FADING_FACTOR (6.f)

class C3DEngine;

enum EDecal_Type
{
    eDecalType_Undefined,
    eDecalType_OS_OwnersVerticesUsed,
    eDecalType_WS_Merged,
    eDecalType_WS_OnTheGround,
    eDecalType_WS_SimpleQuad,
    eDecalType_OS_SimpleQuad
};

class CDecal
    : public Cry3DEngineBase
{
public:

    // cur state
    Vec3                    m_vPos;
    Vec3                    m_vRight, m_vUp, m_vFront;
    float                   m_fSize;
    Vec3                    m_vWSPos;                                                   // Decal position (world coordinates) from DecalInfo.vPos
    float                   m_fWSSize;                                              // Decal size (world coordinates) from DecalInfo.fSize

    // life style
    float                   m_fLifeTime;                                            // relative time left till decal should die
    Vec3                    m_vAmbient;                                             // ambient color
    SDecalOwnerInfo m_ownerInfo;
    EDecal_Type     m_eDecalType;
    float                   m_fGrowTime, m_fGrowTimeAlpha;                                          // e.g. growing blood pools
    float                   m_fLifeBeginTime;                                   //

    uint8                   m_iAssembleSize;                                    // of how many decals has this decal be assembled, 0 if not to assemble
    uint8                   m_sortPrio;
    uint8                   m_bDeferred;

    // render data
    _smart_ptr<IRenderMesh>      m_pRenderMesh;             // only needed for terrain decals, 4 of them because they might cross borders
    float                   m_arrBigDecalRMCustomData[16];      // only needed if one of m_arrBigDecalRMs[]!=0, most likely we can reduce to [12]

    _smart_ptr< IMaterial > m_pMaterial;
    uint32          m_nGroupId; // used for multi-component decals

#ifdef _DEBUG
    char m_decalOwnerEntityClassName[256];
    char m_decalOwnerName[256];
    EERType m_decalOwnerType;
#endif

    CDecal()
        : m_vPos(0, 0, 0)
        , m_vRight(0, 0, 0)
        , m_vUp(0, 0, 0)
        , m_vFront(0, 0, 0)
        , m_fSize(0)
        , m_vWSPos(0, 0, 0)
        , m_fWSSize(0)
        , m_fLifeTime(0)
        , m_vAmbient(0, 0, 0)
        , m_fGrowTime(0)
        , m_fGrowTimeAlpha(0)
        , m_fLifeBeginTime(0)
        , m_sortPrio(0)
        , m_pMaterial(0)
        , m_nGroupId(0)
        , m_iAssembleSize(0)
        , m_bDeferred(0)
    {
        m_eDecalType = eDecalType_Undefined;
        m_pRenderMesh = NULL;
        memset(&m_arrBigDecalRMCustomData[0], 0, sizeof(m_arrBigDecalRMCustomData));

#ifdef _DEBUG
        m_decalOwnerEntityClassName[0] = '\0';
        m_decalOwnerName[0] = '\0';
        m_decalOwnerType = eERType_NotRenderNode;
#endif
    }

    ~CDecal()
    {
        FreeRenderData();
    }

    void Render(const float fFrameTime, int nAfterWater, float fDistanceFading, float fDiatance, const SRenderingPassInfo& passInfo, const SRendItemSorter& rendItemSorter);
    int Update(bool& active, const float fFrameTime);
    void RenderBigDecalOnTerrain(float fAlpha, float fScale, const SRenderingPassInfo& passInfo);
    void FreeRenderData();
    static void ResetStaticData();
    bool IsBigDecalUsed() const { return m_pRenderMesh != 0; }
    Vec3 GetWorldPosition();

    void GetMemoryUsage(ICrySizer* pSizer) const
    {
        pSizer->AddObject(this, sizeof(*this));
    }

private:
    static IGeometry* s_pSphere;
};

class CDecalManager
    : public Cry3DEngineBase
{
    CDecal                      m_arrDecals[DECAL_COUNT];
    bool                            m_arrbActiveDecals[DECAL_COUNT];
    int                             m_nCurDecal;
    PodArray<IRenderNode*> m_arrTempUpdatedOwners;

public: // ---------------------------------------------------------------

    CDecalManager();
    ~CDecalManager();
    bool Spawn(CryEngineDecalInfo Decal, CDecal* pCallerManagedDecal = 0);
    // once per frame
    void Update(const float fFrameTime);
    // maybe multiple times per frame
    void Render(const SRenderingPassInfo& passInfo);
    void OnEntityDeleted(IRenderNode* pEnt);
    void OnRenderMeshDeleted(IRenderMesh* pRenderMesh);

    // complex decals
    void FillBigDecalIndices(IRenderMesh* pRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, PodArray<vtx_idx>* plstIndices, _smart_ptr<IMaterial> pMat, AABB& meshBBox, float& texelAreaDensity);
    _smart_ptr<IRenderMesh> MakeBigDecalRenderMesh(IRenderMesh* pSourceRenderMesh, Vec3 vPos, float fRadius, Vec3 vProjDir, _smart_ptr<IMaterial> pDecalMat, _smart_ptr<IMaterial> pSrcMat);
    void MoveToEdge(IRenderMesh* pRM, const float fRadius, Vec3& vPos, Vec3& vOutNorm, const Vec3& vTri0, const Vec3& vTri1, const Vec3& vTri2);
    void GetMemoryUsage(ICrySizer* pSizer) const;
    void Reset() { memset(m_arrbActiveDecals, 0, sizeof(m_arrbActiveDecals)); m_nCurDecal = 0; }
    void DeleteDecalsInRange(AABB* pAreaBox, IRenderNode* pEntity);
    bool AdjustDecalPosition(CryEngineDecalInfo& DecalInfo, bool bMakeFatTest);
    static bool RayRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const Vec3& vInDir, Vec3& vOutPos, Vec3& vOutNormal, bool bFastTest, float fMaxHitDistance, _smart_ptr<IMaterial> pMat);
    void Serialize(TSerialize ser);
    bool SpawnHierarchical(const CryEngineDecalInfo& rootDecalInfo, CDecal* pCallerManagedDecal);

private:
    _smart_ptr<IMaterial> GetMaterialForDecalTexture(const char* pTextureName);
};

#endif // CRYINCLUDE_CRY3DENGINE_DECALMANAGER_H
