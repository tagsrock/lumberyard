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

#include <CloudGemFramework/ServiceClientJobConfig.h>

namespace CloudGemFramework
{

    class IServiceRequestJobConfig 
        : public virtual IServiceClientJobConfig
    {

    public:

        virtual const Aws::String& GetRequestUrl() = 0;
        virtual std::shared_ptr<Aws::Auth::AWSCredentialsProvider> GetCredentialsProvider() = 0;

        virtual bool IsValid() const = 0;
    };

    #ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable: 4250 )
    // warning C4250: 'CloudGemFramework::ServiceRequestJobConfig<RequestType>' : inherits 'CloudGemFramework::AwsApiJobConfig::CloudGemFramework::AwsApiJobConfig::GetJobContext' via dominance
    // This is the expected and desired behavior. The warning is superfluous.
    // http://stackoverflow.com/questions/11965596/diamond-inheritance-scenario-compiles-fine-in-g-but-produces-warnings-errors
    #endif

    template<class RequestType>
    class ServiceRequestJobConfig 
        : public ServiceClientJobConfig<typename RequestType::ServiceTraits>
        , public virtual IServiceRequestJobConfig
    {

    public:

        AZ_CLASS_ALLOCATOR(ServiceRequestJobConfig, AZ::SystemAllocator, 0);

        using InitializerFunction = AZStd::function<void(ServiceClientJobConfig<typename RequestType::ServiceTraits>& config)>;
        using ServiceClientJobConfigType = ServiceClientJobConfig<typename RequestType::ServiceTraits>;

        /// Initialize an AwsApiClientJobConfig object.
        ///
        /// \param DefaultConfigType - the type of the config object from which
        /// default values will be taken.
        ///
        /// \param defaultConfig - the config object that provivdes valules when
        /// no override has been set in this object. The default is nullptr, which
        /// will cause a default value to be used.
        ///
        /// \param initializer - a function called to initialize this object.
        /// This simplifies the initialization of static instances. The default
        /// value is nullptr, in which case no initializer will be called.
        ServiceRequestJobConfig(AwsApiJobConfig* defaultConfig = nullptr, InitializerFunction initializer = nullptr)
            : ServiceClientJobConfigType{ defaultConfig }
        {
            if(initializer)
            {
                initializer(*this);
            }
        }

        const Aws::String& GetRequestUrl() override
        {
            ServiceClientJobConfigType::EnsureSettingsApplied();
            return m_requestUrl;
        }

        bool IsValid() const override
        {
            // If we failed to get mappings we'll have no URL and should not try to make a request
            return (m_requestUrl.length() > 0);
        }
   
        std::shared_ptr<Aws::Auth::AWSCredentialsProvider> GetCredentialsProvider()
        {
            ServiceClientJobConfigType::EnsureSettingsApplied();
            return m_credentialsProvider;
        }

        void ApplySettings() override
        {
            ServiceClientJobConfigType::ApplySettings();

            m_requestUrl = GetServiceUrl().c_str();
            if (m_requestUrl.length())
            {
                m_requestUrl += RequestType::Path();
            }

            m_credentialsProvider = AwsApiJobConfig::GetCredentialsProvider();

        }

    private:

        Aws::String m_requestUrl;
        std::shared_ptr<Aws::Auth::AWSCredentialsProvider> m_credentialsProvider;

    };

    #ifdef _MSC_VER 
    #pragma warning( pop ) // C4250
    #endif

} // namespace CloudGemFramework
