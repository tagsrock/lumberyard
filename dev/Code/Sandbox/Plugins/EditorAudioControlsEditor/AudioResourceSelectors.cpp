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
#include "IResourceSelectorHost.h"
#include "Events/EventManager.h"
#include "QAudioControlEditorIcons.h"
#include "AudioControlsEditorPlugin.h"
#include "ATLControlsResourceDialog.h"
#include "IGameFramework.h"
#include "IEditor.h"

using namespace AudioControls;

namespace AudioControls
{
    //-------------------------------------------------------------------------------------------//
    dll_string ShowSelectDialog(const SResourceSelectorContext& context, const char* pPreviousValue, const EACEControlType controlType)
    {
        CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetATLModel();
        AZ_Assert(pModel != nullptr, "AudioResourceSelectors - ATL Model is null!");

        char* sLevelName = nullptr;
        GetIEditor()->GetGame()->GetIGameFramework()->GetEditorLevel(&sLevelName, nullptr);

        ATLControlsDialog dialog(context.parentWidget, controlType);
        dialog.SetScope(sLevelName);
        return dialog.ChooseItem(pPreviousValue);
    }

    //-------------------------------------------------------------------------------------------//
    dll_string AudioTriggerSelector(const SResourceSelectorContext& context, const char* pPreviousValue)
    {
        return ShowSelectDialog(context, pPreviousValue, eACET_TRIGGER);
    }

    //-------------------------------------------------------------------------------------------//
    dll_string AudioSwitchSelector(const SResourceSelectorContext& context, const char* pPreviousValue)
    {
        return ShowSelectDialog(context, pPreviousValue, eACET_SWITCH);
    }

    //-------------------------------------------------------------------------------------------//
    dll_string AudioSwitchStateSelector(const SResourceSelectorContext& context, const char* pPreviousValue)
    {
        return ShowSelectDialog(context, pPreviousValue, eACET_SWITCH_STATE);
    }

    //-------------------------------------------------------------------------------------------//
    dll_string AudioRTPCSelector(const SResourceSelectorContext& context, const char* pPreviousValue)
    {
        return ShowSelectDialog(context, pPreviousValue, eACET_RTPC);
    }

    //-------------------------------------------------------------------------------------------//
    dll_string AudioEnvironmentSelector(const SResourceSelectorContext& context, const char* pPreviousValue)
    {
        return ShowSelectDialog(context, pPreviousValue, eACET_ENVIRONMENT);
    }

    //-------------------------------------------------------------------------------------------//
    dll_string AudioPreloadRequestSelector(const SResourceSelectorContext& context, const char* pPreviousValue)
    {
        return ShowSelectDialog(context, pPreviousValue, eACET_PRELOAD);
    }

    //-------------------------------------------------------------------------------------------//
    REGISTER_RESOURCE_SELECTOR("AudioTrigger", AudioTriggerSelector, ":/AudioControlsEditor/Icons/Trigger_Icon.png");
    REGISTER_RESOURCE_SELECTOR("AudioSwitch", AudioSwitchSelector, ":/AudioControlsEditor/Icons/Switch_Icon.png");
    REGISTER_RESOURCE_SELECTOR("AudioSwitchState", AudioSwitchStateSelector, ":/AudioControlsEditor/Icons/State_Icon.png");
    REGISTER_RESOURCE_SELECTOR("AudioRTPC", AudioRTPCSelector, ":/AudioControlsEditor/Icons/RTPC_Icon.png");
    REGISTER_RESOURCE_SELECTOR("AudioEnvironment", AudioEnvironmentSelector, ":/AudioControlsEditor/Icons/Environment_Icon.png");
    REGISTER_RESOURCE_SELECTOR("AudioPreloadRequest", AudioPreloadRequestSelector, ":/AudioControlsEditor/Icons/Bank_Icon.png");
} // namespace AudioControls
