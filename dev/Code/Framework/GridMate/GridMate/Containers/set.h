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
#ifndef GM_CONTAINERS_SET_H
#define GM_CONTAINERS_SET_H

#include <GridMate/Memory.h>
#include <AzCore/std/containers/set.h>

namespace GridMate
{
#if defined(AZ_HAS_TEMPLATE_ALIAS)
    template<class Key, class Compare = AZStd::less<Key>, class Allocator = SysContAlloc>
    using set = AZStd::set<Key, Compare, Allocator>;

    template<class Key, class Compare = AZStd::less<Key>, class Allocator = SysContAlloc>
    using multiset = AZStd::multiset<Key, Compare, Allocator>;
#else
    template<class Key, class Compare = AZStd::less<Key>, class Allocator = SysContAlloc>
    class set
        : public AZStd::set<Key, Compare, Allocator>
    {
    };

    template<class Key, class Compare = AZStd::less<Key>, class Allocator = SysContAlloc>
    class multiset
        : AZStd::multiset<Key, Compare, Allocator>
    {
    };
#endif
}

#endif // GM_CONTAINERS_SET_H
