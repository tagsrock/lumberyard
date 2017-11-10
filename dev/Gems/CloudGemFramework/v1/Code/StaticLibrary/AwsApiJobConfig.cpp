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
#include "StdAfx.h"

#include <CloudGemFramework/AwsApiJobConfig.h>
#include <CloudGemFramework/CloudGemFrameworkBus.h>
#include <aws/core/client/ClientConfiguration.h>

namespace CloudGemFramework
{

    void AwsApiJobConfig::ApplySettings()
    {

        m_jobContext = nullptr;

        Visit<AwsApiJobConfig>(
            [this](const AwsApiJobConfig& config) { 
                CheckAndSet(config.jobContext, m_jobContext); 
            }
        );

        if(!m_jobContext)
        {
            EBUS_EVENT_RESULT(m_jobContext, CloudGemFrameworkRequestBus, GetDefaultJobContext);
        }

        if(!credentialsProvider)
        {
            EBUS_EVENT_RESULT(credentialsProvider, CloudGemFrameworkRequestBus, GetPlayerCredentialsProvider);
        }
        
        m_settingsApplied = true;

    }

    Aws::Client::ClientConfiguration AwsApiJobConfig::GetClientConfiguration() const
    {
        Aws::Client::ClientConfiguration target;
        Visit<AwsApiJobConfig>(
            [&target](const AwsApiJobConfig& config){
                CheckAndSet(config.userAgent, target.userAgent);
                CheckAndSet(config.scheme, target.scheme);
                CheckAndSet(config.region, target.region);
                CheckAndSet(config.maxConnections, target.maxConnections);
                CheckAndSet(config.requestTimeoutMs, target.requestTimeoutMs);
                CheckAndSet(config.connectTimeoutMs, target.connectTimeoutMs);
                CheckAndSet(config.retryStrategy, target.retryStrategy);
                CheckAndSet(config.endpointOverride, target.endpointOverride);
                CheckAndSet(config.proxyHost, target.proxyHost);
                CheckAndSet(config.proxyPort, target.proxyPort);
                CheckAndSet(config.proxyUserName, target.proxyUserName);
                CheckAndSet(config.proxyPassword, target.proxyPassword);
                CheckAndSet(config.executor, target.executor);
                CheckAndSet(config.verifySSL, target.verifySSL);
                CheckAndSet(config.writeRateLimiter, target.writeRateLimiter);
                CheckAndSet(config.readRateLimiter, target.readRateLimiter);
                CheckAndSet(config.httpLibOverride, target.httpLibOverride);
                CheckAndSet(config.followRedirects, target.followRedirects);
                CheckAndSet(config.caFile, target.caFile);
            }
        );
        return target;
    }

    std::shared_ptr<Aws::Auth::AWSCredentialsProvider> AwsApiJobConfig::GetCredentialsProvider() const
    {
        std::shared_ptr<Aws::Auth::AWSCredentialsProvider> target;
        Visit<AwsApiJobConfig>([&target](const AwsApiJobConfig& config){ CheckAndSet(config.credentialsProvider, target); });
        return target;
    }

} // namespace CloudGemFramework

