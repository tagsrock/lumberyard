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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Rtti/BehaviorContext.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Asset/AssetManager.h>

#include <CryCommon/MathConversion.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Cry_Math.h>
#include <I3DEngine.h>
#include <IStatObj.h>
#include <IEditor.h>
#include <Util/PathUtil.h>

#include "EditorLightComponent.h"
#include "EditorAreaLightComponent.h"
#include "EditorPointLightComponent.h"
#include "EditorEnvProbeComponent.h"
#include "EditorProjectorLightComponent.h"


namespace LmbrCentral
{
    // Private statics
    IEditor*             EditorLightComponent::m_editor = nullptr;
    IMaterialManager*    EditorLightComponent::m_materialManager = nullptr;
    
    //class converter. Convert EditorLightComponent to one of four above components
    namespace ClassConverters
    {
        static bool ConvertEditorLightComponent(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            //extract light type
            bool isFound = false;
            LightConfiguration::LightType lightType;                

            int lightConfigIndex = classElement.FindElement(AZ_CRC("EditorLightConfiguration", 0xe4cf6af9));
            if (lightConfigIndex != -1)
            {
                AZ::SerializeContext::DataElementNode configElement = classElement.GetSubElement(lightConfigIndex);
                int baseClassIndex = configElement.FindElement(AZ_CRC("BaseClass1", 0xd4925735));
                if (baseClassIndex != -1)
                {
                    AZ::SerializeContext::DataElementNode baseConfig = configElement.GetSubElement(baseClassIndex);
                    
                    int lightTypeIndex = baseConfig.FindElement(AZ_CRC("LightType", 0x9884ece8));
                    if (lightTypeIndex != -1)
                    {
                        AZ::SerializeContext::DataElementNode lightTypeNode = baseConfig.GetSubElement(lightTypeIndex);
                        lightTypeNode.GetData< LightConfiguration::LightType>(lightType);
                        isFound = true;
                    }
                }
            }

            if (!isFound)
            {
                return false;
            }

            //save all the sub elements for old EditorLightComponent
            typedef AZStd::vector<AZ::SerializeContext::DataElementNode> NodeArray;
            NodeArray subElements;
            for (int i = 0; i < classElement.GetNumSubElements(); i++)
            {
                subElements.push_back(classElement.GetSubElement(i));
            }            

            // Convert to specific editor light component.
            bool result = true;
            switch (lightType)
            {
                case LightConfiguration::LightType::Point:
                    result = classElement.Convert<EditorPointLightComponent>(context);
                    break;
                case LightConfiguration::LightType::Area:
                    result = classElement.Convert<EditorAreaLightComponent>(context);
                    break;
                case LightConfiguration::LightType::Projector:
                    result = classElement.Convert<EditorProjectorLightComponent>(context);
                    break;
                case LightConfiguration::LightType::Probe:
                    result = classElement.Convert<EditorEnvProbeComponent>(context);
                    break;
                default:
                    result = false;
                    break;
            }

            if (result)
            {
                //add base class for the new specific light component
                int baseClass = classElement.AddElement<EditorLightComponent>(context, "BaseClass1");
                AZ::SerializeContext::DataElementNode& baseClassNode = classElement.GetSubElement(baseClass);

                //then add all the sub elements to this base class. it's because we didn't introduce any other new element to specific light components. 
                for (int i = 0; i < subElements.size(); i++)
                {
                    baseClassNode.AddElement(subElements[i]);
                }
            }

            return result;
        }
    }

    void EditorLightComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        EditorLightConfiguration::Reflect(context);

        if (serializeContext)
        {
            //any data using old UUID of EditorLightComponent will be send to converter
            serializeContext->ClassDeprecate("EditorLightComponent", "{33BB1CD4-6A33-46AA-87ED-8BBB40D94B0D}", &ClassConverters::ConvertEditorLightComponent);

            serializeContext->Class<EditorLightComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("EditorLightConfiguration", &EditorLightComponent::m_configuration)
                ->Field("CubemapRegen", &EditorLightComponent::m_cubemapRegen)
                ->Field("ViewCubemap", &EditorLightComponent::m_viewCubemap)
                ->Field("UseCustomizedCubemap", &EditorLightComponent::m_useCustomizedCubemap)
                ->Field("cubemapAsset", &EditorLightComponent::m_cubemapAsset)
            ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<EditorLightComponent>(
                    "Light", "Attach lighting to an entity.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &EditorLightComponent::GetLightTypeText)
                    ->DataElement(0, &EditorLightComponent::m_configuration, "Settings", "Light configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))

                    ->ClassElement(AZ::Edit::ClassElements::Group, "Cubemap generation")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::IsProbe)
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)

                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &EditorLightComponent::m_useCustomizedCubemap, "Use customized cubemap", "Check to enable usage of customized cubemap")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::IsProbe)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLightComponent::OnCustomizedCubemapChanged)

                    //asset selection
                    ->DataElement(0, &EditorLightComponent::m_cubemapAsset, "Cubemap asset", "Cubemap file path")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::IsProbe)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLightComponent::OnCubemapAssetChanged)
                        ->Attribute(AZ::Edit::Attributes::ReadOnly,&EditorLightComponent::CanGenerateCubemap)

                    ->DataElement("Button", &EditorLightComponent::m_cubemapRegen, "Cubemap", "Generate the associated cubemap")
                        ->Attribute(AZ::Edit::Attributes::ButtonText, "Generate")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLightComponent::GenerateCubemap)
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::CanGenerateCubemap)

                    ->DataElement(0, &EditorLightComponent::m_viewCubemap, "View cubemap", "Preview the cubemap in scene")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLightComponent::OnViewCubemapChanged)
                        ->Attribute(AZ::Edit::Attributes::Visibility, &EditorLightComponent::IsProbe)
                ;
            }
        }

        AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext)
        {
            // In the editor we reflect a separate EBus per light type (e.g. Point, Area, Projector). At run-time, we only use a single
            // "LightComponentBus" which is the intersection of the separate buses reflected here.

            // Point Light EBus reflection and VirtualProperties
            behaviorContext->EBus<LightComponentEditorRequestBus>("EditorPointLightComponentBus")
                ->Event("GetVisible", &LightComponentEditorRequestBus::Events::GetVisible)
                ->Event("SetVisible", &LightComponentEditorRequestBus::Events::SetVisible)
                ->VirtualProperty("Visible", "GetVisible", "SetVisible")
                ->Event("GetColor", &LightComponentEditorRequestBus::Events::GetColor)
                ->Event("SetColor", &LightComponentEditorRequestBus::Events::SetColor)
                ->VirtualProperty("Color", "GetColor", "SetColor")
                ->Event("GetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::GetDiffuseMultiplier)
                ->Event("SetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::SetDiffuseMultiplier)
                ->VirtualProperty("DiffuseMultiplier", "GetDiffuseMultiplier", "SetDiffuseMultiplier")
                ->Event("GetSpecularMultiplier", &LightComponentEditorRequestBus::Events::GetSpecularMultiplier)
                ->Event("SetSpecularMultiplier", &LightComponentEditorRequestBus::Events::SetSpecularMultiplier)
                ->VirtualProperty("SpecularMultiplier", "GetSpecularMultiplier", "SetSpecularMultiplier")
                ->Event("GetAmbient", &LightComponentEditorRequestBus::Events::GetAmbient)
                ->Event("SetAmbient", &LightComponentEditorRequestBus::Events::SetAmbient)
                ->VirtualProperty("Ambient", "GetAmbient", "SetAmbient")
                ->Event("GetPointMaxDistance", &LightComponentEditorRequestBus::Events::GetPointMaxDistance)
                ->Event("SetPointMaxDistance", &LightComponentEditorRequestBus::Events::SetPointMaxDistance)
                ->VirtualProperty("PointMaxDistance", "GetPointMaxDistance", "SetPointMaxDistance")
                ->Event("GetPointAttenuationBulbSize", &LightComponentEditorRequestBus::Events::GetPointAttenuationBulbSize)
                ->Event("SetPointAttenuationBulbSize", &LightComponentEditorRequestBus::Events::SetPointAttenuationBulbSize)
                ->VirtualProperty("PointAttenuationBulbSize", "GetPointAttenuationBulbSize", "SetPointAttenuationBulbSize")
                ;

            // Area Light EBus reflection and VirtualProperties
            behaviorContext->EBus<LightComponentEditorRequestBus>("EditorAreaLightComponentBus")
                ->Event("GetVisible", &LightComponentEditorRequestBus::Events::GetVisible)
                ->Event("SetVisible", &LightComponentEditorRequestBus::Events::SetVisible)
                ->VirtualProperty("Visible", "GetVisible", "SetVisible")
                ->Event("GetColor", &LightComponentEditorRequestBus::Events::GetColor)
                ->Event("SetColor", &LightComponentEditorRequestBus::Events::SetColor)
                ->VirtualProperty("Color", "GetColor", "SetColor")
                ->Event("GetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::GetDiffuseMultiplier)
                ->Event("SetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::SetDiffuseMultiplier)
                ->VirtualProperty("DiffuseMultiplier", "GetDiffuseMultiplier", "SetDiffuseMultiplier")
                ->Event("GetSpecularMultiplier", &LightComponentEditorRequestBus::Events::GetSpecularMultiplier)
                ->Event("SetSpecularMultiplier", &LightComponentEditorRequestBus::Events::SetSpecularMultiplier)
                ->VirtualProperty("SpecularMultiplier", "GetSpecularMultiplier", "SetSpecularMultiplier")
                ->Event("GetAmbient", &LightComponentEditorRequestBus::Events::GetAmbient)
                ->Event("SetAmbient", &LightComponentEditorRequestBus::Events::SetAmbient)
                ->VirtualProperty("Ambient", "GetAmbient", "SetAmbient")
                ->Event("GetAreaMaxDistance", &LightComponentEditorRequestBus::Events::GetAreaMaxDistance)
                ->Event("SetAreaMaxDistance", &LightComponentEditorRequestBus::Events::SetAreaMaxDistance)
                ->VirtualProperty("AreaMaxDistance", "GetAreaMaxDistance", "SetAreaMaxDistance")
                ->Event("GetAreaWidth", &LightComponentEditorRequestBus::Events::GetAreaWidth)
                ->Event("SetAreaWidth", &LightComponentEditorRequestBus::Events::SetAreaWidth)
                ->VirtualProperty("AreaWidth", "GetAreaWidth", "SetAreaWidth")
                ->Event("GetAreaHeight", &LightComponentEditorRequestBus::Events::GetAreaHeight)
                ->Event("SetAreaHeight", &LightComponentEditorRequestBus::Events::SetAreaHeight)
                ->VirtualProperty("AreaHeight", "GetAreaHeight", "SetAreaHeight")
                ;

            // Projector Light Ebus reflection and VirtualProperties
            behaviorContext->EBus<LightComponentEditorRequestBus>("EditorProjectorLightComponentBus")
                ->Event("GetVisible", &LightComponentEditorRequestBus::Events::GetVisible)
                ->Event("SetVisible", &LightComponentEditorRequestBus::Events::SetVisible)
                ->VirtualProperty("Visible", "GetVisible", "SetVisible")
                ->Event("GetColor", &LightComponentEditorRequestBus::Events::GetColor)
                ->Event("SetColor", &LightComponentEditorRequestBus::Events::SetColor)
                ->VirtualProperty("Color", "GetColor", "SetColor")
                ->Event("GetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::GetDiffuseMultiplier)
                ->Event("SetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::SetDiffuseMultiplier)
                ->VirtualProperty("DiffuseMultiplier", "GetDiffuseMultiplier", "SetDiffuseMultiplier")
                ->Event("GetSpecularMultiplier", &LightComponentEditorRequestBus::Events::GetSpecularMultiplier)
                ->Event("SetSpecularMultiplier", &LightComponentEditorRequestBus::Events::SetSpecularMultiplier)
                ->VirtualProperty("SpecularMultiplier", "GetSpecularMultiplier", "SetSpecularMultiplier")
                ->Event("GetAmbient", &LightComponentEditorRequestBus::Events::GetAmbient)
                ->Event("SetAmbient", &LightComponentEditorRequestBus::Events::SetAmbient)
                ->VirtualProperty("Ambient", "GetAmbient", "SetAmbient")
                ->Event("GetProjectorMaxDistance", &LightComponentEditorRequestBus::Events::GetProjectorMaxDistance)
                ->Event("SetProjectorMaxDistance", &LightComponentEditorRequestBus::Events::SetProjectorMaxDistance)
                ->VirtualProperty("ProjectorMaxDistance", "GetProjectorMaxDistance", "SetProjectorMaxDistance")
                ->Event("GetProjectorAttenuationBulbSize", &LightComponentEditorRequestBus::Events::GetProjectorAttenuationBulbSize)
                ->Event("SetProjectorAttenuationBulbSize", &LightComponentEditorRequestBus::Events::SetProjectorAttenuationBulbSize)
                ->VirtualProperty("ProjectorAttenuationBulbSize", "GetProjectorAttenuationBulbSize", "SetProjectorAttenuationBulbSize")
                ->Event("GetProjectorFOV", &LightComponentEditorRequestBus::Events::GetProjectorFOV)
                ->Event("SetProjectorFOV", &LightComponentEditorRequestBus::Events::SetProjectorFOV)
                ->VirtualProperty("ProjectorFOV", "GetProjectorFOV", "SetProjectorFOV")
                ->Event("GetProjectorNearPlane", &LightComponentEditorRequestBus::Events::GetProjectorNearPlane)
                ->Event("SetProjectorNearPlane", &LightComponentEditorRequestBus::Events::SetProjectorNearPlane)
                ->VirtualProperty("ProjectorNearPlane", "GetProjectorNearPlane", "SetProjectorNearPlane")
                ;

            // Environment Probe Light Ebus reflection and VirtualProperties
            behaviorContext->EBus<LightComponentEditorRequestBus>("EditorProbeLightComponentBus")
                ->Event("GetVisible", &LightComponentEditorRequestBus::Events::GetVisible)
                ->Event("SetVisible", &LightComponentEditorRequestBus::Events::SetVisible)
                ->VirtualProperty("Visible", "GetVisible", "SetVisible")
                ->Event("GetColor", &LightComponentEditorRequestBus::Events::GetColor)
                ->Event("SetColor", &LightComponentEditorRequestBus::Events::SetColor)
                ->VirtualProperty("Color", "GetColor", "SetColor")
                ->Event("GetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::GetDiffuseMultiplier)
                ->Event("SetDiffuseMultiplier", &LightComponentEditorRequestBus::Events::SetDiffuseMultiplier)
                ->VirtualProperty("DiffuseMultiplier", "GetDiffuseMultiplier", "SetDiffuseMultiplier")
                ->Event("GetSpecularMultiplier", &LightComponentEditorRequestBus::Events::GetSpecularMultiplier)
                ->Event("SetSpecularMultiplier", &LightComponentEditorRequestBus::Events::SetSpecularMultiplier)
                ->VirtualProperty("SpecularMultiplier", "GetSpecularMultiplier", "SetSpecularMultiplier")
                ->Event("GetProbeAreaDimensions", &LightComponentEditorRequestBus::Events::GetProbeAreaDimensions)
                ->Event("SetProbeAreaDimensions", &LightComponentEditorRequestBus::Events::SetProbeAreaDimensions)
                ->VirtualProperty("ProbeAreaDimensions", "GetProbeAreaDimensions", "SetProbeAreaDimensions")
                ->Event("GetProbeSortPriority", &LightComponentEditorRequestBus::Events::GetProbeSortPriority)
                ->Event("SetProbeSortPriority", &LightComponentEditorRequestBus::Events::SetProbeSortPriority)
                ->VirtualProperty("ProbeSortPriority", "GetProbeSortPriority", "SetProbeSortPriority")
                ->Event("GetProbeBoxProjected", &LightComponentEditorRequestBus::Events::GetProbeBoxProjected)
                ->Event("SetProbeBoxProjected", &LightComponentEditorRequestBus::Events::SetProbeBoxProjected)
                ->VirtualProperty("ProbeBoxProjected", "GetProbeBoxProjected", "SetProbeBoxProjected")
                ->Event("GetProbeBoxHeight", &LightComponentEditorRequestBus::Events::GetProbeBoxHeight)
                ->Event("SetProbeBoxHeight", &LightComponentEditorRequestBus::Events::SetProbeBoxHeight)
                ->VirtualProperty("ProbeBoxHeight", "GetProbeBoxHeight", "SetProbeBoxHeight")
                ->Event("GetProbeBoxLength", &LightComponentEditorRequestBus::Events::GetProbeBoxLength)
                ->Event("SetProbeBoxLength", &LightComponentEditorRequestBus::Events::SetProbeBoxLength)
                ->VirtualProperty("ProbeBoxLength", "GetProbeBoxLength", "SetProbeBoxLength")
                ->Event("GetProbeBoxWidth", &LightComponentEditorRequestBus::Events::GetProbeBoxWidth)
                ->Event("SetProbeBoxWidth", &LightComponentEditorRequestBus::Events::SetProbeBoxWidth)
                ->VirtualProperty("ProbeBoxWidth", "GetProbeBoxWidth", "SetProbeBoxWidth")
                ->Event("GetProbeAttenuationFalloff", &LightComponentEditorRequestBus::Events::GetProbeAttenuationFalloff)
                ->Event("SetProbeAttenuationFalloff", &LightComponentEditorRequestBus::Events::SetProbeAttenuationFalloff)
                ->VirtualProperty("ProbeAttenuationFalloff", "GetProbeAttenuationFalloff", "SetProbeAttenuationFalloff")
                ;
        }
    }

    void EditorLightConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorLightConfiguration, LightConfiguration>()->
                Version(1);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<LightConfiguration>(
                    "Configuration", "Light configuration")->

                    ClassElement(AZ::Edit::ClassElements::EditorData, "")->
                      Attribute(AZ::Edit::Attributes::AutoExpand, true)->
                      Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)->

                    DataElement(AZ::Edit::UIHandlers::CheckBox, &LightConfiguration::m_visible, "Visible", "The current visibility status of this flare")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                    
                    DataElement(0, &LightConfiguration::m_onInitially, "On initially", "The light is initially turned on.")->

                    ClassElement(AZ::Edit::ClassElements::Group, "General Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(AZ::Edit::UIHandlers::Color, &LightConfiguration::m_color, "Color", "Light color")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_diffuseMultiplier, "Diffuse multiplier", "Diffuse color multiplier")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->

                    DataElement(0, &LightConfiguration::m_specMultiplier, "Specular multiplier", "Specular multiplier")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->

                    DataElement(0, &LightConfiguration::m_ambient, "Ambient", "Ambient light")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAmbientLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    // Point Light Settings
                    ClassElement(AZ::Edit::ClassElements::Group, "Point Light Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_pointMaxDistance, "Max distance", "Point light radius")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetPointLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        
                    DataElement(0, &LightConfiguration::m_pointAttenuationBulbSize, "Attenuation bulb size", "Radius of area inside falloff.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetPointLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->

                    // Area Light Settings
                    ClassElement(AZ::Edit::ClassElements::Group, "Area Light Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_areaWidth, "Area width", "Area light width.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAreaSettingVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_areaHeight, "Area height", "Area light height.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAreaSettingVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_areaMaxDistance, "Max distance", "Area light max distance.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetAreaSettingVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " _")->

                    // Projector settings.
                    ClassElement(AZ::Edit::ClassElements::Group, "Projector Light Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_projectorRange, "Max distance", "Projector light range")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_projectorAttenuationBulbSize, "Attenuation bulb size", "Radius of area inside falloff.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->

                    DataElement(AZ::Edit::UIHandlers::Slider, &LightConfiguration::m_projectorFOV, "FOV", "Projector light FOV")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 1.f)->
                        Attribute(AZ::Edit::Attributes::Max, 180.0f)->      //Projector will start shrinking if FOV goes above 180 degrees
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " degrees")->

                    DataElement(0, &LightConfiguration::m_projectorNearPlane, "Near plane", "Projector light near plane")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 1.f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_projectorTexture, "Texture", "Projector light texture")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_material, "Material", "Projector light material")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProjectorLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                       
                    //environment probe settings
                    ClassElement(AZ::Edit::ClassElements::Group, "Environment Probe Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_probeArea, "Area dimensions", "Probe area")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::StyleForX, "font: bold; color: rgb(184,51,51);")->
                        Attribute(AZ::Edit::Attributes::StyleForY, "font: bold; color: rgb(48,208,120);")->
                        Attribute(AZ::Edit::Attributes::StyleForZ, "font: bold; color: rgb(66,133,244);")->

                    DataElement(AZ::Edit::UIHandlers::CheckBox, &LightConfiguration::m_isBoxProjected, "Box projected", "Check to enable box projection during runtime")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                    
                    DataElement(0, &LightConfiguration::m_boxHeight, "Box height", "Height of box projection area")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_boxWidth, "Box width", "Width of box projection area")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_boxLength, "Box length", "Length of box projection area")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_attenFalloffMax, "Attenuation falloff", "Attenuation falloff value.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 1.0f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.02f)->

                    DataElement(0, &LightConfiguration::m_probeSortPriority, "Sort priority", "Sort priority")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_probeCubemapResolution, "Resolution", "Cubemap resolution")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetProbeLightVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                        EnumAttribute(ResolutionSetting::ResDefault, "Default (256)")->
                        EnumAttribute(ResolutionSetting::Res32, "32")->
                        EnumAttribute(ResolutionSetting::Res64, "64")->
                        EnumAttribute(ResolutionSetting::Res128, "128")->
                        EnumAttribute(ResolutionSetting::Res256, "256")->
                        EnumAttribute(ResolutionSetting::Res512, "512")->
                            
                    ClassElement(AZ::Edit::ClassElements::Group, "Animation")->

                    DataElement(0, &LightConfiguration::m_animIndex, "Style", "Light animation curve ID (\"style\") as it corresponds to values in Light.cfx")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::OnAnimationSettingChanged)->
                        Attribute(AZ::Edit::Attributes::Max, 255)->

                    DataElement(0, &LightConfiguration::m_animSpeed, "Speed", "Multiple of the base animation rate")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::OnAnimationSettingChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Max, 4.f)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->

                    DataElement(0, &LightConfiguration::m_animPhase, "Phase", "Animation start offset from 0 to 1.  0.1 would be 10% into the animation")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::OnAnimationSettingChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->
                        Attribute(AZ::Edit::Attributes::Max, 1.f)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Options")->

                    DataElement(0, &LightConfiguration::m_viewDistMultiplier, "View distance multiplier", "Adjusts max view distance. If 1.0 then default is used. 1.1 would be 10% further than default.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Suffix, "x")->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_minSpec, "Minimum spec", "Min spec for light to be active.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        EnumAttribute(EngineSpec::Never, "Never")->
                        EnumAttribute(EngineSpec::VeryHigh, "Very high")->
                        EnumAttribute(EngineSpec::High, "High")->
                        EnumAttribute(EngineSpec::Medium, "Medium")->
                        EnumAttribute(EngineSpec::Low, "Low")->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_castShadowsSpec, "Cast shadow spec", "Min spec for shadow casting.")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSpecVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                        EnumAttribute(EngineSpec::Never, "Never")->
                        EnumAttribute(EngineSpec::VeryHigh, "Very high")->
                        EnumAttribute(EngineSpec::High, "High")->
                        EnumAttribute(EngineSpec::Medium, "Medium")->
                        EnumAttribute(EngineSpec::Low, "Low")->

                    DataElement(AZ::Edit::UIHandlers::ComboBox, &LightConfiguration::m_voxelGIMode, "Voxel GI mode", "Mode for light interaction with voxel GI.")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MajorPropertyChanged)->
                        EnumAttribute(IRenderNode::VM_None, "None")->
                        EnumAttribute(IRenderNode::VM_Static, "Static")->
                        EnumAttribute(IRenderNode::VM_Dynamic, "Dynamic")->

                    DataElement(0, &LightConfiguration::m_ignoreVisAreas, "Ignore vis areas", "Ignore vis areas")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_volumetricFog, "Volumetric fog", "Affects volumetric fog")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_volumetricFogOnly, "Volumetric fog only", "Only affects volumetric fog")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_indoorOnly, "Indoor only", "Indoor only")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    DataElement(0, &LightConfiguration::m_affectsThisAreaOnly, "Affects this area only", "Light only affects this area")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->

                    ClassElement(AZ::Edit::ClassElements::Group, "Advanced")->

                    DataElement(0, &LightConfiguration::m_deferred, "Deferred", "Deferred light")->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_Hide", 0x32ab90f7))->         // Deprecated on non mobile platforms - hidden until we have a platform to use this

                    ClassElement(AZ::Edit::ClassElements::Group, "Shadow Settings")->
                        Attribute(AZ::Edit::Attributes::AutoExpand, true)->

                    DataElement(0, &LightConfiguration::m_shadowBias, "Shadow bias", "Shadow bias")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.5f)->

                    DataElement(0, &LightConfiguration::m_shadowSlopeBias, "Shadow slope bias", "Shadow slope bias")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.5f)->

                    DataElement(0, &LightConfiguration::m_shadowResScale, "Shadow resolution scale", "Shadow res scale")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 10.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)->

                    DataElement(0, &LightConfiguration::m_shadowUpdateMinRadius, "Shadow update radius", "Shadow update min radius")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 100.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.5f)->
                        Attribute(AZ::Edit::Attributes::Suffix, " m")->

                    DataElement(0, &LightConfiguration::m_shadowUpdateRatio, "Shadow update ratio", "Shadow update ratio")->
                        Attribute(AZ::Edit::Attributes::Visibility, &LightConfiguration::GetShadowSettingsVisibility)->
                        Attribute(AZ::Edit::Attributes::ChangeNotify, &LightConfiguration::MinorPropertyChanged)->
                        Attribute(AZ::Edit::Attributes::Min, 0.f)->
                        Attribute(AZ::Edit::Attributes::Max, 10.f)->
                        Attribute(AZ::Edit::Attributes::Step, 0.1f)
                ;
            }
        }
    }

    AZ::Crc32 EditorLightConfiguration::GetAmbientLightVisibility() const 
    {
        return m_lightType != LightType::Probe ?
                AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::GetPointLightVisibility() const
    {
        return m_lightType == LightType::Point ?
            AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::GetProjectorLightVisibility() const
    {
        return m_lightType == LightType::Projector ?
            AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::GetProbeLightVisibility() const
    {
        return m_lightType == LightType::Probe ?
            AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::GetShadowSpecVisibility() const
    {
        return m_lightType != LightType::Probe ?
            AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::GetShadowSettingsVisibility() const
    {
        return m_castShadowsSpec != EngineSpec::Never ?
            AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::GetAreaSettingVisibility() const
    {
        return m_lightType == LightType::Area ?
            AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
    }

    AZ::Crc32 EditorLightConfiguration::MajorPropertyChanged()
    {
        if (m_editorEntityId.IsValid())
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, RefreshLight);
        }

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZ::Crc32 EditorLightConfiguration::MinorPropertyChanged()
    {
        if (m_editorEntityId.IsValid())
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, RefreshLight);
        }

        return AZ::Edit::PropertyRefreshLevels::None;
    }

    AZ::Crc32 EditorLightConfiguration::OnAnimationSettingChanged()
    {
        if (m_editorEntityId.IsValid())
        {
            EBUS_EVENT_ID(m_editorEntityId, LightComponentEditorRequestBus, RefreshLight);

            EBUS_EVENT(LightSettingsNotificationsBus, AnimationSettingsChanged);
        }

        return AZ_CRC("RefreshNone", 0x98a5045b);
    }

    EditorLightComponent::EditorLightComponent()
        : m_useCustomizedCubemap(false)
        , m_viewCubemap (false)
        , m_cubemapRegen(false)
    {
        m_configuration.m_projectorTexture.SetAssetPath("engineassets/textures/defaults/spot_default.dds");
    }

    EditorLightComponent::~EditorLightComponent()
    {
    }

    void EditorLightComponent::Init()
    {
        Base::Init();
    }

    void EditorLightComponent::Activate()
    {
        Base::Activate();

        m_configuration.m_editorEntityId = GetEntityId();

        m_cubemapAsset.SetAssetPath(m_configuration.m_probeCubemap.c_str());

        m_light.SetEntity(GetEntityId());
        RefreshLight();

        if (m_configuration.m_lightType == LightConfiguration::LightType::Probe)
        {
            m_cubemapPreview.Setup(m_configuration.m_probeCubemap.c_str());

            AZ::Transform transform = AZ::Transform::Identity();
            AZ::TransformBus::EventResult(transform, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
            m_cubemapPreview.SetTransform(AZTransformToLYTransform(transform));

            OnViewCubemapChanged(); // Check to see if it should be displayed now.
        }

        LightComponentEditorRequestBus::Handler::BusConnect(GetEntityId());
        RenderNodeRequestBus::Handler::BusConnect(GetEntityId());
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
    }

    void EditorLightComponent::Deactivate()
    {
        LightComponentEditorRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();

        gEnv->p3DEngine->FreeRenderNodeState(&m_cubemapPreview);

        m_light.DestroyRenderLight();
        m_light.SetEntity(AZ::EntityId());

        m_configuration.m_editorEntityId.SetInvalid();

        Base::Deactivate();
    }

    void EditorLightComponent::OnEntityVisibilityChanged(bool /*visibility*/)
    {
        RefreshLight();
    }

    void EditorLightComponent::OnEditorSpecChange()
    {
        RefreshLight();
    }

    void EditorLightComponent::RefreshLight()
    {
        EditorLightConfiguration temp = m_configuration;

        // take the entity's visibility into account
        bool entityVisibility = true;
        AzToolsFramework::EditorVisibilityRequestBus::EventResult(entityVisibility, GetEntityId(), &AzToolsFramework::EditorVisibilityRequestBus::Events::GetCurrentVisibility);
        temp.m_visible &= entityVisibility;

        m_light.UpdateRenderLight(temp);
    }

    IRenderNode* EditorLightComponent::GetRenderNode()
    {
        return m_light.GetRenderNode();
    }

    float EditorLightComponent::GetRenderNodeRequestBusOrder() const
    {
        return LightComponent::s_renderNodeRequestBusOrder;
    }

    bool EditorLightComponent::IsProbe() const
    {
        return (m_configuration.m_lightType == LightConfiguration::LightType::Probe);
    }

    const char* EditorLightComponent::GetCubemapAssetName() const
    {
        return m_configuration.m_probeCubemap.c_str();
    }


    bool EditorLightComponent::CanGenerateCubemap() const
    {
        return (m_configuration.m_lightType == LightConfiguration::LightType::Probe) && (!m_useCustomizedCubemap);
    }

    void EditorLightComponent::GenerateCubemap()
    {
        if (CanGenerateCubemap())
        {
            EBUS_EVENT(AzToolsFramework::EditorRequests::Bus,
                GenerateCubemapForEntity,
                GetEntityId(),
                nullptr);
        }
    }

    void EditorLightComponent::OnViewCubemapChanged()
    {
        if (m_viewCubemap)
        {
            gEnv->p3DEngine->RegisterEntity(&m_cubemapPreview);
        }
        else
        {
            gEnv->p3DEngine->FreeRenderNodeState(&m_cubemapPreview);
        }
    }
    
    void EditorLightComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        LightComponent* lightComponent = gameEntity->CreateComponent<LightComponent>();

        if (lightComponent)
        {
            lightComponent->m_configuration = m_configuration;
        }
    }

    void EditorLightComponent::SetCubemap(const char* cubemap)
    {
        if (cubemap != m_configuration.m_probeCubemap)
        {
            AzToolsFramework::ScopedUndoBatch undo("Cubemap Assignment");

            m_cubemapAsset.SetAssetPath(cubemap);
            m_configuration.m_probeCubemap = m_cubemapAsset.GetAssetPath();
            m_cubemapPreview.UpdateTexture(m_configuration.m_probeCubemap.c_str());

            EBUS_EVENT(AzToolsFramework::ToolsApplicationRequests::Bus, AddDirtyEntity, GetEntityId());

            //get the notice when the dds generated by AP. we will only refresh the m_cubemapAsset when the dds is generated
            AzFramework::AssetCatalogEventBus::Handler::BusConnect();

            if (EditorLightConfiguration::LightType::Probe == m_configuration.m_lightType)
            {
                RefreshLight();
            }
        }
    }
    
    void EditorLightComponent::OnCatalogAssetAdded(const AZ::Data::AssetId& assetId)
    {
        AZ::Data::AssetId cmAssetId;
        EBUS_EVENT_RESULT(cmAssetId, AZ::Data::AssetCatalogRequestBus, GetAssetIdByPath, m_cubemapAsset.GetAssetPath().c_str(), 
            m_cubemapAsset.GetAssetType(), true);

        if (cmAssetId == assetId)
        {
            AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
            //refresh the tree since we don't need to wait for the asset imported
            if (IsSelected())
            {
                EBUS_EVENT(AzToolsFramework::ToolsApplicationEvents::Bus, InvalidatePropertyDisplay, AzToolsFramework::Refresh_AttributesAndValues);
            }
        }
    }

    void EditorLightComponent::OnCatalogAssetChanged(const AZ::Data::AssetId& assetId)
    {
        OnCatalogAssetAdded(assetId);
    }

    AZ::Crc32 EditorLightComponent::OnCubemapAssetChanged()
    {        
        //in case user select a "_diff" texture file. remove it and generate specular file name
        static const char* diffExt = "_diff";
        static const int diffStrSize = 5; //string len of "_diff"

        const char* specularCubemap = 0;
        string specularName(m_cubemapAsset.GetAssetPath().c_str());

        int strIndex = specularName.find(diffExt);
        if (strIndex >= 0)
        {
            specularName = specularName.substr(0, strIndex) + specularName.substr(strIndex + diffStrSize, specularName.length());
            specularCubemap = specularName.c_str();
            m_cubemapAsset.SetAssetPath(specularName);
        }

        //set value back to light configuration
        m_configuration.m_probeCubemap = m_cubemapAsset.GetAssetPath();
        m_cubemapPreview.UpdateTexture(m_configuration.m_probeCubemap.c_str());

        RefreshLight();

        return AZ::Edit::PropertyRefreshLevels::ValuesOnly;
    }

    AZ::Crc32 EditorLightComponent::OnCustomizedCubemapChanged()
    {
        //clean assets
        m_cubemapAsset.SetAssetPath("");
        m_configuration.m_probeCubemap = m_cubemapAsset.GetAssetPath();
        m_cubemapPreview.UpdateTexture(m_configuration.m_probeCubemap.c_str());

        RefreshLight();

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZ::u32 EditorLightComponent::GetCubemapResolution()
    {
        return static_cast<AZ::u32>(m_configuration.m_probeCubemapResolution);
    }

    bool EditorLightComponent::UseCustomizedCubemap() const
    {
        return m_useCustomizedCubemap;
    }

    const LightConfiguration& EditorLightComponent::GetConfiguration() const
    {
        return m_configuration;
    }
    
    ////////////////////////////////////////////////////////////
    // Modifiers
    void EditorLightComponent::SetVisible(bool isVisible)
    {
        if (m_configuration.m_visible != isVisible)
        {
            m_configuration.m_visible = isVisible;
            m_configuration.MajorPropertyChanged();
        }
    }

    bool EditorLightComponent::GetVisible()
    {
        return m_configuration.m_visible;
    }

    void EditorLightComponent::SetColor(const AZ::Color& newColor)
    {
        if (m_configuration.m_color != newColor)
        {
            m_configuration.m_color = newColor;
            m_configuration.MinorPropertyChanged();
        }
    }

    const AZ::Color EditorLightComponent::GetColor()
    {
        return m_configuration.m_color;
    }

    void EditorLightComponent::SetDiffuseMultiplier(float newMultiplier)
    {
        if (newMultiplier != m_configuration.m_diffuseMultiplier)
        {
            m_configuration.m_diffuseMultiplier = newMultiplier;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetDiffuseMultiplier()
    {
        return m_configuration.m_diffuseMultiplier;
    }

    void EditorLightComponent::SetSpecularMultiplier(float newMultiplier)
    {
        if (newMultiplier != m_configuration.m_specMultiplier)
        {
            m_configuration.m_specMultiplier = newMultiplier;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetSpecularMultiplier()
    {
        return m_configuration.m_specMultiplier;
    }

    void EditorLightComponent::SetAmbient(bool isAmbient)
    {
        if (isAmbient != m_configuration.m_ambient)
        {
            m_configuration.m_ambient = isAmbient;
            m_configuration.MinorPropertyChanged();
        }
    }
    bool EditorLightComponent::GetAmbient()
    {
        return m_configuration.m_ambient;
    }

    void EditorLightComponent::SetPointMaxDistance(float newMaxDistance)
    {
        if (newMaxDistance != m_configuration.m_pointMaxDistance)
        {
            m_configuration.m_pointMaxDistance = newMaxDistance;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetPointMaxDistance()
    {
        return m_configuration.m_pointMaxDistance;
    }

    void EditorLightComponent::SetPointAttenuationBulbSize(float newAttenuationBulbSize)
    {
        if (newAttenuationBulbSize != m_configuration.m_pointAttenuationBulbSize)
        {
            m_configuration.m_pointAttenuationBulbSize = newAttenuationBulbSize;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetPointAttenuationBulbSize()
    {
        return m_configuration.m_pointAttenuationBulbSize;
    }

    void EditorLightComponent::SetAreaMaxDistance(float newMaxDistance)
    {
        if (newMaxDistance != m_configuration.m_areaMaxDistance)
        {
            m_configuration.m_areaMaxDistance = newMaxDistance;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetAreaMaxDistance()
    {
        return m_configuration.m_areaMaxDistance;
    }

    void EditorLightComponent::SetAreaWidth(float newWidth)
    {
        if (newWidth != m_configuration.m_areaWidth)
        {
            m_configuration.m_areaWidth = newWidth;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetAreaWidth()
    {
        return m_configuration.m_areaWidth;
    }

    void EditorLightComponent::SetAreaHeight(float newHeight)
    {
        if (newHeight != m_configuration.m_areaHeight)
        {
            m_configuration.m_areaHeight = newHeight;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetAreaHeight()
    {
        return m_configuration.m_areaHeight;
    }

    void EditorLightComponent::SetProjectorMaxDistance(float newMaxDistance)
    {
        if (newMaxDistance != m_configuration.m_projectorRange)
        {
            m_configuration.m_projectorRange = newMaxDistance;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetProjectorMaxDistance()
    {
        return m_configuration.m_projectorRange;
    }

    void EditorLightComponent::SetProjectorAttenuationBulbSize(float newAttenuationBulbSize)
    {
        if (newAttenuationBulbSize != m_configuration.m_projectorAttenuationBulbSize)
        {
            m_configuration.m_projectorAttenuationBulbSize = newAttenuationBulbSize;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetProjectorAttenuationBulbSize()
    {
        return m_configuration.m_projectorAttenuationBulbSize;
    }

    void EditorLightComponent::SetProjectorFOV(float newFOV)
    {
        if (newFOV != m_configuration.m_projectorFOV)
        {
            m_configuration.m_projectorFOV = newFOV;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetProjectorFOV()
    {
        return m_configuration.m_projectorFOV;
    }

    void EditorLightComponent::SetProjectorNearPlane(float newNearPlane)
    {
        if (newNearPlane != m_configuration.m_projectorNearPlane)
        {
            m_configuration.m_projectorNearPlane = newNearPlane;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetProjectorNearPlane()
    {
        return m_configuration.m_projectorNearPlane;
    }

    void EditorLightComponent::SetProbeAreaDimensions(const AZ::Vector3& newDimensions)
    {
        if (newDimensions != m_configuration.m_probeArea)
        {
            m_configuration.m_probeArea = newDimensions;
            m_configuration.MinorPropertyChanged();
        }
    }
    const AZ::Vector3 EditorLightComponent::GetProbeAreaDimensions()
    {
        return m_configuration.m_probeArea;
    }

    void EditorLightComponent::SetProbeSortPriority(float newPriority)
    {
        if (newPriority != m_configuration.m_probeSortPriority)
        {
            m_configuration.m_probeSortPriority = (AZ::u32)newPriority;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetProbeSortPriority()
    {
        return m_configuration.m_probeSortPriority;
    }

    void EditorLightComponent::SetProbeBoxProjected(bool isProbeBoxProjected)
    {
        if (isProbeBoxProjected != m_configuration.m_isBoxProjected)
        {
            m_configuration.m_isBoxProjected = isProbeBoxProjected;
            m_configuration.MinorPropertyChanged();
        }
    }
    bool EditorLightComponent::GetProbeBoxProjected()
    {
        return m_configuration.m_isBoxProjected;
    }

    void EditorLightComponent::SetProbeBoxHeight(float newHeight)
    {
        if (newHeight != m_configuration.m_boxHeight)
        {
            m_configuration.m_boxHeight = newHeight;
            m_configuration.MinorPropertyChanged();
        }
    }
    float EditorLightComponent::GetProbeBoxHeight()
    {
        return m_configuration.m_boxHeight;
    }

    void EditorLightComponent::SetProbeBoxLength(float newLength)
    {
        if (newLength != m_configuration.m_boxLength)
        {
            m_configuration.m_boxLength = newLength;
            m_configuration.MinorPropertyChanged();
        }
    }
        
    float EditorLightComponent::GetProbeBoxLength()
    {
        return m_configuration.m_boxLength;
    }

    void EditorLightComponent::SetProbeBoxWidth(float newWidth)
    {
        if (newWidth != m_configuration.m_boxWidth)
        {
            m_configuration.m_boxWidth = newWidth;
            m_configuration.MinorPropertyChanged();
        }
    }

    float EditorLightComponent::GetProbeBoxWidth()
    {
        return m_configuration.m_boxWidth;
    }

    void EditorLightComponent::SetProbeAttenuationFalloff(float newAttenuationFalloff)
    {
        if (newAttenuationFalloff != m_configuration.m_attenFalloffMax)
        {
            m_configuration.m_attenFalloffMax = newAttenuationFalloff;
            m_configuration.MinorPropertyChanged();
        }
    }

    float EditorLightComponent::GetProbeAttenuationFalloff()
    {
        return m_configuration.m_attenFalloffMax;
    }
    ///////////////////////////////////////////////////////////////////


    void EditorLightComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& world)
    {
        m_cubemapPreview.SetTransform(AZTransformToLYTransform(world));
    }

    const char* EditorLightComponent::GetLightTypeText() const
    {
        return "Deprecated Light";
    }

    void EditorLightComponent::DisplayEntity(bool& handled)
    {
        handled = true;

        // Don't draw extra visualization unless selected.
        if (!IsSelected())
        {
            return;
        }

        auto* dc = AzFramework::EntityDebugDisplayRequestBus::FindFirstHandler();
        AZ_Assert(dc, "Invalid display context.");

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        transform.ExtractScaleExact();
        dc->PushMatrix(transform);
        const AZ::Color& color = m_configuration.m_color;
        dc->SetColor(AZ::Vector4(color.GetR(), color.GetG(), color.GetB(), 1.f));

        switch (m_configuration.m_lightType)
        {
        case EditorLightConfiguration::LightType::Point:
        {
            dc->DrawWireSphere(AZ::Vector3::CreateZero(), m_configuration.m_pointMaxDistance);
            dc->DrawWireSphere(AZ::Vector3::CreateZero(), m_configuration.m_pointAttenuationBulbSize);
            break;
        }
        case EditorLightConfiguration::LightType::Area:
        {
            dc->SetColor(AZ::Vector4(color.GetR(), color.GetG(), color.GetB(), 0.5f));
            const auto& area = AZ::Vector3(m_configuration.m_areaMaxDistance, m_configuration.m_areaWidth, m_configuration.m_areaHeight);
            dc->DrawWireBox(AZ::Vector3(0, -area.GetY() * 0.5f, -area.GetZ() * 0.5f),
                AZ::Vector3(area.GetX(),  area.GetY() * 0.5f,  area.GetZ() * 0.5f));
            break;
        }
        case EditorLightConfiguration::LightType::Projector:
        {
            dc->SetColor(AZ::Vector4(color.GetR(), color.GetG(), color.GetB(), 0.5f));

            const float range = m_configuration.m_projectorRange;
            const float attenuation = m_configuration.m_projectorAttenuationBulbSize;
            const float nearPlane = m_configuration.m_projectorNearPlane;

            DrawProjectionGizmo(dc, range);
            DrawProjectionGizmo(dc, attenuation);
            DrawPlaneGizmo(dc, nearPlane);

            break;
        }
        case EditorLightConfiguration::LightType::Probe:
        { 
            AZ::Vector3 halfAreaSize = m_configuration.m_probeArea/2;
            dc->SetColor(1, 1, 0, 0.8f);
            dc->DrawWireBox( -halfAreaSize, halfAreaSize);
            if (m_configuration.m_isBoxProjected)
            {
                AZ::Vector3 halfBoxSize = AZ::Vector3(m_configuration.m_boxWidth, m_configuration.m_boxLength, m_configuration.m_boxHeight) / 2;
                dc->SetColor(0, 1, 0, 0.8f);
                dc->DrawWireBox(-halfBoxSize, halfBoxSize);
            }

            // Note that rendering the cubemap preview is handled by m_cubemapPreview

            break;
        }
        }

        dc->PopMatrix();
    }

    void EditorLightComponent::DrawProjectionGizmo(AzFramework::EntityDebugDisplayRequests* dc, const float radius) const
    {
        //Don't draw if the radius isn't going to result in anything visible
        if (radius <= 0)
        {
            return;
        }

        const int numPoints = 16;     // per one arc
        const int numArcs = 6;

        AZ::Vector3 points[numPoints * numArcs];
        {
            // Generate 4 arcs on intersection of sphere with pyramid.
            const float fov = DEG2RAD(m_configuration.m_projectorFOV);

            const AZ::Vector3 lightAxis(radius, 0.0f, 0.0f);
            const float tanA = tan_tpl(fov * 0.5f);
            const float fovProj = asin_tpl(1.0f / sqrtf(2.0f + 1.0f / (tanA * tanA))) * 2.0f;

            const float halfFov = 0.5f * fov;
            const float halfFovProj = fovProj * 0.5f;
            const float anglePerSegmentOfFovProj = 1.0f / (numPoints - 1) * fovProj;

            const AZ::Quaternion yRot = AZ::Quaternion::CreateRotationY(halfFov);
            AZ::Vector3* arcPoints = points;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = yRot * AZ::Quaternion::CreateRotationZ(angle) * lightAxis;
            }

            const AZ::Quaternion zRot = AZ::Quaternion::CreateRotationZ(halfFov);
            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = (numPoints - i - 1) * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = zRot * AZ::Quaternion::CreateRotationY(angle) * lightAxis;
            }

            const AZ::Quaternion nyRot = AZ::Quaternion::CreateRotationY(-halfFov);
            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = (numPoints - i - 1) * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = nyRot * AZ::Quaternion::CreateRotationZ(angle) * lightAxis;
            }

            const AZ::Quaternion nzRot = AZ::Quaternion::CreateRotationZ(-halfFov);
            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFovProj - halfFovProj;
                arcPoints[i] = nzRot * AZ::Quaternion::CreateRotationY(angle) * lightAxis;
            }

            arcPoints += numPoints;
            const float anglePerSegmentOfFov = 1.0f / (numPoints - 1) * fov;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFov - halfFov;
                arcPoints[i] = AZ::Quaternion::CreateRotationY(angle) * lightAxis;
            }

            arcPoints += numPoints;
            for (int i = 0; i < numPoints; ++i)
            {
                float angle = i * anglePerSegmentOfFov - halfFov;
                arcPoints[i] = AZ::Quaternion::CreateRotationZ(angle) * lightAxis;
            }
        }

        // Draw pyramid and sphere intersection.
        dc->DrawPolyLine(points, numPoints * 4, false);

        // Draw cross.
        dc->DrawPolyLine(points + numPoints * 4, numPoints, false);
        dc->DrawPolyLine(points + numPoints * 5, numPoints, false);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 0]);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 1]);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 2]);
        dc->DrawLine(AZ::Vector3::CreateZero(), points[numPoints * 3]);
    }

    void EditorLightComponent::DrawPlaneGizmo(AzFramework::EntityDebugDisplayRequests* dc, const float depth) const
    {
        //Don't draw if depth isn't going to result in anything visible
        if (depth <= 0)
        {
            return;
        }

        const uint32_t numPoints = 8; //8 Points - 4 corners and 4 half widths

        AZ::Vector3 points[numPoints];

        const float fov = DEG2RAD(m_configuration.m_projectorFOV);
        const float halfWidth = tanf(0.5f * fov) * depth;       //Calculate the half width of the frustum at this depth

        //Add corners
        points[0] = AZ::Vector3(depth, +halfWidth, +halfWidth); //Top-Left
        points[1] = AZ::Vector3(depth, -halfWidth, +halfWidth); //Top-Right
        points[2] = AZ::Vector3(depth, -halfWidth, -halfWidth); //Bottom-Right
        points[3] = AZ::Vector3(depth, +halfWidth, -halfWidth); //Bottom-Left

        //Add points halfway between corners
        points[4] = AZ::Vector3(depth, 0, +halfWidth);  //Top-Middle
        points[5] = AZ::Vector3(depth, -halfWidth, 0);  //Right-Middle
        points[6] = AZ::Vector3(depth, 0, -halfWidth);  //Bottom-Middle
        points[7] = AZ::Vector3(depth, +halfWidth, 0);  //Left-Middle


        //Draw Square
        dc->DrawLine(points[0], points[1]); //TL to TR
        dc->DrawLine(points[1], points[2]); //TR to BR
        dc->DrawLine(points[2], points[3]); //BR to BL
        dc->DrawLine(points[3], points[0]); //BL to TL

        const AZ::Vector3 depthVec(depth, 0, 0);

        // Draw Cross
        dc->DrawLine(depthVec, points[4]);
        dc->DrawLine(depthVec, points[5]);
        dc->DrawLine(depthVec, points[6]);
        dc->DrawLine(depthVec, points[7]);
    }

    EditorLightComponent::CubemapPreview::CubemapPreview() 
        : m_renderTransform(Matrix34::CreateIdentity())
        , m_statObj(nullptr)
    {
        m_dwRndFlags |= ERF_RENDER_ALWAYS;
    }

    void EditorLightComponent::CubemapPreview::Setup(const char* textureName)
    {
        if (!m_editor)
        {
            EBUS_EVENT_RESULT(m_editor, AzToolsFramework::EditorRequests::Bus, GetEditor);
        }

        if (!m_materialManager)
        {
            m_materialManager = m_editor->Get3DEngine()->GetMaterialManager();
        }
        
        _smart_ptr<IMaterial> material = m_materialManager->LoadMaterial("Editor/Objects/envcube", false, true);
        QString matName = Path::GetFileName(textureName);
        if (material)
        {
            SShaderItem& si = material->GetShaderItem();

            // We need to clone the material in order for multiple Environment Probes to not stomp each other's preview materials.
            material = m_materialManager->CreateMaterial(matName.toUtf8().data(), material->GetFlags() | MTL_FLAG_NON_REMOVABLE);
            if (material)
            {
                SInputShaderResources isr = si.m_pShaderResources;
                isr.m_Textures[EFTT_ENV].m_Name = textureName;

                SShaderItem siDst = m_editor->GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, &isr, si.m_pShader->GetGenerationMask());
                material->AssignShaderItem(siDst);
            }
        }

        m_statObj = m_editor->Get3DEngine()->LoadStatObjAutoRef("Editor/Objects/envcube.cgf", nullptr, nullptr, false);
        if (m_statObj)
        {
            // We need to clone the object in order for multiple Environment Probes to not stomp each other's preview materials.
            m_statObj = m_statObj->Clone(false, false, false);

            m_statObj->SetMaterial(material);
        }
    }

    void EditorLightComponent::CubemapPreview::UpdateTexture(const char* textureName)
    {
        if (m_statObj)
        {
            _smart_ptr<IMaterial> material = m_statObj->GetMaterial();

            if (material)
            {
                SShaderItem& si = material->GetShaderItem();

                SInputShaderResources isr = si.m_pShaderResources;
                isr.m_Textures[EFTT_ENV].m_Name = textureName;

                SShaderItem siDst = m_editor->GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, &isr, si.m_pShader->GetGenerationMask());
                material->AssignShaderItem(siDst);
            }
        }
    }

    void EditorLightComponent::CubemapPreview::SetTransform(const Matrix34& transform) 
    {
        m_renderTransform = transform; 
    }

    void EditorLightComponent::CubemapPreview::Render(const struct SRendParams& inRenderParams, const struct SRenderingPassInfo& passInfo)
    {
        if (m_statObj)
        {
            SRendParams rp;
            rp.AmbientColor = ColorF(1.0f, 1.0f, 1.0f, 1);
            rp.fAlpha = 1;
            rp.pMatrix = &m_renderTransform;
            rp.pMaterial = m_statObj->GetMaterial();

            m_statObj->Render(rp, passInfo);
        }
    }

    EERType EditorLightComponent::CubemapPreview::GetRenderNodeType()
    {
        return eERType_RenderComponent; 
    }

    const char* EditorLightComponent::CubemapPreview::GetName() const 
    { 
        return "CubemapPreview";
    }

    const char* EditorLightComponent::CubemapPreview::GetEntityClassName() const 
    { 
        return "CubemapPreview"; 
    }

    Vec3 EditorLightComponent::CubemapPreview::GetPos(bool bWorldOnly) const 
    {
        return m_renderTransform.GetTranslation(); 
    }

    const AABB EditorLightComponent::CubemapPreview::GetBBox() const
    {
        AABB transformedAABB;
        transformedAABB.Reset();
        if (m_statObj)
        {
            transformedAABB.SetTransformedAABB(QuatT(m_renderTransform), m_statObj->GetAABB());
        }
        return transformedAABB;
    }

    struct IPhysicalEntity* EditorLightComponent::CubemapPreview::GetPhysics() const 
    { 
        return nullptr; 
    }

    _smart_ptr<IMaterial> EditorLightComponent::CubemapPreview::GetMaterial(Vec3* pHitPos) 
    {
        return m_statObj ? m_statObj->GetMaterial() : nullptr;
    }

    _smart_ptr<IMaterial> EditorLightComponent::CubemapPreview::GetMaterialOverride() 
    {
        return m_statObj ? m_statObj->GetMaterial() : nullptr;
    }

    IStatObj* EditorLightComponent::CubemapPreview::GetEntityStatObj(unsigned int nPartId, unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
    {
        if (0 == nPartId)
        {
            if (pMatrix)
            {
                *pMatrix = m_renderTransform;
            }

            return m_statObj;
        }

        return nullptr;
    }

    float EditorLightComponent::CubemapPreview::GetMaxViewDist() 
    { 
        return FLT_MAX; 
    }

    void EditorLightComponent::CubemapPreview::GetMemoryUsage(class ICrySizer* pSizer) const
    {
        pSizer->AddObjectSize(this); 
    }



} // namespace LmbrCentral

