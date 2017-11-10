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

#include <SceneAPI/SceneData/Groups/SkinGroup.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphChildIterator.h>
#include <SceneAPI/SceneCore/Containers/Utilities/Filters.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/ISkinWeightData.h>
#include <SceneAPI/SceneData/Behaviors/SkinGroup.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            AZ_CLASS_ALLOCATOR_IMPL(SkinGroup, AZ::SystemAllocator, 0)

            const AZStd::string& SkinGroup::GetName() const
            {
                return m_name;
            }

            void SkinGroup::SetName(const AZStd::string& name)
            {
                m_name = name;
            }

            void SkinGroup::SetName(AZStd::string&& name)
            {
                m_name = AZStd::move(name);
            }

            Containers::RuleContainer& SkinGroup::GetRuleContainer()
            {
                return m_rules;
            }

            const Containers::RuleContainer& SkinGroup::GetRuleContainerConst() const
            {
                return m_rules;
            }

            DataTypes::ISceneNodeSelectionList& SkinGroup::GetSceneNodeSelectionList()
            {
                return m_nodeSelectionList; 
            }

            const DataTypes::ISceneNodeSelectionList& SkinGroup::GetSceneNodeSelectionList() const 
            {
                return m_nodeSelectionList; 
            }

            void SkinGroup::Reflect(AZ::ReflectContext* context)
            {
                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (!serializeContext)
                {
                    return;
                }

                serializeContext->Class<SkinGroup, DataTypes::ISkinGroup>()->Version(2, VersionConverter)
                    ->Field("name", &SkinGroup::m_name)
                    ->Field("nodeSelectionList", &SkinGroup::m_nodeSelectionList)
                    ->Field("rules", &SkinGroup::m_rules);

                EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<SkinGroup>("Skin group", "Name and configure 1 or more skins from your source file.")
                        ->ClassElement(Edit::ClassElements::EditorData, "")
                            ->Attribute("AutoExpand", true)
                            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(AZ_CRC("ManifestName", 0x5215b349), &SkinGroup::m_name, "Name skin",
                            "Name the skin as you want it to appear in the Lumberyard Asset Browser.")
                            ->Attribute("FilterType", DataTypes::ISkinGroup::TYPEINFO_Uuid())
                        ->DataElement(AZ_CRC("ManifestName", 0x5215b349), &SkinGroup::m_nodeSelectionList, "Select skins", "Select 1 or more skins to add to this asset in the Lumberyard Asset Browser.")
                            ->Attribute("FilterName", "skins")
                            ->Attribute("FilterVirtualType", Behaviors::SkinGroup::s_skinVirtualType)
                        ->DataElement(Edit::UIHandlers::Default, &SkinGroup::m_rules, "", "Add or remove rules to fine-tune the export process.")
                            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20));
                }
            }


            bool SkinGroup::VersionConverter(SerializeContext& context, SerializeContext::DataElementNode& classElement)
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