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

#include "StdAfx.h"
#include "DebugCallStack.h"

#if defined(WIN32) || defined(WIN64)

#include <IConsole.h>
#include <IScriptSystem.h>
#include "System.h"

#include <AzCore/Debug/EventTraceDrillerBus.h>

#include "resource.h"
LINK_SYSTEM_LIBRARY(version.lib)

//! Needs one external of DLL handle.
extern HMODULE gDLLHandle;

#pragma warning(push)
#pragma warning(disable : 4091) // Needed to bypass the "'typedef ': ignored on left of '' when no variable is declared" brought in by DbgHelp.h
#include <DbgHelp.h>
#pragma warning(pop)

#pragma comment( lib, "dbghelp" )
#pragma warning(disable: 4244)

#define MAX_PATH_LENGTH 1024
#define MAX_SYMBOL_LENGTH 512

static HWND hwndException = 0;
static bool g_bUserDialog = true;         // true=on crash show dialog box, false=supress user interaction

static int  PrintException(EXCEPTION_POINTERS* pex);

static bool IsFloatingPointException(EXCEPTION_POINTERS* pex);

extern LONG WINAPI CryEngineExceptionFilterWER(struct _EXCEPTION_POINTERS* pExceptionPointers);
extern LONG WINAPI CryEngineExceptionFilterMiniDump(struct _EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath, MINIDUMP_TYPE mdumpValue);

//=============================================================================
CONTEXT CaptureCurrentContext()
{
    CONTEXT context;
    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    return context;
}

LONG __stdcall CryUnhandledExceptionHandler(EXCEPTION_POINTERS* pex)
{
    return DebugCallStack::instance()->handleException(pex);
}


BOOL CALLBACK EnumModules(
    PCSTR   ModuleName,
    DWORD64 BaseOfDll,
    PVOID   UserContext)
{
    DebugCallStack::TModules& modules = *static_cast<DebugCallStack::TModules*>(UserContext);
    modules[(void*)BaseOfDll] = ModuleName;

    return TRUE;
}
//=============================================================================
// Class Statics
//=============================================================================

// Return single instance of class.
IDebugCallStack* IDebugCallStack::instance()
{
    static DebugCallStack sInstance;
    return &sInstance;
}

//------------------------------------------------------------------------------------------------------------------------
// Sets up the symbols for functions in the debug file.
//------------------------------------------------------------------------------------------------------------------------
DebugCallStack::DebugCallStack()
    : prevExceptionHandler(0)
    , m_pSystem(0)
    , m_symbols(false)
    , m_nSkipNumFunctions(0)
    , m_bCrash(false)
    , m_szBugMessage(NULL)
{
}

DebugCallStack::~DebugCallStack()
{
}

/*
BOOL CALLBACK func_PSYM_ENUMSOURCFILES_CALLBACK( PSOURCEFILE pSourceFile, PVOID UserContext )
{
    CryLogAlways( pSourceFile->FileName );
    return TRUE;
}

BOOL CALLBACK func_PSYM_ENUMMODULES_CALLBACK64(
                                                                                PSTR ModuleName,
                                                                                DWORD64 BaseOfDll,
                                                                                PVOID UserContext
                                                                                )
{
    CryLogAlways( "<SymModule> %s: %x",ModuleName,(uint32)BaseOfDll );
    return TRUE;
}

BOOL CALLBACK func_PSYM_ENUMERATESYMBOLS_CALLBACK(
    PSYMBOL_INFO  pSymInfo,
    ULONG         SymbolSize,
    PVOID         UserContext
    )
{
    CryLogAlways( "<Symbol> %08X Size=%08X  :%s",(uint32)pSymInfo->Address,(uint32)pSymInfo->Size,pSymInfo->Name );
    return TRUE;
}
*/

bool DebugCallStack::initSymbols()
{
#ifndef WIN98
    if (m_symbols)
    {
        return true;
    }

    char fullpath[MAX_PATH_LENGTH + 1];
    char pathname[MAX_PATH_LENGTH + 1];
    char fname[MAX_PATH_LENGTH + 1];
    char directory[MAX_PATH_LENGTH + 1];
    char drive[10];

    {
        // Print dbghelp version.
        HMODULE dbgHelpDll = GetModuleHandle("dbghelp.dll");

        char ver[1024 * 8];
        GetModuleFileName(dbgHelpDll, fullpath, _MAX_PATH);
        int fv[4];

        DWORD dwHandle;
        int verSize = GetFileVersionInfoSize(fullpath, &dwHandle);
        if (verSize > 0)
        {
            unsigned int len;
            GetFileVersionInfo(fullpath, dwHandle, 1024 * 8, ver);
            VS_FIXEDFILEINFO* vinfo;
            VerQueryValue(ver, "\\", (void**)&vinfo, &len);

            fv[0] = vinfo->dwFileVersionLS & 0xFFFF;
            fv[1] = vinfo->dwFileVersionLS >> 16;
            fv[2] = vinfo->dwFileVersionMS & 0xFFFF;
            fv[3] = vinfo->dwFileVersionMS >> 16;

            //          WriteLineToLog( "dbghelp.dll version %d.%d.%d.%d",fv[3],fv[2],fv[1],fv[0] );
        }
    }

    //  SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_OMAP_FIND_NEAREST|SYMOPT_INCLUDE_32BIT_MODULES);
    //DWORD res1 = SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_OMAP_FIND_NEAREST);

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_LOAD_ANYTHING | SYMOPT_LOAD_LINES);


    HANDLE hProcess = GetCurrentProcess();

    // Get module file name.
    GetModuleFileName(NULL, fullpath, MAX_PATH_LENGTH);

    // Convert it into search path for symbols.
    cry_strcpy(pathname, fullpath);
    _splitpath(pathname, drive, directory, fname, NULL);
    sprintf_s(pathname, "%s%s", drive, directory);

    // Append the current directory to build a search path forSymInit
    cry_strcat(pathname, ";.;");

    int result = 0;

    m_symbols = false;

    // each call to SymInitialize must have a matching call to SymCleanup
    // so set m_symbols to true whenever it works.
    // we first try the admin, invasive method by setting this to TRUE.
    // this allows it to dig into every attached DLL and module, but may require privelege:
    result = SymInitialize(hProcess, pathname, TRUE);
    if (!result)
    {
        // if this doesn't work, try the light touch.
        result = SymInitialize(hProcess, pathname, FALSE);
    }

    if (result)
    {
        // once we're in, we set paths and refresh the list after doing so:
        SymSetSearchPath(hProcess, pathname);
        SymRefreshModuleList(hProcess);
        SymEnumerateModules64(hProcess, EnumModules, &m_modules);

        m_symbols = true;
    }
    else
    {
        WriteLineToLog("<CrySystem> SymInitialize failed");
    }
#else
    return false;
#endif

    return result != 0;
}

void DebugCallStack::doneSymbols()
{
#ifndef WIN98
    if (m_symbols)
    {
        SymCleanup(GetCurrentProcess());
    }
    m_symbols = false;
#endif
}

void DebugCallStack::RemoveOldFiles()
{
    RemoveFile("error.log");
    RemoveFile("error.bmp");
    RemoveFile("error.dmp");
}

void DebugCallStack::RemoveFile(const char* szFileName)
{
    FILE* pFile = fopen(szFileName, "r");
    const bool bFileExists = (pFile != NULL);

    if (bFileExists)
    {
        fclose(pFile);

        WriteLineToLog("Removing file \"%s\"...", szFileName);
        if (remove(szFileName) == 0)
        {
            WriteLineToLog("File successfully removed.");
        }
        else
        {
            WriteLineToLog("Couldn't remove file!");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::CollectCurrentCallStack(int maxStackEntries)
{
    if (!initSymbols())
    {
        return;
    }

    m_functions.clear();

    memset(&m_context, 0, sizeof(m_context));
    m_context.ContextFlags = CONTEXT_FULL;

    GetThreadContext(GetCurrentThread(), &m_context);

    m_nSkipNumFunctions = 2;

    FillStackTrace(maxStackEntries);
}

//------------------------------------------------------------------------------------------------------------------------
static int callCount = 0;
int DebugCallStack::updateCallStack(EXCEPTION_POINTERS* pex)
{
    if (callCount > 0)
    {
        if (prevExceptionHandler)
        {
            // uninstall our exception handler.
            SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)prevExceptionHandler);
        }
        // Immidiate termination of process.
        abort();
    }
    callCount++;

    HANDLE process = GetCurrentProcess();

    //! Find source line at exception address.
    //m_excLine = lookupFunctionName( (void*)pex->ExceptionRecord->ExceptionAddress,true );

    //! Find Name of .DLL from Exception address.
    strcpy(m_excModule, "<Unknown>");

    if (m_symbols && pex)
    {
        DWORD64 dwAddr = SymGetModuleBase64(process, (DWORD64)pex->ExceptionRecord->ExceptionAddress);
        if (dwAddr)
        {
            char szBuff[MAX_PATH_LENGTH];
            if (GetModuleFileName((HMODULE)dwAddr, szBuff, MAX_PATH_LENGTH))
            {
                strcpy(m_excModule, szBuff);

                char fdir[_MAX_PATH];
                char fdrive[_MAX_PATH];
                char file[_MAX_PATH];
                char fext[_MAX_PATH];
                _splitpath(m_excModule, fdrive, fdir, file, fext);
                _makepath(fdir, NULL, NULL, file, fext);

                strcpy(m_excModule, fdir);
            }
        }
    }

    // Fill stack trace info.
    if (pex)
    {
        m_context = *pex->ContextRecord;
    }
    m_nSkipNumFunctions = 0;
    FillStackTrace();

    return EXCEPTION_CONTINUE_EXECUTION;
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::FillStackTrace(int maxStackEntries, HANDLE hThread)
{
    HANDLE hProcess = GetCurrentProcess();

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    int count;
    STACKFRAME64 stack_frame;
    BOOL b_ret = TRUE; //Setup stack frame
    memset(&stack_frame, 0, sizeof(stack_frame));
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;
    stack_frame.AddrReturn.Mode = AddrModeFlat;
    stack_frame.AddrBStore.Mode = AddrModeFlat;

    DWORD MachineType = IMAGE_FILE_MACHINE_I386;

#if defined(_M_IX86)
    MachineType                   = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrPC.Offset     = m_context.Eip;
    stack_frame.AddrStack.Offset  = m_context.Esp;
    stack_frame.AddrFrame.Offset  = m_context.Ebp;
#elif defined(_M_X64)
    MachineType                   = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset     = m_context.Rip;
    stack_frame.AddrStack.Offset  = m_context.Rsp;
    stack_frame.AddrFrame.Offset  = m_context.Rdi;
#endif

    m_functions.clear();

    //WriteLineToLog( "Start StackWalk" );
    //WriteLineToLog( "eip=%p, esp=%p, ebp=%p",m_context.Eip,m_context.Esp,m_context.Ebp );

    //While there are still functions on the stack..
    for (count = 0; count < maxStackEntries && b_ret == TRUE; count++)
    {
        b_ret = StackWalk64(MachineType,   hProcess, hThread, &stack_frame, &m_context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);

        if (count < m_nSkipNumFunctions)
        {
            continue;
        }

        if (m_symbols)
        {
            string funcName = LookupFunctionName((void*)stack_frame.AddrPC.Offset, true);
            if (!funcName.empty())
            {
                m_functions.push_back(funcName);
            }
            else
            {
                DWORD64 p = (DWORD64)stack_frame.AddrPC.Offset;
                char str[80];
                sprintf_s(str, "function=0x%p", p);
                m_functions.push_back(str);
            }
        }
        else
        {
            DWORD64 p = (DWORD64)stack_frame.AddrPC.Offset;
            char str[80];
            sprintf_s(str, "function=0x%p", p);
            m_functions.push_back(str);
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------
string DebugCallStack::LookupFunctionName(void* address, bool fileInfo)
{
    string fileName, symName;
    int lineNumber;
    void* baseAddr;
    LookupFunctionName(address, fileInfo, symName, fileName, lineNumber, baseAddr);
    symName += "()";
    if (fileInfo)
    {
        char lineNum[1024];
        itoa(lineNumber, lineNum, 10);
        string path;

        char file[1024];
        char fname[1024];
        char fext[1024];
        _splitpath(fileName.c_str(), NULL, NULL, fname, fext);
        _makepath(file, NULL, NULL, fname, fext);
        symName += string("  [") + file + ":" + lineNum + "]";
    }
    return symName;
}

bool DebugCallStack::LookupFunctionName(void* address, bool fileInfo, string& proc, string& file, int& line, void*& baseAddr)
{
    proc = "";
    file = "";
    line = 0;
    baseAddr = address;
#ifndef WIN98
    HANDLE process = GetCurrentProcess();
    char symbolBuf[sizeof(SYMBOL_INFO) + MAX_SYMBOL_LENGTH + 1];
    memset(symbolBuf, 0, sizeof(symbolBuf));
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuf;

    DWORD displacement = 0;
    DWORD64 displacement64 = 0;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYMBOL_LENGTH;
    if (SymFromAddr(process, (DWORD64)address, &displacement64, pSymbol))
    {
        proc = string(pSymbol->Name);
        baseAddr = (void*)((UINT_PTR)address - displacement64);
    }
    else
    {
#if defined(_M_IX86)
        proc.Format("[%08X]", address);
#elif defined(_M_X64)
        proc.Format("[%016llX]", address);
#endif
        return false;
    }

    if (fileInfo)
    {
        // Lookup Line in source file.
        IMAGEHLP_LINE64 lineImg;
        memset(&lineImg, 0, sizeof(lineImg));
        lineImg.SizeOfStruct = sizeof(lineImg);

        if (SymGetLineFromAddr64(process, (DWORD_PTR)address, &displacement, &lineImg))
        {
            file = lineImg.FileName;
            line = lineImg.LineNumber;
        }
        return true;
    }
#endif

    return false;
}

void DebugCallStack::installErrorHandler(ISystem* pSystem)
{
    m_pSystem = pSystem;
    prevExceptionHandler = (void*)SetUnhandledExceptionFilter(CryUnhandledExceptionHandler);
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::SetUserDialogEnable(const bool bUserDialogEnable)
{
    g_bUserDialog = bUserDialogEnable;
}


DWORD g_idDebugThreads[10];
const char* g_nameDebugThreads[10];
int g_nDebugThreads = 0;
volatile int g_lockThreadDumpList = 0;

void MarkThisThreadForDebugging(const char* name)
{
    EBUS_EVENT(AZ::Debug::EventTraceDrillerSetupBus, SetThreadName, AZStd::this_thread::get_id(), name);

    WriteLock lock(g_lockThreadDumpList);
    DWORD id = GetCurrentThreadId();
    if (g_nDebugThreads == sizeof(g_idDebugThreads) / sizeof(g_idDebugThreads[0]))
    {
        return;
    }
    for (int i = 0; i < g_nDebugThreads; i++)
    {
        if (g_idDebugThreads[i] == id)
        {
            return;
        }
    }
    g_nameDebugThreads[g_nDebugThreads] = name;
    g_idDebugThreads[g_nDebugThreads++] = id;
    ((CSystem*)gEnv->pSystem)->EnableFloatExceptions(g_cvars.sys_float_exceptions);
}

void UnmarkThisThreadFromDebugging()
{
    WriteLock lock(g_lockThreadDumpList);
    DWORD id = GetCurrentThreadId();
    for (int i = g_nDebugThreads - 1; i >= 0; i--)
    {
        if (g_idDebugThreads[i] == id)
        {
            memmove(g_idDebugThreads + i, g_idDebugThreads + i + 1, (g_nDebugThreads - 1 - i) * sizeof(g_idDebugThreads[0]));
            memmove(g_nameDebugThreads + i, g_nameDebugThreads + i + 1, (g_nDebugThreads - 1 - i) * sizeof(g_nameDebugThreads[0]));
            --g_nDebugThreads;
        }
    }
}

extern int prev_sys_float_exceptions;
void UpdateFPExceptionsMaskForThreads()
{
    int mask = -iszero(g_cvars.sys_float_exceptions);
    CONTEXT ctx;
    for (int i = 0; i < g_nDebugThreads; i++)
    {
        if (g_idDebugThreads[i] != GetCurrentThreadId())
        {
            HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, TRUE, g_idDebugThreads[i]);
            ctx.ContextFlags = CONTEXT_ALL;
            SuspendThread(hThread);
            GetThreadContext(hThread, &ctx);
#ifndef WIN64
            (ctx.FloatSave.ControlWord |= 7) &= ~5 | mask;
            (*(WORD*)(ctx.ExtendedRegisters + 24) |= 0x280) &= ~0x280 | mask;
#else
            (ctx.FltSave.ControlWord |= 7) &= ~5 | mask;
            (ctx.FltSave.MxCsr |= 0x280) &= ~0x280  | mask;
#endif
            SetThreadContext(hThread, &ctx);
            ResumeThread(hThread);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
int DebugCallStack::handleException(EXCEPTION_POINTERS* exception_pointer)
{
    if (gEnv == NULL)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    gEnv->pLog->FlushAndClose();

    ResetFPU(exception_pointer);

    prev_sys_float_exceptions = 0;
    const int cached_sys_float_exceptions = g_cvars.sys_float_exceptions;

    ((CSystem*)gEnv->pSystem)->EnableFloatExceptions(0);

    if (g_cvars.sys_WER)
    {
        return CryEngineExceptionFilterWER(exception_pointer);
    }

    if (g_cvars.sys_no_crash_dialog)
    {
        DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
        SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);
    }

    m_bCrash = true;

    if (g_cvars.sys_no_crash_dialog)
    {
        DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
        SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);
    }

    static bool firstTime = true;

    if (g_cvars.sys_dump_aux_threads | g_cvars.sys_keyboard_break)
    {
        for (int i = 0; i < g_nDebugThreads; i++)
        {
            if (g_idDebugThreads[i] != GetCurrentThreadId())
            {
                SuspendThread(OpenThread(THREAD_ALL_ACCESS, TRUE, g_idDebugThreads[i]));
            }
        }
    }

    // uninstall our exception handler.
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)prevExceptionHandler);

    if (!firstTime)
    {
        WriteLineToLog("Critical Exception! Called Multiple Times!");
        // Exception called more then once.
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // Print exception info:
    {
        char excCode[80];
        char excAddr[80];
        WriteLineToLog("<CRITICAL EXCEPTION>");
        sprintf_s(excAddr, "0x%04X:0x%p", exception_pointer->ContextRecord->SegCs, exception_pointer->ExceptionRecord->ExceptionAddress);
        sprintf_s(excCode, "0x%08X", exception_pointer->ExceptionRecord->ExceptionCode);
        WriteLineToLog("Exception: %s, at Address: %s", excCode, excAddr);

        if (CSystem* pSystem = (CSystem*)GetSystem())
        {
            if (const char* pLoadingProfilerCallstack = pSystem->GetLoadingProfilerCallstack())
            {
                if (pLoadingProfilerCallstack[0])
                {
                    WriteLineToLog("<CrySystem> LoadingProfilerCallstack: %s", pLoadingProfilerCallstack);
                }
            }
        }

        {
            IMemoryManager::SProcessMemInfo memInfo;
            if (gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo(memInfo))
            {
                uint32 nMemUsage = (uint32)(memInfo.PagefileUsage / (1024 * 1024));
                WriteLineToLog("Virtual memory usage: %dMb", nMemUsage);
            }
            gEnv->szDebugStatus[SSystemGlobalEnvironment::MAX_DEBUG_STRING_LENGTH - 1] = '\0';
            WriteLineToLog("Debug Status: %s", gEnv->szDebugStatus);
        }

        if (gEnv->pRenderer)
        {
            ID3DDebugMessage* pMsg = 0;
            gEnv->pRenderer->EF_Query(EFQ_GetLastD3DDebugMessage, pMsg);
            if (pMsg)
            {
                const char* pStr = pMsg->GetMessage();
                WriteLineToLog("Last D3D debug message: %s", pStr ? pStr : "#unknown#");
                SAFE_RELEASE(pMsg);
            }
        }
    }

    firstTime = false;

    const int ret = SubmitBug(exception_pointer);

    if (ret != IDB_IGNORE)
    {
        CryEngineExceptionFilterWER(exception_pointer);
    }

    if (exception_pointer->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
    {
        // This is non continuable exception. abort application now.
        exit(1);
    }

    //typedef long (__stdcall *ExceptionFunc)(EXCEPTION_POINTERS*);
    //ExceptionFunc prevFunc = (ExceptionFunc)prevExceptionHandler;
    //return prevFunc( (EXCEPTION_POINTERS*)exception_pointer );
    if (ret == IDB_EXIT)
    {
        // Immediate exit.
        // on windows, exit() and _exit() do all sorts of things, unfortuantely
        // TerminateProcess is the only way to die.
        TerminateProcess(GetCurrentProcess(), 1);  // we crashed, so don't return a zero exit code!
        // on linux based systems, _exit will not call ATEXIT and other things, which makes it more suitable for termination in an emergency such
        // as an unhandled exception.
        // however, this function is a windows exception handler.
    }
    else if (ret == IDB_IGNORE)
    {
#ifndef WIN64
        exception_pointer->ContextRecord->FloatSave.StatusWord &= ~31;
        exception_pointer->ContextRecord->FloatSave.ControlWord |= 7;
        (*(WORD*)(exception_pointer->ContextRecord->ExtendedRegisters + 24) &= 31) |= 0x1F80;
#else
        exception_pointer->ContextRecord->FltSave.StatusWord &= ~31;
        exception_pointer->ContextRecord->FltSave.ControlWord |= 7;
        (exception_pointer->ContextRecord->FltSave.MxCsr &= 31) |= 0x1F80;
#endif
        firstTime = true;
        callCount = 0;
        prevExceptionHandler = (void*)SetUnhandledExceptionFilter(CryUnhandledExceptionHandler);
        g_cvars.sys_float_exceptions = cached_sys_float_exceptions;
        ((CSystem*)gEnv->pSystem)->EnableFloatExceptions(g_cvars.sys_float_exceptions);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // Continue;
    return EXCEPTION_EXECUTE_HANDLER;
}

void DebugCallStack::ReportBug(const char* szErrorMessage)
{
    WriteLineToLog("Reporting bug: %s", szErrorMessage);

    m_szBugMessage = szErrorMessage;
    m_context = CaptureCurrentContext();
    SubmitBug(NULL);
    --callCount;
    m_szBugMessage = NULL;
}

void DebugCallStack::dumpCallStack(std::vector<string>& funcs)
{
    WriteLineToLog("=============================================================================");
    int len = (int)funcs.size();
    for (int i = 0; i < len; i++)
    {
        const char* str = funcs[i].c_str();
        WriteLineToLog("%2d) %s", len - i, str);
    }
    WriteLineToLog("=============================================================================");
}

void DebugCallStack::LogMemCallstackFile(int memSize)
{
    if (m_memAllocFileHandle == AZ::IO::InvalidHandle)
    {
        return;
    }

    CollectCurrentCallStack(MAX_DEBUG_STACK_ENTRIES_FILE_DUMP);     // is updating m_functions

    char buffer[16];
    itoa(memSize, buffer, 10);
    CryFixedStringT<64> temp("*** Memory allocation for ");
    temp.append(buffer);
    temp.append(" bytes ");
    int frame = gEnv->pRenderer->GetFrameID(false);
    itoa(frame, buffer, 10);
    temp.append("in frame ");
    temp.append(buffer);
    temp.append("****\n");
    AZ::IO::FileIOBase::GetDirectInstance()->Write(m_memAllocFileHandle, temp.c_str(), temp.size());
    int len = (int)m_functions.size();
    for (int i = 0; i < len; i++)
    {
        const char* str = m_functions[i].c_str();
        itoa(len - i, buffer, 10);
        temp = buffer;
        temp.append(" ");
        temp.append(str);
        temp.append("\n");
        AZ::IO::FileIOBase::GetDirectInstance()->Write(m_memAllocFileHandle, temp.c_str(), temp.size());
    }
    temp = "=============================================================================\n";
    AZ::IO::FileIOBase::GetDirectInstance()->Write(m_memAllocFileHandle, temp.c_str(), temp.size());
}



//////////////////////////////////////////////////////////////////////////
void DebugCallStack::LogExceptionInfo(EXCEPTION_POINTERS* pex)
{
    CDebugAllowFileAccess ignoreInvalidFileAccess;

    static char errorString[s_iCallStackSize];
    errorString[0] = 0;

    // Time and Version.
    char versionbuf[1024];
    strcpy(versionbuf, "");
    PutVersion(versionbuf);
    cry_strcat(errorString, versionbuf);
    cry_strcat(errorString, "\n");

    char excCode[MAX_WARNING_LENGTH];
    char excAddr[80];
    char desc[1024];
    char excDesc[MAX_WARNING_LENGTH];

    // make sure the mouse cursor is visible
    ShowCursor(TRUE);

    const char* excName;
    if (m_bIsFatalError || !pex)
    {
        const char* const szMessage = m_bIsFatalError ? s_szFatalErrorCode : m_szBugMessage;
        excName = szMessage;
        cry_strcpy(excCode, szMessage);
        cry_strcpy(excAddr, "");
        cry_strcpy(desc, "");
        cry_strcpy(m_excModule, "");
        cry_strcpy(excDesc, szMessage);
    }
    else
    {
        sprintf_s(excAddr, "0x%04X:0x%p", pex->ContextRecord->SegCs, pex->ExceptionRecord->ExceptionAddress);
        sprintf_s(excCode, "0x%08X", pex->ExceptionRecord->ExceptionCode);
        excName = TranslateExceptionCode(pex->ExceptionRecord->ExceptionCode);
        cry_strcpy(desc, "");
        sprintf_s(excDesc, "%s\r\n%s", excName, desc);


        if (pex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        {
            if (pex->ExceptionRecord->NumberParameters > 1)
            {
                int iswrite = pex->ExceptionRecord->ExceptionInformation[0];
                DWORD64 accessAddr = pex->ExceptionRecord->ExceptionInformation[1];
                if (iswrite)
                {
                    sprintf_s(desc, "Attempt to write data to address 0x%08p\r\nThe memory could not be \"written\"", accessAddr);
                }
                else
                {
                    sprintf_s(desc, "Attempt to read from address 0x%08p\r\nThe memory could not be \"read\"", accessAddr);
                }
            }
        }
    }


    WriteLineToLog("Exception Code: %s", excCode);
    WriteLineToLog("Exception Addr: %s", excAddr);
    WriteLineToLog("Exception Module: %s", m_excModule);
    WriteLineToLog("Exception Name  : %s", excName);
    WriteLineToLog("Exception Description: %s", desc);


    cry_strcpy(m_excDesc, excDesc);
    cry_strcpy(m_excAddr, excAddr);
    cry_strcpy(m_excCode, excCode);


    char errs[32768];
    sprintf_s(errs, "Exception Code: %s\nException Addr: %s\nException Module: %s\nException Description: %s, %s\n",
        excCode, excAddr, m_excModule, excName, desc);


    IMemoryManager::SProcessMemInfo memInfo;
    if (gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo(memInfo))
    {
        char memoryString[256];
        double MB = 1024 * 1024;
        sprintf_s(memoryString, "Memory in use: %3.1fMB\n", (double)(memInfo.PagefileUsage) / MB);
        cry_strcat(errs, memoryString);
    }
    {
        const int tempStringSize = 256;
        char tempString[tempStringSize];

        gEnv->szDebugStatus[SSystemGlobalEnvironment::MAX_DEBUG_STRING_LENGTH - 1] = '\0';
        sprintf_s(tempString, tempStringSize, "Debug Status: %s\n", gEnv->szDebugStatus);
        cry_strcat(errs, tempString);

        sprintf_s(tempString, tempStringSize, "Out of Memory: %d\n", gEnv->bIsOutOfMemory);
        cry_strcat(errs, tempString);
    }
    cry_strcat(errs, "\nCall Stack Trace:\n");

    std::vector<string> funcs;
    if (gEnv->bIsOutOfMemory)
    {
        cry_strcat(errs, "1) OUT_OF_MEMORY()\n");
    }
    else
    {
        getCallStack(funcs);
        dumpCallStack(funcs);
        // Fill call stack.
        char str[s_iCallStackSize];
        cry_strcpy(str, "");
        for (unsigned int i = 0; i < funcs.size(); i++)
        {
            char temp[s_iCallStackSize];
            sprintf_s(temp, "%2d) %s", funcs.size() - i, (const char*)funcs[i].c_str());
            cry_strcat(str, temp);
            cry_strcat(str, "\r\n");
            cry_strcat(errs, temp);
            cry_strcat(errs, "\n");
        }
        cry_strcpy(m_excCallstack, str);
    }

    cry_strcat(errorString, errs);

    string path("");

    if ((gEnv) && (gEnv->pFileIO))
    {
        const char* logAlias = nullptr;
        if (
            (logAlias = gEnv->pFileIO->GetAlias("@log@")) ||
            (logAlias = gEnv->pFileIO->GetAlias("@root@"))
            )
        {
            path = logAlias;
            path += "/";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    string fileName = path;
    fileName += "error.log";

#if defined(DEDICATED_SERVER)
    string backupPath = PathUtil::ToUnixPath(PathUtil::AddSlash(path + "DumpBackups"));
    gEnv->pFileIO->CreatePath(backupPath.c_str());

    struct stat fileInfo;
    string timeStamp;

    if (stat(fileName.c_str(), &fileInfo) == 0)
    {
        // Backup log
        tm* creationTime = localtime(&fileInfo.st_mtime);
        char tempBuffer[32];
        strftime(tempBuffer, sizeof(tempBuffer), "%d %b %Y (%H %M %S)", creationTime);
        timeStamp = tempBuffer;

        string backupFileName = backupPath + timeStamp + " error.log";
        CopyFile(fileName.c_str(), backupFileName.c_str(), true);
    }
#endif // defined(DEDICATED_SERVER)

    FILE* f = fopen(fileName.c_str(), "wt");
    if (f)
    {
        fwrite(errorString, strlen(errorString), 1, f);
        if (!gEnv->bIsOutOfMemory)
        {
            if (g_cvars.sys_dump_aux_threads | g_cvars.sys_keyboard_break)
            {
                funcs.clear();
                for (int i = 0; i < g_nDebugThreads; i++)
                {
                    if (g_idDebugThreads[i] != GetCurrentThreadId())
                    {
                        fprintf(f, "\n\nSuspended thread (%s):\n", g_nameDebugThreads[i]);
                        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, TRUE, g_idDebugThreads[i]);
                        GetThreadContext(hThread, &m_context);
                        m_nSkipNumFunctions = 0;
                        FillStackTrace(10, hThread);
                        getCallStack(funcs);
                        for (uint32 i = 0; i < funcs.size(); i++)
                        {
                            fprintf(f, "%2d) %s\n", funcs.size() - i, funcs[i].c_str());
                        }
                        ResumeThread(hThread);
                    }
                }
            }
        }
        fflush(f);
        fclose(f);
    }

    if (pex)
    {
        MINIDUMP_TYPE mdumpValue;
        bool bDump = true;
        switch (g_cvars.sys_dump_type)
        {
        case 0:
            bDump = false;
            break;
        case 1:
            mdumpValue = MiniDumpNormal;
            break;
        case 2:
            mdumpValue = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs);
            break;
        case 3:
            mdumpValue = MiniDumpWithFullMemory;
            break;
        default:
            mdumpValue = (MINIDUMP_TYPE)g_cvars.sys_dump_type;
            break;
        }
        if (bDump)
        {
            fileName = path + "error.dmp";
#if defined(DEDICATED_SERVER)
            if (stat(fileName.c_str(), &fileInfo) == 0)
            {
                // Backup dump (use timestamp from error.log if available)
                if (timeStamp.empty())
                {
                    tm* creationTime = localtime(&fileInfo.st_mtime);
                    char tempBuffer[32];
                    strftime(tempBuffer, sizeof(tempBuffer), "%d %b %Y (%H %M %S)", creationTime);
                    timeStamp = tempBuffer;
                }

                string backupFileName = backupPath + timeStamp + " error.dmp";
                CopyFile(fileName.c_str(), backupFileName.c_str(), true);
            }
#endif // defined(DEDICATED_SERVER)

            CryEngineExceptionFilterMiniDump(pex, fileName.c_str(), mdumpValue);
        }
    }

#if !defined(DEDICATED_SERVER)
    Screenshot("@user@/ScreenShots/error.jpg");
#endif // !defined(DEDICATED_SERVER)

    //if no crash dialog don't even submit the bug
    if (m_postBackupProcess && g_cvars.sys_no_crash_dialog == 0 && g_bUserDialog)
    {
        m_postBackupProcess();
    }
    else
    {
        // lawsonn: Disabling the JIRA-based crash reporter for now
        // we'll need to deal with it our own way, pending QA.
        // if you're customizing the engine this is also your opportunity to deal with it.
        if (g_cvars.sys_no_crash_dialog != 0 || !g_bUserDialog)
        {
            // ------------ place custom crash handler here ---------------------
            // it should launch an executable!
            /// by  this time, error.bmp will be in the engine root folder
            // error.log and error.dmp will also be present in the engine root folder
            // if your error dumper wants those, it should zip them up and send them or offer to do so.
            // ------------------------------------------------------------------
        }
    }
    const bool bQuitting = !gEnv || !gEnv->pSystem || gEnv->pSystem->IsQuitting();

    //[AlexMcC|16.04.10] When the engine is shutting down, MessageBox doesn't display a box
    // and immediately returns IDYES. Avoid this by just not trying to save if we're quitting.
    // Don't ask to save if this isn't a real crash (a real crash has exception pointers)
    if (g_cvars.sys_no_crash_dialog == 0 && g_bUserDialog && gEnv->IsEditor() && !bQuitting && pex)
    {
        BackupCurrentLevel();

        const int res = DialogBoxParam(gDLLHandle, MAKEINTRESOURCE(IDD_CONFIRM_SAVE_LEVEL), NULL, DebugCallStack::ConfirmSaveDialogProc, NULL);
        if (res == IDB_CONFIRM_SAVE)
        {
            if (SaveCurrentLevel())
            {
                MessageBox(NULL, "Level has been successfully saved!\r\nPress Ok to terminate Editor.", "Save", MB_OK);
            }
            else
            {
                MessageBox(NULL, "Error saving level.\r\nPress Ok to terminate Editor.", "Save", MB_OK | MB_ICONWARNING);
            }
        }
    }

    if (g_cvars.sys_no_crash_dialog != 0 || !g_bUserDialog)
    {
        // terminate immediately - since we're in a crash, there is no point unwinding stack, we've already done access violation or worse.
        // calling exit will only cause further death down the line...
        TerminateProcess(GetCurrentProcess(), 1);
    }
}


INT_PTR CALLBACK DebugCallStack::ExceptionDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static EXCEPTION_POINTERS* pex;

    static char errorString[32768] = "";

    switch (message)
    {
    case WM_INITDIALOG:
    {
        pex = (EXCEPTION_POINTERS*)lParam;
        HWND h;

        if (pex->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
        {
            // Disable continue button for non continuable exceptions.
            //h = GetDlgItem( hwndDlg,IDB_CONTINUE );
            //if (h) EnableWindow( h,FALSE );
        }

        DebugCallStack* pDCS = static_cast<DebugCallStack*>(DebugCallStack::instance());

        h = GetDlgItem(hwndDlg, IDC_EXCEPTION_DESC);
        if (h)
        {
            SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excDesc);
        }

        h = GetDlgItem(hwndDlg, IDC_EXCEPTION_CODE);
        if (h)
        {
            SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excCode);
        }

        h = GetDlgItem(hwndDlg, IDC_EXCEPTION_MODULE);
        if (h)
        {
            SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excModule);
        }

        h = GetDlgItem(hwndDlg, IDC_EXCEPTION_ADDRESS);
        if (h)
        {
            SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excAddr);
        }

        // Fill call stack.
        HWND callStack = GetDlgItem(hwndDlg, IDC_CALLSTACK);
        if (callStack)
        {
            SendMessage(callStack, WM_SETTEXT, FALSE, (LPARAM)pDCS->m_excCallstack);
        }

        if (hwndException)
        {
            DestroyWindow(hwndException);
            hwndException = 0;
        }

        if (IsFloatingPointException(pex))
        {
            EnableWindow(GetDlgItem(hwndDlg, IDB_IGNORE), TRUE);
        }
    }
    break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDB_EXIT:
        case IDB_IGNORE:
            // Fall through.

            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
    }
    return FALSE;
}

INT_PTR CALLBACK DebugCallStack::ConfirmSaveDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // The user might be holding down the spacebar while the engine crashes.
        // If we don't remove keyboard focus from this dialog, the keypress will
        // press the default button before the dialog actually appears, even if
        // the user has already released the key, which is bad.
        SetFocus(NULL);
    } break;
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_CONFIRM_SAVE:         // Fall through
        case IDB_DONT_SAVE:
        {
            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
        }
    } break;
    }

    return FALSE;
}

bool DebugCallStack::BackupCurrentLevel()
{
    CSystem* pSystem = static_cast<CSystem*>(m_pSystem);
    if (pSystem && pSystem->GetUserCallback())
    {
        return pSystem->GetUserCallback()->OnBackupDocument();
    }

    return false;
}

bool DebugCallStack::SaveCurrentLevel()
{
    CSystem* pSystem = static_cast<CSystem*>(m_pSystem);
    if (pSystem && pSystem->GetUserCallback())
    {
        return pSystem->GetUserCallback()->OnSaveDocument();
    }

    return false;
}

int DebugCallStack::SubmitBug(EXCEPTION_POINTERS* exception_pointer)
{
    int ret = IDB_EXIT;

    assert(!hwndException);

    // If in full screen minimize render window
    {
        ICVar* pFullscreen = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("r_Fullscreen") : 0;
        if (pFullscreen && pFullscreen->GetIVal() != 0 && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
        {
            ::ShowWindow((HWND)gEnv->pRenderer->GetHWND(), SW_MINIMIZE);
        }
    }

    //hwndException = CreateDialog( gDLLHandle,MAKEINTRESOURCE(IDD_EXCEPTION),NULL,NULL );

    RemoveOldFiles();

    if (initSymbols())
    {
        // Rise exception to call updateCallStack method.
        updateCallStack(exception_pointer);

        LogExceptionInfo(exception_pointer);

        if (IsFloatingPointException(exception_pointer))
        {
            //! Print exception dialog.
            ret = PrintException(exception_pointer);
        }

        doneSymbols();
        //exit(0);
    }

    return ret;
}

void DebugCallStack::ResetFPU(EXCEPTION_POINTERS* pex)
{
    if (IsFloatingPointException(pex))
    {
        // How to reset FPU: http://www.experts-exchange.com/Programming/System/Windows__Programming/Q_10310953.html
        _clearfp();
#ifndef WIN64
        pex->ContextRecord->FloatSave.ControlWord |= 0x2F;
        pex->ContextRecord->FloatSave.StatusWord &= ~0x8080;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
int __cdecl WalkStackFrames(CONTEXT& context, void** pCallstack, int maxStackEntries)
{
    int count;
    BOOL b_ret = TRUE; //Setup stack frame

    HANDLE hThread = GetCurrentThread();
    HANDLE hProcess = GetCurrentProcess();

    STACKFRAME64 stack_frame;

    memset(&stack_frame, 0, sizeof(stack_frame));
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;
    stack_frame.AddrReturn.Mode = AddrModeFlat;
    stack_frame.AddrBStore.Mode = AddrModeFlat;

    DWORD MachineType = IMAGE_FILE_MACHINE_I386;

#if defined(_M_IX86)
    MachineType                   = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrPC.Offset     = context.Eip;
    stack_frame.AddrStack.Offset  = context.Esp;
    stack_frame.AddrFrame.Offset  = context.Ebp;
#elif defined(_M_X64)
    MachineType                   = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset     = context.Rip;
    stack_frame.AddrStack.Offset  = context.Rsp;
    stack_frame.AddrFrame.Offset  = context.Rdi;
#endif

    //While there are still functions on the stack..
    for (count = 0; count < maxStackEntries && b_ret == TRUE; count++)
    {
        b_ret = StackWalk64(MachineType,   hProcess, hThread, &stack_frame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);
        pCallstack[count] = (void*)(stack_frame.AddrPC.Offset);
    }
    return count;
}

//////////////////////////////////////////////////////////////////////////
int DebugCallStack::CollectCallStackFrames(void** pCallstack, int maxStackEntries)
{
    if (!m_symbols)
    {
        if (!initSymbols())
        {
            return 0;
        }
    }

    CONTEXT context = CaptureCurrentContext();

    HANDLE hProcess = GetCurrentProcess();

    int count = WalkStackFrames(context, pCallstack, maxStackEntries);
    return count;
}

int DebugCallStack::CollectCallStack(HANDLE thread, void** pCallstack, int maxStackEntries)
{
    if (!m_symbols)
    {
        if (!initSymbols())
        {
            return 0;
        }
    }

    CONTEXT context;
    memset(&context, 0, sizeof(context));
#if defined(_M_IX86)
    context.ContextFlags = CONTEXT_i386 | CONTEXT_FULL;
#elif defined(_M_X64)
    context.ContextFlags = CONTEXT_AMD64 | CONTEXT_FULL;
#endif
    int prev_priority = GetThreadPriority(thread);
    SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
    BOOL result = GetThreadContext(thread, &context);
    ::SetThreadPriority(thread, prev_priority);
    return WalkStackFrames(context, pCallstack, maxStackEntries);
}

string DebugCallStack::GetModuleNameForAddr(void* addr)
{
    if (m_modules.empty())
    {
        return "[unknown]";
    }

    if (addr < m_modules.begin()->first)
    {
        return "[unknown]";
    }

    TModules::const_iterator it = m_modules.begin();
    TModules::const_iterator end = m_modules.end();
    for (; ++it != end; )
    {
        if (addr < it->first)
        {
            return (--it)->second;
        }
    }

    //if address is higher than the last module, we simply assume it is in the last module.
    return m_modules.rbegin()->second;
}

bool DebugCallStack::GetProcNameForAddr(void* addr, string& procName, void*& baseAddr, string& filename, int& line)
{
    return LookupFunctionName(addr, true, procName, filename, line, baseAddr);
}

string DebugCallStack::GetCurrentFilename()
{
    char fullpath[MAX_PATH_LENGTH + 1];
    GetModuleFileName(NULL, fullpath, MAX_PATH_LENGTH);
    return fullpath;
}

static bool IsFloatingPointException(EXCEPTION_POINTERS* pex)
{
    if (!pex)
    {
        return false;
    }

    DWORD exceptionCode = pex->ExceptionRecord->ExceptionCode;
    switch (exceptionCode)
    {
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case STATUS_FLOAT_MULTIPLE_FAULTS:
    case STATUS_FLOAT_MULTIPLE_TRAPS:
        return true;

    default:
        return false;
    }
}

int DebugCallStack::PrintException(EXCEPTION_POINTERS* exception_pointer)
{
    return DialogBoxParam(gDLLHandle, MAKEINTRESOURCE(IDD_CRITICAL_ERROR), NULL, DebugCallStack::ExceptionDialogProc, (LPARAM)exception_pointer);
}

#else
void MarkThisThreadForDebugging(const char*) {}
void UnmarkThisThreadFromDebugging() {}
void UpdateFPExceptionsMaskForThreads() {}
#endif //WIN32
