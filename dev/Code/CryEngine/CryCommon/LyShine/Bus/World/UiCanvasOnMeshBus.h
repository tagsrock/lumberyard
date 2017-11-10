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

#include <AzCore/Component/ComponentBus.h>
#include <Cry_Math.h>

struct SInputEvent;
struct ray_hit;

////////////////////////////////////////////////////////////////////////////////////////////////////
// UiCanvasOnMeshBus
class UiCanvasOnMeshInterface
    : public AZ::ComponentBus
{
public:
    virtual ~UiCanvasOnMeshInterface() {}

    //! Convert the input collision point into a canvas space position and pass the event and that position
    //! to the UI canvas to handle as a positional input event
    //! \return true if the event was handled
    virtual bool ProcessCollisionInputEvent(const SInputEvent& event, int triangleIndex, Vec3 hitPoint) = 0;

    //! Convert the input ray collision into a canvas space position and pass the event and that position
    //! to the UI canvas to handle as a positional input event
    //! \return true if the event was handled
    virtual bool ProcessRayHitInputEvent(const SInputEvent& event, const ray_hit& rayHit) = 0;
};

using UiCanvasOnMeshBus = AZ::EBus<UiCanvasOnMeshInterface>;
