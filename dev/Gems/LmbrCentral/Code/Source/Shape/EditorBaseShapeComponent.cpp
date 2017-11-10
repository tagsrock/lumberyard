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
#include "EditorBaseShapeComponent.h"
#include <AzCore/RTTI/ReflectContext.h>

namespace LmbrCentral
{
    const AZ::Vector4 EditorBaseShapeComponent::s_shapeColor(1.00f, 1.00f, 0.78f, 0.4f);
    const AZ::Vector4 EditorBaseShapeComponent::s_shapeWireColor(1.00f, 1.00f, 0.78f, 0.5f);

    void EditorBaseShapeComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();

        m_currentEntityTransform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(m_currentEntityTransform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
    }

    void EditorBaseShapeComponent::Deactivate()
    {
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
    }

    void EditorBaseShapeComponent::DisplayEntity(bool& handled)
    {
        if (!IsSelected())
        {
            return;
        }

        handled = true;

        AzFramework::EntityDebugDisplayRequests* displayContext = AzFramework::EntityDebugDisplayRequestBus::FindFirstHandler();
        AZ_Assert(displayContext, "Invalid display context.");

        // only uniform scale is supported in physics so the debug visuals reflect this fact
        AZ::Transform transformWithUniformScale = m_currentEntityTransform;
        const AZ::Vector3 scale = transformWithUniformScale.ExtractScale();
        float newScale = AZ::GetMax(AZ::GetMax(scale.GetX(), scale.GetY()), scale.GetZ());
        transformWithUniformScale.MultiplyByScale(AZ::Vector3(newScale, newScale, newScale));
        {
            displayContext->PushMatrix(transformWithUniformScale);
            DrawShape(displayContext);
            displayContext->PopMatrix();
        }
    }

    void EditorBaseShapeComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_currentEntityTransform = world;
    }
} // namespace LmbrCentral
