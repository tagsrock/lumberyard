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
#ifndef GM_MEMORY_H
#define GM_MEMORY_H

#include <AzCore/base.h>
#if defined(AZ_COMPILER_MSVC) // todo fix it
#   pragma warning(push)
#   pragma warning(disable:4127)
#endif
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>
#if defined(AZ_COMPILER_MSVC)
#   pragma warning(pop)
#endif

#include <GridMate/GridMateForTools.h>

namespace GridMate
{
#ifndef GRIDMATE_FOR_TOOLS
    /**
    * GridMateAllocator is used by non-MP portions of GridMate
    */
    class GridMateAllocator
        : public AZ::SystemAllocator
    {
        friend class AZ::AllocatorInstance<GridMateAllocator>;
    public:

        AZ_TYPE_INFO(GridMateAllocator, "{BB127E7A-E4EF-4480-8F17-0C10146D79E0}")

        const char* GetName() const override { return "GridMate Allocator"; }
        const char* GetDescription() const override { return "GridMate fundamental generic memory allocator"; }
    };

    /**
    * GridMateAllocatorMP is used by MP portions of GridMate
    */
    class GridMateAllocatorMP
        : public AZ::SystemAllocator
    {
        friend class AZ::AllocatorInstance<GridMateAllocatorMP>;
    public:

        AZ_TYPE_INFO(GridMateAllocatorMP, "{FABCBC6E-B3E5-4200-861E-A3EC22592678}")

        const char* GetName() const override { return "GridMate Multiplayer Allocator"; }
        const char* GetDescription() const override { return "GridMate Multiplayer data allocations (Session,Replica,Carrier)"; }

        // TODO: We have an aggressive memory policy in the Carrier. We have 2 ways to fix it.
        // Either keep a cap and sacrifice performance or create a carrier->GarbageCollection and call it from here
        //virtual void          GarbageCollect()                 { EBUS_EVENT(CarrierBus,GarbageCollect); m_allocator->GarbageCollect(); }
    };
#else
    using GridMateAllocator = AZ::OSAllocator;
    using GridMateAllocatorMP = AZ::OSAllocator;
#endif

    //! GridMate system container allocator.
    typedef AZ::AZStdAlloc<GridMateAllocator> GridMateStdAlloc;

    //! GridMate system container allocator.
    typedef AZ::AZStdAlloc<GridMateAllocatorMP> SysContAlloc;
}   // namespace GridMate


#define GM_CLASS_ALLOCATOR(_type)       AZ_CLASS_ALLOCATOR(_type, GridMate::GridMateAllocatorMP, 0)
#define GM_CLASS_ALLOCATOR_DECL         AZ_CLASS_ALLOCATOR_DECL
#define GM_CLASS_ALLOCATOR_IMPL(_type)  AZ_CLASS_ALLOCATOR_IMPL(_type, GridMate::GridMateAllocatorMP, 0)

#endif // GM_MEMORY_H
