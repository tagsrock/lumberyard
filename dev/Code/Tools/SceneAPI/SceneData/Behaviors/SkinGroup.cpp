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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneGraph.h>
#include <SceneAPI/SceneCore/Containers/Utilities/Filters.h>
#include <SceneAPI/SceneCore/Containers/Views/FilterIterator.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphChildIterator.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/ISkinWeightData.h>
#include <SceneAPI/SceneCore/DataTypes/DataTypeUtilities.h>
#include <SceneAPI/SceneCore/Utilities/SceneGraphSelector.h>
#include <SceneAPI/SceneCore/Containers/Utilities/SceneGraphUtilities.h>
#include <SceneAPI/SceneData/Groups/SkinGroup.h>
#include <SceneAPI/SceneData/Behaviors/SkinGroup.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Behaviors
        {
            const char* SkinGroup::s_skinVirtualTypeName = "Skin";
            Crc32 SkinGroup::s_skinVirtualType = AZ_CRC(SkinGroup::s_skinVirtualTypeName, 0x0279681e);
            const int SkinGroup::s_rigsPreferredTabOrder = 1;

            void SkinGroup::Activate()
            {
                Events::ManifestMetaInfoBus::Handler::BusConnect();
                Events::GraphMetaInfoBus::Handler::BusConnect();
                Events::AssetImportRequestBus::Handler::BusConnect();
            }

            void SkinGroup::Deactivate()
            {
                Events::AssetImportRequestBus::Handler::BusDisconnect();
                Events::GraphMetaInfoBus::Handler::BusDisconnect();
                Events::ManifestMetaInfoBus::Handler::BusDisconnect();
            }

            void SkinGroup::Reflect(ReflectContext* context)
            {
                SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context);
                if (serializeContext)
                {
                    serializeContext->Class<SkinGroup, BehaviorComponent>()->Version(1);
                }
            }

            void SkinGroup::GetCategoryAssignments(CategoryRegistrationList& categories, const Containers::Scene& scene)
            {
                if (SceneHasSkinGroup(scene) || Utilities::DoesSceneGraphContainDataLike<DataTypes::ISkinWeightData>(scene, false))
                {
                    categories.emplace_back("Rigs", SceneData::SkinGroup::TYPEINFO_Uuid(), s_rigsPreferredTabOrder);
                }
            }

            void SkinGroup::InitializeObject(const Containers::Scene& scene, DataTypes::IManifestObject& target)
            {
                SceneData::SkinGroup* group = azrtti_cast<SceneData::SkinGroup*>(&target);
                if (!group)
                {
                    return;
                }
                
                group->SetName(DataTypes::Utilities::CreateUniqueName<DataTypes::ISkinGroup>(scene.GetName(), scene.GetManifest()));

                Utilities::SceneGraphSelector::UnselectAll(scene.GetGraph(), group->GetSceneNodeSelectionList());

                const Containers::SceneGraph& graph = scene.GetGraph();
                Containers::SceneGraph::ContentStorageConstData graphContent = graph.GetContentStorage();
                auto view = Containers::Views::MakeFilterView(graphContent, Containers::DerivedTypeFilter<DataTypes::IMeshData>());
                for (auto iter = view.begin(); iter != view.end(); ++iter)
                {
                    Containers::SceneGraph::NodeIndex nodeIndex = graph.ConvertToNodeIndex(iter.GetBaseIterator());
                    auto children = Containers::Views::MakeSceneGraphChildView(graph, nodeIndex, iter.GetBaseIterator(), false);
                    if (AZStd::find_if(children.begin(), children.end(),
                        Containers::DerivedTypeFilter<DataTypes::ISkinWeightData>()) != children.end())
                    {
                        group->GetSceneNodeSelectionList().AddSelectedNode(graph.GetNodeName(nodeIndex).GetPath());
                    }
                }

                Utilities::SceneGraphSelector::UpdateNodeSelection(scene.GetGraph(), group->GetSceneNodeSelectionList());
            }

            Events::ProcessingResult SkinGroup::UpdateManifest(Containers::Scene& scene, ManifestAction action,
                RequestingApplication /*requester*/)
            {
                if (action == ManifestAction::ConstructDefault)
                {
                    return BuildDefault(scene);
                }
                else if (action == ManifestAction::Update)
                {
                    return UpdateGroups(scene);
                }
                else
                {
                    return Events::ProcessingResult::Ignored;
                }
            }

            void SkinGroup::GetVirtualTypes(AZStd::set<Crc32>& types, const Containers::Scene& scene,
                Containers::SceneGraph::NodeIndex node)
            {
                if (types.find(s_skinVirtualType) != types.end())
                {
                    // Virtual type for skins has already been added.
                    return;
                }

                const Containers::SceneGraph& graph = scene.GetGraph();
                auto children = Containers::Views::MakeSceneGraphChildView(graph, node, graph.GetContentStorage().begin(), true);
                if (AZStd::find_if(children.begin(), children.end(), 
                    Containers::DerivedTypeFilter<DataTypes::ISkinWeightData>()) != children.end())
                {
                    types.insert(s_skinVirtualType);
                }
            }

            void SkinGroup::GetVirtualTypeName(AZStd::string& name, Crc32 type)
            {
                if (type == s_skinVirtualType)
                {
                    name = s_skinVirtualTypeName;
                }
            }

            Events::ProcessingResult SkinGroup::BuildDefault(Containers::Scene& scene) const
            {
                if (SceneHasSkinGroup(scene) || !Utilities::DoesSceneGraphContainDataLike<DataTypes::ISkinWeightData>(scene, true))
                {
                    return Events::ProcessingResult::Ignored;
                }

                // There are skins but no skin group, so add a default skin group to the manifest.
                AZStd::shared_ptr<SceneData::SkinGroup> group = AZStd::make_shared<SceneData::SkinGroup>();
                EBUS_EVENT(Events::ManifestMetaInfoBus, InitializeObject, scene, *group);
                scene.GetManifest().AddEntry(AZStd::move(group));

                return Events::ProcessingResult::Success;
            }

            Events::ProcessingResult SkinGroup::UpdateGroups(Containers::Scene& scene) const
            {
                bool updated = false;
                Containers::SceneManifest& manifest = scene.GetManifest();
                auto valueStorage = manifest.GetValueStorage();                
                auto view = Containers::MakeDerivedFilterView<SceneData::SkinGroup>(valueStorage);
                for (SceneData::SkinGroup& group : view)
                {
                    if (group.GetName().empty())
                    {
                        group.SetName(DataTypes::Utilities::CreateUniqueName<DataTypes::ISkinGroup>(scene.GetName(), scene.GetManifest()));
                    }
                    Utilities::SceneGraphSelector::UpdateNodeSelection(scene.GetGraph(), group.GetSceneNodeSelectionList());
                    updated = true;
                }
                return updated ? Events::ProcessingResult::Success : Events::ProcessingResult::Ignored;
            }

            bool SkinGroup::SceneHasSkinGroup(const Containers::Scene& scene) const
            {
                const Containers::SceneManifest& manifest = scene.GetManifest();
                Containers::SceneManifest::ValueStorageConstData manifestData = manifest.GetValueStorage();
                auto skinGroup = AZStd::find_if(manifestData.begin(), manifestData.end(), Containers::DerivedTypeFilter<DataTypes::ISkinGroup>());
                return skinGroup != manifestData.end();
            }
        } // namespace Behaviors
    } // namespace SceneAPI
} // namespace AZ
