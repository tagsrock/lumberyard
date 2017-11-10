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

#include <CloudGemFramework/AwsApiJobConfig.h>

namespace CloudGemFramework
{

    /// Provides configuration needed by service jobs.
    class IServiceJobConfig
        : public virtual IAwsApiJobConfig
    {

    public:

        virtual std::shared_ptr<Aws::Utils::RateLimits::RateLimiterInterface> GetReadRateLimiter() = 0;
        virtual std::shared_ptr<Aws::Utils::RateLimits::RateLimiterInterface> GetWriteRateLimiter() = 0;
        virtual std::shared_ptr<Aws::Http::HttpClient> GetHttpClient() = 0;
        virtual const Aws::String& GetUserAgent() = 0;

    };

    #ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable: 4250 )
    // warning C4250: 'CloudGemFramework::ServiceJobConfig' : inherits 'CloudGemFramework::AwsApiJobConfig::CloudGemFramework::AwsApiJobConfig::GetJobContext' via dominance
    // This is the expected and desired behavior. The warning is superfluous.
    // http://stackoverflow.com/questions/11965596/diamond-inheritance-scenario-compiles-fine-in-g-but-produces-warnings-errors
    #endif

    /// Provides service job configuration using settings properties.
    class ServiceJobConfig
        : public AwsApiJobConfig
        , public virtual IServiceJobConfig
    {

    public:

        AZ_CLASS_ALLOCATOR(ServiceJobConfig, AZ::SystemAllocator, 0);

        using InitializerFunction = AZStd::function<void(ServiceJobConfig& config)>;

        /// Initialize an ServiceJobConfig object.
        ///
        /// \param defaultConfig - the config object that provivdes valules when
        /// no override has been set in this object. The default is nullptr, which
        /// will cause a default value to be used.
        ///
        /// \param initializer - a function called to initialize this object.
        /// This simplifies the initialization of static instances. The default
        /// value is nullptr, in which case no initializer will be called.
        ServiceJobConfig(AwsApiJobConfig* defaultConfig = nullptr, InitializerFunction initializer = nullptr)
            : AwsApiJobConfig{defaultConfig}
        {
            if(initializer)
            {
                initializer(*this);
            }
        }
   
        std::shared_ptr<Aws::Utils::RateLimits::RateLimiterInterface> GetReadRateLimiter() override
        {
            EnsureSettingsApplied();
            return m_readRateLimiter;
        }
        
        std::shared_ptr<Aws::Utils::RateLimits::RateLimiterInterface> GetWriteRateLimiter() override
        {
            EnsureSettingsApplied();
            return m_writeRateLimiter;
        }

        std::shared_ptr<Aws::Http::HttpClient> GetHttpClient() override
        {
            EnsureSettingsApplied();
            return m_httpClient;
        }

        const Aws::String& GetUserAgent() override
        {
            EnsureSettingsApplied();
            return m_userAgent;
        }

        void ApplySettings() override;

    private:

        std::shared_ptr<Aws::Utils::RateLimits::RateLimiterInterface> m_readRateLimiter{nullptr};
        std::shared_ptr<Aws::Utils::RateLimits::RateLimiterInterface> m_writeRateLimiter{nullptr};
        std::shared_ptr<Aws::Http::HttpClient> m_httpClient{nullptr};
        Aws::String m_userAgent{};

    };

    #ifdef _MSC_VER 
    #pragma warning( pop ) // C4250
    #endif

}
