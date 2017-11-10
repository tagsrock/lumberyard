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

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            namespace Views
            {
                template<typename Iterator, typename Filter>
                SceneGraphChildIterator<Iterator, Filter>::SceneGraphChildIterator(
                    const SceneGraph& graph, SceneGraph::HierarchyStorageConstIterator graphIterator, Iterator iterator, bool rootIterator)
                    : m_index(-1)
                    , m_graph(nullptr)
                {
                    if (graph.GetHierarchyStorage().end() != graphIterator)
                    {
                        if (graphIterator->HasChild())
                        {
                            m_graph = &graph;
                            m_index = graphIterator->m_childIndex;
                            m_iterator = rootIterator ? AZStd::next(iterator, m_index) : iterator;

                            if (!ShouldAcceptNode(Filter()))
                            {
                                MoveToNext();
                            }
                        }
                    }
                }

                template<typename Iterator, typename Filter>
                SceneGraphChildIterator<Iterator, Filter>::SceneGraphChildIterator()
                    : m_graph(nullptr)
                    , m_index(-1)
                {
                }

                template<typename Iterator, typename Filter>
                auto SceneGraphChildIterator<Iterator, Filter>::operator++()->SceneGraphChildIterator &
                {
                    MoveToNext();
                    return *this;
                }

                template<typename Iterator, typename Filter>
                auto SceneGraphChildIterator<Iterator, Filter>::operator++(int)->SceneGraphChildIterator
                {
                    SceneGraphChildIterator result = *this;
                    MoveToNext();
                    return result;
                }

                template<typename Iterator, typename Filter>
                bool SceneGraphChildIterator<Iterator, Filter>::operator==(const SceneGraphChildIterator& rhs) const
                {
                    return m_graph == rhs.m_graph && m_index == rhs.m_index;
                }

                template<typename Iterator, typename Filter>
                bool SceneGraphChildIterator<Iterator, Filter>::operator!=(const SceneGraphChildIterator& rhs) const
                {
                    return m_graph != rhs.m_graph || m_index != rhs.m_index;
                }

                template<typename Iterator, typename Filter>
                auto SceneGraphChildIterator<Iterator, Filter>::operator*()->reference
                {
                    return *m_iterator;
                }

                template<typename Iterator, typename Filter>
                auto SceneGraphChildIterator<Iterator, Filter>::GetPointer(AZStd::true_type)->pointer
                {
                    return m_iterator;
                }

                template<typename Iterator, typename Filter>
                auto SceneGraphChildIterator<Iterator, Filter>::GetPointer(AZStd::false_type)->pointer
                {
                    return m_iterator.operator->();
                }

                template<typename Iterator, typename Filter>
                auto SceneGraphChildIterator<Iterator, Filter>::operator->()->pointer
                {
                    return GetPointer(typename AZStd::is_pointer<Iterator>::type());
                }

                template<typename Iterator, typename Filter>
                SceneGraph::HierarchyStorageConstIterator SceneGraphChildIterator<Iterator, Filter>::GetHierarchyIterator() const
                {
                    if (m_index >= 0)
                    {
                        return AZStd::next(m_graph->GetHierarchyStorage().begin(), m_index);
                    }
                    else
                    {
                        return SceneGraph::HierarchyStorageConstIterator();
                    }
                }

                template<typename Iterator, typename Filter>
                void SceneGraphChildIterator<Iterator, Filter>::MoveToNext()
                {
                    do
                    {
                        SceneGraph::NodeHeader header = m_graph->GetHierarchyStorage().begin()[m_index];
                        if (header.HasSibling())
                        {
                            AZStd::advance(m_iterator, header.m_siblingIndex - m_index);
                            m_index = header.m_siblingIndex;
                        }
                        else
                        {
                            m_index = -1;
                            m_graph = nullptr;
                            return;
                        }
                    } while (!ShouldAcceptNode(Filter()));
                }

                template<typename Iterator, typename Filter>
                bool SceneGraphChildIterator<Iterator, Filter>::ShouldAcceptNode(AcceptNodesOnly) const
                {
                    SceneGraph::NodeHeader header = m_graph->GetHierarchyStorage().begin()[m_index];
                    return !header.IsEndPoint();
                }

                template<typename Iterator, typename Filter>
                bool SceneGraphChildIterator<Iterator, Filter>::ShouldAcceptNode(AcceptEndPointsOnly) const
                {
                    SceneGraph::NodeHeader header = m_graph->GetHierarchyStorage().begin()[m_index];
                    return header.IsEndPoint();
                }

                template<typename Iterator, typename Filter>
                bool SceneGraphChildIterator<Iterator, Filter>::ShouldAcceptNode(AcceptAll) const
                {
                    return true;
                }

                template<typename Filter, typename Iterator>
                SceneGraphChildIterator<Iterator, Filter> MakeSceneGraphChildIterator(
                    const SceneGraph& graph, SceneGraph::HierarchyStorageConstIterator graphIterator, Iterator iterator, bool rootIterator)
                {
                    return SceneGraphChildIterator<Iterator, Filter>(graph, graphIterator, iterator, rootIterator);
                }

                template<typename Filter, typename Iterator>
                SceneGraphChildIterator<Iterator, Filter> MakeSceneGraphChildIterator(
                    const SceneGraph& graph, SceneGraph::NodeIndex node, Iterator iterator, bool rootIterator)
                {
                    return SceneGraphChildIterator<Iterator, Filter>(graph, graph.ConvertToHierarchyIterator(node), iterator, rootIterator);
                }

                template<typename Iterator>
                SceneGraphChildIterator<Iterator> MakeSceneGraphChildIterator(
                    const SceneGraph& graph, SceneGraph::HierarchyStorageConstIterator graphIterator, Iterator iterator, bool rootIterator)
                {
                    return SceneGraphChildIterator<Iterator>(graph, graphIterator, iterator, rootIterator);
                }

                template<typename Iterator>
                SceneGraphChildIterator<Iterator> MakeSceneGraphChildIterator(
                    const SceneGraph& graph, SceneGraph::NodeIndex node, Iterator iterator, bool rootIterator)
                {
                    return SceneGraphChildIterator<Iterator>(graph, graph.ConvertToHierarchyIterator(node), iterator, rootIterator);
                }

                template<typename Filter, typename Iterator>
                View<SceneGraphChildIterator<Iterator, Filter> > MakeSceneGraphChildView(
                    const SceneGraph& graph, SceneGraph::HierarchyStorageConstIterator graphIterator, Iterator iterator, bool rootIterator)
                {
                    return View<SceneGraphChildIterator<Iterator, Filter> >(
                        SceneGraphChildIterator<Iterator, Filter>(graph, graphIterator, iterator, rootIterator),
                        SceneGraphChildIterator<Iterator, Filter>());
                }

                template<typename Filter, typename Iterator>
                View<SceneGraphChildIterator<Iterator, Filter> > MakeSceneGraphChildView(
                    const SceneGraph& graph, SceneGraph::NodeIndex node, Iterator iterator, bool rootIterator)
                {
                    return View<SceneGraphChildIterator<Iterator, Filter> >(
                        SceneGraphChildIterator<Iterator, Filter>(graph, graph.ConvertToHierarchyIterator(node), iterator, rootIterator),
                        SceneGraphChildIterator<Iterator, Filter>());
                }

                template<typename Iterator>
                View<SceneGraphChildIterator<Iterator> > MakeSceneGraphChildView(
                    const SceneGraph& graph, SceneGraph::HierarchyStorageConstIterator graphIterator, Iterator iterator, bool rootIterator)
                {
                    return View<SceneGraphChildIterator<Iterator> >(
                        SceneGraphChildIterator<Iterator>(graph, graphIterator, iterator, rootIterator),
                        SceneGraphChildIterator<Iterator>());
                }

                template<typename Iterator>
                View<SceneGraphChildIterator<Iterator> > MakeSceneGraphChildView(
                    const SceneGraph& graph, SceneGraph::NodeIndex node, Iterator iterator, bool rootIterator)
                {
                    return View<SceneGraphChildIterator<Iterator> >(
                        SceneGraphChildIterator<Iterator>(graph, graph.ConvertToHierarchyIterator(node), iterator, rootIterator),
                        SceneGraphChildIterator<Iterator>());
                }
            } // Views
        } // Containers
    } // SceneAPI
} // AZ
