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

// Description : Specific to Linux declarations, inline functions etc.


#ifndef CRYINCLUDE_CRYCOMMON_LINUX64SPECIFIC_H
#define CRYINCLUDE_CRYCOMMON_LINUX64SPECIFIC_H
#pragma once



//#define _CPU_X86
#define _CPU_AMD64
#define _CPU_SSE

#define DEBUG_BREAK raise(SIGTRAP)
#define RC_EXECUTABLE "rc"
#define USE_CRT 1
#define SIZEOF_PTR 8

//////////////////////////////////////////////////////////////////////////
// Standard includes.
//////////////////////////////////////////////////////////////////////////
#include <malloc.h>
#include <stdint.h>
#include <sys/dir.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#if defined(_CPU_SSE)
#include <xmmintrin.h>
#endif
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////
#include "BaseTypes.h"

typedef double real;

typedef uint32      DWORD;
typedef DWORD*      LPDWORD;
typedef uint64      DWORD_PTR;
typedef intptr_t INT_PTR, * PINT_PTR;
typedef uintptr_t UINT_PTR, * PUINT_PTR;
typedef char* LPSTR, * PSTR;
typedef uint64      __uint64;
#if !defined(__clang__)
typedef int64       __int64;
#endif
typedef int64       INT64;
typedef uint64      UINT64;

typedef long LONG_PTR, * PLONG_PTR, * PLONG;
typedef unsigned long ULONG_PTR, * PULONG_PTR;

typedef uint8               BYTE;
typedef uint16              WORD;
typedef void*               HWND;
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;
#define PLARGE_INTEGER LARGE_INTEGER *
typedef const char* LPCSTR, * PCSTR;
typedef long long           LONGLONG;
typedef ULONG_PTR           SIZE_T;
typedef uint8               byte;
#define ILINE __forceinline
#define _A_RDONLY (0x01)
#define _A_SUBDIR (0x10)
#define _A_HIDDEN (0x02)

//////////////////////////////////////////////////////////////////////////
// Win32 FileAttributes.
//////////////////////////////////////////////////////////////////////////
#define FILE_ATTRIBUTE_READONLY             0x00000001
#define FILE_ATTRIBUTE_HIDDEN               0x00000002
#define FILE_ATTRIBUTE_SYSTEM               0x00000004
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020
#define FILE_ATTRIBUTE_DEVICE               0x00000040
#define FILE_ATTRIBUTE_NORMAL               0x00000080
#define FILE_ATTRIBUTE_TEMPORARY            0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE          0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT        0x00000400
#define FILE_ATTRIBUTE_COMPRESSED           0x00000800
#define FILE_ATTRIBUTE_OFFLINE              0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED            0x00004000

#define INVALID_FILE_ATTRIBUTES (-1)

#define DEFINE_ALIGNED_DATA(type, name, alignment) \
    type __attribute__ ((aligned(alignment))) name;
#define DEFINE_ALIGNED_DATA_STATIC(type, name, alignment) \
    static type __attribute__ ((aligned(alignment))) name;
#define DEFINE_ALIGNED_DATA_CONST(type, name, alignment) \
    const type __attribute__ ((aligned(alignment))) name;

#include "LinuxSpecific.h"

#define TARGET_DEFAULT_ALIGN (0x8U)

#define PLATFORM_64BIT

#ifdef _RELEASE
    #define __debugbreak()
#else
    #define __debugbreak() "_asm int 3"
#endif

#define __assume(x)

#define _msize(p) malloc_usable_size(p)

#endif // CRYINCLUDE_CRYCOMMON_LINUX64SPECIFIC_H
