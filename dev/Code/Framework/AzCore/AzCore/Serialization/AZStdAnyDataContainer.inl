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

#pragma once

#include <AzCore/std/any.h>
#include <AzCore/Component/ComponentApplicationBus.h>

namespace AZ
{
    namespace Internal
    {
        struct AZStdAnyContainer
            : public SerializeContext::IDataContainer
        {
        public:
            AZStdAnyContainer() = default;

            /// Null if element with this name can't be found.
            const SerializeContext::ClassElement* GetElement(u32) const override
            {
                return nullptr;
            }

            bool GetElement(SerializeContext::ClassElement& classElement, const SerializeContext::DataElement& dataElement) const override
            {
                if (static_cast<AZ::u32>(AZ_CRC("m_data", 0x335cc942)) == dataElement.m_nameCrc)
                {
                    classElement.m_name = "m_data";
                    classElement.m_nameCrc = AZ_CRC("m_data", 0x335cc942);
                    classElement.m_typeId = dataElement.m_id;
                    classElement.m_dataSize = sizeof(void*);
                    classElement.m_offset = 0;
                    classElement.m_azRtti = nullptr;
                    classElement.m_genericClassInfo = m_serializeContext->FindGenericClassInfo(dataElement.m_specializedId);
                    classElement.m_editData = nullptr;
                    classElement.m_flags = SerializeContext::ClassElement::FLG_DYNAMIC_FIELD;
                    return true;
                }
                return false;
            }

            /// Enumerate elements in the array.
            void EnumElements(void* instance, const ElementCB& cb) override
            {
                auto anyPtr = reinterpret_cast<AZStd::any*>(instance);

                AZ::Uuid anyTypeId = anyPtr->type();
                // Empty AZStd::any will not have the empty element serialized
                if (anyPtr->empty() || !AZStd::any_cast<void>(anyPtr))
                {
                    return;
                }

                SerializeContext::ClassElement anyChildElement;
                anyChildElement.m_name = "m_data";
                anyChildElement.m_nameCrc = AZ_CRC("m_data", 0x335cc942);
                anyChildElement.m_typeId = anyTypeId;
                anyChildElement.m_dataSize = sizeof(void*);
                anyChildElement.m_offset = 0;
                anyChildElement.m_azRtti = nullptr;
                anyChildElement.m_genericClassInfo = m_serializeContext->FindGenericClassInfo(anyTypeId);
                anyChildElement.m_editData = nullptr;
                anyChildElement.m_flags = SerializeContext::ClassElement::FLG_DYNAMIC_FIELD | (anyPtr->get_type_info().m_isPointer ? SerializeContext::ClassElement::FLG_POINTER : 0);

                if (!cb(AZStd::any_cast<void>(anyPtr), anyTypeId, anyChildElement.m_genericClassInfo ? anyChildElement.m_genericClassInfo->GetClassData() : nullptr, &anyChildElement))
                {
                    return;
                }
            }

            /// Return number of elements in the container.
            size_t  Size(void* instance) const override
            {
                (void)instance;
                return 1;
            }

            /// Returns the capacity of the container. Returns 0 for objects without fixed capacity.
            size_t Capacity(void* instance) const override
            {
                (void)instance;
                return 1;
            }

            /// Returns true if elements pointers don't change on add/remove. If false you MUST enumerate all elements.
            bool    IsStableElements() const override { return true; }

            /// Returns true if the container is fixed size, otherwise false.
            bool    IsFixedSize() const override { return true; }

            /// Returns if the container is fixed capacity, otherwise false
            bool    IsFixedCapacity() const override { return true; }

            /// Returns true if the container is a smart pointer.
            bool    IsSmartPointer() const override { return false; }

            /// Returns true if elements can be retrieved by index.
            bool    CanAccessElementsByIndex() const override { return false; }

            /// Reserve element
            void* ReserveElement(void* instance, const SerializeContext::ClassElement* classElement) override
            {
                if (m_serializeContext && classElement)
                {
                    auto anyPtr = reinterpret_cast<AZStd::any*>(instance);
                    *anyPtr = m_serializeContext->CreateAny(classElement->m_genericClassInfo ? classElement->m_genericClassInfo->GetSpecializedTypeId() : classElement->m_typeId);
                    return AZStd::any_cast<void>(anyPtr);
                }
                
                return instance;
            }

            /// Get an element's address by its index (called before the element is loaded).
            void* GetElementByIndex(void* instance, const SerializeContext::ClassElement* classElement, size_t index) override
            {
                (void)instance;
                (void)classElement;
                (void)index;
                return nullptr;
            }

            /// Store element
            void StoreElement(void* instance, void* element) override
            {
                (void)instance;
                (void)element;
                // do nothing as we have already pushed the element,
                // we can assert and check if the element belongs to the container
            }

            /// Remove element in the container.
            bool RemoveElement(void* instance, const void*, SerializeContext*) override
            {
                auto anyPtr = reinterpret_cast<AZStd::any*>(instance);
                if (anyPtr->empty())
                {
                    return false;
                }

                anyPtr->clear();
                return true;
            }

            /// Remove elements (removed array of elements) regardless if the container is Stable or not (IsStableElements)
            size_t RemoveElements(void* instance, const void** elements, size_t numElements, SerializeContext* deletePointerDataContext) override
            {
                return numElements == 1 && RemoveElement(instance, elements[0], deletePointerDataContext) ? 1 : 0;
            }

            /// Clear elements in the instance.
            void ClearElements(void* instance, SerializeContext*) override
            {
                auto anyPtr = reinterpret_cast<AZStd::any*>(instance);
                anyPtr->clear();
            }

            void SetSerializeContext(SerializeContext* serializeContext)
            {
                m_serializeContext = serializeContext;
            }

            SerializeContext* m_serializeContext = nullptr;
        };

        inline void ReflectAny(ReflectContext* reflectContext)
        {       
            if (auto serializeContext = azrtti_cast<SerializeContext*>(reflectContext))
            {
                auto *dataContainer =  &Serialize::StaticInstance<AZStdAnyContainer>::s_instance;
                dataContainer->SetSerializeContext(serializeContext);
                serializeContext->Class<AZStd::any>()
                    ->DataContainer(dataContainer);
                    ;
                // Value data is injected into the hierarchy per-instance, since type is dynamic.
            }
        }
    }
}

