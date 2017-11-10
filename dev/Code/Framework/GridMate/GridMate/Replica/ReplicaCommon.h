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
#ifndef GM_REPLICACOMMON_H
#define GM_REPLICACOMMON_H

#include <GridMate/Types.h>
#include <GridMate/Replica/ReplicaDefs.h>

#include <AzCore/std/smart_ptr/intrusive_ptr.h>
#include <AzCore/Math/Crc.h>

#define GM_MAX_CHUNKS_PER_REPLICA   (64)
#define GM_MAX_DATASETS_IN_CHUNK    (32)
#define GM_MAX_RPCS_DECL_PER_CHUNK  (32)
#define GM_MAX_RPC_SEND_PER_REPLICA (65535)
#define GM_MAX_REPLICA_CLASS_TYPES  (256)
#define GM_REPIDS_PER_BLOCK         (1<<25)        //~33M replicaIds/host with up to 128 hosts

#define GM_REPLICA_MSG_CUTOFF 1100

#if !defined(GM_REPLICA_HAS_DEBUG_NAME)
    #if defined(AZ_RELEASE_BUILD)
        #define GM_REPLICA_HAS_DEBUG_NAME 0
    #else
        #define GM_REPLICA_HAS_DEBUG_NAME 1
    #endif
#endif

namespace GridMate
{
    class Replica;
    class ReplicaChunkBase;
    class ReplicaManager;
    class ReplicaPeer;

    class DataSetBase;
    class RpcBase;
    struct RpcContext;

    class ReadBuffer;
    class WriteBuffer;

    typedef AZStd::intrusive_ptr<Replica> ReplicaPtr;
    typedef AZStd::intrusive_ptr<ReplicaChunkBase> ReplicaChunkPtr;

    /*
    * constants
    */
    static const ReplicaId InvalidReplicaId = 0;
    static const PeerId InvalidReplicaPeerId = 0;

    //-----------------------------------------------------------------------------
    //-----------------------------------------------------------------------------
    struct TimeContext
    {
        unsigned int m_realTime;
        unsigned int m_localTime;
    };
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    //-----------------------------------------------------------------------------
    struct ReplicaContext
        : public TimeContext
    {
        ReplicaManager* m_rm;
        ReplicaPeer* m_peer; // peer the replica (or replica update) belongs to or came from

        explicit ReplicaContext(ReplicaManager* rm, const TimeContext& tc, ReplicaPeer* peer = 0)
            : TimeContext(tc)
            , m_rm(rm)
            , m_peer(peer) {}
    };
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    //-----------------------------------------------------------------------------
    struct PrepareDataResult
    {
        PrepareDataResult(bool isDownstreamReliableDirty, bool isDownstreamUnreliableDirty, bool isUpstreamReliableDirty, bool isUpstreamUnreliableDirty)
            : m_isDownstreamReliableDirty(isDownstreamReliableDirty)
            , m_isDownstreamUnreliableDirty(isDownstreamUnreliableDirty)
            , m_isUpstreamReliableDirty(isUpstreamReliableDirty)
            , m_isUpstreamUnreliableDirty(isUpstreamUnreliableDirty)
        { }

        bool m_isDownstreamReliableDirty;
        bool m_isDownstreamUnreliableDirty;
        bool m_isUpstreamReliableDirty;
        bool m_isUpstreamUnreliableDirty;
    };
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    struct MarshalContext
        : public ReplicaContext
    {
        AZ::u32 m_marshalFlags;
        WriteBuffer* m_outBuffer;

        explicit MarshalContext(AZ::u32 marshalFlags, WriteBuffer* writeBuffer, const ReplicaContext& rc)
            : ReplicaContext(rc)
            , m_marshalFlags(marshalFlags)
            , m_outBuffer(writeBuffer)
        { }
    };

    //-----------------------------------------------------------------------------
    struct UnmarshalContext
        : public ReplicaContext
    {
        ReadBuffer* m_iBuf;
        AZ::u32 m_timestamp;
        bool m_hasCtorData;

        explicit UnmarshalContext(const ReplicaContext& rc)
            : ReplicaContext(rc)
            , m_iBuf(nullptr)
            , m_timestamp(0)
            , m_hasCtorData(false) { }
    };
    //-----------------------------------------------------------------------------
    typedef AZ::u16 ReplicaPriority;

    // Predifined replica priorities

    // real time replicas have the highest priority and will not be cut off by any bandwidth limiter
    static const ReplicaPriority k_replicaPriorityRealTime   = 0xFFFF;

    static const ReplicaPriority k_replicaPriorityHighest    = 0xFFFE;
    static const ReplicaPriority k_replicaPriorityHigh       = 0xC000;
    static const ReplicaPriority k_replicaPriorityNormal     = 0x8000;
    static const ReplicaPriority k_replicaPriorityLow        = 0x4000;
    static const ReplicaPriority k_replicaPriorityLowest     = 0x0000;
} // namespace GridMate

#endif // GM_REPLICACOMMON_H
