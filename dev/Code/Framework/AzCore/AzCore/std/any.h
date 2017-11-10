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

#include <AzCore/RTTI/RTTI.h>

#include <AzCore/std/functional.h>
#include <AzCore/std/typetraits/add_const.h>
#include <AzCore/std/typetraits/remove_reference.h>
#include <AzCore/std/typetraits/add_pointer.h>
#include <AzCore/std/typetraits/void_t.h>
#include <AzCore/std/typetraits/is_abstract.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace AZStd
{
    namespace Internal
    {
        // Size of small buffer optimization buffer
        enum { ANY_SBO_BUF_SIZE = 32 };

        //Workaround to get around issue that is_copy_constructible always returns true for containers 
        //http://stackoverflow.com/questions/18404108/issue-with-is-copy-constructible
        template<typename ClassType, typename = AZStd::void_t<>>
        struct template_is_copy_constructible
        {
            using type = typename AZStd::is_copy_constructible<ClassType>::type;
            static const bool value = AZStd::is_copy_constructible<ClassType>::value;
        };

        template<template<typename...> class ClassTemplate, typename... TemplateArgs>
        struct template_is_copy_constructible<ClassTemplate<TemplateArgs...>, typename AZStd::make_void<typename ClassTemplate<TemplateArgs...>::value_type>::type>
        {
            using ValueType = typename ClassTemplate<TemplateArgs...>::value_type;
            static const bool value = AZStd::is_copy_constructible<ClassTemplate<TemplateArgs...>>::value && AZStd::is_copy_constructible<ValueType>::value;
            using type = AZStd::conditional<value, AZStd::true_type, AZStd::false_type>;
        };
    }

    /**
     * The class any describes a type-safe container for single values of any type.
     *
     * See C++ Standard proposal n3804 (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3804.html)
     * And docs (http://en.cppreference.com/w/cpp/utility/any)
     */
    class any
    {
    public:
        AZ_TYPE_INFO(any, "{03924488-C7F4-4D6D-948B-ABC2D1AE2FD3}");
        AZ_CLASS_ALLOCATOR(any, AZ::SystemAllocator, 0);

        /// Typed used to identify other types (acquired via azrtti_typeid<Type>())
        using type_id = AZ::Uuid;

        // Actions available for performing via m_typeInfo.m_handler
        enum class Action
        {
            // dest: The any to reserve memory in. Memory for ValueType will be reserved. source: unused
            Reserve,
            // dest: The any to copy into. If using heap space, \ref Reserve must have already been called. source: The any to copy from.
            Copy,
            // dest: The any to move into. If using heap space, \ref Reserve must have already been called. source: The any to move from (NOTE: const is removed from source when calling Move).
            Move,
            // dest: The any to destroy. The dtor of the stored object is called, heap space is freed, and buffer is 0'ed. source: unused
            Destroy,
        };

        // Information about the type \ref any is storing.
        // NOTE: Building a custom any_type_info is not recommended! Do so only if you absolutely know what you're doing, and why
        struct type_info
        {
            // Helper for type-specific operations
            using HandleFnT = AZStd::function<void(Action action, any* dest, const any* source)>;

            // The id of the type stored
            type_id m_id = type_id::CreateNull();
            // This is where the magic happens
            HandleFnT m_handler = nullptr;
            // Is the type store a pointer to type referenced by m_ids
            bool m_isPointer = false;
            // Should the object be stored on the heap? (never true for pointers)
            bool m_useHeap = false;
        };

        /// Constructs an empty object.
        any(allocator alloc = allocator("AZStd::any"))
            : m_allocator(alloc)
        {
            memset(m_buffer, 0, AZ_ARRAY_SIZE(m_buffer));
        }

        /// Copies content of val into a new instance.
        any(const any& val)
            : any(val.m_allocator)
        {
            copy_from(val);
        }

        /// Moves content of val into a new instance.
        any(any&& val)
            : any(val.m_allocator)
        {
            move_from(move(val));
        }

        /// [Extension] Create an any with a custom typeinfo (copies value in pointer using typeInfo)
        any(void* pointer, const type_info& typeInfo, const AZStd::allocator& alloc)
        {
            // Create temp any, so that we may move the value given
            any temp;
            temp.m_pointer = pointer;
            temp.m_typeInfo = typeInfo;
            temp.m_allocator = alloc;

            // Call copy-from so that we don't try to clear temp
            copy_from(temp, Action::Move);

            // Make sure temp doesn't try to clear memory (we don't own it)
            temp.m_typeInfo = type_info();
        }

        /// Constructs an object with initial content an object of type decay_t<ValueType>, direct-initialized from forward<ValueType>(val)
        template <typename ValueType, typename = enable_if_t<!is_same<decay_t<ValueType>, any>::value>>
        explicit any(ValueType&& val, allocator alloc = allocator("AZStd::any"))
            : any(alloc)
        {
            AZ_STATIC_ASSERT(Internal::template_is_copy_constructible<decay_t<ValueType>>::value
                        ||  (std::is_rvalue_reference<ValueType>::value && std::is_move_constructible<decay_t<ValueType>>::value), "ValueType must be copy constructable or a movable rvalue ref.");

            // Initialize typeinfo from the type given
            m_typeInfo = create_template_type_info<decay_t<ValueType>>();

            // Reserve heap space if necessary
            m_typeInfo.m_handler(Action::Reserve, this, nullptr);

            // Call copy constructor 
            construct<decay_t<ValueType>>(this, forward<ValueType>(val));
        }

        /// Destroys the contained object, if any, as if by a call to clear()
        ~any() { clear(); }

        /// Copies content of val into this (via any(val).swap(*this))
        any& operator=(const any& val)
        {
            any(val).swap(*this);
            return *this;
        }

        /// Moves content of val into this (via any(move(val)).swap(*this))
        any& operator=(any&& val)
        {
            any(move(val)).swap(*this);
            return *this;
        }

        /// Assigns the type and value of rhs (via any(forward<ValueType>(val)).swap(*this))
        template <typename ValueType, typename = enable_if_t<!is_same<decay_t<ValueType>, any>::value>>
        any& operator=(ValueType&& val)
        {
            any(forward<ValueType>(val)).swap(*this);
            return *this;
        }

        /// If not empty, destroys the contained object.
        void clear()
        {
            // If currently storing a value, destruct it and free it's memory
            if (!empty())
            {
                m_typeInfo.m_handler(Action::Destroy, this, nullptr);
                m_typeInfo = type_info();
            }
        }

        /// Swaps the content of two any objects.
        void swap(any& other)
        {
            any tmp;

            tmp.move_from(move(*this));
            move_from(move(other));
            other.move_from(move(tmp));
        }

        /// Returns true if this hasn't been initialized yet or has been cleared
        bool empty() const { return type().IsNull(); }
        /// Returns the type id of the stored type
        type_id type() const { return m_typeInfo.m_id; }
        /// Helper for checking type equality
        template <typename ValueType>
        bool is() const { return type() == azrtti_typeid<ValueType>() && m_typeInfo.m_isPointer == is_pointer<ValueType>::value; }

        const AZStd::any::type_info& get_type_info() const { return m_typeInfo; }

    private:
        // Any elements must be copy constructible
        template<typename ValueType, typename ParamType> // ParamType must be deduced to allow universal ref
        static void construct(AZStd::any* dest, ParamType&& value, AZStd::enable_if_t<!AZStd::is_abstract<ValueType>::value && Internal::template_is_copy_constructible<ValueType>::value, AZStd::true_type*> = nullptr /* AZStd::is_copy_constructible<ValueType>::value*/)
        {
            new (dest->get_data()) ValueType(forward<ParamType>(value));
        }

        // The ValueType is not copy constructible in this case
        template<typename ValueType, typename ParamType>
        static void construct(AZStd::any* dest, ParamType&&, AZStd::enable_if_t<AZStd::is_abstract<ValueType>::value || !Internal::template_is_copy_constructible<ValueType>::value, AZStd::false_type*> = nullptr /* !AZStd::is_copy_constructible<ValueType>::value*/)
        {
            AZ_Assert(false, "Contained type %s is not copyable. Any will not attempt to invoke copy constructor", dest->get_type_info().m_id.ToString<AZStd::string>().data());
        }

        // Templated implementation of HandleFnT.
        template <typename ValueType>
        static void action_handler(Action action, any* dest, const any* source)
        {
            AZ_Assert(dest, "Internal error: dest must never be nullptr!");

            switch (action)
            {
            case Action::Reserve:
                {
                    AZ_Assert(source == nullptr, "Internal error: Reserve called with non-nullptr source.");

                    // If doing small buffer optimization, no need to validate anything
                    if (dest->m_typeInfo.m_useHeap)
                    {
                        // Allocate space for object on heap
                        dest->m_pointer = dest->m_allocator.allocate(sizeof(ValueType), alignment_of<ValueType>::value);
                    }
                    break;
                }
            case Action::Copy:
                {
                    AZ_Assert(source && source->is<ValueType>(), "Internal error: passed wrong ValueType to actionHandler.");

                    // Get const reference to value stored in source
                    const ValueType& sourceVal = *reinterpret_cast<const ValueType*>(source->get_data());
                    // Pass as argument to copy constructor on dest
                    construct<ValueType>(dest, sourceVal);
                    break;
                }
            case Action::Move:
                {
                    AZ_Assert(source && source->is<ValueType>(), "Internal error: passed wrong ValueType to actionHandler.");

                    // Get reference to value stored in source
                    ValueType& sourceVal = *reinterpret_cast<ValueType*>(const_cast<void*>(source->get_data()));
                    // Pass as argument to move constructor on dest
                    construct<ValueType>(dest, move(sourceVal));
                    break;
                }
            case Action::Destroy:
                {
                    AZ_Assert(source == nullptr, "Internal error: Destroy called with non-nullptr source.");
                    AZ_Assert(!dest->empty() && dest->get_data(), "Internal error: dest is invalid.");
                    
                    // Call the destructor
                    reinterpret_cast<ValueType*>(dest->get_data())->~ValueType();

                    // Clear memory
                    if (dest->m_typeInfo.m_useHeap)
                    {
                        dest->m_allocator.deallocate(dest->m_pointer, sizeof(ValueType), alignment_of<ValueType>::value);
                    }
                    break;
                }
            }
        }

        template <typename ValueType>
        static type_info create_template_type_info()
        {
            type_info ti;
            ti.m_id = azrtti_typeid<ValueType>();
            ti.m_isPointer = is_pointer<ValueType>::value;
            ti.m_useHeap = AZStd::GetMax(sizeof(ValueType), AZStd::alignment_of<ValueType>::value) > Internal::ANY_SBO_BUF_SIZE;
            ti.m_handler = type_info::HandleFnT(&action_handler<ValueType>);
            return ti;
        }

        /**
         * NOTE: THIS UNION MUST BE THE FIRST DATA MEMBER OF THIS CLASS
         * Changing this will violate the alignment policy.
         */
        union
        {
            AZ_ALIGN(char m_buffer[Internal::ANY_SBO_BUF_SIZE], 32); // Used for objects smaller than SBO_BUF_SIZE
            void* m_pointer; // Pointer to large objects
        };
        type_info m_typeInfo;
        allocator m_allocator;

        //////////////////////////////////////////////////////////////////////////
        // Helpers

        // Get the pointer to the current storage location
        inline       void* get_data()       { return m_typeInfo.m_useHeap ? m_pointer : reinterpret_cast<      void*>(addressof(m_buffer)); }
        inline const void* get_data() const { return m_typeInfo.m_useHeap ? m_pointer : reinterpret_cast<const void*>(addressof(m_buffer)); }
        // Copy contents from another object.
        inline void copy_from(const any& rhs, Action action = Action::Copy)
        {
            AZ_Assert(empty(), "Internal error: copy_from should only ever be called on an empty object!");
            m_allocator = rhs.m_allocator;
            m_typeInfo = rhs.m_typeInfo;
            if (!rhs.empty())
            {
                m_typeInfo.m_handler(Action::Reserve, this, nullptr); // Create space for the new object (if necessary)
                m_typeInfo.m_handler(action, this, &rhs); // Move value into this
            }
        }
        // Move contents from another object (copy, then clear rhs).
        inline void move_from(any&& rhs)
        {
            copy_from(rhs, Action::Move);
            rhs.clear();
        }

        //////////////////////////////////////////////////////////////////////////

        // Friend to any_cast (so that it may call get_data())
        template <typename ValueType>
        friend add_pointer_t<ValueType> any_cast(any*);
    };

    namespace Internal
    {
        /**
         * Ensures type requested is valid. Only called by overloads not returning a pointer (those just return nullptr).
         * In non-AZ_DEBUG_BUILD builds, this function is a no-op
         *
         * Must meet the following conditions:
         *  - Must be a reference, or copyable (any does not support taking from operand)
         *  - If const is required (bool isConst), and a reference is requested, it must be a const ref (otherwise it'd subvert const correctness).
         *      Note: if type requested is a value (copy requested), const is not required.
         *  - operand must not be empty
         *  - operand must store the same type as that requested
         */
        template <typename ValueType, bool isConst>
        void assert_value_type_valid(const any& operand)
        {
            (void)operand;
            AZ_STATIC_ASSERT(!std::is_rvalue_reference<ValueType>::value, "Type requested from any_cast cannot be an rvalue reference (taking the value stored is not supported).");
            AZ_STATIC_ASSERT(is_reference<ValueType>::value || std::is_copy_constructible<ValueType>::value, "Type requested from any_cast must be a reference or copy constructable.");
            AZ_STATIC_ASSERT(!is_reference<ValueType>::value || (!isConst || is_const<remove_reference_t<ValueType>>::value), "Type requested must be const; const cannot be any_cast'ed away.");
            AZ_Assert(!operand.empty(), "Bad any_cast: object is empty");
            AZ_Assert(operand.is<ValueType>(), "Bad any_cast: type requested doesn't match type stored.\nCall .is<ExpectedType>() before any_cast to properly handle unexpected type.");
        }
    }

    // Overload 1 - Calls overload 4
    template <typename ValueType>
    ValueType any_cast(const any& operand)
    {
        Internal::assert_value_type_valid<ValueType, true>(operand);
        return *any_cast<add_const_t<remove_reference_t<ValueType>>>(&operand);
    }

    // Overload 2 - Calls overload 5
    template <typename ValueType>
    ValueType any_cast(any& operand)
    {
        Internal::assert_value_type_valid<ValueType, false>(operand);
        return *any_cast<remove_reference_t<ValueType>>(&operand);
    }

    // Overload 3 - Calls overload 5
    template <typename ValueType>
    ValueType any_cast(any&& operand)
    {
        Internal::assert_value_type_valid<ValueType, false>(operand);
        return *any_cast<remove_reference_t<ValueType>>(&operand);
    }

    // Overload 4 - Calls overload 5
    template <typename ValueType>
    add_pointer_t<add_const_t<ValueType>> any_cast(const any* operand)
    {
        return any_cast<ValueType>(const_cast<any*>(operand));
    }

    // Overload 5 - Implementation
    template <typename ValueType>
    add_pointer_t<ValueType> any_cast(any* operand)
    {
        AZ_STATIC_ASSERT(!is_reference<ValueType>::value, "ValueType must not be a reference (cannot return pointer to reference).");
        if (!operand || operand->empty() || !operand->is<ValueType>())
        {
            return nullptr;
        }

        return reinterpret_cast<add_pointer_t<ValueType>>(operand->get_data());
    }

    // Overload 6 - Extension (calls get_data)
    template<>
    inline void* any_cast<void>(any* operand)
    {
        return (operand && !operand->empty()) ? operand->get_data() : nullptr;
    }

    /**
     * Extension: Converts an any to a convertible numeric type (see aznumeric_cast)
     *
     * \param operand   The any to cast
     * \param result    The variable to assign the result into if cast succeeds
     *
     * \returns true if conversion is successful, false if unsuccessful
     */
    template <typename ValueType, typename = enable_if_t<std::numeric_limits<ValueType>::is_specialized>>
    bool any_numeric_cast(const any* operand, ValueType& result)
    {
        if (!operand || operand->empty())
        {
            return false;
        }

        // Checks if operand is Type, and if so aznumeric_cast's from Type to ValueType
#define CHECK_TYPE(Type) if (operand->is<Type>()) { result = aznumeric_cast<ValueType>(*any_cast<Type>(operand)); }

        // Test if operand is of type ValueType
        if (auto value = any_cast<ValueType>(operand))
        {
            result = *value;
        }
        else CHECK_TYPE(char)
        else CHECK_TYPE(short)
        else CHECK_TYPE(int)
        else CHECK_TYPE(long)
        else CHECK_TYPE(AZ::s64)
        else CHECK_TYPE(unsigned char)
        else CHECK_TYPE(unsigned short)
        else CHECK_TYPE(unsigned int)
        else CHECK_TYPE(unsigned long)
        else CHECK_TYPE(AZ::u64)
        else CHECK_TYPE(float)
        else CHECK_TYPE(double)
        else
        {
            return false;
        }

        return true;

#undef CHECK_TYPE
    }
}
