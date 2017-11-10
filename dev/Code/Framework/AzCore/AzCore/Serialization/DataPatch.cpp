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
#ifndef AZ_UNITY_BUILD

#include <AzCore/Serialization/DataPatch.h>
#include <AzCore/Serialization/Utils.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/Math/MathUtils.h>

namespace AZ
{
    class DataNode
    {
    public:
        typedef AZStd::list<DataNode>   ChildDataNodes;

        DataNode()
        {
            Reset();
        }

        void Reset()
        {
            m_data = nullptr;
            m_parent = nullptr;
            m_children.clear();
            m_classData = nullptr;
            m_classElement = nullptr;
        }

        void*           m_data;
        DataNode*       m_parent;
        ChildDataNodes  m_children;

        const SerializeContext::ClassData*      m_classData;
        const SerializeContext::ClassElement*   m_classElement;
    };

    class DataNodeTree
    {
    public:
        DataNodeTree(SerializeContext* context)
            : m_currentNode(nullptr)
            , m_context(context)
        {}

        void Build(const void* classPtr, const Uuid& classId);

        bool BeginNode(void* ptr, const SerializeContext::ClassData* classData, const SerializeContext::ClassElement* classElement);
        bool EndNode();

        /// Compare two nodes and fill the patch structure
        static void CompareElements(const DataNode* sourceNode, const DataNode* targetNode, DataPatch::PatchMap& patch, const DataPatch::FlagsMap& patchFlags, SerializeContext* context);
        static void CompareElementsInternal(const DataNode* sourceNode, const DataNode* targetNode, DataPatch::PatchMap& patch, const DataPatch::FlagsMap& patchFlags, SerializeContext* context, DataPatch::AddressType& address, DataPatch::Flags& addressFlags, AZStd::vector<AZ::u8>& tmpSourceBuffer);

        /// Apply patch to elements, return a valid pointer only for the root element
        static void* ApplyToElements(DataNode* sourceNode, const DataPatch::PatchMap& patch, DataPatch::AddressType& address, void* parentPointer, const SerializeContext::ClassData* parentClassData, AZStd::vector<AZ::u8>& tmpSourceBuffer, SerializeContext* context, const AZ::ObjectStream::FilterDescriptor& filterDesc);

        DataNode m_root;
        DataNode* m_currentNode;        ///< Used as temp during tree building
        SerializeContext* m_context;
        AZStd::list<SerializeContext::ClassElement> m_dynamicClassElements; ///< Storage for class elements that represent dynamic serializable fields.
    };

    //=========================================================================
    // DataNodeTree::Build
    //=========================================================================
    void DataNodeTree::Build(const void* rootClassPtr, const Uuid& rootClassId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        m_root.Reset();
        m_currentNode = nullptr;

        if (m_context && rootClassPtr)
        {
            m_context->EnumerateInstanceConst(
                rootClassPtr,
                rootClassId,
                AZStd::bind(&DataNodeTree::BeginNode, this, AZStd::placeholders::_1, AZStd::placeholders::_2, AZStd::placeholders::_3),
                AZStd::bind(&DataNodeTree::EndNode, this),
                SerializeContext::ENUM_ACCESS_FOR_READ,
                nullptr,
                nullptr
                );
        }

        m_currentNode = nullptr;
    }

    //=========================================================================
    // DataNodeTree::BeginNode
    //=========================================================================
    bool DataNodeTree::BeginNode(void* ptr, const SerializeContext::ClassData* classData, const SerializeContext::ClassElement* classElement)
    {
        DataNode* newNode;
        if (m_currentNode)
        {
            m_currentNode->m_children.push_back();
            newNode = &m_currentNode->m_children.back();
        }
        else
        {
            newNode = &m_root;
        }

        newNode->m_parent = m_currentNode;
        newNode->m_classData = classData;

        // ClassElement pointers for DynamicSerializableFields are temporaries, so we need
        // to maintain it locally.
        if (classElement)
        {
            if (classElement->m_flags & SerializeContext::ClassElement::FLG_DYNAMIC_FIELD)
            {
                m_dynamicClassElements.push_back(*classElement);
                classElement = &m_dynamicClassElements.back();
            }
        }

        newNode->m_classElement = classElement;

        if (classElement && (classElement->m_flags & SerializeContext::ClassElement::FLG_POINTER))
        {
            // we always store the value address
            newNode->m_data = *(void**)(ptr);
        }
        else
        {
            newNode->m_data = ptr;
        }

        if (classData->m_eventHandler)
        {
            classData->m_eventHandler->OnReadBegin(newNode->m_data);
        }

        m_currentNode = newNode;
        return true;
    }

    //=========================================================================
    // DataNodeTree::EndNode
    //=========================================================================
    bool DataNodeTree::EndNode()
    {
        if (m_currentNode->m_classData->m_eventHandler)
        {
            m_currentNode->m_classData->m_eventHandler->OnReadEnd(m_currentNode->m_data);
        }

        m_currentNode = m_currentNode->m_parent;
        return true;
    }

    //=========================================================================
    // DataNodeTree::CompareElements
    //=========================================================================
    void DataNodeTree::CompareElements(const DataNode* sourceNode, const DataNode* targetNode, DataPatch::PatchMap& patch, const DataPatch::FlagsMap& patchFlags, SerializeContext* context)
    {
        DataPatch::AddressType tmpAddress;
        DataPatch::Flags tmpAddressFlags = 0;
        AZStd::vector<AZ::u8> tmpSourceBuffer;

        CompareElementsInternal(sourceNode, targetNode, patch, patchFlags, context, tmpAddress, tmpAddressFlags, tmpSourceBuffer);
    }

    //=========================================================================
    // DataNodeTree::CompareElementsInternal
    //=========================================================================
    void DataNodeTree::CompareElementsInternal(const DataNode* sourceNode, const DataNode* targetNode, DataPatch::PatchMap& patch, const DataPatch::FlagsMap& patchFlags, SerializeContext* context, DataPatch::AddressType& address, DataPatch::Flags& addressFlags, AZStd::vector<AZ::u8>& tmpSourceBuffer)
    {
        if (targetNode->m_classData->m_typeId == sourceNode->m_classData->m_typeId)
        {
            if (targetNode->m_classData->m_container)
            {
                // find elements that we have added or modified
                u64 elementIndex = 0;
                u64 elementId = 0;
                for (const DataNode& targetElementNode : targetNode->m_children)
                {
                    const DataNode* sourceNodeMatch = nullptr;
                    SerializeContext::ClassPersistentId targetPersistentIdFunction = targetElementNode.m_classData->GetPersistentId(*context);
                    if (targetPersistentIdFunction)
                    {
                        u64 targetElementId = targetPersistentIdFunction(targetElementNode.m_data);

                        for (const DataNode& sourceElementNode : sourceNode->m_children)
                        {
                            SerializeContext::ClassPersistentId sourcePersistentIdFunction = sourceElementNode.m_classData->GetPersistentId(*context);
                            if (sourcePersistentIdFunction)
                            {
                                if (targetElementId == sourcePersistentIdFunction(sourceElementNode.m_data))
                                {
                                    sourceNodeMatch = &sourceElementNode;
                                    break;
                                }
                            }
                        }

                        elementId = targetElementId; // we use persistent ID for an id
                    }
                    else
                    {
                        // if we don't have IDs use the container index
                        if (elementIndex < sourceNode->m_children.size())
                        {
                            sourceNodeMatch = &(*AZStd::next(sourceNode->m_children.begin(), elementIndex));
                        }

                        elementId = elementIndex; // use index as an ID
                    }

                    address.push_back(elementId);

                    // determine flags for next address
                    DataPatch::Flags nextAddressFlags = addressFlags;
                    auto foundNewAddressFlags = patchFlags.find(address);
                    if (foundNewAddressFlags != patchFlags.end())
                    {
                        nextAddressFlags |= foundNewAddressFlags->second;
                    }

                    if (sourceNodeMatch)
                    {
                        // compare elements
                        CompareElementsInternal(sourceNodeMatch, &targetElementNode, patch, patchFlags, context, address, nextAddressFlags, tmpSourceBuffer);
                    }
                    else
                    {
                        // this is a new node store it
                        auto insertResult = patch.insert_key(address);
                        insertResult.first->second.clear();

                        IO::ByteContainerStream<DataPatch::PatchMap::mapped_type> stream(&insertResult.first->second);
                        if (!Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_BINARY, targetElementNode.m_data, targetElementNode.m_classData->m_typeId, context, targetElementNode.m_classData))
                        {
                            AZ_Assert(false, "Unable to serialize class %s, SaveObjectToStream() failed.", targetElementNode.m_classData->m_name);
                        }
                    }

                    address.pop_back();

                    ++elementIndex;
                }

                // find elements we have removed (todo: mark matching elements in the first traversal above)
                elementIndex = 0;
                elementId = 0;
                for (const DataNode& sourceElementNode : sourceNode->m_children)
                {
                    bool isRemoved = true;

                    SerializeContext::ClassPersistentId sourcePersistentIdFunction = sourceElementNode.m_classData->GetPersistentId(*context);
                    if (sourcePersistentIdFunction)
                    {
                        u64 sourceElementId = sourcePersistentIdFunction(sourceElementNode.m_data);
                        elementId = sourceElementId;

                        for (const DataNode& targetElementNode : targetNode->m_children)
                        {
                            SerializeContext::ClassPersistentId targetPersistentIdFunction = targetElementNode.m_classData->GetPersistentId(*context);
                            if (targetPersistentIdFunction)
                            {
                                if (sourceElementId == targetPersistentIdFunction(targetElementNode.m_data))
                                {
                                    isRemoved = false;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (elementIndex < targetNode->m_children.size())
                        {
                            isRemoved = false;
                        }
                        elementId = elementIndex;
                    }

                    if (isRemoved)
                    {
                        address.push_back(elementId);

                        // record removal of element by inserting a key with a 0 byte patch
                        patch.insert_key(address);

                        address.pop_back();
                    }
                    ++elementIndex;
                }
            }
            else if (targetNode->m_classData->m_serializer)
            {
                AZ_Assert(targetNode->m_classData == sourceNode->m_classData, "Comparison raw data for mismatched types.");

                // This is a leaf element (has a direct serializer).
                // Write to patch if values differ, or if the ForceOverride flag applies to this address
                if ((addressFlags & DataPatch::Flag::ForceOverride)
                    || !targetNode->m_classData->m_serializer->CompareValueData(sourceNode->m_data, targetNode->m_data))
                {
                    //serialize target override
                    auto insertResult = patch.insert_key(address);
                    insertResult.first->second.clear();

                    IO::ByteContainerStream<DataPatch::PatchMap::mapped_type> stream(&insertResult.first->second);
                    if (!Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_BINARY, targetNode->m_data, targetNode->m_classData->m_typeId, context, targetNode->m_classData))
                    {
                        AZ_Assert(false, "Unable to serialize class %s, SaveObjectToStream() failed.", targetNode->m_classData->m_name);
                    }
                }
            }
            else
            {
                // Not containers, just compare elements. Since they are known at compile time and class data is shared
                // elements will be in the order and matching.
                auto targetElementIt = targetNode->m_children.begin();
                auto sourceElementIt = sourceNode->m_children.begin();
                while (targetElementIt != targetNode->m_children.end())
                {
                    address.push_back(sourceElementIt->m_classElement->m_nameCrc); // use element class element name as an ID

                    // determine flags for next address
                    DataPatch::Flags nextAddressFlags = addressFlags;
                    auto foundNewAddressFlags = patchFlags.find(address);
                    if (foundNewAddressFlags != patchFlags.end())
                    {
                        nextAddressFlags |= foundNewAddressFlags->second;
                    }

                    CompareElementsInternal(&(*sourceElementIt), &(*targetElementIt), patch, patchFlags, context, address, nextAddressFlags, tmpSourceBuffer);

                    address.pop_back();

                    ++sourceElementIt;
                    ++targetElementIt;
                }
            }
        }
        else
        {
            // serialize the entire target class
            auto insertResult = patch.insert_key(address);
            insertResult.first->second.clear();

            IO::ByteContainerStream<DataPatch::PatchMap::mapped_type> stream(&insertResult.first->second);
            if (!Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_BINARY, targetNode->m_data, targetNode->m_classData->m_typeId, context, targetNode->m_classData))
            {
                AZ_Assert(false, "Unable to serialize class %s, SaveObjectToStream() failed.", targetNode->m_classData->m_name);
            }
        }
    }

    //=========================================================================
    // ApplyToElements
    //=========================================================================
    void* DataNodeTree::ApplyToElements(DataNode* sourceNode, const DataPatch::PatchMap& patch, DataPatch::AddressType& address, void* parentPointer, const SerializeContext::ClassData* parentClassData, AZStd::vector<AZ::u8>& tmpSourceBuffer, SerializeContext* context, const AZ::ObjectStream::FilterDescriptor& filterDesc)
    {
        void* targetPointer = nullptr;
        auto patchIt = patch.find(address);
        if (patchIt != patch.end())
        {
            // we have a patch to this node
            if (parentPointer)
            {
                if (parentClassData->m_container)
                {
                    if (patchIt->second.empty())
                    {
                        // if patch is empty do remove the element
                        return nullptr;
                    }
                    else
                    {
                        // Allocate space in the container for our element
                        targetPointer = parentClassData->m_container->ReserveElement(parentPointer, sourceNode->m_classElement);
                    }
                }
                else
                {
                    // We are stored by value, use the parent offset
                    targetPointer = reinterpret_cast<char*>(parentPointer) + sourceNode->m_classElement->m_offset;
                }

                IO::MemoryStream stream(patchIt->second.data(), patchIt->second.size());
                if (sourceNode->m_classElement->m_flags & SerializeContext::ClassElement::FLG_POINTER)
                {
                    // load the element
                    *reinterpret_cast<void**>(targetPointer) = Utils::LoadObjectFromStream(stream, context, nullptr, filterDesc);
                }
                else
                {
                    // load in place
                    ObjectStream::LoadBlocking(&stream, *context, ObjectStream::ClassReadyCB(), filterDesc,
                        [&targetPointer, &sourceNode](void** rootAddress, const SerializeContext::ClassData** classData, const Uuid& classId, SerializeContext* context)
                        {
                            (void)context;
                            (void)classId;
                            if (rootAddress)
                            {
                                *rootAddress = targetPointer;
                            }
                            if (classData)
                            {
                                *classData = sourceNode->m_classData;
                            }
                        });
                }

                if (parentClassData->m_container)
                {
                    parentClassData->m_container->StoreElement(parentPointer, targetPointer);
                }
            }
            else
            {
                // Since this is the root element, we will need to allocate it using the creator provided
                IO::MemoryStream stream(patchIt->second.data(), patchIt->second.size());
                return Utils::LoadObjectFromStream(stream, context, nullptr, filterDesc);
            }
        }
        else
        {
            if (parentPointer)
            {
                if (parentClassData->m_container)
                {
                    // Allocate space in the container for our element
                    targetPointer = parentClassData->m_container->ReserveElement(parentPointer, sourceNode->m_classElement);
                }
                else
                {
                    // We are stored by value, use the parent offset
                    targetPointer = reinterpret_cast<char*>(parentPointer) + sourceNode->m_classElement->m_offset;
                }

                // create a new instance if needed
                if (sourceNode->m_classElement->m_flags & SerializeContext::ClassElement::FLG_POINTER)
                {
                    // create a new instance if we are referencing it by pointer
                    AZ_Assert(sourceNode->m_classData->m_factory != nullptr, "We are attempting to create '%s', but no factory is provided! Either provide factory or change data member '%s' to value not pointer!", sourceNode->m_classData->m_name, sourceNode->m_classElement->m_name);
                    void* newTargetPointer = sourceNode->m_classData->m_factory->Create(sourceNode->m_classData->m_name);

                    // we need to account for additional offsets if we have a pointer to a base class.
                    void* basePtr = context->DownCast(newTargetPointer, sourceNode->m_classData->m_typeId, sourceNode->m_classElement->m_typeId, sourceNode->m_classData->m_azRtti, sourceNode->m_classElement->m_azRtti);
                    AZ_Assert(basePtr != nullptr, parentClassData->m_container
                        ? "Can't cast container element %s(0x%x) to %s, make sure classes are registered in the system and not generics!"
                        : "Can't cast %s(0x%x) to %s, make sure classes are registered in the system and not generics!"
                        , sourceNode->m_classElement->m_name ? sourceNode->m_classElement->m_name : "NULL"
                        , sourceNode->m_classElement->m_nameCrc
                        , sourceNode->m_classData->m_name);

                    *reinterpret_cast<void**>(targetPointer) = basePtr; // store the pointer in the class

                    // further construction of the members need to be based off the pointer to the
                    // actual type, not the base type!
                    targetPointer = newTargetPointer;
                }
            }
            else
            {
                // this is a root element, create a new element
                targetPointer = sourceNode->m_classData->m_factory->Create(sourceNode->m_classData->m_name);
            }

            if (sourceNode->m_classData->m_eventHandler)
            {
                sourceNode->m_classData->m_eventHandler->OnWriteBegin(targetPointer);
            }

            if (sourceNode->m_classData->m_serializer)
            {
                // this is leaf node copy from the source
                tmpSourceBuffer.clear();
                IO::ByteContainerStream<DataPatch::PatchMap::mapped_type> sourceStream(&tmpSourceBuffer);
                sourceNode->m_classData->m_serializer->Save(sourceNode->m_data, sourceStream);
                IO::MemoryStream targetStream(tmpSourceBuffer.data(), tmpSourceBuffer.size());
                sourceNode->m_classData->m_serializer->Load(targetPointer, targetStream, sourceNode->m_classData->m_version);
            }
            else
            {
                if (sourceNode->m_classData->m_container)
                {
                    // find elements that we have added or modified
                    u64 elementIndex = 0;
                    u64 elementId = 0;
                    for (DataNode& sourceElementNode : sourceNode->m_children)
                    {
                        SerializeContext::ClassPersistentId sourcePersistentIdFunction = sourceElementNode.m_classData->GetPersistentId(*context);
                        if (sourcePersistentIdFunction)
                        {
                            // we use persistent ID for an id
                            elementId = sourcePersistentIdFunction(sourceElementNode.m_data);
                        }
                        else
                        {
                            // use index as an ID
                            elementId = elementIndex;
                        }

                        address.push_back(elementId);

                        ApplyToElements(&sourceElementNode, patch, address, targetPointer, sourceNode->m_classData, tmpSourceBuffer, context, filterDesc);

                        address.pop_back();

                        ++elementIndex;
                    }

                    // Find missing elements that need to be added to container.
                    // \note check performance, tag new elements to improve it.
                    AZStd::vector<u64> newElementIds;
                    for (auto& keyValue : patch)
                    {
                        if (keyValue.second.empty())
                        {
                            continue; // this is removal of element, we already did that above
                        }

                        DataPatch::AddressType patchAddress = keyValue.first;
                        if (patchAddress.size() == address.size() + 1) // only container elements, not container element, sub element patches
                        {
                            u64 newElementId = patchAddress.back();
                            patchAddress.pop_back();
                            if (patchAddress == address) // make sure that this patch is for the container
                            {
                                elementIndex = 0;
                                bool isFound = false;
                                for (DataNode& sourceElementNode : sourceNode->m_children)
                                {
                                    SerializeContext::ClassPersistentId sourcePersistentIdFunction = sourceElementNode.m_classData->GetPersistentId(*context);
                                    if (sourcePersistentIdFunction)
                                    {
                                        // we use persistent ID for an id
                                        elementId = sourcePersistentIdFunction(sourceElementNode.m_data);
                                    }
                                    else
                                    {
                                        // use index as an ID
                                        elementId = elementIndex;
                                    }

                                    if (elementId == newElementId)
                                    {
                                        isFound = true;
                                        break;
                                    }
                                    ++elementIndex;
                                }

                                if (!isFound) // if element is not in the source container, it will be added
                                {
                                    newElementIds.push_back(newElementId);
                                }
                            }
                        }
                    }

                    // Sort so that elements using index as ID retain relative order.
                    AZStd::sort(newElementIds.begin(), newElementIds.end());

                    // Add missing elements to container.
                    for (u64 newElementId : newElementIds)
                    {
                        address.push_back(newElementId);

                        // pick any child element for a classElement sample
                        DataNode defaultSourceNode;
                        defaultSourceNode.m_classElement = sourceNode->m_classData->m_container->GetElement(sourceNode->m_classData->m_container->GetDefaultElementNameCrc());

                        ApplyToElements(&defaultSourceNode, patch, address, targetPointer, sourceNode->m_classData, tmpSourceBuffer, context, filterDesc);

                        address.pop_back();
                    }
                }
                else
                {
                    // Traverse child elements
                    auto sourceElementIt = sourceNode->m_children.begin();
                    while (sourceElementIt != sourceNode->m_children.end())
                    {
                        address.push_back(sourceElementIt->m_classElement->m_nameCrc); // use element class element name as an ID

                        ApplyToElements(&(*sourceElementIt), patch, address, targetPointer, sourceNode->m_classData, tmpSourceBuffer, context, filterDesc);

                        address.pop_back();

                        ++sourceElementIt;
                    }
                }
            }

            if (sourceNode->m_classData->m_eventHandler)
            {
                sourceNode->m_classData->m_eventHandler->OnWriteEnd(targetPointer);
            }

            if (parentPointer && parentClassData->m_container)
            {
                parentClassData->m_container->StoreElement(parentPointer, targetPointer);
            }
        }

        return targetPointer;
    }

    //=========================================================================
    // DataPatch
    //=========================================================================
    DataPatch::DataPatch()
    {
        m_targetClassId = Uuid::CreateNull();
    }

    //=========================================================================
    // DataPatch
    //=========================================================================
    DataPatch::DataPatch(const DataPatch& rhs)
    {
        m_patch = rhs.m_patch;
        m_targetClassId = rhs.m_targetClassId;
    }

#ifdef AZ_HAS_RVALUE_REFS
    //=========================================================================
    // DataPatch
    //=========================================================================
    DataPatch::DataPatch(DataPatch&& rhs)
    {
        m_patch = AZStd::move(rhs.m_patch);
        m_targetClassId = AZStd::move(rhs.m_targetClassId);
    }

    //=========================================================================
    // operator=
    //=========================================================================
    DataPatch& DataPatch::operator = (DataPatch&& rhs)
    {
        m_patch = AZStd::move(rhs.m_patch);
        m_targetClassId = AZStd::move(rhs.m_targetClassId);
        return *this;
    }
#endif // AZ_HAS_RVALUE_REF

    //=========================================================================
    // operator=
    //=========================================================================
    DataPatch& DataPatch::operator = (const DataPatch& rhs)
    {
        m_patch = rhs.m_patch;
        m_targetClassId = rhs.m_targetClassId;
        return *this;
    }

    //=========================================================================
    // Create
    //=========================================================================
    bool DataPatch::Create(const void* source, const Uuid& sourceClassId, const void* target, const Uuid& targetClassId, const FlagsMap& patchFlags, SerializeContext* context)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (!source || !target)
        {
            AZ_Error("Serialization", false, "Can't generate a patch with invalid input source %p and target %p\n", source, target);
            return false;
        }

        if (!context)
        {
            EBUS_EVENT_RESULT(context, ComponentApplicationBus, GetSerializeContext);
            if (!context)
            {
                AZ_Error("Serialization", false, "Not serialize context provided! Failed to get component application default serialize context! ComponentApp is not started or input serialize context should not be null!");
                return false;
            }
        }

        if (!context->FindClassData(sourceClassId))
        {
            AZ_Error("Serialization", false, "Can't find class data for the source type Uuid %s.", sourceClassId.ToString<AZStd::string>().c_str());
            return false;
        }

        const SerializeContext::ClassData* targetClassData = context->FindClassData(targetClassId);
        if (!targetClassData)
        {
            AZ_Error("Serialization", false, "Can't find class data for the target type Uuid %s.", sourceClassId.ToString<AZStd::string>().c_str());
            return false;
        }

        m_patch.clear();
        m_targetClassId = targetClassId;

        if (sourceClassId != targetClassId)
        {
            // serialize the entire target class
            auto insertResult = m_patch.insert_key(DataPatch::AddressType());
            insertResult.first->second.clear();

            IO::ByteContainerStream<DataPatch::PatchMap::mapped_type> stream(&insertResult.first->second);
            if (!Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_BINARY, target, targetClassId, context))
            {
                AZ_Assert(false, "Unable to serialize class %s, SaveObjectToStream() failed.", targetClassData->m_name);
            }
        }
        else
        {
            // Build the tree for the course and compare it against the target
            DataNodeTree sourceTree(context);
            sourceTree.Build(source, sourceClassId);

            DataNodeTree targetTree(context);
            targetTree.Build(target, targetClassId);

            {
                AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "DataPatch::Create:RecursiveCallToCompareElements");
                sourceTree.CompareElements(&sourceTree.m_root, &targetTree.m_root, m_patch, patchFlags, context);
            }
        }
        return true;
    }

    //=========================================================================
    // Apply
    //=========================================================================
    void* DataPatch::Apply(const void* source, const Uuid& sourceClassId, SerializeContext* context, const AZ::Utils::FilterDescriptor& filterDesc)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (!source)
        {
            AZ_Error("Serialization", false, "Can't apply patch to invalid source %p\n", source);
            return nullptr;
        }

        if (!context)
        {
            EBUS_EVENT_RESULT(context, ComponentApplicationBus, GetSerializeContext);
            if (!context)
            {
                AZ_Error("Serialization", false, "Not serialize context provided! Failed to get component application default serialize context! ComponentApp is not started or input serialize context should not be null!");
                return nullptr;
            }
        }

        if (m_patch.empty())
        {
            // If no patch just clone the object
            return context->CloneObject(source, sourceClassId);
        }

        if (m_patch.size() == 1 && m_patch.begin()->first.empty())  // if we replace the root element
        {
            IO::MemoryStream stream(m_patch.begin()->second.data(), m_patch.begin()->second.size());
            return Utils::LoadObjectFromStream(stream, context, nullptr, filterDesc);
        }

        DataNodeTree sourceTree(context);
        sourceTree.Build(source, sourceClassId);

        DataPatch::AddressType address;
        AZStd::vector<AZ::u8> tmpSourceBuffer;
        void* result;
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "DataPatch::Apply:RecursiveCallToApplyToElements");
            result = DataNodeTree::ApplyToElements(&sourceTree.m_root, m_patch, address, nullptr, nullptr, tmpSourceBuffer, context, filterDesc);
        }
        return result;
    }

    //=========================================================================
    // Apply
    //=========================================================================
    bool DataPatch::Apply(const DataPatch& patch)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (m_targetClassId.IsNull())
        {
            if (patch.m_targetClassId.IsNull())
            {
                return false; // both patches don't have target
            }

            // since this is an empty patch adopt the target class id
            AZ_Assert(m_patch.empty(), "We have patch data, but not targetClassId, this should not happen (invalid patch)!");
            m_targetClassId = patch.m_targetClassId;
        }
        else if (m_targetClassId != patch.m_targetClassId)
        {
            if (patch.m_targetClassId.IsNull())
            {
                //target patch is empty... we can consider it applied
                return true;
            }
            else
            {
                // current patch and the one to apply are pointing to different root/target classes,
                // you can't apply patches that are unrelated.
                return false;
            }
        }

        for (auto extraPatchIt : patch.m_patch)
        {
            // remove patches that are going to be overridden after adding the extra patch.
            for (auto currentPatchIt = m_patch.begin(); currentPatchIt != m_patch.end(); )
            {
                // check if we have a patch which overrides existing patch
                if (currentPatchIt->first.size() >= extraPatchIt.first.size() &&   // check if the address is shorter or equal
                    memcmp(currentPatchIt->first.data(), extraPatchIt.first.data(), extraPatchIt.first.size() * sizeof(DataPatch::AddressType::value_type)) == 0) // if first part of the address matches the extraPatch will override the current
                {
                    currentPatchIt = m_patch.erase(currentPatchIt);
                }
                else if (currentPatchIt->second.size() == 0 &&  // if we removing an element, but have a patch for it in the target patch, undo the removal
                         currentPatchIt->first.size() < extraPatchIt.first.size() &&
                         memcmp(currentPatchIt->first.data(), extraPatchIt.first.data(), currentPatchIt->first.size() * sizeof(DataPatch::AddressType::value_type)) == 0)
                {
                    currentPatchIt = m_patch.erase(currentPatchIt);
                }
                else
                {
                    ++currentPatchIt;
                }
            }

            m_patch.insert(extraPatchIt);
        }

        return true;
    }

    /**
     * Custom serializer for our address type, as we want to be more space efficient and not store every element of the container
     * separately.
     */
    class AddressTypeSerializer
        : public AZ::Internal::AZBinaryData
    {
        /// Load the class data from a stream.
        bool Load(void* classPtr, IO::GenericStream& stream, unsigned int /*version*/, bool isDataBigEndian = false) override
        {
            (void)isDataBigEndian;
            DataPatch::AddressType* address = reinterpret_cast<DataPatch::AddressType*>(classPtr);
            address->clear();
            size_t dataSize = static_cast<size_t>(stream.GetLength());
            size_t numElements = dataSize / sizeof(DataPatch::AddressType::value_type);
            address->resize_no_construct(numElements);
            stream.Read(dataSize, address->data());
            return true;
        }

        /// Store the class data into a stream.
        size_t Save(const void* classPtr, IO::GenericStream& stream, bool isDataBigEndian /*= false*/) override
        {
            (void)isDataBigEndian;
            const  DataPatch::AddressType* container = reinterpret_cast<const DataPatch::AddressType*>(classPtr);
            size_t dataSize = container->size() * sizeof(DataPatch::AddressType::value_type);
            return static_cast<size_t>(stream.Write(dataSize, container->data()));
        }

        bool CompareValueData(const void* lhs, const void* rhs) override
        {
            return SerializeContext::EqualityCompareHelper<DataPatch::AddressType>::CompareValues(lhs, rhs);
        }
    };

    //=========================================================================
    // Apply
    //=========================================================================
    void DataPatch::Reflect(SerializeContext& context)
    {
        context.Class<DataPatch::AddressType>()->
            Serializer<AddressTypeSerializer>();

        context.Class<DataPatch>()->
            Field("m_targetClassId", &DataPatch::m_targetClassId)->
            Field("m_patch", &DataPatch::m_patch);
    }
}   // namespace AZ

#endif // #ifndef AZ_UNITY_BUILD
