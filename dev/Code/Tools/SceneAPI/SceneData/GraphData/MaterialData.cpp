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

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <SceneAPI/SceneData/GraphData/MaterialData.h>

namespace AZ
{
    namespace SceneData
    {
        namespace GraphData
        {
            namespace DataTypes = AZ::SceneAPI::DataTypes;

            const AZStd::string MaterialData::s_DiffuseMapName = "Diffuse";
            const AZStd::string MaterialData::s_SpecularMapName = "Specular";
            const AZStd::string MaterialData::s_BumpMapName = "Bump";
            const AZStd::string MaterialData::s_emptyString = "";

            MaterialData::MaterialData()
                : m_isNoDraw(false)
                , m_diffuseColor(AZ::Vector3::CreateOne())
                , m_specularColor(AZ::Vector3::CreateZero())
                , m_emissiveColor(AZ::Vector3::CreateZero())
                , m_opacity(1.f)
                , m_shininess(10.f)
            {
            }

            void MaterialData::SetTexture(TextureMapType mapType, const char* textureFileName)
            {
                if (textureFileName)
                {
                    SetTexture(mapType, AZStd::string(textureFileName));
                }
            }

            void MaterialData::SetTexture(TextureMapType mapType, const AZStd::string& textureFileName)
            {
                SetTexture(mapType, AZStd::string(textureFileName));
            }

            void MaterialData::SetTexture(TextureMapType mapType, AZStd::string&& textureFileName)
            {
                if (!textureFileName.empty())
                {
                    m_textureMap[mapType] = AZStd::move(textureFileName);
                }
            }

            const AZStd::string& MaterialData::GetTexture(TextureMapType mapType) const
            {
                auto result = m_textureMap.find(mapType);
                if (result != m_textureMap.end())
                {
                    return result->second;
                }

                return s_emptyString;
            }

            void MaterialData::SetNoDraw(bool isNoDraw)
            {
                m_isNoDraw = isNoDraw;
            }

            bool MaterialData::IsNoDraw() const
            {
                return m_isNoDraw;
            }

            void MaterialData::SetDiffuseColor(const AZ::Vector3& color)
            {
                m_diffuseColor = color;
            }

            const AZ::Vector3& MaterialData:: GetDiffuseColor() const
            {
                return m_diffuseColor;
            }
            
            void MaterialData::SetSpecularColor(const AZ::Vector3& color)
            {
                m_specularColor = color;
            }

            const AZ::Vector3& MaterialData::GetSpecularColor() const
            {
                return m_specularColor;
            }

            void MaterialData::SetEmissiveColor(const AZ::Vector3& color)
            {
                m_emissiveColor = color;
            }

            const AZ::Vector3& MaterialData::GetEmissiveColor() const
            {
                return m_emissiveColor;
            }
            
            void MaterialData::SetOpacity(float opacity)
            {
                m_opacity = opacity;
            }

            float MaterialData::GetOpacity() const
            {
                return m_opacity;
            }
            
            void MaterialData::SetShininess(float shininess)
            {
                m_shininess = shininess;
            }

            float MaterialData::GetShininess() const
            {
                return m_shininess;
            }

            void MaterialData::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);

                serializeContext->Class<MaterialData>()->Version(1)
                    ->Field("textureMap", &MaterialData::m_textureMap)
                    ->Field("diffuseColor", &MaterialData::m_diffuseColor)
                    ->Field("specularColor", &MaterialData::m_specularColor)
                    ->Field("emissiveColor", &MaterialData::m_emissiveColor)
                    ->Field("opacity", &MaterialData::m_opacity)
                    ->Field("shininess", &MaterialData::m_shininess)
                    ->Field("noDraw", &MaterialData::m_isNoDraw);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<MaterialData>("Materials", "Material configuration for the parent.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_diffuseColor, "Diffuse", "Diffuse color component of the material.")
                            ->Attribute(Edit::Attributes::StyleForX, "font: bold; color: rgb(184,51,51);")
                            ->Attribute(Edit::Attributes::StyleForY, "font: bold; color: rgb(48,208,120);")
                            ->Attribute(Edit::Attributes::StyleForZ, "font: bold; color: rgb(66,133,244);")
                            ->Attribute(Edit::Attributes::LabelForX, "R")
                            ->Attribute(Edit::Attributes::LabelForY, "G")
                            ->Attribute(Edit::Attributes::LabelForZ, "B")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_specularColor, "Specular", "Specular color component of the material.")
                            ->Attribute(Edit::Attributes::StyleForX, "font: bold; color: rgb(184,51,51);")
                            ->Attribute(Edit::Attributes::StyleForY, "font: bold; color: rgb(48,208,120);")
                            ->Attribute(Edit::Attributes::StyleForZ, "font: bold; color: rgb(66,133,244);")
                            ->Attribute(Edit::Attributes::LabelForX, "R")
                            ->Attribute(Edit::Attributes::LabelForY, "G")
                            ->Attribute(Edit::Attributes::LabelForZ, "B")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_emissiveColor, "Emissive", "Emissive color component of the material.")
                            ->Attribute(Edit::Attributes::StyleForX, "font: bold; color: rgb(184,51,51);")
                            ->Attribute(Edit::Attributes::StyleForY, "font: bold; color: rgb(48,208,120);")
                            ->Attribute(Edit::Attributes::StyleForZ, "font: bold; color: rgb(66,133,244);")
                            ->Attribute(Edit::Attributes::LabelForX, "R")
                            ->Attribute(Edit::Attributes::LabelForY, "G")
                            ->Attribute(Edit::Attributes::LabelForZ, "B")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_opacity, "Opacity", "Opacity strength of the material, with 0 fully transparent and 1 fully opaque.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_shininess, "Shininess", "The shininess strength of the material.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_isNoDraw, "No draw", "If enabled the mesh with material will not be drawn.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &MaterialData::m_textureMap, "Texture map", "List of assigned texture slots.");
                }
            }
        } // namespace GraphData
    } // namespace SceneData
} // namespace AZ
