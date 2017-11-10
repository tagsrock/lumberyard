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
#ifndef AZCORE_SERIALIZE_CONTEXT_H
#define AZCORE_SERIALIZE_CONTEXT_H

#include <AzCore/Memory/SystemAllocator.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/string/string.h>

#include <AzCore/std/typetraits/is_pointer.h>
#include <AzCore/std/typetraits/is_abstract.h>
#include <AzCore/std/typetraits/remove_pointer.h>
#include <AzCore/std/typetraits/is_base_of.h>
#include <AzCore/std/any.h>

#include <AzCore/std/functional.h>

#include <AzCore/RTTI/ReflectContext.h>

#include <AzCore/Math/Crc.h>
#include <AzCore/IO/ByteContainerStream.h>

#define AZ_SERIALIZE_BINARY_STACK_BUFFER 4096

#if defined(AZ_BIG_ENDIAN)
#   define AZ_SERIALIZE_SWAP_ENDIAN(_value, _isSwap)  (void)_isSwap
#else
#   define AZ_SERIALIZE_SWAP_ENDIAN(_value, _isSwap)   if (_isSwap) AZStd::endian_swap(_value)
#endif

namespace AZ
{
    class EditContext;

    class ObjectStream;
    class GenericClassInfo;

    namespace ObjectStreamInternal
    {
        class ObjectStreamImpl;
    }

    namespace Edit
    {
        struct ElementData;
        struct ClassData;
    }

    namespace Serialize
    {
        /**
        * Template to hold a single global instance of a particular type. Keep in mind that this is not DLL safe
        * get the variable from the Environment if store globals with state, currently this is not the case.
        * \ref AllocatorInstance for example with environment as we share allocators state across DLLs
        */
        template<class T>
        struct StaticInstance
        {
            static T s_instance;
        };
        template<class T>
        T StaticInstance<T>::s_instance;
    }

    /**
     * Serialize context is a class that manages information
     * about all reflected data structures. You will use it
     * for all related information when you declare you data
     * for serialization. In addition it will handle data version control.
     */
    class SerializeContext
        : public ReflectContext
    {
        friend class EditContext;
        class ClassInfo;

        static const unsigned int VersionClassDeprecated = (unsigned int)-1;

    public:
        class ClassData;
        struct ClassElement;
        struct DataElement;
        class DataElementNode;
        class IObjectFactory;
        // Alias for backwards compatibility
        using IRttiHelper = AZ::IRttiHelper;
        class IDataSerializer;
        class IDataContainer;
        class IEventHandler;

        AZ_CLASS_ALLOCATOR(SerializeContext, SystemAllocator, 0);
        AZ_RTTI(SerializeContext, "{83482F97-84DA-4FD4-BF9E-7FE34C8E091F}", ReflectContext);

        /// Callback to process data conversion.
        typedef bool(* VersionConverter)(SerializeContext& /*context*/, DataElementNode& /* elements to convert */);

        /// Callback for persistent ID for an instance \note: We should probably switch this to UUID
        typedef u64(* ClassPersistentId)(const void* /*class instance*/);

        /// Callback to manipulate entity saving on a yes/no base, otherwise you will need provide serializer for more advanced logic.
        typedef bool(* ClassDoSave)(const void* /*class instance*/);

        // \todo bind allocator to serialize allocator
        typedef AZStd::unordered_map<Uuid, ClassData> UuidToClassMap;

        /// If registerIntegralTypes is true we will register the default serializer for all integral types.
        SerializeContext(bool registerIntegralTypes = true, bool createEditContext = false);
        virtual ~SerializeContext();

        /// Create an edit context for this serialize context. If one already exist it will be returned.
        EditContext*    CreateEditContext();
        /// Destroys the internal edit context, all edit related data will be freed. You don't have call DestroyEditContext, it's called when you destroy the serialize context.
        void            DestroyEditContext();
        /// Returns the pointer to the current edit context or NULL if one was not created.
        EditContext*    GetEditContext() const;

        /**
        * \anchor SerializeBind
        * \name Code to bind classes and variables for serialization.
        * For details about what can you expose in a class and what options you have \ref SerializeContext::ClassInfo
        * @{
        */
        template<class T>
        ClassInfo       Class();
        template<class T, class B1>
        ClassInfo       Class();
        template<class T, class B1, class B2>
        ClassInfo       Class();
        template<class T, class B1, class B2, class B3>
        ClassInfo       Class();

        /**
         * When a default object factory can't be used, example a singleton class with private constructors or non default constructors
         * you will have to provide a custom factory.
         */
        template<class T>
        ClassInfo       Class(IObjectFactory* factory);
        template<class T, class B1>
        ClassInfo       Class(IObjectFactory* factory);
        template<class T, class B1, class B2>
        ClassInfo       Class(IObjectFactory* factory);
        template<class T, class B1, class B2, class B3>
        ClassInfo       Class(IObjectFactory* factory);

        // Deprecate a previously reflected class so that on load any instances will be silently dropped without the need
        // to keep the original class around.
        // Use of this function should be considered temporary and should be removed as soon as possible.
        void            ClassDeprecate(const char* name, const AZ::Uuid& typeUuid, VersionConverter converter = nullptr);
        // @}

        /**
         * Debug stack element when we enumerate a class hierarchy so we can report better errors!
         */
        struct DbgStackEntry
        {
            void  ToString(AZStd::string& str) const;
            const void*         m_dataPtr;
            const Uuid*         m_uuidPtr;
            const ClassData*    m_classData;
            const char*         m_elementName;
            const ClassElement* m_classElement;
        };

        class ErrorHandler
        {
        public:

            AZ_CLASS_ALLOCATOR(ErrorHandler, AZ::SystemAllocator, 0);

            ErrorHandler()
                : m_nErrors(0)
                , m_nWarnings(0)
            {
            }

            /// Report an error within the system
            void            ReportError(const char* message);
            /// Report a non-fatal warning within the system
            void            ReportWarning(const char* message);
            /// Pushes an entry onto debug stack.
            void            Push(const DbgStackEntry& de);
            /// Pops the last entry from debug stack.
            void            Pop();
            /// Get the number of errors reported.
            unsigned int    GetErrorCount() const           { return m_nErrors; }
            /// Get the number of warnings reported.
            unsigned int    GetWarningCount() const         { return m_nWarnings; }
            /// Reset error counter
            void            Reset();

        private:

            AZStd::string GetStackDescription() const;

            typedef AZStd::vector<DbgStackEntry>    DbgStack;
            DbgStack        m_stack;
            unsigned int    m_nErrors;
            unsigned int    m_nWarnings;
        };

        /**
        * \anchor Enumeration
        * \name Function to enumerate the hierarchy of an instance using the serialization info.
        * @{
        */
        enum EnumerationAccessFlags
        {
            ENUM_ACCESS_FOR_READ    = 0,        // you only need read access to the enumerated data
            ENUM_ACCESS_FOR_WRITE   = 1 << 0,   // you need write access to the enumerated data
            ENUM_ACCESS_HOLD        = 1 << 1    // you want to keep accessing the data after enumeration completes. you are responsible for notifying the data owner when you no longer need access.
        };

        /// EnumerateInstance() calls this callback for each node in the instance hierarchy. Return true to also enumerate the node's children, otherwise false.
        typedef AZStd::function< bool (void* /* instance pointer */, const ClassData* /* classData */, const ClassElement* /* classElement*/) > BeginElemEnumCB;
        /// EnumerateInstance() calls this callback when enumeration of an element's sub-branch has completed. Return true to continue enumeration of the element's siblings, otherwise false.
        typedef AZStd::function< bool () > EndElemEnumCB;

        /**
         * Call this function to traverse an instance's hierarchy by providing address and classId, if you have the typed pointer you can just call \ref EnumerateObject
         * \param ptr pointer to the object for traversal
         * \param classId classId of object for traversal
         * \param beginElemCB callback when we begin/open a child element
         * \param endElemCB callback when we end/close a child element
         * \param accessFlags \ref EnumerationAccessFlags
         * \param classData pointer to the class data for the traversed object to avoid calling FindClassData(classId) (can be null)
         * \param classElement pointer to class element (null for root elements)
         * \param errorHandler optional pointer to the error handler.
         */
        bool EnumerateInstanceConst(const void* ptr, const Uuid& classId, const BeginElemEnumCB& beginElemCB, const EndElemEnumCB& endElemCB, unsigned int accessFlags, const ClassData* classData, const ClassElement* classElement, ErrorHandler* errorHandler = nullptr);
        bool EnumerateInstance(void* ptr, const Uuid& classId, const BeginElemEnumCB& beginElemCB, const EndElemEnumCB& endElemCB, unsigned int accessFlags, const ClassData* classData, const ClassElement* classElement, ErrorHandler* errorHandler = nullptr);
        /// Traverses a give object \ref EnumerateInstance assuming that object is a root element (not classData/classElement information).
        template<class T>
        bool EnumerateObject(T* obj, const BeginElemEnumCB& beginElemCB, const EndElemEnumCB& endElemCB, unsigned int accessFlags, ErrorHandler* errorHandler = nullptr);

        /// TypeInfo enumeration callback. Return true to continue enumeration, otherwise false.
        typedef AZStd::function< bool(const ClassData* /*class data*/, const Uuid& /* typeId used only when RTTI is triggered, 0 the rest of the time */) > TypeInfoCB;
        /// Enumerate all derived classes of classId type in addition we use the typeId to check any rtti possible match.
        void EnumerateDerived(const TypeInfoCB& callback, const Uuid& classId = Uuid::CreateNull(), const Uuid& typeId = Uuid::CreateNull());
        /// Enumerate all base classes of classId type.
        void EnumerateBase(const TypeInfoCB& callback, const Uuid& classId);
        template<class T>
        void EnumerateDerived(const TypeInfoCB& callback);
        template<class T>
        void EnumerateBase(const TypeInfoCB& callback);
        // @}

        /// Makes a copy of obj. The behavior is the same as if obj was first serialized out and the copy was then created from the serialized data.
        template<class T>
        T* CloneObject(const T* obj);
        void* CloneObject(const void* ptr, const Uuid& classId);

        /**
         * Class element. When a class doesn't have a direct serializer,
         * he is an aggregation of ClassElements (which can be another classes).
         */
        struct ClassElement
        {
            enum Flags
            {
                FLG_POINTER             = (1 << 0),       ///< Element is stored as pointer (it's not a value).
                FLG_BASE_CLASS          = (1 << 1),       ///< Set if the element is a base class of the holding class.
                FLG_NO_DEFAULT_VALUE    = (1 << 2),       ///< Set if the class element can't have a default value.
                FLG_DYNAMIC_FIELD       = (1 << 3),       ///< Set if the class element represents a dynamic field (DynamicSerializableField::m_data).
            };

            const char* m_name;                     ///< Used in XML output and debugging purposes
            u32 m_nameCrc;                          ///< CRC32 of m_name
            Uuid m_typeId;
            size_t m_dataSize;
            size_t m_offset;

            IRttiHelper* m_azRtti;                  ///< Interface used to support RTTI.
            GenericClassInfo* m_genericClassInfo = nullptr;   ///< Valid when the generic class is set. So you don't search for the actual type in the class register.
            Edit::ElementData* m_editData;          ///< Pointer to edit data (generated by EditContext).

            int m_flags;    ///<
        };
        typedef AZStd::vector<ClassElement> ClassElementArray;

        /**
         * Class Data contains the data/info for each registered class
         * all if it members (their offsets, etc.), creator, version converts, etc.
         */
        class ClassData
        {
        public:
            ClassData();
            template<class T>
            static ClassData Create(const char* name, const Uuid& typeUuid, IObjectFactory* factory, IDataSerializer* serializer = nullptr, IDataContainer* container = nullptr);

            bool    IsDeprecated() const { return m_version == VersionClassDeprecated; }

            /// Find the persistence id (check base classes) \todo this is a TEMP fix, analyze and cache that information in the class
            ClassPersistentId GetPersistentId(const SerializeContext& context) const;

            const char*         m_name;
            Uuid                m_typeId;
            unsigned int        m_version;          ///< Data version (by default 0)
            VersionConverter    m_converter;        ///< Data version converter, a static member that should not need an instance to convert it's data.
            IObjectFactory*     m_factory;          ///< Interface for object creation.
            ClassPersistentId   m_persistentId;     ///< Function to retrieve class instance persistent Id.
            ClassDoSave         m_doSave;           ///< Function what will choose to Save or not an instance.
            IDataSerializer*    m_serializer;       ///< Interface for actual data serialization. If this is not NULL you can't have m_elements != NULL
            IEventHandler*      m_eventHandler;     ///< Optional interface for Event notification (start/stop serialization, etc.)

            IDataContainer*     m_container;        ///< Interface if this class represents a data container. Data will be accessed using this interface.
            IRttiHelper*        m_azRtti;           ///< Interface used to support RTTI. Set internally based on type provided to Class<T>.

            Edit::ClassData*    m_editData;         ///< Edit data for the class display.
            ClassElementArray   m_elements;         ///< Sub elements. If this is not empty m_serializer should be NULL (there is no point to have sub-elements, if we can serialize the entire class).
        };

        /**
         * Interface for creating and detroying object from the serializer.
         */
        class IObjectFactory
        {
        public:

            virtual ~IObjectFactory() {}

            /// Called to create an instance of an object.
            virtual void* Create(const char* name) = 0;

            /// Called to destroy an instance of an object
            virtual void  Destroy(void* ptr) = 0;
        };

        /**
         * Interface for data serialization. Should be implemented for lowest level
         * of data. Once this implementation is detected, the class will not be drilled
         * down. We will assume this implementation covers the full class.
         */
        class IDataSerializer
        {
        public:
            virtual ~IDataSerializer() {}

            /// Store the class data into a stream.
            virtual size_t  Save(const void* classPtr, IO::GenericStream& stream, bool isDataBigEndian = false) = 0;

            /// Load the class data from a stream.
            virtual bool    Load(void* classPtr, IO::GenericStream& stream, unsigned int version, bool isDataBigEndian = false) = 0;

            /// Convert binary data to text.
            virtual size_t  DataToText(IO::GenericStream& in, IO::GenericStream& out, bool isDataBigEndian /*= false*/) = 0;

            /// Convert text data to binary, to support loading old version formats. We must respect text version if the text->binary format has changed!
            virtual size_t  TextToData(const char* text, unsigned int textVersion, IO::GenericStream& stream, bool isDataBigEndian = false) = 0;

            /// Compares two instances of the type.
            /// \return true if they match.
            /// Note: Input pointers are assumed to point to valid instances of the class.
            virtual bool    CompareValueData(const void* lhs, const void* rhs) = 0;
        };

        /**
         * Helper for directly comparing two instances of a given type.
         * Intended for use in implementations of IDataSerializer::CompareValueData.
         */
        template<typename T>
        struct EqualityCompareHelper
        {
            static bool CompareValues(const void* lhs, const void* rhs)
            {
                const T* dataLhs = reinterpret_cast<const T*>(lhs);
                const T* dataRhs = reinterpret_cast<const T*>(rhs);
                return (*dataLhs == *dataRhs);
            }
        };

        /**
         * Interface for a data container. This might be an AZStd container or just a class with
         * elements defined in some template manner (usually with templates :) )
         */
        class IDataContainer
        {
        public:
            typedef AZStd::function< bool (void* /* instance pointer */, const Uuid& /*elementClassId*/, const ClassData* /* elementGenericClassData */, const ClassElement* /* genericClassElement */) > ElementCB;
            virtual ~IDataContainer() {}

            /// Return default element generic name (used by most containers).
            static inline const char*   GetDefaultElementName()     { return "element"; }
            /// Return default element generic name crc (used by most containers).
            static inline u32 GetDefaultElementNameCrc()            { return AZ_CRC("element", 0x41405e39); }
            /// Returns the generic element (offsets are mostly invalid 0xbad0ffe0, there are exceptions). Null if element with this name can't be found.
            virtual const ClassElement* GetElement(u32 elementNameCrc) const = 0;
            /// Populates the supplied classElement by looking up the name in the DataElement. Returns true if the classElement was populated successfully
            virtual bool GetElement(ClassElement& classElement, const DataElement& dataElement) const = 0;
            /// Enumerate elements in the array.
            virtual void    EnumElements(void* instance, const ElementCB& cb) = 0;
            /// Return number of elements in the container.
            virtual size_t  Size(void* instance) const = 0;
            /// Returns the capacity of the container. Returns 0 for objects without fixed capacity.
            virtual size_t  Capacity(void* instance) const = 0;
            /// Returns true if elements pointers don't change on add/remove. If false you MUST enumerate all elements.
            virtual bool    IsStableElements() const = 0;
            /// Returns true if the container is fixed size (not capacity), otherwise false.
            virtual bool    IsFixedSize() const = 0;
            /// Returns if the container is fixed capacity, otherwise false
            virtual bool    IsFixedCapacity() const = 0;
            /// Returns true if the container represents a smart pointer.
            virtual bool    IsSmartPointer() const = 0;
            /// Returns true if elements can be retrieved by index.
            virtual bool    CanAccessElementsByIndex() const = 0;
            /// Reserve an element and get it's address (called before the element is loaded).
            virtual void*   ReserveElement(void* instance, const ClassElement* classElement) = 0;
            /// Get an element's address by its index (called before the element is loaded).
            virtual void*   GetElementByIndex(void* instance, const ClassElement* classElement, size_t index) = 0;
            /// Store the element that was reserved before (called post loading)
            virtual void    StoreElement(void* instance, void* element) = 0;
            /// Remove element in the container. Returns true if the element was removed, otherwise false. If deletePointerDataContext is NOT null, this indicated that you want the remove function to delete/destroy any Elements that are pointer!
            virtual bool    RemoveElement(void* instance, const void* element, SerializeContext* deletePointerDataContext) = 0;
            /**
             * Remove elements (removed array of elements) regardless if the container is Stable or not (IsStableElements).
             * Element should be sorted on address in acceding order. Returns number of elements removed.
             * If deletePointerDataContext is NOT null, this indicates that you want the remove function to delete/destroy any Elements that are pointer,
             */
            virtual size_t  RemoveElements(void* instance, const void** elements, size_t numElements, SerializeContext* deletePointerDataContext) = 0;
            /// Clear elements in the instance. If deletePointerDataContext is NOT null, this indicated that you want the remove function to delete/destroy any Elements that are pointer!
            virtual void    ClearElements(void* instance, SerializeContext* deletePointerDataContext) = 0;
            /// Called when elements inside the container have been modified.
            virtual void    ElementsUpdated(void* instance) { (void)instance; }

        protected:
            /// Free element data (when the class elements are pointers).
            void DeletePointerData(SerializeContext* context, const ClassElement* classElement, void* element);
        };

        /**
         * Serialize class events.
         * IMPORTANT: Serialize events can be called from serialization thread(s). So all functions should be thread safe.
         */
        class IEventHandler
        {
        public:
            virtual ~IEventHandler() {}

            /// Called right before we start reading from the instance pointed by classPtr.
            virtual void OnReadBegin(void* classPtr) { (void)classPtr; }
            /// Called after we are done reading from the instance pointed by classPtr.
            virtual void OnReadEnd(void* classPtr) { (void)classPtr; }

            /// Called right before we start writing to the instance pointed by classPtr.
            virtual void OnWriteBegin(void* classPtr) { (void)classPtr; }
            /// Called after we are done writing to the instance pointed by classPtr.
            virtual void OnWriteEnd(void* classPtr) { (void)classPtr; }
        };

        /**
         * \anchor VersionControl
         * \name Version Control
         * @{
         */

        /**
         * Represents an element in the tree of serialization data.
         * Each element contains metadata about itself and (possibly) a data value.
         * An element representing an int will have a data value, but an element
         * representing a vector or class will not (their contents are stored in sub-elements).
         */
        struct DataElement
        {
            DataElement();
            ~DataElement();
            DataElement(const DataElement& rhs);
            DataElement& operator = (const DataElement& rhs);
#ifdef AZ_HAS_RVALUE_REFS
            DataElement(DataElement&& rhs);
            DataElement& operator = (DataElement&& rhs);
#endif
            enum DataType
            {
                DT_TEXT,        ///< data points to a string representation of the data
                DT_BINARY,      ///< data points to a binary representation of the data in native endian
                DT_BINARY_BE,   ///< data points to a binary representation of the data in big endian
            };

            const char*     m_name;         ///< Name of the parameter, they must be unique with in the scope of the class.
            u32             m_nameCrc;      ///< CRC32 of name
            DataType        m_dataType;     ///< What type of data, if we have any.
            Uuid            m_id;           ///< Reference ID, the meaning can change depending on what are we referencing.
            unsigned int    m_version;  ///< Version of data in the stream. This can be the current version or older. Newer version will be handled internally.
            size_t          m_dataSize; ///< Size of the data pointed by "data" in bytes.
            Uuid            m_specializedId = AZ::Uuid::CreateNull(); ////< Specialized type id from the GenericClassInfo structure if it exist for the Class Element that this Data Element represents

            AZStd::vector<char> m_buffer; ///< Local buffer used by the ByteContainerStream when the DataElement needs to own the data
            IO::ByteContainerStream<AZStd::vector<char> > m_byteStream; ///< Stream used when the DataElement needs to own the data.

            IO::GenericStream* m_stream; ///< Pointer to the stream that holds the element's data, it may point to m_byteStream.
        };

        /**
         * Represents a node in the tree of serialization data.
         * Holds a DataElement describing itself and a list of sub nodes.
         * For example, a class would be represented as a parent node
         * with its member variables in sub nodes.
         */
        class DataElementNode
        {
            friend class ObjectStreamInternal::ObjectStreamImpl;
            friend class DataOverlayTarget;

        public:
            DataElementNode()
                : m_classData(nullptr) {}

            /**
             * Get/Set data work only on leaf nodes (classes which implement serializers)
             */
            template<typename T>
            bool GetData(T& value);
            template<typename T>
            bool GetChildData(u32 childNameCrc, T& value);
            template<typename T>
            bool SetData(SerializeContext& sc, const T& value, ErrorHandler* errorHandler = nullptr);

            /**
             * Converts current DataElementNode from one type to another.
             * Keep in mind that if the new "type" has sub-elements (not leaf - serialized element)
             * you will need to add those elements after calling convert.
             */
            bool Convert(SerializeContext& sc, const char* name, const Uuid& id);
            bool Convert(SerializeContext& sc, const Uuid& id);
            template<typename T>
            bool Convert(SerializeContext& sc, const char* name);
            template<typename T>
            bool Convert(SerializeContext& sc);

            DataElement&        GetRawDataElement()             { return m_element; }
            const DataElement&  GetRawDataElement() const       { return m_element; }
            u32                 GetName() const                 { return m_element.m_nameCrc; }
            const char*         GetNameString() const           { return m_element.m_name; }
            void                SetName(const char* newName);
            unsigned int        GetVersion() const              { return m_element.m_version; }
            const Uuid&         GetId() const                   { return m_element.m_id; }

            int                 GetNumSubElements() const       { return static_cast<int>(m_subElements.size()); }
            DataElementNode&    GetSubElement(int index)        { return m_subElements[index]; }
            int                 FindElement(u32 crc);
            DataElementNode*    FindSubElement(u32 crc);
            void                RemoveElement(int index);
            bool                RemoveElementByName(u32 crc);
            int                 AddElement(const DataElementNode& elem);
            int                 AddElement(SerializeContext& sc, const char* name, const Uuid& id);
            int                 AddElement(SerializeContext& sc, const char* name, const ClassData& classData);
            template<typename T>
            int                 AddElement(SerializeContext& sc, const char* name);
            /// \returns index of the replaced element (index) if replaced, otherwise -1
            template<typename T>
            int                 AddElementWithData(SerializeContext& sc, const char* name, const T& dataToSet);
            int                 ReplaceElement(SerializeContext& sc, int index, const char* name, const Uuid& id);
            template<typename T>
            int                 ReplaceElement(SerializeContext& sc, int index, const char* name);

        protected:
            typedef AZStd::vector<DataElementNode> NodeArray;

            bool SetDataHierarchy(SerializeContext& sc, const void* objectPtr, const Uuid& classId, ErrorHandler* errorHandler = nullptr);

            DataElement         m_element; ///< Serialization data for this element.
            const ClassData*    m_classData; ///< Reflected ClassData for this element.
            NodeArray           m_subElements; ///< Nodes of sub elements.
        };
        // @}

        /// Find a class data (stored information) based on a class ID and possible parent class data.
        const ClassData* FindClassData(const Uuid& classId, const SerializeContext::ClassData* parent = nullptr, u32 elementNameCrc = 0) const;

        /// Find GenericClassData data based on the supplied class ID
        GenericClassInfo* FindGenericClassInfo(const Uuid& classId) const; 

        /// Find GenericClassData data based on the supplied class ID
        AZStd::any CreateAny(const Uuid& classId) const;


        /**
         * Checks if a type can be downcast to another using Uuid and AZ_RTTI.
         * All classes must be registered with the system.
         */
        bool  CanDowncast(const Uuid& fromClassId, const Uuid& toClassId, const IRttiHelper* fromClassHelper = nullptr, const IRttiHelper* toClassHelper = nullptr) const;

        /**
         * Offsets a pointer from a derived class to a common base class
         * All classes must be registered with the system otherwise a NULL
         * will be returned.
         */
        void* DownCast(void* instance, const Uuid& fromClassId, const Uuid& toClassId, const IRttiHelper* fromClassHelper = nullptr, const IRttiHelper* toClassHelper = nullptr) const;

        /**
         * Casts a pointer using the instance pointer and its class id.
         * In order for the cast to succeed, both types must either support azrtti or be reflected
         * with the serialization context.
         */
        template<class T>
        T Cast(void* instance, const Uuid& instanceClassId) const;

    private:
        /**
         * Generic enumerate function can can take both 'const void*' and 'void*' data pointer types.
         */
        template<class PtrType, class EnumType>
        bool EnumerateInstanceTempl(PtrType ptr, const Uuid& classId, EnumType beginElemCB, const SerializeContext::EndElemEnumCB& endElemCB, const ClassData* classData, const char* elementName, const ClassElement* classElement);

        /// Enumerate function called to enumerate an azrtti hierarchy
        static void EnumerateBaseRTTIEnumCallback(const Uuid& id, void* userData);

        /// Remove class data
        void RemoveClassData(ClassData* classData);

        /// Object cloning callbacks.
        bool BeginCloneElement(void* ptr, const ClassData* classData, const ClassElement* elementData, void* stackData, ErrorHandler* errorHandler);
        bool EndCloneElement(void* stackData);

        /**
         * Internal structure to maintain class information while we are describing a class.
         * User should call variety of functions to describe class features and data.
         * example:
         *  struct MyStruct {
         *      int m_data };
         *
         * expose for serialization
         *  serializeContext->Class<MyStruct>()
         *      ->Version(3,&MyVersionConverter)
         *      ->Field("data",&MyStruct::m_data);
         */
        class ClassInfo
        {
            friend class SerializeContext;
            ClassInfo(SerializeContext* context, const UuidToClassMap::iterator& classMapIter)
                : m_context(context)
                , m_classData(classMapIter)
            {}
            SerializeContext*           m_context;
            UuidToClassMap::iterator    m_classData;
        public:
            ~ClassInfo() {}
            ClassInfo* operator->()     { return this; }

            /// Declare class field with address of the variable in the class
            template<class ClassType, class FieldType>
            ClassInfo*  Field(const char* name, FieldType ClassType::* address);

            /**
             * Declare a class field that belongs to a base class of the class. If you use this function to reflect you base class
             * you can't reflect the entire base class.
             */
            template<class ClassType, class BaseType, class FieldType>
            SerializeContext::ClassInfo* FieldFromBase(const char* name, FieldType BaseType::* address);

            /// Add version control to the structure with optional converter. If not controlled a version of 0 is assigned.
            ClassInfo*  Version(unsigned int version, VersionConverter converter = nullptr);

            /// For data types (usually base types) or types that we handle the full serialize, implement this interface.
            ClassInfo*  Serializer(IDataSerializer* serializer);

            /// Helper function to create a static instance of specific serializer implementation. \ref Serializer(IDataSerializer*)
            template<typename SerializerImplementation>
            ClassInfo*  Serializer()
            {
                return Serializer(&Serialize::StaticInstance<SerializerImplementation>::s_instance);
            }

            /// For class type that are empty, we want the serializer to create on load, but have no child elements.
            ClassInfo*  SerializerForEmptyClass();

            /**
             * Implement and provide interface for event handling when necessary.
             * An example for this will be when the class does some thread work and need to know when
             * we are about to access data for serialization (load/save).
             */
            ClassInfo*  EventHandler(IEventHandler* eventHandler);

            /// Helper function to create a static instance of specific event handler implementation. \ref Serializer(IDataSerializer*)
            template<typename EventHandlerImplementation>
            ClassInfo*  EventHandler()
            {
                return EventHandler(&Serialize::StaticInstance<EventHandlerImplementation>::s_instance);
            }

            /// Adds a DataContainer structure for manipulating contained data in custom ways
            ClassInfo*  DataContainer(IDataContainer* dataContainer);

            /// Helper function to create a static instance of the specific data container implementation. \ref DataContainer(IDataContainer*)
            template<typename DataContainerType>
            ClassInfo*  DataContainer()
            {
                return DataContainer(&Serialize::StaticInstance<DataContainerType>::s_instance);
            }

            /**
             * Sets the class persistent ID function getter. This is used when we store the class in containers, so we can identify elements
             * for the purpose of overriding values (aka slices overrides), otherwise the ability to tweak complex types in containers will be limiting
             * due to the lack of reliable ways to address elements in a container.
             */
            ClassInfo*  PersistentId(ClassPersistentId persistentId);

            /**
             * Provide a function to perform basic yes/no on saving. We don't recommend using such pattern as
             * it's very easy to create asymmetrical serialization and lose silently data.
             */
            ClassInfo*  SerializerDoSave(ClassDoSave isSave);
        };


        EditContext*    m_editContext;  ///< Pointer to optional edit context.
        UuidToClassMap  m_uuidMap;      ///< Map for all class in this serialize context
        AZStd::unordered_map<Uuid, GenericClassInfo*>  m_uuidGenericMap;      ///< Uuid to ClassData map of reflected classes with GenericTypeInfo
        
        using CreateAnyFunc = AZStd::any(*)();
        AZStd::unordered_map<Uuid, CreateAnyFunc>  m_uuidAnyCreationMap;      ///< Uuid to Any creation function map
    };

    /**
    * Base class that will provide various information for a generic entry.
    *
    * The difference between Class id and Specialized id:
    * Class Id represents the Uuid which is stored in the SerializeContext::ClassData structure.
    * It is key value used in the SerializeContext::m_uuidMap for looking up the ClassData in the
    * SerializeContext::FindClassData function
    * Specialized Id represents the Uuid of any class that specializes the SerializeGenericTypeInfo template.
    * By default for non-templated classes the Specialized Id is the Uuid returned by AzTypeInfo<ValueType>::Uuid.
    * For specialized classes the Specialized Id is normally made up of the concatenation of the Template Class Uuid
    * and the Template Arguments Uuids using a SHA-1.
    */
    class GenericClassInfo
    {
    public:
        virtual ~GenericClassInfo() {}

        /// Return the generic class "class Data" undependent from the underlaying templates
        virtual SerializeContext::ClassData* GetClassData() = 0;
        /// Return the generic class "
        //virtual const Uuid& GetClassTypeId() = 0;

        virtual size_t GetNumTemplatedArguments() = 0;

        virtual const Uuid& GetTemplatedTypeId(size_t element) = 0;

        /// By default returns AzTypeInfo<ValueType>::Uuid
        virtual const Uuid& GetSpecializedTypeId() = 0;
    };

    /**
     * When it comes down to template classes you have 2 choices for serialization.
     * You can register the fully specialized type (like MyClass<int>) and use it this
     * way. This is the most flexible and recommended way.
     * But often when you use template containers, pool, helpers in a big project...
     * it might be tricky to preregister all template specializations and make sure
     * there are no duplications.
     * This is why we allow to specialize a template and by doing so providing a generic
     * type, that will apply to all specializations, but it will be used on template use
     * (registered with the "Field" function). While this works fine, it's a little limiting
     * as it relies that on the fact that each class field should have unique name (which is
     * required by the system, to properly identify an element anyway). At the moment
     */
    template<class ValueType>
    struct SerializeGenericTypeInfo
    {
        /// By default we don't have generic class info
        static GenericClassInfo* GetGenericInfo() { return nullptr; }
        /// By default just return the ValueTypeInfo
        static const Uuid& GetClassTypeId();
    };

    /**
    Helper structure to allow the creation of a function pointer for creating AZStd::any objects
    It takes advantage of type erasure to allow a mapping of Uuids, to AZStd(*)() function pointers
    with needing to store the template T
    */
    template<typename ValueType, bool = !AZStd::is_abstract<ValueType>::value && AZStd::Internal::template_is_copy_constructible<ValueType>::value>
    struct AnyTypeInfoConcept
    {
        static AZStd::any CreateAny()
        {
            return AZStd::any(ValueType());
        }
    };

    template<typename ValueType>
    struct AnyTypeInfoConcept<ValueType, false>
    {
        static AZStd::any CreateAny()
        {
            return{};
        }
    };

    /*
     * Helper class to retrieve Uuids and perform AZRtti queries.
     * This helper uses AZRtti when available, and does what it can when not.
     * It automatically resolves pointer-to-pointers to their value types.
     * When type is available, use the static functions provided by this helper
     * directly (ex: SerializeTypeInfo<T>::GetUuid()).
     * Call GetRttiHelper<T>() to retrieve an IRttiHelper interface
     * for AZRtti-enabled types while type info is still available, so it
     * can be used for queries after type info is lost.
     */
    template<typename T>
    struct SerializeTypeInfo
    {
        typedef typename AZStd::remove_pointer<T>::type ValueType;
        static const Uuid& GetUuid(const T* instance = nullptr)
        {
            return GetUuid(instance, typename AZStd::is_pointer<T>::type());
        }
        static const Uuid& GetUuid(const T* instance, const AZStd::true_type& /*is_pointer<T>*/)
        {
            return GetUuidHelper(instance ? *instance : nullptr, typename HasAZRtti<ValueType>::type());
        }
        static const Uuid& GetUuid(const T* instance, const AZStd::false_type& /*is_pointer<T>*/)
        {
            return GetUuidHelper(instance, typename HasAZRtti<ValueType>::type());
        }
        static const char* GetRttiTypeName(ValueType* const* instance)
        {
            return GetRttiTypeName(instance ? *instance : nullptr, typename HasAZRtti<ValueType>::type());
        }
        static const char* GetRttiTypeName(const ValueType* instance)
        {
            return GetRttiTypeName(instance, typename HasAZRtti<ValueType>::type());
        }
        static const char* GetRttiTypeName(const ValueType* instance, const AZStd::true_type& /*HasAZRtti<ValueType>*/)
        {
            return instance ? instance->RTTI_GetTypeName() : ValueType::RTTI_TypeName();
        }
        static const char* GetRttiTypeName(const ValueType* /*instance*/, const AZStd::false_type& /*HasAZRtti<ValueType>*/)
        {
            return "NotAZRttiType";
        }

        static const Uuid& GetRttiTypeId(ValueType* const* instance)
        {
            return GetRttiTypeId(instance ? *instance : nullptr, typename HasAZRtti<ValueType>::type());
        }
        static const Uuid& GetRttiTypeId(const ValueType* instance) { return GetRttiTypeId(instance, typename HasAZRtti<ValueType>::type()); }
        static const Uuid& GetRttiTypeId(const ValueType* instance, const AZStd::true_type& /*HasAZRtti<ValueType>*/)
        {
            return instance ? instance->RTTI_GetType() : ValueType::RTTI_Type();
        }
        static const Uuid& GetRttiTypeId(const ValueType* /*instance*/, const AZStd::false_type& /*HasAZRtti<ValueType>*/)
        {
            static Uuid s_nullUuid = Uuid::CreateNull();
            return s_nullUuid;
        }

        static bool IsRttiTypeOf(const Uuid& id, const AZStd::true_type& /*HasAZRtti<ValueType>*/)
        {
            return ValueType::RTTI_IsContainType(id);
        }
        static bool IsRttiTypeOf(const Uuid& /*id*/, const AZStd::false_type& /*HasAZRtti<ValueType>*/)
        {
            return false;
        }

        static void RttiEnumHierarchy(RTTI_EnumCallback callback, void* userData, const AZStd::true_type& /*HasAZRtti<ValueType>*/)
        {
            return ValueType::RTTI_EnumHierarchy(callback, userData);
        }
        static void RttiEnumHierarchy(RTTI_EnumCallback /*callback*/, void* /*userData*/, const AZStd::false_type& /*HasAZRtti<ValueType>*/)
        {
        }

        // const pointers
        static const void* RttiCast(ValueType* const* instance, const Uuid& asType)
        {
            return RttiCast(instance ? *instance : nullptr, asType, typename HasAZRtti<ValueType>::type());
        }
        static const void* RttiCast(const ValueType* instance, const Uuid& asType)
        {
            return RttiCast(instance, asType, typename HasAZRtti<ValueType>::type());
        }
        static const void* RttiCast(const ValueType* instance, const Uuid& asType, const AZStd::true_type& /*HasAZRtti<ValueType>*/)
        {
            return instance ? instance->RTTI_AddressOf(asType) : nullptr;
        }
        static const void* RttiCast(const ValueType* instance, const Uuid& /*asType*/, const AZStd::false_type& /*HasAZRtti<ValueType>*/)
        {
            return instance;
        }
        // non cost pointers
        static void* RttiCast(ValueType** instance, const Uuid& asType)
        {
            return RttiCast(instance ? *instance : nullptr, asType, typename HasAZRtti<ValueType>::type());
        }
        static void* RttiCast(ValueType* instance, const Uuid& asType)
        {
            return RttiCast(instance, asType, typename HasAZRtti<ValueType>::type());
        }
        static void* RttiCast(ValueType* instance, const Uuid& asType, const AZStd::true_type& /*HasAZRtti<ValueType>*/)
        {
            return instance ? instance->RTTI_AddressOf(asType) : nullptr;
        }
        static void* RttiCast(ValueType* instance, const Uuid& /*asType*/, const AZStd::false_type& /*HasAZRtti<ValueType>*/)
        {
            return instance;
        }

    private:
        static const AZ::Uuid& GetUuidHelper(const ValueType* /* ptr */, const AZStd::false_type& /* HasAZRtti<U>::type() */)
        {
            return SerializeGenericTypeInfo<ValueType>::GetClassTypeId();
        }
        static const AZ::Uuid& GetUuidHelper(const ValueType* ptr, const AZStd::true_type& /* HasAZRtti<U>::type() */)
        {
            if (ptr)
            {
                return ptr->RTTI_GetType();
            }
            return SerializeGenericTypeInfo<ValueType>::GetClassTypeId();
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    // Implementations
    //
    namespace Serialize
    {
        /// Default instance factory to create/destroy a classes (when a factory is not provided)
        template<class T, bool U = HasAZClassAllocator<T>::value, bool A = AZStd::is_abstract<T>::value >
        struct InstanceFactory
            : public SerializeContext::IObjectFactory
        {
            void* Create(const char* name) override
            {
                (void)name;
                return aznewex(name) T;
            }
            void Destroy(void* ptr) override
            {
                delete reinterpret_cast<T*>(ptr);
            }
        };

        /// Default instance for classes without AZ_CLASS_ALLOCATOR (can't use aznew) defined.
        template<class T>
        struct InstanceFactory<T, false, false>
            : public SerializeContext::IObjectFactory
        {
            void* Create(const char* name) override
            {
                return new(azmalloc(sizeof(T), AZStd::alignment_of<T>::value, AZ::SystemAllocator, name))T;
            }
            void Destroy(void* ptr) override
            {
                reinterpret_cast<T*>(ptr)->~T();
                azfree(ptr, AZ::SystemAllocator, sizeof(T), AZStd::alignment_of<T>::value);
            }
        };

        /// Default instance for abstract classes. We can't instantiate abstract classes, but we have this function for assert!
        template<class T, bool U>
        struct InstanceFactory<T, U, true>
            : public SerializeContext::IObjectFactory
        {
            void* Create(const char* name) override
            {
                (void)name;
                AZ_Assert(false, "Can't instantiate abstract classs %s", name);
                return nullptr;
            }
            void Destroy(void* ptr) override
            {
                delete reinterpret_cast<T*>(ptr);
            }
        };
    }

    namespace SerializeInternal
    {
        template<class T>
        struct ElementInfo;

        template<class T, class C>
        struct ElementInfo<T C::*>
        {
            typedef typename AZStd::RemoveEnum<T>::type ElementType;
            typedef C ClassType;
            typedef T Type;
            typedef typename AZStd::remove_pointer<ElementType>::type ValueType;
        };

        template<class Derived, class Base>
        size_t GetBaseOffset()
        {
            Derived* der = reinterpret_cast<Derived*>(AZ_INVALID_POINTER);
            return reinterpret_cast<char*>(static_cast<Base*>(der)) - reinterpret_cast<char*>(der);
        }
    } // namespace SerializeInternal

    //=========================================================================
    // Cast
    // [02/08/2013]
    //=========================================================================
    template<class T>
    T SerializeContext::Cast(void* instance, const Uuid& instanceClassId) const
    {
        void* ptr = DownCast(instance, instanceClassId, SerializeTypeInfo<T>::GetUuid());
        return reinterpret_cast<T>(ptr);
    }

    //=========================================================================
    // EnumerateObject
    // [10/31/2012]
    //=========================================================================
    template<class T>
    bool SerializeContext::EnumerateObject(T* obj, const BeginElemEnumCB& beginElemCB, const EndElemEnumCB& endElemCB, unsigned int accessFlags, ErrorHandler* errorHandler)
    {
        AZ_Assert(!AZStd::is_pointer<T>::value, "Cannot enumerate pointer-to-pointer as root element! This makes no sense!");

        // we will just const cast so we don't have to deal with two versions of everything
        void* classPtr = SerializeTypeInfo<T>::RttiCast(obj, SerializeTypeInfo<T>::GetRttiTypeId(obj));
        const Uuid& classId = SerializeTypeInfo<T>::GetUuid(obj);
        return EnumerateInstance(classPtr, classId, beginElemCB, endElemCB, accessFlags, nullptr, nullptr, errorHandler);
    }

    //=========================================================================
    // CloneObject
    // [03/21/2014]
    //=========================================================================
    template<class T>
    T* SerializeContext::CloneObject(const T* obj)
    {
        const void* classPtr = SerializeTypeInfo<T>::RttiCast(obj, SerializeTypeInfo<T>::GetRttiTypeId(obj));
        const Uuid& classId = SerializeTypeInfo<T>::GetUuid(obj);
        return reinterpret_cast<T*>(CloneObject(classPtr, classId));
    }

    //=========================================================================
    // EnumerateDerived
    // [11/13/2012]
    //=========================================================================
    template<class T>
    inline void SerializeContext::EnumerateDerived(const TypeInfoCB& callback)
    {
        EnumerateDerived(callback, AzTypeInfo<T>::Uuid(), AzTypeInfo<T>::Uuid());
    }

    //=========================================================================
    // EnumerateBase
    // [11/13/2012]
    //=========================================================================
    template<class T>
    inline void SerializeContext::EnumerateBase(const TypeInfoCB& callback)
    {
        EnumerateBase(callback, AzTypeInfo<T>::Uuid());
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T>
    SerializeContext::ClassInfo
    SerializeContext::Class()
    {
        return Class<T>(&Serialize::StaticInstance<Serialize::InstanceFactory<T> >::s_instance);
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T, class B1>
    SerializeContext::ClassInfo
    SerializeContext::Class()
    {
        AZ_STATIC_ASSERT(!(AZStd::is_same<T, B1>::value), "You cannot reflect a type as its own base");
        return Class<T, B1>(&Serialize::StaticInstance<Serialize::InstanceFactory<T> >::s_instance);
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T, class B1, class B2>
    SerializeContext::ClassInfo
    SerializeContext::Class()
    {
        AZ_STATIC_ASSERT(!(AZStd::is_same<T, B1>::value), "You cannot reflect a type as its own base");
        AZ_STATIC_ASSERT(!(AZStd::is_same<T, B2>::value), "You cannot reflect a type as its own base");
        return Class<T, B1, B2>(&Serialize::StaticInstance<Serialize::InstanceFactory<T> >::s_instance);
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T, class B1, class B2, class B3>
    SerializeContext::ClassInfo
    SerializeContext::Class()
    {
        AZ_STATIC_ASSERT(!(AZStd::is_same<T, B1>::value), "You cannot reflect a type as its own base");
        AZ_STATIC_ASSERT(!(AZStd::is_same<T, B2>::value), "You cannot reflect a type as its own base");
        AZ_STATIC_ASSERT(!(AZStd::is_same<T, B3>::value), "You cannot reflect a type as its own base");
        return Class<T, B1, B2, B3>(&Serialize::StaticInstance<Serialize::InstanceFactory<T> >::s_instance);
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T>
    SerializeContext::ClassInfo
    SerializeContext::Class(IObjectFactory* factory)
    {
        const char* name = AzTypeInfo<T>::Name();
        const Uuid& typeUuid = AzTypeInfo<T>::Uuid();
        if (IsRemovingReflection())
        {
            auto mapIt = m_uuidMap.find(typeUuid);
            if (mapIt != m_uuidMap.end())
            {
                RemoveClassData(&mapIt->second);
                m_uuidMap.erase(mapIt);
            }
            return ClassInfo(this, m_uuidMap.end());
        }
        else
        {
            UuidToClassMap::pair_iter_bool result = m_uuidMap.insert(AZStd::make_pair(typeUuid, ClassData::Create<T>(name, typeUuid, factory)));
            AZ_Assert(result.second, "This class type %s has already been registered", name /*,typeUuid.ToString()*/);
            m_uuidAnyCreationMap.emplace(SerializeTypeInfo<T>::GetUuid(), &AnyTypeInfoConcept<T>::CreateAny);
            return ClassInfo(this, result.first);
        }
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T, class B1>
    SerializeContext::ClassInfo
    SerializeContext::Class(IObjectFactory* factory)
    {
        const char* name = AzTypeInfo<T>::Name();
        const Uuid& typeUuid = AzTypeInfo<T>::Uuid();
        if (IsRemovingReflection())
        {
            auto mapIt = m_uuidMap.find(typeUuid);
            if (mapIt != m_uuidMap.end())
            {
                RemoveClassData(&mapIt->second);
                m_uuidMap.erase(mapIt);
            }
            return ClassInfo(this, m_uuidMap.end());
        }
        else
        {
            UuidToClassMap::pair_iter_bool result = m_uuidMap.insert(AZStd::make_pair(typeUuid, ClassData::Create<T>(name, typeUuid, factory)));
            AZ_Assert(result.second, "This class type %s has already been registered", name /*,typeUuid.ToString()*/);

            ClassData& cd = result.first->second;
            m_uuidAnyCreationMap.emplace(SerializeTypeInfo<T>::GetUuid(), &AnyTypeInfoConcept<T>::CreateAny);

            ClassElement ed;
            // base class
            ed.m_name = "BaseClass1";
            ed.m_nameCrc = AZ_CRC("BaseClass1", 0xd4925735);
            ed.m_flags = ClassElement::FLG_BASE_CLASS;
            ed.m_dataSize = sizeof(B1);
            ed.m_typeId = azrtti_typeid<B1>();
            ed.m_offset = SerializeInternal::GetBaseOffset<T, B1>();
            ed.m_genericClassInfo = SerializeGenericTypeInfo<B1>::GetGenericInfo();
            ed.m_azRtti = GetRttiHelper<B1>();
            ed.m_editData = nullptr;
            cd.m_elements.push_back(ed);
            return ClassInfo(this, result.first);
        }
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T, class B1, class B2>
    SerializeContext::ClassInfo
    SerializeContext::Class(IObjectFactory* factory)
    {
        const char* name = AzTypeInfo<T>::Name();
        const Uuid& typeUuid = AzTypeInfo<T>::Uuid();
        if (IsRemovingReflection())
        {
            auto mapIt = m_uuidMap.find(typeUuid);
            if (mapIt != m_uuidMap.end())
            {
                RemoveClassData(&mapIt->second);
                m_uuidMap.erase(mapIt);
            }
            return ClassInfo(this, m_uuidMap.end());
        }
        else
        {
            UuidToClassMap::pair_iter_bool result = m_uuidMap.insert(AZStd::make_pair(typeUuid, ClassData::Create<T>(name, typeUuid, factory)));
            AZ_Assert(result.second, "This class type %s has already been registered", name /*,typeUuid.ToString()*/);

            ClassData& cd = result.first->second;
            m_uuidAnyCreationMap.emplace(SerializeTypeInfo<T>::GetUuid(), &AnyTypeInfoConcept<T>::CreateAny);

            ClassElement ed;
            // base class
            ed.m_name = "BaseClass1";
            ed.m_nameCrc = AZ_CRC("BaseClass1", 0xd4925735);
            ed.m_flags = ClassElement::FLG_BASE_CLASS;
            ed.m_dataSize = sizeof(B1);
            ed.m_typeId = SerializeTypeInfo<B1>::GetUuid();
            ed.m_offset = SerializeInternal::GetBaseOffset<T, B1>();
            ed.m_genericClassInfo = SerializeGenericTypeInfo<B1>::GetGenericInfo();
            ed.m_azRtti = GetRttiHelper<B1>();
            ed.m_editData = NULL;
            cd.m_elements.push_back(ed);

            ed.m_name = "BaseClass2";
            ed.m_nameCrc = AZ_CRC("BaseClass2", 0x4d9b068f);
            ed.m_flags = ClassElement::FLG_BASE_CLASS;
            ed.m_dataSize = sizeof(B2);
            ed.m_typeId = SerializeTypeInfo<B2>::GetUuid();
            ed.m_offset = SerializeInternal::GetBaseOffset<T, B2>();
            ed.m_genericClassInfo = SerializeGenericTypeInfo<B2>::GetGenericInfo();
            ed.m_azRtti = GetRttiHelper<B2>();
            ed.m_editData = NULL;
            cd.m_elements.push_back(ed);
            return ClassInfo(this, result.first);
        }
    }

    //=========================================================================
    // Class
    // [10/31/2012]
    //=========================================================================
    template<class T, class B1, class B2, class B3>
    SerializeContext::ClassInfo
    SerializeContext::Class(IObjectFactory* factory)
    {
        const char* name = AzTypeInfo<T>::Name();
        const Uuid& typeUuid = AzTypeInfo<T>::Uuid();
        if (IsRemovingReflection())
        {
            auto mapIt = m_uuidMap.find(typeUuid);
            if (mapIt != m_uuidMap.end())
            {
                RemoveClassData(&mapIt->second);
                m_uuidMap.erase(mapIt);
            }
            return ClassInfo(this, m_uuidMap.end());
        }
        else
        {
            UuidToClassMap::pair_iter_bool result = m_uuidMap.insert(AZStd::make_pair(typeUuid, ClassData::Create<T>(name, typeUuid, factory)));
            AZ_Assert(result.second, "This class type %s has already been registered", name /*,typeUuid.ToString()*/);

            ClassData& cd = result.first->second;
            m_uuidAnyCreationMap.emplace(SerializeTypeInfo<T>::GetUuid(), &AnyTypeInfoConcept<T>::CreateAny);

            ClassElement ed;
            // base class
            ed.m_name = "BaseClass1";
            ed.m_nameCrc = AZ_CRC("BaseClass1", 0xd4925735);
            ed.m_flags = ClassElement::FLG_BASE_CLASS;
            ed.m_dataSize = sizeof(B1);
            ed.m_typeId = SerializeTypeInfo<B1>::GetUuid();
            ed.m_offset = SerializeInternal::GetBaseOffset<T, B1>();
            ed.m_genericClassInfo = SerializeGenericTypeInfo<B1>::GetGenericInfo();
            ed.m_azRtti = GetRttiHelper<B1>();
            ed.m_editData = NULL;
            cd.m_elements.push_back(ed);

            ed.m_name = "BaseClass2";
            ed.m_nameCrc = AZ_CRC("BaseClass2", 0x4d9b068f);
            ed.m_flags = ClassElement::FLG_BASE_CLASS;
            ed.m_dataSize = sizeof(B2);
            ed.m_typeId = SerializeTypeInfo<B2>::GetUuid();
            ed.m_offset = SerializeInternal::GetBaseOffset<T, B2>();
            ed.m_genericClassInfo = SerializeGenericTypeInfo<B2>::GetGenericInfo();
            ed.m_azRtti = GetRttiHelper<B2>();
            ed.m_editData = NULL;
            cd.m_elements.push_back(ed);

            ed.m_name = "BaseClass3";
            ed.m_nameCrc = AZ_CRC("BaseClass3", 0x3a9c3619);
            ed.m_flags = ClassElement::FLG_BASE_CLASS;
            ed.m_dataSize = sizeof(B3);
            ed.m_typeId = SerializeTypeInfo<B3>::GetUuid();
            ed.m_offset = SerializeInternal::GetBaseOffset<T, B3>();
            ed.m_genericClassInfo = SerializeGenericTypeInfo<B3>::GetGenericInfo();
            ed.m_azRtti = GetRttiHelper<B3>();
            ed.m_editData = NULL;
            cd.m_elements.push_back(ed);
            return ClassInfo(this, result.first);
        }
    }

    //=========================================================================
    // ClassInfo::Field
    // [10/31/2012]
    //=========================================================================
    template<class ClassType, class FieldType>
    SerializeContext::ClassInfo* SerializeContext::ClassInfo::Field(const char* name, FieldType ClassType::* member)
    {
        typedef typename AZStd::RemoveEnum<FieldType>::type ElementType;
        typedef typename AZStd::remove_pointer<ElementType>::type ValueType;

        if (m_context->IsRemovingReflection())
        {
            return this; // we have already removed the class data for this class
        }

        AZ_Assert(!m_classData->second.m_serializer,
            "Class %s has a custom serializer, and can not have additional fields. Classes can either have a custom serializer or child fields.",
            name);

        AZ_Assert(m_classData->second.m_typeId == AzTypeInfo<ClassType>::Uuid(),
            "Field %s is serialized with class %s, but belongs to class %s. If you are trying to expose base class field use FieldFromBase",
            name,
            m_classData->second.m_name,
            AzTypeInfo<ClassType>::Name());

        ClassElement ed;
        ed.m_name = name;
        ed.m_nameCrc = AZ::Crc32(name);
        // Not really portable but works for the supported compilers. It will crash and not work if we have virtual inheritance. Detect and assert at compile time about it. (something like is_virtual_base_of)
        ed.m_offset = reinterpret_cast<size_t>(&(reinterpret_cast<ClassType const volatile*>(0)->*member));
        //ed.m_offset = or pass it to the function with offsetof(typename ElementTypeInfo::ClassType,member);
        ed.m_dataSize = sizeof(ElementType);
        ed.m_flags = AZStd::is_pointer<ElementType>::value ? ClassElement::FLG_POINTER : 0;
        ed.m_editData = nullptr;
        ed.m_azRtti = GetRttiHelper<ValueType>();

        ed.m_genericClassInfo = SerializeGenericTypeInfo<ValueType>::GetGenericInfo();
        ed.m_typeId = SerializeGenericTypeInfo<ValueType>::GetClassTypeId();
        AZ_Assert(!ed.m_typeId.IsNull(), "You must provide a valid class id for class %s", name);

        if (ed.m_genericClassInfo)
        {
            m_context->m_uuidGenericMap.emplace(ed.m_genericClassInfo->GetSpecializedTypeId(), ed.m_genericClassInfo);
            m_context->m_uuidAnyCreationMap.emplace(ed.m_genericClassInfo->GetSpecializedTypeId(), &AnyTypeInfoConcept<ValueType>::CreateAny);
        }

        m_classData->second.m_elements.push_back(ed);
        return this;
    }

    //=========================================================================
    // ClassInfo::FieldFromBase
    //=========================================================================
    template<class ClassType, class BaseType, class FieldType>
    SerializeContext::ClassInfo* SerializeContext::ClassInfo::FieldFromBase(const char* name, FieldType BaseType::* member)
    {
        AZ_STATIC_ASSERT((AZStd::is_base_of<BaseType, ClassType>::value), "Classes BaseType and ClassType are unrelated. BaseType should be base class for ClassType");

        if (m_context->IsRemovingReflection())
        {
            return this; // we have already removed the class data for this class
        }

        AZ_Assert(!m_classData->second.m_serializer,
            "Class %s has a custom serializer, and can not have additional fields. Classes can either have a custom serializer or child fields.",
            name);

        // make sure the class have not been reflected.
        for (ClassElement& element : m_classData->second.m_elements)
        {
            if (element.m_flags & ClassElement::FLG_BASE_CLASS)
            {
                AZ_Assert(element.m_typeId != AzTypeInfo<BaseType>::Uuid(),
                    "You can not reflect %s as base class of %s with Class<%s,%s> and then reflect the some of it's fields with FieldFromBase! Either use FieldFromBase or just reflect the entire base class!",
                    AzTypeInfo<BaseType>::Name(), AzTypeInfo<ClassType>::Name(), AzTypeInfo<ClassType>::Name(), AzTypeInfo<BaseType>::Name()
                    );
            }
            else
            {
                break; // base classes are sorted at the start
            }
        }

        return Field<ClassType>(name, static_cast<FieldType ClassType::*>(member));
    }

    //=========================================================================
    // DataElementNode::GetData
    // [10/31/2012]
    //=========================================================================
    template<typename T>
    bool SerializeContext::DataElementNode::GetData(T& value)
    {
        const ClassData* classData = m_classData;

        // check generic types
        if (!classData)
        {
            GenericClassInfo* genericInfo = SerializeGenericTypeInfo<T>::GetGenericInfo();
            if (genericInfo)
            {
                classData = genericInfo->GetClassData();
            }
        }

        if (classData && classData->m_serializer)
        {
            if (classData->m_typeId == m_element.m_id)
            {
                if (m_element.m_dataSize == 0)
                {
                    // we allow to have 0 bytes written, this is valid "save" state.
                    // If we want to be even "more" correct we can add IsAllowZero to the IDataSerializer interface
                    // to explicitly state if this is allowed, so the return will look like
                    // return classData->m_serializer->CanSaveZeroBytes();
                    return true;
                }
                else
                {
                    if (m_element.m_dataType == DataElement::DT_TEXT)
                    {
                        // convert to binary so we can load the data
                        AZStd::string text;
                        text.resize_no_construct(m_element.m_dataSize);
                        m_element.m_byteStream.Read(text.size(), reinterpret_cast<void*>(text.data()));
                        m_element.m_byteStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        m_element.m_dataSize = classData->m_serializer->TextToData(text.c_str(), m_element.m_version, m_element.m_byteStream);
                        m_element.m_byteStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        m_element.m_dataType = DataElement::DT_BINARY;
                    }

                    bool isLoaded = classData->m_serializer->Load(&value, m_element.m_byteStream, m_element.m_version, m_element.m_dataType == DataElement::DT_BINARY_BE);
                    m_element.m_byteStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN); // reset stream position
                    return isLoaded;
                }
            }
        }

        return false;
    }

    //=========================================================================
    // DataElementNode::GetData
    // [10/31/2012]
    //=========================================================================
    template<typename T>
    bool SerializeContext::DataElementNode::GetChildData(u32 childNameCrc, T& value)
    {
        int dataElementIndex = FindElement(childNameCrc);
        if (dataElementIndex != -1)
        {
            return GetSubElement(dataElementIndex).GetData(value);
        }
        return false;
    }

    //=========================================================================
    // DataElementNode::SetData
    // [10/31/2012]
    //=========================================================================
    template<typename T>
    bool SerializeContext::DataElementNode::SetData(SerializeContext& sc, const T& value, ErrorHandler* errorHandler)
    {
        const Uuid& classTypeId = SerializeGenericTypeInfo<T>::GetClassTypeId();

        if (classTypeId == m_element.m_id)
        {
            const ClassData* classData = m_classData;

            // check generic types
            if (!classData)
            {
                GenericClassInfo* genericInfo = SerializeGenericTypeInfo<T>::GetGenericInfo();
                if (genericInfo)
                {
                    classData = genericInfo->GetClassData();
                }
            }

            if (classData && classData->m_serializer && (classData->m_doSave == nullptr || classData->m_doSave(&value)))
            {
                AZ_Assert(m_element.m_byteStream.GetCurPos() == 0, "We have to be in the beginning of the stream, as it should be for current element only!");
                m_element.m_dataSize = classData->m_serializer->Save(&value, m_element.m_byteStream);
                m_element.m_byteStream.Truncate();
                m_element.m_byteStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN); // reset stream position
                m_element.m_stream = &m_element.m_byteStream;

                m_element.m_dataType = DataElement::DT_BINARY;
                return true;
            }
            else
            {
                return SetDataHierarchy(sc, &value, classTypeId, errorHandler);
            }
        }

        if (errorHandler)
        {
            const AZStd::string errorMessage =
                AZStd::string::format("Specified class type {%s} does not match current element %s with type {%s}.",
                    classTypeId.ToString<AZStd::string>().c_str(), m_element.m_name, m_element.m_id.ToString<AZStd::string>().c_str());
            errorHandler->ReportError(errorMessage.c_str());
        }

        return false;
    }

    //=========================================================================
    // DataElementNode::Convert
    //=========================================================================
    template<class T>
    bool SerializeContext::DataElementNode::Convert(SerializeContext& sc)
    {
        // remove sub elements
        while (!m_subElements.empty())
        {
            RemoveElement(static_cast<int>(m_subElements.size()) - 1);
        }

        // replace element
        m_element.m_id = SerializeGenericTypeInfo<T>::GetClassTypeId();
        m_element.m_dataSize = 0;
        m_element.m_buffer.clear();
        m_element.m_stream = &m_element.m_byteStream;

        GenericClassInfo* genericClassInfo = SerializeGenericTypeInfo<T>::GetGenericInfo();
        if (genericClassInfo)
        {
            m_classData = genericClassInfo->GetClassData();
        }
        else
        {
            UuidToClassMap::iterator it = sc.m_uuidMap.find(m_element.m_id);
            AZ_Assert(it != sc.m_uuidMap.end(), "You are adding element to an unregistered class!");
            m_classData = &it->second;
        }
        m_element.m_version = m_classData->m_version;

        return true;
    }

    //=========================================================================
    // DataElementNode::Convert
    //=========================================================================
    template<class T>
    bool SerializeContext::DataElementNode::Convert(SerializeContext& sc, const char* name)
    {
        AZ_Assert(name != NULL && strlen(name) > 0, "Empty name is an INVALID element name!");
        u32 nameCrc = Crc32(name);

#if defined(AZ_DEBUG_BUILD)
        if (FindElement(nameCrc) != -1)
        {
            AZ_Error("Serialize", false, "We already have a class member %s!", name);
            return false;
        }
#endif // AZ_DEBUG_BUILD

        // remove sub elements
        while (!m_subElements.empty())
        {
            RemoveElement(static_cast<int>(m_subElements.size()) - 1);
        }

        // replace element
        m_element.m_name = name;
        m_element.m_nameCrc = nameCrc;
        m_element.m_id = SerializeGenericTypeInfo<T>::GetClassTypeId();
        m_element.m_dataSize = 0;
        m_element.m_buffer.clear();
        m_element.m_stream = &m_element.m_byteStream;

        GenericClassInfo* genericClassInfo = SerializeGenericTypeInfo<T>::GetGenericInfo();
        if (genericClassInfo)
        {
            m_classData = genericClassInfo->GetClassData();
        }
        else
        {
            UuidToClassMap::iterator it = sc.m_uuidMap.find(m_element.m_id);
            AZ_Assert(it != sc.m_uuidMap.end(), "You are adding element to an unregistered class!");
            m_classData = &it->second;
        }
        m_element.m_version = m_classData->m_version;

        return true;
    }

    //=========================================================================
    // DataElementNode::AddElement
    // [10/31/2012]
    //=========================================================================
    template<typename T>
    int SerializeContext::DataElementNode::AddElement(SerializeContext& sc, const char* name)
    {
        AZ_Assert(name != nullptr && strlen(name) > 0, "Empty name in an INVALID element name!");
        u32 nameCrc = Crc32(name);

#if defined(AZ_DEBUG_BUILD)
        if (FindElement(nameCrc) != -1 && !m_classData->m_container)
        {
            AZ_Error("Serialize", false, "We already have a class member %s!", name);
            return -1;
        }
#endif // AZ_DEBUG_BUILD

        DataElementNode node;
        node.m_element.m_name = name;
        node.m_element.m_nameCrc = nameCrc;
        node.m_element.m_id = SerializeGenericTypeInfo<T>::GetClassTypeId();

        GenericClassInfo* genericClassInfo = SerializeGenericTypeInfo<T>::GetGenericInfo();
        if (genericClassInfo)
        {
            node.m_classData = genericClassInfo->GetClassData();
        }
        else
        {
            // if we are NOT a generic container
            UuidToClassMap::iterator it = sc.m_uuidMap.find(node.m_element.m_id);
            AZ_Assert(it != sc.m_uuidMap.end(), "You are adding element to an unregistered class!");
            node.m_classData = &it->second;
        }

        node.m_element.m_version = node.m_classData->m_version;

        m_subElements.push_back(node);
        return static_cast<int>(m_subElements.size() - 1);
    }

    //=========================================================================
    // DataElementNode::AddElement
    // [7/29/2016]
    //=========================================================================
    template<typename T>
    int SerializeContext::DataElementNode::AddElementWithData(SerializeContext& sc, const char* name, const T& dataToSet)
    {
        int retVal = AddElement<T>(sc, name);
        if (retVal != -1)
        {
            m_subElements[retVal].SetData<T>(sc, dataToSet);
        }
        return retVal;
    }

    //=========================================================================
    // DataElementNode::AddElement
    // [10/31/2012]
    //=========================================================================
    template<typename T>
    int SerializeContext::DataElementNode::ReplaceElement(SerializeContext& sc, int index, const char* name)
    {
        DataElementNode& node = m_subElements[index];

        if (node.Convert<T>(sc, name))
        {
            return index;
        }
        else
        {
            return -1;
        }
    }

    //=========================================================================
    // ClassData::ClassData
    // [10/31/2012]
    //=========================================================================
    template<class T>
    SerializeContext::ClassData SerializeContext::ClassData::Create(const char* name, const Uuid& typeUuid, IObjectFactory* factory, IDataSerializer* serializer, IDataContainer* container)
    {
        ClassData cd;
        cd.m_name = name;
        cd.m_typeId = typeUuid;
        cd.m_version = 0;
        cd.m_converter = nullptr;
        cd.m_serializer = serializer;
        cd.m_factory = factory;
        cd.m_persistentId = nullptr;
        cd.m_doSave = nullptr;
        cd.m_eventHandler = nullptr;
        cd.m_container = container;
        cd.m_azRtti = GetRttiHelper<T>();
        cd.m_editData = nullptr;
        return cd;
    }

    //=========================================================================
    // SerializeGenericTypeInfo<ValueType>::GetClassTypeId
    //=========================================================================
    template<class ValueType>
    const Uuid& SerializeGenericTypeInfo<ValueType>::GetClassTypeId()
    {
        return AzTypeInfo<typename AZStd::RemoveEnum<ValueType>::type>::Uuid();
    };
}   // namespace AZ

/// include AZStd containers generics
#include <AzCore/Serialization/AZStdContainers.inl>

/// include asset generics
#include <AzCore/Asset/AssetSerializer.h>

/// include AZStd any serializer support
#include <AzCore/Serialization/AZStdAnyDataContainer.inl>

#endif // AZCORE_SERIALIZE_CONTEXT_H
#pragma once
