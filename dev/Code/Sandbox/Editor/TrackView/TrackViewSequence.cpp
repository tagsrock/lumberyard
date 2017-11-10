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

#include <LmbrCentral/Cinematics/EditorSequenceComponentBus.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzToolsFramework/API/ComponentEntityObjectBus.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>

#include "TrackViewSequence.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewAnimNode.h"
#include "TrackViewUndo.h"
#include "TrackViewTrack.h"
#include "TrackViewNodeFactories.h"
#include "TrackViewSequence.h"
#include "AnimationContext.h"
#include "Objects/ObjectLayer.h"
#include "Objects/EntityObject.h"
#include "Clipboard.h"

#include <QMessageBox>

//////////////////////////////////////////////////////////////////////////
CTrackViewSequence::CTrackViewSequence(IAnimSequence* pSequence)
    : CTrackViewAnimNode(pSequence, nullptr, nullptr)
    , m_pAnimSequence(pSequence)
    , m_bBoundToEditorObjects(false)
    , m_selectionRecursionLevel(0)
    , m_bQueueNotifications(false)
    , m_bKeySelectionChanged(false)
    , m_bKeysChanged(false)
    , m_bForceAnimation(false)
    , m_bNodeSelectionChanged(false)
    , m_time(0.0f)
    , m_bNoNotifications(false)
{
    assert(m_pAnimSequence);

    GetIEditor()->GetSequenceManager()->AddListener(this);

    SetExpanded(true);
}

//////////////////////////////////////////////////////////////////////////
CTrackViewSequence::~CTrackViewSequence()
{
    GetIEditor()->GetSequenceManager()->RemoveListener(this);
    GetIEditor()->GetUndoManager()->RemoveListener(this);       // For safety. Should be done by OnRemoveSequence callback

    // For safety, disconnect to any buses we may have been listening on for record mode
    if (m_pAnimSequence && m_pAnimSequence->GetSequenceType() == eSequenceType_SequenceComponent)
    {
        // disconnect from all EBuses for notification of changes for all AZ::Entities in our sequence
        for (int i = m_pAnimSequence->GetNodeCount(); --i >= 0;)
        {
            IAnimNode* animNode = m_pAnimSequence->GetNode(i);
            if (animNode->GetType() == eAnimNodeType_AzEntity)
            {
                ConnectToBusesForRecording(animNode->GetAzEntityId(), false);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::Load()
{
    m_childNodes.clear();

    const int nodeCount = m_pAnimSequence->GetNodeCount();
    for (int i = 0; i < nodeCount; ++i)
    {
        IAnimNode* pNode = m_pAnimSequence->GetNode(i);

        // Only add top level nodes to sequence
        if (!pNode->GetParent())
        {
            CTrackViewAnimNodeFactory animNodeFactory;
            CTrackViewAnimNode* pNewTVAnimNode = animNodeFactory.BuildAnimNode(m_pAnimSequence, pNode, this);
            m_childNodes.push_back(std::unique_ptr<CTrackViewNode>(pNewTVAnimNode));
        }
    }

    SortNodes();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::BindToEditorObjects()
{
    m_bBoundToEditorObjects = true;
    CTrackViewAnimNode::BindToEditorObjects();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::UnBindFromEditorObjects()
{
    m_bBoundToEditorObjects = false;
    CTrackViewAnimNode::UnBindFromEditorObjects();
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewSequence::IsBoundToEditorObjects() const
{
    return m_bBoundToEditorObjects;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewKeyHandle CTrackViewSequence::FindSingleSelectedKey()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
    if (!pSequence)
    {
        return CTrackViewKeyHandle();
    }

    CTrackViewKeyBundle selectedKeys = pSequence->GetSelectedKeys();

    if (selectedKeys.GetKeyCount() != 1)
    {
        return CTrackViewKeyHandle();
    }

    return selectedKeys.GetKey(0);
}

//////////////////////////////////////////////////////////////////////////
CObjectLayer* CTrackViewSequence::GetSequenceObjectLayer() const
{
    CObjectLayer* retLayer = nullptr;

    ESequenceType sequenceType = m_pAnimSequence->GetSequenceType();
    switch (sequenceType)
    {
        case eSequenceType_Legacy:
        {
            retLayer = GetSequenceObject()->GetLayer();
            break;
        }
        case eSequenceType_SequenceComponent:
        {
            CEntityObject* entityObject = nullptr;
            AzToolsFramework::ComponentEntityEditorRequestBus::EventResult(entityObject, m_pAnimSequence->GetOwnerId(), &AzToolsFramework::ComponentEntityEditorRequestBus::Events::GetSandboxObject);
            if (entityObject)
            {
                retLayer = entityObject->GetLayer();
            }
            break;
        }
    }

    return retLayer;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::PrepareForSave()
{
    // Notify the connected SequenceComponent that we're about to save. This allows the SequenceComponent to stash the AnimSequence serialization
    // in a string in it's component for saving outside of the legacy Cry Level.
    if (m_pAnimSequence && m_pAnimSequence->GetSequenceType() == eSequenceType_SequenceComponent)
    {
        LmbrCentral::EditorSequenceComponentRequestBus::Event(m_pAnimSequence->GetOwnerId(), &LmbrCentral::EditorSequenceComponentRequestBus::Events::OnBeforeSave);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnEntityComponentPropertyChanged(AZ::ComponentId changedComponentId)
{
    const AZ::EntityId entityId = *AzToolsFramework::PropertyEditorEntityChangeNotificationBus::GetCurrentBusId();
   
    // find the component node for this changeComponentId if it exists
    for (int i = m_pAnimSequence->GetNodeCount(); --i >= 0;)
    {
        IAnimNode* animNode = m_pAnimSequence->GetNode(i);
        if (animNode && animNode->GetComponentId() == changedComponentId)
        {
            // we have a component animNode for this changedComponentId. Process the component change
            AZ::Uuid componentTypeId;
            AzFramework::ApplicationRequests::Bus::BroadcastResult(componentTypeId, &AzFramework::ApplicationRequests::Bus::Events::GetComponentTypeId, entityId, changedComponentId);

            // ignore Transform updates which we catch in OnTransformChanged notifications
            if (componentTypeId != AzToolsFramework::Components::TransformComponent::TYPEINFO_Uuid())
            {
                RecordTrackChangesForNode(static_cast<CTrackViewAnimNode*>(animNode->GetNodeOwner()));
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::ConnectToBusesForRecording(const AZ::EntityId& entityId, bool enableConnection)
{
    // we connect to PropertyEditorEntityChangeNotificationBus for all other changes
    if (enableConnection)
    {
        AzToolsFramework::PropertyEditorEntityChangeNotificationBus::MultiHandler::BusConnect(entityId);
    }
    else
    {
        AzToolsFramework::PropertyEditorEntityChangeNotificationBus::MultiHandler::BusDisconnect(entityId);
    }
}

//////////////////////////////////////////////////////////////////////////
int CTrackViewSequence::RecordTrackChangesForNode(CTrackViewAnimNode* componentNode)
{
    int retNumKeysSet = 0;

    if (componentNode)
    {
        retNumKeysSet = componentNode->SetKeysForChangedTrackValues(GetIEditor()->GetAnimation()->GetTime());
        if (retNumKeysSet)
        {
            OnKeysChanged();    // change notification for updating TrackView UI
        }
    }

    return retNumKeysSet;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SetRecording(bool enableRecording)
{
    if (m_pAnimSequence && m_pAnimSequence->GetSequenceType() == eSequenceType_SequenceComponent)
    {
        // connect (or disconnect) to EBuses for notification of changes for all AZ::Entities in our sequence
        for (int i = m_pAnimSequence->GetNodeCount(); --i >= 0;)
        {
            IAnimNode* animNode = m_pAnimSequence->GetNode(i);
            if (animNode->GetType() == eAnimNodeType_AzEntity)
            {
                ConnectToBusesForRecording(animNode->GetAzEntityId(), enableRecording);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewSequence::IsAncestorOf(CTrackViewSequence* pSequence) const
{
    return m_pAnimSequence->IsAncestorOf(pSequence->m_pAnimSequence);
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewSequence::IsLayerLocked() const
{
    CObjectLayer* pLayer = GetSequenceObjectLayer();
    return pLayer ? pLayer->IsFrozen() : false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::BeginCutScene(const bool bResetFx) const
{
    IMovieUser* pMovieUser = GetIEditor()->GetMovieSystem()->GetUser();

    if (pMovieUser)
    {
        pMovieUser->BeginCutScene(m_pAnimSequence, m_pAnimSequence->GetCutSceneFlags(false), bResetFx);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::EndCutScene() const
{
    IMovieUser* pMovieUser = GetIEditor()->GetMovieSystem()->GetUser();

    if (pMovieUser)
    {
        pMovieUser->EndCutScene(m_pAnimSequence, m_pAnimSequence->GetCutSceneFlags(true));
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::Render(const SAnimContext& animContext)
{
    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
            pChildAnimNode->Render(animContext);
        }
    }

    m_pAnimSequence->Render();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::Animate(const SAnimContext& animContext)
{
    if (!m_pAnimSequence->IsActivated())
    {
        return;
    }

    m_time = animContext.time;

    m_pAnimSequence->Animate(animContext);

    CTrackViewSequenceNoNotificationContext context(this);
    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
            pChildAnimNode->Animate(animContext);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::AddListener(ITrackViewSequenceListener* pListener)
{
    stl::push_back_unique(m_sequenceListeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::RemoveListener(ITrackViewSequenceListener* pListener)
{
    stl::find_and_erase(m_sequenceListeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnNodeSelectionChanged()
{
    if (m_bNoNotifications)
    {
        return;
    }

    if (m_bQueueNotifications)
    {
        m_bNodeSelectionChanged = true;
    }
    else
    {
        CTrackViewSequenceNoNotificationContext context(this);
        for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
        {
            (*iter)->OnNodeSelectionChanged(this);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::ForceAnimation()
{
    if (m_bNoNotifications)
    {
        return;
    }

    if (m_bQueueNotifications)
    {
        m_bForceAnimation = true;
    }
    else
    {
        if (IsActive())
        {
            GetIEditor()->GetAnimation()->ForceAnimation();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnKeySelectionChanged()
{
    if (m_bNoNotifications)
    {
        return;
    }

    if (m_bQueueNotifications)
    {
        m_bKeySelectionChanged = true;
    }
    else
    {
        CTrackViewSequenceNoNotificationContext context(this);
        for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
        {
            (*iter)->OnKeySelectionChanged(this);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnKeysChanged()
{
    if (m_bNoNotifications)
    {
        return;
    }

    if (m_bQueueNotifications)
    {
        m_bKeysChanged = true;
    }
    else
    {
        CTrackViewSequenceNoNotificationContext context(this);
        for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
        {
            (*iter)->OnKeysChanged(this);
        }

        if (IsActive())
        {
            GetIEditor()->GetAnimation()->ForceAnimation();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnKeyAdded(CTrackViewKeyHandle& addedKeyHandle)
{
    if (m_bNoNotifications)
    {
        return;
    }

    CTrackViewSequenceNoNotificationContext context(this);
    for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
    {
        (*iter)->OnKeyAdded(addedKeyHandle);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnNodeChanged(CTrackViewNode* pNode, ITrackViewSequenceListener::ENodeChangeType type)
{
    if (pNode && pNode->GetNodeType() == eTVNT_AnimNode)
    {
        CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pNode);
        CEntityObject* pNodeEntity = pAnimNode->GetNodeEntity();

        if (pAnimNode->IsActive() && pNodeEntity)
        {
            switch (type)
            {
            case ITrackViewSequenceListener::eNodeChangeType_Added:
                pNodeEntity->SetTransformDelegate(pAnimNode);
                pNodeEntity->RegisterListener(pAnimNode);
                ForceAnimation();

                // if we're in record mode and this is an AzEntity node, add the node to the buses we listen to for notification of changes
                if (pAnimNode->GetType() == eAnimNodeType_AzEntity && GetIEditor()->GetAnimation()->IsRecordMode())
                {
                    ConnectToBusesForRecording(pAnimNode->GetAzEntityId(), true);
                }
                break;
            case ITrackViewSequenceListener::eNodeChangeType_Removed:
                pNodeEntity->SetTransformDelegate(nullptr);
                pNodeEntity->UnregisterListener(pAnimNode);
                ForceAnimation();

                // if we're in record mode and this is an AzEntity node, remove the node to the buses we listen to for notification of changes
                if (pAnimNode->GetType() == eAnimNodeType_AzEntity && GetIEditor()->GetAnimation()->IsRecordMode())
                {
                    ConnectToBusesForRecording(pAnimNode->GetAzEntityId(), false);
                }
                break;
            }
        }

        switch (type)
        {
        case ITrackViewSequenceListener::eNodeChangeType_Enabled:
        // Fall through
        case ITrackViewSequenceListener::eNodeChangeType_Hidden:
        // Fall through
        case ITrackViewSequenceListener::eNodeChangeType_SetAsActiveDirector:
        // Fall through
        case ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged:
            ForceAnimation();
            break;
        }
    }

    // Mark Layer with Sequence Object as dirty for non-internal or non-UI changes
    if (type != ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged && 
        type != ITrackViewSequenceListener::eNodeChangeType_Selected &&
        type != ITrackViewSequenceListener::eNodeChangeType_Deselected && 
        type != ITrackViewSequenceListener::eNodeChangeType_Collapsed &&
        type != ITrackViewSequenceListener::eNodeChangeType_Expanded)
    {
        MarkAsModified();
    }

    if (m_bNoNotifications)
    {
        return;
    }

    CTrackViewSequenceNoNotificationContext context(this);
    for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
    {
        (*iter)->OnNodeChanged(pNode, type);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnNodeRenamed(CTrackViewNode* pNode, const char* pOldName)
{
    bool bLightAnimationSetActive = GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;
    if (bLightAnimationSetActive)
    {
        UpdateLightAnimationRefs(pOldName, pNode->GetName());
    }

    // Marks Layer with Sequence Object as dirty
    MarkAsModified();

    if (m_bNoNotifications)
    {
        return;
    }

    CTrackViewSequenceNoNotificationContext context(this);
    for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
    {
        (*iter)->OnNodeRenamed(pNode, pOldName);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnSequenceSettingsChanged()
{
    MarkAsModified();

    if (m_bNoNotifications)
    {
        return;
    }

    CTrackViewSequenceNoNotificationContext context(this);
    for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
    {
        (*iter)->OnSequenceSettingsChanged(this);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::MarkAsModified()
{
    if (m_pAnimSequence)
    {
        switch (GetSequenceType())
        {
            case eSequenceType_SequenceComponent:
            {
                LmbrCentral::EditorSequenceComponentRequestBus::Event(m_pAnimSequence->GetOwnerId(), &LmbrCentral::EditorSequenceComponentRequestBus::Events::MarkEntityLayerAsDirty);            
                break;
            }
            case eSequenceType_Legacy:
            {
                if (m_pAnimSequence->GetOwner())
                {
                    m_pAnimSequence->GetOwner()->OnModified();
                }
                break;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::QueueNotifications()
{
    m_bQueueNotifications = true;
    ++m_selectionRecursionLevel;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SubmitPendingNotifcations()
{
    assert(m_selectionRecursionLevel > 0);
    if (m_selectionRecursionLevel > 0)
    {
        --m_selectionRecursionLevel;
    }

    if (m_selectionRecursionLevel == 0)
    {
        m_bQueueNotifications = false;

        if (m_bNodeSelectionChanged)
        {
            OnNodeSelectionChanged();
        }

        if (m_bKeysChanged)
        {
            OnKeysChanged();
        }

        if (m_bKeySelectionChanged)
        {
            OnKeySelectionChanged();
        }

        if (m_bForceAnimation)
        {
            ForceAnimation();
        }

        m_bForceAnimation = false;
        m_bKeysChanged = false;
        m_bNodeSelectionChanged = false;
        m_bKeySelectionChanged = false;
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnSequenceRemoved(CTrackViewSequence* removedSequence)
{
    if (removedSequence == this)
    {
        // submit any queued notifications before removing
        if (m_bQueueNotifications)
        {
            m_selectionRecursionLevel = 1;  // this forces the next SubmitPendingNotifcations() to submit the notifications
            SubmitPendingNotifcations();
        }

        // remove ourselves as listeners from the undo manager
        GetIEditor()->GetUndoManager()->RemoveListener(this);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OnSequenceAdded(CTrackViewSequence* addedSequence)
{
    if (addedSequence == this)
    {
        GetIEditor()->GetUndoManager()->AddListener(this);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::DeleteSelectedNodes()
{
    assert(CUndo::IsRecording());

    CTrackViewSequenceNotificationContext context(this);

    if (IsSelected())
    {
        GetIEditor()->GetSequenceManager()->DeleteSequence(this);
        return;
    }

    CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
    const unsigned int numSelectedNodes = selectedNodes.GetCount();

    // Check if any reference to the light animation to be deleted exists, and abort the removal, if any.
    const bool bLightAnimationSetActive = GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;
    if (bLightAnimationSetActive)
    {
        QStringList lightNodes;

        // Construct set of selected light nodes
        for (unsigned int i = 0; i < numSelectedNodes; ++i)
        {
            CTrackViewAnimNode* pCurrentNode = selectedNodes.GetNode(i);
            if (pCurrentNode->GetType() == eAnimNodeType_Light)
            {
                stl::push_back_unique(lightNodes, pCurrentNode->GetName());
            }
        }

        // Check all entities if any is referencing any selected light node
        std::vector<CBaseObject*> entityObjects;
        GetIEditor()->GetObjectManager()->FindObjectsOfType(&CEntityObject::staticMetaObject, entityObjects);

        for (size_t i = 0; i < entityObjects.size(); ++i)
        {
            QString lightAnimationName = static_cast<CEntityObject*>(entityObjects[i])->GetLightAnimation();
            if (stl::find(lightNodes, lightAnimationName))
            {
                QMessageBox::critical(QApplication::activeWindow(), QString(), QObject::tr("The node '%1' cannot be removed since there is a light entity still using it.").arg(lightAnimationName));
                return;
            }
        }
    }

    CTrackViewTrackBundle selectedTracks = GetSelectedTracks();
    const unsigned int numSelectedTracks = selectedTracks.GetCount();

    for (unsigned int i = 0; i < numSelectedTracks; ++i)
    {
        CTrackViewTrack* pTrack = selectedTracks.GetTrack(i);

        // Ignore sub tracks
        if (!pTrack->IsSubTrack())
        {
            pTrack->GetAnimNode()->RemoveTrack(pTrack);
        }
    }

    for (unsigned int i = 0; i < numSelectedNodes; ++i)
    {
        CTrackViewAnimNode* pNode = selectedNodes.GetNode(i);
        CTrackViewAnimNode* pParentNode = static_cast<CTrackViewAnimNode*>(pNode->GetParentNode());
        pParentNode->RemoveSubNode(pNode);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SelectSelectedNodesInViewport()
{
    assert(CUndo::IsRecording());

    CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
    const unsigned int numSelectedNodes = selectedNodes.GetCount();

    std::vector<CBaseObject*> entitiesToBeSelected;

    // Also select objects that refer to light animation
    const bool bLightAnimationSetActive = GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;
    if (bLightAnimationSetActive)
    {
        QStringList lightNodes;

        // Construct set of selected light nodes
        for (unsigned int i = 0; i < numSelectedNodes; ++i)
        {
            CTrackViewAnimNode* pCurrentNode = selectedNodes.GetNode(i);
            if (pCurrentNode->GetType() == eAnimNodeType_Light)
            {
                stl::push_back_unique(lightNodes, pCurrentNode->GetName());
            }
        }

        // Check all entities if any is referencing any selected light node
        std::vector<CBaseObject*> entityObjects;
        GetIEditor()->GetObjectManager()->FindObjectsOfType(&CEntityObject::staticMetaObject, entityObjects);

        for (size_t i = 0; i < entityObjects.size(); ++i)
        {
            QString lightAnimationName = static_cast<CEntityObject*>(entityObjects[i])->GetLightAnimation();
            if (stl::find(lightNodes, lightAnimationName))
            {
                stl::push_back_unique(entitiesToBeSelected, entityObjects[i]);
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < numSelectedNodes; ++i)
        {
            CTrackViewAnimNode* pNode = selectedNodes.GetNode(i);
            CEntityObject* pEntity = pNode->GetNodeEntity();
            if (pEntity)
            {
                stl::push_back_unique(entitiesToBeSelected, pEntity);
            }
        }
    }

    for (auto iter = entitiesToBeSelected.begin(); iter != entitiesToBeSelected.end(); ++iter)
    {
        GetIEditor()->SelectObject(*iter);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SyncSelectedTracksToBase()
{
    CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
    bool bNothingWasSynced = true;

    const unsigned int numSelectedNodes = selectedNodes.GetCount();
    if (numSelectedNodes > 0)
    {
        CUndo undo("Sync selected tracks to base");

        for (unsigned int i = 0; i < numSelectedNodes; ++i)
        {
            CTrackViewAnimNode* pAnimNode = selectedNodes.GetNode(i);
            CEntityObject* pEntityObject = pAnimNode->GetNodeEntity();

            if (pEntityObject)
            {
                CTrackViewAnimNode* pAnimNode = GetIEditor()->GetSequenceManager()->GetActiveAnimNode(pEntityObject);

                if (pAnimNode)
                {
                    ITransformDelegate* pDelegate = pEntityObject->GetTransformDelegate();
                    pEntityObject->SetTransformDelegate(nullptr);


                    const Vec3 position = pAnimNode->GetPos();
                    pEntityObject->SetPos(position);

                    const Quat rotation = pAnimNode->GetRotation();
                    pEntityObject->SetRotation(rotation);

                    const Vec3 scale = pAnimNode->GetScale();
                    pEntityObject->SetScale(scale);

                    pEntityObject->SetTransformDelegate(pDelegate);

                    bNothingWasSynced = false;
                }
            }
        }

        if (bNothingWasSynced)
        {
            undo.Cancel();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SyncSelectedTracksFromBase()
{
    CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
    bool bNothingWasSynced = true;

    const unsigned int numSelectedNodes = selectedNodes.GetCount();
    if (numSelectedNodes > 0)
    {
        CUndo undo("Sync selected tracks to base");

        for (unsigned int i = 0; i < numSelectedNodes; ++i)
        {
            CTrackViewAnimNode* pAnimNode = selectedNodes.GetNode(i);
            CEntityObject* pEntityObject = pAnimNode->GetNodeEntity();

            if (pEntityObject)
            {
                CTrackViewAnimNode* pAnimNode = GetIEditor()->GetSequenceManager()->GetActiveAnimNode(pEntityObject);

                if (pAnimNode)
                {
                    ITransformDelegate* pDelegate = pEntityObject->GetTransformDelegate();
                    pEntityObject->SetTransformDelegate(nullptr);

                    const Vec3 position = pEntityObject->GetPos();
                    pAnimNode->SetPos(position);

                    const Quat rotation = pEntityObject->GetRotation();
                    pAnimNode->SetRotation(rotation);

                    const Vec3 scale = pEntityObject->GetScale();
                    pEntityObject->SetScale(scale);

                    pEntityObject->SetTransformDelegate(pDelegate);

                    bNothingWasSynced = false;
                }
            }
        }

        if (bNothingWasSynced)
        {
            undo.Cancel();
        }
    }

    if (IsActive())
    {
        GetIEditor()->GetAnimation()->ForceAnimation();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::UpdateLightAnimationRefs(const char* pOldName, const char* pNewName)
{
    std::vector<CBaseObject*> entityObjects;
    GetIEditor()->GetObjectManager()->FindObjectsOfType(&CEntityObject::staticMetaObject, entityObjects);
    std::for_each(std::begin(entityObjects), std::end(entityObjects),
        [&pOldName, &pNewName](CBaseObject* pBaseObject)
	{
		CEntityObject *pEntityObject = static_cast<CEntityObject*>(pBaseObject);
		bool bLight = pEntityObject && pEntityObject->GetEntityClass().compare("Light") == 0;
		if(bLight)
		{
            QString lightAnimation = pEntityObject->GetEntityPropertyString("lightanimation_LightAnimation");
            if(lightAnimation == pOldName)
			{
				pEntityObject->SetEntityPropertyString("lightanimation_LightAnimation", pNewName);
			}
		}
	});
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewSequence::SetName(const char* pName)
{
    // Check if there is already a sequence with that name
    const CTrackViewSequenceManager* pSequenceManager = GetIEditor()->GetSequenceManager();
    if (pSequenceManager->GetSequenceByName(pName))
    {
        return false;
    }

    string oldName = GetName();
    m_pAnimSequence->SetName(pName);
    MarkAsModified();

    if (CUndo::IsRecording())
    {
        CUndo::Record(new CUndoAnimNodeRename(this, oldName));
    }

    GetSequence()->OnNodeRenamed(this, oldName);

    return true;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::DeleteSelectedKeys()
{
    assert(CUndo::IsRecording());

    StoreUndoForTracksWithSelectedKeys();

    CTrackViewSequenceNotificationContext context(this);
    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();
    for (int k = (int)selectedKeys.GetKeyCount() - 1; k >= 0; --k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        skey.Delete();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::StoreUndoForTracksWithSelectedKeys()
{
    assert(CUndo::IsRecording());

    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();

    // Construct the set of tracks that have selected keys
    std::set<CTrackViewTrack*> tracks;
    for (int k = 0; k < (int)selectedKeys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        tracks.insert(skey.GetTrack());
    }

    // Store one key selection undo before...
    CUndo::Record(new CUndoAnimKeySelection(this));

    // For each of those tracks store an undo object
    for (auto iter = tracks.begin(); iter != tracks.end(); ++iter)
    {
        CUndo::Record(new CUndoTrackObject(*iter, false));
    }

    // ... and one after key changes
    CUndo::Record(new CUndoAnimKeySelection(this));
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::CopyKeysToClipboard(const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
    XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("CopyKeysNode");
    CopyKeysToClipboard(copyNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);

    CClipboard clip(nullptr);
    clip.Put(copyNode, "Track view keys");
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        pChildNode->CopyKeysToClipboard(xmlNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::PasteKeysFromClipboard(CTrackViewAnimNode* pTargetNode, CTrackViewTrack* pTargetTrack, const float timeOffset)
{
    assert(CUndo::IsRecording());

    CClipboard clipboard(nullptr);
    XmlNodeRef clipboardContent = clipboard.Get();
    if (clipboardContent)
    {
        std::vector<TMatchedTrackLocation> matchedLocations = GetMatchedPasteLocations(clipboardContent, pTargetNode, pTargetTrack);

        for (auto iter = matchedLocations.begin(); iter != matchedLocations.end(); ++iter)
        {
            const TMatchedTrackLocation& location = *iter;
            CTrackViewTrack* pTrack = location.first;
            const XmlNodeRef& trackNode = location.second;
            pTrack->PasteKeys(trackNode, timeOffset);
        }

        OnKeysChanged();
    }
}

//////////////////////////////////////////////////////////////////////////
std::vector<CTrackViewSequence::TMatchedTrackLocation>
CTrackViewSequence::GetMatchedPasteLocations(XmlNodeRef clipboardContent, CTrackViewAnimNode* pTargetNode, CTrackViewTrack* pTargetTrack)
{
    std::vector<TMatchedTrackLocation> matchedLocations;

    bool bPastingSingleNode = false;
    XmlNodeRef singleNode;
    bool bPastingSingleTrack = false;
    XmlNodeRef singleTrack;

    // Check if the XML tree only contains one node and if so if that node only contains one track
    for (XmlNodeRef currentNode = clipboardContent; currentNode->getChildCount() > 0; currentNode = currentNode->getChild(0))
    {
        bool bAllChildsAreTracks = true;
        const unsigned int numChilds = currentNode->getChildCount();
        for (unsigned int i = 0; i < numChilds; ++i)
        {
            XmlNodeRef childNode = currentNode->getChild(i);
            if (strcmp(currentNode->getChild(0)->getTag(), "Track") != 0)
            {
                bAllChildsAreTracks = false;
                break;
            }
        }

        if (bAllChildsAreTracks)
        {
            bPastingSingleNode = true;
            singleNode = currentNode;

            if (currentNode->getChildCount() == 1)
            {
                bPastingSingleTrack = true;
                singleTrack = currentNode->getChild(0);
            }
        }
        else if (currentNode->getChildCount() != 1)
        {
            break;
        }
    }

    if (bPastingSingleTrack && pTargetNode && pTargetTrack)
    {
        // We have a target node & track, so try to match the value type
        int valueType = 0;
        if (singleTrack->getAttr("valueType", valueType))
        {
            if (pTargetTrack->GetValueType() == valueType)
            {
                matchedLocations.push_back(TMatchedTrackLocation(pTargetTrack, singleTrack));
                return matchedLocations;
            }
        }
    }

    if (bPastingSingleNode && pTargetNode)
    {
        // Set of tracks that were already matched
        std::vector<CTrackViewTrack*> matchedTracks;

        // We have a single node to paste and have been given a target node
        // so try to match the tracks by param type
        const unsigned int numTracks = singleNode->getChildCount();
        for (unsigned int i = 0; i < numTracks; ++i)
        {
            XmlNodeRef trackNode = singleNode->getChild(i);

            // Try to match the track
            auto matchingTracks = GetMatchingTracks(pTargetNode, trackNode);
            for (auto iter = matchingTracks.begin(); iter != matchingTracks.end(); ++iter)
            {
                CTrackViewTrack* pMatchedTrack = *iter;
                // Pick the first track that was matched *and* was not already matched
                if (!stl::find(matchedTracks, pMatchedTrack))
                {
                    stl::push_back_unique(matchedTracks, pMatchedTrack);
                    matchedLocations.push_back(TMatchedTrackLocation(pMatchedTrack, trackNode));
                    break;
                }
            }
        }

        // Return if matching succeeded
        if (matchedLocations.size() > 0)
        {
            return matchedLocations;
        }
    }

    if (!bPastingSingleNode)
    {
        // Ok, we're pasting keys from multiple nodes, haven't been given any target
        // or matching the targets failed. Ignore given target pointers and start
        // a recursive match at the sequence root.
        GetMatchedPasteLocationsRec(matchedLocations, this, clipboardContent);
    }

    return matchedLocations;
}

//////////////////////////////////////////////////////////////////////////
std::deque<CTrackViewTrack*> CTrackViewSequence::GetMatchingTracks(CTrackViewAnimNode* pAnimNode, XmlNodeRef trackNode)
{
    std::deque<CTrackViewTrack*> matchingTracks;

    const string trackName = trackNode->getAttr("name");

    CAnimParamType animParamType;
    animParamType.Serialize(trackNode, true);

    int valueType;
    if (!trackNode->getAttr("valueType", valueType))
    {
        return matchingTracks;
    }

    CTrackViewTrackBundle tracks = pAnimNode->GetTracksByParam(animParamType);
    const unsigned int trackCount = tracks.GetCount();

    if (trackCount > 0)
    {
        // Search for a track with the given name and value type
        for (unsigned int i = 0; i < trackCount; ++i)
        {
            CTrackViewTrack* pTrack = tracks.GetTrack(i);

            if (pTrack->GetValueType() == valueType)
            {
                if (pTrack->GetName() == trackName)
                {
                    matchingTracks.push_back(pTrack);
                }
            }
        }

        // Then with lower precedence add the tracks that only match the value
        for (unsigned int i = 0; i < trackCount; ++i)
        {
            CTrackViewTrack* pTrack = tracks.GetTrack(i);

            if (pTrack->GetValueType() == valueType)
            {
                stl::push_back_unique(matchingTracks, pTrack);
            }
        }
    }

    return matchingTracks;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::GetMatchedPasteLocationsRec(std::vector<TMatchedTrackLocation>& locations, CTrackViewNode* pCurrentNode, XmlNodeRef clipboardNode)
{
    if (pCurrentNode->GetNodeType() == eTVNT_Sequence)
    {
        if (strcmp(clipboardNode->getTag(), "CopyKeysNode") != 0)
        {
            return;
        }
    }

    const unsigned int numChildNodes = clipboardNode->getChildCount();
    for (unsigned int nodeIndex = 0; nodeIndex < numChildNodes; ++nodeIndex)
    {
        XmlNodeRef xmlChildNode = clipboardNode->getChild(nodeIndex);
        const string tagName = xmlChildNode->getTag();

        if (tagName == "Node")
        {
            const string nodeName = xmlChildNode->getAttr("name");

            int nodeType = eAnimNodeType_Invalid;
            xmlChildNode->getAttr("type", nodeType);

            const unsigned int childCount = pCurrentNode->GetChildCount();
            for (unsigned int i = 0; i < childCount; ++i)
            {
                CTrackViewNode* pChildNode = pCurrentNode->GetChild(i);

                if (pChildNode->GetNodeType() == eTVNT_AnimNode)
                {
                    CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
                    if (pAnimNode->GetName() == nodeName && pAnimNode->GetType() == nodeType)
                    {
                        GetMatchedPasteLocationsRec(locations, pChildNode, xmlChildNode);
                    }
                }
            }
        }
        else if (tagName == "Track")
        {
            const string trackName = xmlChildNode->getAttr("name");

            CAnimParamType trackParamType;
            trackParamType.Serialize(xmlChildNode, true);

            int trackParamValue = eAnimValue_Unknown;
            xmlChildNode->getAttr("valueType", trackParamValue);

            const unsigned int childCount = pCurrentNode->GetChildCount();
            for (unsigned int i = 0; i < childCount; ++i)
            {
                CTrackViewNode* pNode = pCurrentNode->GetChild(i);

                if (pNode->GetNodeType() == eTVNT_Track)
                {
                    CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);
                    if (pTrack->GetName() == trackName && pTrack->GetParameterType() == trackParamType)
                    {
                        locations.push_back(TMatchedTrackLocation(pTrack, xmlChildNode));
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::AdjustKeysToTimeRange(Range newTimeRange)
{
    assert (CUndo::IsRecording());

    // Store one key selection undo before...
    CUndo::Record(new CUndoAnimKeySelection(this));

    // Store key undo for each track
    CTrackViewTrackBundle tracks = GetAllTracks();
    const unsigned int numTracks = tracks.GetCount();
    for (unsigned int i = 0; i < numTracks; ++i)
    {
        CTrackViewTrack* pTrack = tracks.GetTrack(i);
        CUndo::Record(new CUndoTrackObject(pTrack, false));
    }

    // ... and one after key changes
    CUndo::Record(new CUndoAnimKeySelection(this));

    // Set new time range
    Range oldTimeRange = GetTimeRange();
    float offset = newTimeRange.start - oldTimeRange.start;
    // Calculate scale ratio.
    float scale = newTimeRange.Length() / oldTimeRange.Length();
    SetTimeRange(newTimeRange);

    CTrackViewKeyBundle keyBundle = GetAllKeys();
    const unsigned int numKeys = keyBundle.GetKeyCount();

    for (unsigned int i = 0; i < numKeys; ++i)
    {
        CTrackViewKeyHandle keyHandle = keyBundle.GetKey(i);
        keyHandle.SetTime(offset + keyHandle.GetTime() * scale);
    }

    MarkAsModified();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SetTimeRange(Range timeRange)
{
    if (CUndo::IsRecording())
    {
        // Store old sequence settings
        CUndo::Record(new CUndoSequenceSettings(this));
    }

    m_pAnimSequence->SetTimeRange(timeRange);
    OnSequenceSettingsChanged();
}

//////////////////////////////////////////////////////////////////////////
Range CTrackViewSequence::GetTimeRange() const
{
    return m_pAnimSequence->GetTimeRange();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SetFlags(IAnimSequence::EAnimSequenceFlags flags)
{
    if (CUndo::IsRecording())
    {
        // Store old sequence settings
        CUndo::Record(new CUndoSequenceSettings(this));
    }

    m_pAnimSequence->SetFlags(flags);
    OnSequenceSettingsChanged();
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence::EAnimSequenceFlags CTrackViewSequence::GetFlags() const
{
    return (IAnimSequence::EAnimSequenceFlags)m_pAnimSequence->GetFlags();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::DeselectAllKeys()
{
    assert(CUndo::IsRecording());
    CTrackViewSequenceNotificationContext context(this);

    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();
    for (int i = 0; i < selectedKeys.GetKeyCount(); ++i)
    {
        CTrackViewKeyHandle keyHandle = selectedKeys.GetKey(i);
        keyHandle.Select(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::OffsetSelectedKeys(const float timeOffset)
{
    assert(CUndo::IsRecording());
    CTrackViewSequenceNotificationContext context(this);

    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();

    const CTrackViewTrack* pTrack = nullptr;
    for (int k = 0; k < (int)selectedKeys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        skey.Offset(timeOffset);
    }
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewSequence::ClipTimeOffsetForOffsetting(const float timeOffset)
{
    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();

    float newTimeOffset = timeOffset;
    for (int k = 0; k < (int)selectedKeys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        const float keyTime = skey.GetTime();
        float newKeyTime = keyTime + timeOffset;

        Range extendedTimeRange(0.0f, GetTimeRange().end);
        extendedTimeRange.ClipValue(newKeyTime);

        float offset = newKeyTime - keyTime;
        if (fabs(offset) < fabs(newTimeOffset))
        {
            newTimeOffset = offset;
        }
    }

    return newTimeOffset;
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewSequence::ClipTimeOffsetForScaling(float timeOffset)
{
    if (timeOffset <= 0)
    {
        return timeOffset;
    }

    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();

    float newTimeOffset = timeOffset;
    for (int k = 0; k < (int)selectedKeys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        float keyTime = skey.GetTime();
        float newKeyTime = keyTime * timeOffset;
        GetTimeRange().ClipValue(newKeyTime);
        float offset = newKeyTime / keyTime;
        if (offset < newTimeOffset)
        {
            newTimeOffset = offset;
        }
    }

    return newTimeOffset;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::ScaleSelectedKeys(const float timeOffset)
{
    assert(CUndo::IsRecording());
    CTrackViewSequenceNotificationContext context(this);

    if (timeOffset <= 0)
    {
        return;
    }

    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();

    const CTrackViewTrack* pTrack = nullptr;
    for (int k = 0; k < (int)selectedKeys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        if (pTrack != skey.GetTrack())
        {
            pTrack = skey.GetTrack();
        }

        float keyt = skey.GetTime() * timeOffset;
        skey.SetTime(keyt);
    }
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewSequence::ClipTimeOffsetForSliding(const float timeOffset)
{
    CTrackViewKeyBundle keys = GetSelectedKeys();

    std::set<CTrackViewTrack*> tracks;
    std::set<CTrackViewTrack*>::const_iterator pTrackIter;

    Range timeRange = GetTimeRange();

    // Get the first key in the timeline among selected and
    // also gather tracks.
    float time0 = timeRange.end;
    for (int k = 0; k < (int)keys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = keys.GetKey(k);
        tracks.insert(skey.GetTrack());
        float keyTime = skey.GetTime();
        if (keyTime < time0)
        {
            time0 = keyTime;
        }
    }

    // If 'bAll' is true, slide all tracks.
    // (Otherwise, slide only selected tracks.)
    bool bAll = Qt::AltModifier & QApplication::queryKeyboardModifiers();
    if (bAll)
    {
        keys = GetKeysInTimeRange(time0, timeRange.end);
        // Gather tracks again.
        tracks.clear();
        for (int k = 0; k < (int)keys.GetKeyCount(); ++k)
        {
            CTrackViewKeyHandle skey = keys.GetKey(k);
            tracks.insert(skey.GetTrack());
        }
    }

    float newTimeOffset = timeOffset;
    for (pTrackIter = tracks.begin(); pTrackIter != tracks.end(); ++pTrackIter)
    {
        CTrackViewTrack* pTrack = *pTrackIter;
        for (int i = 0; i < pTrack->GetKeyCount(); ++i)
        {
            const CTrackViewKeyHandle& keyHandle = pTrack->GetKey(i);

            const float keyTime = keyHandle.GetTime();
            if (keyTime >= time0)
            {
                float newKeyTime = keyTime + timeOffset;
                timeRange.ClipValue(newKeyTime);
                float offset = newKeyTime - keyTime;
                if (fabs(offset) < fabs(newTimeOffset))
                {
                    newTimeOffset = offset;
                }
            }
        }
    }

    return newTimeOffset;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::SlideKeys(float timeOffset)
{
    assert(CUndo::IsRecording());
    CTrackViewSequenceNotificationContext context(this);

    CTrackViewKeyBundle keys = GetSelectedKeys();

    std::set<CTrackViewTrack*> tracks;
    std::set<CTrackViewTrack*>::const_iterator pTrackIter;
    Range timeRange = GetTimeRange();

    // Get the first key in the timeline among selected and
    // also gather tracks.
    float time0 = timeRange.end;
    for (int k = 0; k < (int)keys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = keys.GetKey(k);
        tracks.insert(skey.GetTrack());
        float keyTime = skey.GetTime();
        if (keyTime < time0)
        {
            time0 = keyTime;
        }
    }

    // If 'bAll' is true, slide all tracks.
    // (Otherwise, slide only selected tracks.)
    bool bAll = Qt::AltModifier & QApplication::queryKeyboardModifiers();
    if (bAll)
    {
        keys = GetKeysInTimeRange(time0, timeRange.end);
        // Gather tracks again.
        tracks.clear();
        for (int k = 0; k < (int)keys.GetKeyCount(); ++k)
        {
            CTrackViewKeyHandle skey = keys.GetKey(k);
            tracks.insert(skey.GetTrack());
        }
    }

    for (pTrackIter = tracks.begin(); pTrackIter != tracks.end(); ++pTrackIter)
    {
        CTrackViewTrack* pTrack = *pTrackIter;
        pTrack->SlideKeys(time0, timeOffset);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::CloneSelectedKeys()
{
    assert(CUndo::IsRecording());
    CTrackViewSequenceNotificationContext context(this);

    CTrackViewKeyBundle selectedKeys = GetSelectedKeys();

    const CTrackViewTrack* pTrack = nullptr;
    // In case of multiple cloning, indices cannot be used as a solid pointer to the original.
    // So use the time of keys as an identifier, instead.
    std::vector<float> selectedKeyTimes;
    for (size_t k = 0; k < selectedKeys.GetKeyCount(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        if (pTrack != skey.GetTrack())
        {
            pTrack = skey.GetTrack();
        }

        selectedKeyTimes.push_back(skey.GetTime());
    }

    // Now, do the actual cloning.
    for (size_t k = 0; k < selectedKeyTimes.size(); ++k)
    {
        CTrackViewKeyHandle skey = selectedKeys.GetKey(k);
        skey = skey.GetTrack()->GetKeyByTime(selectedKeyTimes[k]);

        assert(skey.IsValid());
        if (!skey.IsValid())
        {
            continue;
        }

        CTrackViewKeyHandle newKey = skey.Clone();

        // Select new key.
        newKey.Select(true);
        // Deselect cloned key.
        skey.Select(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::BeginUndoTransaction()
{
    QueueNotifications();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::EndUndoTransaction()
{
    // if the sequence was added during a redo, it will add itself as an UndoManagerListener in the process and we'll
    // get an EndUndoTransaction without a corresponding BeginUndoTransaction() call - only SubmitPendingNotifications()
    // if we're queued
    if (m_bQueueNotifications)
    {
        SubmitPendingNotifcations();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::BeginRestoreTransaction()
{
    QueueNotifications();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::EndRestoreTransaction()
{
    // if the sequence was added during a restore, it will add itself as an UndoManagerListener in the process and we'll
    // get an EndUndoTransaction without a corresponding BeginUndoTransaction() call - only SubmitPendingNotifications()
    // if we're queued
    if (m_bQueueNotifications)
    {
        SubmitPendingNotifcations();
    }
}


//////////////////////////////////////////////////////////////////////////
bool CTrackViewSequence::IsActiveSequence() const
{
    return GetIEditor()->GetAnimation()->GetSequence() == this;
}

//////////////////////////////////////////////////////////////////////////
const float CTrackViewSequence::GetTime() const
{
    return m_time;
}
