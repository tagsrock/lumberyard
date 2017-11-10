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

#include <LyShine/Animation/IUiAnimation.h>

class CUiAnimSequence
    : public IUiAnimSequence
{
public:
    AZ_CLASS_ALLOCATOR(CUiAnimSequence, AZ::SystemAllocator, 0)
    AZ_RTTI(CUiAnimSequence, "{AA5AB4ED-CB98-4166-953E-0FE1EF7AC61F}", IUiAnimSequence);

    CUiAnimSequence();  // required for serialization
    CUiAnimSequence(IUiAnimationSystem* pUiAnimationSystem, uint32 id);
    ~CUiAnimSequence();

    //////////////////////////////////////////////////////////////////////////
    void add_ref() override;
    void release() override;
    //////////////////////////////////////////////////////////////////////////

    // Animation system.
    IUiAnimationSystem* GetUiAnimationSystem() const override { return m_pUiAnimationSystem; };

    void SetName(const char* name);
    const char* GetName() const;
    uint32 GetId() const { return m_id; }

    float GetTime() const { return m_time; }

    float GetFixedTimeStep() const { return m_fixedTimeStep; }
    void SetFixedTimeStep(float dt) { m_fixedTimeStep = dt; }

    virtual void SetOwner(IUiAnimSequenceOwner* pOwner)
    { m_pOwner = pOwner; }
    virtual IUiAnimSequenceOwner* GetOwner() const
    { return m_pOwner; }

    virtual void SetActiveDirector(IUiAnimNode* pDirectorNode);
    virtual IUiAnimNode* GetActiveDirector() const;

    virtual void SetFlags(int flags);
    virtual int GetFlags() const;
    virtual int GetCutSceneFlags(const bool localFlags = false) const;

    virtual void SetParentSequence(IUiAnimSequence* pParentSequence);
    virtual const IUiAnimSequence* GetParentSequence() const;
    virtual bool IsAncestorOf(const IUiAnimSequence* pSequence) const;

    void SetTimeRange(Range timeRange);
    Range GetTimeRange() { return m_timeRange; };

    void AdjustKeysToTimeRange(const Range& timeRange);

    //! Return number of animation nodes in sequence.
    int GetNodeCount() const;
    //! Get specified animation node.
    IUiAnimNode* GetNode(int index) const;

    IUiAnimNode* FindNodeByName(const char* sNodeName, const IUiAnimNode* pParentDirector);
    IUiAnimNode* FindNodeById(int nNodeId);
    virtual void ReorderNode(IUiAnimNode* node, IUiAnimNode* pPivotNode, bool next);

    void Reset(bool bSeekToStart);
    void ResetHard();
    void Pause();
    void Resume();
    bool IsPaused() const;

    virtual void OnStart();
    virtual void OnStop();
    void OnLoop() override;

    //! Add animation node to sequence.
    bool AddNode(IUiAnimNode* node);
    IUiAnimNode* CreateNode(EUiAnimNodeType nodeType);
    IUiAnimNode* CreateNode(XmlNodeRef node);
    void RemoveNode(IUiAnimNode* node);
    //! Add scene node to sequence.
    void RemoveAll();

    virtual void Activate();
    virtual bool IsActivated() const { return m_bActive; }
    virtual void Deactivate();

    virtual void PrecacheData(float startTime);
    void PrecacheStatic(const float startTime);
    void PrecacheDynamic(float time);
    void PrecacheEntity(IEntity* pEntity);

    void StillUpdate();
    void Animate(const SUiAnimContext& ec);
    void Render();

    void Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true, uint32 overrideId = 0, bool bResetLightAnimSet = false);
    void InitPostLoad(IUiAnimationSystem* pUiAnimationSystem, bool remapIds, LyShine::EntityIdMap* entityIdMap);

    void CopyNodes(XmlNodeRef& xmlNode, IUiAnimNode** pSelectedNodes, uint32 count);
    void PasteNodes(const XmlNodeRef& xmlNode, IUiAnimNode* pParent);

    static void Reflect(AZ::SerializeContext* serializeContext);

private:
    void ComputeTimeRange();
    void CopyNodeChildren(XmlNodeRef& xmlNode, IUiAnimNode* pAnimNode);

    // Create a new animation node.
    IUiAnimNode* CreateNodeInternal(EUiAnimNodeType nodeType, uint32 nNodeId = -1);

    bool AddNodeNeedToRender(IUiAnimNode* pNode);
    void RemoveNodeNeedToRender(IUiAnimNode* pNode);

    int m_refCount;

    typedef AZStd::vector< AZStd::intrusive_ptr<IUiAnimNode> > AnimNodes;
    AnimNodes m_nodes;
    AnimNodes m_nodesNeedToRender;

    uint32 m_id;
    AZStd::string m_name;
    mutable string m_fullNameHolder;
    Range m_timeRange;

    // Listeners
    int m_flags;

    bool m_precached;
    bool m_bResetting;

    IUiAnimSequence* m_pParentSequence;

    //
    IUiAnimationSystem* m_pUiAnimationSystem;
    bool m_bPaused;
    bool m_bActive;

    uint32 m_lastGenId;

    IUiAnimSequenceOwner* m_pOwner;

    IUiAnimNode* m_pActiveDirector;

    float m_time;
    float m_fixedTimeStep;

    VectorSet<IEntity*> m_precachedEntitiesSet;
};
