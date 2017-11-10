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

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <SceneAPI/SceneData/Rules/SkinMeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexUVData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexColorData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(SkinMeshAdvancedRule, SystemAllocator, 0)

            SkinMeshAdvancedRule::SkinMeshAdvancedRule()
                : m_use32bitVertices(false)
            {
            }

            void SkinMeshAdvancedRule::SetUse32bitVertices(bool value)
            {
                m_use32bitVertices = value;
            }

            bool SkinMeshAdvancedRule::Use32bitVertices() const
            {
                return m_use32bitVertices;
            }

            bool SkinMeshAdvancedRule::MergeMeshes() const
            {
                return true;
            }

            void SkinMeshAdvancedRule::SetVertexColorStreamName(const AZStd::string& name)
            {
                m_vertexColorStreamName = name;
            }

            void SkinMeshAdvancedRule::SetVertexColorStreamName(AZStd::string&& name)
            {
                m_vertexColorStreamName = AZStd::move(name);
            }

            const AZStd::string& SkinMeshAdvancedRule::GetVertexColorStreamName() const
            {
                return m_vertexColorStreamName;
            }

            bool SkinMeshAdvancedRule::IsVertexColorStreamDisabled() const
            {
                return m_vertexColorStreamName == DataTypes::s_advancedDisabledString;
            }

            void SkinMeshAdvancedRule::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext)
                {
                    return;
                }

                serializeContext->Class<SkinMeshAdvancedRule, DataTypes::IMeshAdvancedRule>()->Version(5)
                    ->Field("use32bitVertices", &SkinMeshAdvancedRule::m_use32bitVertices)
                    ->Field("vertexColorStreamName", &SkinMeshAdvancedRule::m_vertexColorStreamName);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<SkinMeshAdvancedRule>("Skin (Advanced)", "Configure advanced properties for this skin group.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(Edit::UIHandlers::Default, &SkinMeshAdvancedRule::m_use32bitVertices, "32-bit Vertex Precision",
                            "Activating will use 32-bits of precision for the position of each vertex, increasing accuracy when the skin is located far from its pivot.\n\n"
                            "Note that Sony Playstation platforms only supports 16-bit precision. For more details please see documentation.")
                        ->DataElement("NodeListSelection", &SkinMeshAdvancedRule::m_vertexColorStreamName, "Vertex Color Stream",
                            "Select a vertex color stream to enable Vertex Coloring or 'Disable' to turn Vertex Coloring off.\n\n"
                            "Vertex Coloring works in conjunction with materials. If a material was previously generated,\n"
                            "changing vertex coloring will require the material to be reset or the material editor to be used\n"
                            "to enable 'Vertex Coloring'.")
                            ->Attribute("ClassTypeIdFilter", DataTypes::IMeshVertexColorData::TYPEINFO_Uuid())
                            ->Attribute("DisabledOption", DataTypes::s_advancedDisabledString)
                            ->Attribute("UseShortNames", true);                            
                }
            }
        } // SceneData
    } // SceneAPI
} // AZ
