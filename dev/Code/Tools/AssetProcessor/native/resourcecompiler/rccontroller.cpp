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

#include "rccontroller.h"
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QtMath> // for qCeil
#include <QCoreApplication>
#include <QThreadPool>

#include "native/utilities/AssetUtils.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/resourcecompiler/RCCommon.h"

#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AzCore/Casting/numeric_cast.h>

namespace AssetProcessor
{
    RCController::RCController(int cfg_minJobs, int cfg_maxJobs, QObject* parent)
        : QObject(parent)
        , m_dispatchingJobs(false)
        , m_shuttingDown(false)
    {
        AssetProcessorPlatformBus::Handler::BusConnect();

        // Determine a good starting value for max jobs
        int maxJobs = QThread::idealThreadCount();
        if (maxJobs == -1)
        {
            maxJobs = 3;
        }

        maxJobs = qMax<int>(maxJobs - 1, 1);

        // if the user has specified max jobs in the cfg file, then we obey their request
        // regardless of whether they have chosen something bad or not - they would have had to explicitly
        // pick this value (we ship with default 0 meaning auto), so if they've changed it, they intend it that way
        m_maxJobs = cfg_maxJobs ? qMax(cfg_minJobs, cfg_maxJobs) :  maxJobs;

        m_RCQueueSortModel.AttachToModel(&m_RCJobListModel);

        // make sure that the global thread pool has enough slots to accomidate your request though, since 
        // by default, the global thread pool has idealThreadCount() slots only.
        // leave an extra slot for non-job work.
        int currentMaxThreadCount = QThreadPool::globalInstance()->maxThreadCount();
        int newMaxThreadCount = qMax<int>(currentMaxThreadCount, m_maxJobs + 1);
        QThreadPool::globalInstance()->setMaxThreadCount(newMaxThreadCount);

        QObject::connect(this, &RCController::EscalateJobs, &m_RCQueueSortModel, &AssetProcessor::RCQueueSortModel::OnEscalateJobs);
    }

    RCController::~RCController()
    {
        AssetProcessorPlatformBus::Handler::BusDisconnect();
        m_RCQueueSortModel.AttachToModel(nullptr);
    }

    RCJobListModel* RCController::GetQueueModel()
    {
        return &m_RCJobListModel;
    }

    void RCController::StartJob(RCJob* rcJob)
    {
        Q_ASSERT(rcJob);
        // request to be notified when job is done
        QObject::connect(rcJob, &RCJob::Finished, this, [this, rcJob]()
            {
            FinishJob(rcJob);
            }, Qt::QueuedConnection);

        QObject::connect(rcJob, &RCJob::BeginWork, this, [this, rcJob]()
        {
            m_RCJobListModel.markAsStarted(rcJob);
        }, Qt::QueuedConnection);

        // Mark as "being processed" by moving to Processing list
        m_RCJobListModel.markAsProcessing(rcJob);
        Q_EMIT JobStatusChanged(rcJob->GetJobEntry(), AzToolsFramework::AssetSystem::JobStatus::InProgress);
        rcJob->Start();
        Q_EMIT JobStarted(rcJob->GetInputFileRelativePath(), rcJob->GetPlatform());
    }

    void RCController::QuitRequested()
    {
        m_shuttingDown = true;

        if (m_RCJobListModel.jobsInFlight() == 0)
        {
            Q_EMIT ReadyToQuit(this);
            return;
        }

        QTimer::singleShot(10, this, SLOT(QuitRequested()));
    }

    int RCController::NumberOfPendingCriticalJobsPerPlatform(QString platform)
    {
        return m_pendingCriticalJobsPerPlatform[platform.toLower()];
    }

    void RCController::FinishJob(RCJob* rcJob)
    {
        m_RCQueueSortModel.RemoveJobIdEntry(rcJob);
        QString platform = rcJob->GetPlatform();
        auto found = m_jobsCountPerPlatform.find(platform);
        if (found != m_jobsCountPerPlatform.end())
        {
            int prevCount = found.value();
            if (prevCount > 0)
            {
                int newCount = prevCount - 1;
                m_jobsCountPerPlatform[platform] = newCount;
                Q_EMIT JobsInQueuePerPlatform(platform, newCount);
            }
        }

        CheckCompileAssetsGroup(rcJob->GetElementID(), rcJob->GetState());

        if (rcJob->IsCritical())
        {
            int criticalJobsCount = m_pendingCriticalJobsPerPlatform[platform.toLower()] - 1;
            m_pendingCriticalJobsPerPlatform[platform.toLower()] = criticalJobsCount;
        }

        if (rcJob->GetState() == RCJob::cancelled)
        {
            Q_EMIT FileCancelled(rcJob->GetJobEntry());
        }
        else if (rcJob->GetState() != RCJob::completed)
        {
            Q_EMIT FileFailed(rcJob->GetJobEntry());
        }
        else
        {
            Q_EMIT FileCompiled(rcJob->GetJobEntry(), AZStd::move(rcJob->GetProcessJobResponse()));
        }

        // Move to Completed list which will mark as "completed"
        // unless a different state has been set.
        m_RCJobListModel.markAsCompleted(rcJob);

        Q_EMIT ActiveJobsCountChanged(aznumeric_cast<unsigned int>(m_RCJobListModel.itemCount() - m_RCJobListModel.FailedJobsCount()));

        if (!m_shuttingDown)
        {
            // Start next job only if we are not shutting down
            DispatchJobs();

            // if there is no next job, and nothing is in flight, we are done.
            if (IsIdle())
            {
                Q_EMIT BecameIdle();
            }
        }
    }

    bool RCController::IsIdle()
    {
        return ((!m_RCQueueSortModel.GetNextPendingJob()) && (m_RCJobListModel.jobsInFlight() == 0));
    }

    void RCController::JobSubmitted(JobDetails details)
    {
        AssetProcessor::QueueElementID checkFile(details.m_jobEntry.m_relativePathToFile, details.m_jobEntry.m_platform, details.m_jobEntry.m_jobKey);

        if (m_RCJobListModel.isInQueue(checkFile))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Job is already in queue - ignored [%s, %s, %s]\n", checkFile.GetInputAssetName().toUtf8().data(), checkFile.GetPlatform().toUtf8().data(), checkFile.GetJobDescriptor().toUtf8().data());
            return;
        }

        if (m_RCJobListModel.isInFlight(checkFile))
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Cancelling Job [%s, %s, %s]\n", checkFile.GetInputAssetName().toUtf8().data(), checkFile.GetPlatform().toUtf8().data(), checkFile.GetJobDescriptor().toUtf8().data());
            int existingJobIndex = m_RCJobListModel.GetIndexOfProcessingJob(checkFile);
            if (existingJobIndex != -1)
            {
                RCJob* job = m_RCJobListModel.getItem(existingJobIndex);
                job->SetState(RCJob::JobState::cancelled);
                AssetBuilderSDK::JobCommandBus::Event(job->GetJobEntry().m_jobRunKey, &AssetBuilderSDK::JobCommandBus::Events::Cancel);
                m_RCJobListModel.UpdateRow(existingJobIndex);
            }
        }

        RCJob* rcJob = new RCJob(&m_RCJobListModel);
        rcJob->Init(details); // note - move operation.  From this point on you must use the job details to refer to it.

        m_RCQueueSortModel.AddJobIdEntry(rcJob);
        m_RCJobListModel.addNewJob(rcJob);
        QString platformName = rcJob->GetPlatform();// we need to get the actual platform from the rcJob
        if (rcJob->IsCritical())
        {
            int criticalJobsCount = m_pendingCriticalJobsPerPlatform[platformName.toLower()] + 1;
            m_pendingCriticalJobsPerPlatform[platformName.toLower()] = criticalJobsCount;
        }
        auto found = m_jobsCountPerPlatform.find(platformName);
        if (found != m_jobsCountPerPlatform.end())
        {
            int newCount = found.value() + 1;
            m_jobsCountPerPlatform[platformName] = newCount;
        }
        else
        {
            m_jobsCountPerPlatform[platformName] = 1;
        }
        Q_EMIT JobsInQueuePerPlatform(platformName, m_jobsCountPerPlatform[platformName]);
        Q_EMIT JobStatusChanged(rcJob->GetJobEntry(), AzToolsFramework::AssetSystem::JobStatus::Queued);

        if (!m_dispatchingPaused)
        {
            Q_EMIT ActiveJobsCountChanged(aznumeric_cast<unsigned int>(m_RCJobListModel.itemCount() - m_RCJobListModel.FailedJobsCount()));
        }

        // Start the job we just received if no job currently running
        if ((!m_shuttingDown) && (!m_dispatchingJobs))
        {
            QMetaObject::invokeMethod(this, "DispatchJobs", Qt::QueuedConnection);
        }
    }

    void RCController::SetDispatchPaused(bool pause)
    {
        if (m_dispatchingPaused != pause)
        {
            m_dispatchingPaused = pause;
            if (!pause)
            {
                if ((!m_shuttingDown) && (!m_dispatchingJobs))
                {
                    QMetaObject::invokeMethod(this, "DispatchJobs", Qt::QueuedConnection);
                    Q_EMIT ActiveJobsCountChanged(aznumeric_cast<unsigned int>(m_RCJobListModel.itemCount() - m_RCJobListModel.FailedJobsCount()));
                }
            }
        }
    }

    void RCController::DispatchJobs()
    {
        if (m_dispatchingPaused)
        {
            return;
        }
        if (!m_dispatchingJobs)
        {
            m_dispatchingJobs = true;
            RCJob* rcJob = m_RCQueueSortModel.GetNextPendingJob();

            while (m_RCJobListModel.jobsInFlight() < m_maxJobs && rcJob && !m_shuttingDown)
            {
                StartJob(rcJob);
                rcJob = m_RCQueueSortModel.GetNextPendingJob();
            }
            m_dispatchingJobs = false;
        }
    }

    void RCController::OnRequestCompileGroup(AssetProcessor::NetworkRequestID groupID, QString platform, QString searchTerm, bool isStatusRequest)
    {
        // someone has asked for a compile group to be created that conforms to that search term.
        // the goal here is to use a heuristic to find any assets that match the search term and place them in a new group
        // then respond with the appropriate response.

        // lets do some minimal processing on the search term
        AssetProcessor::JobIdEscalationList escalationList;
        QSet<AssetProcessor::QueueElementID> results;
        m_RCJobListModel.PerformHeuristicSearch(AssetUtilities::NormalizeAndRemoveAlias(searchTerm), platform, results, escalationList, isStatusRequest);

        if (results.isEmpty())
        {
            // nothing found
            Q_EMIT CompileGroupCreated(groupID, AzFramework::AssetSystem::AssetStatus_Unknown);
        }
        else
        {
            m_RCQueueSortModel.OnEscalateJobs(escalationList);
            
            m_activeCompileGroups.push_back(AssetCompileGroup());
            m_activeCompileGroups.back().m_groupMembers.swap(results);
            m_activeCompileGroups.back().m_requestID = groupID;

            Q_EMIT CompileGroupCreated(groupID, AzFramework::AssetSystem::AssetStatus_Queued);
        }
    }

    void RCController::CheckCompileAssetsGroup(const AssetProcessor::QueueElementID& queuedElement, RCJob::JobState state)
    {
        if (m_activeCompileGroups.empty())
        {
            return;
        }

        // start at the end so that we can actually erase the compile groups and not skip any:
        for (int groupIdx = m_activeCompileGroups.size() - 1; groupIdx >= 0; --groupIdx)
        {
            AssetCompileGroup& compileGroup = m_activeCompileGroups[groupIdx];
            auto it = compileGroup.m_groupMembers.find(queuedElement);
            if (it != compileGroup.m_groupMembers.end())
            {
                compileGroup.m_groupMembers.erase(it);
                if ((compileGroup.m_groupMembers.isEmpty()) || (state != RCJob::completed))
                {
                    // if we get here, we're either empty (and succeeded) or we failed one and have now failed
                    Q_EMIT CompileGroupFinished(compileGroup.m_requestID, state != RCJob::completed ? AzFramework::AssetSystem::AssetStatus_Failed : AzFramework::AssetSystem::AssetStatus_Compiled);
                    m_activeCompileGroups.removeAt(groupIdx);
                }
            }
        }
    }
} // Namespace AssetProcessor

#include <native/resourcecompiler/rccontroller.moc>
