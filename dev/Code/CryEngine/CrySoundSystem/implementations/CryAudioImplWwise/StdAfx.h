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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#pragma once

#include <CryModuleDefs.h>
#define eCryModule eCryM_AudioImpl

#include <platform.h>
#include <AzCore/Debug/Trace.h>
#include <ProjectDefines.h>
#include <StlUtils.h>

#if !defined(_RELEASE)
    // Enable logging for non-Release builds.
    #define ENABLE_AUDIO_LOGGING
    // Production code is enabled in non-Release builds.
    #define INCLUDE_WWISE_IMPL_PRODUCTION_CODE
#endif // !_RELEASE


// Windows or Xbox One
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
    #include <windows.h>
#endif // Windows or Xbox One

// Windows32
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_WINDOWS) && !defined(AZ_PLATFORM_WINDOWS_X64)
#endif // Windows32

// Windows64
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_WINDOWS_X64)
#endif // Windows64

// Xbox One
///////////////////////////////////////////////////////////////////////////////////////////////////

// PS4
///////////////////////////////////////////////////////////////////////////////////////////////////

// Mac
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_APPLE_OSX)
#endif // Mac

// iOS
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_APPLE_IOS)
#endif // iOS

// AppleTV
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_APPLE_TV)
#endif // AppleTV

// Android
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_ANDROID)
#endif // Android

// Linux
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_PLATFORM_LINUX_X64)
#endif // Linux


#include <AudioAllocators.h>
#include <AudioLogger.h>

namespace Audio
{
    extern CAudioLogger g_audioImplLogger_wwise;
} // namespace Audio


// Secondary Memory Allocation Pool
#if defined(PROVIDE_WWISE_IMPL_SECONDARY_POOL)

#include <CryPool/PoolAlloc.h>

typedef NCryPoolAlloc::CThreadSafe<NCryPoolAlloc::CBestFit<NCryPoolAlloc::CReferenced<NCryPoolAlloc::CMemoryDynamic, 4 * 1024, true>, NCryPoolAlloc::CListItemReference> > tMemoryPoolReferenced;

namespace Audio
{
    extern tMemoryPoolReferenced g_audioImplMemoryPoolSecondary_wwise;
} // namespace Audio

#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

