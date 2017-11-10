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

#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Component/ComponentBus.h>

namespace LmbrCentral
{
    class LightConfiguration;

    /*!
     * LightComponentRequestBus
     * Messages serviced by the light component.
     */
    class LightComponentRequests
        : public AZ::ComponentBus
    {
    public:

        enum class State
        {
            On = 0,
            Off,
        };

        virtual ~LightComponentRequests() {}

        //! Control light state.
        virtual void SetLightState(State state) {}

        //! Turns light on. Returns true if the light was successfully turned on
        virtual bool TurnOnLight() { return false; }

        //! Turns light off. Returns true if the light was successfully turned off
        virtual bool TurnOffLight() { return false; }

        //! Toggles light state.
        virtual void ToggleLight() {}

        ////////////////////////////////////////////////////////////////////
        // Modifiers - these must match the same virutal methods in LightComponentEditorRequests

        // General Settings Modifiers
        virtual void SetVisible(bool isVisible) { (void)isVisible; }
        virtual bool GetVisible() { return true; }

        virtual void SetColor(const AZ::Color& newColor) { (void)newColor; };
        virtual const AZ::Color GetColor() { return AZ::Color::CreateOne(); }

        virtual void SetDiffuseMultiplier(float newMultiplier) { (void)newMultiplier; };
        virtual float GetDiffuseMultiplier() { return FLT_MAX; }

        virtual void SetSpecularMultiplier(float newMultiplier) { (void)newMultiplier; };
        virtual float GetSpecularMultiplier() { return FLT_MAX; }

        virtual void SetAmbient(bool isAmbient) { (void)isAmbient; }
        virtual bool GetAmbient() { return true; }

        // Point Light Specific Modifiers
        virtual void SetPointMaxDistance(float newMaxDistance) { (void)newMaxDistance; };
        virtual float GetPointMaxDistance() { return FLT_MAX; }

        virtual void SetPointAttenuationBulbSize(float newAttenuationBulbSize) { (void)newAttenuationBulbSize; };
        virtual float GetPointAttenuationBulbSize() { return FLT_MAX; }

        // Area Light Specific Modifiers
        virtual void SetAreaMaxDistance(float newMaxDistance) { (void)newMaxDistance; };
        virtual float GetAreaMaxDistance() { return FLT_MAX; }

        virtual void SetAreaWidth(float newWidth) { (void)newWidth; };
        virtual float GetAreaWidth() { return FLT_MAX; }

        virtual void SetAreaHeight(float newHeight) { (void)newHeight; };
        virtual float GetAreaHeight() { return FLT_MAX; }

        // Project Light Specific Modifiers
        virtual void SetProjectorMaxDistance(float newMaxDistance) { (void)newMaxDistance; };
        virtual float GetProjectorMaxDistance() { return FLT_MAX; }

        virtual void SetProjectorAttenuationBulbSize(float newAttenuationBulbSize) { (void)newAttenuationBulbSize; };
        virtual float GetProjectorAttenuationBulbSize() { return FLT_MAX; }

        virtual void SetProjectorFOV(float newFOV) { (void)newFOV; };
        virtual float GetProjectorFOV() { return FLT_MAX; }

        virtual void SetProjectorNearPlane(float newNearPlane) { (void)newNearPlane; };
        virtual float GetProjectorNearPlane() { return FLT_MAX; }

        // Environment Probe Settings
        virtual void SetProbeAreaDimensions(const AZ::Vector3& newDimensions) { (void)newDimensions; }
        virtual const AZ::Vector3 GetProbeAreaDimensions() { return AZ::Vector3::CreateOne(); }

        virtual void SetProbeSortPriority(float newPriority) { (void)newPriority; }
        virtual float GetProbeSortPriority() { return UINT32_MAX; }

        virtual void SetProbeBoxProjected(bool isProbeBoxProjected) { (void)isProbeBoxProjected; }
        virtual bool GetProbeBoxProjected() { return false; }

        virtual void SetProbeBoxHeight(float newHeight) { (void)newHeight; }
        virtual float GetProbeBoxHeight() { return FLT_MAX; }

        virtual void SetProbeBoxLength(float newLength) { (void)newLength; }
        virtual float GetProbeBoxLength() { return FLT_MAX; }

        virtual void SetProbeBoxWidth(float newWidth) { (void)newWidth; }
        virtual float GetProbeBoxWidth() { return FLT_MAX; }

        virtual void SetProbeAttenuationFalloff(float newAttenuationFalloff) { (void)newAttenuationFalloff; }
        virtual float GetProbeAttenuationFalloff() { return FLT_MAX; }
        ////////////////////////////////////////////////////////////////////  
    };

    using LightComponentRequestBus = AZ::EBus<LightComponentRequests>;

    /*!
     * LightComponentEditorRequestBus
     * Editor/UI messages serviced by the light component.
     */
    class LightComponentEditorRequests
        : public AZ::ComponentBus
    {
    public:
        virtual ~LightComponentEditorRequests() {}

        //! Recreates the render light.
        virtual void RefreshLight() {}

        //! Sets the active cubemap resource.
        virtual void SetCubemap(const char* cubemap) {}

        //! Retrieves configured cubemap resolution for generation.
        virtual AZ::u32 GetCubemapResolution() { return 0; }

        //! Retrieves Configuration
        virtual const LightConfiguration& GetConfiguration() const = 0;
        
        //! get if it's customized cubemap
        virtual bool UseCustomizedCubemap() const { return false; }
        ////////////////////////////////////////////////////////////////////
        // Modifiers - these must match the same virutal methods in LightComponentRequests

        // General Light Settings
        virtual void SetVisible(bool isVisible) { (void)isVisible; }
        virtual bool GetVisible() { return true; }

        virtual void SetColor(const AZ::Color& newColor) { (void)newColor; };
        virtual const AZ::Color GetColor() { return AZ::Color::CreateOne(); }

        virtual void SetDiffuseMultiplier(float newMultiplier) { (void)newMultiplier; };
        virtual float GetDiffuseMultiplier() { return FLT_MAX; }

        virtual void SetSpecularMultiplier(float newMultiplier) { (void)newMultiplier; };
        virtual float GetSpecularMultiplier() { return FLT_MAX; }

        virtual void SetAmbient(bool isAmbient) { (void)isAmbient; }
        virtual bool GetAmbient() { return true; }

        // Point Light Specific Modifiers
        virtual void SetPointMaxDistance(float newMaxDistance) { (void)newMaxDistance; };
        virtual float GetPointMaxDistance() { return FLT_MAX; }

        virtual void SetPointAttenuationBulbSize(float newAttenuationBulbSize) { (void)newAttenuationBulbSize; };
        virtual float GetPointAttenuationBulbSize() { return FLT_MAX; }

        // Area Light Specific Modifiers
        virtual void SetAreaMaxDistance(float newMaxDistance) { (void)newMaxDistance; };
        virtual float GetAreaMaxDistance() { return FLT_MAX; }

        virtual void SetAreaWidth(float newWidth) { (void)newWidth; };
        virtual float GetAreaWidth() { return FLT_MAX; }

        virtual void SetAreaHeight(float newHeight) { (void)newHeight; };
        virtual float GetAreaHeight() { return FLT_MAX; }

        // Project Light Specific Modifiers
        virtual void SetProjectorMaxDistance(float newMaxDistance) { (void)newMaxDistance; };
        virtual float GetProjectorMaxDistance() { return FLT_MAX; }

        virtual void SetProjectorAttenuationBulbSize(float newAttenuationBulbSize) { (void)newAttenuationBulbSize; };
        virtual float GetProjectorAttenuationBulbSize() { return FLT_MAX; }

        virtual void SetProjectorFOV(float newFOV) { (void)newFOV; };
        virtual float GetProjectorFOV() { return FLT_MAX; }

        virtual void SetProjectorNearPlane(float newNearPlane) { (void)newNearPlane; };
        virtual float GetProjectorNearPlane() { return FLT_MAX; }

        // Environment Probe Settings
        virtual void SetProbeAreaDimensions(const AZ::Vector3& newDimensions) { (void)newDimensions; }
        virtual const AZ::Vector3 GetProbeAreaDimensions() { return AZ::Vector3::CreateOne(); }

        virtual void SetProbeSortPriority(float newPriority) { (void)newPriority; }
        virtual float GetProbeSortPriority() { return UINT32_MAX; }

        virtual void SetProbeBoxProjected(bool isProbeBoxProjected) { (void)isProbeBoxProjected; }
        virtual bool GetProbeBoxProjected() { return false; }

        virtual void SetProbeBoxHeight(float newHeight) { (void)newHeight; }
        virtual float GetProbeBoxHeight() { return FLT_MAX; }

        virtual void SetProbeBoxLength(float newLength) { (void)newLength; }
        virtual float GetProbeBoxLength() { return FLT_MAX; }

        virtual void SetProbeBoxWidth(float newWidth) { (void)newWidth; }
        virtual float GetProbeBoxWidth() { return FLT_MAX; }

        virtual void SetProbeAttenuationFalloff(float newAttenuationFalloff) { (void)newAttenuationFalloff; }
        virtual float GetProbeAttenuationFalloff() { return FLT_MAX; }
        ////////////////////////////////////////////////////////////////////
    };

    using LightComponentEditorRequestBus = AZ::EBus < LightComponentEditorRequests >;

    /*!
     * LightComponentNotificationBus
     * Events dispatched by the light component.
     */
    class LightComponentNotifications
        : public AZ::ComponentBus
    {
    public:

        virtual ~LightComponentNotifications() {}

        // Sent when the light is turned on.
        virtual void LightTurnedOn() {}

        // Sent when the light is turned off.
        virtual void LightTurnedOff() {}
    };

    using LightComponentNotificationBus = AZ::EBus < LightComponentNotifications >;

    /*!
    * LightSettingsNotifications
    * Events dispatched by the light component or light component editor when settings have changed.
    */
    class LightSettingsNotifications
        : public AZ::ComponentBus
    {
    public:

        virtual ~LightSettingsNotifications() {}

        virtual void AnimationSettingsChanged() = 0;
    };

    using LightSettingsNotificationsBus = AZ::EBus < LightSettingsNotifications >;
    
} // namespace LmbrCentral
