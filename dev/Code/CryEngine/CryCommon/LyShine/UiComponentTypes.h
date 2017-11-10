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

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Common types used across the LyShine UI system
namespace LyShine
{
    // Uuid strings for all component types define by LyShine.
    // This can be used to create components at runtime. E.g.:
    //     entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
    const char* const UiButtonComponentUuid =           "{7329DFE8-0F3C-4629-B395-78B2CF646B96}";
    const char* const UiCanvasComponentUuid =           "{50B8CF6C-B19A-4D86-AFE9-96EFB820D422}";
    const char* const UiCheckboxComponentUuid =         "{68D62281-B360-4426-AACA-E8BDE8BFEB3A}";
    const char* const UiDraggableComponentUuid =        "{C96B1EEF-033A-479B-829B-ED3555D0F33A}";
    const char* const UiDropTargetComponentUuid =       "{E4B8ACE0-FCE7-42D8-9836-942F910168B4}";
    const char* const UiDynamicLayoutComponentUuid =    "{690BEC14-3642-4247-BD96-FE414CCB7DE7}";
    const char* const UiDynamicScrollBoxComponentUuid = "{6982C200-4D32-43CC-A7F6-F54FA50FCFF5}";
    const char* const UiElementComponentUuid =          "{4A97D63E-CE7A-45B6-AAE4-102DB4334688}";
    const char* const UiFaderComponentUuid =            "{CD01FF77-2249-4ED8-BFFB-33A66A47E17C}";
    const char* const UiImageComponentUuid =            "{BDBEFD23-DBB4-4726-A32D-4FEAC24E51F6}";
    const char* const UiLayoutColumnComponentUuid =     "{4BC2E786-360B-4426-8D9C-9B254C5EA21F}";
    const char* const UiLayoutCellComponentUuid =       "{A0568E58-4382-47F8-8B88-77C64B99AC80}";
    const char* const UiLayoutGridComponentUuid =       "{ADDA3AE5-B9AB-44B7-A462-8B89B398A837}";
    const char* const UiLayoutRowComponentUuid =        "{7B2820C4-7FC7-4F02-B777-6727EB4BAC13}";
    const char* const UiMaskComponentUuid =             "{2279AA38-271D-4D4F-A472-E42B984088AC}";
    const char* const UiScrollBarComponentUuid =        "{6B283F90-3519-47DA-A1DD-65A79CE119CF}";
    const char* const UiScrollBoxComponentUuid =        "{2F539588-AEAB-4341-A6A6-AF645D129693}";
    const char* const UiSliderComponentUuid =           "{2913D76B-36A0-45E0-A104-33C668EB612D}";
    const char* const UiTextComponentUuid =             "{5B3FB2A7-5DC4-4033-A970-001CEC85B6C4}";
    const char* const UiTextInputComponentUuid =        "{2CB3872B-D2B4-4DDB-B39A-97492310AE11}";
    const char* const UiTooltipComponentUuid =          "{493EBF89-C299-4722-829D-4DFAB926795B}";
    const char* const UiTooltipDisplayComponentUuid =   "{18CEE6A7-3CBC-4638-9F4D-87E8D53DDF1A}";
    const char* const UiTransform2dComponentUuid =      "{2751A5A5-3291-4A4D-9FC0-9CB0EB8D1DE6}";

    // The Uuid of the LyShine system component. Can be used from tools to activate this component.
    const char* const lyShineSystemComponentUuid =      "{B0C78B8D-1E5B-47D7-95D0-EC69C0513804}";
};
