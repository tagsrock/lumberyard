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

#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>



#include <CloudGemFramework/AwsApiClientJob.h>

#include <LmbrAWS/IAWSClientManager.h>
#include <LmbrAWS/ILmbrAWS.h>



namespace CloudGemFramework
{
    /////////////////////////////////////////////
    // EBus Definitions
    /////////////////////////////////////////////
    class HttpClientComponentRequests
        : public AZ::ComponentBus
    {
    public:
        virtual ~HttpClientComponentRequests() {}
        virtual void MakeHttpRequest(AZStd::string url, AZStd::string method, AZStd::string jsonBody) {}
    };

    using HttpClientComponentRequestBus = AZ::EBus<HttpClientComponentRequests>;

    class HttpClientComponentNotifications
        : public AZ::ComponentBus
    {
    public:
        virtual ~HttpClientComponentNotifications() {}
        virtual void OnHttpRequestSuccess(int responseCode, AZStd::string responseBody) {}
        virtual void OnHttpRequestFailure(int responseCode) {}
    };

    using HttpClientComponentNotificationBus = AZ::EBus<HttpClientComponentNotifications>;

    /////////////////////////////////////////////
    // Entity Component
    /////////////////////////////////////////////
    class HttpClientComponent
        : public AZ::Component
        , public HttpClientComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(HttpClientComponent, "{EB9DB999-AD75-46AF-8FDA-956B15186D90}");
        virtual ~HttpClientComponent() = default;

        void Init() override;
        void Activate() override;
        void Deactivate() override;

        void MakeHttpRequest(AZStd::string, AZStd::string, AZStd::string) override;

        static void Reflect(AZ::ReflectContext*);
    };
} // namespace CloudGemFramework
