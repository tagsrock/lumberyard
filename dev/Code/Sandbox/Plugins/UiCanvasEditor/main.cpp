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
#include "stdafx.h"

#include "EditorCommon.h"

// UI_ANIMATION_REVISIT, added includes so that we can register the UI Animation system on startup
#include "EditorDefs.h"
#include "Resource.h"
#include "Animation/UiAnimViewDialog.h"
#include "Animation/UiAnimViewSequenceManager.h"

// there must be exactly one of these:
#include "platform_impl.h"

static bool IsCanvasEditorEnabled()
{
    bool isCanvasEditorEnabled = false;

    const ICVar* isCanvasEditorEnabledCVar = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("sys_enableCanvasEditor") : NULL;
    if (isCanvasEditorEnabledCVar && (isCanvasEditorEnabledCVar->GetIVal() == 1))
    {
        isCanvasEditorEnabled = true;
    }

    return isCanvasEditorEnabled;
}

class CUiCanvasEditorPlugin
    : public IPlugin
{
public:
    CUiCanvasEditorPlugin(IEditor* editor)
    {
        if (IsCanvasEditorEnabled())
        {
            // Calculate default editor size and position.
            // For landscape screens, use 75% of the screen. For portrait screens, use 95% of screen width and 4:3 aspect ratio
            QRect deskRect = QApplication::desktop()->availableGeometry();
            float availableWidth = (float)deskRect.width() * ((deskRect.width() > deskRect.height()) ? 0.75f : 0.95f);
            float availableHeight = (float)deskRect.height() * 0.75f;
            float editorWidth = availableWidth;
            float editorHeight = (deskRect.width() > deskRect.height()) ? availableHeight : (editorWidth * 3.0f / 4.0f);
            if ((availableWidth / availableHeight) > (editorWidth / editorHeight))
            {
                editorWidth = editorWidth * availableHeight / editorHeight;
                editorHeight = availableHeight;
            }
            else
            {
                editorWidth = availableWidth;
                editorHeight = editorHeight * availableWidth / editorWidth;
            }
            int x = (int)((float)deskRect.left() + (((float)deskRect.width() - availableWidth) / 2.0f) + ((availableWidth - editorWidth) / 2.0f));
            int y = (int)((float)deskRect.top() + (((float)deskRect.height() - availableHeight) / 2.0f) + ((availableHeight - editorHeight) / 2.0f));

            QtViewOptions opt;
            opt.isPreview = true;
            opt.paneRect = QRect(x, y, (int)editorWidth, (int)editorHeight);
            opt.isDeletable = true; // we're in a plugin; make sure we can be deleted
            // opt.canHaveMultipleInstances = true; // uncomment this when CUiAnimViewSequenceManager::CanvasUnloading supports multiple canvases
            opt.sendViewPaneNameBackToAmazonAnalyticsServers = true;
            RegisterQtViewPane<EditorWrapper>(editor, UICANVASEDITOR_NAME_LONG, LyViewPane::CategoryTools, opt);
            CUiAnimViewSequenceManager::Create();
        }
    }

    void Release() override
    {
        if (IsCanvasEditorEnabled())
        {
            UnregisterQtViewPane<EditorWrapper>();
            CUiAnimViewSequenceManager::Destroy();
        }

        delete this;
    }

    void ShowAbout() override {}
    const char* GetPluginGUID() override { return "{E064E1AE-EB6B-4EB0-A5B8-FA3967CA961C}"; }
    DWORD GetPluginVersion() override { return 1; }
    const char* GetPluginName() override { return UICANVASEDITOR_NAME_SHORT; }
    bool CanExitNow() override { return true; }
    void OnEditorNotify(EEditorNotifyEvent aEventId) override {}
};


PLUGIN_API IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)
{
    if (pInitParam->pluginVersion != SANDBOX_PLUGIN_SYSTEM_VERSION)
    {
        pInitParam->outErrorCode = IPlugin::eError_VersionMismatch;
        return nullptr;
    }

    ModuleInitISystem(GetIEditor()->GetSystem(), "UiCanvasEditor");
    return new CUiCanvasEditorPlugin(GetIEditor());
}

HINSTANCE g_hInstance = 0;
BOOL __stdcall DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hinstDLL;
    }

    return TRUE;
}
