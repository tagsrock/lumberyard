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
#include "SystemInit.h"
#if defined(MAP_LOADING_SLICING)
#include "SystemScheduler.h"
#endif // defined(MAP_LOADING_SLICING)
#include "CryLibrary.h"
#include "CryPath.h"
#include <StringUtils.h>
#include "NullImplementation/NullInput.h"
#include "NullImplementation/NullResponseSystem.h"
#include "MemoryManager.h"
#include <IThreadManager.h>

#include <AzFramework/IO/LocalFileIO.h>
#include "RemoteFileIO.h"

#include <IEngineModule.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <AzCore/IO/SystemFile.h> // for AZ_MAX_PATH_LEN
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzFramework/Asset/AssetSystemBus.h>
#include "AZCoreLogSink.h"
#include <AzCore/Math/MathUtils.h>
#include "CryPakFileIO.h"

#include <LoadScreenBus.h>
#include <LyShine/Bus/UiSystemBus.h>

#if defined(APPLE) || defined(LINUX) && !defined(DEDICATED_SERVER)
#include <dlfcn.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <float.h>

// To enable profiling with vtune (https://software.intel.com/en-us/intel-vtune-amplifier-xe), make sure the line below is not commented out
//#define  PROFILE_WITH_VTUNE

#endif //WIN32

#include <INetwork.h>
#include <I3DEngine.h>
#include <IAISystem.h>
#include <IRenderer.h>
#include <ICryPak.h>
#include <AzCore/IO/FileIO.h>
#include <IMovieSystem.h>
#include <IEntitySystem.h>
#include <IInput.h>
#include <ILog.h>
#include <IAudioSystem.h>
#include <ICryAnimation.h>
#include <IScriptSystem.h>
#include <ICmdLine.h>
#include <IProcess.h>
#include <LyShine/ILyShine.h>
#include <HMDBus.h>
#include "HMDCVars.h"

#include "CryPak.h"
#include "XConsole.h"
#include "Telemetry/TelemetrySystem.h"
#include "Telemetry/TelemetryFileStream.h"
#include "Telemetry/TelemetryUDPStream.h"
#include "Log.h"
#include "XML/xml.h"
#include "StreamEngine/StreamEngine.h"
#include "BudgetingSystem.h"
#include "PhysRenderer.h"
#include "LocalizedStringManager.h"
#include "SystemEventDispatcher.h"
#include "Statistics.h"
#include "Statistics/LocalMemoryUsage.h"
#include "ThreadProfiler.h"
#include "ThreadConfigManager.h"
#include "HardwareMouse.h"
#include "Validator.h"
#include "ServerThrottle.h"
#include "SystemCFG.h"
#include "AutoDetectSpec.h"
#include "ResourceManager.h"
#include "LoadingProfiler.h"
#include "BootProfiler.h"
#include "DiskProfiler.h"
#include "Statoscope.h"
#include "TestSystemLegacy.h"
#include "VisRegTest.h"
#include "MTSafeAllocator.h"
#include "NotificationNetwork.h"
#include "ExtensionSystem/CryFactoryRegistryImpl.h"
#include "ExtensionSystem/TestCases/TestExtensions.h"
#include "ProfileLogSystem.h"
#include "CodeCoverage/CodeCheckpointMgr.h"
#include "SoftCode/SoftCodeMgr.h"
#include "ZLibCompressor.h"
#include "ZLibDecompressor.h"
#include "LZ4Decompressor.h"
#include "LevelHeap.h"
#include "OverloadSceneManager/OverloadSceneManager.h"
#include "ServiceNetwork.h"
#include "RemoteCommand.h"
#include "NullImplementation/NULLAudioSystems.h"
#include "ISimpleHttpServer.h"
#include "GemManager.h"
#include "GameFilePathManager.h"
#include <Cryptography/Crypto.h>
#include <CrySystemBus.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Jobs/JobManagerBus.h>

#include "IPlatformOS.h"
#include "PerfHUD.h"
#include "MiniGUI/MiniGUI.h"

#include <IGame.h>
#include <IGameFramework.h>

#include "Stroboscope/Stroboscope.h"

#if USE_STEAM
#include "Steamworks/public/steam/steam_api.h"
#include "Steamworks/public/steam/isteamremotestorage.h"
#endif

#if defined(IOS) || defined(APPLETV)
#include "IOSConsole.h"
#endif

#if defined(ANDROID)
    #include <AzCore/Android/Utils.h>
    #include <AzCore/Android/JNI/Object.h>

    #include "AndroidConsole.h"
#endif

#include "IDebugCallStack.h"

#include "WindowsConsole.h"

// select the asset processor based on cvars and defines.
// uncomment the following and edit the path where it is instantiated if you'd like to use the test file client
//#define USE_TEST_FILE_CLIENT

#if defined(REMOTE_ASSET_PROCESSOR)
// Over here, we'd put the header to the Remote Asset Processor interface (as opposed to the Local built in version  below)
#   include <AzFramework/Network/AssetProcessorConnection.h>
#endif

// if we enable the built-in local version instead of remote:
#if defined(CRY_ENABLE_RC_HELPER)
#include "ResourceCompilerHelper.h"
#endif

#ifdef WIN32
extern LONG WINAPI CryEngineExceptionFilterWER(struct _EXCEPTION_POINTERS* pExceptionPointers);
#endif

#if defined(USE_UNIXCONSOLE)
#if defined(LINUX) && !defined(ANDROID)
CUNIXConsole* pUnixConsole;
#endif
#endif // USE_UNIXCONSOLE

//////////////////////////////////////////////////////////////////////////
#define DEFAULT_LOG_FILENAME "@log@/Log.txt"

#define CRYENGINE_ENGINE_FOLDER "Engine"

//////////////////////////////////////////////////////////////////////////
#define CRYENGINE_DEFAULT_LOCALIZATION_LANG "english"

//////////////////////////////////////////////////////////////////////////
// Where possible, these are defaults used to initialize cvars
// System.cfg can then be used to override them
// This includes the Game DLL, although it is loaded elsewhere

#define DLL_SOUND           "CrySoundSystem"
#define DLL_NETWORK         "CryNetwork"
#define DLL_ONLINE          "CryOnline"
#define DLL_ENTITYSYSTEM    "CryEntitySystem"
#define DLL_SCRIPTSYSTEM    "CryScriptSystem"
#define DLL_INPUT                 "CryInput"
#define DLL_PHYSICS           "CryPhysics"
#define DLL_MOVIE                 "CryMovie"
#define DLL_AI                      "CryAISystem"
#define DLL_ANIMATION         "CryAnimation"
#define DLL_FONT                    "CryFont"
#define DLL_3DENGINE            "Cry3DEngine"
#define DLL_RENDERER_DX9  "CryRenderD3D9"
#define DLL_RENDERER_DX11 "CryRenderD3D11"
#define DLL_RENDERER_DX12 "CryRenderD3D12"
#define DLL_RENDERER_METAL  "CryRenderMetal"
#define DLL_RENDERER_GL   "CryRenderGL"
#define DLL_RENDERER_NULL "CryRenderNULL"
#define DLL_GAME                    "GameDLL"
#define DLL_UNITTESTS       "CryUnitTests"
#define DLL_SHINE           "LyShine"
#define DLL_LMBRAWS         "LmbrAWS"

//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(LINUX) ||  defined(DURANGO) || defined(APPLE)
#   define DLL_MODULE_INIT_ISYSTEM "ModuleInitISystem"
#   define DLL_MODULE_SHUTDOWN_ISYSTEM "ModuleShutdownISystem"
#   define DLL_INITFUNC_RENDERER "PackageRenderConstructor"
#   define DLL_INITFUNC_ENTITY "CreateEntitySystem"
#   define DLL_INITFUNC_INPUT "CreateInput"
#   define DLL_INITFUNC_SOUND "CreateSoundSystem"
#   define DLL_INITFUNC_PHYSIC "CreatePhysicalWorld"
#   define DLL_INITFUNC_MOVIE "CreateMovieSystem"
#   define DLL_INITFUNC_AI "CreateAISystem"
#   define DLL_INITFUNC_SCRIPT "CreateScriptSystem"
#   define DLL_INITFUNC_FONT "CreateCryFontInterface"
#   define DLL_INITFUNC_3DENGINE "CreateCry3DEngine"
#   define DLL_INITFUNC_ANIMATION "CreateCharManager"
#   define DLL_INITFUNC_UI "CreateLyShineInterface"
#else
#   define DLL_MODULE_INIT_ISYSTEM (LPCSTR)2
#   define DLL_MODULE_SHUTDOWN_ISYSTEM (LPCSTR)3
#   define DLL_INITFUNC_RENDERER  (LPCSTR)1
#   define DLL_INITFUNC_RENDERER  (LPCSTR)1
#   define DLL_INITFUNC_ENTITY    (LPCSTR)1
#   define DLL_INITFUNC_INPUT     (LPCSTR)1
#   define DLL_INITFUNC_SOUND     (LPCSTR)1
#   define DLL_INITFUNC_PHYSIC    (LPCSTR)1
#   define DLL_INITFUNC_MOVIE     (LPCSTR)1
#   define DLL_INITFUNC_AI        (LPCSTR)1
#   define DLL_INITFUNC_SCRIPT    (LPCSTR)1
#   define DLL_INITFUNC_FONT      (LPCSTR)1
#   define DLL_INITFUNC_3DENGINE  (LPCSTR)1
#   define DLL_INITFUNC_ANIMATION (LPCSTR)1
#   define DLL_INITFUNC_UI        (LPCSTR)1
#endif

//////////////////////////////////////////////////////////////////////////
// Extern declarations for static libraries.
//////////////////////////////////////////////////////////////////////////
#if defined(AZ_MONOLITHIC_BUILD)
extern "C"
{
IAISystem* CreateAISystem(ISystem* pSystem);
IMovieSystem* CreateMovieSystem(ISystem* pSystem);
}
#endif //AZ_MONOLITHIC_BUILD
//////////////////////////////////////////////////////////////////////////

extern CMTSafeHeap* g_pPakHeap;

#ifdef WIN32
extern HMODULE gDLLHandle;
#endif

namespace
{
#if defined(AZ_PLATFORM_WINDOWS)
    // on windows, we lock our cache using a lockfile.  On other platforms this is not necessary since devices like ios, android, PS4, and XBONE cannot
    // run more than one game process that uses the same folder anyway.
    HANDLE g_cacheLock = INVALID_HANDLE_VALUE;
#endif
}

#if defined(AZ_PLATFORM_ANDROID)
// let the java code know the native renderer is taking over now
extern "C" void OnEngineRendererTakeover(bool engineSplashActive)
{
    bool engineSplashEnabled = (g_cvars.sys_rendersplashscreen != 0);
    if (engineSplashActive != engineSplashEnabled)
    {
        return;
    }

    AZ::Android::JNI::Object activity(AZ::Android::Utils::GetActivityClassRef(), AZ::Android::Utils::GetActivityRef());

    activity.RegisterMethod("OnEngineRendererTakeover", "()V");
    activity.InvokeVoidMethod("OnEngineRendererTakeover");
}
#endif

//static int g_sysSpecChanged = false;

const char* g_szLvlResExt = "_LvlRes.txt";

#if defined(DEDICATED_SERVER)
struct SCVarsClientConfigSink
    : public ILoadConfigurationEntrySink
{
    virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
    {
        gEnv->pConsole->SetClientDataProbeString(szKey, szValue);
    }
};
#endif

//////////////////////////////////////////////////////////////////////////
static inline void InlineInitializationProcessing(const char* sDescription)
{
    assert(CryMemory::IsHeapValid());
    if (gEnv->pLog)
    {
        gEnv->pLog->UpdateLoadingScreen(0);
    }
}

//////////////////////////////////////////////////////////////////////////
static void CmdCrashTest(IConsoleCmdArgs* pArgs)
{
    assert(pArgs);

    if (pArgs->GetArgCount() == 2)
    {
        //This method intentionally crashes, a lot.

        int crashType = atoi(pArgs->GetArg(1));
        switch (crashType)
        {
        case 1:
        {
            int* p = 0;
            PREFAST_SUPPRESS_WARNING(6011) * p = 0xABCD;
        }
        break;
        case 2:
        {
            float a = 1.0f;
            memset(&a, 0, sizeof(a));
            float* b = &a;
            float c = 3;
            CryLog("%f", (c / *b));
        }
        break;
        case 3:
            while (true)
            {
                char* element = new char[10240];
            }
            break;
        case 4:
            CryFatalError("sys_crashtest 4");
            break;
        case 5:
            while (true)
            {
                char* element = new char[128];     //testing the crash handler an exception in the cry memory allocation occurred
            }
        case 6:
        {
            CRY_ASSERT_MESSAGE(false, "Testing assert for testing crashes");
        }
        break;
        case 7:
            __debugbreak();
            break;
        case 8:
            CrySleep(1000 * 60 * 10);
            break;
        }
    }
}

#if USE_STEAM
//////////////////////////////////////////////////////////////////////////
static void CmdWipeSteamCloud(IConsoleCmdArgs* pArgs)
{
    if (!gEnv->pSystem->SteamInit())
    {
        return;
    }

    int32 fileCount = SteamRemoteStorage()->GetFileCount();
    for (int i = 0; i < fileCount; i++)
    {
        int32 size = 0;
        const char* name = SteamRemoteStorage()->GetFileNameAndSize(i, &size);
        bool success = SteamRemoteStorage()->FileDelete(name);
        CryLog("Deleting file: %s - success: %d", name, success);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
static void CmdDumpJobManagerJobList(IConsoleCmdArgs* pArgs)
{
    if (gEnv->pJobManager)
    {
        gEnv->pJobManager->DumpJobList();
    }
}

//////////////////////////////////////////////////////////////////////////
struct SysSpecOverrideSink
    : public ILoadConfigurationEntrySink
{
    virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
    {
        ICVar* pCvar = gEnv->pConsole->GetCVar(szKey);

        if (pCvar)
        {
            const bool wasNotInConfig = ((pCvar->GetFlags() & VF_WASINCONFIG) == 0);
            bool applyCvar = wasNotInConfig;
            if (applyCvar == false)
            {
                // Special handling for sys_spec_full
                if (_stricmp(szKey, "sys_spec_full") == 0)
                {
                    // If it is set to 0 then ignore this request to set to something else
                    // If it is set to 0 then the user wants to changes system spec settings in system.cfg
                    if (pCvar->GetIVal() != 0)
                    {
                        applyCvar = true;
                    }
                }
            }

            if (applyCvar)
            {
                pCvar->Set(szValue);
            }
            else
            {
                CryLogAlways("NOT VF_WASINCONFIG Ignoring cvar '%s' new value '%s' old value '%s' group '%s'", szKey, szValue, pCvar->GetString(), szGroup);
            }
        }
        else
        {
            CryLogAlways("Can't find cvar '%s' value '%s' group '%s'", szKey, szValue, szGroup);
        }
    }
};

#if !defined(CONSOLE)
struct SysSpecOverrideSinkConsole
    : public ILoadConfigurationEntrySink
{
    virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
    {
        // Ignore platform-specific cvars that should just be executed on the console
        if (stricmp(szGroup, "Platform") == 0)
        {
            return;
        }

        ICVar* pCvar = gEnv->pConsole->GetCVar(szKey);
        if (pCvar)
        {
            pCvar->Set(szValue);
        }
    }
};
#endif

static void OnSysSpecChange(ICVar* pVar)
{
    CDebugAllowFileAccess ignoreInvalidFileAccess;
    SysSpecOverrideSink sysSpecOverrideSink;
    ILoadConfigurationEntrySink* pSysSpecOverrideSinkConsole = NULL;

#if !defined(CONSOLE)
    SysSpecOverrideSinkConsole sysSpecOverrideSinkConsole;
    pSysSpecOverrideSinkConsole = &sysSpecOverrideSinkConsole;
#endif

    //  g_sysSpecChanged = true;
    static int no_recursive = false;
    if (no_recursive)
    {
        return;
    }
    no_recursive = true;
    // Called when sys_spec (client config spec) variable changes.
    int spec = pVar->GetIVal();

    if (spec > ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec())
    {
        spec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();
        pVar->Set(spec);
    }

    CryLog("OnSysSpecChange(%d)", spec);
    const char* driverName = GetISystem()->GetRenderingDriverName();
    switch (spec)
    {
    case CONFIG_LOW_SPEC:
        GetISystem()->LoadConfiguration("LowSpec.cfg", &sysSpecOverrideSink);
        break;
    case CONFIG_MEDIUM_SPEC:
        GetISystem()->LoadConfiguration("MedSpec.cfg", &sysSpecOverrideSink);
        break;
    case CONFIG_HIGH_SPEC:
        GetISystem()->LoadConfiguration("HighSpec.cfg", &sysSpecOverrideSink);
        break;
    case CONFIG_VERYHIGH_SPEC:
        GetISystem()->LoadConfiguration("VeryHighSpec.cfg", &sysSpecOverrideSink);
        break;
    case CONFIG_DURANGO:
        GetISystem()->LoadConfiguration("durango.cfg", pSysSpecOverrideSinkConsole);
        break;
    case CONFIG_ORBIS:
        GetISystem()->LoadConfiguration("orbis.cfg", pSysSpecOverrideSinkConsole);
        break;
    case CONFIG_ANDROID:
        GetISystem()->LoadConfiguration("android.cfg", pSysSpecOverrideSinkConsole);
        break;
    case CONFIG_IOS:
        GetISystem()->LoadConfiguration("ios.cfg", pSysSpecOverrideSinkConsole);
        break;
    case CONFIG_APPLETV:
        GetISystem()->LoadConfiguration("appletv.cfg", pSysSpecOverrideSinkConsole);
        break;
    case CONFIG_MACOS_GL:
        AZ_Assert(driverName == nullptr || (strcmp(driverName, "GL") == 0), "Rendering driver and sys_spec dont match. Please edit the r_driver and sys_spec in system_osx_osx.cfg to fix this issue.");
        GetISystem()->LoadConfiguration("osx_gl.cfg", pSysSpecOverrideSinkConsole);
        break;
    case CONFIG_MACOS_METAL:
        AZ_Assert(driverName == nullptr || (strcmp(driverName, "METAL") == 0), "Rendering driver and sys_spec dont match. Please edit the r_driver and sys_spec in system_osx_osx.cfg to fix this issue.");
        GetISystem()->LoadConfiguration("osx_metal.cfg", pSysSpecOverrideSinkConsole);
        break;
    default:
        // Do nothing.
        break;
    }

    // make sure editor specific settings are not changed
    if (gEnv->IsEditor())
    {
        GetISystem()->LoadConfiguration("editor.cfg");
    }

    bool bMultiGPUEnabled = false;
    if (gEnv->pRenderer)
    {
        gEnv->pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPUEnabled);
    }
    if (bMultiGPUEnabled)
    {
        GetISystem()->LoadConfiguration("mgpu.cfg");
    }

    bool bChangeServerSpec = true;
    if (gEnv->pGame && gEnv->bMultiplayer)
    {
        bChangeServerSpec = false;
    }
    if (bChangeServerSpec)
    {
        GetISystem()->SetConfigSpec((ESystemConfigSpec)spec, false);
    }

    if (gEnv->p3DEngine)
    {
        gEnv->p3DEngine->GetMaterialManager()->RefreshMaterialRuntime();
    }

    no_recursive = false;
}

//////////////////////////////////////////////////////////////////////////
struct SCryEngineLanguageConfigLoader
    : public ILoadConfigurationEntrySink
{
    CSystem* m_pSystem;
    string m_language;
    string m_pakFile;

    SCryEngineLanguageConfigLoader(CSystem* pSystem) { m_pSystem = pSystem; }
    void Load(const char* sCfgFilename)
    {
        CSystemConfiguration cfg(sCfgFilename, m_pSystem, this); // Parse folders config file.
    }
    virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup)
    {
        if (_stricmp(szKey, "Language") == 0)
        {
            m_language = szValue;
        }
        else if (_stricmp(szKey, "PAK") == 0)
        {
            m_pakFile = szValue;
        }
    }
    virtual void OnLoadConfigurationEntry_End() {}
};

//////////////////////////////////////////////////////////////////////////
#if !defined(AZ_MONOLITHIC_BUILD) && !defined(AZ_PLATFORM_PS4)
WIN_HMODULE CSystem::LoadDynamiclibrary(const char* dllName) const
{
    WIN_HMODULE handle = NULL;
#ifdef WIN32
    if (m_binariesDir.empty())
    {
        handle = CryLoadLibrary(dllName);
        if (!handle)
        {
            DWORD dwErrorCode = GetLastError();
            CryLogAlways("DLL Failed to load with error code 0x%X: %s", dwErrorCode, dllName);
        }
    }
    else
    {
        char currentDirectory[1024];
        GetCurrentDirectory(sizeof(currentDirectory), currentDirectory);
        SetCurrentDirectory(m_binariesDir.c_str());
        handle = CryLoadLibrary(dllName);
        SetCurrentDirectory(currentDirectory);
    }
#else
    handle = CryLoadLibrary(dllName);
#endif
    return handle;
}

//////////////////////////////////////////////////////////////////////////
WIN_HMODULE CSystem::LoadDLL(const char* dllName, bool bQuitIfNotFound)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    CryComment("Loading DLL: %s", dllName);

    WIN_HMODULE handle = LoadDynamiclibrary(dllName);

    if (!handle)
    {
        if (bQuitIfNotFound)
        {
#if defined(LINUX) || defined(APPLE)
            CryFatalError("Error loading DLL: %s, error :  %s\n", dllName, dlerror());
#else
            CryFatalError("Error loading DLL: %s, error code %d", dllName, GetLastError());
#endif
            Quit();
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////////
    // After loading DLL initialize it by calling ModuleInitISystem
    //////////////////////////////////////////////////////////////////////////
    string moduleName = PathUtil::GetFileName(dllName);

    typedef void*(* PtrFunc_ModuleInitISystem)(ISystem* pSystem, const char* moduleName);
    PtrFunc_ModuleInitISystem pfnModuleInitISystem = (PtrFunc_ModuleInitISystem) CryGetProcAddress(handle, DLL_MODULE_INIT_ISYSTEM);
    if (pfnModuleInitISystem)
    {
        pfnModuleInitISystem(this, moduleName.c_str());
    }

    return handle;
}
#endif //#if !defined(AZ_MONOLITHIC_BUILD) && !defined(AZ_PLATFORM_PS4)

//////////////////////////////////////////////////////////////////////////
bool CSystem::LoadEngineDLLs()
{
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::UnloadDLL(const char* dllName)
{
    bool isSuccess = false;

    WIN_HMODULE const hModule = stl::find_in_map(m_moduleDLLHandles, dllName, NULL);

    if (hModule != NULL)
    {
        CryComment("Unloading DLL: %s", dllName);
        CryFreeLibrary(hModule);
        m_moduleDLLHandles.erase(dllName);
        isSuccess = true;
    }

    return isSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitializeEngineModule(const char* dllName, const char* moduleClassName, const SSystemInitParams& initParams, bool bQuitIfNotFound)
{
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "InitializeEngineModule");
    MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "%s", moduleClassName);

    bool bResult = false;

    stack_string msg;
    msg = "Initializing ";
    msg += dllName;
    msg += "...";

    if (m_pUserCallback)
    {
        m_pUserCallback->OnInitProgress(msg.c_str());
    }
    CryLog("%s", msg.c_str());

    IMemoryManager::SProcessMemInfo memStart, memEnd;
    if (GetIMemoryManager())
    {
        GetIMemoryManager()->GetProcessMemInfo(memStart);
    }
    else
    {
        ZeroStruct(memStart);
    }

    stack_string dllfile = dllName;

#if defined(LINUX)
    dllfile = "lib" + PathUtil::ReplaceExtension(dllfile, "so");
#ifndef LINUX
    dllfile.MakeLower();
#endif
#elif defined(APPLE)
    dllfile = "lib" + PathUtil::ReplaceExtension(dllfile, "dylib");
#else
    dllfile = PathUtil::ReplaceExtension(dllfile, "dll");
#endif

#if !defined(AZ_MONOLITHIC_BUILD)
    WIN_HMODULE hModule = LoadDLL(dllfile.c_str(), bQuitIfNotFound);
    if (!hModule)
    {
        return bResult;
    }
    m_moduleDLLHandles.insert(std::make_pair(dllfile.c_str(), hModule));
#endif // #if !defined(AZ_MONOLITHIC_BUILD)


    boost::shared_ptr<IEngineModule> pModule;
    if (CryCreateClassInstance(moduleClassName, pModule))
    {
        MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Initialize module: %s", moduleClassName);
        bResult = pModule->Initialize(m_env, initParams);
    }

    if (GetIMemoryManager())
    {
        GetIMemoryManager()->GetProcessMemInfo(memEnd);

        uint64 memUsed = memEnd.WorkingSetSize - memStart.WorkingSetSize;
        CryLog("Initializing %s done, MemUsage=%dKb", dllName, uint32(memUsed / 1024));
    }

    return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::UnloadEngineModule(const char* dllName, const char* moduleClassName)
{
    bool isSuccess = false;

    // Remove the factory.
    ICryFactoryRegistryImpl* const pReg = static_cast<ICryFactoryRegistryImpl*>(GetCryFactoryRegistry());

    if (pReg != NULL)
    {
        ICryFactory* pICryFactory = pReg->GetFactory(moduleClassName);

        if (pICryFactory != NULL)
        {
            pReg->UnregisterFactory(pICryFactory);
        }
    }

    stack_string msg;
    msg = "Unloading ";
    msg += dllName;
    msg += "...";

    CryLog("%s", msg.c_str());

    stack_string dllfile = dllName;

#if defined(LINUX)
    dllfile = "lib" + PathUtil::ReplaceExtension(dllfile, "so");
#ifndef LINUX
    dllfile.MakeLower();
#endif
#elif defined(APPLE)
    dllfile = "lib" + PathUtil::ReplaceExtension(dllfile, "dylib");
#else
    dllfile = PathUtil::ReplaceExtension(dllfile, "dll");
#endif

#if !defined(AZ_MONOLITHIC_BUILD)
    isSuccess = UnloadDLL(dllfile.c_str());
#endif // #if !defined(AZ_MONOLITHIC_BUILD)

    return isSuccess;
}


//////////////////////////////////////////////////////////////////////////
void CSystem::ShutdownModuleLibraries()
{
#if !defined(AZ_MONOLITHIC_BUILD)
    for (auto iterator = m_moduleDLLHandles.begin(); iterator != m_moduleDLLHandles.end(); ++iterator)
    {
        typedef void*( * PtrFunc_ModuleShutdownISystem )(ISystem* pSystem);

        PtrFunc_ModuleShutdownISystem pfnModuleShutdownISystem =
            reinterpret_cast<PtrFunc_ModuleShutdownISystem>(CryGetProcAddress(iterator->second, DLL_MODULE_SHUTDOWN_ISYSTEM));

        if (pfnModuleShutdownISystem)
        {
            pfnModuleShutdownISystem(this);
        }

        FreeLib(iterator->second);
    }

    m_moduleDLLHandles.clear();

#endif // !defined(AZ_MONOLITHIC_BUILD)
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::OpenRenderLibrary(const char* t_rend, const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());


    if (gEnv->IsDedicated())
    {
        return OpenRenderLibrary(R_NULL_RENDERER, initParams);
    }

    if (_stricmp(t_rend, "DX9") == 0)
    {
        return OpenRenderLibrary(R_DX9_RENDERER, initParams);
    }
    else if (_stricmp(t_rend, "DX11") == 0)
    {
        return OpenRenderLibrary(R_DX11_RENDERER, initParams);
    }
    else if (stricmp(t_rend, "DX12") == 0)
    {
        return OpenRenderLibrary(R_DX12_RENDERER, initParams);
    }
    else if (_stricmp(t_rend, "GL") == 0)
    {
        return OpenRenderLibrary(R_GL_RENDERER, initParams);
    }
    else if (_stricmp(t_rend, "METAL") == 0)
    {
        return OpenRenderLibrary(R_METAL_RENDERER, initParams);
    }
    else if (_stricmp(t_rend, "NULL") == 0)
    {
        return OpenRenderLibrary(R_NULL_RENDERER, initParams);
    }


    CryFatalError ("Unknown renderer type: %s", t_rend);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

#if defined(WIN32) || defined(WIN64)
wstring GetErrorStringUnsupportedGPU(const char* gpuName, unsigned int gpuVendorId, unsigned int gpuDeviceId)
{
    const size_t fullLangID = (size_t) GetKeyboardLayout(0);
    const size_t primLangID = fullLangID & 0x3FF;

    const wchar_t* pFmt = L"Unsupported video card detected! Continuing to run might lead to unexpected results or crashes. "
        L"Please check the manual for further information on hardware requirements.\n\n\"%S\" [vendor id = 0x%.4x, device id = 0x%.4x]";

    switch (primLangID)
    {
    case 0x04: // Chinese
    {
        static const wchar_t fmt[] = {0x5075, 0x6E2C, 0x5230, 0x4E0D, 0x652F, 0x63F4, 0x7684, 0x986F, 0x793A, 0x5361, 0xFF01, 0x7E7C, 0x7E8C, 0x57F7, 0x884C, 0x53EF, 0x80FD, 0x5C0E, 0x81F4, 0x7121, 0x6CD5, 0x9810, 0x671F, 0x7684, 0x7D50, 0x679C, 0x6216, 0x7576, 0x6A5F, 0x3002, 0x8ACB, 0x6AA2, 0x67E5, 0x8AAA, 0x660E, 0x66F8, 0x4E0A, 0x7684, 0x786C, 0x9AD4, 0x9700, 0x6C42, 0x4EE5, 0x53D6, 0x5F97, 0x66F4, 0x591A, 0x76F8, 0x95DC, 0x8CC7, 0x8A0A, 0x3002, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x5EE0, 0x5546, 0x7DE8, 0x865F, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x88DD, 0x7F6E, 0x7DE8, 0x865F, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x05: // Czech
    {
        static const wchar_t fmt[] = {0x0042, 0x0079, 0x006C, 0x0061, 0x0020, 0x0064, 0x0065, 0x0074, 0x0065, 0x006B, 0x006F, 0x0076, 0x00E1, 0x006E, 0x0061, 0x0020, 0x0067, 0x0072, 0x0061, 0x0066, 0x0069, 0x0063, 0x006B, 0x00E1, 0x0020, 0x006B, 0x0061, 0x0072, 0x0074, 0x0061, 0x002C, 0x0020, 0x006B, 0x0074, 0x0065, 0x0072, 0x00E1, 0x0020, 0x006E, 0x0065, 0x006E, 0x00ED, 0x0020, 0x0070, 0x006F, 0x0064, 0x0070, 0x006F, 0x0072, 0x006F, 0x0076, 0x00E1, 0x006E, 0x0061, 0x002E, 0x0020, 0x0050, 0x006F, 0x006B, 0x0072, 0x0061, 0x010D, 0x006F, 0x0076, 0x00E1, 0x006E, 0x00ED, 0x0020, 0x006D, 0x016F, 0x017E, 0x0065, 0x0020, 0x0076, 0x00E9, 0x0073, 0x0074, 0x0020, 0x006B, 0x0065, 0x0020, 0x006B, 0x0072, 0x0069, 0x0074, 0x0069, 0x0063, 0x006B, 0x00FD, 0x006D, 0x0020, 0x0063, 0x0068, 0x0079, 0x0062, 0x00E1, 0x006D, 0x0020, 0x006E, 0x0065, 0x0062, 0x006F, 0x0020, 0x006E, 0x0065, 0x0073, 0x0074, 0x0061, 0x0062, 0x0069, 0x006C, 0x0069, 0x0074, 0x011B, 0x0020, 0x0073, 0x0079, 0x0073, 0x0074, 0x00E9, 0x006D, 0x0075, 0x002E, 0x0020, 0x0050, 0x0159, 0x0065, 0x010D, 0x0074, 0x011B, 0x0074, 0x0065, 0x0020, 0x0073, 0x0069, 0x0020, 0x0070, 0x0072, 0x006F, 0x0073, 0x00ED, 0x006D, 0x0020, 0x0075, 0x017E, 0x0069, 0x0076, 0x0061, 0x0074, 0x0065, 0x006C, 0x0073, 0x006B, 0x006F, 0x0075, 0x0020, 0x0070, 0x0159, 0x00ED, 0x0072, 0x0075, 0x010D, 0x006B, 0x0075, 0x0020, 0x0070, 0x0072, 0x006F, 0x0020, 0x0070, 0x006F, 0x0064, 0x0072, 0x006F, 0x0062, 0x006E, 0x00E9, 0x0020, 0x0069, 0x006E, 0x0066, 0x006F, 0x0072, 0x006D, 0x0061, 0x0063, 0x0065, 0x0020, 0x006F, 0x0020, 0x0073, 0x0079, 0x0073, 0x0074, 0x00E9, 0x006D, 0x006F, 0x0076, 0x00FD, 0x0063, 0x0068, 0x0020, 0x0070, 0x006F, 0x017E, 0x0061, 0x0064, 0x0061, 0x0076, 0x0063, 0x00ED, 0x0063, 0x0068, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x07: // German
    {
        static const wchar_t fmt[] = {0x004E, 0x0069, 0x0063, 0x0068, 0x0074, 0x002D, 0x0075, 0x006E, 0x0074, 0x0065, 0x0072, 0x0073, 0x0074, 0x00FC, 0x0074, 0x007A, 0x0074, 0x0065, 0x0020, 0x0056, 0x0069, 0x0064, 0x0065, 0x006F, 0x006B, 0x0061, 0x0072, 0x0074, 0x0065, 0x0020, 0x0067, 0x0065, 0x0066, 0x0075, 0x006E, 0x0064, 0x0065, 0x006E, 0x0021, 0x0020, 0x0046, 0x006F, 0x0072, 0x0074, 0x0066, 0x0061, 0x0068, 0x0072, 0x0065, 0x006E, 0x0020, 0x006B, 0x0061, 0x006E, 0x006E, 0x0020, 0x007A, 0x0075, 0x0020, 0x0075, 0x006E, 0x0065, 0x0072, 0x0077, 0x0061, 0x0072, 0x0074, 0x0065, 0x0074, 0x0065, 0x006E, 0x0020, 0x0045, 0x0072, 0x0067, 0x0065, 0x0062, 0x006E, 0x0069, 0x0073, 0x0073, 0x0065, 0x006E, 0x0020, 0x006F, 0x0064, 0x0065, 0x0072, 0x0020, 0x0041, 0x0062, 0x0073, 0x0074, 0x00FC, 0x0072, 0x007A, 0x0065, 0x006E, 0x0020, 0x0066, 0x00FC, 0x0068, 0x0072, 0x0065, 0x006E, 0x002E, 0x0020, 0x0042, 0x0069, 0x0074, 0x0074, 0x0065, 0x0020, 0x006C, 0x0069, 0x0065, 0x0073, 0x0020, 0x0064, 0x0061, 0x0073, 0x0020, 0x004D, 0x0061, 0x006E, 0x0075, 0x0061, 0x006C, 0x0020, 0x0066, 0x00FC, 0x0072, 0x0020, 0x0077, 0x0065, 0x0069, 0x0074, 0x0065, 0x0072, 0x0065, 0x0020, 0x0049, 0x006E, 0x0066, 0x006F, 0x0072, 0x006D, 0x0061, 0x0074, 0x0069, 0x006F, 0x006E, 0x0065, 0x006E, 0x0020, 0x007A, 0x0075, 0x0020, 0x0048, 0x0061, 0x0072, 0x0064, 0x0077, 0x0061, 0x0072, 0x0065, 0x002D, 0x0041, 0x006E, 0x0066, 0x006F, 0x0072, 0x0064, 0x0065, 0x0072, 0x0075, 0x006E, 0x0067, 0x0065, 0x006E, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x0a: // Spanish
    {
        static const wchar_t fmt[] = {0x0053, 0x0065, 0x0020, 0x0068, 0x0061, 0x0020, 0x0064, 0x0065, 0x0074, 0x0065, 0x0063, 0x0074, 0x0061, 0x0064, 0x006F, 0x0020, 0x0075, 0x006E, 0x0061, 0x0020, 0x0074, 0x0061, 0x0072, 0x006A, 0x0065, 0x0074, 0x0061, 0x0020, 0x0067, 0x0072, 0x00E1, 0x0066, 0x0069, 0x0063, 0x0061, 0x0020, 0x006E, 0x006F, 0x0020, 0x0063, 0x006F, 0x006D, 0x0070, 0x0061, 0x0074, 0x0069, 0x0062, 0x006C, 0x0065, 0x002E, 0x0020, 0x0053, 0x0069, 0x0020, 0x0073, 0x0069, 0x0067, 0x0075, 0x0065, 0x0073, 0x0020, 0x0065, 0x006A, 0x0065, 0x0063, 0x0075, 0x0074, 0x0061, 0x006E, 0x0064, 0x006F, 0x0020, 0x0065, 0x006C, 0x0020, 0x006A, 0x0075, 0x0065, 0x0067, 0x006F, 0x002C, 0x0020, 0x0065, 0x0073, 0x0020, 0x0070, 0x006F, 0x0073, 0x0069, 0x0062, 0x006C, 0x0065, 0x0020, 0x0071, 0x0075, 0x0065, 0x0020, 0x0073, 0x0065, 0x0020, 0x0070, 0x0072, 0x006F, 0x0064, 0x0075, 0x007A, 0x0063, 0x0061, 0x006E, 0x0020, 0x0065, 0x0066, 0x0065, 0x0063, 0x0074, 0x006F, 0x0073, 0x0020, 0x0069, 0x006E, 0x0065, 0x0073, 0x0070, 0x0065, 0x0072, 0x0061, 0x0064, 0x006F, 0x0073, 0x0020, 0x006F, 0x0020, 0x0071, 0x0075, 0x0065, 0x0020, 0x0065, 0x006C, 0x0020, 0x0070, 0x0072, 0x006F, 0x0067, 0x0072, 0x0061, 0x006D, 0x0061, 0x0020, 0x0064, 0x0065, 0x006A, 0x0065, 0x0020, 0x0064, 0x0065, 0x0020, 0x0066, 0x0075, 0x006E, 0x0063, 0x0069, 0x006F, 0x006E, 0x0061, 0x0072, 0x002E, 0x0020, 0x0050, 0x006F, 0x0072, 0x0020, 0x0066, 0x0061, 0x0076, 0x006F, 0x0072, 0x002C, 0x0020, 0x0063, 0x006F, 0x006D, 0x0070, 0x0072, 0x0075, 0x0065, 0x0062, 0x0061, 0x0020, 0x0065, 0x006C, 0x0020, 0x006D, 0x0061, 0x006E, 0x0075, 0x0061, 0x006C, 0x0020, 0x0070, 0x0061, 0x0072, 0x0061, 0x0020, 0x006F, 0x0062, 0x0074, 0x0065, 0x006E, 0x0065, 0x0072, 0x0020, 0x006D, 0x00E1, 0x0073, 0x0020, 0x0069, 0x006E, 0x0066, 0x006F, 0x0072, 0x006D, 0x0061, 0x0063, 0x0069, 0x00F3, 0x006E, 0x0020, 0x0061, 0x0063, 0x0065, 0x0072, 0x0063, 0x0061, 0x0020, 0x0064, 0x0065, 0x0020, 0x006C, 0x006F, 0x0073, 0x0020, 0x0072, 0x0065, 0x0071, 0x0075, 0x0069, 0x0073, 0x0069, 0x0074, 0x006F, 0x0073, 0x0020, 0x0064, 0x0065, 0x006C, 0x0020, 0x0073, 0x0069, 0x0073, 0x0074, 0x0065, 0x006D, 0x0061, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x0c: // French
    {
        static const wchar_t fmt[] = {0x0041, 0x0074, 0x0074, 0x0065, 0x006E, 0x0074, 0x0069, 0x006F, 0x006E, 0x002C, 0x0020, 0x006C, 0x0061, 0x0020, 0x0063, 0x0061, 0x0072, 0x0074, 0x0065, 0x0020, 0x0076, 0x0069, 0x0064, 0x00E9, 0x006F, 0x0020, 0x0064, 0x00E9, 0x0074, 0x0065, 0x0063, 0x0074, 0x00E9, 0x0065, 0x0020, 0x006E, 0x2019, 0x0065, 0x0073, 0x0074, 0x0020, 0x0070, 0x0061, 0x0073, 0x0020, 0x0073, 0x0075, 0x0070, 0x0070, 0x006F, 0x0072, 0x0074, 0x00E9, 0x0065, 0x0020, 0x0021, 0x0020, 0x0050, 0x006F, 0x0075, 0x0072, 0x0073, 0x0075, 0x0069, 0x0076, 0x0072, 0x0065, 0x0020, 0x006C, 0x2019, 0x0061, 0x0070, 0x0070, 0x006C, 0x0069, 0x0063, 0x0061, 0x0074, 0x0069, 0x006F, 0x006E, 0x0020, 0x0070, 0x006F, 0x0075, 0x0072, 0x0072, 0x0061, 0x0069, 0x0074, 0x0020, 0x0065, 0x006E, 0x0067, 0x0065, 0x006E, 0x0064, 0x0072, 0x0065, 0x0072, 0x0020, 0x0064, 0x0065, 0x0073, 0x0020, 0x0069, 0x006E, 0x0073, 0x0074, 0x0061, 0x0062, 0x0069, 0x006C, 0x0069, 0x0074, 0x00E9, 0x0073, 0x0020, 0x006F, 0x0075, 0x0020, 0x0064, 0x0065, 0x0073, 0x0020, 0x0063, 0x0072, 0x0061, 0x0073, 0x0068, 0x0073, 0x002E, 0x0020, 0x0056, 0x0065, 0x0075, 0x0069, 0x006C, 0x006C, 0x0065, 0x007A, 0x0020, 0x0076, 0x006F, 0x0075, 0x0073, 0x0020, 0x0072, 0x0065, 0x0070, 0x006F, 0x0072, 0x0074, 0x0065, 0x0072, 0x0020, 0x0061, 0x0075, 0x0020, 0x006D, 0x0061, 0x006E, 0x0075, 0x0065, 0x006C, 0x0020, 0x0070, 0x006F, 0x0075, 0x0072, 0x0020, 0x0070, 0x006C, 0x0075, 0x0073, 0x0020, 0x0064, 0x2019, 0x0069, 0x006E, 0x0066, 0x006F, 0x0072, 0x006D, 0x0061, 0x0074, 0x0069, 0x006F, 0x006E, 0x0073, 0x0020, 0x0073, 0x0075, 0x0072, 0x0020, 0x006C, 0x0065, 0x0073, 0x0020, 0x0070, 0x0072, 0x00E9, 0x002D, 0x0072, 0x0065, 0x0071, 0x0075, 0x0069, 0x0073, 0x0020, 0x006D, 0x0061, 0x0074, 0x00E9, 0x0072, 0x0069, 0x0065, 0x006C, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x10: // Italian
    {
        static const wchar_t fmt[] = {0x00C8, 0x0020, 0x0073, 0x0074, 0x0061, 0x0074, 0x0061, 0x0020, 0x0072, 0x0069, 0x006C, 0x0065, 0x0076, 0x0061, 0x0074, 0x0061, 0x0020, 0x0075, 0x006E, 0x0061, 0x0020, 0x0073, 0x0063, 0x0068, 0x0065, 0x0064, 0x0061, 0x0020, 0x0067, 0x0072, 0x0061, 0x0066, 0x0069, 0x0063, 0x0061, 0x0020, 0x006E, 0x006F, 0x006E, 0x0020, 0x0073, 0x0075, 0x0070, 0x0070, 0x006F, 0x0072, 0x0074, 0x0061, 0x0074, 0x0061, 0x0021, 0x0020, 0x0053, 0x0065, 0x0020, 0x0073, 0x0069, 0x0020, 0x0063, 0x006F, 0x006E, 0x0074, 0x0069, 0x006E, 0x0075, 0x0061, 0x002C, 0x0020, 0x0073, 0x0069, 0x0020, 0x0070, 0x006F, 0x0074, 0x0072, 0x0065, 0x0062, 0x0062, 0x0065, 0x0072, 0x006F, 0x0020, 0x0076, 0x0065, 0x0072, 0x0069, 0x0066, 0x0069, 0x0063, 0x0061, 0x0072, 0x0065, 0x0020, 0x0072, 0x0069, 0x0073, 0x0075, 0x006C, 0x0074, 0x0061, 0x0074, 0x0069, 0x0020, 0x0069, 0x006E, 0x0061, 0x0074, 0x0074, 0x0065, 0x0073, 0x0069, 0x0020, 0x006F, 0x0020, 0x0063, 0x0072, 0x0061, 0x0073, 0x0068, 0x002E, 0x0020, 0x0043, 0x006F, 0x006E, 0x0073, 0x0075, 0x006C, 0x0074, 0x0061, 0x0020, 0x0069, 0x006C, 0x0020, 0x006D, 0x0061, 0x006E, 0x0075, 0x0061, 0x006C, 0x0065, 0x0020, 0x0070, 0x0065, 0x0072, 0x0020, 0x0075, 0x006C, 0x0074, 0x0065, 0x0072, 0x0069, 0x006F, 0x0072, 0x0069, 0x0020, 0x0069, 0x006E, 0x0066, 0x006F, 0x0072, 0x006D, 0x0061, 0x007A, 0x0069, 0x006F, 0x006E, 0x0069, 0x0020, 0x0073, 0x0075, 0x0069, 0x0020, 0x0072, 0x0065, 0x0071, 0x0075, 0x0069, 0x0073, 0x0069, 0x0074, 0x0069, 0x0020, 0x0064, 0x0069, 0x0020, 0x0073, 0x0069, 0x0073, 0x0074, 0x0065, 0x006D, 0x0061, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x11: // Japanese
    {
        static const wchar_t fmt[] = {0x30B5, 0x30DD, 0x30FC, 0x30C8, 0x3055, 0x308C, 0x3066, 0x3044, 0x306A, 0x3044, 0x30D3, 0x30C7, 0x30AA, 0x30AB, 0x30FC, 0x30C9, 0x304C, 0x691C, 0x51FA, 0x3055, 0x308C, 0x307E, 0x3057, 0x305F, 0xFF01, 0x0020, 0x3053, 0x306E, 0x307E, 0x307E, 0x7D9A, 0x3051, 0x308B, 0x3068, 0x4E88, 0x671F, 0x3057, 0x306A, 0x3044, 0x7D50, 0x679C, 0x3084, 0x30AF, 0x30E9, 0x30C3, 0x30B7, 0x30E5, 0x306E, 0x6050, 0x308C, 0x304C, 0x3042, 0x308A, 0x307E, 0x3059, 0x3002, 0x0020, 0x30DE, 0x30CB, 0x30E5, 0x30A2, 0x30EB, 0x306E, 0x5FC5, 0x8981, 0x52D5, 0x4F5C, 0x74B0, 0x5883, 0x3092, 0x3054, 0x78BA, 0x8A8D, 0x304F, 0x3060, 0x3055, 0x3044, 0x3002, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x30D9, 0x30F3, 0x30C0, 0x30FC, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x30C7, 0x30D0, 0x30A4, 0x30B9, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x15: // Polish
    {
        static const wchar_t fmt[] = {0x0057, 0x0079, 0x006B, 0x0072, 0x0079, 0x0074, 0x006F, 0x0020, 0x006E, 0x0069, 0x0065, 0x006F, 0x0062, 0x0073, 0x0142, 0x0075, 0x0067, 0x0069, 0x0077, 0x0061, 0x006E, 0x0105, 0x0020, 0x006B, 0x0061, 0x0072, 0x0074, 0x0119, 0x0020, 0x0067, 0x0072, 0x0061, 0x0066, 0x0069, 0x0063, 0x007A, 0x006E, 0x0105, 0x0021, 0x0020, 0x0044, 0x0061, 0x006C, 0x0073, 0x007A, 0x0065, 0x0020, 0x006B, 0x006F, 0x0072, 0x007A, 0x0079, 0x0073, 0x0074, 0x0061, 0x006E, 0x0069, 0x0065, 0x0020, 0x007A, 0x0020, 0x0070, 0x0072, 0x006F, 0x0064, 0x0075, 0x006B, 0x0074, 0x0075, 0x0020, 0x006D, 0x006F, 0x017C, 0x0065, 0x0020, 0x0073, 0x0070, 0x006F, 0x0077, 0x006F, 0x0064, 0x006F, 0x0077, 0x0061, 0x0107, 0x0020, 0x006E, 0x0069, 0x0065, 0x0070, 0x006F, 0x017C, 0x0105, 0x0064, 0x0061, 0x006E, 0x0065, 0x0020, 0x007A, 0x0061, 0x0063, 0x0068, 0x006F, 0x0077, 0x0061, 0x006E, 0x0069, 0x0065, 0x0020, 0x006C, 0x0075, 0x0062, 0x0020, 0x0077, 0x0073, 0x0074, 0x0072, 0x007A, 0x0079, 0x006D, 0x0061, 0x006E, 0x0069, 0x0065, 0x0020, 0x0070, 0x0072, 0x006F, 0x0067, 0x0072, 0x0061, 0x006D, 0x0075, 0x002E, 0x0020, 0x0041, 0x0062, 0x0079, 0x0020, 0x0075, 0x007A, 0x0079, 0x0073, 0x006B, 0x0061, 0x0107, 0x0020, 0x0077, 0x0069, 0x0119, 0x0063, 0x0065, 0x006A, 0x0020, 0x0069, 0x006E, 0x0066, 0x006F, 0x0072, 0x006D, 0x0061, 0x0063, 0x006A, 0x0069, 0x002C, 0x0020, 0x0073, 0x006B, 0x006F, 0x006E, 0x0073, 0x0075, 0x006C, 0x0074, 0x0075, 0x006A, 0x0020, 0x0073, 0x0069, 0x0119, 0x0020, 0x007A, 0x0020, 0x0069, 0x006E, 0x0073, 0x0074, 0x0072, 0x0075, 0x006B, 0x0063, 0x006A, 0x0105, 0x0020, 0x006F, 0x0062, 0x0073, 0x0142, 0x0075, 0x0067, 0x0069, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x19: // Russian
    {
        static const wchar_t fmt[] = {0x0412, 0x0430, 0x0448, 0x0430, 0x0020, 0x0432, 0x0438, 0x0434, 0x0435, 0x043E, 0x0020, 0x043A, 0x0430, 0x0440, 0x0442, 0x0430, 0x0020, 0x043D, 0x0435, 0x0020, 0x043F, 0x043E, 0x0434, 0x0434, 0x0435, 0x0440, 0x0436, 0x0438, 0x0432, 0x0430, 0x0435, 0x0442, 0x0441, 0x044F, 0x0021, 0x0020, 0x042D, 0x0442, 0x043E, 0x0020, 0x043C, 0x043E, 0x0436, 0x0435, 0x0442, 0x0020, 0x043F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0441, 0x0442, 0x0438, 0x0020, 0x043A, 0x0020, 0x043D, 0x0435, 0x043F, 0x0440, 0x0435, 0x0434, 0x0441, 0x043A, 0x0430, 0x0437, 0x0443, 0x0435, 0x043C, 0x043E, 0x043C, 0x0443, 0x0020, 0x043F, 0x043E, 0x0432, 0x0435, 0x0434, 0x0435, 0x043D, 0x0438, 0x044E, 0x0020, 0x0438, 0x0020, 0x0437, 0x0430, 0x0432, 0x0438, 0x0441, 0x0430, 0x043D, 0x0438, 0x044E, 0x0020, 0x0438, 0x0433, 0x0440, 0x044B, 0x002E, 0x0020, 0x0414, 0x043B, 0x044F, 0x0020, 0x043F, 0x043E, 0x043B, 0x0443, 0x0447, 0x0435, 0x043D, 0x0438, 0x044F, 0x0020, 0x0438, 0x043D, 0x0444, 0x043E, 0x0440, 0x043C, 0x0430, 0x0446, 0x0438, 0x0438, 0x0020, 0x043E, 0x0020, 0x0441, 0x0438, 0x0441, 0x0442, 0x0435, 0x043C, 0x043D, 0x044B, 0x0445, 0x0020, 0x0442, 0x0440, 0x0435, 0x0431, 0x043E, 0x0432, 0x0430, 0x043D, 0x0438, 0x044F, 0x0445, 0x0020, 0x043E, 0x0431, 0x0440, 0x0430, 0x0442, 0x0438, 0x0442, 0x0435, 0x0441, 0x044C, 0x0020, 0x043A, 0x0020, 0x0440, 0x0443, 0x043A, 0x043E, 0x0432, 0x043E, 0x0434, 0x0441, 0x0442, 0x0432, 0x0443, 0x0020, 0x043F, 0x043E, 0x043B, 0x044C, 0x0437, 0x043E, 0x0432, 0x0430, 0x0442, 0x0435, 0x043B, 0x044F, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }
    case 0x1f: // Turkish
    {
        static const wchar_t fmt[] = {0x0044, 0x0065, 0x0073, 0x0074, 0x0065, 0x006B, 0x006C, 0x0065, 0x006E, 0x006D, 0x0065, 0x0079, 0x0065, 0x006E, 0x0020, 0x0062, 0x0069, 0x0072, 0x0020, 0x0065, 0x006B, 0x0072, 0x0061, 0x006E, 0x0020, 0x006B, 0x0061, 0x0072, 0x0074, 0x0131, 0x0020, 0x0061, 0x006C, 0x0067, 0x0131, 0x006C, 0x0061, 0x006E, 0x0064, 0x0131, 0x0021, 0x0020, 0x0044, 0x0065, 0x0076, 0x0061, 0x006D, 0x0020, 0x0065, 0x0074, 0x006D, 0x0065, 0x006B, 0x0020, 0x0062, 0x0065, 0x006B, 0x006C, 0x0065, 0x006E, 0x006D, 0x0065, 0x0064, 0x0069, 0x006B, 0x0020, 0x0073, 0x006F, 0x006E, 0x0075, 0x00E7, 0x006C, 0x0061, 0x0072, 0x0061, 0x0020, 0x0076, 0x0065, 0x0020, 0x00E7, 0x00F6, 0x006B, 0x006D, 0x0065, 0x006C, 0x0065, 0x0072, 0x0065, 0x0020, 0x0079, 0x006F, 0x006C, 0x0020, 0x0061, 0x00E7, 0x0061, 0x0062, 0x0069, 0x006C, 0x0069, 0x0072, 0x002E, 0x0020, 0x0044, 0x006F, 0x006E, 0x0061, 0x006E, 0x0131, 0x006D, 0x0020, 0x0067, 0x0065, 0x0072, 0x0065, 0x006B, 0x006C, 0x0069, 0x006C, 0x0069, 0x006B, 0x006C, 0x0065, 0x0072, 0x0069, 0x0020, 0x0069, 0x00E7, 0x0069, 0x006E, 0x0020, 0x006C, 0x00FC, 0x0074, 0x0066, 0x0065, 0x006E, 0x0020, 0x0072, 0x0065, 0x0068, 0x0062, 0x0065, 0x0072, 0x0069, 0x006E, 0x0069, 0x007A, 0x0065, 0x0020, 0x0062, 0x0061, 0x015F, 0x0076, 0x0075, 0x0072, 0x0075, 0x006E, 0x002E, 0x000A, 0x000A, 0x0022, 0x0025, 0x0053, 0x0022, 0x0020, 0x005B, 0x0076, 0x0065, 0x006E, 0x0064, 0x006F, 0x0072, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x002C, 0x0020, 0x0064, 0x0065, 0x0076, 0x0069, 0x0063, 0x0065, 0x0020, 0x0069, 0x0064, 0x0020, 0x003D, 0x0020, 0x0030, 0x0078, 0x0025, 0x002E, 0x0034, 0x0078, 0x005D, 0};
        pFmt = fmt;
        break;
    }

    case 0x09: // English
    default:
        break;
    }

    wchar_t msg[1024];
    msg[0] = L'\0';
    msg[sizeof(msg) / sizeof(msg[0]) - 1] = L'\0';
    _snwprintf(msg, sizeof(msg) / sizeof(msg[0]) - 1, pFmt, gpuName, gpuVendorId, gpuDeviceId);

    return msg;
}
#endif

bool CSystem::OpenRenderLibrary(int type, const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION;
#if !defined(DEDICATED_SERVER)
#if defined(WIN32) || defined(WIN64)
    if (!m_env.IsEditor() && !gEnv->IsDedicated())
    {
        unsigned int gpuVendorId = 0, gpuDeviceId = 0, totVidMem = 0;
        char gpuName[256];
        Win32SysInspect::DXFeatureLevel featureLevel = Win32SysInspect::DXFL_Undefined;
        Win32SysInspect::GetGPUInfo(gpuName, sizeof(gpuName), gpuVendorId, gpuDeviceId, totVidMem, featureLevel);

        if (featureLevel < Win32SysInspect::DXFL_11_0)
        {
            const char logMsgFmt[] ("Unsupported GPU configuration!\n- %s (vendor = 0x%.4x, device = 0x%.4x)\n- Dedicated video memory: %d MB\n- Feature level: %s\n");
            CryLogAlways(logMsgFmt, gpuName, gpuVendorId, gpuDeviceId, totVidMem >> 20, GetFeatureLevelAsString(featureLevel));

#if !defined(_RELEASE)
            const bool allowPrompts = m_env.pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "noprompt") == 0;
#else
            const bool allowPrompts = true;
#endif // !defined(_RELEASE)
            if (allowPrompts)
            {
                CryLogAlways("Asking user if they wish to continue...");
                const int mbRes = MessageBoxW(0, GetErrorStringUnsupportedGPU(gpuName, gpuVendorId, gpuDeviceId).c_str(), L"Lumberyard", MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2 | MB_DEFAULT_DESKTOP_ONLY);
                if (mbRes == IDCANCEL)
                {
                    CryLogAlways("User chose to cancel.");
                    return false;
                }
            }
            else
            {
#if !defined(_RELEASE)
                const bool obeyGPUCheck = m_env.pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "anygpu") == 0;
#else
                const bool obeyGPUCheck = true;
#endif // !defined(_RELEASE)
                if (obeyGPUCheck)
                {
                    CryLogAlways("No prompts allowed and unsupported GPU check active. Treating unsupported GPU as error and exiting.");
                    return false;
                }
            }

            CryLogAlways("User chose to continue despite unsupported GPU!");
        }
    }
#endif
#endif // !defined(DEDICATED_SERVER)


#if defined(DEDICATED_SERVER)
    type = R_NULL_RENDERER;
#else
    if (gEnv->IsDedicated())
    {
        type = R_NULL_RENDERER;
    }
#endif



    const char* libname = "";
    if (type == R_DX9_RENDERER)
    {
        libname = DLL_RENDERER_DX9;
    }
    else if (type == R_DX11_RENDERER)
    {
        libname = DLL_RENDERER_DX11;
    }
    else if (type == R_DX12_RENDERER)
    {
        libname = DLL_RENDERER_DX12;
    }
    else if (type == R_NULL_RENDERER)
    {
        libname = DLL_RENDERER_NULL;
    }
    else if (type == R_GL_RENDERER)
    {
        libname = DLL_RENDERER_GL;
    }
    else if (type == R_METAL_RENDERER)
    {
        libname = DLL_RENDERER_METAL;
    }
    else
    {
        CryFatalError("No renderer specified!");
        return false;
    }

    if (!InitializeEngineModule(libname, "EngineModule_CryRenderer", initParams, true))
    {
        return false;
    }

    if (!m_env.pRenderer)
    {
        CryFatalError("Error creating Render System!");
        return false;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitNetwork(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (!InitializeEngineModule(DLL_NETWORK, "EngineModule_CryNetwork", initParams, true))
    {
        return false;
    }

    if (m_env.pNetwork == NULL)
    {
        CryFatalError("Error creating Network System!");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitEntitySystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (!InitializeEngineModule(DLL_ENTITYSYSTEM, "EngineModule_CryEntitySystem", initParams, true))
    {
        return false;
    }

    if (!m_env.pEntitySystem)
    {
        CryFatalError("Error creating Entity System!");
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitDynamicResponseSystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    const char* sDLLName = m_sys_dll_response_system->GetString();

    if (!sDLLName || sDLLName[0] == '\0')
    {
        return false;
    }
    else
    {
        InitializeEngineModule(sDLLName, "EngineModule_CryDynamicResponseSystem", initParams, true);
    }

    if (!m_env.pDynamicResponseSystem)
    {
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Error creating Dynamic Response System from dll: '%s'!", sDLLName);
        return false;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////
bool CSystem::InitInput(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (m_sys_skip_input->GetIVal() || initParams.bUnattendedMode) // Disable CryInput in export mode
    {
        m_env.pInput = new CNullInput();
        return true;
    }

    if (!InitializeEngineModule(DLL_INPUT, "EngineModule_CryInput", initParams, true))
    {
#ifdef WIN32
        if (!initParams.bUnattendedMode)
        {
            MessageBox(NULL, "CryInput.dll could not be loaded. This is likely due to not having XInput support installed.\nPlease install the most recent version of the DirectX runtime and reboot after installing DirectX.", "ERROR: CryInput.dll could not be loaded!", MB_OK | MB_ICONERROR);
        }
#endif
        return false;
    }

    if (!m_env.pInput)
    {
        CryFatalError("Error creating Input System!");
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitConsole()
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (m_env.pConsole)
    {
        m_env.pConsole->Init(this);
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
// attaches the given variable to the given container;
// recreates the variable if necessary
ICVar* CSystem::attachVariable (const char* szVarName, int* pContainer, const char* szComment, int dwFlags)
{
    IConsole* pConsole = GetIConsole();

    ICVar* pOldVar = pConsole->GetCVar (szVarName);
    int nDefault;
    if (pOldVar)
    {
        nDefault = pOldVar->GetIVal();
        pConsole->UnregisterVariable(szVarName, true);
    }

    // NOTE: maybe we should preserve the actual value of the variable across the registration,
    // because of the strange architecture of IConsole that converts int->float->int

    REGISTER_CVAR2(szVarName, pContainer, *pContainer, dwFlags, szComment);

    ICVar* pVar = pConsole->GetCVar(szVarName);

#ifdef _DEBUG
    // test if the variable really has this container
    assert (*pContainer == pVar->GetIVal());
    ++*pContainer;
    assert (*pContainer == pVar->GetIVal());
    --*pContainer;
#endif

    if (pOldVar)
    {
        // carry on the default value from the old variable anyway
        pVar->Set(nDefault);
    }
    return pVar;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitRenderer(WIN_HINSTANCE hinst, WIN_HWND hwnd, const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (m_pUserCallback)
    {
        m_pUserCallback->OnInitProgress("Initializing Renderer...");
    }

    if (m_bEditor)
    {
        m_env.pConsole->GetCVar("r_Width");

        // save current screen width/height/bpp, so they can be restored on shutdown
        m_iWidth = m_env.pConsole->GetCVar("r_Width")->GetIVal();
        m_iHeight = m_env.pConsole->GetCVar("r_Height")->GetIVal();
        m_iColorBits = m_env.pConsole->GetCVar("r_ColorBits")->GetIVal();
    }

    if (!OpenRenderLibrary(m_rDriver->GetString(), initParams))
    {
        return false;
    }

#if defined(AZ_PLATFORM_APPLE_IOS) || defined(AZ_PLATFORM_ANDROID)
    if (m_rWidthAndHeightAsFractionOfScreenSize->GetFlags() & VF_WASINCONFIG)
    {
        int displayWidth = 0;
        int displayHeight = 0;
        if (GetPrimaryPhysicalDisplayDimensions(displayWidth, displayHeight))
        {
            // Ideally we would probably want to clamp this at the source,
            // but I don't believe cvars support specifying a valid range.
            const float scaleFactor = AZ::GetClamp(m_rWidthAndHeightAsFractionOfScreenSize->GetFVal(), 0.1f, 1.0f);
            displayWidth *= scaleFactor;
            displayHeight *= scaleFactor;

            const int maxWidth = m_rMaxWidth->GetIVal();
            if (maxWidth > 0 && maxWidth < displayWidth)
            {
                const float widthScaleFactor = static_cast<float>(maxWidth) / static_cast<float>(displayWidth);
                displayWidth *= widthScaleFactor;
                displayHeight *= widthScaleFactor;
            }

            const int maxHeight = m_rMaxHeight->GetIVal();
            if (maxHeight > 0 && maxHeight < displayHeight)
            {
                const float heightScaleFactor = static_cast<float>(maxHeight) / static_cast<float>(displayHeight);
                displayWidth *= heightScaleFactor;
                displayHeight *= heightScaleFactor;
            }

            m_rWidth->Set(displayWidth);
            m_rHeight->Set(displayHeight);
        }
    }
#endif // defined(AZ_PLATFORM_APPLE_IOS) || defined(AZ_PLATFORM_ANDROID)

#ifdef WIN32
    if (m_env.pRenderer)
    {
        SCustomRenderInitArgs args;
        args.appStartedFromMediaCenter = strstr(initParams.szSystemCmdLine, "ReLaunchMediaCenter") != 0;

        m_hWnd = m_env.pRenderer->Init(0, 0, m_rWidth->GetIVal(), m_rHeight->GetIVal(), m_rColorBits->GetIVal(), m_rDepthBits->GetIVal(), m_rStencilBits->GetIVal(), m_rFullscreen->GetIVal() ? true : false, hinst, hwnd, false, &args, initParams.bShaderCacheGen);
        //Timur, Not very clean code, we need to push new hwnd value to the system init params, so other modules can used when initializing.
        (const_cast<SSystemInitParams*>(&initParams))->hWnd = m_hWnd;

        InitPhysicsRenderer(initParams);

        return (initParams.bShaderCacheGen || m_hWnd != 0);
    }
#else
    if (m_env.pRenderer)
    {
        WIN_HWND h = m_env.pRenderer->Init(0, 0, m_rWidth->GetIVal(), m_rHeight->GetIVal(), m_rColorBits->GetIVal(), m_rDepthBits->GetIVal(), m_rStencilBits->GetIVal(), m_rFullscreen->GetIVal() ? true : false, hinst, hwnd);
        InitPhysicsRenderer(initParams);

#if (defined(LINUX) && !defined(AZ_PLATFORM_ANDROID))|| defined(ORBIS)
        return true;
#else
        if (h)
        {
            return true;
        }
        return (false);
#endif
    }
#endif
    return true;
}

/////////////////////////////////////////////////////////////////////////////////
char* PhysHelpersToStr(int iHelpers, char* strHelpers)
{
    char* ptr = strHelpers;
    if (iHelpers & 128)
    {
        *ptr++ = 't';
    }
    if (iHelpers & 256)
    {
        *ptr++ = 's';
    }
    if (iHelpers & 512)
    {
        *ptr++ = 'r';
    }
    if (iHelpers & 1024)
    {
        *ptr++ = 'R';
    }
    if (iHelpers & 2048)
    {
        *ptr++ = 'l';
    }
    if (iHelpers & 4096)
    {
        *ptr++ = 'i';
    }
    if (iHelpers & 8192)
    {
        *ptr++ = 'e';
    }
    if (iHelpers & 16384)
    {
        *ptr++ = 'g';
    }
    if (iHelpers & 32768)
    {
        *ptr++ = 'w';
    }
    if (iHelpers & 32)
    {
        *ptr++ = 'a';
    }
    if (iHelpers & 64)
    {
        *ptr++ = 'y';
    }
    *ptr++ = iHelpers ? '_' : '0';
    if (iHelpers & 1)
    {
        *ptr++ = 'c';
    }
    if (iHelpers & 2)
    {
        *ptr++ = 'g';
    }
    if (iHelpers & 4)
    {
        *ptr++ = 'b';
    }
    if (iHelpers & 8)
    {
        *ptr++ = 'l';
    }
    if (iHelpers & 16)
    {
        *ptr++ = 'j';
    }
    if (iHelpers >> 16)
    {
        if (!(iHelpers & 1 << 27))
        {
            ptr += sprintf(ptr, "t(%d)", iHelpers >> 16);
        }
        else
        {
            for (int i = 0; i < 16; i++)
            {
                if (i != 11 && iHelpers & 1 << (16 + i))
                {
                    ptr += sprintf(ptr, "f(%d)", i);
                }
            }
        }
    }
    *ptr++ = 0;
    return strHelpers;
}

int StrToPhysHelpers(const char* strHelpers)
{
    const char* ptr;
    int iHelpers = 0, level = 0;
    if (*strHelpers == '1')
    {
        return 7970;
    }
    if (*strHelpers == '2')
    {
        return 7970 | 1 << 31 | 1 << 27;
    }
    for (ptr = strHelpers; *ptr && *ptr != '_'; ptr++)
    {
        switch (*ptr)
        {
        case 't':
            iHelpers |= 128;
            break;
        case 's':
            iHelpers |= 256;
            break;
        case 'r':
            iHelpers |= 512;
            break;
        case 'R':
            iHelpers |= 1024;
            break;
        case 'l':
            iHelpers |= 2048;
            break;
        case 'i':
            iHelpers |= 4096;
            break;
        case 'e':
            iHelpers |= 8192;
            break;
        case 'g':
            iHelpers |= 16384;
            break;
        case 'w':
            iHelpers |= 32768;
            break;
        case 'a':
            iHelpers |= 32;
            break;
        case 'y':
            iHelpers |= 64;
            break;
        }
    }
    if (*ptr == '_')
    {
        ptr++;
    }
    for (; *ptr; ptr++)
    {
        switch (*ptr)
        {
        case 'c':
            iHelpers |= 1;
            break;
        case 'g':
            iHelpers |= 2;
            break;
        case 'b':
            iHelpers |= 4;
            break;
        case 'l':
            iHelpers |= 8;
            break;
        case 'j':
            iHelpers |= 16;
            break;
        case 'f':
            if (*++ptr && *++ptr)
            {
                for (level = 0; *(ptr + 1) && *ptr != ')'; ptr++)
                {
                    level = level * 10 + *ptr - '0';
                }
            }
            iHelpers |= 1 << (16 + level) | 1 << 27;
            break;
        case 't':
            if (*++ptr && *++ptr)
            {
                for (level = 0; *(ptr + 1) && *ptr != ')'; ptr++)
                {
                    level = level * 10 + *ptr - '0';
                }
            }
            iHelpers |= level << 16 | 2;
        }
    }
    return iHelpers;
}

void OnDrawHelpersStrChange(ICVar* pVar)
{
    gEnv->pPhysicalWorld->GetPhysVars()->iDrawHelpers = StrToPhysHelpers(pVar->GetString());
}

bool CSystem::InitPhysics(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Physics, 0, "Init Physics");

    if (!InitializeEngineModule(DLL_PHYSICS, "EngineModule_CryPhysics", initParams, true))
    {
        return false;
    }

    if (!m_env.pPhysicalWorld)
    {
        CryFatalError("Error creating Physics System!");
        return false;
    }
    //m_env.pPhysicalWorld->Init(); // don't need a second Init, the world is created initialized

    // Register physics console variables.
    IConsole* pConsole = GetIConsole();

    PhysicsVars* pVars = m_env.pPhysicalWorld->GetPhysVars();

    REGISTER_CVAR2("p_fly_mode", &pVars->bFlyMode, pVars->bFlyMode, VF_CHEAT,
        "Toggles fly mode.\n"
        "Usage: p_fly_mode [0/1]");
    REGISTER_CVAR2("p_collision_mode", &pVars->iCollisionMode, pVars->iCollisionMode, VF_CHEAT,
        "This variable is obsolete.");
    REGISTER_CVAR2("p_single_step_mode", &pVars->bSingleStepMode, pVars->bSingleStepMode, VF_CHEAT,
        "Toggles physics system 'single step' mode."
        "Usage: p_single_step_mode [0/1]\n"
        "Default is 0 (off). Set to 1 to switch physics system (except\n"
        "players) to single step mode. Each step must be explicitly\n"
        "requested with a 'p_do_step' instruction.");
    REGISTER_CVAR2("p_do_step", &pVars->bDoStep, pVars->bDoStep, VF_CHEAT,
        "Steps physics system forward when in single step mode.\n"
        "Usage: p_do_step 1\n"
        "Default is 0 (off). Each 'p_do_step 1' instruction allows\n"
        "the physics system to advance a single step.");
    REGISTER_CVAR2("p_fixed_timestep", &pVars->fixedTimestep, pVars->fixedTimestep, VF_CHEAT,
        "Toggles fixed time step mode."
        "Usage: p_fixed_timestep [0/1]\n"
        "Forces fixed time step when set to 1. When set to 0, the\n"
        "time step is variable, based on the frame rate.");
    REGISTER_CVAR2("p_draw_helpers_num", &pVars->iDrawHelpers, pVars->iDrawHelpers, VF_CHEAT,
        "Toggles display of various physical helpers. The value is a bitmask:\n"
        "bit 0  - show contact points\n"
        "bit 1  - show physical geometry\n"
        "bit 8  - show helpers for static objects\n"
        "bit 9  - show helpers for sleeping physicalized objects (rigid bodies, ragdolls)\n"
        "bit 10 - show helpers for active physicalized objects\n"
        "bit 11 - show helpers for players\n"
        "bit 12 - show helpers for independent entities (alive physical skeletons,particles,ropes)\n"
        "bits 16-31 - level of bounding volume trees to display (if 0, it just shows geometry)\n"
        "Examples: show static objects - 258, show active rigid bodies - 1026, show players - 2050");
    REGISTER_CVAR2("p_check_out_of_bounds", &pVars->iOutOfBounds, pVars->iOutOfBounds, 0,
        "Check for physics entities outside world (terrain) grid:\n"
        "1 - Enable raycasts; 2 - Enable proximity checks; 3 - Both");
    REGISTER_CVAR2("p_max_contact_gap", &pVars->maxContactGap, pVars->maxContactGap, 0,
        "Sets the gap, enforced whenever possible, between\n"
        "contacting physical objects."
        "Usage: p_max_contact_gap 0.01\n"
        "This variable is used for internal tweaking only.");
    REGISTER_CVAR2("p_max_contact_gap_player", &pVars->maxContactGapPlayer, pVars->maxContactGapPlayer, 0,
        "Sets the safe contact gap for player collisions with\n"
        "the physical environment."
        "Usage: p_max_contact_gap_player 0.01\n"
        "This variable is used for internal tweaking only.");
    REGISTER_CVAR2("p_gravity_z", &pVars->gravity.z, pVars->gravity.z, 0, "");
    REGISTER_CVAR2("p_max_substeps", &pVars->nMaxSubsteps, pVars->nMaxSubsteps, 0,
        "Limits the number of substeps allowed in variable time step mode.\n"
        "Usage: p_max_substeps 5\n"
        "Objects that are not allowed to perform time steps\n"
        "beyond some value make several substeps.");
    REGISTER_CVAR2("p_prohibit_unprojection", &pVars->bProhibitUnprojection, pVars->bProhibitUnprojection, 0,
        "This variable is obsolete.");
    REGISTER_CVAR2("p_enforce_contacts", &pVars->bEnforceContacts, pVars->bEnforceContacts, 0,
        "This variable is obsolete.");
    REGISTER_CVAR2("p_damping_group_size", &pVars->nGroupDamping, pVars->nGroupDamping, 0,
        "Sets contacting objects group size\n"
        "before group damping is used."
        "Usage: p_damping_group_size 3\n"
        "Used for internal tweaking only.");
    REGISTER_CVAR2("p_group_damping", &pVars->groupDamping, pVars->groupDamping, 0,
        "Toggles damping for object groups.\n"
        "Usage: p_group_damping [0/1]\n"
        "Default is 1 (on). Used for internal tweaking only.");
    REGISTER_CVAR2("p_max_substeps_large_group", &pVars->nMaxSubstepsLargeGroup, pVars->nMaxSubstepsLargeGroup, 0,
        "Limits the number of substeps large groups of objects can make");
    REGISTER_CVAR2("p_num_bodies_large_group", &pVars->nBodiesLargeGroup, pVars->nBodiesLargeGroup, 0,
        "Group size to be used with p_max_substeps_large_group, in bodies");
    REGISTER_CVAR2("p_break_on_validation", &pVars->bBreakOnValidation, pVars->bBreakOnValidation, 0,
        "Toggles break on validation error.\n"
        "Usage: p_break_on_validation [0/1]\n"
        "Default is 0 (off). Issues DebugBreak() call in case of\n"
        "a physics parameter validation error.");
    REGISTER_CVAR2("p_time_granularity", &pVars->timeGranularity, pVars->timeGranularity, 0,
        "Sets physical time step granularity.\n"
        "Usage: p_time_granularity [0..0.1]\n"
        "Used for internal tweaking only.");
    REGISTER_CVAR2("p_list_active_objects", &pVars->bLogActiveObjects, pVars->bLogActiveObjects, VF_NULL, "");
    REGISTER_CVAR2("p_profile_entities", &pVars->bProfileEntities, pVars->bProfileEntities, 0,
        "Enables per-entity time step profiling");
    REGISTER_CVAR2("p_profile_functions", &pVars->bProfileFunx, pVars->bProfileFunx, 0,
        "Enables detailed profiling of physical environment-sampling functions");
    REGISTER_CVAR2("p_profile", &pVars->bProfileGroups, pVars->bProfileGroups, 0,
        "Enables group profiling of physical entities");
    REGISTER_CVAR2("p_GEB_max_cells", &pVars->nGEBMaxCells, pVars->nGEBMaxCells, 0,
        "Specifies the cell number threshold after which GetEntitiesInBox issues a warning");
    REGISTER_CVAR2("p_max_velocity", &pVars->maxVel, pVars->maxVel, 0,
        "Clamps physicalized objects' velocities to this value");
    REGISTER_CVAR2("p_max_player_velocity", &pVars->maxVelPlayers, pVars->maxVelPlayers, 0,
        "Clamps players' velocities to this value");
    REGISTER_CVAR2("p_max_bone_velocity", &pVars->maxVelBones, pVars->maxVelBones, 0,
        "Clamps character bone velocities estimated from animations");
    REGISTER_CVAR2("p_force_sync", &pVars->bForceSyncPhysics, 1, 0, "Forces main thread to wait on physics if not completed in time");

    REGISTER_CVAR2("p_max_MC_iters", &pVars->nMaxMCiters, pVars->nMaxMCiters, 0,
        "Specifies the maximum number of microcontact solver iterations *per contact*");
    REGISTER_CVAR2("p_min_MC_iters", &pVars->nMinMCiters, pVars->nMinMCiters, 0,
        "Specifies the minmum number of microcontact solver iterations *per contact set* (this has precedence over p_max_mc_iters)");
    REGISTER_CVAR2("p_accuracy_MC", &pVars->accuracyMC, pVars->accuracyMC, 0,
        "Desired accuracy of microcontact solver (velocity-related, m/s)");
    REGISTER_CVAR2("p_accuracy_LCPCG", &pVars->accuracyLCPCG, pVars->accuracyLCPCG, 0,
        "Desired accuracy of LCP CG solver (velocity-related, m/s)");
    REGISTER_CVAR2("p_max_contacts", &pVars->nMaxContacts, pVars->nMaxContacts, 0,
        "Maximum contact number, after which contact reduction mode is activated");
    REGISTER_CVAR2("p_max_plane_contacts", &pVars->nMaxPlaneContacts, pVars->nMaxPlaneContacts, 0,
        "Maximum number of contacts lying in one plane between two rigid bodies\n"
        "(the system tries to remove the least important contacts to get to this value)");
    REGISTER_CVAR2("p_max_plane_contacts_distress", &pVars->nMaxPlaneContactsDistress, pVars->nMaxPlaneContactsDistress, 0,
        "Same as p_max_plane_contacts, but is effective if total number of contacts is above p_max_contacts");
    REGISTER_CVAR2("p_max_LCPCG_subiters", &pVars->nMaxLCPCGsubiters, pVars->nMaxLCPCGsubiters, 0,
        "Limits the number of LCP CG solver inner iterations (should be of the order of the number of contacts)");
    REGISTER_CVAR2("p_max_LCPCG_subiters_final", &pVars->nMaxLCPCGsubitersFinal, pVars->nMaxLCPCGsubitersFinal, 0,
        "Limits the number of LCP CG solver inner iterations during the final iteration (should be of the order of the number of contacts)");
    REGISTER_CVAR2("p_max_LCPCG_microiters", &pVars->nMaxLCPCGmicroiters, pVars->nMaxLCPCGmicroiters, 0,
        "Limits the total number of per-contact iterations during one LCP CG iteration\n"
        "(number of microiters = number of subiters * number of contacts)");
    REGISTER_CVAR2("p_max_LCPCG_microiters_final", &pVars->nMaxLCPCGmicroitersFinal, pVars->nMaxLCPCGmicroitersFinal, 0,
        "Same as p_max_LCPCG_microiters, but for the final LCP CG iteration");
    REGISTER_CVAR2("p_max_LCPCG_iters", &pVars->nMaxLCPCGiters, pVars->nMaxLCPCGiters, 0,
        "Maximum number of LCP CG iterations");
    REGISTER_CVAR2("p_min_LCPCG_improvement", &pVars->minLCPCGimprovement, pVars->minLCPCGimprovement, 0,
        "Defines a required residual squared length improvement, in fractions of 1");
    REGISTER_CVAR2("p_max_LCPCG_fruitless_iters", &pVars->nMaxLCPCGFruitlessIters, pVars->nMaxLCPCGFruitlessIters, 0,
        "Maximum number of LCP CG iterations w/o improvement (defined by p_min_LCPCGimprovement)");
    REGISTER_CVAR2("p_accuracy_LCPCG_no_improvement", &pVars->accuracyLCPCGnoimprovement, pVars->accuracyLCPCGnoimprovement, 0,
        "Required LCP CG accuracy that allows to stop if there was no improvement after p_max_LCPCG_fruitless_iters");
    REGISTER_CVAR2("p_min_separation_speed", &pVars->minSeparationSpeed, pVars->minSeparationSpeed, 0,
        "Used a threshold in some places (namely, to determine when a particle\n"
        "goes to rest, and a sliding condition in microcontact solver)");
    REGISTER_CVAR2("p_use_distance_contacts", &pVars->bUseDistanceContacts, pVars->bUseDistanceContacts, 0,
        "Allows to use distance-based contacts (is forced off in multiplayer)");
    REGISTER_CVAR2("p_unproj_vel_scale", &pVars->unprojVelScale, pVars->unprojVelScale, 0,
        "Requested unprojection velocity is set equal to penetration depth multiplied by this number");
    REGISTER_CVAR2("p_max_unproj_vel", &pVars->maxUnprojVel, pVars->maxUnprojVel, 0,
        "Limits the maximum unprojection velocity request");
    REGISTER_CVAR2("p_penalty_scale", &pVars->penaltyScale, pVars->penaltyScale, 0,
        "Scales the penalty impulse for objects that use the simple solver");
    REGISTER_CVAR2("p_max_contact_gap_simple", &pVars->maxContactGapSimple, pVars->maxContactGapSimple, 0,
        "Specifies the maximum contact gap for objects that use the simple solver");
    REGISTER_CVAR2("p_skip_redundant_colldet", &pVars->bSkipRedundantColldet, pVars->bSkipRedundantColldet, 0,
        "Specifies whether to skip furher collision checks between two convex objects using the simple solver\n"
        "when they have enough contacts between them");
    REGISTER_CVAR2("p_limit_simple_solver_energy", &pVars->bLimitSimpleSolverEnergy, pVars->bLimitSimpleSolverEnergy, 0,
        "Specifies whether the energy added by the simple solver is limited (0 or 1)");
    REGISTER_CVAR2("p_max_world_step", &pVars->maxWorldStep, pVars->maxWorldStep, 0,
        "Specifies the maximum step physical world can make (larger steps will be truncated)");
    REGISTER_CVAR2("p_use_unproj_vel", &pVars->bCGUnprojVel, pVars->bCGUnprojVel, 0, "internal solver tweak");
    REGISTER_CVAR2("p_tick_breakable", &pVars->tickBreakable, pVars->tickBreakable, 0,
        "Sets the breakable objects structure update interval");
    REGISTER_CVAR2("p_log_lattice_tension", &pVars->bLogLatticeTension, pVars->bLogLatticeTension, 0,
        "If set, breakable objects will log tensions at the weakest spots");
    REGISTER_CVAR2("p_debug_joints", &pVars->bLogLatticeTension, pVars->bLogLatticeTension, 0,
        "If set, breakable objects will log tensions at the weakest spots");
    REGISTER_CVAR2("p_lattice_max_iters", &pVars->nMaxLatticeIters, pVars->nMaxLatticeIters, 0,
        "Limits the number of iterations of lattice tension solver");
    REGISTER_CVAR2("p_max_entity_cells", &pVars->nMaxEntityCells, pVars->nMaxEntityCells, 0,
        "Limits the number of entity grid cells an entity can occupy");
    REGISTER_CVAR2("p_max_MC_mass_ratio", &pVars->maxMCMassRatio, pVars->maxMCMassRatio, 0,
        "Maximum mass ratio between objects in an island that MC solver is considered safe to handle");
    REGISTER_CVAR2("p_max_MC_vel", &pVars->maxMCVel, pVars->maxMCVel, 0,
        "Maximum object velocity in an island that MC solver is considered safe to handle");
    REGISTER_CVAR2("p_max_LCPCG_contacts", &pVars->maxLCPCGContacts, pVars->maxLCPCGContacts, 0,
        "Maximum number of contacts that LCPCG solver is allowed to handle");
    REGISTER_CVAR2("p_approx_caps_len", &pVars->approxCapsLen, pVars->approxCapsLen, 0,
        "Breakable trees are approximated with capsules of this length (0 disables approximation)");
    REGISTER_CVAR2("p_max_approx_caps", &pVars->nMaxApproxCaps, pVars->nMaxApproxCaps, 0,
        "Maximum number of capsule approximation levels for breakable trees");
    REGISTER_CVAR2("p_players_can_break", &pVars->bPlayersCanBreak, pVars->bPlayersCanBreak, 0,
        "Whether living entities are allowed to break static objects with breakable joints");
    REGISTER_CVAR2("p_max_debris_mass", &pVars->massLimitDebris, 10.0f, 0,
        "Broken pieces with mass<=this limit use debris collision settings");
    REGISTER_CVAR2("p_max_object_splashes", &pVars->maxSplashesPerObj, pVars->maxSplashesPerObj, 0,
        "Specifies how many splash events one entity is allowed to generate");
    REGISTER_CVAR2("p_splash_dist0", &pVars->splashDist0, pVars->splashDist0, 0,
        "Range start for splash event distance culling");
    REGISTER_CVAR2("p_splash_force0", &pVars->minSplashForce0, pVars->minSplashForce0, 0,
        "Minimum water hit force to generate splash events at p_splash_dist0");
    REGISTER_CVAR2("p_splash_vel0", &pVars->minSplashVel0, pVars->minSplashVel0, 0,
        "Minimum water hit velocity to generate splash events at p_splash_dist0");
    REGISTER_CVAR2("p_splash_dist1", &pVars->splashDist1, pVars->splashDist1, 0,
        "Range end for splash event distance culling");
    REGISTER_CVAR2("p_splash_force1", &pVars->minSplashForce1, pVars->minSplashForce1, 0,
        "Minimum water hit force to generate splash events at p_splash_dist1");
    REGISTER_CVAR2("p_splash_vel1", &pVars->minSplashVel1, pVars->minSplashVel1, 0,
        "Minimum water hit velocity to generate splash events at p_splash_dist1");
    REGISTER_CVAR2("p_joint_gravity_step", &pVars->jointGravityStep, pVars->jointGravityStep, 0,
        "Time step used for gravity in breakable joints (larger = stronger gravity effects)");
    REGISTER_CVAR2("p_debug_explosions", &pVars->bDebugExplosions, pVars->bDebugExplosions, 0,
        "Turns on explosions debug mode");
    REGISTER_CVAR2("p_num_threads", &pVars->numThreads, pVars->numThreads, 0,
        "The number of internal physics threads");
    REGISTER_CVAR2("p_joint_damage_accum", &pVars->jointDmgAccum, pVars->jointDmgAccum, 0,
        "Default fraction of damage (tension) accumulated on a breakable joint");
    REGISTER_CVAR2("p_joint_damage_accum_threshold", &pVars->jointDmgAccumThresh, pVars->jointDmgAccumThresh, 0,
        "Default damage threshold (0..1) for p_joint_damage_accum");
    REGISTER_CVAR2("p_rope_collider_size_limit", &pVars->maxRopeColliderSize, pVars->maxRopeColliderSize, 0,
        "Disables rope collisions with meshes having more triangles than this (0-skip the check)");

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
    REGISTER_CVAR2("p_net_interp", &pVars->netInterpTime, pVars->netInterpTime, 0,
        "The amount of time which the client will lag behind received packet updates. High values result in smoother movement but introduces additional lag as a trade-off.");
    REGISTER_CVAR2("p_net_extrapmax", &pVars->netExtrapMaxTime, pVars->netExtrapMaxTime, 0,
        "The maximum amount of time the client is allowed to extrapolate the position based on last received packet.");
    REGISTER_CVAR2("p_net_sequencefrequency", &pVars->netSequenceFrequency, pVars->netSequenceFrequency, 0,
        "The frequency at which sequence numbers increase per second, higher values add accuracy but go too high and the sequence numbers will wrap round too fast");
    REGISTER_CVAR2("p_net_debugDraw", &pVars->netDebugDraw, pVars->netDebugDraw, 0,
        "Draw some debug graphics to help diagnose issues (requires p_draw_helpers to be switch on to work, e.g. p_draw_helpers rR_b)");
#else
    REGISTER_CVAR2("p_net_minsnapdist", &pVars->netMinSnapDist, pVars->netMinSnapDist, 0,
        "Minimum distance between server position and client position at which to start snapping");
    REGISTER_CVAR2("p_net_velsnapmul", &pVars->netVelSnapMul, pVars->netVelSnapMul, 0,
        "Multiplier to expand the p_net_minsnapdist based on the objects velocity");
    REGISTER_CVAR2("p_net_minsnapdot", &pVars->netMinSnapDot, pVars->netMinSnapDot, 0,
        "Minimum quat dot product between server orientation and client orientation at which to start snapping");
    REGISTER_CVAR2("p_net_angsnapmul", &pVars->netAngSnapMul, pVars->netAngSnapMul, 0,
        "Multiplier to expand the p_net_minsnapdot based on the objects angular velocity");
    REGISTER_CVAR2("p_net_smoothtime", &pVars->netSmoothTime, pVars->netSmoothTime, 0,
        "How much time should non-snapped positions take to synchronize completely?");
#endif

    REGISTER_CVAR2("p_ent_grid_use_obb", &pVars->bEntGridUseOBB, pVars->bEntGridUseOBB, 0,
        "Whether to use OBBs rather than AABBs for the entity grid setup for brushes");
    REGISTER_CVAR2("p_num_startup_overload_checks", &pVars->nStartupOverloadChecks, pVars->nStartupOverloadChecks, 0,
        "For this many frames after loading a level, check if the physics gets overloaded and freezes non-player physicalized objects that are slow enough");

    pVars->flagsColliderDebris = geom_colltype_debris;
    pVars->flagsANDDebris = ~(geom_colltype_vehicle | geom_colltype6);
    pVars->ticksPerSecond = gEnv->pTimer->GetTicksPerSecond();

    if (m_bEditor)
    {
        // Setup physical grid for Editor.
        int nCellSize = 16;
        m_env.pPhysicalWorld->SetupEntityGrid(2, Vec3(0, 0, 0), (2048) / nCellSize, (2048) / nCellSize, (float)nCellSize, (float)nCellSize);
        pConsole->CreateKeyBind("comma", "#System.SetCVar(\"p_single_step_mode\",1-System.GetCVar(\"p_single_step_mode\"));");
        pConsole->CreateKeyBind("period", "p_do_step 1");
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitPhysicsRenderer(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION;
    //////////////////////////////////////////////////////////////////////////
    // Physics Renderer (for debug helpers)
    //////////////////////////////////////////////////////////////////////////
    if (!initParams.bSkipRenderer && !initParams.bShaderCacheGen)
    {
        m_pPhysRenderer = new CPhysRenderer;
        m_pPhysRenderer->Init(); // needs to be created after physics and renderer
        m_p_draw_helpers_str = REGISTER_STRING_CB("p_draw_helpers", "0", VF_CHEAT,
                "Same as p_draw_helpers_num, but encoded in letters\n"
                "Usage [Entity_Types]_[Helper_Types] - [t|s|r|R|l|i|g|a|y|e]_[g|c|b|l|t(#)]\n"
                "Entity Types:\n"
                "t - show terrain\n"
                "s - show static entities\n"
                "r - show sleeping rigid bodies\n"
                "R - show active rigid bodies\n"
                "l - show living entities\n"
                "i - show independent entities\n"
                "g - show triggers\n"
                "a - show areas\n"
                "y - show rays in RayWorldIntersection\n"
                "e - show explosion occlusion maps\n"
                "Helper Types\n"
                "g - show geometry\n"
                "c - show contact points\n"
                "b - show bounding boxes\n"
                "l - show tetrahedra lattices for breakable objects\n"
                "j - show structural joints (will force translucency on the main geometry)\n"
                "t(#) - show bounding volume trees up to the level #\n"
                "f(#) - only show geometries with this bit flag set (multiple f\'s stack)\n"
                "Example: p_draw_helpers larRis_g - show geometry for static, sleeping, active, independent entities and areas",
                OnDrawHelpersStrChange);

        REGISTER_CVAR2("p_cull_distance", &m_pPhysRenderer->m_cullDist, m_pPhysRenderer->m_cullDist, 0,
            "Culling distance for physics helpers rendering");
        REGISTER_CVAR2("p_wireframe_distance", &m_pPhysRenderer->m_wireframeDist, m_pPhysRenderer->m_wireframeDist, 0,
            "Maximum distance at which wireframe is drawn on physics helpers");
        REGISTER_CVAR2("p_ray_fadeout", &m_pPhysRenderer->m_timeRayFadein, m_pPhysRenderer->m_timeRayFadein, 0,
            "Fade-out time for ray physics helpers");
        REGISTER_CVAR2("p_ray_peak_time", &m_pPhysRenderer->m_rayPeakTime, m_pPhysRenderer->m_rayPeakTime, 0,
            "Rays that take longer then this (in ms) will use different color");
        REGISTER_CVAR2("p_proxy_highlight_threshold", &m_pPhysRenderer->m_maxTris, m_pPhysRenderer->m_maxTris, 0,
            "Physics proxies with triangle counts large than this will be highlighted");
        REGISTER_CVAR2("p_proxy_highlight_range", &m_pPhysRenderer->m_maxTrisRange, m_pPhysRenderer->m_maxTrisRange, 0,
            "Physics proxies with triangle counts >= p_proxy_highlight_threshold+p_proxy_highlight_range will get the maximum highlight");
        REGISTER_CVAR2("p_jump_to_profile_ent", &(m_iJumpToPhysProfileEnt = 0), 0, 0,
            "Move the local player next to the corresponding entity in the p_profile_entities list");
        GetIConsole()->CreateKeyBind("alt_1", "p_jump_to_profile_ent 1");
        GetIConsole()->CreateKeyBind("alt_2", "p_jump_to_profile_ent 2");
        GetIConsole()->CreateKeyBind("alt_3", "p_jump_to_profile_ent 3");
        GetIConsole()->CreateKeyBind("alt_4", "p_jump_to_profile_ent 4");
        GetIConsole()->CreateKeyBind("alt_5", "p_jump_to_profile_ent 5");
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitMovieSystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (!InitializeEngineModule(DLL_MOVIE, "EngineModule_CryMovie", initParams, true))
    {
        return false;
    }

    if (!m_env.pMovieSystem)
    {
        CryFatalError("Error creating Movie System!");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitAISystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init AISystem ");

    const char* sDLLName = m_sys_dll_ai->GetString();
    if (!InitializeEngineModule(sDLLName, "EngineModule_CryAISystem", initParams, false))
    {
        return false;
    }

    if (!m_env.pAISystem)
    {
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Cannot create AI System!");
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitScriptSystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_LUA, 0, "Init ScriptSystem");

    if (!InitializeEngineModule(DLL_SCRIPTSYSTEM, "EngineModule_CryScriptSystem", initParams, true))
    {
        return false;
    }

    if (m_env.pScriptSystem == NULL)
    {
        CryFatalError("Error creating Script System!");
        return (false);
    }

    if (!m_env.IsEditor())
    {
        m_env.pScriptSystem->PostInit();
    }

    return (true);
}

bool CSystem::LaunchAssetProcessor()
{
#if defined(AZ_PLATFORM_WINDOWS) && defined(REMOTE_ASSET_PROCESSOR)
    char exeName[AZ_MAX_PATH_LEN] = { 0 };
    ::GetModuleFileName(::GetModuleHandle(nullptr), exeName, AZ_MAX_PATH_LEN);
    char drive[AZ_MAX_PATH_LEN] = { 0 };
    char dir[AZ_MAX_PATH_LEN] = { 0 };
    char assetProcessorExe[AZ_MAX_PATH_LEN] = { 0 };
    char workingDir[AZ_MAX_PATH_LEN] = { 0 };
    _splitpath_s(exeName, drive, AZ_MAX_PATH_LEN, dir, AZ_MAX_PATH_LEN, nullptr, 0, nullptr, 0);
    _makepath_s(assetProcessorExe, AZ_MAX_PATH_LEN, drive, dir, "AssetProcessor", "exe");
    _makepath_s(workingDir, AZ_MAX_PATH_LEN, drive, dir, nullptr, nullptr);

    if (m_pUserCallback)
    {
        m_pUserCallback->OnInitProgress("Launching remote Asset Processor...");
    }

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_MINIMIZE;
    PROCESS_INFORMATION pi;
    char full_launch_command[AZ_MAX_PATH_LEN] = { 0 };
    azsnprintf(full_launch_command, AZ_MAX_PATH_LEN, "\"%s\" --start-hidden", assetProcessorExe);

    bool created = ::CreateProcess(nullptr, full_launch_command, nullptr, nullptr, FALSE, 0, nullptr, workingDir, &si, &pi) != 0;
    if (!created)
    {
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Unable to start AssetProcessor: %s", assetProcessorExe);
        return false;
    }

    return true;
#elif defined(AZ_PLATFORM_APPLE_OSX)
    if (m_pUserCallback)
    {
        m_pUserCallback->OnInitProgress("Launching remote Asset Processor...");
    }

    int pipefds[2];
    if (pipe(pipefds) || fcntl(pipefds[1], F_SETFD, fcntl(pipefds[1], F_GETFD) | FD_CLOEXEC))
    {
        return false;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Unable to fork process to start AssetProcessor: %s", strerror(errno));
        return false;
    }
    else if (pid == 0)
    {
        // This is the child process that we want to run the asset processor
        close(pipefds[0]);

        // Because execv requires a non-const array of character pointers we
        // have to allocate and copy the strings manually. The last entry in
        // the array has to be a null pointer hence 3 being the size of the 
        // array.
        char* args[3] = { 0 };

        const int sizeOfAssetProcessorString = sizeof("AssetProcessor") + 1;
        args[0] = new char[sizeOfAssetProcessorString];
        azstrcpy(args[0], sizeOfAssetProcessorString, "AssetProcessor");
        const int sizeOfStartHiddenArg = sizeof("--start-hidden") + 1;
        args[1] = new char[sizeOfStartHiddenArg];
        azstrcpy(args[1], sizeOfStartHiddenArg, "--start-hidden");

        // Current assumption, which is true for the editor, is that the current
        // directory is at the root of the game/editor directory and so we can
        // build a directory to the AssetProcessor from there.
        string assetProcessorCommand = "./BinMac64";
#ifdef _DEBUG
        assetProcessorCommand += ".Debug";
#endif
        assetProcessorCommand += "/AssetProcessor";

        execv(assetProcessorCommand.c_str(), args);

        // If we get here execv failed to run properly and the only thing we can
        // do without doing any damage is to exit
        write(pipefds[1], &errno, sizeof(int));
        exit(errno);
    }
    else
    {
        // We are the parent if we get here.
        close(pipefds[1]);
        int count = 0; 
        int err = 0;
        while ((count = read(pipefds[0], &err, sizeof(errno))) == -1)
        {
            if (errno != EAGAIN && errno != EINTR)
            {
                break;
            }
        }

        if (count) {
            CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Unable to start AssetProcessor: %s", strerror(err));
            return false;
        }
        close(pipefds[0]);
        return true;
    }
#endif
    return false;
}

#ifdef REMOTE_ASSET_PROCESSOR
bool CSystem::ConnectToAssetProcessor(const SSystemInitParams& initParams, bool waitForConnect)
{
    using AzFramework::AssetSystem::AssetSystemErrors;

    //get the engine connection
    AzFramework::AssetSystem::AssetProcessorConnection* engineConnection = static_cast<AzFramework::AssetSystem::AssetProcessorConnection*>(AzFramework::SocketConnection::GetInstance());
    if (!engineConnection)
    {
        return false;
    }

    //are we supposed to initiate the connection to the ap
    if (initParams.connectToRemote)
    {
        if (m_pUserCallback)
        {
            m_pUserCallback->OnInitProgress("Connecting to the Asset Processor.");
        }

        //start the asset processor connection async
        engineConnection->Connect(initParams.remoteIP, initParams.remotePort);

        //if we should wait for the connection before proceeding
        if (waitForConnect)
        {
            AZStd::chrono::system_clock::time_point start, last;
            start = last = AZStd::chrono::system_clock::now();
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_APPLE)
            //poll, wait for either connection or failure/timeout
            //we don't care if we actually connected and the negotiation failed until
            //the last check. This will give the user the maximum amount of time to
            //make any adjustments, like adding the ip to the white list
            //however if we timeout, meaning it never negotiated then
            //we can present that information to the user

            //we should be able to connect
            while (!engineConnection->IsConnected() && AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::system_clock::now() - start) < AZStd::chrono::milliseconds(2500))
            {
                //update the feedback text to animate and pump every 250 milliseconds
                if (m_pUserCallback && AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::system_clock::now() - last) >= AZStd::chrono::milliseconds(250))
                {
                    last = AZStd::chrono::system_clock::now();
                    AZStd::string AnimateConnectMessage("Connecting to the Asset Processor");
                    int dots = (AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::system_clock::now() - start) / AZStd::chrono::milliseconds(250)) % 3;
                    AnimateConnectMessage.append("....", dots);
                    m_pUserCallback->OnInitProgress(AnimateConnectMessage.c_str());
                }

                AZStd::this_thread::yield();
            }
            if (!engineConnection->IsConnected())
            {
                bool isAssetProcessorLaunched = LaunchAssetProcessor();
                if (!isAssetProcessorLaunched)
                {
                    // if we are unable to launch asset processor
                    EBUS_EVENT(AzFramework::AssetSystemInfoBus, OnError, AssetSystemErrors::ASSETSYSTEM_FAILED_TO_LAUNCH_ASSETPROCESSOR);
                }
            }
#endif//defined(AZ_PLATFORM_WINDOWS)

            //give the AP 20 seconds to connect (virus scanner can really slow it down on its initial launch!)
            start = AZStd::chrono::system_clock::now();
            last = start;
            while (!engineConnection->IsConnected() && AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::system_clock::now() - start) < AZStd::chrono::milliseconds(120000))
            {
                if (m_pUserCallback && AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::system_clock::now() - last) >= AZStd::chrono::milliseconds(250))
                {
                    last = AZStd::chrono::system_clock::now();
                    AZStd::string AnimateConnectMessage("Connecting to the Asset Processor");
                    int dots = (AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(AZStd::chrono::system_clock::now() - start) / AZStd::chrono::milliseconds(250)) % 3;
                    AnimateConnectMessage.append("....", dots);
                    m_pUserCallback->OnInitProgress(AnimateConnectMessage.c_str());
                }

                AZStd::this_thread::yield();
            }

            if (!engineConnection->IsConnected() && engineConnection->NegotiationFailed())
            {
                //The negotiation failed, this means we did connect, but failed to establish a connection
                EBUS_EVENT(AzFramework::AssetSystemConnectionNotificationsBus, ConnectionFailed);

                engineConnection->Disconnect(); //should stop the async connection from retry

                CryLogAlways("Negotiation with AssetProcessor failed. Check your bootstrap.cfg to make sure your settings are correct and that the Asset Processor is running on the computer that is connected via USB to, or on the same network as, this device");

                if (m_pUserCallback)
                {
                    m_pUserCallback->OnInitProgress("Negotiation failed with the Asset Processor! Quitting...");
                }

                IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
                if (pOS)
                {
                    pOS->DebugMessageBox("Negotiation failed with the Asset Processor!\nMake sure the Asset Processor is running out of the correct branch.\nQuitting...", "AP Fatal");
                }

                return false;
            }
            else if(!engineConnection->IsConnected())
            {
                //We never contacted an AP and we timed out
                EBUS_EVENT(AzFramework::AssetSystemConnectionNotificationsBus, ConnectionFailed);

                engineConnection->Disconnect(); //should stop the async connection from retry

                CryLogAlways("Connection timeout with AssetProcessor. Check your bootstrap.cfg to make sure your settings are correct and that the Asset Processor is running on the computer that is connected via USB to, or on the same network as, this device");

                if (m_pUserCallback)
                {
                    m_pUserCallback->OnInitProgress("Connection timeout with the Asset Processor! Quitting...");
                }

                IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
                if (pOS)
                {
                    pOS->DebugMessageBox("Connection timeout with the Asset Processor!\nMake sure the Asset Processor is running out of the correct branch.\nQuitting...", "AP Fatal");
                }

                return false;
            }
        }
    }

    return true;
}

void CSystem::WaitForAssetProcessorToBeReady()
{
    using namespace AzFramework::AssetSystem;
    AzFramework::SocketConnection* engineConnection = AzFramework::SocketConnection::GetInstance();
    if (!engineConnection)
    {
        return;
    }

    // while we wait, let's get some ping times.

    float pingTime = 0.0f;
    AzFramework::AssetSystemRequestBus::BroadcastResult(pingTime, &AzFramework::AssetSystem::AssetSystemRequests::GetAssetProcessorPingTimeMilliseconds);
    if (pingTime > 0.0f)
    {
        AZ_TracePrintf("AssetSystem", "Ping time to asset processor: %0.2f milliseconds\n", pingTime);
    }
    AZ_TRACE_METHOD();
    bool isAssetProcessorReady = false;
    string platformName = GetAssetsPlatform();
    while (!isAssetProcessorReady)
    {
        // it is assumed that PumpWindowMessage keeps consuming system messages on whatever system you are on
        PumpWindowMessage(true, m_hWnd);

        if (!engineConnection->IsConnected())
        {
            //If we are here than it means we have lost connection with AP
            IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
            if (pOS)
            {
                pOS->DebugMessageBox("Lost the connection to the Asset Processor!\nMake sure the Asset Processor is running.", "AP Fatal");
            }

            return;
        }
        //Keep asking the AP about it status, until it is ready
        RequestAssetProcessorStatus request;
        request.m_platform = platformName;
        ResponseAssetProcessorStatus response;
        if (!SendRequest(request, response))
        {
            AZ_Error("Editor", false, "Failed to send Asset Processor Status request for platform %s", platformName.c_str());

            IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
            if (pOS)
            {
                pOS->DebugMessageBox("Failed to send Asset Processor Status request!", "AP Fatal");
            }

            return;
        }
        else
        {
            if (response.m_isAssetProcessorReady)
            {
                isAssetProcessorReady = true;

                if (m_pUserCallback)
                {
                    m_pUserCallback->OnInitProgress("Asset Processor is now ready...");
                    return;
                }
            }
            else
            {
                if (m_pUserCallback)
                {
                    char msgBuffer[1024];
                    if (response.m_numberOfPendingJobs)
                    {
                        azsnprintf(msgBuffer, 1024, "Asset Processor working... %d jobs remaining.", response.m_numberOfPendingJobs);
                    }
                    else
                    {
                        azsnprintf(msgBuffer, 1024, "Asset Processor working...");
                    }

                    m_pUserCallback->OnInitProgress(msgBuffer);
                }
            }
        }
        // Throttle this, each loop actually sends network traffic to the AP and there's no point in running at 100x a second, but 10x is smooth.
        AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100)); // on some systems, PumpWindowMessage may not sleep.
    }
}

void CSystem::ConnectFromAssetProcessor(const SSystemInitParams& initParams, bool waitForConnect)
{
    AzFramework::AssetSystem::AssetProcessorConnection* engineConnection = static_cast<AzFramework::AssetSystem::AssetProcessorConnection*>(AzFramework::SocketConnection::GetInstance());
    if (!engineConnection)
    {
        return;
    }

#if !defined(CONSOLE)
    CRY_ASSERT(!initParams.connectToRemote && !m_env.IsEditor());
#endif

    //should we listen for a connection from the asset processor
    if (!initParams.connectToRemote)
    {
        //game instances listen on port 22229 currently its not configurable
        engineConnection->Listen(22229);

        //should we wait for connection to proceed
        if (waitForConnect)
        {
            //infinite wait for a connection
            while (!engineConnection->IsConnected())
            {
                AZStd::this_thread::yield();
            }

            //we always give the ap some time to setup our connection
            AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(1000));

            if (m_pUserCallback)
            {
                m_pUserCallback->OnInitProgress("Connection to Remote Asset Processor established...");
            }
        }
    }
}

#endif //REMOTE_ASSET_PROCESSOR

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFileSystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION;
    using namespace AzFramework::AssetSystem;

    if (m_pUserCallback)
    {
        m_pUserCallback->OnInitProgress("Initializing File System...");
    }

    bool bLvlRes = false; // true: all assets since executable start are recorded, false otherwise

    // set file IO, used by CryPak and others
    m_env.pFileIO = aznew AZ::IO::LocalFileIO();
    m_env.pResourceCompilerHelper = nullptr;

#if defined(REMOTE_ASSET_PROCESSOR)

    bool allowedEngineConnection = !m_env.IsInToolMode() && !initParams.bMinimal && !initParams.bTestMode;
    bool allowedRemoteIO = allowedEngineConnection && initParams.remoteFileIO && !m_env.IsEditor();
    bool connInitialized = false;

    AZStd::string branch = initParams.branchToken;
    AZStd::string platform = m_env.pSystem->GetAssetsPlatform();
    AZStd::string identifier = m_env.IsEditor() ? "EDITOR" : "GAME";
    EBUS_EVENT_RESULT(connInitialized, AzFramework::AssetSystemRequestBus, ConfigureSocketConnection, branch, platform, identifier);

    AzFramework::AssetSystem::AssetProcessorConnection* engineConnection = static_cast<AzFramework::AssetSystem::AssetProcessorConnection*>(AzFramework::SocketConnection::GetInstance());
    if (engineConnection)
    {
        if (allowedEngineConnection && connInitialized)
        {
            bool waitForConnection = m_env.IsEditor() || initParams.waitForConnection || initParams.remoteFileIO;
            // if not in tool mode than Init AssetProcessor and connect with it
            if (initParams.connectToRemote)
            {
                //we want to initiate a connection to the ap
                //since we are initiating, we can fail, we take care of that here by returning false
                if (!ConnectToAssetProcessor(initParams, waitForConnection))
                {
                    IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
                    if (pOS)
                    {
                        pOS->DebugMessageBox("We failed to connect to the Asset Processor!\nQuitting...", "AP Fatal");
                    }

                    return false;
                }
            }
            else
            {
                //we want the listen for a connection from the ap
                //listening can not fail, we wait infinitely for a valid connection
                ConnectFromAssetProcessor(initParams, waitForConnection);
            }
        }

        if (allowedRemoteIO)
        {
            m_env.pFileIO = new AZ::IO::RemoteFileIO(m_env.pFileIO); //use the local file io for exclusion io
        }

        if (engineConnection->IsConnected())
        {
            //if we are connected than we must check and make sure that AP is ready before proceeding further.
            WaitForAssetProcessorToBeReady();

            if (!engineConnection->IsConnected())
            {
#if defined(AZ_PLATFORM_WINDOWS)
                CryMessageBox("Closing application.  The application cannot continue because the AssetProcessor is no longer running.  Please restart the application and make sure that the AssetProcessor is running", "Error", MB_OK | MB_ICONERROR);
#endif
                IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
                if (pOS)
                {
                    pOS->DebugMessageBox("Closing application.  The application cannot continue because the AssetProcessor is no longer running.  Please restart the application and make sure that the AssetProcessor is running!\nQuitting...", "AP Fatal");
                }

                return false;
            }
        }
    }

#endif

    // The SetInstance calls below will assert if this has already been set and we don't clear first
    // Application::StartCommon will attempt to set up our FileIO, in Monolithic builds this means
    // g_fileIOInstance is set here already
    AZ::IO::FileIOBase::SetInstance(nullptr);
    AZ::IO::FileIOBase::SetDirectInstance(nullptr);

    // set up the AZCore binding to this file IO:
    // we start with both bindings set directly to the same one, until crypak works:
    AZ::IO::FileIOBase::SetInstance(m_env.pFileIO);
    AZ::IO::FileIOBase::SetDirectInstance(m_env.pFileIO);

#if !defined(_RELEASE)
    const ICmdLineArg* pArg = m_pCmdLine->FindArg(eCLAT_Pre, "LvlRes");          // -LvlRes command line option

    if (pArg)
    {
        bLvlRes = true;
    }
#endif // !defined(_RELEASE)

    bool usingAssetCache = initParams.UseAssetCache();
    const char* rootPath = usingAssetCache ? initParams.rootPathCache : initParams.rootPath;
    const char* assetsPath = usingAssetCache ? initParams.assetsPathCache : initParams.assetsPath;

    if (rootPath == 0)
    {
        CryLog("No root path specified in SystemInitParams. We cannot continue!");

        IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
        if (pOS)
        {
            pOS->DebugMessageBox("No root path specified in SystemInitParams. We cannot continue!\nQuitting...", "SystemInitParams Fatal");
        }

        return false;
    }

    if (assetsPath == 0)
    {
        CryLog("No assets path specified in SystemInitParams. We cannot continue!");

        IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
        if (pOS)
        {
            pOS->DebugMessageBox("No assets path specified in SystemInitParams. We cannot continue!\nQuitting...", "SystemInitParams Fatal");
        }

        return false;
    }

    // establish the root folder and assets folder immediately.
    // Other folders that can be computed from the root can be specified later.
    m_env.pFileIO->SetAlias("@root@", rootPath);
    m_env.pFileIO->SetAlias("@assets@", assetsPath);

    if (initParams.userPath[0] == 0)
    {
        string outPath = PathUtil::Make(rootPath, "user");
        m_env.pFileIO->SetAlias("@user@", outPath.c_str());
    }
    else
    {
        m_env.pFileIO->SetAlias("@user@", initParams.userPath);
    }

    if (initParams.logPath[0] == 0)
    {
        char resolveBuffer[AZ_MAX_PATH_LEN] = { 0 };

        m_env.pFileIO->ResolvePath("@user@", resolveBuffer, AZ_MAX_PATH_LEN);
        string outPath = PathUtil::Make(resolveBuffer, "log");
        m_env.pFileIO->SetAlias("@log@", outPath.c_str());
    }
    else
    {
        m_env.pFileIO->SetAlias("@log@", initParams.logPath);
    }

    m_env.pFileIO->CreatePath("@root@");
    m_env.pFileIO->CreatePath("@user@");
    m_env.pFileIO->CreatePath("@log@");

    if ((!m_env.IsInToolMode()) || (m_bShaderCacheGenMode)) // in tool mode, the promise is that you won't access @cache@!
    {
        string finalCachePath;
        if (initParams.cachePath[0] == 0)
        {
            char resolveBuffer[AZ_MAX_PATH_LEN] = { 0 };

            m_env.pFileIO->ResolvePath("@user@", resolveBuffer, AZ_MAX_PATH_LEN);
            finalCachePath = PathUtil::Make(resolveBuffer, "cache");
        }
        else
        {
            finalCachePath = initParams.cachePath;
        }

#if defined(AZ_PLATFORM_WINDOWS) && defined(REMOTE_ASSET_PROCESSOR)
        // on windows, we might be running editor and game and multiple games and so on (consoles cant do this)
        // pick a non-locked cache, since shaders actually require separate caches:

        string originalPath = finalCachePath;

        if (!allowedRemoteIO) // not running on VFS
        {
            int attemptNumber = 0;
            const int maxAttempts = 16;

            char workBuffer[AZ_MAX_PATH_LEN] = { 0 };
            while (attemptNumber < maxAttempts)
            {
                finalCachePath = originalPath;
                if (attemptNumber != 0)
                {
                    azsnprintf(workBuffer, AZ_MAX_PATH_LEN, "%s%i", originalPath.c_str(), attemptNumber);
                    finalCachePath = workBuffer;
                }
                else
                {
                    finalCachePath = originalPath;
                }

                ++attemptNumber; // do this here so we don't forget

                m_env.pFileIO->CreatePath(finalCachePath.c_str());
                // if the directory already exists, check for locked file
                string outLockPath = PathUtil::Make(finalCachePath.c_str(), "lockfile.txt");

                // note, the zero here after GENERIC_READ|GENERIC_WRITE indicates no share access at all
                g_cacheLock = CreateFileA(outLockPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, 0);
                if (g_cacheLock != INVALID_HANDLE_VALUE)
                {
                    break;
                }
            }

            if (attemptNumber >= maxAttempts)
            {
                CryLogAlways("Couldn't find a valid cache folder after %i attempts, cannot continue", attemptNumber);

                IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
                if (pOS)
                {
                    pOS->DebugMessageBox("Couldn't find a valid cache folder, cannot continue!\nQuitting...", "AP Fatal");
                }

                return false;
            }
        }

#endif // defined(AZ_PLATFORM_WINDOWS)&&defined(REMOTE_ASSET_PROCESSOR)
        CryLog("Using %s folder for cache.", finalCachePath.c_str());
        m_env.pFileIO->SetAlias("@cache@", finalCachePath.c_str());
        m_env.pFileIO->CreatePath("@cache@");
    }

    m_env.pFilePathManager = new GameFilePathManager();

    CCryPak* pCryPak;
    pCryPak = new CCryPak(m_env.pLog, &g_cvars.pakVars, bLvlRes, initParams.pGameStartup);
    pCryPak->SetGameFolderWritable(m_bGameFolderWritable);
    m_env.pCryPak = pCryPak;

    // now that crypak is set up, swap the default instance of fileIO with a crypak-based file io
    AZ::IO::CryPakFileIO* pakFileIO = aznew AZ::IO::CryPakFileIO();
    AZ::IO::FileIOBase::SetInstance(nullptr);
    AZ::IO::FileIOBase::SetInstance(pakFileIO);
    m_env.pFileIO = pakFileIO;

    if (m_bEditor || bLvlRes)
    {
        m_env.pCryPak->RecordFileOpen(ICryPak::RFOM_EngineStartup);
    }

    //init crypak
    if (m_env.pCryPak->Init(""))
    {
#if !defined(_RELEASE)
        const ICmdLineArg* pakalias = m_pCmdLine->FindArg(eCLAT_Pre, "pakalias");
#else
        const ICmdLineArg* pakalias = NULL;
#endif // !defined(_RELEASE)
        if (pakalias && strlen(pakalias->GetValue()) > 0)
        {
            m_env.pCryPak->ParseAliases(pakalias->GetValue());
        }
    }
    else
    {
        IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
        if (pOS)
        {
            pOS->DebugMessageBox("Crypak init failed, cannot continue!\nQuitting...", "CryPak Fatal");
        }

        return false;
    }

    //we are good to go
    return true;
}


void CSystem::ShutdownFileSystem()
{
#if defined(AZ_PLATFORM_WINDOWS)
    if (g_cacheLock != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_cacheLock);
        g_cacheLock = INVALID_HANDLE_VALUE;
    }
#endif

    using namespace AZ::IO;

    FileIOBase* directInstance = FileIOBase::GetDirectInstance();
    FileIOBase* pakInstance = FileIOBase::GetInstance();

    if (directInstance == m_env.pFileIO)
    {
        // we only mess with file io if we own the instance that we installed.
        // if we dont' own the instance, then we never configured fileIO and we should not alter it.
        delete directInstance;
        FileIOBase::SetDirectInstance(nullptr);

        if (pakInstance != directInstance)
        {
            delete pakInstance;
            FileIOBase::SetInstance(nullptr);
        }
    }

    m_env.pFileIO = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFileSystem_LoadEngineFolders(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION;

    // This CVar needs to be present before any files are loaded
    REGISTER_INT("sys_FilesystemCaseSensitivity", 0, VF_NULL, "0 = Ignore letter casing mismatches, 1 = Show warning on mismatch, 2 = Show error on mismatch");

    // Load value of sys_game_folder from system.cfg into the sys_game_folder console variable
    {
        ILoadConfigurationEntrySink* pCVarsWhiteListConfigSink = GetCVarsWhiteListConfigSink();
        LoadConfiguration(m_systemConfigName.c_str(), pCVarsWhiteListConfigSink);
        CryLogAlways("Loading system configuration from %s...", m_systemConfigName.c_str());
    }


#if defined(CRY_ENABLE_RC_HELPER)
    if (!m_env.pResourceCompilerHelper)
    {
        m_env.pResourceCompilerHelper = new CResourceCompilerHelper();
    }
#endif
    // you may not set these in game.cfg or in system.cfg
    m_sys_game_folder->ForceSet(initParams.gameFolderName);
    m_sys_dll_game->ForceSet(initParams.gameDLLName);

    CryLogAlways("GameDir: %s\n", m_sys_game_folder->GetString());

#if !defined(AZ_MONOLITHIC_BUILD)
    CryLogAlways("GameDLL: %s%s%s\n", CrySharedLibraryPrefix, m_sys_dll_game->GetString(), CrySharedLibraryExtension);
#endif // AZ_MONOLITHIC_BUILD


    // simply open all paks if fast load pak can't be found
    if (!m_pResourceManager->LoadFastLoadPaks(true))
    {
        OpenBasicPaks();
    }


    // Load game-specific folder.
    LoadConfiguration("game.cfg");

    if (initParams.bShaderCacheGen)
    {
        LoadConfiguration("shadercachegen.cfg");
    }

#if !defined(_RELEASE)
    if (const ICmdLineArg* pModArg = GetICmdLine()->FindArg(eCLAT_Pre, "MOD"))
    {
        if (IsMODValid(pModArg->GetValue()))
        {
            string modPath;
            modPath.append("Mods\\");
            modPath.append(pModArg->GetValue());
            modPath.append("\\");

            m_env.pCryPak->AddMod(modPath.c_str());
        }
    }

#endif // !defined(_RELEASE)

    // We do not use CVar groups on the consoles
    AddCVarGroupDirectory("Config/CVarGroups");

    return (true);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitStreamEngine()
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (m_pUserCallback)
    {
        m_pUserCallback->OnInitProgress("Initializing Stream Engine...");
    }

    m_pStreamEngine = new CStreamEngine();

    return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitFont(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init FontSystem");

    if (!InitializeEngineModule(DLL_FONT, "EngineModule_CryFont", initParams, true))
    {
        return false;
    }

    if (!m_env.pCryFont)
    {
        CryFatalError("Error creating Font System!");
        return false;
    }

    if (gEnv->IsDedicated())
    {
        return true;
    }

    if (!LoadFontInternal(m_pIFont, "default"))
    {
        return false;
    }

    if (!LoadFontInternal(m_pIFontUi, "default-ui"))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::Init3DEngine(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init 3D Engine");

    if (!InitializeEngineModule(DLL_3DENGINE, "EngineModule_Cry3DEngine", initParams, true))
    {
        return false;
    }

    if (!m_env.p3DEngine)
    {
        CryFatalError("Error creating 3D Engine!");
        return false;
    }

    if (!m_env.p3DEngine->Init())
    {
        CryFatalError("Error initializing 3D Engine!");
        return false;
    }
    m_pProcess = m_env.p3DEngine;
    m_pProcess->SetFlags(PROC_3DENGINE);

    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitAudioSystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init Audio System");

    // Music System is legacy and will be removed soon!
    m_env.pMusicSystem = new Audio::CNULLMusicSystem();
    if (!m_env.pMusicSystem)
    {
        CryFatalError("<Audio>: Could not create an instance of CNULLMusicSystem!");
    }

    // Create a deprecation object as pAudioSystem...
    m_env.pAudioSystem = new Audio::AudioSystemDeprecationShim();
    if (!m_env.pAudioSystem)
    {
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<Audio>: Could not create an instance of AudioSystemDeprecationShim!");
    }

    // Initialize the main Audio system and implementation modules.
    bool bAudioInitSuccess = false;
    if (!initParams.bPreview
        && !m_bDedicatedServer
        && !initParams.bShaderCacheGen
        && m_sys_audio_disable->GetIVal() == 0)
    {
        INDENT_LOG_DURING_SCOPE();
        bAudioInitSuccess = InitializeEngineModule(DLL_SOUND, AUDIO_SYSTEM_MODULE_NAME, initParams, false);
    }
    else
    {
        new Audio::CNULLAudioSystem();
        bAudioInitSuccess = true;

        if (m_bDedicatedServer)
        {
            bAudioInitSuccess = true;
            CryLogAlways("<Audio>: Running with NULL Audio System on Dedicated Server.");
        }
        else
        {
            CryLogAlways("<Audio>: Skipping Audio System Initialization - it was disabled by startup settings!");
        }
    }

    if (bAudioInitSuccess)
    {
        AZ_Assert(Audio::AudioSystemRequestBus::HasHandlers(),
            "Initialization of Audio System was a success, yet Audio System is not fully connected to EBus!");
    }
    else
    {
        AZ_Assert(!Audio::AudioSystemRequestBus::HasHandlers(),
            "Initialization of Audio System was NOT a success, yet Audio System EBus is connected!");
    }

    return bAudioInitSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitAnimationSystem(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init AnimationSystem");

    if (!InitializeEngineModule(DLL_ANIMATION, "EngineModule_CryAnimation", initParams, true))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitVTuneProfiler()
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

#ifdef PROFILE_WITH_VTUNE
    HMODULE hModule = CryLoadLibrary("VTuneApi.dll");
    if (hModule)
    {
        VTPause = (VTuneFunction) CryGetProcAddress(hModule, "VTPause");
        VTResume = (VTuneFunction) CryGetProcAddress(hModule, "VTResume");
        if (!VTPause || !VTResume)
        {
            CryFatalError("Failed to find VTPause");
        }
        else
        {
            CryLogAlways("VTune API Initialized");
        }
    }
    else
    {
        CryFatalError("Failed to load VTuneAPI.dll");
    }
#endif //PROFILE_WITH_VTUNE
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitShine(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init LyShine");

    EBUS_EVENT(UiSystemBus, InitializeSystem);

    if (!m_env.pLyShine)
    {
        // We currently require the LyShine gem to be enabled for all projects.
        // We don't really need CryFatalError here - there is no need to do a debug break since returning false
        // will exit the program. However, there doesn't currently seem to be a good system to force a
        // popup dialog message from here without using this. AZ_Error only displays a dialog in debug.
        CryFatalError("Error initializing the LyShine system! Please check that the LyShine gem is enabled for this project in ProjectConfigurator.");
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::InitGems(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    if (!m_pGemManager)
    {
        m_pGemManager = CreateIGemManager();
    }

    if (!m_pGemManager->Initialize(initParams))
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////
bool CSystem::InitLmbrAWS(const SSystemInitParams& initParams)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init LmbrAWS");

    if (!InitializeEngineModule(DLL_LMBRAWS, "EngineModule_LmbrAWS", initParams))
    {
        return false;
    }

    if (!m_env.pLmbrAWS)
    {
        CryFatalError("Error creating LmbrAWS System!");
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InitLocalization()
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Open Localization Pak");

    // Set the localization folder
    ICVar* pCVar = m_env.pConsole != 0 ? m_env.pConsole->GetCVar("sys_localization_folder") : 0;
    if (pCVar)
    {
        static_cast<CCryPak* const>(m_env.pCryPak)->SetLocalizationFolder(g_cvars.sys_localization_folder->GetString());
    }

    string language = CRYENGINE_DEFAULT_LOCALIZATION_LANG;

    if (m_pLocalizationManager == NULL)
    {
        m_pLocalizationManager = new CLocalizedStringsManager(this);
    }

    pCVar = m_env.pConsole != 0 ? m_env.pConsole->GetCVar("g_language") : 0;
    if (pCVar)
    {
        if (strlen(pCVar->GetString()) == 0)
        {
            pCVar->Set(language);
        }
        else
        {
            language = pCVar->GetString();
        }
    }
    GetLocalizationManager()->SetLanguage(language);

    // if the language value cannot be found, let's default to the english pak
    OpenLanguagePak(language);

    pCVar = m_env.pConsole != 0 ? m_env.pConsole->GetCVar("g_languageAudio") : 0;
    if (pCVar)
    {
        if (strlen(pCVar->GetString()) == 0)
        {
            pCVar->Set(language);
        }
        else
        {
            language = pCVar->GetString();
        }
    }
    OpenLanguageAudioPak(language);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenBasicPaks()
{
    static bool bBasicPaksLoaded = false;
    if (bBasicPaksLoaded)
    {
        return;
    }
    bBasicPaksLoaded = true;

    LOADING_TIME_PROFILE_SECTION;
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Open Pak Files");

    // open pak files
    string paksFolder = "@assets@/*.pak"; // (@assets@ assumed)
    m_env.pCryPak->OpenPacks(paksFolder.c_str());

    InlineInitializationProcessing("CSystem::OpenBasicPaks OpenPacks( paksFolder.c_str() )");

    //////////////////////////////////////////////////////////////////////////
    // Open engine packs
    //////////////////////////////////////////////////////////////////////////

    // After game paks to have same search order as with files on disk
    m_env.pCryPak->OpenPack("@assets@", "Engine.pak");
    m_env.pCryPak->OpenPack("@assets@", "ShaderCache.pak");
    m_env.pCryPak->OpenPack("@assets@", "ShaderCacheStartup.pak");
    m_env.pCryPak->OpenPack("@assets@", "Shaders.pak");
    m_env.pCryPak->OpenPack("@assets@", "ShadersBin.pak");

#ifdef AZ_PLATFORM_ANDROID
    // Load Android Obb files if available
    const char* obbStorage = AZ::Android::Utils::GetObbStoragePath();
    AZStd::string mainObbPath = AZStd::move(AZStd::string::format("%s/%s", obbStorage, AZ::Android::Utils::GetObbFileName(true).c_str()));
    AZStd::string patchObbPath = AZStd::move(AZStd::string::format("%s/%s", obbStorage, AZ::Android::Utils::GetObbFileName(false).c_str()));
    m_env.pCryPak->OpenPack("@assets@", mainObbPath.c_str());
    m_env.pCryPak->OpenPack("@assets@", patchObbPath.c_str());
#endif //AZ_PLATFORM_ANDROID

    InlineInitializationProcessing("CSystem::OpenBasicPaks OpenPacks( Engine... )");

    //////////////////////////////////////////////////////////////////////////
    // Open paks in MOD subfolders.
    //////////////////////////////////////////////////////////////////////////
#if !defined(_RELEASE)
    if (const ICmdLineArg* pModArg = GetICmdLine()->FindArg(eCLAT_Pre, "MOD"))
    {
        if (IsMODValid(pModArg->GetValue()))
        {
            string modFolder = "Mods\\";
            modFolder += pModArg->GetValue();
            modFolder += "\\*.pak";
            GetIPak()->OpenPacks("@assets@", modFolder.c_str(), ICryPak::FLAGS_PATH_REAL | ICryArchive::FLAGS_OVERRIDE_PAK);
        }
    }
#endif // !defined(_RELEASE)

    // Load paks required for game init to mem
    gEnv->pCryPak->LoadPakToMemory("Engine.pak", ICryPak::eInMemoryPakLocale_GPU);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenLanguagePak(const char* sLanguage)
{
    // Don't attempt to open a language PAK file if the game doesn't have a
    // loc folder configured.
    if (!GetLocalizationManager()->ProjectUsesLocalization())
    {
        return;
    }

    // Initialize languages.

    // Omit the trailing slash!
    string sLocalizationFolder = PathUtil::GetLocalizationFolder();

    // load xml pak with full filenames to perform wildcard searches.
    string sLocalizedPath;
    GetLocalizedPath(sLanguage, sLocalizedPath);
    if (!m_env.pCryPak->OpenPacks(sLocalizationFolder.c_str(), sLocalizedPath, 0))
    {
        // make sure the localized language is found - not really necessary, for TC
        CryLogAlways("Localized language content(%s) not available or modified from the original installation.", sLanguage);
    }

    //Debugging code for profiling memory usage of pak system
    /*ICryPak::PakInfo* pPakInfo = m_env.pCryPak->GetPakInfo();
    size_t openPakSize = 0;
    for( uint32 pak = 0; pak < pPakInfo->numOpenPaks; pak++ )
    {
        openPakSize += pPakInfo->arrPaks[pak].nUsedMem;
    }
    m_env.pCryPak->FreePakInfo(pPakInfo);

    CryLogAlways("Total pak size after loading localization is %d", openPakSize);*/
}


//////////////////////////////////////////////////////////////////////////
void CSystem::OpenLanguageAudioPak(const char* sLanguage)
{
    // Don't attempt to open a language PAK file if the game doesn't have a
    // loc folder configured.
    if (!GetLocalizationManager()->ProjectUsesLocalization())
    {
        return;
    }

    // Initialize languages.

    int nPakFlags = 0;

    // Omit the trailing slash!
    string sLocalizationFolder(string().assign(PathUtil::GetLocalizationFolder(), 0, PathUtil::GetLocalizationFolder().size() - 1));

    if (sLocalizationFolder.compareNoCase("Languages") == 0)
    {
        sLocalizationFolder = "@assets@";
    }

    // load localized pak with crc32 filenames on consoles to save memory.
    string sLocalizedPath;
    GetLocalizedAudioPath(sLanguage, sLocalizedPath);
    if (!m_env.pCryPak->OpenPacks(sLocalizationFolder.c_str(), sLocalizedPath, nPakFlags))
    {
        // make sure the localized language is found - not really necessary, for TC
        CryLogAlways("Localized language content(%s) not available or modified from the original installation.", sLanguage);
    }

    //Debugging code for profiling memory usage of pak system
    /*ICryPak::PakInfo* pPakInfo = m_env.pCryPak->GetPakInfo();
    size_t openPakSize = 0;
    for( uint32 pak = 0; pak < pPakInfo->numOpenPaks; pak++ )
    {
        openPakSize += pPakInfo->arrPaks[pak].nUsedMem;
    }
    m_env.pCryPak->FreePakInfo(pPakInfo);

    CryLogAlways("Total pak size after loading localization is %d", openPakSize);*/
}


string GetUniqueLogFileName(string logFileName)
{
    int instance = gEnv->pSystem->GetApplicationInstance();

    string logFileNamePrefix = logFileName;
    if ((logFileNamePrefix[0] != '@') && (AzFramework::StringFunc::Path::IsRelative(logFileNamePrefix)))
    {
        logFileNamePrefix = "@log@/";
        logFileNamePrefix += logFileName;
    }

    if (instance == 0)
    {
        return logFileNamePrefix;
    }

    string logFileExtension;
    size_t extensionIndex = logFileName.find_last_of('.');
    if (extensionIndex != string::npos)
    {
        logFileExtension = logFileName.substr(extensionIndex, logFileName.length() - extensionIndex);
        logFileNamePrefix = logFileName.substr(0, extensionIndex);
    }

    logFileName.Format("%s(%d)%s", logFileNamePrefix.c_str(), instance, logFileExtension.c_str());

    return logFileName;
}



void OnLevelLoadingDump(ICVar* pArgs)
{
    gEnv->pSystem->OutputLoadingTimeStats();
}

#if defined(WIN32) || defined(WIN64)
static wstring GetErrorStringUnsupportedCPU()
{
    static const wchar_t s_EN[] = L"Unsupported CPU detected. CPU needs to support SSE, SSE2 and SSE3.";
    static const wchar_t s_FR[] = { 0 };
    static const wchar_t s_RU[] = { 0 };
    static const wchar_t s_ES[] = { 0 };
    static const wchar_t s_DE[] = { 0 };
    static const wchar_t s_IT[] = { 0 };

    const size_t fullLangID = (size_t) GetKeyboardLayout(0);
    const size_t primLangID = fullLangID & 0x3FF;
    const wchar_t* pFmt = s_EN;

    /*switch (primLangID)
    {
    case 0x07: // German
        pFmt = s_DE;
        break;
    case 0x0a: // Spanish
        pFmt = s_ES;
        break;
    case 0x0c: // French
        pFmt = s_FR;
        break;
    case 0x10: // Italian
        pFmt = s_IT;
        break;
    case 0x19: // Russian
        pFmt = s_RU;
        break;
    case 0x09: // English
    default:
        break;
    }*/
    wchar_t msg[1024];
    msg[0] = L'\0';
    msg[sizeof(msg) / sizeof(msg[0]) - 1] = L'\0';
    _snwprintf(msg, sizeof(msg) / sizeof(msg[0]) - 1, pFmt);
    return msg;
}
#endif

static bool CheckCPURequirements(CCpuFeatures* pCpu, CSystem* pSystem)
{
#if !defined(DEDICATED_SERVER)
#if defined(WIN32) || defined(WIN64)
    if (!gEnv->IsEditor() && !gEnv->IsDedicated())
    {
        if (!(pCpu->hasSSE() && pCpu->hasSSE2() && pCpu->hasSSE3()))
        {
            CryLogAlways("Unsupported CPU! Need SSE, SSE2 and SSE3 instructions to be available.");

#if !defined(_RELEASE)
            const bool allowPrompts = pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "noprompt") == 0;
#else
            const bool allowPrompts = true;
#endif // !defined(_RELEASE)
            if (allowPrompts)
            {
                CryLogAlways("Asking user if they wish to continue...");
                const int mbRes = MessageBoxW(0, GetErrorStringUnsupportedCPU().c_str(), L"Lumberyard", MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2 | MB_DEFAULT_DESKTOP_ONLY);
                if (mbRes == IDCANCEL)
                {
                    CryLogAlways("User chose to cancel.");
                    return false;
                }
            }
            else
            {
#if !defined(_RELEASE)
                const bool obeyCPUCheck = pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "anycpu") == 0;
#else
                const bool obeyCPUCheck = true;
#endif // !defined(_RELEASE)
                if (obeyCPUCheck)
                {
                    CryLogAlways("No prompts allowed and unsupported CPU check active. Treating unsupported CPU as error and exiting.");
                    return false;
                }
            }

            CryLogAlways("User chose to continue despite unsupported CPU!");
        }
    }
#endif
#endif // !defined(DEDICATED_SERVER)
    return true;
}

// System initialization
/////////////////////////////////////////////////////////////////////////////////
// INIT
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::Init(const SSystemInitParams& startupParams)
{
    LOADING_TIME_PROFILE_SECTION;

    SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_INIT);
    gEnv->mMainThreadId = GetCurrentThreadId();         //Set this ASAP on startup

    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "System Initialization");

    InlineInitializationProcessing("CSystem::Init start");
    m_szCmdLine = startupParams.szSystemCmdLine;

    m_env.szCmdLine = m_szCmdLine.c_str();
    m_env.bTesting = startupParams.bTesting;
    m_env.bNoAssertDialog = startupParams.bTesting;
    m_env.bNoRandomSeed = startupParams.bNoRandom;
    m_bShaderCacheGenMode = startupParams.bShaderCacheGen;

    if (startupParams.bUnattendedMode)
    {
        m_bNoCrashDialog = true;
        m_env.bNoAssertDialog = true; //this also suppresses CryMessageBox
        g_cvars.sys_no_crash_dialog = true;
        AddPlatformOSCreateFlag(IPlatformOS::eCF_NoDialogs);
    }
    
        AZCoreLogSink::Connect();

    m_assetPlatform = startupParams.assetsPlatform;

    // compute system config name
    if (m_assetPlatform.empty())
    {
        AZ_Warning("CrySystem", false, "You must supply a valid asset platform in bootstrap.cfg");
        return false;
    }

    m_systemConfigName = "system_" CURRENT_PLATFORM_NAME "_";
    m_systemConfigName += m_assetPlatform;
    m_systemConfigName += ".cfg";

    assert(CryMemory::IsHeapValid());

#ifdef EXTENSION_SYSTEM_INCLUDE_TESTCASES
    TestExtensions(&CCryFactoryRegistryImpl::Access());
#endif

    //_controlfp(0, _EM_INVALID|_EM_ZERODIVIDE | _PC_64 );

#if defined(WIN32) || defined(WIN64)
    // check OS version - we only want to run on XP or higher - talk to Martin Mittring if you want to change this
    {
        OSVERSIONINFO osvi;

        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        GetVersionExA(&osvi);

        bool bIsWindowsXPorLater = osvi.dwMajorVersion > 5 || (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion >= 1);

        if (!bIsWindowsXPorLater)
        {
            CryFatalError("Windows XP or later is required");
            return false;
        }
    }
#endif

    m_pResourceManager->Init();

    // Get file version information.
    QueryVersionInfo();
    DetectGameFolderAccessRights();

    m_hInst = (WIN_HINSTANCE)startupParams.hInstance;
    m_hWnd = (WIN_HWND)startupParams.hWnd;

    m_userRootDir = startupParams.userPath;
    m_logsDir = startupParams.logPath;
    m_cacheDir = startupParams.cachePath;

    m_binariesDir = startupParams.szBinariesDir;
    m_bEditor = startupParams.bEditor;
    m_bPreviewMode = startupParams.bPreview;
    m_bTestMode = startupParams.bTestMode;
    m_pUserCallback = startupParams.pUserCallback;
    m_bMinimal = startupParams.bMinimal;

#if defined(CVARS_WHITELIST)
    m_pCVarsWhitelist = startupParams.pCVarsWhitelist;
#endif // defined(CVARS_WHITELIST)
    m_bDedicatedServer = startupParams.bDedicatedServer;
    m_pCmdLine = new CCmdLine(startupParams.szSystemCmdLine);
    m_currentLanguageAudio = "";
#if defined(DEDICATED_SERVER)
    m_bNoCrashDialog = true;
#else
    m_bNoCrashDialog = false;
#endif

    memcpy(gEnv->pProtectedFunctions, startupParams.pProtectedFunctions, sizeof(startupParams.pProtectedFunctions));

#if !defined(CONSOLE)
    m_env.SetIsEditor(m_bEditor);
    m_env.SetIsEditorGameMode(false);
    m_env.SetIsEditorSimulationMode(false);
#endif

    m_env.SetToolMode(startupParams.bToolMode);
    m_env.bIsOutOfMemory = false;

    if (m_bEditor)
    {
        m_bInDevMode = true;
    }

#if !defined(DEDICATED_SERVER)
    const ICmdLineArg* crashdialog = m_pCmdLine->FindArg(eCLAT_Post, "sys_no_crash_dialog");
    if (crashdialog)
    {
        m_bNoCrashDialog = true;
    }
#endif

    if (!startupParams.pValidator)
    {
        m_pDefaultValidator = new SDefaultValidator(this);
        m_pValidator = m_pDefaultValidator;
    }
    else
    {
        m_pValidator = startupParams.pValidator;
    }

#if !defined(_RELEASE)
    if (!m_bDedicatedServer)
    {
        const ICmdLineArg* dedicated = m_pCmdLine->FindArg(eCLAT_Pre, "dedicated");
        if (dedicated)
        {
            m_bDedicatedServer = true;
#if !defined(CONSOLE)
            gEnv->SetIsDedicated(true);
#endif
        }
    }
#endif // !defined(_RELEASE)

#if defined(DEDICATED_SERVER)
    m_bDedicatedServer = true;
#if !defined(CONSOLE)
    gEnv->SetIsDedicated(true);
#endif
#endif // #if defined(DEDICATED_SERVER)

#if !defined(CONSOLE)
#if !defined(_RELEASE)
    bool isDaemonMode = (m_pCmdLine->FindArg(eCLAT_Pre, "daemon") != 0);
#else
    bool isDaemonMode = false;
#endif // !defined(_RELEASE)

#if defined(USE_DEDICATED_SERVER_CONSOLE)

#if !defined(_RELEASE)
    bool isSimpleConsole = (m_pCmdLine->FindArg(eCLAT_Pre, "simple_console") != 0);

    if (!(isDaemonMode || isSimpleConsole))
#endif // !defined(_RELEASE)
    {
#if defined(USE_UNIXCONSOLE)
        CUNIXConsole* pConsole = new CUNIXConsole();
#if defined(LINUX)
        pUnixConsole = pConsole;
#endif
#elif defined(USE_IOSCONSOLE)
        CIOSConsole* pConsole = new CIOSConsole();
#elif defined(USE_WINDOWSCONSOLE)
        CWindowsConsole* pConsole = new CWindowsConsole();
#elif defined(USE_ANDROIDCONSOLE)
        CAndroidConsole* pConsole = new CAndroidConsole();
#else
        CNULLConsole* pConsole = new CNULLConsole(false);
#endif
        m_pTextModeConsole = static_cast<ITextModeConsole*>(pConsole);

        if (m_pUserCallback == NULL && m_bDedicatedServer)
        {
            char headerString[128];
            m_pUserCallback = pConsole;
            pConsole->SetRequireDedicatedServer(true);

            strcpy(
                headerString,
                "Lumberyard - "
#if defined(LINUX)
                "Linux "
#elif defined(MAC)
                "MAC "
#elif defined(IOS)
                "iOS "
#elif defined(APPLETV)
                "AppleTV "
#endif
                "Dedicated Server"
                " - Version ");

            char* str = headerString + strlen(headerString);
            GetProductVersion().ToString(str, sizeof(headerString) - (str - headerString));
            pConsole->SetHeader(headerString);
        }
    }
#if !defined(_RELEASE)
    else
#endif
#endif

#if !(defined(USE_DEDICATED_SERVER_CONSOLE) && defined(_RELEASE))
    {
        CNULLConsole* pConsole = new CNULLConsole(isDaemonMode);
        m_pTextModeConsole = pConsole;

        if (m_pUserCallback == NULL && m_bDedicatedServer)
        {
            m_pUserCallback = pConsole;
        }
    }
#endif

#endif // !defined(CONSOLE)

    //////////////////////////////////////////////////////////////////////////
    // Create PlatformOS
    //////////////////////////////////////////////////////////////////////////
    // moved before 'InitFileSystem' for streaming install initialization:
    //  10/06/2013 by Andriy
    m_pPlatformOS.reset(IPlatformOS::Create(m_PlatformOSCreateFlags));
    InlineInitializationProcessing("CSystem::Init PlatformOS");

    {
        EBUS_EVENT(CrySystemEventBus, OnCrySystemPreInitialize, *this, startupParams);

        //////////////////////////////////////////////////////////////////////////
        // File system, must be very early
        //////////////////////////////////////////////////////////////////////////
        if (!InitFileSystem(startupParams))
        {
            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        InlineInitializationProcessing("CSystem::Init InitFileSystem");

#if defined(ENABLE_LOADING_PROFILER)
        CLoadingProfilerSystem::Init();
#endif

        //////////////////////////////////////////////////////////////////////////
        // Logging is only available after file system initialization.
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.pLog)
        {
            m_env.pLog = new CLog(this);
            if (startupParams.pLogCallback)
            {
                m_env.pLog->AddCallback(startupParams.pLogCallback);
            }

            const ICmdLineArg* logfile = m_pCmdLine->FindArg(eCLAT_Pre, "logfile"); //see if the user specified a log name, if so use it
            if (logfile && strlen(logfile->GetValue()) > 0)
            {
                m_env.pLog->SetFileName(logfile->GetValue(), startupParams.autoBackupLogs);
            }
            else if (startupParams.sLogFileName)    //otherwise see if the startup params has a log file name, if so use it
            {
                const string sUniqueLogFileName = GetUniqueLogFileName(startupParams.sLogFileName);
                m_env.pLog->SetFileName(sUniqueLogFileName.c_str(), startupParams.autoBackupLogs);
            }
            else//use the default log name
            {
                m_env.pLog->SetFileName(DEFAULT_LOG_FILENAME, startupParams.autoBackupLogs);
            }
        }
        else
        {
            m_env.pLog = startupParams.pLog;
        }

        //here we should be good to ask Crypak to do something

        //#define GEN_PAK_CDR_CRC
#ifdef GEN_PAK_CDR_CRC

        const char* filename = m_pCmdLine->GetArg(1)->GetName();
        gEnv->pCryPak->OpenPack(filename);

        int crc = gEnv->pCryPak->ComputeCachedPakCDR_CRC(filename, false);

        exit(crc);

#endif
        // Initialise after pLog and CPU feature initialization
        // AND after console creation (Editor only)
        // May need access to engine folder .pak files
        gEnv->pThreadManager->GetThreadConfigManager()->LoadConfig("config/engine_core.thread_config");

        if (m_bEditor)
        {
            gEnv->pThreadManager->GetThreadConfigManager()->LoadConfig("config/engine_sandbox.thread_config");
        }

        // Setup main thread
        void* pThreadHandle = 0; // Let system figure out thread handle
        gEnv->pThreadManager->RegisterThirdPartyThread(pThreadHandle, "Main");
        m_env.pProfileLogSystem = new CProfileLogSystem();

#ifdef CODECHECKPOINT_ENABLED
        // Setup code checkpoint manager if checkpoints are enabled
        m_env.pCodeCheckpointMgr = new CCodeCheckpointMgr;
#else
        m_env.pCodeCheckpointMgr = NULL;
#endif

        bool devModeEnable = true;

#if defined(_RELEASE)
        // disable devmode by default in release builds outside the editor
        devModeEnable = m_bEditor;
#endif

        // disable devmode in launcher if someone really wants to (even in non release builds)
        if (!m_bEditor && m_pCmdLine->FindArg(eCLAT_Pre, "nodevmode"))
        {
            devModeEnable = false;
        }

        SetDevMode(devModeEnable);

        //////////////////////////////////////////////////////////////////////////
        // CREATE NOTIFICATION NETWORK
        //////////////////////////////////////////////////////////////////////////
        m_pNotificationNetwork = NULL;
#ifndef _RELEASE
    #ifndef LINUX

        if (!startupParams.bMinimal)
        {
            m_pNotificationNetwork = CNotificationNetwork::Create();
        }
    #endif//LINUX
#endif // _RELEASE

        InlineInitializationProcessing("CSystem::Init NotificationNetwork");

        //////////////////////////////////////////////////////////////////////////
        // CREATE CONSOLE
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipConsole)
        {
            m_env.pConsole = new CXConsole;
            ((CTestSystemLegacy*)m_pTestSystem)->Init(m_env.pConsole);

            if (startupParams.pPrintSync)
            {
                m_env.pConsole->AddOutputPrintSink(startupParams.pPrintSync);
            }
        }

        //////////////////////////////////////////////////////////////////////////

        if (m_pUserCallback)
        {
            m_pUserCallback->OnInit(this);
        }

        m_env.pLog->RegisterConsoleVariables();

        GetIRemoteConsole()->RegisterConsoleVariables();

#if defined(ENABLE_PROFILING_CODE)
        CStroboscope::GetInst()->RegisterCommands();
#endif

#ifdef ENABLE_LOADING_PROFILER
        CBootProfiler::GetInstance().RegisterCVars();
#endif

        if (!startupParams.bSkipConsole)
        {
            // Register system console variables.
            CreateSystemVars();

            // Register Audio-related system CVars
            CreateAudioVars();

            // Callback
            if (m_pUserCallback && m_env.pConsole)
            {
                m_pUserCallback->OnConsoleCreated(m_env.pConsole);
            }

            // Let listeners know its safe to register cvars
            EBUS_EVENT(CrySystemEventBus, OnCrySystemCVarRegistry);
        }


        // Set this as soon as the system cvars got initialized.
        static_cast<CCryPak* const>(m_env.pCryPak)->SetLocalizationFolder(g_cvars.sys_localization_folder->GetString());

        ((CCryPak*)m_env.pCryPak)->SetLog(m_env.pLog);

        InlineInitializationProcessing("CSystem::Init Create console");

        if (!startupParams.bSkipRenderer)
        {
            m_FrameProfileSystem.Init(this, m_sys_profile_allThreads->GetIVal());
            CreateRendererVars(startupParams);
        }

        InitFileSystem_LoadEngineFolders(startupParams);

        LogVersion();

        // CPU features detection.
        m_pCpu = new CCpuFeatures;
        m_pCpu->Detect();
        m_env.pi.numCoresAvailableToProcess = m_pCpu->GetCPUCount();
        m_env.pi.numLogicalProcessors = m_pCpu->GetLogicalCPUCount();

        // Check hard minimum CPU requirements
        if (!CheckCPURequirements(m_pCpu, this))
        {
            return false;
        }

        if (!startupParams.bSkipConsole)
        {
            LogSystemInfo();
        }

        InlineInitializationProcessing("CSystem::Init Load Engine Folders");

        //////////////////////////////////////////////////////////////////////////
        //Load config files
        //////////////////////////////////////////////////////////////////////////

        int curSpecVal = 0;
        ICVar* pSysSpecCVar = gEnv->pConsole->GetCVar("sys_spec");
        if (gEnv->pSystem->IsDevMode())
        {
            if (pSysSpecCVar && pSysSpecCVar->GetFlags() & VF_WASINCONFIG)
            {
                curSpecVal = pSysSpecCVar->GetIVal();
                pSysSpecCVar->SetFlags(pSysSpecCVar->GetFlags() | VF_SYSSPEC_OVERWRITE);
            }
        }

        // tools may not interact with @user@
        if (!gEnv->IsInToolMode())
        {
            if (m_pCmdLine->FindArg(eCLAT_Pre, "ResetProfile") == 0)
            {
                LoadConfiguration("@user@/game.cfg", 0, false);
            }
        }

        // If sys spec variable was specified, is not 0, and we are in devmode restore the value from before loading game.cfg
        // This enables setting of a specific sys_spec outside menu and game.cfg
        if (gEnv->pSystem->IsDevMode())
        {
            if (pSysSpecCVar && curSpecVal && curSpecVal != pSysSpecCVar->GetIVal())
            {
                pSysSpecCVar->Set(curSpecVal);
            }
        }

        {
            ILoadConfigurationEntrySink* pCVarsWhiteListConfigSink = GetCVarsWhiteListConfigSink();

            // We have to load this file again since first time we did it without devmode
            LoadConfiguration(m_systemConfigName.c_str(), pCVarsWhiteListConfigSink);
            // Optional user defined overrides
            LoadConfiguration("user.cfg", pCVarsWhiteListConfigSink);

            if (!startupParams.bSkipRenderer)
            {
                // Load the hmd.cfg if it exists. This will enable optional stereo rendering.
                LoadConfiguration("hmd.cfg");
            }

            if (startupParams.bShaderCacheGen)
            {
                LoadConfiguration("shadercachegen.cfg", pCVarsWhiteListConfigSink);
            }

#if defined(ENABLE_STATS_AGENT)
            if (m_pCmdLine->FindArg(eCLAT_Pre, "useamblecfg"))
            {
                LoadConfiguration("amble.cfg", pCVarsWhiteListConfigSink);
            }
#endif
        }

#if   defined(AZ_PLATFORM_ANDROID)
        LoadConfiguration("android.cfg");
#elif defined(AZ_PLATFORM_APPLE_IOS)
        LoadConfiguration("ios.cfg");
#elif defined(AZ_PLATFORM_APPLE_TV)
        LoadConfiguration("appletv.cfg");
#elif defined(AZ_PLATFORM_APPLE_OSX)
        const char* driverName = GetRenderingDriverName();
        if (driverName && strcmp(driverName, "GL") == 0)
        {
            LoadConfiguration("osx_gl.cfg");
        }
        else if (driverName && strcmp(driverName, "METAL") == 0)
        {
            LoadConfiguration("osx_metal.cfg");
        }
#endif

#if defined(PERFORMANCE_BUILD)
        LoadConfiguration("performance.cfg");
#endif

        //////////////////////////////////////////////////////////////////////////
        if (g_cvars.sys_asserts == 0)
        {
            gEnv->bIgnoreAllAsserts = true;
        }
        if (g_cvars.sys_asserts == 2)
        {
            gEnv->bNoAssertDialog = true;
        }

        //////////////////////////////////////////////////////////////////////////
        // CREATE JOBMANAGER
        //////////////////////////////////////////////////////////////////////////
        m_env.pJobManager->Init(m_sys_job_system_max_worker ? m_sys_job_system_max_worker->GetIVal() : 0);

        //////////////////////////////////////////////////////////////////////////
        // Stream Engine
        //////////////////////////////////////////////////////////////////////////
        CryLogAlways("Stream Engine Initialization");
        InitStreamEngine();
        InlineInitializationProcessing("CSystem::Init StreamEngine");

        //  if (!g_sysSpecChanged)
        //      OnSysSpecChange( m_sys_spec );

        {
            if (m_pCmdLine->FindArg(eCLAT_Pre, "DX11"))
            {
                m_env.pConsole->LoadConfigVar("r_Driver", "DX11");
            }
            else if (m_pCmdLine->FindArg(eCLAT_Pre, "GL"))
            {
                m_env.pConsole->LoadConfigVar("r_Driver", "GL");
            }
        }

        LogBuildInfo();

        InlineInitializationProcessing("CSystem::Init LoadConfigurations");

#if ENABLE_STATOSCOPE
        if (!m_env.pStatoscope)
        {
            m_env.pStatoscope = new CStatoscope();
        }
#else
        m_env.pStatoscope = NULL;
#endif

        m_env.pOverloadSceneManager = new COverloadSceneManager;

        if (m_bDedicatedServer && m_rDriver)
        {
            m_sSavedRDriver = m_rDriver->GetString();
            m_rDriver->Set("NULL");
        }

#if defined(WIN32) || defined(WIN64)
        if (!startupParams.bSkipRenderer)
        {
            if (_stricmp(m_rDriver->GetString(), "Auto") == 0)
            {
                m_rDriver->Set("DX11");
            }
        }

        if (gEnv->IsEditor() && _stricmp(m_rDriver->GetString(), "DX12") == 0)
        {
            AZ_Warning("System", false, "DX12 mode is not supported in the editor. Reverting to DX11 mode.");
            m_rDriver->Set("DX11");
        }
#endif

#ifdef WIN32
        if ((g_cvars.sys_WER) && (!startupParams.bMinimal))
        {
            SetUnhandledExceptionFilter(CryEngineExceptionFilterWER);
        }

#endif

        //////////////////////////////////////////////////////////////////////////
        // PHYSICS
        //////////////////////////////////////////////////////////////////////////
        //if (!params.bPreview)
        if (!startupParams.bShaderCacheGen && !startupParams.bSkipPhysics)
        {
            CryLogAlways("Physics initialization");
            if (!InitPhysics(startupParams))
            {
                return false;
            }
        }

        InlineInitializationProcessing("CSystem::Init InitPhysics");

        //////////////////////////////////////////////////////////////////////////
        // Localization
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bMinimal)
        {
            InitLocalization();
        }
        InlineInitializationProcessing("CSystem::Init InitLocalizations");

        //////////////////////////////////////////////////////////////////////////
        // RENDERER
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipRenderer)
        {
            assert(CryMemory::IsHeapValid());
            CryLogAlways("Renderer initialization");
            if (!InitRenderer(m_hInst, (startupParams.bEditor) ? (WIN_HWND)1 : m_hWnd, startupParams))
            {
                return false;
            }
            assert(CryMemory::IsHeapValid());
            if (m_env.pRenderer)
            {
                bool bMultiGPUEnabled = false;
                m_env.pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPUEnabled);
                if (bMultiGPUEnabled)
                {
                    LoadConfiguration("mgpu.cfg");
                }
            }
        }

        InlineInitializationProcessing("CSystem::Init InitRenderer");

        if (m_env.pCryFont)
        {
            m_env.pCryFont->SetRendererProperties(m_env.pRenderer);
        }

        InlineInitializationProcessing("CSystem::Init m_pResourceManager->UnloadFastLoadPaks");

        assert(m_env.pRenderer || startupParams.bSkipRenderer);

        if (g_cvars.sys_rendersplashscreen && !startupParams.bEditor && !startupParams.bShaderCacheGen)
        {
            if (m_env.pRenderer)
            {
                LOADING_TIME_PROFILE_SECTION_NAMED("Rendering Splash Screen");
                ITexture* pTex = m_env.pRenderer->EF_LoadTexture(g_cvars.sys_splashscreen, FT_DONT_STREAM | FT_NOMIPS | FT_USAGE_ALLOWREADSRGB);
                //check the width and height as extra verification hack. This texture is loaded before the replace me, so there is
                //no backup if it fails to load.
                if (pTex && pTex->GetWidth() && pTex->GetHeight())
                {
                    const int splashWidth = pTex->GetWidth();
                    const int splashHeight = pTex->GetHeight();

                    const int screenWidth = m_env.pRenderer->GetOverlayWidth();
                    const int screenHeight = m_env.pRenderer->GetOverlayHeight();

                    const float scaleX = (float)screenWidth / (float)splashWidth;
                    const float scaleY = (float)screenHeight / (float)splashHeight;

                    float scale = 1.0f;
                    switch (g_cvars.sys_splashScreenScaleMode)
                    {
                    case SSystemCVars::SplashScreenScaleMode_Fit:
                    {
                        scale = AZStd::GetMin(scaleX, scaleY);
                    }
                    break;
                    case SSystemCVars::SplashScreenScaleMode_Fill:
                    {
                        scale = AZStd::GetMax(scaleX, scaleY);
                    }
                    break;
                    }

                    const float w = splashWidth * scale;
                    const float h = splashHeight * scale;
                    const float x = (screenWidth - w) * 0.5f;
                    const float y = (screenHeight - h) * 0.5f;

                    const float vx = (800.0f / (float) screenWidth);
                    const float vy = (600.0f / (float) screenHeight);

                    m_env.pRenderer->SetViewport(0, 0, screenWidth, screenHeight);

                    // make sure it's rendered in full screen mode when triple buffering is enabled as well
                    for (size_t n = 0; n < 3; n++)
                    {
                        m_env.pRenderer->BeginFrame();
                        m_env.pRenderer->SetCullMode(R_CULL_NONE);
                        m_env.pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
                        m_env.pRenderer->Draw2dImageStretchMode(true);
                        m_env.pRenderer->Draw2dImage(x * vx, y * vy, w * vx, h * vy, pTex->GetTextureID(), 0.0f, 1.0f, 1.0f, 0.0f);
                        m_env.pRenderer->Draw2dImageStretchMode(false);
                        m_env.pRenderer->EndFrame();
                    }

                    pTex->Release();
                }

        #if defined(AZ_PLATFORM_ANDROID)
                // passing true because the engine renderer is now taking over with it's own
                // splash screen so we need to tell the java code to clean up it's version
                OnEngineRendererTakeover(true);
        #endif
            }
            else
            {
                CryWarning(EValidatorModule::VALIDATOR_MODULE_SYSTEM, EValidatorSeverity::VALIDATOR_WARNING, "Failed to load startscreen.tif");
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // Open basic pak files after intro movie playback started
        //////////////////////////////////////////////////////////////////////////
        OpenBasicPaks();

        //////////////////////////////////////////////////////////////////////////
        // AUDIO
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bMinimal)
        {
            if (!InitAudioSystem(startupParams))
            {
                // Failure to initialize audio system is no longer a fatal error, warn here...
                CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "<Audio>: Running without any AudioSystem!");
            }
            else
            {
                // Pump the Log - audio initialization happened on a non-main thread, there may be log messages queued up.
                gEnv->pLog->Update();
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // FONT
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipFont)
        {
            CryLogAlways("Font initialization");
            if (!InitFont(startupParams))
            {
                return false;
            }
        }

        InlineInitializationProcessing("CSystem::Init InitFonts");

        // The last update to the loading screen message was 'Initializing CryFont...'
        // Compiling the default system textures can be the lengthiest portion of
        // editor initialization, so it is useful to inform users that they are waiting on
        // the necessary default textures to compile, and that they are not waiting on CryFont.
        if (m_pUserCallback)
        {
            m_pUserCallback->OnInitProgress("First time asset processing - may take a minute...");
        }

        // may as well make sure everything in the texturemsg folder is precompiled now.
        EBUS_EVENT(AzFramework::AssetSystemRequestBus, GetAssetStatus, "/texturemsg/");

        //////////////////////////////////////////////////////////////////////////
        // POST RENDERER
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipRenderer && m_env.pRenderer)
        {
            m_env.pRenderer->PostInit();

            if (!startupParams.bShaderCacheGen)
            {
                // try to do a flush to keep the renderer busy during loading
                m_env.pRenderer->TryFlush();
            }
        }
        InlineInitializationProcessing("CSystem::Init Renderer::PostInit");

        //////////////////////////////////////////////////////////////////////////
        // CREATE TELEMETRY SYSTEM
        //////////////////////////////////////////////////////////////////////////

        m_env.pTelemetrySystem = new Telemetry::CTelemetrySystem();

        CRY_ASSERT(m_env.pTelemetrySystem);

        if (!m_env.pTelemetrySystem->Init())
        {
            CryFatalError("Failed to initialize telemetry system!");

            return false;
        }

        m_pTelemetryFileStream = new Telemetry::CFileStream();

        CRY_ASSERT(m_pTelemetryFileStream);

        m_pTelemetryUDPStream = new Telemetry::CUDPStream();

        CRY_ASSERT(m_pTelemetryUDPStream);

        TelemetryStreamFileChanged(gEnv->pConsole->GetCVar("sys_telemetry_stream_file"));

        TelemetryStreamIPChanged(gEnv->pConsole->GetCVar("sys_telemetry_stream_ip"));


#ifdef SOFTCODE_SYSTEM_ENABLED
        m_env.pSoftCodeMgr = new SoftCodeMgr();
#else
        m_env.pSoftCodeMgr = NULL;
#endif

        //////////////////////////////////////////////////////////////////////////
        // MOVIE
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipMovie && !startupParams.bShaderCacheGen)
        {
            CryLogAlways("MovieSystem initialization");
            INDENT_LOG_DURING_SCOPE();
            if (!InitMovieSystem(startupParams))
            {
                return false;
            }
        }

        InlineInitializationProcessing("CSystem::Init InitMovie");

        //////////////////////////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////////////
        // Hardware mouse
        //////////////////////////////////////////////////////////////////////////
        // - Dedicated server is in console mode by default (Hardware Mouse is always shown when console is)
        // - Mouse is always visible by default in Editor (we never start directly in Game Mode)
        // - Mouse has to be enabled manually by the Game (this is typically done in the main menu)

        m_env.pHardwareMouse = NULL;

#if !defined(DEDICATED_SERVER)
        if (m_env.pRenderer)
        {
            if (!startupParams.bTesting)
            {
#if defined(AZ_FRAMEWORK_INPUT_ENABLED)
                m_env.pHardwareMouse = new HardwareMouse(false, "EngineAssets/Textures/Cursor_Green.tif");
#else
                m_env.pHardwareMouse = new CHardwareMouse(true);
#endif // defined(AZ_FRAMEWORK_INPUT_ENABLED)
            }
        }
#endif

        //////////////////////////////////////////////////////////////////////////
        // THREAD PROFILER
        //////////////////////////////////////////////////////////////////////////
        m_pThreadProfiler = new CThreadProfiler;

        //////////////////////////////////////////////////////////////////////////
        // DISK PROFILER
        //////////////////////////////////////////////////////////////////////////
#if defined(USE_DISK_PROFILER)
        m_pDiskProfiler = new CDiskProfiler(this);
#else
        m_pDiskProfiler = 0;
#endif

        // TIME
        //////////////////////////////////////////////////////////////////////////
        CryLogAlways("Time initialization");
        if (!m_Time.Init())
        {
            return (false);
        }
        m_Time.ResetTimer();

        //////////////////////////////////////////////////////////////////////////
        // INPUT
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bPreview && !gEnv->IsDedicated() && !startupParams.bShaderCacheGen)
        {
            CryLogAlways("Input initialization");
            INDENT_LOG_DURING_SCOPE();
            if (!InitInput(startupParams)) // !!! TODO: FIX ME !!!
            {
                return false;
            }

#if !defined(AZ_FRAMEWORK_INPUT_ENABLED)
            if (m_env.pHardwareMouse)
            {
                m_env.pHardwareMouse->OnPostInitInput();
            }
#endif // !defined(AZ_FRAMEWORK_INPUT_ENABLED)
        }

        InlineInitializationProcessing("CSystem::Init InitInput");

        //////////////////////////////////////////////////////////////////////////
        // UI. Should be after input and hardware mouse
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bShaderCacheGen && !m_bDedicatedServer)
        {
            CryLogAlways("UI system initialization");
            INDENT_LOG_DURING_SCOPE();
            if (!InitShine(startupParams))
            {
                return false;
            }
        }

        InlineInitializationProcessing("CSystem::Init InitShine");

        //////////////////////////////////////////////////////////////////////////
        // Create MiniGUI
        //////////////////////////////////////////////////////////////////////////
        minigui::IMiniGUIPtr pMiniGUI;
        if (CryCreateClassInstanceForInterface(cryiidof<minigui::IMiniGUI>(), pMiniGUI))
        {
            m_pMiniGUI = pMiniGUI.get();
            m_pMiniGUI->Init();
        }

        InlineInitializationProcessing("CSystem::Init InitMiniGUI");

        //////////////////////////////////////////////////////////////////////////
        // AI
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bPreview && !startupParams.bShaderCacheGen)
        {
            if (gEnv->IsDedicated() && m_svAISystem && !m_svAISystem->GetIVal())
            {
                ;
            }
            else
            {
                CryLogAlways("AI initialization");
                INDENT_LOG_DURING_SCOPE();

                if (!InitAISystem(startupParams))
                {
                    return false;
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // CONSOLE
        //////////////////////////////////////////////////////////////////////////
        if (!InitConsole())
        {
            return false;
        }

        //////////////////////////////////////////////////////////////////////////
        // Init Animation system
        //////////////////////////////////////////////////////////////////////////
        {
            CryLogAlways("Initializing Animation System");
            INDENT_LOG_DURING_SCOPE();
            if (!startupParams.bSkipAnimation && !startupParams.bShaderCacheGen)
            {
                if (!InitAnimationSystem(startupParams))
                {
                    return false;
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // Init 3d engine
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipRenderer && !startupParams.bShaderCacheGen)
        {
            CryLogAlways("Initializing 3D Engine");
            INDENT_LOG_DURING_SCOPE();

            if (!Init3DEngine(startupParams))
            {
                return false;
            }

            // try flush to keep renderer busy
            if (m_env.pRenderer)
            {
                m_env.pRenderer->TryFlush();
            }
        }

        InlineInitializationProcessing("CSystem::Init Init3DEngine");
        if (m_env.pCharacterManager)
        {
            m_env.pCharacterManager->PostInit();
        }

#ifdef DOWNLOAD_MANAGER
        m_pDownloadManager = new CDownloadManager;
        m_pDownloadManager->Create(this);
#endif //DOWNLOAD_MANAGER

        //#ifndef MEM_STD
        //  REGISTER_COMMAND("MemStats",::DumpAllocs,"");
        //#endif

        //////////////////////////////////////////////////////////////////////////
        // Initialize Gems
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bPreview && !startupParams.bShaderCacheGen && !startupParams.bMinimal)
        {
            if (m_pUserCallback)
            {
                m_pUserCallback->OnInitProgress("Initializing Gems...");
            }

            CryLogAlways("Initializing Gems System");
            INDENT_LOG_DURING_SCOPE();

            if (!InitGems(startupParams))
            {
                //return false;
            }
        }

        InlineInitializationProcessing("CSystem::Init InitGems");

        //////////////////////////////////////////////////////////////////////////
        // SCRIPT SYSTEM
        //////////////////////////////////////////////////////////////////////////
        CryLogAlways("Script System Initialization");
        INDENT_LOG_DURING_SCOPE();

        if (!startupParams.bSkipScriptSystem && !InitScriptSystem(startupParams))
        {
            return false;
        }

        InlineInitializationProcessing("CSystem::Init InitScripts");
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // ENTITY SYSTEM
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bPreview && !startupParams.bShaderCacheGen)
        {
            CryLogAlways("Entity system initialization");
            INDENT_LOG_DURING_SCOPE();

            if (!InitEntitySystem(startupParams))
            {
                return false;
            }
        }

        InlineInitializationProcessing("CSystem::Init InitEntitySystem");
        //////////////////////////////////////////////////////////////////////////

        m_crypto = new Crypto();

        //////////////////////////////////////////////////////////////////////////
        // NETWORK
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipNetwork && !startupParams.bPreview && !startupParams.bShaderCacheGen)
        {
            CryLogAlways("Network initialization");
            INDENT_LOG_DURING_SCOPE();
            InitNetwork(startupParams);

            if (gEnv->IsDedicated())
            {
                m_pServerThrottle.reset(new CServerThrottle(this, m_pCpu->GetCPUCount()));
            }
        }
        InlineInitializationProcessing("CSystem::Init InitNetwork");

        //////////////////////////////////////////////////////////////////////////
        // SERVICE NETWORK
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipNetwork && !startupParams.bMinimal)
        {
            m_env.pServiceNetwork = new CServiceNetwork();
        }

        //////////////////////////////////////////////////////////////////////////
        // REMOTE COMMAND SYTSTEM
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipNetwork && !startupParams.bMinimal)
        {
            m_env.pRemoteCommandManager = new CRemoteCommandManager();
        }


        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // VR SYSTEM INITIALIZATION (relies on GEMs)
        //////////////////////////////////////////////////////////////////////////
        if ((!startupParams.bSkipRenderer) && (!startupParams.bMinimal))
        {
            if (m_pUserCallback)
            {
                m_pUserCallback->OnInitProgress("Initializing VR Systems...");
            }

            AZStd::vector<AZ::VR::HMDInitBus*> devices;
            AZ::VR::HMDInitRequestBus::EnumerateHandlers([&devices](AZ::VR::HMDInitBus* device)
                {
                    devices.emplace_back(device);
                    return true;
                });

            // Order the devices so that devices that only support one type of HMD are ordered first as we want
            // to use any device-specific drivers over more general ones.
            std::sort(devices.begin(), devices.end(), [](AZ::VR::HMDInitBus* left, AZ::VR::HMDInitBus* right)
                {
                    return left->GetInitPriority() > right->GetInitPriority();
                });

            //Start up a job to init the HMDs since they may take a while to start up
            AZ::JobContext* jobContext = nullptr;
            EBUS_EVENT_RESULT(jobContext, AZ::JobManagerBus, GetGlobalContext);
            AZ::Job* hmdJob = AZ::CreateJobFunction([devices]()
                    {
                        // Loop through the attached devices and attempt to initialize them. We'll use the first
                        // one that succeeds since we currently only support a single HMD.
                        for (AZ::VR::HMDInitBus* device : devices)
                        {
                            if (device->AttemptInit())
                            {
                                // At this point if any device connected to the HMDDeviceRequestBus then we are good to go for VR.
                                EBUS_EVENT(AZ::VR::HMDDeviceRequestBus, OutputHMDInfo);
                                EBUS_EVENT(AZ::VR::HMDDeviceRequestBus, EnableDebugging, false);

                                // Since this was a job and we may have beaten the level's output_to_hmd cvar initialization
                                // We just want to retrigger the callback to this cvar
                                ICVar* outputToHMD = gEnv->pConsole->GetCVar("output_to_hmd");
                                if (outputToHMD != nullptr)
                                {
                                    int outputToHMDVal = gEnv->pConsole->GetCVar("output_to_hmd")->GetIVal();
                                    gEnv->pConsole->GetCVar("output_to_hmd")->Set(outputToHMDVal);
                                }
                                break;
                            }
                        }
                    }, true, jobContext);
            hmdJob->Start();
        }

        //////////////////////////////////////////////////////////////////////////
        // AI SYSTEM INITIALIZATION
        //////////////////////////////////////////////////////////////////////////
        // AI System needs to be initialized after entity system
        if (!startupParams.bPreview && m_env.pAISystem)
        {
            MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Initialize AI System");

            if (m_pUserCallback)
            {
                m_pUserCallback->OnInitProgress("Initializing AI System...");
            }
            CryLogAlways("Initializing AI System");
            INDENT_LOG_DURING_SCOPE();
            m_env.pAISystem->Init();
        }

        if (m_pUserCallback)
        {
            m_pUserCallback->OnInitProgress("Initializing additional systems...");
        }
        CryLogAlways("Initializing additional systems");


        InlineInitializationProcessing("CSystem::Init AIInit");

        //////////////////////////////////////////////////////////////////////////
        // DYNAMIC RESPONSE SYSTEM
        {
            CryLogAlways("Dynamic Response System initialization");
            INDENT_LOG_DURING_SCOPE();

            if (m_pUserCallback)
            {
                m_pUserCallback->OnInitProgress("Initializing Dynamic Response System...");
            }

            if (!InitDynamicResponseSystem(startupParams))
            {
                CryLogAlways("No Dynamic Response System was loaded from a module, will use the NULL DynamicResponseSystem.");
                m_env.pDynamicResponseSystem = new NULLDynamicResponse::CSystem();
                if (m_env.pDynamicResponseSystem == NULL)
                {
                    CryFatalError("<DRS>: Could not create an instance of NULLDynamicResponse::CSystem!");
                }
            }
        }

        InlineInitializationProcessing("CSystem::Init Dynamic Response System");

        //////////////////////////////////////////////////////////////////////////
        // BUDGETING SYSTEM
        m_pIBudgetingSystem = new CBudgetingSystem();

        InlineInitializationProcessing("CSystem::Init BudgetingSystem");

        //////////////////////////////////////////////////////////////////////////
        // Zlib compressor
        m_pIZLibCompressor = new CZLibCompressor();

        InlineInitializationProcessing("CSystem::Init ZLibCompressor");

        //////////////////////////////////////////////////////////////////////////
        // Zlib decompressor
        m_pIZLibDecompressor = new CZLibDecompressor();

        InlineInitializationProcessing("CSystem::Init ZLibDecompressor");

        //////////////////////////////////////////////////////////////////////////
        // Zlib decompressor
        m_pILZ4Decompressor = new CLZ4Decompressor();

        InlineInitializationProcessing("CSystem::Init LZ4Decompressor");

        //////////////////////////////////////////////////////////////////////////
        // Create PerfHUD
        //////////////////////////////////////////////////////////////////////////

#if defined(USE_PERFHUD)
        if (!gEnv->bTesting && !gEnv->IsInToolMode())
        {
            MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Init PerfHUD");
            //Create late in Init so that associated CVars have already been created
            ICryPerfHUDPtr pPerfHUD;
            if (CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD))
            {
                m_pPerfHUD = pPerfHUD.get();
                m_pPerfHUD->Init();
            }
        }
#endif


        //////////////////////////////////////////////////////////////////////////
        // Initialize task threads.
        //////////////////////////////////////////////////////////////////////////
        if (!startupParams.bSkipRenderer)
        {
            m_pThreadTaskManager->InitThreads();

            SetAffinity();
            assert(CryMemory::IsHeapValid());


            if (strstr(startupParams.szSystemCmdLine, "-VTUNE") != 0 || g_cvars.sys_vtune != 0)
            {
                InitVTuneProfiler();
            }


            RegisterEngineStatistics();
        }

        InlineInitializationProcessing("CSystem::Init InitTaskThreads");

        //////////////////////////////////////////////////////////////////////////
        // Input Post Initialise - enables input threads to be created after thread init
        //////////////////////////////////////////////////////////////////////////
        if (m_env.pInput)
        {
            m_env.pInput->PostInit();
        }

        if (m_env.pLyShine)
        {
            m_env.pLyShine->PostInit();
        }

        ///////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_SUPPORTS_AWS_NATIVE_SDK)
        if ((!startupParams.bMinimal) && (!startupParams.bShaderCacheGen))
        {
            CryLogAlways("LmbrAWS system initialization");
            INDENT_LOG_DURING_SCOPE();
            if (!InitLmbrAWS(startupParams))
            {
                return false;
            }
        }
#endif

        InlineInitializationProcessing("CSystem::Init InitLmbrAWS");

        // final tryflush to be sure that all framework init request have been processed
        if (!startupParams.bShaderCacheGen && m_env.pRenderer)
        {
            m_env.pRenderer->TryFlush();
        }

#if !defined(RELEASE)
        m_env.pLocalMemoryUsage = new CLocalMemoryUsage();
#else
        m_env.pLocalMemoryUsage = NULL;
#endif

#if defined(WIN32)
#   if !defined(WIN64)
        _controlfp(_PC_64, _MCW_PC); // not supported on WIN64
#   endif
#endif

        if (g_cvars.sys_float_exceptions > 0)
        {
            if (g_cvars.sys_float_exceptions == 3 && gEnv->IsEditor()) // Turn off float exceptions in editor if sys_float_exceptions = 3
            {
                g_cvars.sys_float_exceptions = 0;
            }
            if (g_cvars.sys_float_exceptions > 0)
            {
                CryLogAlways("enabled float exceptions(sys_float_exceptions %d) this makes the performance slower.", g_cvars.sys_float_exceptions);
            }
        }
        EnableFloatExceptions(g_cvars.sys_float_exceptions);

#if defined(WIN64) && defined(SECUROM_64)
        if (!m_bEditor && !IsDedicated())
        {
            int res = TestSecurom64();
            if (res != b64_ok)
            {
                _controlfp(0, _MCW_EM); // Enable floating point exceptions (Will eventually cause crash).
            }
        }
#endif

        MarkThisThreadForDebugging("Main");
    }

#if defined(ENABLE_LOADING_PROFILER)
    CLoadingProfilerSystem::SaveTimeContainersToFile("EngineStart.crylp", 0.0, true);
#endif

    InlineInitializationProcessing("CSystem::Init End");

#if defined(IS_PROSDK)
    SDKEvaluation::InitSDKEvaluation(gEnv, &m_pUserCallback);
#endif

    InlineInitializationProcessing("CSystem::Init End");

#if defined(DEDICATED_SERVER)
    SCVarsClientConfigSink CVarsClientConfigSink;
    LoadConfiguration("client.cfg", &CVarsClientConfigSink);
#endif

    // All CVars should be registered by this point, we must now flush the cvar groups
    OnSysSpecChange(m_sys_spec);

    //Connect to the render bus
    AZ::RenderNotificationsBus::Handler::BusConnect();

    m_bInitializedSuccessfully = true;

    // Send out EBus event
    EBUS_EVENT(CrySystemEventBus, OnCrySystemInitialized, *this, startupParams);

    return (true);
}


static void LoadConfigurationCmd(IConsoleCmdArgs* pParams)
{
    assert(pParams);

    if (pParams->GetArgCount() != 2)
    {
        gEnv->pLog->LogError("LoadConfiguration failed, one parameter needed");
        return;
    }

    ILoadConfigurationEntrySink* pCVarsWhiteListConfigSink = GetISystem()->GetCVarsWhiteListConfigSink();
    GetISystem()->LoadConfiguration(string("Config/") + pParams->GetArg(1), pCVarsWhiteListConfigSink);
}


// --------------------------------------------------------------------------------------------------------------------------



static void _LvlRes_export_IResourceList(AZ::IO::HandleType fileHandle, const ICryPak::ERecordFileOpenList& eList)
{
    IResourceList* pResList = gEnv->pCryPak->GetResourceList(eList);

    for (const char* filename = pResList->GetFirst(); filename; filename = pResList->GetNext())
    {
        enum
        {
            nMaxPath = 0x800
        };
        char szAbsPathBuf[nMaxPath];

        const char* szAbsPath = gEnv->pCryPak->AdjustFileName(filename, szAbsPathBuf, 0);

        gEnv->pCryPak->FPrintf(fileHandle, "%s\n", szAbsPath);
    }
}

void LvlRes_export(IConsoleCmdArgs* pParams)
{
    // * this assumes the level was already loaded in the editor (resources have been recorded)
    // * it could be easily changed to run the launcher, start recording, load a level and quit (useful to autoexport many levels)

    const char* szLevelName = gEnv->pGame->GetIGameFramework()->GetLevelName();
    char szAbsLevelPathBuf[512];
    const char* szAbsLevelPath = gEnv->pGame->GetIGameFramework()->GetAbsLevelPath(szAbsLevelPathBuf, sizeof(szAbsLevelPathBuf));

    if (!szAbsLevelPath || !szLevelName)
    {
        gEnv->pLog->LogError("Error: LvlRes_export no level loaded?");
        return;
    }

    string sPureLevelName = PathUtil::GetFile(szLevelName);     // level name without path

    // record all assets that might be loaded after level loading
    if (gEnv->pGame)
    {
        if (gEnv->pGame->GetIGameFramework())
        {
            gEnv->pGame->GetIGameFramework()->PrefetchLevelAssets(true);
        }
    }

    enum
    {
        nMaxPath = 0x800
    };
    char szAbsPathBuf[nMaxPath];
    szAbsPathBuf[0] = '\0';

    sprintf(szAbsPathBuf, "%s/%s%s", szAbsLevelPath, sPureLevelName.c_str(), g_szLvlResExt);

    // Write resource list to file.
    AZ::IO::HandleType fileHandle = gEnv->pCryPak->FOpen(szAbsPathBuf, "wt");

    if (fileHandle == AZ::IO::InvalidHandle)
    {
        gEnv->pLog->LogError("Error: LvlRes_export file open failed");
        return;
    }

    gEnv->pCryPak->FPrintf(fileHandle, "; this file can be safely deleted - it's only purpose is to produce a striped build (without unused assets)\n\n");

    char rootpath[_MAX_PATH];
    CryGetCurrentDirectory(sizeof(rootpath), rootpath);

    gEnv->pCryPak->FPrintf(fileHandle, "; EngineStartup\n");
    _LvlRes_export_IResourceList(fileHandle, ICryPak::RFOM_EngineStartup);
    gEnv->pCryPak->FPrintf(fileHandle, "; Level '%s'\n", szAbsLevelPath);
    _LvlRes_export_IResourceList(fileHandle, ICryPak::RFOM_Level);

    gEnv->pCryPak->FClose(fileHandle);
}

static string ConcatPath(const char* szPart1, const char* szPart2)
{
    if (szPart1[0] == 0)
    {
        return szPart2;
    }

    string ret;

    ret.reserve(strlen(szPart1) + 1 + strlen(szPart2));

    ret = szPart1;
    ret += "/";
    ret += szPart2;

    return ret;
}



class CLvlRes_base
{
public:

    // destructor
    virtual ~CLvlRes_base()
    {
    }

    void RegisterAllLevelPaks(const string& sPath)
    {
        _finddata_t fd;

        string sPathPattern = ConcatPath(sPath, "*.*");

        intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd);

        if (handle < 0)
        {
            gEnv->pLog->LogError("ERROR: CLvlRes_base failed '%s'", sPathPattern.c_str());
            return;
        }

        do
        {
            if (fd.attrib & _A_SUBDIR)
            {
                if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
                {
                    RegisterAllLevelPaks(ConcatPath(sPath, fd.name));
                }
            }
            else if (HasRightExtension(fd.name))         // open only the level paks if there is a LvlRes.txt, opening all would be too slow
            {
                OnPakEntry(sPath, fd.name);
            }
        } while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

        gEnv->pCryPak->FindClose(handle);
    }



    void Process(const string& sPath)
    {
        _finddata_t fd;

        string sPathPattern = ConcatPath(sPath, "*.*");

        intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd);

        if (handle < 0)
        {
            gEnv->pLog->LogError("ERROR: LvlRes_finalstep failed '%s'", sPathPattern.c_str());
            return;
        }

        do
        {
            if (fd.attrib & _A_SUBDIR)
            {
                if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
                {
                    Process(ConcatPath(sPath, fd.name));
                }
            }
            else if (HasRightExtension(fd.name))
            {
                string sFilePath = ConcatPath(sPath, fd.name);

                gEnv->pLog->Log("CLvlRes_base processing '%s' ...", sFilePath.c_str());

                AZ::IO::HandleType fileHandle = gEnv->pCryPak->FOpen(sFilePath.c_str(), "rb");

                if (fileHandle != AZ::IO::InvalidHandle)
                {
                    std::vector<char> vBuffer;

                    size_t len = gEnv->pCryPak->FGetSize(fileHandle);
                    vBuffer.resize(len + 1);

                    if (len)
                    {
                        if (gEnv->pCryPak->FReadRaw(&vBuffer[0], len, 1, fileHandle) == 1)
                        {
                            vBuffer[len] = 0;                                                         // end terminator

                            char* p = &vBuffer[0];

                            while (*p)
                            {
                                while (*p != 0 && *p <= ' ')                                     // jump over whitespace
                                {
                                    ++p;
                                }

                                char* pLineStart = p;

                                while (*p != 0 && *p != 10 && *p != 13)                    // goto end of line
                                {
                                    ++p;
                                }

                                char* pLineEnd = p;

                                while (*p != 0 && (*p == 10 || *p == 13))              // goto next line with data
                                {
                                    ++p;
                                }

                                if (*pLineStart != ';')                                            // if it's not a commented line
                                {
                                    *pLineEnd = 0;
                                    OnFileEntry(pLineStart);                // add line
                                }
                            }
                        }
                        else
                        {
                            gEnv->pLog->LogError("Error: LvlRes_finalstep file open '%s' failed", sFilePath.c_str());
                        }
                    }

                    gEnv->pCryPak->FClose(fileHandle);
                }
                else
                {
                    gEnv->pLog->LogError("Error: LvlRes_finalstep file open '%s' failed", sFilePath.c_str());
                }
            }
        } while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

        gEnv->pCryPak->FindClose(handle);
    }

    bool IsFileKnown(const char* szFilePath)
    {
        string sFilePath = szFilePath;

        return m_UniqueFileList.find(sFilePath) != m_UniqueFileList.end();
    }

protected: // -------------------------------------------------------------------------

    static bool HasRightExtension(const char* szFileName)
    {
        const char* szLvlResExt = szFileName;

        size_t lenName = strlen(szLvlResExt);
        static size_t lenLvlExt = strlen(g_szLvlResExt);

        if (lenName >= lenLvlExt)
        {
            szLvlResExt += lenName - lenLvlExt;         // "test_LvlRes.txt" -> "_LvlRes.txt"
        }
        return _stricmp(szLvlResExt, g_szLvlResExt) == 0;
    }

    // Arguments
    //   sFilePath - e.g. "game/object/vehices/car01.dds"
    void OnFileEntry(const char* szFilePath)
    {
        string sFilePath = szFilePath;
        if (m_UniqueFileList.find(sFilePath) == m_UniqueFileList.end())        // to to file processing only once per file
        {
            m_UniqueFileList.insert(sFilePath);

            ProcessFile(sFilePath);

            gEnv->pLog->UpdateLoadingScreen(0);
        }
    }

    virtual void ProcessFile(const string& sFilePath) = 0;

    virtual void OnPakEntry(const string& sPath, const char* szPak) {}

    // -----------------------------------------------------------------

    std::set<string>                m_UniqueFileList;               // to removed duplicate files
};





class CLvlRes_finalstep
    : public CLvlRes_base
{
public:

    // constructor
    CLvlRes_finalstep(const char* szPath)
        : m_sPath(szPath)
    {
        assert(szPath);
    }

    // destructor
    virtual ~CLvlRes_finalstep()
    {
        // free registered paks
        std::set<string>::iterator it, end = m_RegisteredPakFiles.end();

        for (it = m_RegisteredPakFiles.begin(); it != end; ++it)
        {
            string sName = *it;

            gEnv->pCryPak->ClosePack(sName.c_str());
        }
    }

    // register a pak file so all files within do not become file entries but the pak file becomes
    void RegisterPak(const string& sPath, const char* szFile)
    {
        string sPak = ConcatPath(sPath, szFile);

        gEnv->pCryPak->ClosePack(sPak.c_str());         // so we don't get error for paks that were already opened

        if (!gEnv->pCryPak->OpenPack(sPak.c_str()))
        {
            CryLog("RegisterPak '%s' failed - file not present?", sPak.c_str());
            return;
        }

        enum
        {
            nMaxPath = 0x800
        };
        char szAbsPathBuf[nMaxPath];

        const char* szAbsPath = gEnv->pCryPak->AdjustFileName(sPak, szAbsPathBuf, 0);

        //      string sAbsPath = PathUtil::RemoveSlash(PathUtil::GetPath(szAbsPath));

        // debug
        CryLog("RegisterPak '%s'", szAbsPath);

        m_RegisteredPakFiles.insert(string(szAbsPath));

        OnFileEntry(sPak);      // include pak as file entry
    }

    // finds a specific file
    static bool FindFile(const char* szFilePath, const char* szFile, _finddata_t& fd)
    {
        intptr_t handle = gEnv->pCryPak->FindFirst(szFilePath, &fd);

        if (handle < 0)
        {
            return false;
        }

        do
        {
            if (_stricmp(fd.name, szFile) == 0)
            {
                gEnv->pCryPak->FindClose(handle);
                return true;
            }
        } while (gEnv->pCryPak->FindNext(handle, &fd));

        gEnv->pCryPak->FindClose(handle);
        return false;
    }

    // slow but safe (to correct path and file name upper/lower case to the existing files)
    // some code might rely on the case (e.g. CVarGroup creation) so it's better to correct the case
    static void CorrectCaseInPlace(char* szFilePath)
    {
        // required for FindFirst, TODO: investigate as this seems wrong behavior
        {
            // jump over "Game"
            while (*szFilePath != '/' && *szFilePath != '\\' && *szFilePath != 0)
            {
                ++szFilePath;
            }
            // jump over "/"
            if (*szFilePath != 0)
            {
                ++szFilePath;
            }
        }

        char* szFile = szFilePath, * p = szFilePath;

        for (;; )
        {
            if (*p == '/' || *p == '\\' || *p == 0)
            {
                char cOldChar = *p;
                *p = 0;                                 // create zero termination
                _finddata_t fd;

                bool bOk = FindFile(szFilePath, szFile, fd);

                if (bOk)
                {
                    assert(strlen(szFile) == strlen(fd.name));
                }

                *p = cOldChar;                                  // get back the old separator

                if (!bOk)
                {
                    return;
                }


                memcpy((void*)szFile, fd.name, strlen(fd.name));     // set

                if (*p == 0)
                {
                    break;
                }

                ++p;
                szFile = p;
            }
            else
            {
                ++p;
            }
        }
    }

    virtual void ProcessFile(const string& _sFilePath)
    {
        string sFilePath = _sFilePath;

        CorrectCaseInPlace((char*)&sFilePath[0]);

        gEnv->pLog->LogWithType(ILog::eAlways, "LvlRes: %s", sFilePath.c_str());

        CCryFile file;
        std::vector<char> data;

        if (!file.Open(sFilePath.c_str(), "rb"))
        {
            gEnv->pLog->LogError("ERROR: failed to open '%s'", sFilePath.c_str());          // pak not opened ?
            return;
        }

        if (IsInRegisteredPak(file.GetHandle()))
        {
            return;                 // then don't process as we include the pak
        }
        // Save this file in target folder.
        string trgFilename = PathUtil::Make(m_sPath, sFilePath);
        int fsize = file.GetLength();

        size_t len = file.GetLength();

        if (fsize > (int)data.size())
        {
            data.resize(fsize + 16);
        }

        // Read data.
        file.ReadRaw(&data[0], fsize);

        // Save this data to target file.
        string trgFileDir = PathUtil::ToDosPath(PathUtil::RemoveSlash(PathUtil::GetPath(trgFilename)));

        gEnv->pFileIO->CreatePath(trgFileDir);      // ensure path exists

        // Create target file
        FILE* trgFile = fopen(trgFilename, "wb");

        if (trgFile)
        {
            fwrite(&data[0], fsize, 1, trgFile);
            fclose(trgFile);
        }
        else
        {
            gEnv->pLog->LogError("ERROR: failed to write '%s' (write protected/disk full/rights)", trgFilename.c_str());
            assert(0);
        }
    }

    bool IsInRegisteredPak(AZ::IO::HandleType fileHandle)
    {
        const char* szPak = gEnv->pCryPak->GetFileArchivePath(fileHandle);

        if (!szPak)
        {
            return false;           // outside pak
        }
        bool bInsideRegisteredPak = m_RegisteredPakFiles.find(szPak) != m_RegisteredPakFiles.end();

        return bInsideRegisteredPak;
    }

    virtual void OnPakEntry(const string& sPath, const char* szPak)
    {
        RegisterPak(sPath, "level.pak");
        RegisterPak(sPath, "levelmm.pak");
    }

    // -------------------------------------------------------------------------------

    string                                  m_sPath;                                // directory path to store the assets e.g. "c:\temp\Out"
    std::set<string>                m_RegisteredPakFiles;       // abs path to pak files we registered e.g. "c:\MasterCD\game\GameData.pak", to avoid processing files inside these pak files - the ones we anyway want to include
};


class CLvlRes_findunused
    : public CLvlRes_base
{
public:

    virtual void ProcessFile(const string& sFilePath)
    {
    }
};






static void LvlRes_finalstep(IConsoleCmdArgs* pParams)
{
    assert(pParams);

    uint32 dwCnt = pParams->GetArgCount();

    if (dwCnt != 2)
    {
        gEnv->pLog->LogWithType(ILog::eError, "ERROR: sys_LvlRes_finalstep requires destination path as parameter");
        return;
    }

    const char* szPath = pParams->GetArg(1);
    assert(szPath);

    gEnv->pLog->LogWithType(ILog::eInputResponse, "sys_LvlRes_finalstep %s ...", szPath);

    // open console
    gEnv->pConsole->ShowConsole(true);

    CLvlRes_finalstep sink(szPath);

    sink.RegisterPak("@assets@", "GameData.pak");
    sink.RegisterPak("@assets@", "Shaders.pak");

    sink.RegisterAllLevelPaks("levels");
    sink.Process("levels");
}




static void _LvlRes_findunused_recursive(CLvlRes_findunused& sink, const string& sPath,
    uint32& dwUnused, uint32& dwAll)
{
    _finddata_t fd;

    string sPathPattern = ConcatPath(sPath, "*.*");

    // ignore some directories
    if (_stricmp(sPath.c_str(), "Shaders") == 0
        || _stricmp(sPath.c_str(), "ScreenShots") == 0
        || _stricmp(sPath.c_str(), "Scripts") == 0
        || _stricmp(sPath.c_str(), "Config") == 0
        || _stricmp(sPath.c_str(), "LowSpec") == 0)
    {
        return;
    }

    //  gEnv->pLog->Log("_LvlRes_findunused_recursive '%s'",sPath.c_str());

    intptr_t handle = gEnv->pCryPak->FindFirst(sPathPattern.c_str(), &fd);

    if (handle < 0)
    {
        gEnv->pLog->LogError("ERROR: _LvlRes_findunused_recursive failed '%s'", sPathPattern.c_str());
        return;
    }

    do
    {
        if (fd.attrib & _A_SUBDIR)
        {
            if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
            {
                _LvlRes_findunused_recursive(sink, ConcatPath(sPath, fd.name), dwUnused, dwAll);
            }
        }
        else
        {
            /*
            // ignore some extensions
            if(_stricmp(PathUtil::GetExt(fd.name),"cry")==0)
                continue;

            // ignore some files
            if(_stricmp(fd.name,"TerrainTexture.pak")==0)
                continue;
            */

            string sFilePath = CryStringUtils::ToLower(ConcatPath(sPath, fd.name));
            enum
            {
                nMaxPath = 0x800
            };
            char szAbsPathBuf[nMaxPath];

            gEnv->pCryPak->AdjustFileName(sFilePath.c_str(), szAbsPathBuf, 0);

            if (!sink.IsFileKnown(szAbsPathBuf))
            {
                gEnv->pLog->LogWithType(IMiniLog::eAlways, "%d, %s", (uint32)fd.size, szAbsPathBuf);
                ++dwUnused;
            }
            ++dwAll;
        }
    } while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

    gEnv->pCryPak->FindClose(handle);
}


static void LvlRes_findunused(IConsoleCmdArgs* pParams)
{
    assert(pParams);

    gEnv->pLog->LogWithType(ILog::eInputResponse, "sys_LvlRes_findunused ...");

    // open console
    gEnv->pConsole->ShowConsole(true);

    CLvlRes_findunused sink;

    sink.RegisterAllLevelPaks("levels");
    sink.Process("levels");

    gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
    gEnv->pLog->LogWithType(ILog::eInputResponse, "Assets not used by the existing LvlRes data:");
    gEnv->pLog->LogWithType(ILog::eInputResponse, " ");

    char rootpath[_MAX_PATH];
    CryGetCurrentDirectory(sizeof(rootpath), rootpath);

    gEnv->pLog->LogWithType(ILog::eInputResponse, "Folder: %s", rootpath);

    uint32 dwUnused = 0, dwAll = 0;

    string unused;
    _LvlRes_findunused_recursive(sink, unused, dwUnused, dwAll);

    gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
    gEnv->pLog->LogWithType(ILog::eInputResponse, "Unused assets: %d/%d", dwUnused, dwAll);
    gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
}

void CryResetStats(void);

static void DumpAllocs(IConsoleCmdArgs* pParams)
{
    CryGetIMemReplay()->DumpStats();
}

static void ReplayDumpSymbols(IConsoleCmdArgs* pParams)
{
    CryGetIMemReplay()->DumpSymbols();
}

static void ReplayStop(IConsoleCmdArgs* pParams)
{
    CryGetIMemReplay()->Stop();
}

static void ReplayPause(IConsoleCmdArgs* pParams)
{
    CryGetIMemReplay()->Start(true);
}

static void ReplayResume(IConsoleCmdArgs* pParams)
{
    CryGetIMemReplay()->Start(false);
}

static void ResetAllocs(IConsoleCmdArgs* pParams)
{
    CryResetStats();
}

static void AddReplayLabel(IConsoleCmdArgs* pParams)
{
    if (pParams->GetArgCount() < 2)
    {
        CryLog("Not enough arguments");
    }
    else
    {
        CryGetIMemReplay()->AddLabel(pParams->GetArg(1));
    }
}

static void ReplayInfo(IConsoleCmdArgs* pParams)
{
    CryReplayInfo info;
    CryGetIMemReplay()->GetInfo(info);

    CryLog("Uncompressed length: %llu", info.uncompressedLength);
    CryLog("Written length: %llu", info.writtenLength);
    CryLog("Tracking overhead: %u", info.trackingSize);
    CryLog("Output filename: %s", info.filename ? info.filename : "(not open)");
}

static void AddReplaySizerTree(IConsoleCmdArgs* pParams)
{
    const char* name = "Sizers";

    if (pParams->GetArgCount() >= 2)
    {
        name = pParams->GetArg(1);
    }

    CryGetIMemReplay()->AddSizerTree(name);
}

// --------------------------------------------------------------------------------------------------------------------------

static void RecordClipCmd(IConsoleCmdArgs* pArgs)
{
    // "Usage: RecordClipCmd <exec/config> <time before> <time after> <local backup (backup/no_backup)> <annotation text>"
    // Params:
    // 0 - RecordClipCmd
    // 1 - record the clip (exec) or configure its parameters (config), 2 - time to record before the command, 3 - time to record after the command,
    // 4 - back up to HDD (backup/no_backup), 5 to end - clip description
    if (IPlatformOS::IClipCaptureOS* pClipCapture = gEnv->pSystem->GetPlatformOS()->GetClipCapture())
    {
#define DEFAULT_CLIP_DESC L"Alt F11 Clip"
#define COUNT_OF_CHARS_IN_ARRAY(arr) (sizeof(arr) / sizeof(0[arr]))

        const size_t MAX_WSTR_BUFFER = 64;

        // make sure the description string has always a reasonable size and the default string is withing size range
        COMPILE_TIME_ASSERT(COUNT_OF_CHARS_IN_ARRAY(DEFAULT_CLIP_DESC) <= MAX_WSTR_BUFFER);
        COMPILE_TIME_ASSERT(MAX_WSTR_BUFFER >= 8 && MAX_WSTR_BUFFER <= 64);

        // parameters to keep
        static CryFixedWStringT<MAX_WSTR_BUFFER> sParamFixedWStrDescription(DEFAULT_CLIP_DESC);
        static float sParamTimeAfter = 0.f, sParamTimeBefore = 10.f;
        static bool sParamDoBackUp = true;

        const uint32 argCount = pArgs->GetArgCount();

        bool bShouldRecordClip = true;

        // check argument count validity
        if (argCount > 2 && argCount < 6)
        {
            gEnv->pLog->LogWithType(ILog::eError, "RecordClipCmd requires either no parameters, the first parameter or all of them (5)'.\nYou entered: [%u]", argCount - 1);
            return;
        }

        // command type (exec/config)
        if (argCount > 1)
        {
            bShouldRecordClip = stack_string(pArgs->GetArg(1)).MakeLower() == "exec";
            if (bShouldRecordClip == false && stack_string(pArgs->GetArg(1)).MakeLower() != "config")
            {
                gEnv->pLog->LogWithType(ILog::eError, "RecordClipCmd requires that if used with parameters the first one should be either 'exec' or 'config'.\nYou entered: [%s]", pArgs->GetArg(1));
                return;
            }
        }

        // configuration parameters
        if (argCount > 2)
        {
            // time line
            sParamTimeBefore = float(atoi(pArgs->GetArg(2)));
            sParamTimeAfter = float(atoi(pArgs->GetArg(3)));

            // backup
            const bool b = stack_string(pArgs->GetArg(4)).MakeLower() == "backup";
            if (b == false && stack_string(pArgs->GetArg(4)).MakeLower() != "no_backup")
            {
                gEnv->pLog->LogWithType(ILog::eError, "RecordClipCmd requires the backup parameter to be either 'backup' or 'no_backup'.\nYou entered: [%s]", pArgs->GetArg(4));
                return;
            }
            sParamDoBackUp = b;
        }

        // clip description
        stack_string strClipDescription;
        for (uint32 i = 5; i < argCount; ++i)
        {
            if (i > 5)
            {
                strClipDescription += " ";
            }

            strClipDescription += pArgs->GetArg(i);
        }

        if (strClipDescription.empty() == false)
        {
            stack_string subStr = strClipDescription.substr(0, MAX_WSTR_BUFFER - 1);
            sParamFixedWStrDescription = CryStringUtils::UTF8ToWStr(subStr.c_str()).c_str();
        }

        // record clip or display configuration
        if (bShouldRecordClip)
        {
            IPlatformOS::IClipCaptureOS::SSpan span(sParamTimeBefore, sParamTimeAfter);
            IPlatformOS::IClipCaptureOS::SClipTextInfo clipTextInfo("RecordClipCmd", sParamFixedWStrDescription.c_str());
            pClipCapture->RecordClip(clipTextInfo, span, NULL, sParamDoBackUp);
        }
        else
        {
            gEnv->pLog->LogWithType(ILog::eMessage, "RecordClipCmd params: before[%.2f] after[%.2f] backUp[%s] desc[%ls]", sParamTimeBefore, sParamTimeAfter, sParamDoBackUp ? "True" : "False", sParamFixedWStrDescription.c_str());
        }
#undef COUNT_OF_CHARS_IN_ARRAY
#undef DEFAULT_CLIP_DESC
    }
}

static void ScreenshotCmd(IConsoleCmdArgs* pParams)
{
    assert(pParams);

    uint32 dwCnt = pParams->GetArgCount();

    if (dwCnt <= 1)
    {
        if (!gEnv->IsEditing())
        {
            // open console one line only

            //line should lie within title safe area, so calculate overscan border
            Vec2 overscanBorders = Vec2(0.0f, 0.0f);
            gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorders);
            float yDelta = /*((float)gEnv->pRenderer->GetHeight())*/ 600.0f * overscanBorders.y;

            //set console height depending on top/bottom overscan border
            gEnv->pConsole->ShowConsole(true, (int)(16 + yDelta));
            gEnv->pConsole->SetInputLine("Screenshot ");
        }
        else
        {
            gEnv->pLog->LogWithType(ILog::eInputResponse, "Screenshot <annotation> missing - no screenshot was done");
        }
    }
    else
    {
        static int iScreenshotNumber = -1;

        const char* szPrefix = "Screenshot";
        uint32 dwPrefixSize = strlen(szPrefix);

        char path[ICryPak::g_nMaxPath];
        path[sizeof(path) - 1] = 0;
        gEnv->pCryPak->AdjustFileName("@user@/ScreenShots", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);

        if (iScreenshotNumber == -1)       // first time - find max number to start
        {
            ICryPak* pCryPak = gEnv->pCryPak;
            _finddata_t fd;

            intptr_t handle = pCryPak->FindFirst((string(path) + "/*.*").c_str(), &fd);       // mastercd folder
            if (handle != -1)
            {
                int res = 0;
                do
                {
                    int iCurScreenshotNumber;

                    if (_strnicmp(fd.name, szPrefix, dwPrefixSize) == 0)
                    {
                        int iCnt = sscanf(fd.name + 10, "%d", &iCurScreenshotNumber);

                        if (iCnt)
                        {
                            iScreenshotNumber = max(iCurScreenshotNumber, iScreenshotNumber);
                        }
                    }

                    res = pCryPak->FindNext(handle, &fd);
                } while (res >= 0);
                pCryPak->FindClose(handle);
            }
        }

        ++iScreenshotNumber;

        char szNumber[16];
        sprintf(szNumber, "%.4d ", iScreenshotNumber);

        string sScreenshotName = string(szPrefix) + szNumber;

        for (uint32 dwI = 1; dwI < dwCnt; ++dwI)
        {
            if (dwI > 1)
            {
                sScreenshotName += "_";
            }

            sScreenshotName += pParams->GetArg(dwI);
        }

        sScreenshotName.replace("\\", "_");
        sScreenshotName.replace("/", "_");
        sScreenshotName.replace(":", "_");
        sScreenshotName.replace(".", "_");

        gEnv->pConsole->ShowConsole(false);

        CSystem* pCSystem = (CSystem*)(gEnv->pSystem);
        pCSystem->GetDelayedScreeenshot() = string(path) + "/" + sScreenshotName;// to delay a screenshot call for a frame
    }
}


static void SysRestoreSpecCmd(IConsoleCmdArgs* pParams)
{
    assert(pParams);

    if (pParams->GetArgCount() == 2)
    {
        const char* szArg = pParams->GetArg(1);

        ICVar* pCVar = gEnv->pConsole->GetCVar("sys_spec_Full");

        if (!pCVar)
        {
            gEnv->pLog->LogWithType(ILog::eInputResponse, "sys_RestoreSpec: no action");     // e.g. running Editor in shder compile mode
            return;
        }

        ICVar::EConsoleLogMode mode = ICVar::eCLM_Off;

        if (_stricmp(szArg, "test") == 0)
        {
            mode = ICVar::eCLM_ConsoleAndFile;
        }
        else if (_stricmp(szArg, "test*") == 0)
        {
            mode = ICVar::eCLM_FileOnly;
        }
        else if (_stricmp(szArg, "info") == 0)
        {
            mode = ICVar::eCLM_FullInfo;
        }

        if (mode != ICVar::eCLM_Off)
        {
            bool bFileOrConsole = (mode == ICVar::eCLM_FileOnly || mode == ICVar::eCLM_FullInfo);

            if (bFileOrConsole)
            {
                gEnv->pLog->LogToFile(" ");
            }
            else
            {
                CryLog(" ");
            }

            int iSysSpec = pCVar->GetRealIVal();

            if (iSysSpec == -1)
            {
                iSysSpec = ((CSystem*)gEnv->pSystem)->GetMaxConfigSpec();

                if (bFileOrConsole)
                {
                    gEnv->pLog->LogToFile("   sys_spec = Custom (assuming %d)", iSysSpec);
                }
                else
                {
                    gEnv->pLog->LogWithType(ILog::eInputResponse, "   $3sys_spec = $6Custom (assuming %d)", iSysSpec);
                }
            }
            else
            {
                if (bFileOrConsole)
                {
                    gEnv->pLog->LogToFile("   sys_spec = %d", iSysSpec);
                }
                else
                {
                    gEnv->pLog->LogWithType(ILog::eInputResponse, "   $3sys_spec = $6%d", iSysSpec);
                }
            }

            pCVar->DebugLog(iSysSpec, mode);

            if (bFileOrConsole)
            {
                gEnv->pLog->LogToFile(" ");
            }
            else
            {
                gEnv->pLog->LogWithType(ILog::eInputResponse, " ");
            }

            return;
        }
        else if (strcmp(szArg, "apply") == 0)
        {
            const char* szPrefix = "sys_spec_";

            ESystemConfigSpec originalSpec = CONFIG_CUSTOM;
            if (gEnv->IsEditor())
            {
                originalSpec = gEnv->pSystem->GetConfigSpec(true);
            }

            std::vector<const char*> cmds;

            cmds.resize(gEnv->pConsole->GetSortedVars(0, 0, szPrefix));
            gEnv->pConsole->GetSortedVars(&cmds[0], cmds.size(), szPrefix);

            gEnv->pLog->LogWithType(IMiniLog::eInputResponse, " ");

            std::vector<const char*>::const_iterator it, end = cmds.end();

            for (it = cmds.begin(); it != end; ++it)
            {
                const char* szName = *it;

                if (_stricmp(szName, "sys_spec_Full") == 0)
                {
                    continue;
                }

                pCVar = gEnv->pConsole->GetCVar(szName);
                assert(pCVar);

                if (!pCVar)
                {
                    continue;
                }

                bool bNeeded = pCVar->GetIVal() != pCVar->GetRealIVal();

                gEnv->pLog->LogWithType(IMiniLog::eInputResponse, " $3%s = $6%d ... %s",
                    szName, pCVar->GetIVal(),
                    bNeeded ? "$4restored" : "valid");

                if (bNeeded)
                {
                    pCVar->Set(pCVar->GetIVal());
                }
            }

            gEnv->pLog->LogWithType(IMiniLog::eInputResponse, " ");

            if (gEnv->IsEditor())
            {
                gEnv->pSystem->SetConfigSpec(originalSpec, true);
            }
            return;
        }
    }

    gEnv->pLog->LogWithType(ILog::eInputResponse, "ERROR: sys_RestoreSpec invalid arguments");
}

void ChangeLogAllocations(ICVar* pVal)
{
    g_iTraceAllocations = pVal->GetIVal();

    if (g_iTraceAllocations == 2)
    {
        IDebugCallStack::instance()->StartMemLog();
    }
    else
    {
        IDebugCallStack::instance()->StopMemLog();
    }
}

static void VisRegTest(IConsoleCmdArgs* pParams)
{
    CSystem* pCSystem = (CSystem*)(gEnv->pSystem);
    CVisRegTest*& visRegTest = pCSystem->GetVisRegTestPtrRef();
    if (!visRegTest)
    {
        visRegTest = new CVisRegTest();
    }

    visRegTest->Init(pParams);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CreateSystemVars()
{
    MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Create System CVars");

    assert(gEnv);
    assert(gEnv->pConsole);

    // Register DLL names as cvars before we load them
    //
    EVarFlags dllFlags = (EVarFlags)0;
    m_sys_dll_ai = REGISTER_STRING("sys_dll_ai", DLL_AI, dllFlags,                      "Specifies the DLL to load for the AI system");
    m_sys_dll_game = REGISTER_STRING("sys_dll_game", DLL_GAME, dllFlags | VF_READONLY,            "Specifies the game DLL to load (without the .dll extension)");
    m_sys_game_folder = REGISTER_STRING("sys_game_folder", "EmptyTemplate", VF_READONLY,            "Specifies the game folder to read all data from. Can be fully pathed for external folders or relative path for folders inside the root.");
    m_sys_dll_response_system = REGISTER_STRING("sys_dll_response_system", 0, dllFlags,                 "Specifies the DLL to load for the dynamic response system");

    m_sys_initpreloadpacks = REGISTER_STRING("sys_initpreloadpacks", "", 0,     "Specifies the paks for an engine initialization");
    m_sys_menupreloadpacks = REGISTER_STRING("sys_menupreloadpacks", 0, 0,      "Specifies the paks for a main menu loading");

#ifndef _RELEASE
    m_sys_resource_cache_folder = REGISTER_STRING("sys_resource_cache_folder", "Editor\\ResourceCache", 0, "Folder for resource compiled locally. Managed by Sandbox.");
#endif

#if AZ_LOADSCREENCOMPONENT_ENABLED
    m_game_load_screen_uicanvas_path = REGISTER_STRING("game_load_screen_uicanvas_path", "", 0, "Game load screen UiCanvas path.");
    m_level_load_screen_uicanvas_path = REGISTER_STRING("level_load_screen_uicanvas_path", "", 0, "Level load screen UiCanvas path.");
    m_game_load_screen_sequence_to_auto_play = REGISTER_STRING("game_load_screen_sequence_to_auto_play", "", 0, "Game load screen UiCanvas animation sequence to play on load.");
    m_level_load_screen_sequence_to_auto_play = REGISTER_STRING("level_load_screen_sequence_to_auto_play", "", 0, "Level load screen UiCanvas animation sequence to play on load.");
    m_game_load_screen_sequence_fixed_fps = REGISTER_FLOAT("game_load_screen_sequence_fixed_fps", 60.0f, 0, "Fixed frame rate fed to updates of the game load screen sequence.");
    m_level_load_screen_sequence_fixed_fps = REGISTER_FLOAT("level_load_screen_sequence_fixed_fps", 60.0f, 0, "Fixed frame rate fed to updates of the level load screen sequence.");
    m_game_load_screen_max_fps = REGISTER_FLOAT("game_load_screen_max_fps", 30.0f, 0, "Max frame rate to update the game load screen sequence.");
    m_level_load_screen_max_fps = REGISTER_FLOAT("level_load_screen_max_fps", 30.0f, 0, "Max frame rate to update the level load screen sequence.");
#endif // if AZ_LOADSCREENCOMPONENT_ENABLED

    m_cvGameName = REGISTER_STRING("sys_game_name", "Lumberyard", VF_DUMPTODISK,    "Specifies the name to be displayed in the Launcher window title bar");

    REGISTER_INT("cvDoVerboseWindowTitle", 0, VF_NULL, "");

    m_pCVarQuit = REGISTER_INT("ExitOnQuit", 1, VF_NULL, "");

    REGISTER_COMMAND("quit", "System.Quit()", VF_RESTRICTEDMODE, "Quit/Shutdown the engine");

    REGISTER_STRING_CB("sys_telemetry_stream_file", "", VF_NULL, "Telemetry stream file name", TelemetryStreamFileChanged);
    REGISTER_STRING_CB("sys_telemetry_stream_ip", "", VF_NULL, "Telemetry stream ip address", TelemetryStreamIPChanged);
    REGISTER_FLOAT("sys_telemetry_keep_alive", 5.0f, VF_NULL, "Telemetry stream keep-alive time");

#ifndef _RELEASE
    REGISTER_STRING_CB("sys_version", "", VF_CHEAT, "Override system file/product version", SystemVersionChanged);
#endif // #ifndef _RELEASE

    m_cvAIUpdate = REGISTER_INT("ai_NoUpdate", 0, VF_CHEAT, "Disables AI system update when 1");

    REGISTER_INT("r_AllowLiveMoCap", 0, VF_CHEAT, "Offers the LiveCreate MoCap Editor on Editor Startup when 1");

    m_iTraceAllocations = g_iTraceAllocations;
    REGISTER_CVAR2_CB("sys_logallocations", &m_iTraceAllocations, m_iTraceAllocations, VF_DUMPTODISK, "Save allocation call stack", ChangeLogAllocations);

    m_cvMemStats = REGISTER_INT("MemStats", 0, 0,
            "0/x=refresh rate in milliseconds\n"
            "Use 1000 to switch on and 0 to switch off\n"
            "Usage: MemStats [0..]");
    m_cvMemStatsThreshold = REGISTER_INT ("MemStatsThreshold", 32000, VF_NULL, "");
    m_cvMemStatsMaxDepth = REGISTER_INT("MemStatsMaxDepth", 4, VF_NULL, "");

    if (m_bEditor)
    {
        // In Editor our Pak priority is always 0
        g_cvars.pakVars.nPriority = ePakPriorityFileFirst;
    }

    // allows for loading gems and map files for release mode dedicated servers
#if defined(_RELEASE) && defined(DEDICATED_SERVER)
    const int dwPakPriorityFlags = VF_DEDI_ONLY;
#else
    const int dwPakPriorityFlags = VF_READONLY | VF_CHEAT;
#endif

    attachVariable("sys_SetLogLevel", &g_cvars.pakVars.nSetLogLevel, "Set AWS log level [0 - 6].");
    attachVariable("sys_PakPriority", &g_cvars.pakVars.nPriority, "If set to 1, tells CryPak to try to open the file in pak first, then go to file system", dwPakPriorityFlags);
    attachVariable("sys_PakReadSlice", &g_cvars.pakVars.nReadSlice, "If non-0, means number of kilobytes to use to read files in portions. Should only be used on Win9x kernels");
    attachVariable("sys_PakLogMissingFiles", &g_cvars.pakVars.nLogMissingFiles,
        "If non-0, missing file names go to mastercd/MissingFilesX.log.\n"
        "1) only resulting report\n"
        "2) run-time report is ON, one entry per file\n"
        "3) full run-time report");

    attachVariable("sys_PakInMemorySizeLimit", &g_cvars.pakVars.nInMemoryPerPakSizeLimit, "Individual pak size limit for being loaded into memory (MB)");
    attachVariable("sys_PakTotalInMemorySizeLimit", &g_cvars.pakVars.nTotalInMemoryPakSizeLimit, "Total limit (in MB) for all in memory paks");
    attachVariable("sys_PakLoadCache", &g_cvars.pakVars.nLoadCache, "Load in memory paks from _LoadCache folder");
    attachVariable("sys_PakLoadModePaks", &g_cvars.pakVars.nLoadModePaks, "Load mode switching paks from modes folder");
    attachVariable("sys_PakStreamCache", &g_cvars.pakVars.nStreamCache, "Load in memory paks for faster streaming (cgf_cache.pak,dds_cache.pak)");
    attachVariable("sys_PakSaveTotalResourceList", &g_cvars.pakVars.nSaveTotalResourceList, "Save resource list");
    attachVariable("sys_PakSaveLevelResourceList", &g_cvars.pakVars.nSaveLevelResourceList, "Save resource list when loading level");
    attachVariable("sys_PakSaveFastLoadResourceList", &g_cvars.pakVars.nSaveFastloadResourceList, "Save resource list during initial loading");
    attachVariable("sys_PakSaveMenuCommonResourceList", &g_cvars.pakVars.nSaveMenuCommonResourceList, "Save resource list during front end menu flow");
    attachVariable("sys_PakMessageInvalidFileAccess", &g_cvars.pakVars.nMessageInvalidFileAccess, "Message Box synchronous file access when in game");
    attachVariable("sys_PakLogInvalidFileAccess", &g_cvars.pakVars.nLogInvalidFileAccess, "Log synchronous file access when in game");
#ifndef _RELEASE
    attachVariable("sys_PakLogAllFileAccess", &g_cvars.pakVars.nLogAllFileAccess, "Log all file access allowing you to easily see whether a file has been loaded directly, or which pak file.");
#endif
    attachVariable("sys_PakValidateFileHash", &g_cvars.pakVars.nValidateFileHashes, "Validate file hashes in pak files for collisions");
    attachVariable("sys_LoadFrontendShaderCache", &g_cvars.pakVars.nLoadFrontendShaderCache, "Load frontend shader cache (on/off)");
    attachVariable("sys_UncachedStreamReads", &g_cvars.pakVars.nUncachedStreamReads, "Enable stream reads via an uncached file handle");
    attachVariable("sys_PakDisableNonLevelRelatedPaks", &g_cvars.pakVars.nDisableNonLevelRelatedPaks, "Disables all paks that are not required by specific level; This is used with per level splitted assets.");
    attachVariable("sys_PakWarnOnPakAccessFailures", &g_cvars.pakVars.nWarnOnPakAccessFails, "If 1, access failure for Paks is treated as a warning, if zero it is only a log message.");


    {
        int nDefaultRenderSplashScreen = 1;
        REGISTER_CVAR2("sys_rendersplashscreen", &g_cvars.sys_rendersplashscreen, nDefaultRenderSplashScreen, VF_NULL,
            "Render the splash screen during game initialization");
        REGISTER_CVAR2("sys_splashscreenscalemode", &g_cvars.sys_splashScreenScaleMode, static_cast<int>(SSystemCVars::SplashScreenScaleMode_Fill), VF_NULL,
            "0 - scale to fit (letterbox)\n"
            "1 - scale to fill (cropped)\n"
            "Default is 1");
        REGISTER_CVAR2("sys_splashscreen", &g_cvars.sys_splashscreen, "EngineAssets/Textures/startscreen.tif", VF_NULL,
            "The splash screen to render during game initialization");
    }

    REGISTER_CVAR2("sys_deferAudioUpdateOptim", &g_cvars.sys_deferAudioUpdateOptim, 1, VF_NULL,
        "0 - disable optimisation\n"
        "1 - enable optimisation\n"
        "Default is 1");

#if USE_STEAM
#ifndef RELEASE
    REGISTER_CVAR2("sys_steamAppId", &g_cvars.sys_steamAppId, 0, VF_NULL, "steam appId used for development testing");
    REGISTER_COMMAND("sys_wipeSteamCloud", CmdWipeSteamCloud, VF_CHEAT, "Delete all files from steam cloud for this user");
#endif // RELEASE
    REGISTER_CVAR2("sys_useSteamCloudForPlatformSaving", &g_cvars.sys_useSteamCloudForPlatformSaving, 0, VF_NULL, "Use steam cloud for save games and profile on PC (instead of the user folder)");
#endif

    m_sysNoUpdate = REGISTER_INT("sys_noupdate", 0, VF_CHEAT,
            "Toggles updating of system with sys_script_debugger.\n"
            "Usage: sys_noupdate [0/1]\n"
            "Default is 0 (system updates during debug).");

    m_sysWarnings = REGISTER_INT("sys_warnings", 0, 0,
            "Toggles printing system warnings.\n"
            "Usage: sys_warnings [0/1]\n"
            "Default is 0 (off).");

#if defined(_RELEASE) && defined(CONSOLE) && !defined(ENABLE_LW_PROFILERS)
    enum
    {
        e_sysKeyboardDefault = 0
    };
#else
    enum
    {
        e_sysKeyboardDefault = 1
    };
#endif
    m_sysKeyboard = REGISTER_INT("sys_keyboard", e_sysKeyboardDefault, 0,
            "Enables keyboard.\n"
            "Usage: sys_keyboard [0/1]\n"
            "Default is 1 (on).");

    m_svDedicatedMaxRate = REGISTER_FLOAT("sv_DedicatedMaxRate", 30.0f, 0,
            "Sets the maximum update rate when running as a dedicated server.\n"
            "Usage: sv_DedicatedMaxRate [5..500]\n"
            "Default is 30.");

    REGISTER_FLOAT("sv_DedicatedCPUPercent", 0.0f, 0,
        "Sets the target CPU usage when running as a dedicated server, or disable this feature if it's zero.\n"
        "Usage: sv_DedicatedCPUPercent [0..100]\n"
        "Default is 0 (disabled).");
    REGISTER_FLOAT("sv_DedicatedCPUVariance", 10.0f, 0,
        "Sets how much the CPU can vary from sv_DedicateCPU (up or down) without adjusting the framerate.\n"
        "Usage: sv_DedicatedCPUVariance [5..50]\n"
        "Default is 10.");

    m_svAISystem = REGISTER_INT("sv_AISystem", 1, VF_REQUIRE_APP_RESTART, "Load and use the AI system on the server");

    m_clAISystem = REGISTER_INT("cl_AISystem", 0, VF_REQUIRE_APP_RESTART, "Load and use the AI system on the client");

    m_cvSSInfo =  REGISTER_INT("sys_SSInfo", 0, 0,
            "Show SourceSafe information (Name,Comment,Date) for file errors."
            "Usage: sys_SSInfo [0/1]\n"
            "Default is 0 (off)");

    m_cvEntitySuppressionLevel = REGISTER_INT("e_EntitySuppressionLevel", 0, 0,
            "Defines the level at which entities are spawned.\n"
            "Entities marked with lower level will not be spawned - 0 means no level.\n"
            "Usage: e_EntitySuppressionLevel [0-infinity]\n"
            "Default is 0 (off)");

    m_sys_profile = REGISTER_INT("profile", 0, 0, "Allows CPU profiling\n"
            "Usage: profile #\n"
            "Where # sets the profiling to:\n"
            "	0: Profiling off\n"
            "	1: Self Time\n"
            "	2: Hierarchical Time\n"
            "	3: Extended Self Time\n"
            "	4: Extended Hierarchical Time\n"
            "	5: Peaks Time\n"
            "	6: Subsystem Info\n"
            "	7: Calls Numbers\n"
            "	8: Standard Deviation\n"
            "	9: Memory Allocation\n"
            "	10: Memory Allocation (Bytes)\n"
            "	11: Stalls\n"
            "	-1: Profiling enabled, but not displayed\n"
            "Default is 0 (off)");


    m_sys_profile_additionalsub = REGISTER_INT("profile_additionalsub", 0, 0, "Enable displaying additional sub-system profiling.\n"
            "Usage: profile_additionalsub #\n"
            "Where where # may be:\n"
            "	0: no additional subsystem information\n"
            "	1: display additional subsystem information\n"
            "Default is 0 (off)");

    m_sys_profile_filter = REGISTER_STRING("profile_filter", "", 0,
            "Profiles a specified subsystem.\n"
            "Usage: profile_filter subsystem\n"
            "Where 'subsystem' may be:\n"
            "Renderer\n"
            "3DEngine\n"
            "Animation\n"
            "AI\n"
            "Entity\n"
            "Physics\n"
            "Sound\n"
            "System\n"
            "Game\n"
            "Editor\n"
            "Script\n"
            "Network");
    m_sys_profile_filter_thread = REGISTER_STRING("profile_filter_thread", "", 0,
            "Profiles a specified thread only.\n"
            "Usage: profile_filter threadName\n"
            "Where 'threadName' may be:\n"
            "Main\n"
            "Renderer\n"
            "Streaming\n"
            "etc...");
    m_sys_profile_graph = REGISTER_INT("profile_graph", 0, 0,
            "Enable drawing of profiling graph.");
    m_sys_profile_graphScale = REGISTER_FLOAT("profile_graphScale", 100.0f, 0,
            "Sets the scale of profiling histograms.\n"
            "Usage: profileGraphScale 100");
    m_sys_profile_pagefaultsgraph = REGISTER_INT("profile_pagefaults", 0, 0,
            "Enable drawing of page faults graph.");
    m_sys_profile_allThreads = REGISTER_INT("profile_allthreads", 0, 0,
            "Enables profiling of non-main threads.\n");
    m_sys_profile_network = REGISTER_INT("profile_network", 0, 0,
            "Enables network profiling");
    m_sys_profile_peak = REGISTER_FLOAT("profile_peak", 10.0f, 0,
            "Profiler Peaks Tolerance in Milliseconds");
    m_sys_profile_peak_time = REGISTER_FLOAT("profile_peak_display", 8.0f, 0,
            "hot to cold time for peak display");
    m_sys_profile_memory = REGISTER_INT("MemInfo", 0, 0, "Display memory information by modules\n1=on, 0=off");

    m_sys_profile_sampler = REGISTER_FLOAT("profile_sampler", 0, 0,
            "Set to 1 to start sampling profiling");
    m_sys_profile_sampler_max_samples = REGISTER_FLOAT("profile_sampler_max_samples", 2000, 0,
            "Number of samples to collect for sampling profiler");
    m_sys_job_system_filter = REGISTER_STRING("sys_job_system_filter", "", 0,
            "Filters a Job.\n"
            "Usage: sys_job_system_filter name1,name2,..\n"
            "Where 'name' refers to the exact name of the job, 0 disables it\n"
            "More than one job can be specified by using a comma separated list");
    m_sys_job_system_enable = REGISTER_INT("sys_job_system_enable", 1, 0,
            "Enable the JobSystem.\n"
            "Usage: sys_job_system_enable 0/1\n"
            "0: The Jobsystem is disabled, each job is executed in its invoking thread.\n"
            "1: The JobSystem is enabled, each job is invoked in one of the worker threads.");
    m_sys_job_system_profiler = REGISTER_INT("sys_job_system_profiler", 0, 0,
            "Enable the job system profiler.\n"
            "Usage: sys_job_system_profiler <value>\n"
            "0: Disable the profiler\n"
            "1: Show the full profiler\n"
            "2: Show only the execution graph\n");
#if defined(WIN32) || defined(WIN64) || defined(DURANGO)
    const uint32 nJobSystemDefaultCoreNumber = 8;
#else
    const uint32 nJobSystemDefaultCoreNumber = 4;
#endif
    m_sys_job_system_max_worker = REGISTER_INT("sys_job_system_max_worker", nJobSystemDefaultCoreNumber, 0,
            "Sets the number of threads to use for the job system"
            "Defaults to 4 on consoles and 8 threads an PC"
            "Set to 0 to create as many threads as cores are available");

    REGISTER_COMMAND("sys_job_system_dump_job_list", CmdDumpJobManagerJobList, VF_CHEAT, "Show a list of all registered job in the console");

    m_sys_spec = REGISTER_INT_CB("sys_spec", CONFIG_CUSTOM, VF_ALWAYSONCHANGE,      // starts with CONFIG_CUSTOM so callback is called when setting initial value
            "Specifies the system cfg spec. (0=custom, 1=low, 2=med, 3=high, 4=very high, 5=Xbox One, 6=PS4, 7=Mobile)\n"
            "Setting this in system.cfg only applies to the game launcher and not the editor.\n"
            "Setting this in editor.cfg hardcodes the config spec in the editor and will no longer be configurable via the Config Spec menu item.",
            OnSysSpecChange);

    m_sys_SimulateTask = REGISTER_INT("sys_SimulateTask", 0, 0,
            "Simulate a task in System:Update which takes X ms");

    m_sys_firstlaunch = REGISTER_INT("sys_firstlaunch", 0, 0,
            "Indicates that the game was run for the first time.");

    m_sys_main_CPU = REGISTER_INT("sys_main_CPU", 0, 0,
            "Specifies the physical CPU index main will run on");

    m_sys_TaskThread_CPU[0] = REGISTER_INT("sys_TaskThread0_CPU", 3, 0,
            "Specifies the physical CPU index taskthread0 will run on");

    m_sys_TaskThread_CPU[1] = REGISTER_INT("sys_TaskThread1_CPU", 5, 0,
            "Specifies the physical CPU index taskthread1 will run on");

    m_sys_TaskThread_CPU[2] = REGISTER_INT("sys_TaskThread2_CPU", 4, 0,
            "Specifies the physical CPU index taskthread2 will run on");

    m_sys_TaskThread_CPU[3] = REGISTER_INT("sys_TaskThread3_CPU", 3, 0,
            "Specifies the physical CPU index taskthread3 will run on");

    m_sys_TaskThread_CPU[4] = REGISTER_INT("sys_TaskThread4_CPU", 2, 0,
            "Specifies the physical CPU index taskthread4 will run on");

    m_sys_TaskThread_CPU[5] = REGISTER_INT("sys_TaskThread5_CPU", 1, 0,
            "Specifies the physical CPU index taskthread5 will run on");

    //if physics thread is excluded all locks inside are mapped to NO_LOCK
    //var must be not visible to accidentally get enabled
#if defined(EXCLUDE_PHYSICS_THREAD)
    m_sys_physics_CPU = REGISTER_INT("sys_physics_CPU_disabled", 0, 0,
            "Specifies the physical CPU index physics will run on");
#else
    m_sys_physics_CPU = REGISTER_INT("sys_physics_CPU", 1, 0,
            "Specifies the physical CPU index physics will run on");
#endif


    m_sys_min_step = REGISTER_FLOAT("sys_min_step", 0.01f, 0,
            "Specifies the minimum physics step in a separate thread");
    m_sys_max_step = REGISTER_FLOAT("sys_max_step", 0.05f, 0,
            "Specifies the maximum physics step in a separate thread");

    m_sys_enable_budgetmonitoring = REGISTER_INT("sys_enable_budgetmonitoring", 0, 0,
            "Enables budget monitoring. Use #System.SetBudget( sysMemLimitInMB, videoMemLimitInMB,\n"
            "frameTimeLimitInMS, soundChannelsPlaying ) or sys_budget_sysmem, sys_budget_videomem\n"
            "or sys_budget_fps to set budget limits.");

    // used in define MEMORY_DEBUG_POINT()
    m_sys_memory_debug = REGISTER_INT("sys_memory_debug", 0, VF_CHEAT,
            "Enables to activate low memory situation is specific places in the code (argument defines which place), 0=off");

    REGISTER_CVAR2("sys_vtune", &g_cvars.sys_vtune, 0, VF_NULL, "");

    REGISTER_CVAR2("sys_streaming_CPU", &g_cvars.sys_streaming_cpu, 1, VF_NULL, "Specifies the physical CPU file IO thread run on");
    REGISTER_CVAR2("sys_streaming_CPU_worker", &g_cvars.sys_streaming_cpu_worker, 5, VF_NULL, "Specifies the physical CPU file IO worker thread/s run on");
    REGISTER_CVAR2("sys_streaming_memory_budget", &g_cvars.sys_streaming_memory_budget, 10 * 1024, VF_NULL, "Temp memory streaming system can use in KB");
    REGISTER_CVAR2("sys_streaming_max_finalize_per_frame", &g_cvars.sys_streaming_max_finalize_per_frame, 0, VF_NULL,
        "Maximum stream finalizing calls per frame to reduce the CPU impact on main thread (0 to disable)");
    REGISTER_CVAR2("sys_streaming_max_bandwidth", &g_cvars.sys_streaming_max_bandwidth, 0, VF_NULL, "Enables capping of max streaming bandwidth in MB/s");
    REGISTER_CVAR2("sys_streaming_debug", &g_cvars.sys_streaming_debug, 0, VF_NULL, "Enable streaming debug information\n"
        "0=off\n"
        "1=Streaming Stats\n"
        "2=File IO\n"
        "3=Request Order\n"
        "4=Write to Log\n"
        "5=Stats per extension\n"
        );
    REGISTER_CVAR2("sys_streaming_requests_grouping_time_period", &g_cvars.sys_streaming_requests_grouping_time_period, 2, VF_NULL, // Vlad: 2 works better than 4 visually, should be be re-tested when streaming pak's activated
        "Streaming requests are grouped by request time and then sorted by disk offset");
    REGISTER_CVAR2("sys_streaming_debug_filter", &g_cvars.sys_streaming_debug_filter, 0, VF_NULL, "Set streaming debug information filter.\n"
        "0=all\n"
        "1=Texture\n"
        "2=Geometry\n"
        "3=Terrain\n"
        "4=Animation\n"
        "5=Music\n"
        "6=Sound\n"
        "7=Shader\n"
        );
    g_cvars.sys_streaming_debug_filter_file_name = REGISTER_STRING("sys_streaming_debug_filter_file_name", "", VF_CHEAT,
            "Set streaming debug information filter");
    REGISTER_CVAR2("sys_streaming_debug_filter_min_time", &g_cvars.sys_streaming_debug_filter_min_time, 0.f, VF_NULL, "Show only slow items.");
    REGISTER_CVAR2("sys_streaming_resetstats", &g_cvars.sys_streaming_resetstats, 0, VF_NULL,
        "Reset all the streaming stats");
#if defined(DEDICATED_SERVER)
#define DEFAULT_USE_OPTICAL_DRIVE_THREAD 0
#else
#define DEFAULT_USE_OPTICAL_DRIVE_THREAD 1
#endif // defined(DEDICATED_SERVER)
    REGISTER_CVAR2("sys_streaming_use_optical_drive_thread", &g_cvars.sys_streaming_use_optical_drive_thread, DEFAULT_USE_OPTICAL_DRIVE_THREAD, VF_NULL,
        "Allow usage of an extra optical drive thread for faster streaming from 2 medias");

    const char* localizeFolder = "Localization";
    g_cvars.sys_localization_folder = REGISTER_STRING_CB("sys_localization_folder", localizeFolder, VF_NULL,
            "Sets the folder where to look for localized data.\n"
            "This cvar allows for backwards compatibility so localized data under the game folder can still be found.\n"
            "Usage: sys_localization_folder <folder name>\n"
            "Default: Localization\n",
            CSystem::OnLocalizationFolderCVarChanged);

    REGISTER_CVAR2("sys_streaming_in_blocks", &g_cvars.sys_streaming_in_blocks, 1, VF_NULL,
        "Streaming of large files happens in blocks");

#if (defined(WIN32) || defined(WIN64)) && !defined(_RELEASE)
    REGISTER_CVAR2("sys_float_exceptions", &g_cvars.sys_float_exceptions, 3, 0, "Use or not use floating point exceptions.");
#else // Float exceptions by default disabled for console builds.
    REGISTER_CVAR2("sys_float_exceptions", &g_cvars.sys_float_exceptions, 0, 0, "Use or not use floating point exceptions.");
#endif

    REGISTER_CVAR2("sys_update_profile_time", &g_cvars.sys_update_profile_time, 1.0f, 0, "Time to keep updates timings history for.");
    REGISTER_CVAR2("sys_no_crash_dialog", &g_cvars.sys_no_crash_dialog, m_bNoCrashDialog, VF_NULL, "");
#if !defined(DEDICATED_SERVER) && defined(_RELEASE)
    REGISTER_CVAR2("sys_WER", &g_cvars.sys_WER, 1, 0, "Enables Windows Error Reporting");
#else
    REGISTER_CVAR2("sys_WER", &g_cvars.sys_WER, 0, 0, "Enables Windows Error Reporting");
#endif

#ifdef USE_HTTP_WEBSOCKETS
    REGISTER_CVAR2("sys_simple_http_base_port", &g_cvars.sys_simple_http_base_port, 1880, VF_REQUIRE_APP_RESTART,
        "sets the base port for the simple http server to run on, defaults to 1880");
#endif

    const int DEFAULT_DUMP_TYPE = 2;

    REGISTER_CVAR2("sys_dump_type", &g_cvars.sys_dump_type, DEFAULT_DUMP_TYPE, VF_NULL,
        "Specifies type of crash dump to create - see MINIDUMP_TYPE in dbghelp.h for full list of values\n"
        "0: Do not create a minidump\n"
        "1: Create a small minidump (stacktrace)\n"
        "2: Create a medium minidump (+ some variables)\n"
        "3: Create a full minidump (+ all memory)\n"
        );
    REGISTER_CVAR2("sys_dump_aux_threads", &g_cvars.sys_dump_aux_threads, 1, VF_NULL, "Dumps callstacks of other threads in case of a crash");
    REGISTER_CVAR2("sys_keyboard_break", &g_cvars.sys_keyboard_break, 0, VF_NULL, "Enables keyboard break handler");

    REGISTER_CVAR2("sys_limit_phys_thread_count", &g_cvars.sys_limit_phys_thread_count, 1, VF_NULL, "Limits p_num_threads to physical CPU count - 1");

#if (defined(WIN32) || defined(WIN64)) && defined(_RELEASE)
    const int DEFAULT_SYS_MAX_FPS = 0;
#else
    const int DEFAULT_SYS_MAX_FPS = -1;
#endif
    REGISTER_CVAR2("sys_MaxFPS", &g_cvars.sys_MaxFPS, DEFAULT_SYS_MAX_FPS, VF_NULL, "Limits the frame rate to specified number n (if n>0 and if vsync is disabled).\n"
        " 0 = on PC if vsync is off auto throttles fps while in menu or game is paused (default)\n"
        "-1 = off");

    REGISTER_CVAR2("sys_maxTimeStepForMovieSystem", &g_cvars.sys_maxTimeStepForMovieSystem, 0.1f, VF_NULL, "Caps the time step for the movie system so that a cut-scene won't be jumped in the case of an extreme stall.");

    REGISTER_CVAR2("sys_force_installtohdd_mode", &g_cvars.sys_force_installtohdd_mode, 0, VF_NULL, "Forces install to HDD mode even when doing DVD emulation");

    m_sys_preload = REGISTER_INT("sys_preload", 0, 0, "Preload Game Resources");
    REGISTER_COMMAND("sys_crashtest", CmdCrashTest, VF_CHEAT, "Make the game crash\n"
        "0=off\n"
        "1=null pointer exception\n"
        "2=floating point exception\n"
        "3=memory allocation exception\n"
        "4=cry fatal error is called\n"
        "5=memory allocation for small blocks\n"
        "6=assert\n"
        "7=debugbreak\n"
        "8=10min sleep"
        );

    REGISTER_FLOAT("sys_scale3DMouseTranslation", 0.2f, 0, "Scales translation speed of supported 3DMouse devices.");
    REGISTER_FLOAT("sys_Scale3DMouseYPR", 0.05f, 0, "Scales rotation speed of supported 3DMouse devices.");

    REGISTER_INT("capture_frames", 0, 0, "Enables capturing of frames. 0=off, 1=on");
    REGISTER_STRING("capture_folder", "CaptureOutput", 0, "Specifies sub folder to write captured frames.");
    REGISTER_STRING("capture_file_format", "jpg", 0, "Specifies file format of captured files (jpg, tga, tif).");
    REGISTER_INT("capture_frame_once", 0, 0, "Makes capture single frame only");
    REGISTER_STRING("capture_file_name", "", 0, "If set, specifies the path and name to use for the captured frame");
    REGISTER_STRING("capture_file_prefix", "", 0, "If set, specifies the prefix to use for the captured frame instead of the default 'Frame'.");
    REGISTER_INT("capture_buffer", 0, 0,
        "Buffer to capture when capture_frames is enabled.\n"
        "0=Color\n"
        "1=Color with Alpha (requires capture_file_format=tga)");


    m_gpu_particle_physics = REGISTER_INT("gpu_particle_physics", 0, VF_REQUIRE_APP_RESTART, "Enable GPU physics if available (0=off / 1=enabled).");
    assert(m_gpu_particle_physics);

    REGISTER_COMMAND("LoadConfig", &LoadConfigurationCmd, 0,
        "Load .cfg file from disk (from the {Game}/Config directory)\n"
        "e.g. LoadConfig lowspec.cfg\n"
        "Usage: LoadConfig <filename>");

    REGISTER_CVAR(sys_ProfileLevelLoading, 0, VF_CHEAT,
        "Output level loading stats into log\n"
        "0 = Off\n"
        "1 = Output basic info about loading time per function\n"
        "2 = Output full statistics including loading time and memory allocations with call stack info");

    REGISTER_CVAR_CB(sys_ProfileLevelLoadingDump, 0, VF_CHEAT,  "Output level loading dump stats into log\n", OnLevelLoadingDump);


    assert(m_env.pConsole);
    m_env.pConsole->CreateKeyBind("alt_f12", "Screenshot");
    m_env.pConsole->CreateKeyBind("alt_f11", "RecordClip");

    // video clip recording functionality in system as console command
    REGISTER_COMMAND("RecordClip", &RecordClipCmd, VF_BLOCKFRAME,
        "Records a video clip of the game\n"
        "Usage: RecordClipCmd <exec/config> <time before> <time after> <local backup (backup/no_backup)> <annotation text>\n"
        "e.g. RecordClipCmd config 10 5 backup My Test Video\n"
        "     Configures the recording parameters\n"
        "e.g. RecordClipCmd config\n"
        "     Shows the current parameters\n"
        "e.g. RecordClipCmd\n"
        "     Records a video clip using the stored parameters\n"
        "e.g. RecordClipCmd exec 3 6 no_backup Other Test Video\n"
        "     Records a video clip using the given recording parameters and updates the configuration\n");

    // screenshot functionality in system as console
    REGISTER_COMMAND("Screenshot", &ScreenshotCmd, VF_BLOCKFRAME,
        "Create a screenshot with annotation\n"
        "e.g. Screenshot beach scene with shark\n"
        "Usage: Screenshot <annotation text>");

    REGISTER_COMMAND("sys_LvlRes_finalstep", &LvlRes_finalstep, 0, "to combine all recorded level resources and create final stripped build (pass directory name as parameter)");
    REGISTER_COMMAND("sys_LvlRes_findunused", &LvlRes_findunused, 0, "find unused level resources");
    /*
        // experimental feature? - needs to be created very early
        m_sys_filecache = REGISTER_INT("sys_FileCache",0,0,
            "To speed up loading from non HD media\n"
            "0=off / 1=enabled");
    */
    REGISTER_CVAR2("sys_AI", &g_cvars.sys_ai, 1, 0, "Enables AI Update");
    REGISTER_CVAR2("sys_physics", &g_cvars.sys_physics, 1, 0, "Enables Physics Update");
    REGISTER_CVAR2("sys_entities", &g_cvars.sys_entitysystem, 1, 0, "Enables Entities Update");
    REGISTER_CVAR2("sys_trackview", &g_cvars.sys_trackview, 1, 0, "Enables TrackView Update");

    m_sys_skip_input = REGISTER_INT("sys_skip_input", 0, VF_NULL, "Skip input initialization");

    //Defines selected language.
    REGISTER_STRING_CB("g_language", "", VF_NULL, "Defines which language pak is loaded", CSystem::OnLanguageCVarChanged);
    REGISTER_STRING_CB("g_languageAudio", "", VF_NULL, "Will automatically match g_language setting unless specified otherwise", CSystem::OnLanguageAudioCVarChanged);

    REGISTER_COMMAND("sys_RestoreSpec", &SysRestoreSpecCmd, 0,
        "Restore or test the cvar settings of game specific spec settings,\n"
        "'test*' and 'info' log to the log file only\n"
        "Usage: sys_RestoreSpec [test|test*|apply|info]");

    REGISTER_COMMAND("VisRegTest", &VisRegTest, 0, "Run visual regression test.\n"
        "Usage: VisRegTest [<name>=test] [<config>=visregtest.xml] [quit=false]");

#if CAPTURE_REPLAY_LOG
    REGISTER_COMMAND("memDumpAllocs", &DumpAllocs, 0, "print allocs with stack traces");
    REGISTER_COMMAND("memReplayDumpSymbols", &ReplayDumpSymbols, 0, "dump symbol info to mem replay log");
    REGISTER_COMMAND("memReplayStop", &ReplayStop, 0, "stop logging to mem replay");
    REGISTER_COMMAND("memReplayPause", &ReplayPause, 0, "Pause collection of mem replay data");
    REGISTER_COMMAND("memReplayResume", &ReplayResume, 0, "Resume collection of mem replay data (use with -memReplayPaused cmdline)");
    REGISTER_COMMAND("memResetAllocs", &ResetAllocs, 0, "clears memHierarchy tree");
    REGISTER_COMMAND("memReplayLabel", &AddReplayLabel, 0, "record a label in the mem replay log");
    REGISTER_COMMAND("memReplayInfo", &ReplayInfo, 0, "output some info about the replay log");
    REGISTER_COMMAND("memReplayAddSizerTree", &AddReplaySizerTree, 0, "output in-game sizer information to the log");
#endif

#if USE_LEVEL_HEAP
    CLevelHeap::RegisterCVars();
#endif

#ifndef MEMMAN_STATIC
    CCryMemoryManager::RegisterCVars();
#endif

#if defined(WIN32) || defined(DURANGO)
    REGISTER_CVAR2("sys_display_threads", &g_cvars.sys_display_threads, 0, 0, "Displays Thread info");
#endif

#if defined(WIN32)
    static const int default_sys_usePlatformSavingAPI = 0;
    static const int default_sys_usePlatformSavingAPIDefault = 0;
#else
    static const int default_sys_usePlatformSavingAPI = 1;
    static const int default_sys_usePlatformSavingAPIDefault = 1;
#endif

    REGISTER_CVAR2("sys_usePlatformSavingAPI", &g_cvars.sys_usePlatformSavingAPI, default_sys_usePlatformSavingAPI, VF_CHEAT, "Use the platform APIs for saving and loading (complies with TRCs, but allocates lots of memory)");
#ifndef _RELEASE
    REGISTER_CVAR2("sys_usePlatformSavingAPIEncryption", &g_cvars.sys_usePlatformSavingAPIEncryption, default_sys_usePlatformSavingAPIDefault, VF_CHEAT, "Use encryption cipher when using the platform APIs for saving and loading");
#endif

    REGISTER_CVAR2("sys_asserts", &g_cvars.sys_asserts, 1, VF_CHEAT,
        "0 = Disable Asserts\n"
        "1 = Enable Asserts\n"
        "2 = Fatal Error on Assert\n"
        );

    REGISTER_CVAR2("sys_error_debugbreak", &g_cvars.sys_error_debugbreak, 0, VF_CHEAT, "__debugbreak() if a VALIDATOR_ERROR_DBGBREAK message is hit");

    // [VR]
    AZ::VR::HMDCVars::Register();

    REGISTER_STRING("dlc_directory", "", 0, "Holds the path to the directory where DLC should be installed to and read from");

#if defined(MAP_LOADING_SLICING)
    CreateSystemScheduler(this);
#endif // defined(MAP_LOADING_SLICING)

#if defined(WIN32) || defined(WIN64)
    REGISTER_INT("sys_screensaver_allowed", 0, VF_NULL, "Specifies if screen saver is allowed to start up while the game is running.");
#endif

    // Since the UI Canvas Editor is incomplete, we have a variable to enable it.
    // By default it is now enabled. Modify system.cfg or game.cfg to disable it
    REGISTER_INT("sys_enableCanvasEditor", 1, VF_NULL, "Enables the UI Canvas Editor");
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CreateAudioVars()
{
    assert(gEnv);
    assert(gEnv->pConsole);

    m_sys_audio_disable = REGISTER_INT("sys_audio_disable", 0, VF_REQUIRE_APP_RESTART,
            "Specifies whether to use the NULLAudioSystem in place of the regular AudioSystem\n"
            "Usage: sys_audio_disable [0/1]\n"
            "0: use regular AudioSystem.\n"
            "1: use NullAudioSystem (disable all audio functionality).\n"
            "Default: 0 (enable audio functionality)");
}

/////////////////////////////////////////////////////////////////////
void CSystem::AddCVarGroupDirectory(const string& sPath)
{
    CryLog("creating CVarGroups from directory '%s' ...", sPath.c_str());
    INDENT_LOG_DURING_SCOPE();

    _finddata_t fd;

    intptr_t handle = gEnv->pCryPak->FindFirst(ConcatPath(sPath, "*.cfg"), &fd);

    if (handle < 0)
    {
        return;
    }

    do
    {
        if (fd.attrib & _A_SUBDIR)
        {
            if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
            {
                AddCVarGroupDirectory(ConcatPath(sPath, fd.name));
            }
        }
        else
        {
            string sFilePath = ConcatPath(sPath, fd.name);

            string sCVarName = sFilePath;
            PathUtil::RemoveExtension(sCVarName);

            if (m_env.pConsole != 0)
            {
                ((CXConsole*)m_env.pConsole)->RegisterCVarGroup(PathUtil::GetFile(sCVarName), sFilePath);
            }
        }
    } while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

    gEnv->pCryPak->FindClose(handle);
}

void CSystem::OutputLoadingTimeStats()
{
#if defined(ENABLE_LOADING_PROFILER)
    if (GetIConsole())
    {
        if (ICVar* pVar = GetIConsole()->GetCVar("sys_ProfileLevelLoading"))
        {
            CLoadingProfilerSystem::OutputLoadingTimeStats(GetILog(), pVar->GetIVal());
        }
    }
#endif
}

SLoadingTimeContainer* CSystem::StartLoadingSectionProfiling(CLoadingTimeProfiler* pProfiler, const char* szFuncName)
{
#if defined(ENABLE_LOADING_PROFILER)
    return CLoadingProfilerSystem::StartLoadingSectionProfiling(pProfiler, szFuncName);
#else
    return 0;
#endif
}

void CSystem::EndLoadingSectionProfiling(CLoadingTimeProfiler* pProfiler)
{
#if defined(ENABLE_LOADING_PROFILER)
    CLoadingProfilerSystem::EndLoadingSectionProfiling(pProfiler);
#endif
}

const char* CSystem::GetLoadingProfilerCallstack()
{
#if defined(ENABLE_LOADING_PROFILER)
    return CLoadingProfilerSystem::GetLoadingProfilerCallstack();
#else
    return 0;
#endif
}

CBootProfilerRecord* CSystem::StartBootSectionProfiler(const char* name, const char* args)
{
#if defined(ENABLE_LOADING_PROFILER)
    CBootProfiler& profiler = CBootProfiler::GetInstance();
    return profiler.StartBlock(name, args);
#else
    return NULL;
#endif
}

void CSystem::StopBootSectionProfiler(CBootProfilerRecord* record)
{
#if defined(ENABLE_LOADING_PROFILER)
    CBootProfiler& profiler = CBootProfiler::GetInstance();
    profiler.StopBlock(record);
#endif
}

void CSystem::StartBootProfilerSessionFrames(const char* pName)
{
#if defined(ENABLE_LOADING_PROFILER)
    CBootProfiler& profiler = CBootProfiler::GetInstance();
    profiler.StartFrame(pName);
#endif
}

void CSystem::StopBootProfilerSessionFrames()
{
#if defined(ENABLE_LOADING_PROFILER)
    CBootProfiler& profiler = CBootProfiler::GetInstance();
    profiler.StopFrame();
#endif
}

bool CSystem::RegisterErrorObserver(IErrorObserver* errorObserver)
{
    return stl::push_back_unique(m_errorObservers, errorObserver);
}

bool CSystem::UnregisterErrorObserver(IErrorObserver* errorObserver)
{
    return stl::find_and_erase(m_errorObservers, errorObserver);
}

void CSystem::OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber)
{
    if (g_cvars.sys_asserts == 0)
    {
        return;
    }

    std::vector<IErrorObserver*>::const_iterator end = m_errorObservers.end();
    for (std::vector<IErrorObserver*>::const_iterator it = m_errorObservers.begin(); it != end; ++it)
    {
        (*it)->OnAssert(condition, message, fileName, fileLineNumber);
    }
    if (g_cvars.sys_asserts > 1)
    {
        CryFatalError("<assert> %s\r\n%s\r\n%s (%d)\r\n", condition, message, fileName, fileLineNumber);
    }
}

void CSystem::OnFatalError(const char* message)
{
    std::vector<IErrorObserver*>::const_iterator end = m_errorObservers.end();
    for (std::vector<IErrorObserver*>::const_iterator it = m_errorObservers.begin(); it != end; ++it)
    {
        (*it)->OnFatalError(message);
    }
}

bool CSystem::IsAssertDialogVisible() const
{
    return m_bIsAsserting;
}

void CSystem::SetAssertVisible(bool bAssertVisble)
{
    m_bIsAsserting = bAssertVisble;
}

bool CSystem::LoadFontInternal(IFFont*& font, const string& fontName)
{
    font = m_env.pCryFont->NewFont(fontName);
    if (!font)
    {
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Error creating the default fonts");
        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    string szFontPath = "Fonts/" + fontName + ".font";

    if (!font->Load(szFontPath.c_str()))
    {
        string szError = "Error loading the default font from ";
        szError += szFontPath;
        szError += ". You're probably running the executable from the wrong working folder.";
        CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, szError.c_str());

        return false;
    }

    return true;
}
