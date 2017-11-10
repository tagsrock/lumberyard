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
#ifndef AZCORE_FRAME_PROFILER_COMPONENT_H
#define AZCORE_FRAME_PROFILER_COMPONENT_H

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Debug/FrameProfiler.h>
#include <AzCore/std/parallel/threadbus.h>
#include <AzCore/Math/Crc.h>

namespace AZ
{
    namespace Debug
    {
        /**
         * Frame profiler component provides a frame profiling information
         * (from FPS counter to profiler registers manipulation and so on).
         * It's a debug system so it should not be active in release
         */
        class FrameProfilerComponent
            : public Component
            , public AZ::TickBus::Handler
            , public AZStd::ThreadEventBus::Handler
        {
        public:
            AZ_COMPONENT(AZ::Debug::FrameProfilerComponent, "{B81739EF-ED77-4F67-9D05-6ADF94F0431A}")

            FrameProfilerComponent();
            virtual ~FrameProfilerComponent();

        private:
            //////////////////////////////////////////////////////////////////////////
            // Component base
            void Activate() override;
            void Deactivate() override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Tick bus
            void OnTick(float deltaTime, ScriptTimePoint time) override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // Thread event bus
            /// Called when we enter a thread, optional thread_desc is provided when the use provides one.
            void OnThreadEnter(const AZStd::thread::id& id, const AZStd::thread_desc* desc) override;
            /// Called when we exit a thread.
            void OnThreadExit(const AZStd::thread::id& id) override;
            //////////////////////////////////////////////////////////////////////////

            /// \ref ComponentDescriptor::GetProvidedServices
            static void GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided);
            /// \ref ComponentDescriptor::GetIncompatibleServices
            static void GetIncompatibleServices(ComponentDescriptor::DependencyArrayType& incompatible);
            /// \ref ComponentDescriptor::GetDependentServices
            static void GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent);
            /// \red ComponentDescriptor::Reflect
            static void Reflect(ReflectContext* reflection);

            /// callback for reading profiler registers
            bool            ReadProfilerRegisters(const ProfilerRegister& reg, const AZStd::thread_id& id);

            // Keep in mind memory usage, increases quickly. Prefer remote tools (where the history is kept on the PC) instead of keeping long history
            unsigned int    m_numFramesStored;  ///< Number of frames that we will store in history buffers. >= 1
            unsigned int    m_frameId;      ///< Frame id (it's just counted from the start).

            unsigned int    m_pauseOnFrame; ///< Allows you to specify a frame the code will pause onto.


            FrameProfiler::ThreadDataArray  m_threads;              ///< Array with samplers for all threads
            FrameProfiler::ThreadData*      m_currentThreadData;    ///< Cached pointer to the last accessed thread data.
        };
    }
}

#endif // AZCORE_FRAME_PROFILER_COMPONENT_H
#pragma once