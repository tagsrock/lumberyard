/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <AzCore/Driller/Driller.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/parallel/threadbus.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/Debug/EventTraceDrillerBus.h>

namespace AZ
{
    namespace Debug
    {
        class EventTraceDriller
            : public Driller
            , public EventTraceDrillerBus::Handler
            , public EventTraceDrillerSetupBus::Handler
            , public AZStd::ThreadEventBus::Handler
            , public AZ::TickBus::Handler
        {
        public:
            AZ_CLASS_ALLOCATOR(EventTraceDriller, OSAllocator, 0)

            EventTraceDriller();
            virtual ~EventTraceDriller();

        private:
            // Driller
            //////////////////////////////////////////////////////////////////////////
            const char*  GroupName() const override { return "SystemDrillers"; }
            const char*  GetName() const override { return "EventTraceDriller"; }
            const char*  GetDescription() const override { return "Handles timed events for a Chrome Tracing."; }
            void         Start(const Param* params = NULL, int numParams = 0) override;
            void         Stop() override;

            // ThreadBus
            //////////////////////////////////////////////////////////////////////////
            void OnThreadEnter(const AZStd::thread::id& id, const AZStd::thread_desc* desc) override;
            void OnThreadExit(const AZStd::thread::id& id) override;

            // TickBus
            //////////////////////////////////////////////////////////////////////////
            void OnTick(float deltaTime, ScriptTimePoint time) override;

            // EventTraceDrillerSetupBus
            //////////////////////////////////////////////////////////////////////////
            void SetThreadName(const AZStd::thread_id& threadId, const char* name) override;

            // EventTraceDrillerBus
            //////////////////////////////////////////////////////////////////////////
            void RecordSlice(
                const char* name,
                const char* category,
                const AZStd::thread_id& threadId,
                AZ::u64 timestamp,
                AZ::u32 duration) override;

            void RecordInstantGlobal(
                const char* name,
                const char* category,
                AZ::u64 timestamp) override;

            void RecordInstantThread(
                const char* name,
                const char* category,
                const AZStd::thread_id& threadId,
                AZ::u64 timestamp) override;

            void RecordThreads();

            struct ThreadData
            {
                AZStd::string name;
            };

            AZStd::recursive_mutex m_ThreadMutex;
            AZStd::unordered_map<size_t, ThreadData, AZStd::hash<size_t>, AZStd::equal_to<size_t>, OSStdAllocator> m_Threads;
        };
    }
} // namespace AZ
