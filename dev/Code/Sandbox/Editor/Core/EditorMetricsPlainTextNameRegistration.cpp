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
#include "EditorMetricsPlainTextNameRegistration.h"

namespace Editor
{

EditorMetricsPlainTextNameRegistrationBusListener::EditorMetricsPlainTextNameRegistrationBusListener()
{
    AzFramework::MetricsPlainTextNameRegistrationBus::Handler::BusConnect();
}

EditorMetricsPlainTextNameRegistrationBusListener::~EditorMetricsPlainTextNameRegistrationBusListener()
{
    AzFramework::MetricsPlainTextNameRegistrationBus::Handler::BusDisconnect();
}

void EditorMetricsPlainTextNameRegistrationBusListener::RegisterForNameSending(const AZStd::vector<AZ::Uuid>& typeIdsThatCanBeSentAsPlainText)
{
    m_registeredTypeIds.insert(typeIdsThatCanBeSentAsPlainText.begin(), typeIdsThatCanBeSentAsPlainText.end());
}

bool EditorMetricsPlainTextNameRegistrationBusListener::IsTypeRegisteredForNameSending(const AZ::Uuid& typeId)
{
    return (m_registeredTypeIds.find(typeId) != m_registeredTypeIds.end());
}

} // namespace editor
