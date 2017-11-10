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

// Description : A multiplatform base class for handling errors and collecting call stacks


#include "StdAfx.h"
#include "IDebugCallStack.h"
#include <IPlatformOS.h>
#include "System.h"
#include <AzFramework/IO/FileOperations.h>

//#if !defined(LINUX)

#include <ISystem.h>

const char* const IDebugCallStack::s_szFatalErrorCode = "FATAL_ERROR";

IDebugCallStack::IDebugCallStack()
    : m_bIsFatalError(false)
    , m_postBackupProcess(0)
    , m_memAllocFileHandle(AZ::IO::InvalidHandle)
{
}

IDebugCallStack::~IDebugCallStack()
{
    StopMemLog();
}

#if !defined(DURANGO) && !defined(ORBIS) && !defined(_WIN32)
IDebugCallStack* IDebugCallStack::instance()
{
    static IDebugCallStack sInstance;
    return &sInstance;
}
#endif

void IDebugCallStack::FileCreationCallback(void (* postBackupProcess)())
{
    m_postBackupProcess = postBackupProcess;
}
//////////////////////////////////////////////////////////////////////////
void IDebugCallStack::LogCallstack()
{
    CollectCurrentCallStack();      // is updating m_functions

    WriteLineToLog("=============================================================================");
    int len = (int)m_functions.size();
    for (int i = 0; i < len; i++)
    {
        const char* str = m_functions[i].c_str();
        WriteLineToLog("%2d) %s", len - i, str);
    }
    WriteLineToLog("=============================================================================");
}

const char* IDebugCallStack::TranslateExceptionCode(DWORD dwExcept)
{
    switch (dwExcept)
    {
#if !defined(LINUX) && !defined(APPLE) && !defined(ORBIS)
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION";
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT";
        break;
    case EXCEPTION_BREAKPOINT:
        return "EXCEPTION_BREAKPOINT";
        break;
    case EXCEPTION_SINGLE_STEP:
        return "EXCEPTION_SINGLE_STEP";
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "EXCEPTION_FLT_DENORMAL_OPERAND";
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "EXCEPTION_FLT_INEXACT_RESULT";
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "EXCEPTION_FLT_INVALID_OPERATION";
        break;
    case EXCEPTION_FLT_OVERFLOW:
        return "EXCEPTION_FLT_OVERFLOW";
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        return "EXCEPTION_FLT_STACK_CHECK";
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        return "EXCEPTION_FLT_UNDERFLOW";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        break;
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW";
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        return "EXCEPTION_PRIV_INSTRUCTION";
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR";
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW";
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        return "EXCEPTION_INVALID_DISPOSITION";
        break;
    case EXCEPTION_GUARD_PAGE:
        return "EXCEPTION_GUARD_PAGE";
        break;
    case EXCEPTION_INVALID_HANDLE:
        return "EXCEPTION_INVALID_HANDLE";
        break;
    //case EXCEPTION_POSSIBLE_DEADLOCK: return "EXCEPTION_POSSIBLE_DEADLOCK";   break ;

    case STATUS_FLOAT_MULTIPLE_FAULTS:
        return "STATUS_FLOAT_MULTIPLE_FAULTS";
        break;
    case STATUS_FLOAT_MULTIPLE_TRAPS:
        return "STATUS_FLOAT_MULTIPLE_TRAPS";
        break;


#endif
    default:
        return "Unknown";
        break;
    }
}

void IDebugCallStack::PutVersion(char* str)
{
    if (!gEnv || !gEnv->pSystem)
    {
        return;
    }

    char sFileVersion[128];
    gEnv->pSystem->GetFileVersion().ToString(sFileVersion, sizeof(sFileVersion));

    char sProductVersion[128];
    gEnv->pSystem->GetProductVersion().ToString(sProductVersion, sizeof(sFileVersion));


    //! Get time.
    time_t ltime;
    time(&ltime);
    tm* today = localtime(&ltime);

    char s[1024];
    //! Use strftime to build a customized time string.
    strftime(s, 128, "Logged at %#c\n", today);
    strcat(str, s);
    sprintf_s(s, "FileVersion: %s\n", sFileVersion);
    strcat(str, s);
    sprintf_s(s, "ProductVersion: %s\n", sProductVersion);
    strcat(str, s);

    if (gEnv->pLog)
    {
        const char* logfile = gEnv->pLog->GetFileName();
        if (logfile)
        {
            sprintf (s, "LogFile: %s\n", logfile);
            strcat(str, s);
        }
    }

    if (gEnv->pConsole)
    {
        if (ICVar*  pCVarGameDir = gEnv->pConsole->GetCVar("sys_game_folder"))
        {
            sprintf(s, "GameDir: %s\n", pCVarGameDir->GetString());
            strcat(str, s);
        }
    }

#if !defined(LINUX) && !defined(APPLE) && !defined(DURANGO) && !defined(ORBIS)
    GetModuleFileNameA(NULL, s, sizeof(s));
    strcat(str, "Executable: ");
    strcat(str, s);
    strcat(str, "\n");
#endif
}


//Crash the application, in this way the debug callstack routine will be called and it will create all the necessary files (error.log, dump, and eventually screenshot)
void IDebugCallStack::FatalError(const char* description)
{
    m_bIsFatalError = true;
    WriteLineToLog(description);

#ifndef _RELEASE
    IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
    bool bShowDebugScreen = pOS && g_cvars.sys_no_crash_dialog == 0;
    // showing the debug screen is not safe when not called from mainthread
    // it normally leads to a infinity recursion followed by a stack overflow, preventing
    // useful call stacks, thus they are disabled
    bShowDebugScreen = bShowDebugScreen && gEnv->mMainThreadId == CryGetCurrentThreadId();
    if (bShowDebugScreen)
    {
        pOS->DebugMessageBox(description, "Lumberyard Fatal Error");
    }
#endif

#if defined(WIN32) || !defined(_RELEASE)
    int* p = 0x0;
    PREFAST_SUPPRESS_WARNING(6011) * p = 1; // we're intentionally crashing here
#endif
}

void IDebugCallStack::WriteLineToLog(const char* format, ...)
{
    CDebugAllowFileAccess allowFileAccess;

    va_list ArgList;
    char        szBuffer[MAX_WARNING_LENGTH];
    va_start(ArgList, format);
    int count = vsnprintf_s(szBuffer, sizeof(szBuffer), sizeof(szBuffer) - 1, format, ArgList);
    cry_strcat(szBuffer, "\n");
    szBuffer[sizeof(szBuffer) - 1] = '\0';
    va_end(ArgList);

    AZ::IO::HandleType fileHandle = AZ::IO::InvalidHandle;
    AZ::IO::FileIOBase::GetDirectInstance()->Open("@Log@\\error.log", AZ::IO::GetOpenModeFromStringMode("a+t"), fileHandle);
    if (fileHandle != AZ::IO::InvalidHandle)
    {
        AZ::IO::FileIOBase::GetDirectInstance()->Write(fileHandle, szBuffer, strlen(szBuffer));
        AZ::IO::FileIOBase::GetDirectInstance()->Flush(fileHandle);
        AZ::IO::FileIOBase::GetDirectInstance()->Close(fileHandle);
    }
}

void IDebugCallStack::Screenshot(const char* szFileName)
{
    WriteLineToLog("Attempting to create error screenshot \"%s\"", szFileName);

    static int g_numScreenshots = 0;
    if (gEnv->pRenderer && !g_numScreenshots++)
    {
        if (gEnv->pRenderer->ScreenShot(szFileName))
        {
            WriteLineToLog("Successfully created screenshot.");
        }
        else
        {
            WriteLineToLog("Error creating screenshot.");
        }
    }
    else
    {
        WriteLineToLog("Ignoring multiple calls to Screenshot");
    }
}

//////////////////////////////////////////////////////////////////////////
void IDebugCallStack::StartMemLog()
{
    AZ::IO::FileIOBase::GetDirectInstance()->Open("@Log@\\memallocfile.log", AZ::IO::OpenMode::ModeWrite, m_memAllocFileHandle);

    assert(m_memAllocFileHandle != AZ::IO::InvalidHandle);
}

//////////////////////////////////////////////////////////////////////////
void IDebugCallStack::StopMemLog()
{
    if (m_memAllocFileHandle != AZ::IO::InvalidHandle)
    {
        AZ::IO::FileIOBase::GetDirectInstance()->Close(m_memAllocFileHandle);
        m_memAllocFileHandle = AZ::IO::InvalidHandle;
    }
}
//#endif //!defined(LINUX)
