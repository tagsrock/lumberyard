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
#include "AnimAZEntityNode.h"
#include "AnimComponentNode.h"

#include <AzFramework/Components/CameraBus.h>   // for definition of EditorCameraComponentTypeId
#include <AzCore/Component/TransformBus.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <LmbrCentral/Cinematics/SequenceComponentBus.h>

#include "Components/IComponentEntityNode.h"

//////////////////////////////////////////////////////////////////////////
CAnimAzEntityNode::CAnimAzEntityNode(const int id)
    : CAnimNode(id)
{
    SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
}

EAnimNodeType CAnimAzEntityNode::GetType() const
{ 
    return eAnimNodeType_AzEntity; 
}

//////////////////////////////////////////////////////////////////////////
CAnimAzEntityNode::~CAnimAzEntityNode()
{
}

//////////////////////////////////////////////////////////////////////////
void CAnimAzEntityNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
    CAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
    if (bLoading)
    {
        AZ::u64 id64;
        if (xmlNode->getAttr("AnimatedEntityId", id64))
        {
            m_entityId = AZ::EntityId(id64);
        }
    }
    else
    {
        // saving
        if (m_entityId.IsValid())
        {
            AZ::u64 id64 = static_cast<AZ::u64>(m_entityId);
            xmlNode->setAttr("AnimatedEntityId", id64);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimAzEntityNode::SetSkipInterpolatedCameraNode(const bool skipNodeCameraAnimation)
{
    // Skip animations on transforms
    CAnimComponentNode* transformNode = GetTransformComponentNode();
    if (transformNode)
    {
        transformNode->SetSkipComponentAnimationUpdates(skipNodeCameraAnimation);
    }

    // Skip animations on cameras
    CAnimComponentNode* cameraComponentNode = GetComponentNodeForComponentWithTypeId(AZ::Uuid(EditorCameraComponentTypeId));
    if (!cameraComponentNode)
    {
        cameraComponentNode = GetComponentNodeForComponentWithTypeId(AZ::Uuid(CameraComponentTypeId));
    }
    if (cameraComponentNode)
    {
        cameraComponentNode->SetSkipComponentAnimationUpdates(skipNodeCameraAnimation);
    }
}

//////////////////////////////////////////////////////////////////////////
void CAnimAzEntityNode::AppendNonBehaviorAnimatableComponents(AZStd::vector<AZ::ComponentId>& animatableComponents) const
{
    AZ::Entity* entity = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, m_entityId);
    if (entity)
    {
        const AZ::Entity::ComponentArrayType& components = entity->GetComponents();
        for (auto iter = components.begin(); iter != components.end(); iter++)
        {
            if (CAnimComponentNode::IsComponentAnimatedOutsideBehaviorContext((*iter)->RTTI_GetType()))
            {
                // This component has non-behavioral animated properties - Append it
                animatableComponents.push_back((*iter)->GetId());
            }
        }
    }
}

CAnimComponentNode* CAnimAzEntityNode::GetComponentNodeForComponentWithTypeId(const AZ::Uuid& componentTypeId) const
{
    CAnimComponentNode* retTransformNode = nullptr;

    for (int i = m_pSequence->GetNodeCount(); --i >= 0;)
    {
        IAnimNode* node = m_pSequence->GetNode(i);
        if (node && node->GetParent() == this && node->GetType() == eAnimNodeType_Component)
        {
            if (static_cast<CAnimComponentNode*>(node)->GetComponentTypeId() == componentTypeId)
            {
                retTransformNode = static_cast<CAnimComponentNode*>(node);
                break;
            }
        }
    }
    return retTransformNode;
}

//////////////////////////////////////////////////////////////////////////
CAnimComponentNode* CAnimAzEntityNode::GetTransformComponentNode() const
{
    CAnimComponentNode* retTransformNode = GetComponentNodeForComponentWithTypeId(AZ::Uuid(ToolsTransformComponentTypeId));
    
    if (!retTransformNode)
    {
        // if not Editor transform, try run-time transform
        retTransformNode = GetComponentNodeForComponentWithTypeId(AzFramework::TransformComponent::TYPEINFO_Uuid());
    }
    return retTransformNode;

}

//////////////////////////////////////////////////////////////////////////
void CAnimAzEntityNode::SetPos(float time, const Vec3& pos)
{
    CAnimComponentNode* transformComponent = GetTransformComponentNode();
    if (transformComponent)
    {
        transformComponent->SetPos(time, pos);
    }
}

Vec3 CAnimAzEntityNode::GetPos()
{
    CAnimComponentNode* transformComponent = GetTransformComponentNode();

    if (transformComponent)
    {
        return transformComponent->GetPos();
    }
    return Vec3(.0f, .0f, .0f);
}

//////////////////////////////////////////////////////////////////////////
void CAnimAzEntityNode::SetRotate(float time, const Quat& rotation)
{
    CAnimComponentNode* transformComponent = GetTransformComponentNode();
    if (transformComponent)
    {
        transformComponent->SetRotate(time, rotation);
    }
}

Quat CAnimAzEntityNode::GetRotate()
{
    CAnimComponentNode* transformComponent = GetTransformComponentNode();

    if (transformComponent)
    {
        return transformComponent->GetRotate();
    }
    return Quat::CreateIdentity();
}

//////////////////////////////////////////////////////////////////////////
void CAnimAzEntityNode::SetScale(float time, const Vec3& scale)
{
    CAnimComponentNode* transformComponent = GetTransformComponentNode();
    if (transformComponent)
    {
        transformComponent->SetScale(time, scale);
    }
}

Vec3 CAnimAzEntityNode::GetScale()
{
    CAnimComponentNode* transformComponent = GetTransformComponentNode();

    if (transformComponent)
    {
        return transformComponent->GetScale();
    }
    return Vec3(.0f, .0f, .0f);
}