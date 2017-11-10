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
#ifndef GRID_MATE_H
#define GRID_MATE_H 1

#include <GridMate/Carrier/Carrier.h>
#include <GridMate/Types.h>

/// \file gridmate.h

namespace GridMate
{
    /**
    * GridMate creation descriptor.
    */
    struct GridMateDesc
    {
        GridMateDesc()
            : m_allocatorDesc()
            , m_endianType(EndianType::BigEndian) { }

        /**
        * GridMate default allocator. It will be used for all basic services and online module.
        */
        GridMateAllocator::Descriptor m_allocatorDesc;

        /**
        * Endianness serialized to the network.
        */
        EndianType m_endianType;
    };

    class GridMateService;
    class SessionService;
    struct SessionServiceDesc;
    struct SessionParams;
    struct JoinParams;
    struct SearchParams;
    struct SearchInfo;
    struct InviteInfo;
    struct SessionIdInfo;
    class GridSession;
    class GridSearch;

    class LeaderboardService;

    class AchievementMgr;
    struct AchievementServiceDesc;

    class GridStorageService;
    struct GridStorageServiceDesc;

    /**
    * GridMate interface.
    */
    class IGridMate
    {
    public:
        virtual ~IGridMate() { }
        virtual void Update() = 0;

        virtual EndianType GetDefaultEndianType() const = 0;

        // Binds service to GridMate instance, GridMate owns session pointer after that and responsible for releasing it,
        // if delegateOwnership flag is set -> GridMate will take ownership over service instance and will be soleily responsible for it's deletion
        virtual void RegisterService(GridMateServiceId id, GridMateService* service, bool delegateOwnership = false) = 0;

        // Unbinds service from GridMate instance, service should not be used after this is called
        virtual void UnregisterService(GridMateServiceId id) = 0;

        // Returns true if a service with the specified service id is currently registered with this GridMate
        virtual bool HasService(GridMateServiceId id) = 0;

        // Returns the service registered with the specified service id, or nullptr if not found.
        virtual GridMateService* GetServiceById(GridMateServiceId id) = 0;

        //////////////////////////////////////////////////////////////////////////
        // Requires on-line service to be started
        /*
        * Leaderboard service
        */
        virtual bool StartLeaderboardService(ServiceType type) = 0;
        virtual bool StartLeaderboardServiceCustom(GridMate::LeaderboardService* userService) = 0;
        virtual void StopLeaderboardService() = 0;
        virtual bool IsLeaderboardServiceStarted() const = 0;
        virtual LeaderboardService* GetLeaderboardService() = 0;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // Requires on-line service to be started
        virtual bool StartAchievementService(ServiceType type, const AchievementServiceDesc& desc) = 0;
        virtual bool StartAchievementServiceCustom(AchievementMgr* userService) = 0;
        virtual void StopAchievementService() = 0;
        virtual bool IsAchievementServiceStarted() const = 0;
        virtual AchievementMgr* GetAchievementService() = 0;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // Requires on-line service to be started
        virtual bool StartStorageService(ServiceType type, const GridStorageServiceDesc& desc) = 0;
        virtual bool StartStorageServiceCustom(GridStorageService* userService) = 0;
        virtual void StopStorageService() = 0;
        virtual bool IsStorageServiceStarted() const = 0;
        virtual GridStorageService* GetStorageService() = 0;
        //////////////////////////////////////////////////////////////////////////
    };



    /**
    * Helper function to start service of given type and register it with GridMate.
    * Newly created service instance will be owned by GridMate.
    */
    template<class ServiceType, class ... Args>
    ServiceType* StartGridMateService(IGridMate* gridMate, Args&& ... args)
    {
        ServiceType* service = aznew ServiceType(AZStd::forward<Args>(args) ...);
        gridMate->RegisterService(ServiceType::GetGridMateServiceId(),service, true);
        return service;
    }

    template<class ServiceType>
    void StopGridMateService(IGridMate* gridMate)
    {
        gridMate->UnregisterService(ServiceType::GetGridMateServiceId());
    }

    template<class ServiceType>
    bool HasGridMateService(IGridMate* gridMate)
    {
        return gridMate->HasService(ServiceType::GetGridMateServiceId());
    }

    /// Create GridMate interface object. You are allowed to have only one active at a time. \todo use shared_ptr or intrusive_ptr
    IGridMate* GridMateCreate(const GridMateDesc& desc);
    /// Destroys and frees all GridMate resources.
    void GridMateDestroy(IGridMate* gridMate);
}

#endif // GRID_MATE_H
