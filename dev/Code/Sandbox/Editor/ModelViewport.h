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

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   ModelViewport.h
//  Version:     v1.00
//  Created:     8/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef CRYINCLUDE_EDITOR_MODELVIEWPORT_H
#define CRYINCLUDE_EDITOR_MODELVIEWPORT_H
#include "RenderViewport.h"
#include "Material/Material.h"
#include <ICryAnimation.h>
#include <IInput.h>
#include <IEntitySystem.h>
#include "Util/Variable.h"

struct IPhysicalEntity;
struct CryCharAnimationParams;
struct ISkeletonAnim;
class CAnimationSet;

/////////////////////////////////////////////////////////////////////////////
// TODO: Merge this code with copied code from CryAction
class CAnimatedCharacterEffectManager
    : public IEditorNotifyListener
{
public:
    CAnimatedCharacterEffectManager();
    ~CAnimatedCharacterEffectManager();

    void SetSkeleton(ISkeletonAnim* pSkeletonAnim, ISkeletonPose* pSkeletonPose, const IDefaultSkeleton* pIDefaultSkeleton);
    void Update(const QuatT& rPhysEntity);
    void SpawnEffect(int animID, const char* animName, const char* effectName, const char* boneName, const char* secondBoneName, const Vec3& offset, const Vec3& dir);
    void KillAllEffects();
    void Render(SRendParams& params, const SRenderingPassInfo& passInfo);

    // IEditorNotifyListener
    virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
    // ~IEditorNotifyListener

private:
    struct EffectEntry
    {
        EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, int secondBoneID, const Vec3& offset, const Vec3& dir, int animID);
        ~EffectEntry();

        _smart_ptr<IParticleEffect> pEffect;
        _smart_ptr<IParticleEmitter> pEmitter;
        int boneID;
        int secondBoneID;
        Vec3 offset;
        Vec3 dir;
        int animID;
    };

    void GetEffectLoc(QuatTS& loc, int boneID, int secondBoneID, const Vec3& offset, const Vec3& dir);
    bool IsPlayingAnimation(int animID);
    bool IsPlayingEffect(const char* effectName);

    ISkeletonAnim* m_pISkeletonAnim;
    ISkeletonPose* m_pISkeletonPose;
    IDefaultSkeleton* m_pIDefaultSkeleton;
    DynArray<EffectEntry> m_effects;

    /// Logs information relating to animation effects spawning/destruction.
    static int m_debugAnimationEffects;
};

/////////////////////////////////////////////////////////////////////////////
// CModelViewport window
class SANDBOX_API CModelViewport
    : public CRenderViewport
    , public IInputEventListener
    , public IEntityEventListener
{
    Q_OBJECT
    // Construction
public:
    CModelViewport(const char* settingsPath = "Settings\\CharacterEditorUserOptions", QWidget* parent = nullptr);
    virtual ~CModelViewport();

    virtual EViewportType GetType() const { return ET_ViewportModel; }
    virtual void SetType(EViewportType type) { assert(type == ET_ViewportModel); };

    virtual void LoadObject(const QString& obj, float scale);

    virtual void OnActivate();
    virtual void OnDeactivate();

    virtual bool CanDrop(const QPoint& point, IDataBaseItem* pItem);
    virtual void Drop(const QPoint& point, IDataBaseItem* pItem);

    virtual void SetSelected(bool const bSelect);

    void AttachObjectToBone(const QString& model, const QString& bone);
    void AttachObjectToFace(const QString& model);

    // Callbacks.
    void OnShowShaders(IVariable* var);
    void OnShowNormals(IVariable* var);
    void OnShowTangents(IVariable* var);

    void OnShowPortals(IVariable* var);
    void OnShowShadowVolumes(IVariable* var);
    void OnShowTextureUsage(IVariable* var);
    void OnCharPhysics(IVariable* var);
    void OnShowOcclusion(IVariable* var);

    void OnLightColor(IVariable* var);
    void OnLightMultiplier(IVariable* var);
    void OnDisableVisibility(IVariable* var);

    ICharacterInstance* GetCharacterBase()
    {
        return m_pCharacterBase;
    }

    IStatObj* GetStaticObject(){ return m_object; }

    void GetOnDisableVisibility(IVariable* var);

    const CVarObject* GetVarObject() const { return &m_vars; }
    CVarObject* GetVarObject() { return &m_vars; }

    virtual void Update();


    void UseWeaponIK(bool val) { m_weaponIK = true; }

    // Set current material to render object.
    void SetCustomMaterial(CMaterial* pMaterial);
    // Get custom material that object is rendered with.
    CMaterial* GetCustomMaterial() { return m_pCurrentMaterial; };

    ICharacterManager* GetAnimationSystem() { return m_pAnimationSystem; };

    // Get material the object is actually rendered with.
    CMaterial* GetMaterial();

    void ReleaseObject();
    void RePhysicalize();

    Vec3 m_GridOrigin;
    _smart_ptr<ICharacterInstance> m_pCharacterBase;

    void SetPaused(bool bPaused);
    bool GetPaused() {return m_bPaused; }

    bool IsCameraAttached() const{ return mv_AttachCamera; }

    virtual void PlayAnimation(const char* szName);

    const QString& GetLoadedFileName() const { return m_loadedFile; }

    void Physicalize();

    virtual void OnEntityEvent(IEntity* pEntity, SEntityEvent& event);

protected:

    void LoadStaticObject(const QString& file);

    // Called to render stuff.
    virtual void OnRender();

    virtual void DrawFloorGrid(const Quat& tmRotation, const Vec3& MotionTranslation, const Matrix33& rGridRot);
    void DrawCoordSystem(const QuatT& q, f32 length);

    void SaveDebugOptions() const;
    void RestoreDebugOptions();

    virtual void DrawModel(const SRenderingPassInfo& passInfo);
    virtual void DrawLights(const SRenderingPassInfo& passInfo);
    virtual void DrawSkyBox(const SRenderingPassInfo& passInfo);

    //This implementation is dangerous. If we change or rename the specialized function, we use this as fallback and don't execute anything
    virtual void DrawCharacter(ICharacterInstance* pInstance, const SRendParams& rp, const SRenderingPassInfo& pass)
    {
        CryFatalError("never execute the base-function");  //make sure you execute the overloaded version;
    }

    void DrawInfo() const;

    void SetConsoleVar(const char* var, int value);

    void OnEditorNotifyEvent(EEditorNotifyEvent event)
    {
        if (event != eNotify_OnBeginGameMode)
        {
            // the base class responds to this by forcing itself to be the current context.
            // we don't want that to be the case for previewer viewports.
            CRenderViewport::OnEditorNotifyEvent(event);
        }
    }


    //virtual bool OnInputEvent( const SInputEvent &event ) = 0;
    bool OnInputEvent(const SInputEvent& rInputEvent);
    void CreateAudioListener();

    f32 m_RT;
    Vec2 m_LTHUMB;
    Vec2 m_RTHUMB;

    Vec2 m_arrLTHUMB[0x100];

    IStatObj* m_object;
    IStatObj* m_weaponModel;
    // this is the character to attach, instead of weaponModel
    _smart_ptr<ICharacterInstance> m_attachedCharacter;

    QString m_attachBone;
    AABB m_AABB;

    struct BBox
    {
        OBB obb;
        Vec3 pos;
        ColorB col;
    };
    std::vector<BBox>  m_arrBBoxes;

    // Camera control.
    float m_camRadius;

    // True to show grid.
    bool m_bGrid;
    bool m_bBase;

    QString m_settingsPath;

    int m_rollupIndex;
    int m_rollupIndex2;

    bool m_weaponIK;

    ICharacterManager* m_pAnimationSystem;

    QString m_loadedFile;
    std::vector<CDLight> m_VPLights;

    f32 m_LightRotationRadian;

    class CRESky* m_pRESky;
    struct ICVar* m_pSkyboxName;
    IShader* m_pSkyBoxShader;
    _smart_ptr<CMaterial> m_pCurrentMaterial;

    // Audio
    IEntity*    m_pAudioListener = nullptr;

    //---------------------------------------------------
    //---    debug options                            ---
    //---------------------------------------------------
    CVariable<bool> mv_showGrid;
    CVariable<bool> mv_showBase;
    CVariable<bool> mv_showLocator;
    CVariable<bool> mv_InPlaceMovement;
    CVariable<bool> mv_StrafingControl;

    CVariable<bool> mv_showWireframe1;  //draw wireframe instead of solid-geometry.
    CVariable<bool> mv_showWireframe2;  //this one is software-wireframe rendered on top of the solid geometry
    CVariable<bool> mv_showTangents;
    CVariable<bool> mv_showBinormals;
    CVariable<bool> mv_showNormals;

    CVariable<bool> mv_showSkeleton;
    CVariable<bool> mv_showJointNames;
    CVariable<bool> mv_showJointsValues;
    CVariable<bool> mv_showStartLocation;
    CVariable<bool> mv_showMotionParam;
    CVariable<float> mv_UniformScaling;

    CVariable<bool> mv_printDebugText;
    CVariable<bool> mv_AttachCamera;

    CVariable<bool> mv_showShaders;

    CVariable<bool> mv_lighting;
    CVariable<bool> mv_animateLights;

    CVariable<Vec3> mv_backgroundColor;
    CVariable<Vec3> mv_objectAmbientColor;

    CVariable<Vec3> mv_lightDiffuseColor0;
    CVariable<Vec3> mv_lightDiffuseColor1;
    CVariable<Vec3> mv_lightDiffuseColor2;
    CVariable<float> mv_lightMultiplier;
    CVariable<float> mv_lightSpecMultiplier;
    CVariable<float> mv_lightRadius;
    CVariable<float> mv_lightOrbit;

    CVariable<float> mv_fov;
    CVariable<bool> mv_showPhysics;
    CVariable<bool> mv_useCharPhysics;
    CVariable<bool> mv_showPhysicsTetriders;
    CVariable<int> mv_forceLODNum;

    CVariableArray mv_advancedTable;

    CVarObject m_vars;

public:
    IPhysicalEntity* m_pPhysicalEntity;
public slots:
    virtual void OnAnimPlay();
    virtual void OnAnimBack();
    virtual void OnAnimFastBack();
    virtual void OnAnimFastForward();
    virtual void OnAnimFront();
protected:
    CAnimatedCharacterEffectManager m_effectManager;

    bool m_bPaused;

    void OnDestroy();
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
};

#endif // CRYINCLUDE_EDITOR_MODELVIEWPORT_H
