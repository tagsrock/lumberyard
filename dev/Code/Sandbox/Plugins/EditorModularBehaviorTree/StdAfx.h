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

#pragma warning(disable: 4103) // '...\stlport\stl\_cprolog.h' : alignment changed after including header, may be due to missing #pragma pack(pop)

//#define NOT_USE_CRY_MEMORY_MANAGER

// STL Port in debug for debug builds
#if defined(_DEBUG)
//#  define _STLP_DEBUG 1
#endif
#include <STLPortConfig.h>

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers
#endif

#ifndef WINVER
#define WINVER 0x0600 // Windows Vista
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS WINVER
#endif

#ifndef _WIN32_IE
#define _WIN32_IE WINVER
#endif

#include <stdlib.h>
#include <afxwin.h>
#include <afxext.h>


//#define INCLUDE_SAVECGF

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include "CryModuleDefs.h"
#include "platform.h"

/////////////////////////////////////////////////////////////////////////////
// STL
/////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <memory>

/////////////////////////////////////////////////////////////////////////////
// BOOST
/////////////////////////////////////////////////////////////////////////////
#include <BoostHelpers.h>

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#include <ISystem.h>
#include "Util/EditorUtils.h"
#include <functor.h>
#include <CryFixedArray.h>
#include "EditorCoreAPI.h"