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

#include <algorithm>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <SceneAPI/SceneData/Groups/MeshGroup.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>
#include <SceneAPI/SceneUI/RowWidgets/ManifestVectorHandler.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            const AZStd::string& MeshGroup::GetName() const
            {
                return m_name;
            }

            void MeshGroup::SetName(const AZStd::string& name)
            {
                m_name = name;
            }

            void MeshGroup::SetName(AZStd::string&& name)
            {
                m_name = AZStd::move(name);
            }

            Containers::RuleContainer& MeshGroup::GetRuleContainer()
            {
                return m_rules;
            }

            const Containers::RuleContainer& MeshGroup::GetRuleContainerConst() const
            {
                return m_rules;
            }
            
            DataTypes::ISceneNodeSelectionList& MeshGroup::GetSceneNodeSelectionList()
            {
                return m_nodeSelectionList;
            }

            const DataTypes::ISceneNodeSelectionList& MeshGroup::GetSceneNodeSelectionList() const
            {
                return m_nodeSelectionList;
            }

            void MeshGroup::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (!serializeContext)
                {
                    return;
                }

                serializeContext->Class<MeshGroup, DataTypes::IMeshGroup>()
                    ->Version(2, VersionConverter)
                    ->Field("name", &MeshGroup::m_name)
                    ->Field("nodeSelectionList", &MeshGroup::m_nodeSelectionList)
                    ->Field("rules", &MeshGroup::m_rules);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<MeshGroup>("Mesh group", "Name and configure 1 or more meshes from your source file.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(AZ_CRC("ManifestName", 0x5215b349), &MeshGroup::m_name, "Name mesh",
                            "Name the mesh as you want it to appear in the Lumberyard Asset Browser.")
                            ->Attribute("FilterType", DataTypes::IMeshGroup::TYPEINFO_Uuid())
                        ->DataElement(Edit::UIHandlers::Default, &MeshGroup::m_nodeSelectionList, "Select meshes", "Select 1 or more meshes to add to this asset in the Lumberyard Asset Browser.")
                            ->Attribute("FilterName", "meshes")
                            ->Attribute("FilterType", DataTypes::IMeshData::TYPEINFO_Uuid())
                        ->DataElement(Edit::UIHandlers::Default, &MeshGroup::m_rules, "", "Add or remove rules to fine-tune the export process.")
                            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20));
                }
            }


            bool MeshGroup::VersionConverter(SerializeContext& context, SerializeContext::DataElementNode& classElement)
            {
                const unsigned int version = classElement.GetVersion();

                // Replaced vector<IRule> with RuleContainer.
                if (version == 1)
                {
                    return Containers::RuleContainer::VectorToRuleContainerConverter(context, classElement);
                }

                return true;
            }

        } // namespace SceneData
    } // namespace SceneAPI
} // namespace AZ