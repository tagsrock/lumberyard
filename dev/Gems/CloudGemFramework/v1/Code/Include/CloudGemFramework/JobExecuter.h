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

#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Jobs/JobFunction.h>

#include <aws/core/utils/threading/Executor.h>

namespace CloudGemFramework
{

    /// AZ:Job type used by the JobExecuter. It is used call the callback 
    /// function provided by the AWS SDK.
    using ExecuterJob = AZ::JobFunction<std::function<void()>>;

    /// This class provides a simple alturnative to using the the AwsRequestJob, 
    /// AwsApiClientJob, or AwsApiJob classes. Those classes provide configuration 
    /// management and more abstracted usage patterns. With JobExecutor you need
    /// to do all the configuration management and work directly with the AWS API.
    /// 
    /// An AWS API async executer that uses the AZ::Job system to make AWS service calls.
    /// To use, set the Aws::Client::ClientConfiguration executor field so it points to
    /// an instance of this class, then use that client configuration object when creating
    /// AWS service client objects. This will cause the ...Async APIs on the AWS service 
    /// client object to use the AZ::Job system to execute the request.
    class JobExecuter
        : public Aws::Utils::Threading::Executor
    {

    public:

        /// Initialize a JobExecuter object.
        ///
        /// \param context - The JobContext that will be used to execute the jobs created
        /// by the JobExecuter.
        ///
        /// By default the global JobContext is used. However, the AWS SDK currently 
        /// only supports blocking calls, so, to avoid impacting other jobs, it is 
        /// recommended that you create a JobContext with a JobManager dedicated to 
        /// dedicated to processing these jobs. This context can also be used with 
        /// AwsApiCore::HttpJob.
        JobExecuter(AZ::JobContext* context)
            : m_context{context}
        {
        }

    protected:

        AZ::JobContext* m_context;

        /// Called by the AWS SDK to queue a callback for execution.
        bool SubmitToThread(std::function<void()>&& callback) override
        {
            ExecuterJob* job = aznew ExecuterJob(callback, true, m_context);
            job->Start();
        }

    };

} // namespace CloudGemFramework

