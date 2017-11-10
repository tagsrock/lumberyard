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
#include <SceneAPI/SceneData/Rules/StaticMeshAdvancedRule.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexUVData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshVertexColorData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(StaticMeshAdvancedRule, SystemAllocator, 0)

            StaticMeshAdvancedRule::StaticMeshAdvancedRule()
                : m_use32bitVertices(false)
                , m_mergeMeshes(true)
            {
            }

            void StaticMeshAdvancedRule::SetUse32bitVertices(bool value)
            {
                m_use32bitVertices = value;
            }

            bool StaticMeshAdvancedRule::Use32bitVertices() const
            {
                return m_use32bitVertices;
            }

            void StaticMeshAdvancedRule::SetMergeMeshes(bool value)
            {
                m_mergeMeshes = value;
            }

            bool StaticMeshAdvancedRule::MergeMeshes() const
            {
                return m_mergeMeshes;
            }

            void StaticMeshAdvancedRule::SetVertexColorStreamName(const AZStd::string& name)
            {
                m_vertexColorStreamName = name;
            }

            void StaticMeshAdvancedRule::SetVertexColorStreamName(AZStd::string&& name)
            {
                m_vertexColorStreamName = AZStd::move(name);
            }

            const AZStd::string& StaticMeshAdvancedRule::GetVertexColorStreamName() const
            {
                return m_vertexColorStreamName;
            }

            bool StaticMeshAdvancedRule::IsVertexColorStreamDisabled() const
            {
                return m_vertexColorStreamName == DataTypes::s_advancedDisabledString;
            }


            void StaticMeshAdvancedRule::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext)
                {
                    return;
                }

                serializeContext->Class<StaticMeshAdvancedRule, DataTypes::IMeshAdvancedRule>()->Version(5)
                    ->Field("use32bitVertices", &StaticMeshAdvancedRule::m_use32bitVertices)
                    ->Field("mergeMeshes", &StaticMeshAdvancedRule::m_mergeMeshes)
                    ->Field("vertexColorStreamName", &StaticMeshAdvancedRule::m_vertexColorStreamName);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<StaticMeshAdvancedRule>("Mesh (Advanced)", "Configure advanced properties for this mesh group.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(Edit::UIHandlers::Default, &StaticMeshAdvancedRule::m_use32bitVertices, "32-bit Vertex Precision",
                            "Activating will use 32-bits of precision for the position of each vertex, increasing accuracy when the mesh is located far from its pivot.\n\n"
                            "Note that Sony Playstation platforms only supports 16-bit precision. For more details please see documentation.")
                        ->DataElement(Edit::UIHandlers::Default, &StaticMeshAdvancedRule::m_mergeMeshes, "Merge Meshes", "Merge all meshes into one single mesh.")
                        ->DataElement("NodeListSelection", &StaticMeshAdvancedRule::m_vertexColorStreamName, "Vertex Color Stream",
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
