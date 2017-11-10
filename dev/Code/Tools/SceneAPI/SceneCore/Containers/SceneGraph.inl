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

#include <AzCore/std/iterator.h>
#include <AzCore/Casting/numeric_cast.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            //
            // SceneGraph NodeHeader
            //
            SceneGraph::NodeHeader::NodeHeader()
                : m_isEndPoint(0)
                , m_parentIndex(NodeHeader::INVALID_INDEX)
                , m_siblingIndex(NodeHeader::INVALID_INDEX)
                , m_childIndex(NodeHeader::INVALID_INDEX)
            {
            }

            bool SceneGraph::NodeHeader::HasParent() const
            {
                return m_parentIndex != NodeHeader::INVALID_INDEX;
            }

            bool SceneGraph::NodeHeader::HasSibling() const
            {
                return m_siblingIndex != NodeHeader::INVALID_INDEX;
            }

            bool SceneGraph::NodeHeader::HasChild() const
            {
                return m_childIndex != NodeHeader::INVALID_INDEX;
            }

            bool SceneGraph::NodeHeader::IsEndPoint() const
            {
                return m_isEndPoint;
            }

            //
            // NodeIndex
            //
            bool SceneGraph::NodeIndex::IsValid() const
            {
                return m_value != NodeIndex::INVALID_INDEX;
            }

            bool SceneGraph::NodeIndex::operator==(NodeIndex rhs) const
            {
                return m_value == rhs.m_value;
            }

            bool SceneGraph::NodeIndex::operator!=(NodeIndex rhs) const
            {
                return m_value != rhs.m_value;
            }

            SceneGraph::NodeIndex::IndexType SceneGraph::NodeIndex::AsNumber() const
            {
                return m_value;
            }

            SceneGraph::NodeIndex::NodeIndex()
                : m_value(NodeIndex::INVALID_INDEX)
            {
            }

            SceneGraph::NodeIndex::NodeIndex(IndexType value)
                : m_value(value)
            {
            }

            //
            // Name
            //
            SceneGraph::Name::Name()
                : m_nameOffset(0)
            {
            }

            SceneGraph::Name::Name(Name&& rhs)
                : m_path(AZStd::move(rhs.m_path))
                , m_nameOffset(rhs.m_nameOffset)
            {
            }

            SceneGraph::Name::Name(AZStd::string&& pathName, size_t nameOffset)
                : m_path(AZStd::move(pathName))
                , m_nameOffset(nameOffset)
            {
                if (m_nameOffset >= m_path.size())
                {
                    m_nameOffset = m_path.size();
                }
            }

            SceneGraph::Name& SceneGraph::Name::operator=(Name&& rhs)
            {
                m_path = AZStd::move(rhs.m_path);
                m_nameOffset = rhs.m_nameOffset;
                return *this;
            }

            bool SceneGraph::Name::operator==(const Name& rhs) const
            {
                return m_nameOffset == rhs.m_nameOffset && m_path == rhs.m_path;
            }

            bool SceneGraph::Name::operator!=(const Name& rhs) const
            {
                return m_nameOffset != rhs.m_nameOffset || m_path != rhs.m_path;
            }

            const char* SceneGraph::Name::GetPath() const
            {
                return m_path.c_str();
            }

            const char* SceneGraph::Name::GetName() const
            {
                AZ_Assert(m_nameOffset <= m_path.length(), "Offset to name in SceneGraph path is invalid.");
                return m_path.c_str() + m_nameOffset;
            }

            size_t SceneGraph::Name::GetPathLength() const
            {
                return m_path.length();
            }

            size_t SceneGraph::Name::GetNameLength() const
            {
                AZ_Assert(m_nameOffset <= m_path.length(), "Offset to name in SceneGraph path is invalid.");
                return m_path.length() - m_nameOffset;
            }

            //
            // SceneGraph
            //
            AZStd::shared_ptr<const DataTypes::IGraphObject> SceneGraph::ConstDataConverter(const AZStd::shared_ptr<DataTypes::IGraphObject>& value)
            {
                return AZStd::shared_ptr<const DataTypes::IGraphObject>(value);
            }

            SceneGraph::NodeIndex SceneGraph::GetRoot() const
            {
                return NodeIndex(0);
            }

            SceneGraph::NodeIndex SceneGraph::Find(const AZStd::string& path) const
            {
                return Find(path.c_str());
            }

            SceneGraph::NodeIndex SceneGraph::Find(const Name& name)
            {
                return Find(name.GetPath());
            }

            SceneGraph::NodeIndex SceneGraph::Find(NodeIndex root, const AZStd::string& name) const
            {
                return Find(root, name.c_str());
            }

            bool SceneGraph::HasNodeContent(NodeIndex node) const
            {
                return node.m_value < m_content.size() ? m_content[node.m_value] != nullptr : false;
            }

            bool SceneGraph::HasNodeSibling(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].HasSibling() : false;
            }

            bool SceneGraph::HasNodeChild(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].HasChild() : false;
            }

            bool SceneGraph::HasNodeParent(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].HasParent() : false;
            }

            bool SceneGraph::IsNodeEndPoint(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy[node.m_value].IsEndPoint() : true;
            }

            AZStd::shared_ptr<DataTypes::IGraphObject> SceneGraph::GetNodeContent(NodeIndex node)
            {
                return node.m_value < m_content.size() ? m_content[node.m_value] : nullptr;
            }

            AZStd::shared_ptr<const DataTypes::IGraphObject> SceneGraph::GetNodeContent(NodeIndex node) const
            {
                return node.m_value < m_content.size() ? m_content[node.m_value] : nullptr;
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeParent(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? GetNodeParent(m_hierarchy[node.m_value]) : NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeParent(NodeHeader node) const
            {
                return NodeIndex(node.m_parentIndex != NodeHeader::INVALID_INDEX ? static_cast<uint32_t>(node.m_parentIndex) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeSibling(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? GetNodeSibling(m_hierarchy[node.m_value]) : NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeSibling(NodeHeader node) const
            {
                return NodeIndex(node.m_siblingIndex != NodeHeader::INVALID_INDEX ? static_cast<uint32_t>(node.m_siblingIndex) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeChild(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? GetNodeChild(m_hierarchy[node.m_value]) : NodeIndex();
            }

            SceneGraph::NodeIndex SceneGraph::GetNodeChild(NodeHeader node) const
            {
                return NodeIndex(node.m_childIndex != NodeHeader::INVALID_INDEX ? static_cast<uint32_t>(node.m_childIndex) : NodeIndex::INVALID_INDEX);
            }

            size_t SceneGraph::GetNodeCount() const
            {
                return m_hierarchy.size();
            }

            SceneGraph::HierarchyStorageConstData::iterator SceneGraph::ConvertToHierarchyIterator(NodeIndex node) const
            {
                return node.m_value < m_hierarchy.size() ? m_hierarchy.cbegin() + node.m_value : m_hierarchy.cend();
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(HierarchyStorageConstData::iterator iterator) const
            {
                return NodeIndex(iterator != m_hierarchy.cend() ? static_cast<uint32_t>(std::distance(m_hierarchy.cbegin(), iterator)) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(NameStorageConstData::iterator iterator) const
            {
                return NodeIndex(iterator != m_names.cend() ? static_cast<uint32_t>(std::distance(m_names.cbegin(), iterator)) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(ContentStorageData::iterator iterator) const
            {
                return NodeIndex(iterator != m_content.end() ? static_cast<uint32_t>(std::distance(m_content.begin(), iterator)) : NodeIndex::INVALID_INDEX);
            }

            SceneGraph::NodeIndex SceneGraph::ConvertToNodeIndex(ContentStorageConstData::iterator iterator) const
            {
                return NodeIndex(
                    iterator.GetBaseIterator() != m_content.cend() ?
                    aznumeric_caster(AZStd::distance(m_content.cbegin(), iterator.GetBaseIterator())) :
                    NodeIndex::INVALID_INDEX);
            }

            SceneGraph::HierarchyStorageConstData SceneGraph::GetHierarchyStorage() const
            {
                return HierarchyStorageConstData(m_hierarchy.begin(), m_hierarchy.end());
            }

            SceneGraph::NameStorageConstData SceneGraph::GetNameStorage() const
            {
                return NameStorageConstData(m_names.begin(), m_names.end());
            }

            SceneGraph::ContentStorageData SceneGraph::GetContentStorage()
            {
                return ContentStorageData(m_content.begin(), m_content.end());
            }

            SceneGraph::ContentStorageConstData SceneGraph::GetContentStorage() const
            {
                return ContentStorageConstData(
                    Views::MakeConvertIterator(m_content.cbegin(), ConstDataConverter),
                    Views::MakeConvertIterator(m_content.cend(), ConstDataConverter));
            }

            bool SceneGraph::IsValidName(const AZStd::string& name)
            {
                return name.size() > 0 ? IsValidName(name.c_str()) : false;
            }
        } // Containers
    } // SceneAPI
} // AZ
