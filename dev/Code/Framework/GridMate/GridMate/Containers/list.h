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
#ifndef GM_CONTAINERS_LIST_H
#define GM_CONTAINERS_LIST_H

#include <GridMate/Memory.h>
#include <AzCore/std/containers/list.h>

namespace GridMate
{
#if defined(AZ_HAS_TEMPLATE_ALIAS)
    template<class T, class Allocator = SysContAlloc>
    using list = AZStd::list<T, Allocator>;
#else
    template<typename T>
    struct list
        : public AZStd::list<T, SysContAlloc>
    {
    };
#endif
}

#endif // GM_CONTAINERS_LIST_H
