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
//    Fixed is_pointer, is_reference, is_const, is_volatile, is_same,
//    is_member_pointer based on the Simulated Partial Specialization work
//    of Mat Marcus and Jesse Jones. See  http://opensource.adobe.com or
//    http://groups.yahoo.com/group/boost/message/5441
//    Some workarounds in here use ideas suggested from "Generic<Programming>:
//    Mappings between Types and Values"
//    by Andrei Alexandrescu (see http://www.cuj.com/experts/1810/alexandr.html).
#ifndef AZSTD_TYPE_TRAITS_IS_MEMBER_POINTER_INCLUDED
#define AZSTD_TYPE_TRAITS_IS_MEMBER_POINTER_INCLUDED

#include <AzCore/std/typetraits/is_member_function_pointer.h>
#include <AzCore/std/typetraits/bool_trait_def.h>

namespace AZStd
{
    AZSTD_TYPE_TRAIT_BOOL_DEF1(is_member_pointer, T, ::AZStd::is_member_function_pointer<T>::value)
    AZSTD_TYPE_TRAIT_BOOL_PARTIAL_SPEC1_2(typename T, typename U, is_member_pointer, U T::*, true)
}

#endif // AZSTD_TYPE_TRAITS_IS_MEMBER_POINTER_INCLUDED
#pragma once