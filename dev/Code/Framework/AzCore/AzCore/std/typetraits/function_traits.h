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
#ifndef AZSTD_TYPE_TRAITS_FUNCTION_TRAITS_INCLUDED
#define AZSTD_TYPE_TRAITS_FUNCTION_TRAITS_INCLUDED

#include <AzCore/std/typetraits/is_function.h>
#include <AzCore/std/typetraits/add_pointer.h>

namespace AZStd
{
    namespace Internal
    {
        template<typename ...Ts>
        struct pack_traits_arg_sequence;

        template<size_t index, typename T>
        struct pack_traits_get_arg;

        template<size_t index>
        struct pack_traits_get_arg<index, pack_traits_arg_sequence<>>
        {
            static_assert(AZStd::integral_constant<size_t, index>::value && false, "Element index out of bounds");
        };

        template<typename First, typename ...Rest>
        struct pack_traits_get_arg<0, pack_traits_arg_sequence<First, Rest...>>
        {
            using type = First;
        };

        template<size_t index, typename First, typename ...Rest>
        struct pack_traits_get_arg<index, pack_traits_arg_sequence<First, Rest...>> : pack_traits_get_arg<index - 1, pack_traits_arg_sequence<Rest...>> {};

        template<size_t index, typename ...Args>
        using pack_traits_get_arg_t = typename pack_traits_get_arg<index, pack_traits_arg_sequence<Args...>>::type;

        template<typename Function>
        struct function_traits_helper : function_traits_helper<decltype(&Function::operator())> {};

        template<typename C, typename R, typename... Args>
        struct function_traits_helper<R(C::*)(Args...) const> : function_traits_helper<R(*)(Args...)>
        {
            using class_fp_type = R(C::*)(Args...) const;
            using class_type = C;
        };

        template<typename C, typename R, typename... Args>
        struct function_traits_helper<R(C::*)(Args...)> : function_traits_helper<R(*)(Args...)>
        {
            using class_fp_type = R(C::*)(Args...);
            using class_type = C;
        };

        template<typename R, typename ...Args>
        struct function_traits_helper<R(*)(Args...)>
        {
            AZSTD_STATIC_CONSTANT(size_t, arity = sizeof...(Args));
            AZSTD_STATIC_CONSTANT(size_t, num_args = sizeof...(Args));
            using result_type = R;
            using raw_fp_type = result_type(*)(Args...);
            
            template<size_t index>
            using get_arg_t = pack_traits_get_arg_t<index, Args...>;
        };
    }

    template<typename Function>
    struct function_traits : public Internal::function_traits_helper<Function>{};

    template<typename R, typename ...Args>
    struct function_traits<R(Args...)> : public Internal::function_traits_helper<add_pointer_t<R(Args...)>>{};

    template<typename Function>
    using function_traits_get_result_t = typename function_traits<Function>::result_type;

    template<typename Function, size_t index>
    using function_traits_get_arg_t = typename function_traits<Function>::template get_arg_t<index>;
}

#endif // AZSTD_TYPE_TRAITS_FUNCTION_TRAITS_INCLUDED
#pragma once