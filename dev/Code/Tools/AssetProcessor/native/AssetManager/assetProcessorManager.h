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

#pragma once

#include <QString>
#include <QByteArray>
#include <QQueue>
#include <QVector>
#include <QHash>
#include <QDir>
#include <QSet>
#include <QMap>
#include <QPair>
#include <QMutex>

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/containers/unordered_map.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/assetprocessor.h"
#include "native/utilities/assetUtilEBusHelper.h"
#include "native/utilities/ThreadHelper.h"
#include "native/AssetManager/AssetCatalog.h"
#include "native/AssetDatabase/AssetDatabase.h"
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/map.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzCore/IO/SystemFile.h> // for AZ_MAX_PATH_LEN

class FileWatcher;

namespace AzFramework
{
    namespace AssetSystem
    {
        class BaseAssetProcessorMessage;

        class GetRelativeProductPathFromFullSourceOrProductPathRequest;
        class GetRelativeProductPathFromFullSourceOrProductPathResponse;

        class GetFullSourcePathFromRelativeProductPathRequest;
        class GetFullSourcePathFromRelativeProductPathResponse;
        class AssetNotificationMessage;
    } // namespace AssetSystem
} // namespace AzFramework

namespace AzToolsFramework
{
    namespace AssetSystem
    {
        class AssetJobLogRequest;
        class AssetJobLogResponse;

        class AssetJobsInfoRequest;
        class AssetJobsInfoResponse;
    } // namespace AssetSystem
} // namespace AzToolsFramework

namespace AssetProcessor
{
    class AssetProcessingStateData;
    struct AssetRecognizer;
    class PlatformConfiguration;
    class ScanFolderInfo;

    //! The Asset Processor Manager is the heart of the pipeline
    //! It is what makes the critical decisions about what should and should not be processed
    //! It emits signals when jobs need to be performed and when assets are complete or have failed.
    class AssetProcessorManager
        : public QObject
        , public AssetProcessor::ProcessingJobInfoBus::Handler
        , public AzToolsFramework::AssetSystemRequestBus::Handler
    {
        using BaseAssetProcessorMessage = AzFramework::AssetSystem::BaseAssetProcessorMessage;
        using AssetJobsInfoRequest = AzToolsFramework::AssetSystem::AssetJobsInfoRequest;
        using AssetJobsInfoResponse = AzToolsFramework::AssetSystem::AssetJobsInfoResponse;
        using JobInfo = AzToolsFramework::AssetSystem::JobInfo;
        using JobStatus = AzToolsFramework::AssetSystem::JobStatus;
        using AssetJobLogRequest = AzToolsFramework::AssetSystem::AssetJobLogRequest;
        using AssetJobLogResponse = AzToolsFramework::AssetSystem::AssetJobLogResponse;
        using GetRelativeProductPathFromFullSourceOrProductPathRequest = AzFramework::AssetSystem::GetRelativeProductPathFromFullSourceOrProductPathRequest;
        using GetRelativeProductPathFromFullSourceOrProductPathResponse = AzFramework::AssetSystem::GetRelativeProductPathFromFullSourceOrProductPathResponse;
        using GetFullSourcePathFromRelativeProductPathRequest = AzFramework::AssetSystem::GetFullSourcePathFromRelativeProductPathRequest;
        using GetFullSourcePathFromRelativeProductPathResponse = AzFramework::AssetSystem::GetFullSourcePathFromRelativeProductPathResponse;

        Q_OBJECT

    private:
        struct FileEntry
        {
            QString m_fileName;
            bool m_isDelete = false;

            FileEntry(const QString& fileName = QString(), bool isDelete = false)
                : m_fileName(fileName)
                , m_isDelete(isDelete)
            {
            }

            FileEntry(const FileEntry& rhs)
                : m_fileName(rhs.m_fileName)
                , m_isDelete(rhs.m_isDelete)
            {
            }
        };

        struct AssetProcessedEntry
        {
            JobEntry m_entry;
            AssetBuilderSDK::ProcessJobResponse m_response;

            AssetProcessedEntry() = default;
            AssetProcessedEntry(JobEntry& entry, AssetBuilderSDK::ProcessJobResponse& response)
                : m_entry(AZStd::move(entry))
                , m_response(AZStd::move(response))
            {
            }

            AssetProcessedEntry(const AssetProcessedEntry& other) = default;
            AssetProcessedEntry(AssetProcessedEntry&& other)
                : m_entry(AZStd::move(other.m_entry))
                , m_response(AZStd::move(other.m_response))
            {
            }

            AssetProcessedEntry& operator=(AssetProcessedEntry&& other)
            {
                if (this != &other)
                {
                    m_entry = AZStd::move(other.m_entry);
                    m_response = AZStd::move(other.m_response);
                }
                return *this;
            }
        };

        //! Internal structure that will hold all the necessary source info
        struct SourceFileInfo
        {
            QString m_relativePath;
            const ScanFolderInfo* m_scanFolder;
        };

    public:
        explicit AssetProcessorManager(AssetProcessor::PlatformConfiguration* config, QObject* parent = 0);
        virtual ~AssetProcessorManager();
        bool IsIdle();
        bool HasProcessedCriticalAssets() const;

        //////////////////////////////////////////////////////////////////////////
        // ProcessingJobInfoBus::Handler overrides
        void BeginIgnoringCacheFileDelete(const AZStd::string productPath) override;
        void StopIgnoringCacheFileDelete(const AZStd::string productPath, bool queueAgainForProcessing) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        //AzToolsFramework::AssetSystem::AssetSystemRequestBus::Handler overrides
        const char* GetAbsoluteDevGameFolderPath() override;
        const char* GetAbsoluteDevRootFolderPath() override;
        bool GetRelativeProductPathFromFullSourceOrProductPath(const AZStd::string& fullPath, AZStd::string& relativeProductPath) override;
        bool GetFullSourcePathFromRelativeProductPath(const AZStd::string& relPath, AZStd::string& fullSourcePath) override;
        void UpdateQueuedEvents() override;
        bool GetSourceAssetInfoById(const AZ::Uuid& guid, AZStd::string& watchFolder, AZStd::string& relativePath) override;
        ////////////////////////////////////////////////////////////////////////////////

        AZStd::shared_ptr<AssetDatabaseConnection> GetDatabaseConnection() const;

        //! Internal structure that will hold all the necessary information to process jobs later.
        //! We need to hold these jobs because they have declared source dependency on other sources and
        //! we can only resolve these dependencies once all the create jobs are completed.
        struct JobToProcessEntry
        {
            SourceFileInfo m_sourceFileInfo;
            AZStd::vector<JobDetails> m_jobsToAnalyze;
        };

Q_SIGNALS:
        void NumRemainingJobsChanged(int newNumJobs);

        void AssetToProcess(JobDetails jobDetails);

        //! Emit whenever a new asset is found or an existing asset is updated
        void AssetMessage(QString platform, AzFramework::AssetSystem::AssetNotificationMessage message);
        
        // InputAssetProcessed - uses absolute asset path of input file.
        void InputAssetProcessed(QString fullAssetPath, QString platform);

        void RequestInputAssetStatus(QString inputAssetPath, QString platform, QString jobDescription);
        void RequestPriorityAssetCompile(QString inputAssetPath, QString platform, QString jobDescription);

        //! AssetProcessorManagerIdleState is emitted when APM idle state changes, we emit true when 
        //! APM is waiting for outside stimulus i.e its has eaten through all of its queues and is only waiting for
        //! responses back from other systems (like its waiting for responses back from the compiler)
        void AssetProcessorManagerIdleState(bool state);
        void ReadyToQuit(QObject* source);

        void CreateAssetsRequest(unsigned int nonce, QString name, QString platform, bool onlyExactMatch = true, bool syncRequest = false);

        void SendAssetExistsResponse(NetworkRequestID groupID, bool exists);

        void FenceFileDetected(unsigned int fenceId);

        void EscalateJobs(AssetProcessor::JobIdEscalationList jobIdEscalationList);

    public Q_SLOTS:
        void AssetProcessed(JobEntry jobEntry, AssetBuilderSDK::ProcessJobResponse response);
        void AssetProcessed_Impl();

        void AssetFailed(JobEntry jobEntry);
        void AssetCancelled(JobEntry jobEntry);

        void AssessModifiedFile(QString filePath);
        void AssessAddedFile(QString filePath);
        void AssessDeletedFile(QString filePath);
        void OnAssetScannerStatusChange(AssetProcessor::AssetScanningStatus status);
        void OnJobStatusChanged(JobEntry jobEntry, JobStatus status);
        void CheckAssetProcessorIdleState();

        void QuitRequested();

        //! A network request came in asking, for a given input asset, what the status is of any jobs related to that request
        void ProcessGetAssetJobsInfoRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed = false);

        //! A network request came in, Given a JOB ID (from the above Job Request), asking for the actual log for that job.
        void ProcessGetAssetJobLogRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed = false);

        // given some absolute path, please respond with its relative product path.  For now, this will be a
        // string like 'textures/blah.tif' (we don't care about extensions), but eventually, this will
        // be an actual asset UUID.
        void ProcessGetRelativeProductPathFromFullSourceOrProductPathRequest(const AZStd::string& fullPath, AZStd::string& relativeProductPath);

        // This function helps in determining the full product path of an relative product path.
        //In the future we will be sending an asset UUID to this function to request for full path.
        void ProcessGetFullSourcePathFromRelativeProductPathRequest(const AZStd::string& relPath, AZStd::string& fullSourcePath);

        //! This request comes in and is expected to do whatever heuristic is required in order to determine if an asset actually exists in the database.
        void OnRequestAssetExists(NetworkRequestID requestId, QString platform, QString searchTerm);

        //! Searches the product and source asset tables to try and find a match
        QString GuessProductOrSourceAssetName(QString searchTerm, bool useLikeSearch);

        void RequestReady(NetworkRequestID requestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed = false);

        void ProcessFilesToExamineQueue();
        void CheckForIdle();
        void CheckMissingFiles();
        void ProcessGetAssetJobsInfoRequest(AssetJobsInfoRequest& request, AssetJobsInfoResponse& response);
        void ProcessGetAssetJobLogRequest(const AssetJobLogRequest& request, AssetJobLogResponse& response);
        void ScheduleNextUpdate();

    private:
        template <class R>
        bool Recv(unsigned int connId, QByteArray payload, R& request);
        void AssessFileInternal(QString fullFile, bool isDelete);
        void CheckSource(const FileEntry& source);
        void CheckMissingJobs(QString relativeSourceFile, const AZStd::vector<JobDetails>& jobsThisTime);
        void CheckDeletedProductFile(QString normalizedPath);
        void CheckDeletedSourceFile(QString normalizedPath, QString relativeSourceFile);
        void CheckModifiedSourceFile(QString normalizedPath, QString relativeSourceFile);
        bool AnalyzeJob(JobDetails& details, const ScanFolderInfo* scanFolder, bool& sentSourceFileChangedMessage);
        void CheckDeletedCacheFolder(QString normalizedPath);
        void CheckDeletedSourceFolder(QString normalizedPath, QString relativePath, QString scanFolderPath);
        void CheckCreatedSourceFolder(QString normalizedPath);
        void CheckMetaDataRealFiles(QString relativePath);
        bool DeleteProducts(const AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer& products);
        void DispatchFileChange();
        bool InitializeCacheRoot();
        //! Given a job details structure, computes what files contribute to the final fingerprint. This includes the original file, 
        //! and its metafile(s), as well its dependencies (but not their metafiles)
        void PopulateFilesForFingerprinting(JobDetails& jobDetails);

        // given a file name and a root to not go beyond, add the parent folder and its parent folders recursively
        // to the list of known folders.
        void AddKnownFoldersRecursivelyForFile(QString file, QString root);
        void CleanEmptyFoldersForFile(QString file, QString root);

        QString GuessProductNameInDatabase(QString path);

        void ProcessBuilders(QString normalizedPath, QString relativePathToFile, const ScanFolderInfo* scanFolder, const AssetProcessor::BuilderInfoList& builderInfoList);

        void ProcessJobs(QString relativePathToFile, AZStd::vector<JobDetails>& jobsToAnalyze, const ScanFolderInfo* scanFolder);
        
        //! Search the database and the the source dependency maps for the the sourceUuid. if found returns the relative source path 
        bool SearchSourceBySourceUUID(AZ::Uuid sourceUuid, QString& relSourcePath);
        
        //!  Adds the source to the database and returns the corresponding sourceDatabase Entry
        void AddSourceToDatabase(AzToolsFramework::AssetDatabase::SourceDatabaseEntry& sourceDatabaseEntry, const ScanFolderInfo* scanFolder, QString relativeSourceFilePath);
        //!  Queries the database and populates databaseSourceFileDependencies with all the source file dependency entries present in the database which match the given source file and builderguid.  
        void GetSourceDependenciesFromDatabase(AzToolsFramework::AssetDatabase::SourceFileDependencyEntryContainer& newSourceFileDependencies, AzToolsFramework::AssetDatabase::SourceFileDependencyEntryContainer& databaseSourceFileDependencies);
        //!  Compares source file entries from the database to the source file entries which got emitted to determine which entries should be deleted and added to the database.
        //!  Entries that should be added to the database will be contained in newSourceFileDependencies and the entries that should be removed will be contained in databaseSourceFileDependencies containers
        void CompareEmittedSourceDependenciesFromDatabase(AzToolsFramework::AssetDatabase::SourceFileDependencyEntryContainer& newSourceFileDependencies, AzToolsFramework::AssetDatabase::SourceFileDependencyEntryContainer& databaseSourceFileDependencies);

        //! Populates the complete source file dependency list for the (builderUuid, sourcefile) key
        //! Since this method is recursive therefore if we find a source file dependency it will add source file dependencies of that file as well
        void PopulateSourceDependencyList(JobDetails& jobDetail, QString relSourceFilePath, AZStd::unordered_set<AZStd::string>& sourceFileDependencyNameList);

    protected:
        AZ::s64 GenerateNewJobRunKey();
        // Attempt to erase a log file.  Failing to erase it is not a critical problem, but should be logged.
        // returns true if there is no log file there after this operation completes
        bool EraseLogFile(const char* fileName);

        // Load the old scan folders and match them up with new scan folders.  Make sure they're
        bool MigrateScanFolders();

        //! Checks whether the AP is aware of any source file that has indicated the inputted 
        //! source file as its dependency, and if found do we need to put that file back in the asset pipeline queue again  
        QStringList CheckSourceFileDependency(const QString& sourcePath);

        //! Resolves every known source file dependency uuid's to source names
        void UpdateSourceFileDependencyInfo();

        //! Updates the database with all the changes related to source dependency
        void UpdateSourceFileDependencyDatabase();
        //! Analyze JobDetail for every hold jobs
        void AnalyzeJobDetail(JobToProcessEntry& jobEntry);

        void ProcessCreateJobsResponse(AssetBuilderSDK::CreateJobsResponse& createJobsResponse, const AssetBuilderSDK::CreateJobsRequest& createJobsRequest);

        AssetProcessor::PlatformConfiguration* m_platformConfig = nullptr;

        bool m_queuedExamination = false;
        bool m_hasProcessedCriticalAssets = false;

        QQueue<FileEntry> m_activeFiles;
        QSet<QString> m_alreadyActiveFiles; // a simple optimization to only do the exhaustive search if we know its there.
        AZStd::vector<AssetProcessedEntry> m_assetProcessedList;
        AZStd::shared_ptr<AssetDatabaseConnection> m_stateData;
        ThreadController<AssetCatalog>* m_assetCatalog;
        typedef QHash<QString, FileEntry> FileExamineContainer;
        FileExamineContainer m_filesToExamine; // order does not actually matter in this (yet)
        
        //! This map contains all source files are are not relevant anymore, the key is the absolute path of the file 
        //! and the value is the relative path stored in the db
        QMap<QString, QString> m_SourceFilesInDatabase;
        QSet<QString> m_knownFolders; // a cache of all known folder names, normalized to have forward slashes.

        typedef AZStd::unordered_map<AZ::u64, AzToolsFramework::AssetSystem::JobInfo> JobRunKeyToJobInfoMap;  // for when network requests come in about the jobInfo

        JobRunKeyToJobInfoMap m_jobRunKeyToJobInfoMap;
        AZStd::multimap<AZStd::string, AZ::u64> m_jobKeyToJobRunKeyMap;

        struct SourceInfo
        {
            QString m_watchFolder;
            QString m_sourceName;
        };

        using SourceUUIDToSourceNameMap = AZStd::unordered_map<AZ::Uuid, SourceInfo>;
        SourceUUIDToSourceNameMap m_sourceUUIDToSourceNameMap;

        AZStd::mutex m_sourceUUIDToSourceNameMapMutex;

        //! This map will contain all those sources that are related to source dependency
        //! Importantly It also contain uuids of those sources that create no jobs
        AZStd::unordered_map<AZ::Uuid, QString> m_sourceDependencyUUIDToSourceNameMap;

        QString m_normalizedCacheRootPath;
        char m_absoluteDevFolderPath[AZ_MAX_PATH_LEN];
        char m_absoluteDevGameFolderPath[AZ_MAX_PATH_LEN];
        QStringList m_commandLinePlatformsList;
        QDir m_cacheRootDir;
        bool m_isCurrentlyScanning = false;
        bool m_quitRequested = false;
        bool m_processedQueued = false;
        bool m_AssetProcessorIsBusy = false;
        int m_platformFlags = 0;
        bool m_alreadyScheduledUpdate = false;
        QMutex m_processingJobMutex;
        AZStd::unordered_set<AZStd::string> m_processingProductInfoList;
        AZ::s64 m_highestJobRunKeySoFar = 0;
        AZStd::vector<JobToProcessEntry> m_jobsToProcessLater;
        AZStd::multimap<AZStd::string, AssetProcessor::SourceFileDependencyInternal> m_sourceFileDependencyInfoMap;
        //! List of all scanfolders that are present in the database but not currently watched by AP
        AZStd::unordered_map<AZStd::string, AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry> m_scanFoldersInDatabase;
        AZStd::multimap<QString, QString> m_dependsOnSourceToSourceMap; //multimap since different source files can declare dependency on the same file
        AZStd::multimap <AZ::Uuid, QString > m_dependsOnSourceUuidToSourceMap; //multimap since different source files can declare dependency on the same file
        bool m_SourceDependencyInfoNeedsUpdate = false;
    };
} // namespace AssetProcessor

