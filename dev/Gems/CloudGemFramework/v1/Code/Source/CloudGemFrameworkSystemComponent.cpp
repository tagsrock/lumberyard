
#include "StdAfx.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Jobs/JobManagerBus.h>
#include <AzCore/std/parallel/atomic.h>

#include <ISystem.h>
#include <LmbrAWS/ILmbrAWS.h>
#include <LmbrAWS/IAWSClientManager.h>

#include "CloudGemFrameworkSystemComponent.h"
#include "CloudGemFramework/AwsApiJob.h"

#include <aws/core/auth/AWSCredentialsProvider.h>

#include <CloudCanvasCommon/CloudCanvasCommonBus.h>
#include <CloudGemFramework/Error.h>
#include <CloudGemFramework/HttpClientComponent.h>

namespace CloudGemFramework
{

    const char* CloudGemFrameworkSystemComponent::COMPONENT_DISPLAY_NAME = "CloudGemFramework";
    const char* CloudGemFrameworkSystemComponent::COMPONENT_DESCRIPTION = "Provides a framework for Gems that use AWS.";
    const char* CloudGemFrameworkSystemComponent::COMPONENT_CATEGORY = "CloudCanvas";
    const char* CloudGemFrameworkSystemComponent::SERVICE_NAME = "CloudGemFrameworkService";

    void CloudGemFrameworkSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        Error::Reflect(context);
        AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
        if (serialize)
        {

            serialize->Class<CloudGemFrameworkSystemComponent, AZ::Component>()
                ->Version(1)
                ->Field("ThreadCount", &CloudGemFrameworkSystemComponent::m_threadCount)
                ->Field("FirstThreadCPU", &CloudGemFrameworkSystemComponent::m_firstThreadCPU)
                ->Field("ThreadPriority", &CloudGemFrameworkSystemComponent::m_threadPriority)
                ->Field("ThreadStackSize", &CloudGemFrameworkSystemComponent::m_threadStackSize);

            AZ::EditContext* ec = serialize->GetEditContext();
            if (ec)
            {
                ec->Class<CloudGemFrameworkSystemComponent>(COMPONENT_DISPLAY_NAME, COMPONENT_DESCRIPTION)
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, COMPONENT_CATEGORY)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC(COMPONENT_CATEGORY))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudGemFrameworkSystemComponent::m_threadCount,
                            "Thread Count", "Number of threads dedicated to executing AWS API jobs. A value of 0 means that AWS API jobs execute on the global job thread pool.")
                            ->Attribute(AZ::Edit::Attributes::Min, 0)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudGemFrameworkSystemComponent::m_firstThreadCPU,
                            "First Thread CPU", "The CPU to which the first dedicated execution thread will be assigned. A value of -1 means that the threads can run on any CPU.")
                            ->Attribute(AZ::Edit::Attributes::Min, -1)
                    ;
            }
        }
    }

    void CloudGemFrameworkSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC(SERVICE_NAME));
    }

    void CloudGemFrameworkSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC(SERVICE_NAME));
    }

    void CloudGemFrameworkSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("CloudCanvasCommonService"));
        required.push_back(AZ_CRC("JobsService"));
    }

    void CloudGemFrameworkSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        (void)dependent;
    }

    void CloudGemFrameworkSystemComponent::Init()
    {
    }

    void CloudGemFrameworkSystemComponent::Activate()
    {
        CloudGemFrameworkRequestBus::Handler::BusConnect();
    }

#ifdef _DEBUG

    AZStd::atomic_int g_jobCount{0};

    void CloudGemFrameworkSystemComponent::IncrementJobCount()
    {
        ++g_jobCount;
    }

    void CloudGemFrameworkSystemComponent::DecrementJobCount()
    {
        --g_jobCount;
    }

#endif

    void CloudGemFrameworkSystemComponent::Deactivate()
    {
        CloudGemFrameworkRequestBus::Handler::BusDisconnect();

        EBUS_EVENT(InternalCloudGemFrameworkNotificationBus, OnCloudGemFrameworkDeactivated);

#ifdef _DEBUG
        if(g_jobCount > 0)
        {
            AZ_Error(COMPONENT_DISPLAY_NAME, g_jobCount == 0, "%i AwsApiJob objects were not deleted before CloudGemFrameworkSystemComponent was deactivated.", (int)g_jobCount);
        }
#endif

    }

    AZStd::string CloudGemFrameworkSystemComponent::GetServiceUrl(const AZStd::string& serviceName) {
        AZStd::string configName = serviceName + ".ServiceApi";
        AZStd::string serviceUrl = gEnv->pLmbrAWS->GetClientManager()->GetConfigurationParameters().GetParameter(configName.c_str()).c_str();
        AZ_Error(COMPONENT_DISPLAY_NAME, !serviceUrl.empty(), "No mapping provided for the %s service.", serviceName.c_str());
        return serviceUrl;
    }

    LmbrAWS::RequestRootCAFileResult CloudGemFrameworkSystemComponent::GetRootCAFile(AZStd::string& filePath)
    {
        LmbrAWS::RequestRootCAFileResult requestResult;
        EBUS_EVENT_RESULT(requestResult, CloudCanvasCommon::CloudCanvasCommonRequestBus,RequestRootCAFile, filePath);
        return requestResult;
    }

    AZ::JobContext* CloudGemFrameworkSystemComponent::GetDefaultJobContext()
    {
        if(m_threadCount < 1)
        {
            AZ::JobContext* jobContext{nullptr};
            EBUS_EVENT_RESULT(jobContext, AZ::JobManagerBus, GetGlobalContext);
            return jobContext;
        }
        else
        {
            if(!m_jobContext)
            {

                // If m_firstThreadCPU isn't -1, then each thread will be
                // assigned to a specific CPU starting with the specified
                // CPU.
                AZ::JobManagerDesc jobManagerDesc{};
                AZ::JobManagerThreadDesc threadDesc(m_firstThreadCPU, m_threadPriority, m_threadStackSize);
                for (unsigned int i = 0; i < m_threadCount; ++i)
                {
                    jobManagerDesc.m_workerThreads.push_back(threadDesc);
                    if (threadDesc.m_cpuId > -1)
                    {
                        threadDesc.m_cpuId++;
                    }
                }

                m_jobCancelGroup.reset(aznew AZ::JobCancelGroup());
                m_jobManager.reset(aznew AZ::JobManager(jobManagerDesc));
                m_jobContext.reset(aznew AZ::JobContext(*m_jobManager, *m_jobCancelGroup));

            }
            return m_jobContext.get();
        }
    }

    std::shared_ptr<Aws::Auth::AWSCredentialsProvider> CloudGemFrameworkSystemComponent::GetPlayerCredentialsProvider()
    {
        return gEnv->pLmbrAWS->GetClientManager()->GetDefaultClientSettings().credentialProvider;
    }

}
