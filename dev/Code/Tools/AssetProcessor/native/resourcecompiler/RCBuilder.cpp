/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "RCBuilder.h"

#include <QElapsedTimer>
#include <QCoreApplication>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/parallel/atomic.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/Process/ProcessCommunicator.h>
#include <AzToolsFramework/Process/ProcessWatcher.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/resourcecompiler/rccontroller.h"

#include "native/utilities/assetUtils.h"
#include "native/utilities/AssetBuilderInfo.h"

#if defined (AZ_PLATFORM_WINDOWS)
#define LEGACY_RC_RELATIVE_PATH "/rc/rc.exe"    // Location of the legacy RC compiler relative to the BinXX folder the asset processor resides in
#elif defined (AZ_PLATFORM_APPLE_OSX)
#define LEGACY_RC_RELATIVE_PATH "/rc/rc"    // Location of the legacy RC compiler relative to the BinXX folder the asset processor resides in
#else
#error Unsupported Platform for RC
#endif



namespace AssetProcessor
{
    // Temporary solution to get around the fact that we don't have job dependencies
    static AZStd::atomic_bool s_TempSolution_CopyJobsFinished(false);
    static AZStd::atomic<int> s_TempSolution_CopyJobActivityCounter(0);

    static void TempSolution_TouchCopyJobActivity()
    {
        s_TempSolution_CopyJobActivityCounter++;
        s_TempSolution_CopyJobsFinished = false;
    }

    //! Special ini configuration keyword to mark a asset pattern for skipping
    const QString ASSET_PROCESSOR_CONFIG_KEYWORD_SKIP = "skip";

    //! Special ini configuration keyword to mark a asset pattern for copying
    const QString ASSET_PROCESSOR_CONFIG_KEYWORD_COPY = "copy";

#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_APPLE)
    // remove the above IFDEF as soon as the AzToolsFramework ProcessCommunicator functions on OSX, along with the rest of the similar IFDEFs in this file.

    //! CommunicatorTracePrinter listens to stderr and stdout of a running process and writes its output to the AZ_Trace system
    //! Importantly, it does not do any blocking operations.
    class CommunicatorTracePrinter
    {
    public:
        CommunicatorTracePrinter(AzToolsFramework::ProcessCommunicator* communicator)
            : m_communicator(communicator)
        {
            m_stringBeingConcatenated.reserve(1024);
        }

        ~CommunicatorTracePrinter()
        {
            WriteCurrentString();
        }

        // call this periodically to drain the buffers and write them.
        void Pump()
        {
            if (m_communicator->IsValid())
            {
                // Don't call readOutput unless there is output or else it will block...
                while (m_communicator->PeekOutput())
                {
                    AZ::u32 readSize = m_communicator->ReadOutput(m_streamBuffer, AZ_ARRAY_SIZE(m_streamBuffer));
                    ParseDataBuffer(readSize);
                }
                while (m_communicator->PeekError())
                {
                    AZ::u32 readSize = m_communicator->ReadError(m_streamBuffer, AZ_ARRAY_SIZE(m_streamBuffer));
                    ParseDataBuffer(readSize);
                }
            }
        }

        // drains the buffer into the string thats being built, then traces the string when it hits a newline.
        void ParseDataBuffer(AZ::u32 readSize)
        {
            if (readSize > AZ_ARRAY_SIZE(m_streamBuffer))
            {
                AZ_ErrorOnce("ERROR", false, "Programmer bug:  Read size is overflowing in traceprintf communicator.");
                return;
            }
            for (size_t pos = 0; pos < readSize; ++pos)
            {
                if ((m_streamBuffer[pos] == '\n') || (m_streamBuffer[pos] == '\r'))
                {
                    WriteCurrentString();
                }
                else
                {
                    m_stringBeingConcatenated.push_back(m_streamBuffer[pos]);
                }
            }
        }

        void WriteCurrentString()
        {
            if (!m_stringBeingConcatenated.empty())
            {
                AZ_TracePrintf("RC Builder", "%s", m_stringBeingConcatenated.c_str());
            }
            m_stringBeingConcatenated.clear();
        }


    private:
        AzToolsFramework::ProcessCommunicator* m_communicator = nullptr;
        char m_streamBuffer[128];
        AZStd::string m_stringBeingConcatenated;
    };

#endif // AZ_PLATFORM_WINDOWS || AZ_PLATFORM_APPLE

    namespace Internal
    {
        void PopulateCommonDescriptorParams(AssetBuilderSDK::JobDescriptor& descriptor, int platform, const AssetPlatformSpec& platformSpec, const InternalAssetRecognizer* const recognizer)
        {
            descriptor.m_jobKey = recognizer->m_name.toUtf8().data();
            descriptor.m_platform = platform;
            descriptor.m_priority = recognizer->m_priority;
            descriptor.m_checkExclusiveLock = recognizer->m_testLockSource;

            QString extraInformationForFingerprinting;
            extraInformationForFingerprinting.append(platformSpec.m_extraRCParams);
            extraInformationForFingerprinting.append(recognizer->m_version);

            // if we have specified the product asset type, changing it should cuase
            if (!recognizer->m_productAssetType.IsNull())
            {
                char typeAsString[64] = { 0 };
                recognizer->m_productAssetType.ToString(typeAsString, AZ_ARRAY_SIZE(typeAsString));
                extraInformationForFingerprinting.append(typeAsString);
            }

            descriptor.m_priority = recognizer->m_priority;

            descriptor.m_additionalFingerprintInfo = AZStd::string(extraInformationForFingerprinting.toUtf8().data());

            bool isCopyJob = (platformSpec.m_extraRCParams.compare(ASSET_PROCESSOR_CONFIG_KEYWORD_COPY) == 0);

            // Temporary solution to get around the fact that we don't have job dependencies
            if (isCopyJob)
            {
                TempSolution_TouchCopyJobActivity();
            }

            // If this is a copy job or critical is set to true in the ini file, then its a critical job
            descriptor.m_critical = recognizer->m_isCritical || isCopyJob;

            // If the priority of copy job is default then we update it to 1
            // This will ensure that copy jobs will be processed before other critical jobs having default priority
            if (isCopyJob && recognizer->m_priority == 0)
            {
                descriptor.m_priority = 1;
            }
        }
    }

    // don't make this too high, its basically how slowly the app responds to a job finishing.
    // this basically puts a hard cap on how many RC jobs can execute per second, since at 10ms per job (minimum), with 8 cores, thats a max
    // of 800 jobs per second that can possibly run.  However, the actual time it takes to launch RC.EXE is far, far longer than 10ms, so this is not a bad number for now...
    const int NativeLegacyRCCompiler::s_maxSleepTime = 10;

    // You have up to 60 minutes to finish processing an asset.
    // This was increased from 10 to account for PVRTC compression
    // taking up to an hour for large normal map textures, and should
    // be reduced again once we move to the ASTC compression format, or
    // find another solution to reduce processing times to be reasonable.
    const unsigned int NativeLegacyRCCompiler::s_jobMaximumWaitTime = 1000 * 60 * 60;

    NativeLegacyRCCompiler::Result::Result(int exitCode, bool crashed, const QString& outputDir)
        : m_exitCode(exitCode)
        , m_crashed(crashed)
        , m_outputDir(outputDir)
    {
    }

    NativeLegacyRCCompiler::NativeLegacyRCCompiler()
        : m_resourceCompilerInitialized(false)
        , m_systemRoot()
        , m_rcExecutableFullPath()
        , m_requestedQuit(false)
    {
    }

    NativeLegacyRCCompiler::~NativeLegacyRCCompiler()
    {
        AssetRegistryNotificationBus::Handler::BusDisconnect();
    }

    bool NativeLegacyRCCompiler::Initialize(const QString& systemRoot, const QString& rcExecutableFullPath)
    {
        // QFile::exists(normalizedPath)
        if (!QDir(systemRoot).exists())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, QString("Cannot locate system root dir %1").arg(systemRoot).toUtf8().data());
            return false;
        }

        AssetRegistryNotificationBus::Handler::BusConnect();

#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_APPLE)

        if (!AZ::IO::SystemFile::Exists(rcExecutableFullPath.toUtf8().data()))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, QString("Invalid executable path '%1'").arg(rcExecutableFullPath).toUtf8().data());
            return false;
        }
        this->m_systemRoot = systemRoot;
        this->m_rcExecutableFullPath = rcExecutableFullPath;
        this->m_resourceCompilerInitialized = true;
        return true;
#else // defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_APPLE)
        AZ_TracePrintf(AssetProcessor::DebugChannel, "There is no implementation for how to compile assets on this platform");
        return false;
#endif // defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_APPLE)
    }


    bool NativeLegacyRCCompiler::Execute(const QString& inputFile, const QString& watchFolder, int platformId, 
        const QString& params, const QString& dest, const AssetBuilderSDK::JobCancelListener* jobCancelListener, Result& result) const
    {
        QString platform = AssetUtilities::ComputePlatformName(platformId);

#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_APPLE)
        if (!this->m_resourceCompilerInitialized)
        {
            result.m_exitCode = JobExitCode_RCCouldNotBeLaunched;
            result.m_crashed = false;
            AZ_Warning("RC Builder", false, "RC Compiler has not been initialized before use.");
            return false;
        }

        // build the command line:
        QString commandString = NativeLegacyRCCompiler::BuildCommand(inputFile, watchFolder, platform, platformId, params, dest);

        AzToolsFramework::ProcessLauncher::ProcessLaunchInfo processLaunchInfo;

        // while it might be tempting to set the executable in processLaunchInfo.m_processExecutableString, it turns out that RC.EXE
        // won't work if you do that because it assumes the first command line param is the exe name, which is not the case if you do it that way...

        QString formatter("\"%1\" %2");
        processLaunchInfo.m_commandlineParameters = QString(formatter).arg(m_rcExecutableFullPath).arg(commandString).toUtf8().data();
        processLaunchInfo.m_showWindow = false;
        processLaunchInfo.m_workingDirectory = m_systemRoot.absolutePath().toUtf8().data();
        processLaunchInfo.m_processPriority = AzToolsFramework::PROCESSPRIORITY_IDLE;

        AZ_TracePrintf("RC Builder", "Executing RC.EXE: '%s' ...\n", processLaunchInfo.m_commandlineParameters.c_str());
        AZ_TracePrintf("Rc Builder", "Executing RC.EXE with working directory: '%s' ...\n", processLaunchInfo.m_workingDirectory.c_str());
        
        AzToolsFramework::ProcessWatcher* watcher = AzToolsFramework::ProcessWatcher::LaunchProcess(processLaunchInfo, AzToolsFramework::COMMUNICATOR_TYPE_STDINOUT);

        if (!watcher)
        {
            result.m_exitCode = JobExitCode_RCCouldNotBeLaunched;
            result.m_crashed = false;
            AZ_Error("RC Builder", false, "RC failed to execute\n");

            return false;
        }

        QElapsedTimer ticker;
        ticker.start();

        // it created the process, wait for it to exit:
        bool finishedOK = false;
        {
            CommunicatorTracePrinter tracer(watcher->GetCommunicator()); // allow this to go out of scope...
            while ((!m_requestedQuit) && (!finishedOK))
            {
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(NativeLegacyRCCompiler::s_maxSleepTime));

                tracer.Pump();

                if (ticker.elapsed() > s_jobMaximumWaitTime || (jobCancelListener && jobCancelListener->IsCancelled()))
                {
                    break;
                }

                AZ::u32 exitCode = 0;
                if (!watcher->IsProcessRunning(&exitCode))
                {
                    finishedOK = true; // we either cant wait for it, or it finished.
                    result.m_exitCode = exitCode;
                    result.m_crashed = (exitCode == 100) || (exitCode == 101); // these indicate fatal errors.
                    break;
                }
            }

            tracer.Pump(); // empty whats left if possible.
        }

        if (!finishedOK)
        {
            if (watcher->IsProcessRunning())
            {
                watcher->TerminateProcess(0xFFFFFFFF);
            }

            if (!this->m_requestedQuit)
            {
                if (jobCancelListener == nullptr || !jobCancelListener->IsCancelled())
                {
                    AZ_Error("RC Builder", false, "RC failed to complete within the maximum allowed time and was terminated. please see %s/rc_log.log for details", result.m_outputDir.toUtf8().data());
                }
                else
                {
                    AZ_TracePrintf("RC Builder", "RC was terminated. There was a request to cancel the job.\n");
                    result.m_exitCode = JobExitCode_JobCancelled;
                }
            }
            else
            {
                AZ_Warning("RC Builder", false, "RC terminated because the application is shutting down.\n");
                result.m_exitCode = JobExitCode_JobCancelled;
            }
            result.m_crashed = false;
        }
        AZ_TracePrintf("RC Builder", "RC.EXE execution has ended\n");

        delete watcher;

        return finishedOK;

#else // AZ_PLATFORM_WINDOWS || AZ_PLATFORM_APPLE
        result.m_exitCode = JobExitCode_RCCouldNotBeLaunched;
        result.m_crashed = false;
        AZ_Error("RC Builder", false, "There is no implementation for how to compile assets via RC on this platform");
        return false;
#endif // AZ_PLATFORM_WINDOWS || AZ_PLATFORM_APPLE
    }

    QString NativeLegacyRCCompiler::BuildCommand(const QString& inputFile, const QString& watchFolder, const QString& platform, 
        int platformId, const QString& params, const QString& dest)
    {
        QString cmdLine;
        if (!dest.isEmpty())
        {
            QDir engineRoot;
            AssetUtilities::ComputeEngineRoot(engineRoot);
            QString gameRoot = engineRoot.absoluteFilePath(AssetUtilities::ComputeGameName());
            QString gameName = AssetUtilities::ComputeGameName();

            int portNumber = 0;
            ApplicationServerBus::BroadcastResult(portNumber, &ApplicationServerBus::Events::GetServerListeningPort);

            cmdLine = QString("\"%1\" /p=%2 /pi=%9 %3 /unattended /threads=1 /gameroot=\"%4\" /watchfolder=\"%6\" /targetroot=\"%5\" /logprefix=\"%5/\" /port=%7 /gamesubdirectory=\"%8\"");
            cmdLine = cmdLine.arg(inputFile, platform, params, gameRoot, dest, watchFolder).arg(portNumber).arg(gameName).arg(platformId);
        }
        else
        {
            cmdLine = QString("\"%1\" /p=%2 /pi=%4 %3 /threads=1").arg(inputFile, platform, params).arg(platformId);
        }
        return cmdLine;
    }

    void NativeLegacyRCCompiler::RequestQuit()
    {
        this->m_requestedQuit = true;
    }

    BuilderIdAndName::BuilderIdAndName(QString builderName, QString builderId, Type type, QString rcParam /*=QString("")*/)
        : m_builderName(builderName)
        , m_builderId(builderId)
        , m_type(type)
        , m_rcParam(rcParam)
    {
    }
    BuilderIdAndName& BuilderIdAndName::operator=(const AssetProcessor::BuilderIdAndName& src)
    {
        this->m_builderId = src.m_builderId;
        this->m_builderName = src.m_builderName;
        this->m_type = src.m_type;
        this->m_rcParam = src.m_rcParam;
        return *this;
    }

    const QString& BuilderIdAndName::GetName() const
    {
        return this->m_builderName;
    }

    bool BuilderIdAndName::GetUuid(AZ::Uuid& builderUuid) const
    {
        if (this->m_type == Type::REGISTERED_BUILDER)
        {
            builderUuid = AZ::Uuid::CreateString(this->m_builderId.toUtf8().data());
            return true;
        }
        else
        {
            return false;
        }
    }

    const QString& BuilderIdAndName::GetRcParam() const
    {
        return this->m_rcParam;
    }

    const QString& BuilderIdAndName::GetId() const
    {
        return this->m_builderId;
    }

    const BuilderIdAndName::Type BuilderIdAndName::GetType() const
    {
        return this->m_type;
    }

    const char* INTERNAL_BUILDER_UUID_STR = "589BE398-2EBB-4E3C-BE66-C894E34C944D";
    const BuilderIdAndName  BUILDER_ID_COPY("Internal Copy Builder", "31B74BFD-7046-47AC-A7DA-7D5167E9B2F8", BuilderIdAndName::Type::REGISTERED_BUILDER, ASSET_PROCESSOR_CONFIG_KEYWORD_COPY);
    const BuilderIdAndName  BUILDER_ID_RC  ("Internal RC Builder",   "0BBFC8C1-9137-4404-BD94-64C0364EFBFB", BuilderIdAndName::Type::REGISTERED_BUILDER);
    const BuilderIdAndName  BUILDER_ID_SKIP("Internal Skip Builder", "A033AF24-5041-4E24-ACEC-161A2E522BB6", BuilderIdAndName::Type::UNREGISTERED_BUILDER, ASSET_PROCESSOR_CONFIG_KEYWORD_SKIP);

    const QHash<QString, BuilderIdAndName> ALL_INTERNAL_BUILDER_BY_ID =
    {
        { BUILDER_ID_COPY.GetId(), BUILDER_ID_COPY },
        { BUILDER_ID_RC.GetId(), BUILDER_ID_RC },
        { BUILDER_ID_SKIP.GetId(), BUILDER_ID_SKIP }
    };

    InternalAssetRecognizer::InternalAssetRecognizer(const AssetRecognizer& src, const QString& builderId, const QHash<QString, AssetPlatformSpec>& assetPlatformSpecByPlatform)
        : AssetRecognizer(src.m_name, src.m_testLockSource, src.m_priority, src.m_isCritical, src.m_supportsCreateJobs, src.m_patternMatcher, src.m_version, src.m_productAssetType)
        , m_builderId(builderId)
    {
        for (auto iterSrcPlatformSpec = assetPlatformSpecByPlatform.begin();
             iterSrcPlatformSpec != assetPlatformSpecByPlatform.end();
             iterSrcPlatformSpec++)
        {
            int convertedSrcPlatformFlag = AssetUtilities::ComputePlatformFlag(iterSrcPlatformSpec.key());
            this->m_platformSpecsByPlatform[convertedSrcPlatformFlag] = *iterSrcPlatformSpec;
        }
        this->m_paramID = CalculateCRC();
    }

    AZ::u32 InternalAssetRecognizer::CalculateCRC() const
    {
        AZ::Crc32 crc;

        crc.Add(m_name.toUtf8().data());
        crc.Add(m_builderId.toUtf8().data());
        crc.Add(const_cast<void*>(static_cast<const void*>(&m_testLockSource)), sizeof(m_testLockSource));
        crc.Add(const_cast<void*>(static_cast<const void*>(&m_priority)), sizeof(m_priority));
        crc.Add(m_patternMatcher.GetBuilderPattern().m_pattern.c_str());
        crc.Add(const_cast<void*>(static_cast<const void*>(&m_patternMatcher.GetBuilderPattern().m_type)), sizeof(m_patternMatcher.GetBuilderPattern().m_type));
        return static_cast<AZ::u32>(crc);
    }

    //! Constructor to initialize the internal builders and a general internal builder uuid that is used for bus 
    //! registration.  This constructor is helpful for deriving other classes from this builder for purposes like
    //! unit testing.
    InternalRecognizerBasedBuilder::InternalRecognizerBasedBuilder(QHash<QString, BuilderIdAndName> inputBuilderByIdMap, AZ::Uuid internalBuilderUuid)
        : m_isShuttingDown(false)
        , m_rcCompiler(new NativeLegacyRCCompiler())
        , m_internalRecognizerBuilderUuid(internalBuilderUuid)
    {
        for (BuilderIdAndName builder : inputBuilderByIdMap.values())
        {
            m_builderById[builder.GetId()] = inputBuilderByIdMap[builder.GetId()];
        }
        AssetBuilderSDK::AssetBuilderCommandBus::Handler::BusConnect(m_internalRecognizerBuilderUuid);
    }

    //! Constructor to initialize the internal based builder to a present set of internal builders and fixed bus id
    InternalRecognizerBasedBuilder::InternalRecognizerBasedBuilder()
        : InternalRecognizerBasedBuilder(ALL_INTERNAL_BUILDER_BY_ID, AZ::Uuid::CreateString(INTERNAL_BUILDER_UUID_STR))
    {
    }

    InternalRecognizerBasedBuilder::~InternalRecognizerBasedBuilder()
    {
        AssetBuilderSDK::AssetBuilderCommandBus::Handler::BusDisconnect(m_internalRecognizerBuilderUuid);
        for (auto assetRecognizer : m_assetRecognizerDictionary)
        {
            delete assetRecognizer;
        }
    }

    AssetBuilderSDK::AssetBuilderDesc InternalRecognizerBasedBuilder::CreateBuilderDesc(const QString& builderId, const AZStd::vector<AssetBuilderSDK::AssetBuilderPattern>& builderPatterns)
    {
        const BuilderIdAndName& builder = m_builderById[builderId];

        AssetBuilderSDK::AssetBuilderDesc   builderDesc;
        builderDesc.m_name = builder.GetName().toUtf8().data();
        builderDesc.m_patterns = builderPatterns;

        // Only set a bus id on the descriptor if the builder is a registered builder
        AZ::Uuid busId;
        if (builder.GetUuid(busId))
        {
            builderDesc.m_busId = AZ::Uuid::CreateString(builderId.toUtf8().data());
        }

        builderDesc.m_createJobFunction = AZStd::bind(&InternalRecognizerBasedBuilder::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDesc.m_processJobFunction = AZStd::bind(&InternalRecognizerBasedBuilder::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        return builderDesc;
    }

    void InternalRecognizerBasedBuilder::ShutDown()
    {
        m_isShuttingDown = true;
        this->m_rcCompiler->RequestQuit();
    }

    bool InternalRecognizerBasedBuilder::Initialize(const RecognizerConfiguration& recognizerConfig)
    {
        InitializeAssetRecognizers(recognizerConfig.GetAssetRecognizerContainer());

        QDir systemRoot;
        bool computeRootResult = AssetUtilities::ComputeEngineRoot(systemRoot);
        AZ_Assert(computeRootResult, "AssetUtilities::ComputeEngineRoot failed");

        QString rcExecutableFullPath = QCoreApplication::applicationDirPath() + LEGACY_RC_RELATIVE_PATH;

        if (!m_rcCompiler->Initialize(systemRoot.canonicalPath(), rcExecutableFullPath))
        {
            AssetBuilderSDK::BuilderLog(m_internalRecognizerBuilderUuid, "Unable to find rc.exe from the engine root (%1).", rcExecutableFullPath.toUtf8().data());
            return false;
        }
        return true;
    }


    void InternalRecognizerBasedBuilder::InitializeAssetRecognizers(const RecognizerContainer& assetRecognizers)
    {
        // Split the asset recognizers that were scanned in into 'buckets' for each of the 3 builder ids based on
        // either the custom fixed rc params or the standard rc param ('copy','skip', or others)
        QHash<QString, InternalAssetRecognizerList> internalRecognizerListByType;
        InternalRecognizerBasedBuilder::BuildInternalAssetRecognizersByType(assetRecognizers, internalRecognizerListByType);

        for (auto internalRecognizerList = internalRecognizerListByType.begin();
             internalRecognizerList != internalRecognizerListByType.end();
             internalRecognizerList++)
        {
            QString builderId = internalRecognizerList.key();
            const BuilderIdAndName& builderInfo = m_builderById[builderId];
            QString builderName = builderInfo.GetName();
            AZStd::vector<AssetBuilderSDK::AssetBuilderPattern> builderPatterns;

            for (auto internalAssetRecognizer : * internalRecognizerList)
            {
                if (internalAssetRecognizer->m_platformSpecsByPlatform.size() == 0)
                {
                    delete internalAssetRecognizer;
                    AZ_Warning(AssetProcessor::DebugChannel, "Skipping recognizer %s, no platforms supported\n", builderName.toUtf8().data());
                    continue;
                }

                // Ignore duplicate recognizers
                if (m_assetRecognizerDictionary.contains(internalAssetRecognizer->m_paramID))
                {
                    delete internalAssetRecognizer;
                    AZ_Warning(AssetProcessor::DebugChannel, false, "Ignoring duplicate asset recognizer in configuration: %s\n", builderName.toUtf8().data());
                    continue;
                }

                // Register the recognizer
                builderPatterns.push_back(internalAssetRecognizer->m_patternMatcher.GetBuilderPattern());
                m_assetRecognizerDictionary[internalAssetRecognizer->m_paramID] = internalAssetRecognizer;
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Registering %s as a %s\n", internalAssetRecognizer->m_name.toUtf8().data(),
                    builderName.toUtf8().data());
            }
            // Register the builder desc if its registrable
            if (builderInfo.GetType() == BuilderIdAndName::Type::REGISTERED_BUILDER)
            {
                AssetBuilderSDK::AssetBuilderDesc builderDesc = CreateBuilderDesc(builderId, builderPatterns);
                EBUS_EVENT(AssetBuilderSDK::AssetBuilderBus, RegisterBuilderInformation, builderDesc);
            }
        }
    }

    void InternalRecognizerBasedBuilder::UnInitialize()
    {
        for (BuilderIdAndName builder: m_builderById.values())
        {
            AZ::Uuid builderUuid;
            // Register the builder desc if its registrable
            if ((builder.GetType() == BuilderIdAndName::Type::REGISTERED_BUILDER) && (builder.GetUuid(builderUuid)))
            {
                EBUS_EVENT(AssetBuilderRegistrationBus, UnRegisterBuilderDescriptor, builderUuid);
            }
        }
    }

    bool InternalRecognizerBasedBuilder::GetMatchingRecognizers(int platformFlags, const QString fileName, InternalRecognizerPointerContainer& output) const
    {
        AZ_Assert(fileName.contains('\\') == false, "fileName must not contain backslashes: %s", fileName.toUtf8().constData());

        bool foundAny = false;
        for (const InternalAssetRecognizer* recognizer : m_assetRecognizerDictionary)
        {
            if (recognizer->m_patternMatcher.MatchesPath(fileName))
            {
                bool matchPlatform = false;
                // found a match, now match the platform
                for (auto iterPlatformKeys = recognizer->m_platformSpecsByPlatform.keyBegin();
                     iterPlatformKeys != recognizer->m_platformSpecsByPlatform.keyEnd();
                     iterPlatformKeys++)
                {
                    if (platformFlags & (*iterPlatformKeys))
                    {
                        matchPlatform = true;
                        break;
                    }
                }
                if (matchPlatform)
                {
                    output.push_back(recognizer);
                }
                foundAny = true;
            }
        }
        return foundAny;
    }

    void InternalRecognizerBasedBuilder::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        // Convert the incoming builder id (AZ::Uuid) to the equivalent GUID from the asset recognizers
        AZStd::string azBuilderId;
        request.m_builderid.ToString(azBuilderId,false);
        QString requestedBuilderID = QString(azBuilderId.c_str());

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
        
        QDir watchFolder(request.m_watchFolder.c_str());
        QString normalizedPath = watchFolder.absoluteFilePath(request.m_sourceFile.c_str());
        normalizedPath = AssetUtilities::NormalizeFilePath(normalizedPath);
        
        // Locate recognizers that match the file
        InternalRecognizerPointerContainer  recognizers;
        if (!GetMatchingRecognizers(request.m_platformFlags, normalizedPath, recognizers))
        {
            AssetBuilderSDK::BuilderLog(m_internalRecognizerBuilderUuid, "Cannot find recognizer for %s.", request.m_sourceFile.c_str());
            return;
        }

        // First pass
        for (const InternalAssetRecognizer* recognizer : recognizers)
        {
            if (recognizer->m_supportsCreateJobs)
            {
                // The recognizer's builder id must match the job requests' builder id
                if (recognizer->m_builderId.compare(requestedBuilderID) != 0)
                {
                    continue;
                }

                AssetBuilderSDK::CreateJobsResponse rcResponse;

                CreateLegacyRCJob(request, "", rcResponse);

                if (rcResponse.m_result != AssetBuilderSDK::CreateJobsResultCode::Success)
                {
                    // Error is already printed out by CreateLegacyRCJob
                    continue;
                }

                for (auto& descriptor : rcResponse.m_createJobOutputs)
                {
                    descriptor.m_jobParameters[recognizer->m_paramID] = descriptor.m_jobKey;
                }

                // Move-append the response outputs
                response.m_createJobOutputs.reserve(response.m_createJobOutputs.size() + rcResponse.m_createJobOutputs.size());
                response.m_createJobOutputs.reserve(response.m_sourceFileDependencyList.size() + rcResponse.m_sourceFileDependencyList.size());

                AZStd::move(rcResponse.m_createJobOutputs.begin(), rcResponse.m_createJobOutputs.end(), AZStd::back_inserter(response.m_createJobOutputs));
                AZStd::move(rcResponse.m_sourceFileDependencyList.begin(), rcResponse.m_sourceFileDependencyList.end(), AZStd::back_inserter(response.m_sourceFileDependencyList));

                response.m_result = rcResponse.m_result;
            }
            else
            {
                bool skippedByPlatform = false;

                // Iterate through the platform specific specs and apply the ones that match the platform flag
                for (auto iterPlatformSpec = recognizer->m_platformSpecsByPlatform.cbegin();
                    iterPlatformSpec != recognizer->m_platformSpecsByPlatform.cend();
                    iterPlatformSpec++)
                {
                    if (iterPlatformSpec.key() & request.m_platformFlags)
                    {
                        QString rcParam = iterPlatformSpec.value().m_extraRCParams;

                        // Check if this is the 'skip' parameter
                        if (rcParam.compare(ASSET_PROCESSOR_CONFIG_KEYWORD_SKIP) == 0)
                        {
                            skippedByPlatform = true;
                        }
                        // The recognizer's builder id must match the job requests' builder id
                        else if (recognizer->m_builderId.compare(requestedBuilderID) == 0)
                        {
                            AssetBuilderSDK::JobDescriptor descriptor;
                            Internal::PopulateCommonDescriptorParams(descriptor, iterPlatformSpec.key(), iterPlatformSpec.value(), recognizer);
                            // Job Parameter Value can be any arbitrary string since we are relying on the key to lookup
                            // the parameter in the process job
                            descriptor.m_jobParameters[recognizer->m_paramID] = descriptor.m_jobKey;

                            response.m_createJobOutputs.push_back(descriptor);
                            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
                        }
                    }
                }

                // Adjust response if we did not get any jobs, but one or more platforms were marked as skipped
                if ((response.m_result == AssetBuilderSDK::CreateJobsResultCode::Failed) && (skippedByPlatform))
                {
                    response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
                }
            }
        }
    }


    void InternalRecognizerBasedBuilder::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        AssetBuilderSDK::JobCancelListener jobCancelListener(request.m_jobId);
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;

        if (request.m_jobDescription.m_jobParameters.empty())
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                "Job request for %s in builder %s missing job parameters.",
                request.m_sourceFile.c_str(),
                BUILDER_ID_RC.GetId().toUtf8().data());

            return;
        }

        for (auto jobParam = request.m_jobDescription.m_jobParameters.begin();
             jobParam != request.m_jobDescription.m_jobParameters.end();
             jobParam++)
        {
            if (jobCancelListener.IsCancelled())
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                return;
            }

            if (this->m_assetRecognizerDictionary.find(jobParam->first) == this->m_assetRecognizerDictionary.end())
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel,
                    "Job request for %s in builder %s has invalid job parameter (%ld).",
                    request.m_sourceFile.c_str(),
                    BUILDER_ID_RC.GetId().toUtf8().data(),
                    jobParam->first);
                continue;
            }
            InternalAssetRecognizer* assetRecognizer = this->m_assetRecognizerDictionary[jobParam->first];
            if (!assetRecognizer->m_platformSpecsByPlatform.contains(request.m_jobDescription.m_platform))
            {
                // Skip due to platform restrictions
                continue;
            }
            QString rcParam = assetRecognizer->m_platformSpecsByPlatform[request.m_jobDescription.m_platform].m_extraRCParams;

            //
            if (rcParam.compare(ASSET_PROCESSOR_CONFIG_KEYWORD_COPY) == 0)
            {
                ProcessCopyJob(request, assetRecognizer->m_productAssetType, jobCancelListener, response);
            }
            else if (rcParam.compare(ASSET_PROCESSOR_CONFIG_KEYWORD_SKIP) == 0)
            {
                // This should not occur because 'skipped' jobs should not be processed
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Job ID %lld Failed, encountered an invalid 'skip' parameter during job processing", AssetProcessor::GetThreadLocalJobId());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            }
            else
            {
                ProcessLegacyRCJob(request, rcParam, assetRecognizer->m_productAssetType, jobCancelListener, response);
            }

            if (jobCancelListener.IsCancelled())
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }

            if (response.m_resultCode != AssetBuilderSDK::ProcessJobResult_Success)
            {
                // If anything other than a success occurred, break out of the loop and report the failed job
                return;
            }

        }
    }

    void InternalRecognizerBasedBuilder::CreateLegacyRCJob(const AssetBuilderSDK::CreateJobsRequest& request, QString rcParam, AssetBuilderSDK::CreateJobsResponse& response)
    {
        const char* requestFileName = "createjobsRequest.xml";
        const char* responseFileName = "createjobsResponse.xml";
        const char* createJobsParam = "/createjobs";

        QDir watchDir(request.m_watchFolder.c_str());
        auto normalizedPath = watchDir.absoluteFilePath(request.m_sourceFile.c_str());

        QString workFolder;

        if (!AssetUtilities::CreateTempWorkspace(workFolder))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Failed to create temporary workspace");
            return;
        }

        QString watchFolder = request.m_watchFolder.c_str();
        NativeLegacyRCCompiler::Result  rcResult;

        QDir workDir(workFolder);
        QString requestPath = workDir.absoluteFilePath(requestFileName);
        QString responsePath = workDir.absoluteFilePath(responseFileName);

        if (!AZ::Utils::SaveObjectToFile(requestPath.toStdString().c_str(), AZ::DataStream::ST_XML, &request))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Failed to write CreateJobsRequest to file %s", requestPath.toStdString().c_str());
            return;
        }

        QString params = QString("%1=\"%2\"").arg(createJobsParam).arg(requestPath);

        //Platform and platform id are hard coded to PC because it doesn't matter, the actual platform info is in the CreateJobsRequest
        if ((!this->m_rcCompiler->Execute(normalizedPath, watchFolder, AssetBuilderSDK::Platform_PC, rcParam.append(params), workFolder, nullptr, rcResult)) || (rcResult.m_exitCode != 0))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Job ID %lld Failed with exit code %d\n", AssetProcessor::GetThreadLocalJobId(), rcResult.m_exitCode);
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
            return;
        }

        if (AZ::Utils::LoadObjectFromFileInPlace(responsePath.toStdString().c_str(), response))
        {
            workDir.removeRecursively();
        }
    }

    void InternalRecognizerBasedBuilder::ProcessLegacyRCJob(const AssetBuilderSDK::ProcessJobRequest& request, QString rcParam, AZ::Uuid productAssetType, const AssetBuilderSDK::JobCancelListener& jobCancelListener, AssetBuilderSDK::ProcessJobResponse& response)
    {
        // Process this job
        QString inputFile = QString(request.m_fullPath.c_str());
        int platformId = request.m_jobDescription.m_platform;
        QString dest = request.m_tempDirPath.c_str();
        QString watchFolder = request.m_watchFolder.c_str();
        NativeLegacyRCCompiler::Result  rcResult;

        // Temporary solution to get around the fact that we don't have job dependencies
        if (!s_TempSolution_CopyJobsFinished)
        {
            int copyJobActivityCounter = 0;

            do {
                copyJobActivityCounter = s_TempSolution_CopyJobActivityCounter;
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(1000));
            } while (copyJobActivityCounter != s_TempSolution_CopyJobActivityCounter);

            // always wait for a registry save here
            {
                AssetUtilities::AssetRegistryListener listener;
                listener.WaitForSync();
            }

            s_TempSolution_CopyJobsFinished = true;
        }

        if ((!this->m_rcCompiler->Execute(inputFile, watchFolder, platformId, rcParam, dest, &jobCancelListener, rcResult)) || (rcResult.m_exitCode != 0))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Job ID %lld Failed with exit code %d\n", AssetProcessor::GetThreadLocalJobId(), rcResult.m_exitCode);
            if (jobCancelListener.IsCancelled())
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }
            else
            {
                response.m_resultCode = rcResult.m_crashed ? AssetBuilderSDK::ProcessJobResult_Crashed : AssetBuilderSDK::ProcessJobResult_Failed;
            }
            return;
        }

        // Get all of the files from the dest folder
        QFileInfoList   originalFiles = GetFilesInDirectory(dest);
        QFileInfoList   filteredFiles;

        // Filter out the log files and add to the result products
        AZStd::unordered_set<AZ::u32> m_alreadyAssignedSubIDs;

        bool hasSubIdCollision = false;

        for (const auto& file : originalFiles)
        {
            if (jobCancelListener.IsCancelled())
            {
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                return;
            }

            QString outputFilename = file.fileName();
            if (MatchTempFileToSkip(outputFilename))
            {
                AZ_TracePrintf("RC Builder", "RC created temporary file: (%s), ignoring.\n", file.absoluteFilePath().toUtf8().data());
                continue;
            }

            // this kind of job can output multiple products.
            // we are going to generate SUBIds for them if they collide, here!
            // ideally, the builder SDK builder written for this asset type would deal with it.

            AZ_TracePrintf("RC Builder", "RC created product file: (%s).\n", file.absoluteFilePath().toUtf8().data());
            response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(AZStd::string(file.absoluteFilePath().toUtf8().data()), productAssetType));
            hasSubIdCollision |= (m_alreadyAssignedSubIDs.insert(response.m_outputProducts.back().m_productSubID).second == false); // insert returns pair<iter, bool> where the bool is false if it was already there.
        }

        // now fix any subid collisions, but only if we have an actual collision.
        if (hasSubIdCollision)
        {
            m_alreadyAssignedSubIDs.clear();
            for (AssetBuilderSDK::JobProduct& product : response.m_outputProducts)
            {
                if (jobCancelListener.IsCancelled())
                {
                    response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                    return;
                }

                AZ_TracePrintf("RC Builder", "SubId collision detected for product file: (%s).\n", product.m_productFileName.c_str());
                AZ::u32 seedValue = 0;
                while (m_alreadyAssignedSubIDs.find(product.m_productSubID) != m_alreadyAssignedSubIDs.end())
                {
                    // its already in!  pick another one.  For now, lets pick something based on the name so that ordering doesn't mess it up
                    QFileInfo productFileInfo(product.m_productFileName.c_str());
                    AZ::u32 fullCRC = AZ::Crc32(productFileInfo.fileName().toUtf8().data());
                    AZ::u32 maskedCRC = (fullCRC + seedValue) & AssetBuilderSDK::SUBID_MASK_ID;

                    // preserve the LOD and the other flags, but replace the CRC:
                    product.m_productSubID = AssetBuilderSDK::ConstructSubID(maskedCRC, AssetBuilderSDK::GetSubID_LOD(product.m_productSubID), product.m_productSubID);
                    ++seedValue;
                }

                m_alreadyAssignedSubIDs.insert(product.m_productSubID);
            }
        }

        // its fine for RC to decide there are no outputs.  The only factor is what its exit code is.
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }

    void InternalRecognizerBasedBuilder::ProcessCopyJob(const AssetBuilderSDK::ProcessJobRequest& request, AZ::Uuid productAssetType, const AssetBuilderSDK::JobCancelListener& jobCancelListener, AssetBuilderSDK::ProcessJobResponse& response)
    {
        response.m_outputProducts.push_back(AssetBuilderSDK::JobProduct(request.m_fullPath, productAssetType));
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;

        if (jobCancelListener.IsCancelled())
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }
        // Temporary solution to get around the fact that we don't have job dependencies
        TempSolution_TouchCopyJobActivity();
    }

    QFileInfoList InternalRecognizerBasedBuilder::GetFilesInDirectory(const QString& directoryPath)
    {
        QDir            workingDir(directoryPath);
        QFileInfoList   filesInDir(workingDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files));
        return filesInDir;
    }


    bool InternalRecognizerBasedBuilder::MatchTempFileToSkip(const QString& outputFilename)
    {
        // List of specific files to skip
        static const char* s_fileNamesToSkip[] = {
            "rc_createdfiles.txt",
            "rc_log.log",
            "rc_log_warnings.log",
            "rc_log_errors.log"
        };

        for (const char* filenameToSkip : s_fileNamesToSkip)
        {
            if (QString::compare(outputFilename, filenameToSkip, Qt::CaseInsensitive) == 0)
            {
                return true;
            }
        }

        // List of specific file name patters to skip
        static const QString s_filePatternsToSkip[] = {
            QString(".*\\.\\$.*"),
            QString("log.*\\.txt")
        };

        for (const QString& patternsToSkip : s_filePatternsToSkip)
        {
            QRegExp skipRegex(patternsToSkip, Qt::CaseInsensitive, QRegExp::RegExp);
            if (skipRegex.exactMatch(outputFilename))
            {
                return true;
            }
        }

        return false;
    }


    void InternalRecognizerBasedBuilder::RegisterInternalAssetRecognizerToMap(const AssetRecognizer& assetRecognizer, const QString& builderId, QHash<QString, AssetPlatformSpec>& sourceAssetPlatformSpecs, QHash<QString, InternalAssetRecognizerList>& internalRecognizerListByType)
    {
        InternalAssetRecognizer* newAssetRecognizer = new InternalAssetRecognizer(assetRecognizer, builderId, sourceAssetPlatformSpecs);
        internalRecognizerListByType[builderId].push_back(newAssetRecognizer);
    }

    void InternalRecognizerBasedBuilder::BuildInternalAssetRecognizersByType(const RecognizerContainer& assetRecognizers, QHash<QString, InternalAssetRecognizerList>& internalRecognizerListByType)
    {
        // Go through each asset recognizer's platform specs to determine which type bucket to create and put the converted internal
        // assert recognizer into
        for (const AssetRecognizer& assetRecognizer : assetRecognizers)
        {
            QHash<QString, AssetPlatformSpec>   copyAssetPlatformSpecs;
            QHash<QString, AssetPlatformSpec>   skipAssetPlatformSpecs;
            QHash<QString, AssetPlatformSpec>   rcAssetPlatformSpecs;

            // Go through the global asset recognizers and split them by operation keywords if they exist or by the main rc param
            for (auto iterSrcPlatformSpec = assetRecognizer.m_platformSpecs.begin();
                 iterSrcPlatformSpec != assetRecognizer.m_platformSpecs.end();
                 iterSrcPlatformSpec++)
            {
                if (iterSrcPlatformSpec->m_extraRCParams.compare(BUILDER_ID_COPY.GetRcParam()) == 0)
                {
                    copyAssetPlatformSpecs[iterSrcPlatformSpec.key()] = iterSrcPlatformSpec.value();
                }
                else if (iterSrcPlatformSpec->m_extraRCParams.compare(BUILDER_ID_SKIP.GetRcParam()) == 0)
                {
                    skipAssetPlatformSpecs[iterSrcPlatformSpec.key()] = iterSrcPlatformSpec.value();
                }
                else
                {
                    rcAssetPlatformSpecs[iterSrcPlatformSpec.key()] = iterSrcPlatformSpec.value();
                }
            }

            // Create separate internal asset recognizers based on whether or not they were detected
            if (copyAssetPlatformSpecs.size() > 0)
            {
                RegisterInternalAssetRecognizerToMap(assetRecognizer, BUILDER_ID_COPY.GetId(), copyAssetPlatformSpecs, internalRecognizerListByType);
            }
            if (skipAssetPlatformSpecs.size() > 0)
            {
                RegisterInternalAssetRecognizerToMap(assetRecognizer, BUILDER_ID_SKIP.GetId(), skipAssetPlatformSpecs, internalRecognizerListByType);
            }
            if (rcAssetPlatformSpecs.size() > 0)
            {
                RegisterInternalAssetRecognizerToMap(assetRecognizer, BUILDER_ID_RC.GetId(), rcAssetPlatformSpecs, internalRecognizerListByType);
            }
        }
    }
}

