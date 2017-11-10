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

// Description : to get some defines available in every CryEngine project


#ifndef CRYINCLUDE_CRYCOMMON_PROJECTDEFINES_H
#define CRYINCLUDE_CRYCOMMON_PROJECTDEFINES_H
#pragma once


#include "BaseTypes.h"

#if defined(_RELEASE) && !defined(RELEASE)
    #define RELEASE
#endif

#if   defined(WIN32) || defined(WIN64)
#   if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
#       define ENABLE_STATS_AGENT
#   endif
#endif

#define USE_STEAM 0 // Enable this to start using Steam

// The following definitions are used by Sandbox and RC to determine which platform support is needed
#define TOOLS_SUPPORT_POWERVR

// Type used for vertex indices
// WARNING: If you change this typedef, you need to update AssetProcessorPlatformConfig.ini to convert cgf and abc files to the proper index format.
#if defined(RESOURCE_COMPILER)
typedef uint32 vtx_idx;
#elif defined(MOBILE)
typedef uint16 vtx_idx;
#else
// Uncomment one of the two following typedefs:
typedef uint32 vtx_idx;
//typedef uint16 vtx_idx;
#endif


// see http://wiki/bin/view/CryEngine/TerrainTexCompression for more details on this
// 0=off, 1=on
#define TERRAIN_USE_CIE_COLORSPACE 0

// for consoles every bit of memory is important so files for documentation purpose are excluded
// they are part of regular compiling to verify the interface

// When non-zero, const cvar accesses (by name) are logged in release-mode on consoles.
// This can be used to find non-optimal usage scenario's, where the constant should be used directly instead.
// Since read accesses tend to be used in flow-control logic, constants allow for better optimization by the compiler.
#define LOG_CONST_CVAR_ACCESS 0

#if defined(WIN32) || defined(WIN64) || LOG_CONST_CVAR_ACCESS
#define RELEASE_LOGGING
#if defined(_RELEASE)
#define CVARS_WHITELIST
#endif // defined(_RELEASE)
#endif

#if defined(_RELEASE) && !defined(RELEASE_LOGGING)
#define EXCLUDE_NORMAL_LOG
#endif

// Add the "REMOTE_ASSET_PROCESSOR" define except in release
// this makes it so that asset processor functions.  Without this, all assets must be present and on local media
// with this, the asset processor can be used to remotely process assets.
#if !defined(_RELEASE)
#   define REMOTE_ASSET_PROCESSOR
#endif

#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD))
    #define USE_HTTP_WEBSOCKETS 0
#endif

#if (defined(WIN32) || defined(ORBIS) || defined(DURANGO)) && !defined(RESOURCE_COMPILER)
  #define CAPTURE_REPLAY_LOG 1
#endif

#if defined(RESOURCE_COMPILER) || defined(_RELEASE)
  #undef CAPTURE_REPLAY_LOG
#endif

#ifndef CAPTURE_REPLAY_LOG
  #define CAPTURE_REPLAY_LOG 0
#endif

#if (defined(LINUX) || defined(ANDROID) || defined(APPLE) ||  defined(WIN32) || defined(DURANGO) || defined(ORBIS))
    #define USE_GLOBAL_BUCKET_ALLOCATOR
#endif

#ifdef IS_PROSDK
#   define USING_TAGES_SECURITY                 // Wrapper for TGVM security
# if defined(LINUX) || defined(APPLE)
#   error LINUX and Mac does not support evaluation version
# endif
#endif

#ifdef USING_TAGES_SECURITY
#   define TAGES_EXPORT __declspec(dllexport)
#else
#   define TAGES_EXPORT
#endif // USING_TAGES_SECURITY
// test -------------------------------------

#define _DATAPROBE



//This feature allows automatic crash submission to JIRA, but does not work outside of CryTek
//Note: This #define will be commented out during code export
#define ENABLE_CRASH_HANDLER

#if !defined(PHYSICS_STACK_SIZE)
# define PHYSICS_STACK_SIZE (128U << 10)
#endif

#if !defined(USE_LEVEL_HEAP)
#define USE_LEVEL_HEAP 0
#endif

#if USE_LEVEL_HEAP && !defined(_RELEASE)
#define TRACK_LEVEL_HEAP_USAGE 1
#endif

#ifndef TRACK_LEVEL_HEAP_USAGE
#define TRACK_LEVEL_HEAP_USAGE 0
#endif

#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD)) && !defined(RESOURCE_COMPILER)
#ifndef ENABLE_PROFILING_CODE
    #define ENABLE_PROFILING_CODE
#endif
#if !(defined(SANDBOX_EXPORTS) || defined(PLUGIN_EXPORTS) || (defined(AZ_MONOLITHIC_BUILD) && !defined(__ORBIS__)))
    #define ENABLE_PROFILING_MARKERS
#endif

//lightweight profilers, disable for submissions, disables displayinfo inside 3dengine as well
#ifndef ENABLE_LW_PROFILERS
    #define ENABLE_LW_PROFILERS
#endif
#endif

#if !defined(_RELEASE)
#define USE_FRAME_PROFILER      // comment this define to remove most profiler related code in the engine
#define CRY_TRACE_HEAP
#endif

#undef ENABLE_STATOSCOPE
#if defined(ENABLE_PROFILING_CODE)
  #define ENABLE_STATOSCOPE 1
#endif

#if defined(ENABLE_PROFILING_CODE)
  #define USE_PERFHUD
#endif

#if defined(ENABLE_PROFILING_CODE)
#define ENABLE_ART_RT_TIME_ESTIMATE
#endif

#if defined(ENABLE_STATOSCOPE) && !defined(_RELEASE)
    #define FMOD_STREAMING_DEBUGGING 1
#endif

#if defined(WIN32) || defined(WIN64) || defined(APPLE)
#define FLARES_SUPPORT_EDITING
#endif

// Reflect texture slot information - only used in the editor
#if defined(WIN32) || defined(WIN64)
#define SHADER_REFLECT_TEXTURE_SLOTS 1
#else
#define SHADER_REFLECT_TEXTURE_SLOTS 0
#endif

#if (defined(WIN32) || defined(WIN64)) && (!defined(_RELEASE) || defined(RESOURCE_COMPILER))
#define CRY_ENABLE_RC_HELPER 1
#endif

#if !defined(_RELEASE) && !defined(LINUX) && !defined(APPLE) && !defined(DURANGO) && !defined(ORBIS)
    #define SOFTCODE_SYSTEM_ENABLED
#endif

// Is SoftCoding enabled for this module? Usually set by the SoftCode AddIn in conjunction with a SoftCode.props file.
#ifdef SOFTCODE_ENABLED

// Is this current compilation unit part of a SOFTCODE build?
    #ifdef SOFTCODE
// Import any SC functions from the host module
        #define SC_API __declspec(dllimport)
    #else
// Export any SC functions from the host module
        #define SC_API __declspec(dllexport)
    #endif

#else   // SoftCode disabled

    #define SC_API

#endif

// these enable and disable certain net features to give compatibility between PCs and consoles / profile and performance builds
#define PC_CONSOLE_NET_COMPATIBLE 0
#define PROFILE_PERFORMANCE_NET_COMPATIBLE 0

#if (!defined(_RELEASE) || defined(PERFORMANCE_BUILD)) && !PROFILE_PERFORMANCE_NET_COMPATIBLE
#define USE_LAGOMETER (1)
#else
#define USE_LAGOMETER (0)
#endif

// enable this in order to support old style material names in old data ("engine/material.mtl" or "mygame/material.mtl" as opposed to just "material.mtl")
// previously, material names could have the game folder in it, but this is not necessary anymore and would not work with things like gems
// note that if you use any older projects such as GameSDK this should remain enabled
#define SUPPORT_LEGACY_MATERIAL_NAMES

// Enable additional structures and code for sprite motion blur. Currently non-functional and disabled
// #define PARTICLE_MOTION_BLUR

// a special ticker thread to run during load and unload of levels
#define USE_NETWORK_STALL_TICKER_THREAD

#if !defined(MOBILE)
//---------------------------------------------------------------------
// Enable Tessellation Features
// (displacement mapping, subdivision, water tessellation)
//---------------------------------------------------------------------
// Modules   : 3DEngine, Renderer
// Depends on: DX11

// Global tessellation feature flag
    #define TESSELLATION
    #ifdef TESSELLATION
// Specific features flags
        #define WATER_TESSELLATION
        #define PARTICLES_TESSELLATION

// Mesh tessellation (displacement, smoothing, subd)
            #define MESH_TESSELLATION
// Mesh tessellation also in motion blur passes
            #define MOTIONBLUR_TESSELLATION

// Dependencies
        #ifdef MESH_TESSELLATION
            #define MESH_TESSELLATION_ENGINE
        #endif
            #ifndef NULL_RENDERER
            #ifdef WATER_TESSELLATION
                #define WATER_TESSELLATION_RENDERER
            #endif
            #ifdef PARTICLES_TESSELLATION
                #define PARTICLES_TESSELLATION_RENDERER
            #endif
            #ifdef MESH_TESSELLATION_ENGINE
                #define MESH_TESSELLATION_RENDERER
            #endif

            #if defined(WATER_TESSELLATION_RENDERER) || defined(PARTICLES_TESSELLATION_RENDERER) || defined(MESH_TESSELLATION_RENDERER)
// Common tessellation flag enabling tessellation stages in renderer
                #define TESSELLATION_RENDERER
            #endif
        #endif // !NULL_RENDERER
    #endif // TESSELLATION
#endif // !defined(MOBILE)


#define USE_GEOM_CACHES

//------------------------------------------------------
// SVO GI
//------------------------------------------------------
// Modules   : Renderer, Engine
// Platform  : DX11
#if !defined(RENDERNODES_LEAN_AND_MEAN) && (defined(WIN32) || defined(DURANGO) /*|| defined(ORBIS)*/)
#define FEATURE_SVO_GI
#if defined(WIN32)
    #define FEATURE_SVO_GI_ALLOW_HQ
#endif
#endif

#if defined(ENABLE_PROFILING_CODE)
#   define USE_DISK_PROFILER
#   define ENABLE_LOADING_PROFILER
#endif


#include "ProjectDefinesInclude.h"

#if defined(SOFTCODE_ENABLED) && defined(NOT_USE_CRY_MEMORY_MANAGER)
    #error "SoftCode currently relies on CryMemoryManager being enabled. Either build without SoftCode support, or enable CryMemoryManager."
#endif

//Encryption & security defines

//Defines for various encryption methodologies that we support (or did support at some stage)
#define SUPPORT_UNENCRYPTED_PAKS                                                //Enable during dev and on consoles to support paks that aren't encrypted in any way
#if defined(_RELEASE) // Require signing (at least)
#define SUPPORT_UNSIGNED_PAKS                                                       //Enabled during dev to test release builds easier (remove this to enforce signed paks in release builds)
#endif

//#define SUPPORT_XTEA_PAK_ENCRYPTION                                       //C2 Style. Compromised - do not use
//#define SUPPORT_STREAMCIPHER_PAK_ENCRYPTION                       //C2 DLC Style - by Mark Tully
#define SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION     //C3/Warface Style - By Timur Davidenko and integrated by Rob Jessop
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
#define SUPPORT_UNSIGNED_PAKS                                                   //Enable to load paks that aren't RSA signed
#endif //!_RELEASE || PERFORMANCE_BUILD
#define SUPPORT_RSA_PAK_SIGNING                                             //RSA signature verification

// Disabled - needs fixing with April XDK

#if (defined(WIN32) || defined(WIN64) || defined(LINUX) || defined(APPLE) || defined(ORBIS)) && !defined(NULL_RENDERER)
#define GPU_PARTICLES 1
#else
#define GPU_PARTICLES 0
#endif

#if defined(SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION) || defined(SUPPORT_RSA_PAK_SIGNING)
//Use LibTomMath and LibTomCrypt for cryptography
#define INCLUDE_LIBTOMCRYPT
#endif

//This enables checking of CRCs on archived files when they are loaded fully and synchronously in CryPak.
//Computes a CRC of the decompressed data and compares it to the CRC stored in the archive CDR for that file.
//Files with CRC mismatches will return Z_ERROR_CORRUPT and invoke the global handler in the PlatformOS.
#define VERIFY_PAK_ENTRY_CRC

//#define CHECK_CRC_ONLY_ONCE   //Do NOT enable this if using SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION - it will break subsequent decryption attempts for a file as it nulls the stored CRC32

#if 0 // Enable when clear on which platforms we want this check
//On consoles we can trust files that have been loaded from optical drives
#define SKIP_CHECKSUM_FROM_OPTICAL_MEDIA
#endif // 0

//End of encryption & security defines

#define EXPOSE_D3DDEVICE

// The maximum number of joints in an animation
#define MAX_JOINT_AMOUNT 1024

#endif // CRYINCLUDE_CRYCOMMON_PROJECTDEFINES_H
