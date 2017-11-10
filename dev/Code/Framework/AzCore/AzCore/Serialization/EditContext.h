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
#ifndef AZCORE_EDIT_CONTEXT_H
#define AZCORE_EDIT_CONTEXT_H

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/typetraits/is_function.h>

#include <AzCore/Math/Crc.h> // use by all functions to set the attribute id.

#include <AzCore/Serialization/EditContextConstants.inl>

namespace AZ
{
    namespace Edit
    {
        using AttributeId = AZ::AttributeId;
        using AttributePair = AZ::AttributePair;
        using AttributeArray = AZ::AttributeArray;

        using Attribute = AZ::Attribute;
        template<class T>
        using AttributeData = AZ::AttributeData<T>;
        template<class T>
        using AttributeMemberData = AZ::AttributeMemberData<T>;
        template<class F>
        using AttributeFunction = AZ::AttributeFunction<F>;
        template<class F>
        using AttributeMemberFunction = AZ::AttributeMemberFunction<F>;

        /**
         * Signature of the dynamic edit data provider function.
         * handlerPtr: pointer to the object whose edit data registered the handler
         * elementPtr: pointer to the sub-member of handlePtr that we are querying edit data for.
         * The function should return a pointer to the ElementData to use, or nullptr to use the
         * default one.
         */
        typedef const ElementData* DynamicEditDataProvider (const void* /*handlerPtr*/, const void* /*elementPtr*/, const Uuid& /*elementType*/);

        /**
         * Edit data is assigned to each SerializeContext::ClassInfo::Field. You can assign all kinds of
         * generic attributes. You can have elements for class members called DataElements or Elements which define
         * attributes for the class itself called ClassElement.
         */
        struct ElementData
        {
            ElementData()
                : m_elementId(0)
                , m_description(nullptr)
                , m_name(nullptr)
                , m_serializeClassElement(nullptr)
            {}

            void ClearAttributes();

            bool IsClassElement() const   { return m_serializeClassElement == nullptr; }
            Edit::Attribute* FindAttribute(AttributeId attributeId) const;

            AttributeId         m_elementId;
            const char*         m_description;
            const char*         m_name;
            SerializeContext::ClassElement* m_serializeClassElement; ///< If nullptr this is class (logical) element, not physical element exists in the class
            AttributeArray      m_attributes;
        };

        /**
         * Class data is assigned to every Serialize::Class. Don't confuse m_elements with
         * ElementData. Elements contains class Elements (like groups, etc.) while the ElementData
         * contains attributes related to ehe SerializeContext::ClassInfo::Field.
         */
        struct ClassData
        {
            ClassData()
                : m_name(nullptr)
                , m_description(nullptr)
                , m_classData(nullptr)
                , m_editDataProvider(nullptr)
            {}

            void ClearElements();
            const ElementData* FindElementData(AttributeId elementId) const;

            const char*                 m_name;
            const char*                 m_description;
            SerializeContext::ClassData* m_classData;
            DynamicEditDataProvider*    m_editDataProvider;
            AZStd::list<ElementData>    m_elements;
        };
    }

    /**
     * EditContext is bound to serialize context. It uses it for data manipulation.
     * It's role is to be an abstract way to generate and describe how a class should
     * be edited. It doesn't rely on any edit/editor/etc. code and it should not as
     * it's ABSTRACT. Please refer to your editor/edit tools to check what are the "id"
     * for the uiElements and their respective properties "id" and values.
     */
    class EditContext
    {
        class ClassInfo;
        class EnumInfo;
    public:
        AZ_CLASS_ALLOCATOR(EditContext, SystemAllocator, 0);

        /**
         * EditContext uses serialize context to interact with data, so serialize context is
         * required.
         */
        EditContext(SerializeContext& serializeContext);
        ~EditContext();

        template<class T>
        ClassInfo Class(const char* displayName, const char* description);

        template <class E>
        EnumInfo Enum(const char* displayName, const char* description);

        void RemoveClassData(SerializeContext::ClassData* classData);

    private:
        EditContext(const EditContext&);
        EditContext& operator=(const EditContext&);

        /**
         * Internal structure to maintain class information while we are describing a class.
         * User should call variety of functions to describe class features and data.
         * example:
         *  struct MyStruct {
         *      int m_data };
         *
         * expose for edit
         *  editContext->Class<MyStruct>("My structure","This structure was made to apply structure action!")->
         *      ClassElement(AZ::Edit::ClassElements::Group,"MyGroup")->
         *          Attribute("Callback",&MyStruct::IsMyGroup)->
         *      DataElement(AZ::Edit::UIHandlers::Slider,&MyStruct::m_data,"Structure data","My structure data")->
         *          Attribute(AZ::Edit::Attributes::Min,0)->
         *          Attribute(AZ::Edit::Attributes::Max,100)->
         *          Attribute(AZ::Edit::Attributes::Step,5);
         *
         *  or if you have some structure to group information or offer default values, etc. (that you know if the code and it's safe to include)
         *  you can do something like:
         *
         *  serializeContext->Class<MyStruct>("My structure","This structure was made to apply structure action!")->
         *      ClassElement(AZ::Edit::ClassElements::Group,"MyGroup")->
         *          Attribute("Callback",&MyStruct::IsMyGroup)->
         *      DataElement(AZ::Edit::UIHandlers::Slider,&MyStruct::m_data,"Structure data","My structure data")->
         *          Attribute("Params",SliderParams(0,100,5));
         *
         *  Attributes can be any value (must be copy constructed) or a function type. We do supported member functions and member data.
         *  look at the unit tests and example to see use cases.
         *
         */
        class ClassInfo
        {
            friend EditContext;
            ClassInfo(EditContext* context, SerializeContext::ClassData* classData, Edit::ClassData* classElement)
                : m_context(context)
                , m_classData(classData)
                , m_classElement(classElement)
                , m_editElement(nullptr)
            {}
            EditContext*                    m_context;
            SerializeContext::ClassData*    m_classData;
            Edit::ClassData*                m_classElement;
            Edit::ElementData*              m_editElement;

            Edit::ClassData* FindClassData(const Uuid& typeId);

            // If E is an enum, copy any globally reflected values into the ElementData
            template <class E>
            typename AZStd::enable_if<AZStd::is_enum<E>::value>::type
            CopyEnumValues(Edit::ElementData* ed);

            // Do nothing for non-enum types
            template <class E>
            typename AZStd::enable_if<!AZStd::is_enum<E>::value>::type
            CopyEnumValues(Edit::ElementData*) {}

        public:
            ~ClassInfo() {}
            ClassInfo* operator->()     { return this; }

            /**
             * Declare element with attributes that belong to the class SerializeContext::Class, this is a logical structure, you can have one or more ClassElements.
             * \uiId is the logical element ID (for instance "Group" when you want to group certain elements in this class.
             * then in each DataElement you can attach the appropriate group attribute.
             */
            ClassInfo*  ClassElement(Crc32 elementIdCrc, const char* description);

            /** Declare element that will handle a specific class member variable (SerializeContext::ClassInfo::Field).
             * \param uiId - us element ID ("Int" or "Real", etc. how to edit the memberVariable)
             * \param memberVariable - reference to the member variable to we can bind to serializations data.
             * \param name - descriptive name of the field. Use this when using types in context. For example T is 'int' and name describes what it does.
             * Sometime 'T' will have edit context with enough information for name and description. In such cases use the DataElement function below.
             * \param description - detailed description that will usually appear in a tool tip.
             */
            template<class T>
            ClassInfo*  DataElement(const char* uiId, T memberVariable, const char* name, const char* description);

            /** Declare element that will handle a specific class member variable (SerializeContext::ClassInfo::Field).
            * \param uiId - Crc32 of us element ID ("Int" or "Real", etc. how to edit the memberVariable)
            * \param memberVariable - reference to the member variable to we can bind to serializations data.
            * \param name - descriptive name of the field. Use this when using types in context. For example T is 'int' and name describes what it does.
            * Sometime 'T' will have edit context with enough information for name and description. In such cases use the DataElement function below.
            * \param description - detailed description that will usually appear in a tool tip.
            */
            template<class T>
            ClassInfo*  DataElement(Crc32 uiIdCrc, T memberVariable, const char* name, const char* description);

            /**
             * Same as above, except we will get the name and description from the edit context of the 'T' type. If 'T' doesn't have edit context
             * both name and the description will be set AzTypeInfo<T>::Name().
             * \note this function will search the context for 'T' type, it must be registered at the time of reflection, otherwise it will use the fallback
             * (AzTypeInfo<T>::Name()).
             */
            template<class T>
            ClassInfo*  DataElement(const char* uiId, T memberVariable);

            /**
            * Same as above, except we will get the name and description from the edit context of the 'T' type. If 'T' doesn't have edit context
            * both name and the description will be set AzTypeInfo<T>::Name().
            * \note this function will search the context for 'T' type, it must be registered at the time of reflection, otherwise it will use the fallback
            * (AzTypeInfo<T>::Name()).
            */
            template<class T>
            ClassInfo*  DataElement(Crc32 uiIdCrc, T memberVariable);

            /**
             * All T (attribute value) MUST be copy constructible as they are stored in internal
             * AttributeContainer<T>, which can be accessed by azrtti and AttributeData.
             * Attributes can be assigned to either classes or DataElements.
             */
            template<class T>
            ClassInfo* Attribute(const char* id, T value);

            /**
            * All T (attribute value) MUST be copy constructible as they are stored in internal
            * AttributeContainer<T>, which can be accessed by azrtti and AttributeData.
            * Attributes can be assigned to either classes or DataElements.
            */
            template<class T>
            ClassInfo* Attribute(Crc32 idCrc, T value);

            /**
             * Specialized attribute for defining enum values with an associated description.
             * Given the prevalence of the enum case, this prevents users from having to manually
             * create a pair<value, description> for every reflected enum value.
             * \note Do not add a bunch of these kinds of specializations. This one is generic
             * enough and common enough that it's warranted for programmer ease.
             */
            template<class T>
            ClassInfo* EnumAttribute(T value, const char* descriptor);

            /**
             * Specialized attribute for setting properties on elements inside a container.
             * This could be used to specify a spinbox handler for all elements inside a container
             * while only having to specify it once (on the parent container).
             */
            template<class T>
            ClassInfo* ElementAttribute(const char* id, T value);

            /**
            * Specialized attribute for setting properties on elements inside a container.
            * This could be used to specify a spinbox handler for all elements inside a container
            * while only having to specify it once (on the parent container).
            */
            template<class T>
            ClassInfo* ElementAttribute(Crc32 idCrc, T value);

            ClassInfo*  SetDynamicEditDataProvider(Edit::DynamicEditDataProvider* pHandler);
        };

        /**
        * Internal structure to maintain class information while we are describing an enum globally.
        * User should call Value() to reflect the possible values for the enum.
        * example:
        *  struct MyStruct {
        *      SomeEnum m_data };
        *
        * expose for edit
        *  editContext->Enum<SomeEnum>("My enum","This enum was made to apply enumerated action!")->
        *      Value("SomeValue", SomeEnum::SomeValue)->
        *      Value("SomeOtherValue", SomeEnum::SomeOtherValue);
        *
        *
        */
        class EnumInfo
        {
            friend EditContext;

            EnumInfo(EditContext* editContext, Edit::ElementData* elementData)
                : m_context(editContext)
                , m_elementData(elementData)
            {}

            EditContext* m_context;
            Edit::ElementData* m_elementData;
        public:
            EnumInfo* operator->() { return this; }

            template <class E>
            EnumInfo* Value(const char* name, E value);
        };

        typedef AZStd::list<Edit::ClassData> ClassDataListType;
        typedef AZStd::unordered_map<AZ::Uuid, Edit::ElementData> EnumDataMapType;

        ClassDataListType   m_classData;
        EnumDataMapType     m_enumData;
        SerializeContext&   m_serializeContext;
    };

    namespace Internal
    {
        inline Crc32 UuidToCrc32(const AZ::Uuid& uuid)
        {
            return Crc32(uuid.begin(), uuid.end() - uuid.begin());
        }

        inline bool IsModifyingGlobalEnum(Crc32 idCrc, Edit::ElementData& ed)
        {
            if (ed.m_serializeClassElement)
            {
                const Crc32 typeCrc = UuidToCrc32(ed.m_serializeClassElement->m_typeId);
                if (ed.m_elementId == typeCrc)
                {
                    return idCrc == AZ::Edit::InternalAttributes::EnumValue || idCrc == AZ::Edit::Attributes::EnumValues;
                }
            }
            return false;
        }
    }

    //=========================================================================
    // Class
    //=========================================================================
    template<class T>
    EditContext::ClassInfo
    EditContext::Class(const char* displayName, const char* description)
    {
        // find the class data in the serialize context.
        SerializeContext::UuidToClassMap::iterator classDataIter = m_serializeContext.m_uuidMap.find(AzTypeInfo<T>::Uuid());
        AZ_Assert(classDataIter != m_serializeContext.m_uuidMap.end(), "Class %s is not reflected in the serializer yet! Edit context can be set after the class is reflected!", AzTypeInfo<T>::Name());
        SerializeContext::ClassData* serializeClassData = &classDataIter->second;

        m_classData.push_back();
        Edit::ClassData& editClassData = m_classData.back();
        editClassData.m_name = displayName;
        editClassData.m_description = description;
        editClassData.m_editDataProvider = nullptr;
        editClassData.m_classData = serializeClassData;
        serializeClassData->m_editData = &editClassData;
        return EditContext::ClassInfo(this, serializeClassData, &editClassData);
    }

    //=========================================================================
    // Enum
    //=========================================================================
    template <class E>
    EditContext::EnumInfo
    EditContext::Enum(const char* displayName, const char* description)
    {
        AZ_STATIC_ASSERT(Internal::HasAZTypeInfo<E>::value, "Enums must have reflection type info (via AZ_TYPE_INFO_SPECIALIZE or AzTypeInfo<Enum>) to be reflected globally");
        const AZ::Uuid& enumId = azrtti_typeid<E>();
        AZ_Assert(m_enumData.find(enumId) == m_enumData.end(), "Enum %s has already been reflected to EditContext", displayName);
        Edit::ElementData& enumData = m_enumData[enumId];
        
        // Set the elementId to the Crc of the typeId, this indicates that it's globally reflected
        const Crc32 typeCrc = Internal::UuidToCrc32(enumId);
        enumData.m_elementId = typeCrc;
        enumData.m_name = displayName;
        enumData.m_description = description;
        return EditContext::EnumInfo(this, &enumData);
    }

    //=========================================================================
    // ClassElement
    //=========================================================================
    inline EditContext::ClassInfo*
    EditContext::ClassInfo::ClassElement(Crc32 elementIdCrc, const char* description)
    {
        m_classElement->m_elements.push_back();
        Edit::ElementData& ed = m_classElement->m_elements.back();
        ed.m_elementId = elementIdCrc;
        ed.m_description = description;
        m_editElement = &ed;
        return this;
    }

    //=========================================================================
    // DataElement
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::DataElement(const char* uiId, T memberVariable, const char* name, const char* description)
    {
        return DataElement(Crc32(uiId), memberVariable, name, description);
    }

    //=========================================================================
    // DataElement
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::DataElement(Crc32 uiIdCrc, T memberVariable, const char* name, const char* description)
    {
        using ElementTypeInfo = typename SerializeInternal::ElementInfo<T>;
        using EnumElementType = typename AZStd::Utils::if_c<AZStd::is_enum<typename ElementTypeInfo::Type>::value, typename ElementTypeInfo::Type, void>::type;
        AZ_Assert(m_classData->m_typeId == AzTypeInfo<typename ElementTypeInfo::ClassType>::Uuid(), "Data element (%s) belongs to a different class!", description);

        // Not really portable but works for the supported compilers
        size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<typename ElementTypeInfo::ClassType const volatile*>(0)->*memberVariable));
        //offset = or pass it to the function with offsetof(typename ElementTypeInfo::ClassType,memberVariable);

        SerializeContext::ClassElement* classElement = nullptr;
        for (size_t i = 0; i < m_classData->m_elements.size(); ++i)
        {
            SerializeContext::ClassElement* element = &m_classData->m_elements[i];
            if (element->m_offset == offset)
            {
                classElement = element;
                break;
            }
        }
        AZ_Assert(classElement != NULL, "Class element for editor data element reflection '%s' was NOT found in the serialize context! This member MUST be serializable to be editable!", name);

        m_classElement->m_elements.push_back();
        Edit::ElementData* ed = &m_classElement->m_elements.back();
        
        // If this is an enum that is globally reflected, copy value attributes
        const bool isSpecializedEnum = AZStd::is_enum<EnumElementType>::value && !AzTypeInfo<EnumElementType>::Uuid().IsNull();
        if (isSpecializedEnum)
        {
            CopyEnumValues<EnumElementType>(ed);
        }
        
        classElement->m_editData = ed;
        m_editElement = ed;
        ed->m_elementId = uiIdCrc;
        ed->m_name = name;
        ed->m_description = description;
        ed->m_serializeClassElement = classElement;
        return this;
    }

    //=========================================================================
    // DataElement
    //=========================================================================
    template<class T>
    EditContext::ClassInfo* EditContext::ClassInfo::DataElement(const char* uiId, T memberVariable)
    {
        using ElementTypeInfo = typename SerializeInternal::ElementInfo<T>;
        using ElementType = typename AZStd::Utils::if_c<AZStd::is_enum<typename ElementTypeInfo::Type>::value, typename ElementTypeInfo::Type, typename ElementTypeInfo::ElementType>::type;
        AZ_Assert(m_classData->m_typeId == AzTypeInfo<typename ElementTypeInfo::ClassType>::Uuid(), "Data element (%s) belongs to a different class!", AzTypeInfo<typename ElementTypeInfo::ValueType>::Name());

        const SerializeContext::ClassData* classData = m_context->m_serializeContext.FindClassData(AzTypeInfo<typename ElementTypeInfo::ValueType>::Uuid());
        if (classData && classData->m_editData)
        {
            return DataElement<T>(uiId, memberVariable, classData->m_editData->m_name, classData->m_editData->m_description);
        }
        else if (AZStd::is_enum<ElementType>::value && AzTypeInfo<ElementType>::Name() != nullptr)
        {
            auto enumIter = m_context->m_enumData.find(AzTypeInfo<ElementType>::Uuid());
            if (enumIter != m_context->m_enumData.end())
            {
                return DataElement<T>(uiId, memberVariable, enumIter->second.m_name, enumIter->second.m_description);
            }
        }
        
        const char* typeName = AzTypeInfo<typename ElementTypeInfo::ValueType>::Name();
        return DataElement<T>(uiId, memberVariable, typeName, typeName);
    }

    //=========================================================================
    // DataElement
    //=========================================================================
    template<class T>
    EditContext::ClassInfo* EditContext::ClassInfo::DataElement(Crc32 uiIdCrc, T memberVariable)
    {
        typedef typename SerializeInternal::ElementInfo<T>  ElementTypeInfo;
        AZ_Assert(m_classData->m_typeId == AzTypeInfo<typename ElementTypeInfo::ClassType>::Uuid(), "Data element (%s) belongs to a different class!", AzTypeInfo<typename ElementTypeInfo::ValueType>::Name());

        const SerializeContext::ClassData* classData = m_context->m_serializeContext.FindClassData(AzTypeInfo<typename ElementTypeInfo::ValueType>::Uuid());
        if (classData && classData->m_editData)
        {
            return DataElement<T>(uiIdCrc, memberVariable, classData->m_editData->m_name, classData->m_editData->m_description);
        }
        else
        {
            const char* typeName = AzTypeInfo<typename ElementTypeInfo::ValueType>::Name();
            return DataElement<T>(uiIdCrc, memberVariable, typeName, typeName);
        }
    }

    //=========================================================================
    // SetDynamicEditDataProvider
    //=========================================================================
    inline EditContext::ClassInfo*
    EditContext::ClassInfo::SetDynamicEditDataProvider(Edit::DynamicEditDataProvider* pHandler)
    {
        m_classElement->m_editDataProvider = pHandler;
        return this;
    }

    //=========================================================================
    // Attribute
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::Attribute(const char* id, T value)
    {
        return Attribute(Crc32(id), value);
    }

    //=========================================================================
    // Attribute
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::Attribute(Crc32 idCrc, T value)
    {
        AZ_Assert(Internal::AttributeValueTypeClassChecker<T>::Check(m_classData->m_typeId, m_classData->m_azRtti), "Attribute (0x%08x) doesn't belong to '%s' class! You can't reference other classes!", idCrc, m_classData->m_name);
        typedef typename AZStd::Utils::if_c<AZStd::is_member_pointer<T>::value,
            typename AZStd::Utils::if_c<AZStd::is_member_function_pointer<T>::value, Edit::AttributeMemberFunction<T>, Edit::AttributeMemberData<T> >::type,
            typename AZStd::Utils::if_c<AZStd::is_function<typename AZStd::remove_pointer<T>::type>::value, Edit::AttributeFunction<typename AZStd::remove_pointer<T>::type>, Edit::AttributeData<T> >::type
            >::type ContainerType;
        AZ_Assert(m_editElement, "You can attach attributes only to UiElements!");
        if (m_editElement)
        {
            // Detect adding an EnumValue attribute to an enum which is reflected globally
            const bool modifyingGlobalEnum = Internal::IsModifyingGlobalEnum(idCrc, *m_editElement);
            AZ_Error("EditContext", !modifyingGlobalEnum, "You cannot add enum values to an enum which is globally reflected");
            if (!modifyingGlobalEnum)
            {
                m_editElement->m_attributes.push_back(Edit::AttributePair(idCrc, aznew ContainerType(value)));
            }
        }
        return this;
    }

    namespace Edit
    {
        template<class EnumType>
        struct EnumConstant
        {
            AZ_TYPE_INFO(EnumConstant, "{4CDFEE70-7271-4B27-833B-F8F72AA64C40}");

            typedef typename AZStd::RemoveEnum<EnumType>::type UnderlyingType;

            EnumConstant() {}
            EnumConstant(EnumType first, const char* description)
            {
                m_value = static_cast<UnderlyingType>(first);
                m_description = description;
            }

            UnderlyingType m_value;
            AZStd::string m_description;
        };
    } // namespace Edit

    //=========================================================================
    // EnumAttribute
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::EnumAttribute(T value, const char* description)
    {
        AZ_STATIC_ASSERT(AZStd::is_enum<T>::value, "Type passed to EnumAttribute is not an enum.");
        // If the name of the element is the same as the class name, then this is the global reflection (see EditContext::Enum<E>())
        const bool isReflectedGlobally = m_editElement->m_serializeClassElement && m_editElement->m_elementId == Internal::UuidToCrc32(m_editElement->m_serializeClassElement->m_typeId);
        AZ_Error("EditContext", !isReflectedGlobally, "You cannot add enum values to an enum which is globally reflected (while reflecting %s %s)", AzTypeInfo<T>::Name(), m_editElement->m_name);
        if (!isReflectedGlobally)
        {
            const Edit::EnumConstant<T> internalValue(value, description);
            using ContainerType = Edit::AttributeData<Edit::EnumConstant<T>>;
            AZ_Assert(m_editElement, "You can attach attributes only to UiElements!");
            if (m_editElement)
            {
                m_editElement->m_attributes.push_back(Edit::AttributePair(AZ::Edit::InternalAttributes::EnumValue, aznew ContainerType(internalValue)));
            }
        }
        return this;
    }

    //=========================================================================
    // CopyEnumValues
    //=========================================================================
    template<class E>
    typename AZStd::enable_if<AZStd::is_enum<E>::value>::type
    EditContext::ClassInfo::CopyEnumValues(Edit::ElementData* ed)
    {
        AZ_STATIC_ASSERT(AZStd::is_enum<E>::value, "You cannot copy enum values for a non-enum type!");
        using ContainerType = Edit::AttributeData<Edit::EnumConstant<E>>;
        auto enumIter = m_context->m_enumData.find(AzTypeInfo<E>::Uuid());
        if (enumIter != m_context->m_enumData.end())
        {
            // have to deep copy the attribute data, since it will be deleted independently
            const Edit::ElementData& enumData = enumIter->second;
            for (const Edit::AttributePair& attr : enumData.m_attributes)
            {
                AZ_Assert(azrtti_cast<ContainerType*>(attr.second), "There is non-EnumConstant data in the global reflection of enum %s", enumData.m_name);
                Edit::AttributePair newAttr(attr.first, aznew ContainerType(static_cast<ContainerType*>(attr.second)->Get(nullptr)));
                ed->m_attributes.push_back(newAttr);
            }
        }
    }

    //=========================================================================
    // ElementAttribute
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::ElementAttribute(const char* id, T value)
    {
        AZ_Assert(Internal::AttributeValueTypeClassChecker<T>::Check(m_classData->m_typeId, m_classData->m_azRtti), "ElementAttribute (%s) doesn't belong to '%s' class! You can't reference other classes!", id, m_classData->m_name);
        return ElementAttribute(Crc32(id), value);
    }

    //=========================================================================
    // ElementAttribute
    //=========================================================================
    template<class T>
    EditContext::ClassInfo*
    EditContext::ClassInfo::ElementAttribute(Crc32 idCrc, T value)
    {
        AZ_Assert(Internal::AttributeValueTypeClassChecker<T>::Check(m_classData->m_typeId, m_classData->m_azRtti), "ElementAttribute (0x%08u) doesn't belong to '%s' class! You can't reference other classes!", idCrc, m_classData->m_name);
        typedef typename AZStd::Utils::if_c<AZStd::is_member_pointer<T>::value,
            typename AZStd::Utils::if_c<AZStd::is_member_function_pointer<T>::value, Edit::AttributeMemberFunction<T>, Edit::AttributeMemberData<T> >::type,
            typename AZStd::Utils::if_c<AZStd::is_function<typename AZStd::remove_pointer<T>::type>::value, Edit::AttributeFunction<typename AZStd::remove_pointer<T>::type>, Edit::AttributeData<T> >::type
        >::type ContainerType;
        AZ_Assert(m_editElement, "You can attach ElementAttributes only to UiElements!");
        if (m_editElement)
        {
            // Detect adding an EnumValue attribute to an enum which is reflected globally
            const bool modifyingGlobalEnum = Internal::IsModifyingGlobalEnum(idCrc, *m_editElement);
            AZ_Error("EditContext", !modifyingGlobalEnum, "You cannot add enum values to an enum which is globally reflected");
            if (!modifyingGlobalEnum)
            {
                Edit::AttributePair attribute(idCrc, aznew ContainerType(value));
                attribute.second->m_describesChildren = true;
                m_editElement->m_attributes.push_back(attribute);
            }
        }
        return this;
    }

    template <class E>
    EditContext::EnumInfo*
    EditContext::EnumInfo::Value(const char* name, E value)
    {
        AZ_STATIC_ASSERT(AZStd::is_enum<E>::value, "Only values that are part of an enum are valid as value attributes");
        AZ_STATIC_ASSERT(Internal::HasAZTypeInfo<E>::value, "Enums must have reflection type info (via AZ_TYPEINFO_SPECIALIZE or AzTypeInfo<Enum>) to be reflected globally");
        AZ_Assert(m_elementData, "Attempted to add a value attribute (%s) to a non-existent enum element data", name);
        const Edit::EnumConstant<E> internalValue(value, name);
        using ContainerType = Edit::AttributeData<Edit::EnumConstant<E>>;
        if (m_elementData)
        {
            m_elementData->m_attributes.push_back(Edit::AttributePair(AZ::Edit::InternalAttributes::EnumValue, aznew ContainerType(internalValue)));
            if (m_elementData->m_elementId == AZ::Edit::UIHandlers::Default)
            {
                m_elementData->m_elementId = AZ::Edit::UIHandlers::ComboBox;
            }
        }
        return this;
    }

}   // namespace AZ

#endif // AZCORE_EDIT_CONTEXT_H
#pragma once
