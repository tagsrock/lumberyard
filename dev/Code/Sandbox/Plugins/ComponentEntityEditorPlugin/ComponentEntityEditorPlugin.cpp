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
#include "QtViewPane.h"
#include "IResourceSelectorHost.h"
#include "CryExtension/ICryFactoryRegistry.h"

#include "UI/QComponentEntityEditorMainWindow.h"
#include "UI/QComponentEntityEditorOutlinerWindow.h"
#include "UI/ComponentPalette/ComponentPaletteSettings.h"
#include "UI/ComponentPalette/ComponentPaletteWindow.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzFramework/API/ApplicationAPI.h>

#include "ComponentEntityEditorPlugin.h"
#include "SandboxIntegration.h"
#include "Objects/ComponentEntityObject.h"

void RegisterSandboxObjects()
{
    GetIEditor()->GetClassFactory()->RegisterClass(new CTemplateObjectClassDesc<CComponentEntityObject>("ComponentEntity", "", "", OBJTYPE_AZENTITY, 201, "*.entity"));

    AZ::SerializeContext* serializeContext = nullptr;
    EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(serializeContext, "Serialization context not available");

    if (serializeContext)
    {
        ComponentPaletteSettings::Reflect(serializeContext);
    }
}

void UnregisterSandboxObjects()
{
    GetIEditor()->GetClassFactory()->UnregisterClass("ComponentEntity");
}

ComponentEntityEditorPlugin::ComponentEntityEditorPlugin(IEditor* editor)
    : m_registered(false)
{
    m_appListener = new SandboxIntegrationManager();
    m_appListener->Setup();

    QtViewOptions inspectorOptions;
    inspectorOptions.canHaveMultipleInstances = true;
    inspectorOptions.preferedDockingArea = Qt::RightDockWidgetArea;
    inspectorOptions.sendViewPaneNameBackToAmazonAnalyticsServers = true;
    RegisterQtViewPane<QComponentEntityEditorInspectorWindow>(
        editor,
        LyViewPane::EntityInspector,
        LyViewPane::CategoryTools,
        inspectorOptions);

    QtViewOptions outlinerOptions;
    outlinerOptions.canHaveMultipleInstances = true;
    outlinerOptions.preferedDockingArea = Qt::LeftDockWidgetArea;
    outlinerOptions.sendViewPaneNameBackToAmazonAnalyticsServers = true;
    RegisterQtViewPane<QComponentEntityEditorOutlinerWindow>(
        editor,
        LyViewPane::EntityOutliner,
        LyViewPane::CategoryTools,
        outlinerOptions);

    RegisterModuleResourceSelectors(GetIEditor()->GetResourceSelectorHost());

    RegisterSandboxObjects();

    m_registered = true;
}

void ComponentEntityEditorPlugin::Release()
{
    if (m_registered)
    {
        UnregisterQtViewPane<QComponentEntityEditorInspectorWindow>();
        UnregisterQtViewPane<QComponentEntityEditorOutlinerWindow>();

        UnregisterSandboxObjects();
    }

    delete m_appListener;

    delete this;
}

