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
#pragma once

#include "EditorLightComponent.h"

namespace LmbrCentral
{
    /*!
     * In-editor point light component.
     * Handles previewing and activating lights in the editor.
     */
    class EditorEnvProbeComponent
        : public EditorLightComponent
    {
    public:
        AZ_COMPONENT(EditorEnvProbeComponent, "{8DBD6035-583E-409F-AFD9-F36829A0655D}", EditorLightComponent);

        static void Reflect(AZ::ReflectContext* context);
        
        void Init() override;

    protected:
        const char* GetLightTypeText() const override
        {
            return "Environment Probe";
        }
    };
} // namespace LmbrCentral

