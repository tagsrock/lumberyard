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

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/JSON/stringbuffer.h>
#include <AzCore/JSON/writer.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <CloudGemFramework/AwsApiRequestJob.h>
#include <LmbrAWS/IAWSClientManager.h>
#include <LmbrAWS/ILmbrAWS.h>
#include <ISystem.h>

#include <ctime>
#include <memory>

#include "UserPoolTokenRetrievalStrategy.h"

#include "CloudGemPlayerAccountSystemComponent.h"
#include "AWS/ServiceAPI/CloudGemPlayerAccountClientComponent.h"

#pragma warning(push)
#pragma warning(disable: 4355 4251) // <future> includes ppltasks.h which throws a C4355 warning: 'this' used in base member initializer list
// Secret Hash generation
#include <aws/core/utils/crypto/Sha256HMAC.h>
#include <aws/core/utils/HashingUtils.h>

// Cognito User Pool actions
#include <aws/cognito-idp/model/ChangePasswordRequest.h>
#include <aws/cognito-idp/model/ConfirmForgotPasswordRequest.h>
#include <aws/cognito-idp/model/ConfirmSignUpRequest.h>
#include <aws/cognito-idp/model/DeleteUserAttributesRequest.h>
#include <aws/cognito-idp/model/DeleteUserRequest.h>
#include <aws/cognito-idp/model/ForgotPasswordRequest.h>
#include <aws/cognito-idp/model/GetUserRequest.h>
#include <aws/cognito-idp/model/GlobalSignOutRequest.h>
#include <aws/cognito-idp/model/InitiateAuthRequest.h>
#include <aws/cognito-idp/model/ResendConfirmationCodeRequest.h>
#include <aws/cognito-idp/model/RespondToAuthChallengeRequest.h>
#include <aws/cognito-idp/model/SetUserSettingsRequest.h>
#include <aws/cognito-idp/model/SignUpRequest.h>
#include <aws/cognito-idp/model/UpdateDeviceStatusRequest.h>
#include <aws/cognito-idp/model/UpdateUserAttributesRequest.h>
#include <aws/cognito-idp/model/VerifyUserAttributeRequest.h>

#pragma warning(pop)

#define ENSURE_SIGNED_IN(_EVENT_NAME, _USERNAME)                                                \
    if (GetUserAuthDetails(username).refreshToken.empty())                                      \
    {                                                                                           \
        BasicResultInfo resultInfo                                                              \
        {                                                                                       \
            false,                                                                              \
            _USERNAME,                                                                          \
            NOT_SIGNED_IN_ERROR_TYPE,                                                           \
            NON_STANDARD_ERROR_TYPE,                                                            \
            NOT_SIGNED_IN_ERROR_MSG                                                             \
        };                                                                                      \
        EBUS_EVENT(CloudGemPlayerAccountNotificationBus, _EVENT_NAME, resultInfo);              \
        return;                                                                                 \
    }

#define ENSURE_SIGNED_IN_WITH_ARGS(_EVENT_NAME, _USERNAME, ...)                                 \
    if (GetUserAuthDetails(username).refreshToken.empty())                                      \
    {                                                                                           \
        BasicResultInfo resultInfo                                                              \
        {                                                                                       \
            false,                                                                              \
            _USERNAME,                                                                          \
            NOT_SIGNED_IN_ERROR_TYPE,                                                           \
            NON_STANDARD_ERROR_TYPE,                                                            \
            NOT_SIGNED_IN_ERROR_MSG                                                             \
        };                                                                                      \
        EBUS_EVENT(CloudGemPlayerAccountNotificationBus, _EVENT_NAME, resultInfo, __VA_ARGS__); \
        return;                                                                                 \
    }

#define BASIC_RESULT_INITIALIZER(_JOB, _USERNAME)       \
    {                                                   \
        _JOB->WasSuccess(),                             \
        _USERNAME,                                      \
        _JOB->error.GetExceptionName().c_str(),         \
        static_cast<int>(_JOB->error.GetErrorType()),   \
        _JOB->error.GetMessage().c_str()                \
    }

#define SUCCESSFUL_RESULT_INITIALIZER(_USERNAME)        \
    {                                                   \
        true,                                           \
        _USERNAME,                                      \
        "",                                             \
        0,                                              \
        ""                                              \
    }

#define FAILED_RESULT_INITIALIZER(_USERNAME, _ERROR_TYPE, _MESSAGE) \
    {                                                               \
        false,                                                      \
        _USERNAME,                                                  \
        _ERROR_TYPE,                                                \
        NON_STANDARD_ERROR_TYPE,                                    \
        _MESSAGE                                                    \
    }

namespace
{
    const char* ALLOC_TAG = "CloudGemPlayerAccount::CloudGemPlayerAccountSystemComponent";

    // Used when comparing against the expiration time so that we give requests enough time to complete.
    // Note that this is used on top of the built-in cusion that AuthTokenGroup uses when calculating expiration.
    const long long EXPIRATION_CUSHION_IN_SECONDS = 30LL;

    // Cognito IDP provider names are of the form "cognito-idp.us-east-1.amazonaws.com/us-east-1_123456789"
    const AZStd::string PROVIDER_NAME_USER_POOL_START = "cognito-idp.";
    const AZStd::string PROVIDER_NAME_USER_POOL_MIDDLE = ".amazonaws.com/";

    const int NON_STANDARD_ERROR_TYPE = -1;
    const char* BLACKLIST_MESSAGE_SUBSTRING = "blacklist";
    const char* BLACKLIST_ERROR_TYPE = "ACCOUNT_BLACKLISTED";
    const char* FORCE_CHANGE_PASSWORD_ERROR_TYPE = "FORCE_CHANGE_PASSWORD";
    const char* FORCE_CHANGE_PASSWORD_ERROR_MSG = "A password change is required.";
    const char* GENERAL_AUTH_ERROR_TYPE = "GENERAL_AUTH_ERROR";
    const char* GENERAL_AUTH_ERROR_MSG = "There was an unexpected error in the authorization process";
    const char* NOT_SIGNED_IN_ERROR_TYPE = "NOT_SIGNED_IN_ERROR";
    const char* NOT_SIGNED_IN_ERROR_MSG = "User must be signed in";
}

namespace CloudGemPlayerAccount
{
    const char* CloudGemPlayerAccountSystemComponent::COMPONENT_DISPLAY_NAME = "CloudGemPlayerAccount";
    const char* CloudGemPlayerAccountSystemComponent::COMPONENT_DESCRIPTION = "Allows an Entity to manage Cognito User Pool accounts, including registration, login, etc.";
    const char* CloudGemPlayerAccountSystemComponent::COMPONENT_CATEGORY = "CloudCanvas";
    const char* CloudGemPlayerAccountSystemComponent::SERVICE_NAME = "CloudGemPlayerAccountService";

    void CloudGemPlayerAccountSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<CloudGemPlayerAccountSystemComponent, AZ::Component>()
                ->Version(1)
                ->Field("LogicalUserPoolName", &CloudGemPlayerAccountSystemComponent::m_userPoolLogicalName)
                ->Field("ClientAppName", &CloudGemPlayerAccountSystemComponent::m_clientAppName);

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<CloudGemPlayerAccountSystemComponent>(COMPONENT_DISPLAY_NAME, COMPONENT_DESCRIPTION)
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, COMPONENT_CATEGORY)
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC(COMPONENT_CATEGORY))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &CloudGemPlayerAccountSystemComponent::m_userPoolLogicalName, "Logical user pool name",
                        "The logical name of the user pool resource")
                    ->DataElement(0, &CloudGemPlayerAccountSystemComponent::m_clientAppName, "Client app name",
                        "The app name of this user pool client app")
                    ;
            }
        }

        // Reflect all associated data structures
        BasicResultInfo::Reflect(context);
        DeliveryDetails::Reflect(context);
        DeliveryDetailsArray::Reflect(context);
        PlayerAccount::Reflect(context);
        AccountResultInfo::Reflect(context);
        UserAttributeList::Reflect(context);
        UserAttributeValues::Reflect(context);

        AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext)
        {
            behaviorContext->EBus<CloudGemPlayerAccountRequestBus>("CloudGemPlayerAccountRequestBus")
                // one of these for each function
                ->Event("GetCurrentUser", &CloudGemPlayerAccountRequestBus::Events::GetCurrentUser)
                ->Event("SignUp", &CloudGemPlayerAccountRequestBus::Events::SignUp)
                ->Event("ConfirmSignUp", &CloudGemPlayerAccountRequestBus::Events::ConfirmSignUp)
                ->Event("ResendConfirmationCode", &CloudGemPlayerAccountRequestBus::Events::ResendConfirmationCode)
                ->Event("ChangePassword", &CloudGemPlayerAccountRequestBus::Events::ChangePassword)
                ->Event("ForgotPassword", &CloudGemPlayerAccountRequestBus::Events::ForgotPassword)
                ->Event("ConfirmForgotPassword", &CloudGemPlayerAccountRequestBus::Events::ConfirmForgotPassword)
                ->Event("InitiateAuth", &CloudGemPlayerAccountRequestBus::Events::InitiateAuth)
                ->Event("RespondToForceChangePasswordChallenge", &CloudGemPlayerAccountRequestBus::Events::RespondToForceChangePasswordChallenge)
                ->Event("SignOut", &CloudGemPlayerAccountRequestBus::Events::SignOut)
                ->Event("GlobalSignOut", &CloudGemPlayerAccountRequestBus::Events::GlobalSignOut)
                ->Event("DeleteOwnAccount", &CloudGemPlayerAccountRequestBus::Events::DeleteOwnAccount)
                ->Event("GetUser", &CloudGemPlayerAccountRequestBus::Events::GetUser)
                ->Event("VerifyUserAttribute", &CloudGemPlayerAccountRequestBus::Events::VerifyUserAttribute)
                ->Event("DeleteUserAttributes", &CloudGemPlayerAccountRequestBus::Events::DeleteUserAttributes)
                ->Event("UpdateUserAttributes", &CloudGemPlayerAccountRequestBus::Events::UpdateUserAttributes)
                ->Event("GetPlayerAccount", &CloudGemPlayerAccountRequestBus::Events::GetPlayerAccount)
                ->Event("UpdatePlayerAccount", &CloudGemPlayerAccountRequestBus::Events::UpdatePlayerAccount)
                ;
            behaviorContext->EBus<CloudGemPlayerAccountNotificationBus>("CloudGemPlayerAccountNotificationBus")
                ->Handler<CloudGemPlayerAccountNotificationBusHandler>()
                ;
        }
    }

    void CloudGemPlayerAccountSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("CloudGemPlayerAccountService"));
    }

    void CloudGemPlayerAccountSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("CloudGemPlayerAccountService"));
    }

    void CloudGemPlayerAccountSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("CloudGemFrameworkService"));
    }

    void CloudGemPlayerAccountSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        (void)dependent;
    }

    void CloudGemPlayerAccountSystemComponent::Init()
    {
        m_anonymousCredentialsProvider = std::make_shared<Aws::Auth::AnonymousAWSCredentialsProvider>();
    }

    void CloudGemPlayerAccountSystemComponent::Activate()
    {
        CloudGemPlayerAccountRequestBus::Handler::BusConnect();
        LmbrAWS::ClientManagerNotificationBus::Handler::BusConnect();
    }

    void CloudGemPlayerAccountSystemComponent::Deactivate()
    {
        CloudGemPlayerAccountRequestBus::Handler::BusDisconnect();
        LmbrAWS::ClientManagerNotificationBus::Handler::BusDisconnect();

        if (gEnv)
        {
            gEnv->pLmbrAWS->GetClientManager()->RemoveTokenRetrievalStrategy(m_userPoolProviderName.c_str());
        }
    }

    void CloudGemPlayerAccountSystemComponent::OnBeforeConfigurationChange()
    {
        if (m_userPoolLogicalName.empty()) {
            gEnv->pLog->LogWarning("CloudGemPlayerAccountSystemComponent: The user pool logical name has not been set.");
            return;
        }

        AZStd::string poolId = GetPoolId();     // String of the form "us-east-1_123456789"
        const auto regionSize = poolId.find('_');
        if (regionSize == AZStd::string::npos)
        {
            gEnv->pLog->LogWarning("CloudGemPlayerAccountSystemComponent: Unable to register token retrieval strategy, missing region.");
            return;
        }

        // Cognito IDP provider names are of the form "cognito-idp.us-east-1.amazonaws.com/us-east-1_123456789"
        m_userPoolProviderName = PROVIDER_NAME_USER_POOL_START;        // cognito-idp.
        m_userPoolProviderName.append(poolId, 0, regionSize);          // us-east-1
        m_userPoolProviderName.append(PROVIDER_NAME_USER_POOL_MIDDLE); // .amazonaws.com/
        m_userPoolProviderName.append(poolId);                         // us-east-1_123456789

        auto strategy = Aws::MakeShared<UserPoolTokenRetrievalStrategy>(ALLOC_TAG, this);
        gEnv->pLmbrAWS->GetClientManager()->AddTokenRetrievalStrategy(m_userPoolProviderName.c_str(), strategy);
    }

    bool CloudGemPlayerAccountSystemComponent::HasCachedCredentials(const AZStd::string& username)
    {
        AuthTokenGroup tokenGroup = GetUserAuthDetails(username);
        return tokenGroup.refreshToken.length() > 0;
    }

    //////////////////////////////// Start Public User Pool Wrapper Functions ////////////////////////////////////////////

    void CloudGemPlayerAccountSystemComponent::GetCurrentUser()
    {
        string refreshToken;
        if (!gEnv->pLmbrAWS->GetClientManager()->GetRefreshTokenForProvider(refreshToken, m_userPoolProviderName.c_str()))
        {
            BasicResultInfo resultInfo FAILED_RESULT_INITIALIZER("", NOT_SIGNED_IN_ERROR_TYPE, "The user is not logged into the Cognito user pool");
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGetCurrentUserComplete, resultInfo);
            return;
        }

        AZStd::string username;
        if (GetCachedUsernameForRefreshToken(username, refreshToken.c_str()))
        {
            BasicResultInfo resultInfo SUCCESSFUL_RESULT_INITIALIZER(username.c_str());
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGetCurrentUserComplete, resultInfo);
            return;
        }

        auto refreshCallback = [this](const BasicResultInfo& basicResult, const Model::AuthenticationResultType& authenticationResult)
        {
            if (!basicResult.wasSuccessful)
            {
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGetCurrentUserComplete, basicResult);
                return;
            }

            auto getUserCallback = [this, authenticationResult](const BasicResultInfo& basicResult)
            {
                if (basicResult.wasSuccessful)
                {
                    CacheUserAuthDetails(basicResult.username.c_str(), authenticationResult);
                }
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGetCurrentUserComplete, basicResult);
            };

            GetUserForAcccessToken(getUserCallback, authenticationResult.GetAccessToken().c_str());
        };

        AuthenticateWithRefreshToken(refreshCallback, refreshToken.c_str());
    }

    void CloudGemPlayerAccountSystemComponent::SignUp(const AZStd::string& username, const AZStd::string& password, const UserAttributeValues& attributes)
    {
        using SignUpRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, SignUp);

        // An anonymous credentials provider is used because the request does not need to be signed.
        SignUpRequestJob::Config config{ SignUpRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username, password](SignUpRequestJob* job)
        {
            DeliveryDetails deliveryDetails;
            deliveryDetails.Reset(job->result.GetCodeDeliveryDetails());
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnSignUpComplete, resultInfo, deliveryDetails, job->result.GetUserConfirmed());
        };

        auto job = SignUpRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetUsername(username.c_str());
        job->request.SetPassword(password.c_str());

        auto& attributeData = attributes.GetData();
        for (auto& attributePair : attributeData)
        {
            Aws::CognitoIdentityProvider::Model::AttributeType attr;
            attr.SetName(attributePair.first.c_str());
            attr.SetValue(attributePair.second.c_str());
            job->request.AddUserAttributes(attr);
        }

        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::ConfirmSignUp(const AZStd::string& username, const AZStd::string& confirmationCode)
    {
        using ConfirmSignUpRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, ConfirmSignUp);

        // An anonymous credentials provider is used because the request does not need to be signed.
        ConfirmSignUpRequestJob::Config config{ ConfirmSignUpRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username, confirmationCode](ConfirmSignUpRequestJob* job)
        {
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnConfirmSignUpComplete, resultInfo);
        };

        auto job = ConfirmSignUpRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetUsername(username.c_str());
        job->request.SetConfirmationCode(confirmationCode.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::ResendConfirmationCode(const AZStd::string& username)
    {
        using ResendConfirmationCodeRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, ResendConfirmationCode);

        // An anonymous credentials provider is used because the request does not need to be signed.
        ResendConfirmationCodeRequestJob::Config config{ ResendConfirmationCodeRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username](ResendConfirmationCodeRequestJob* job)
        {
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
            DeliveryDetails deliveryDetails;
            deliveryDetails.Reset(job->result.GetCodeDeliveryDetails());
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnResendConfirmationCodeComplete, resultInfo, deliveryDetails);
        };

        auto job = ResendConfirmationCodeRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetUsername(username.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::ForgotPassword(const AZStd::string& username)
    {
        using ForgotPasswordRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, ForgotPassword);

        // An anonymous credentials provider is used because the request does not need to be signed.
        ForgotPasswordRequestJob::Config config{ ForgotPasswordRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username](ForgotPasswordRequestJob* job)
        {
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
            DeliveryDetails deliveryDetails;
            deliveryDetails.Reset(job->result.GetCodeDeliveryDetails());
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnForgotPasswordComplete, resultInfo, deliveryDetails);
        };

        auto job = ForgotPasswordRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetUsername(username.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::ConfirmForgotPassword(const AZStd::string& username, const AZStd::string& password, const AZStd::string& confirmationCode)
    {
        using ConfirmForgotPasswordRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, ConfirmForgotPassword);

        // An anonymous credentials provider is used because the request does not need to be signed.
        ConfirmForgotPasswordRequestJob::Config config{ ConfirmForgotPasswordRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username, password](ConfirmForgotPasswordRequestJob* job)
        {
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnConfirmForgotPasswordComplete, resultInfo);
        };

        auto job = ConfirmForgotPasswordRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetConfirmationCode(confirmationCode.c_str());
        job->request.SetPassword(password.c_str());
        job->request.SetUsername(username.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::InitiateAuth(const AZStd::string& username, const AZStd::string& password)
    {
        auto onComplete = [](const BasicResultInfo& resultInfo)
        {
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnInitiateAuthComplete, resultInfo);
        };
        CallInitiateAuth(username, password, "", onComplete);
    }

    void CloudGemPlayerAccountSystemComponent::RespondToForceChangePasswordChallenge(const AZStd::string& username,
        const AZStd::string& currentPassword, const AZStd::string& newPassword)
    {
        auto onComplete = [](const BasicResultInfo& resultInfo)
        {
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnRespondToForceChangePasswordChallengeComplete, resultInfo);
        };
        CallInitiateAuth(username, currentPassword, newPassword, onComplete);
    }

    void CloudGemPlayerAccountSystemComponent::CallInitiateAuth(const AZStd::string& username, const AZStd::string& currentPassword,
        const AZStd::string& newPassword, const AuthCallback& authCallback)
    {
        using InitiateAuthRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, InitiateAuth);

        // An anonymous credentials provider is used because the request does not need to be signed.
        InitiateAuthRequestJob::Config config{ InitiateAuthRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username, currentPassword, newPassword, authCallback](InitiateAuthRequestJob* job)
        {
            if (!job->WasSuccess())   // See if first step in auth process failed
            {
                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                authCallback(resultInfo);
            }
            else
            {
                if (job->result.GetChallengeName() == Model::ChallengeNameType::CUSTOM_CHALLENGE)
                {
                    AZStd::string challengeType;
                    auto challengeParameters = job->result.GetChallengeParameters();
                    auto challengeTypeIter = challengeParameters.find("type");
                    if (challengeTypeIter != challengeParameters.end()) {
                        challengeType = challengeTypeIter->second.c_str();
                    }

                    if (challengeType == "ForceChangePassword" && newPassword.empty()) {
                        BasicResultInfo resultInfo FAILED_RESULT_INITIALIZER(username, FORCE_CHANGE_PASSWORD_ERROR_TYPE, FORCE_CHANGE_PASSWORD_ERROR_MSG);
                        authCallback(resultInfo);
                        return;
                    }

                    RespondToAuthChallenge(job->result.GetChallengeName(), challengeType, job->result, currentPassword, newPassword, authCallback);
                }
                else
                {
                    BasicResultInfo resultInfo FAILED_RESULT_INITIALIZER(username, GENERAL_AUTH_ERROR_TYPE, "Unexpected auth challenge");
                    authCallback(resultInfo);
                }
            }
        };

        auto job = InitiateAuthRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetAuthFlow(Model::AuthFlowType::CUSTOM_AUTH);
        job->request.AddAuthParameters("USERNAME", username.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::SignOut(const AZStd::string& username)
    {
        LocalSignOut(username);

        EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnSignOutComplete, username.c_str());
    }

    void CloudGemPlayerAccountSystemComponent::ChangePassword(const AZStd::string& username, const AZStd::string& previousPassword, const AZStd::string& proposedPassword)
    {
        ENSURE_SIGNED_IN(OnChangePasswordComplete, username)

        RefreshAccessTokensIfExpired(username, [this, username, previousPassword, proposedPassword](AuthTokenGroup tokenGroup)
        {
            using ChangePasswordRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, ChangePassword);

            // An anonymous credentials provider is used because the request does not need to be signed.
            ChangePasswordRequestJob::Config config{ ChangePasswordRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username, previousPassword, proposedPassword](ChangePasswordRequestJob* job)
            {
                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnChangePasswordComplete, resultInfo);
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = ChangePasswordRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            job->request.SetPreviousPassword(previousPassword.c_str());
            job->request.SetProposedPassword(proposedPassword.c_str());
            job->Start();
        });
    }

    void CloudGemPlayerAccountSystemComponent::GlobalSignOut(const AZStd::string& username)
    {
        ENSURE_SIGNED_IN(OnGlobalSignOutComplete, username)

        RefreshAccessTokensIfExpired(username, [this, username](AuthTokenGroup tokenGroup)
        {
            using GlobalSignOutRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, GlobalSignOut);

            // An anonymous credentials provider is used because the request does not need to be signed.
            GlobalSignOutRequestJob::Config config{ GlobalSignOutRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username](GlobalSignOutRequestJob* job)
            {
                if (job->WasSuccess())
                {
                    LocalSignOut(username); // Erase all local credential caches
                }

                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGlobalSignOutComplete, resultInfo);
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = GlobalSignOutRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            job->Start();
        });
    }

    void CloudGemPlayerAccountSystemComponent::DeleteOwnAccount(const AZStd::string& username)
    {
        ENSURE_SIGNED_IN(OnDeleteOwnAccountComplete, username)

        RefreshAccessTokensIfExpired(username, [this, username](AuthTokenGroup tokenGroup)
        {
            using DeleteUserRequestJob = AWS_API_REQUEST_JOB_NO_RESULT(CognitoIdentityProvider, DeleteUser);

            // An anonymous credentials provider is used because the request does not need to be signed.
            DeleteUserRequestJob::Config config{ DeleteUserRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username](DeleteUserRequestJob* job)
            {
                if (job->WasSuccess())
                {
                    LocalSignOut(username); // Erase all local credential caches
                }

                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnDeleteOwnAccountComplete, resultInfo);
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = DeleteUserRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            job->Start();
        });
    }

    void CloudGemPlayerAccountSystemComponent::GetUser(const AZStd::string& username)
    {
        ENSURE_SIGNED_IN_WITH_ARGS(OnGetUserComplete, username, UserAttributeValues(), UserAttributeList())

        RefreshAccessTokensIfExpired(username, [this, username](AuthTokenGroup tokenGroup)
        {
            using GetUserRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, GetUser);

            // An anonymous credentials provider is used because the request does not need to be signed.
            GetUserRequestJob::Config config{ GetUserRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username](GetUserRequestJob* job)
            {
                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);

                UserAttributeValues attrs;
                UserAttributeList mfaOps;
                if (job->WasSuccess())
                {
                    auto& resultAttrs = job->result.GetUserAttributes();
                    for (auto& attr : resultAttrs)
                    {
                        attrs.SetAttribute(attr.GetName().c_str(), attr.GetValue().c_str());
                    }

                    auto& mfaOptions = job->result.GetMFAOptions();
                    for (auto& option : mfaOptions)
                    {
                        mfaOps.AddAttribute(option.GetAttributeName().c_str());
                    }
                }

                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGetUserComplete, resultInfo, attrs, mfaOps);
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = GetUserRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            job->Start();
        });
    }

    void CloudGemPlayerAccountSystemComponent::VerifyUserAttribute(const AZStd::string& username, const AZStd::string& attributeName, const AZStd::string& confirmationCode)
    {
        ENSURE_SIGNED_IN(OnVerifyUserAttributeComplete, username)

        RefreshAccessTokensIfExpired(username, [this, username, attributeName, confirmationCode](AuthTokenGroup tokenGroup)
        {
            using VerifyUserAttributeRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, VerifyUserAttribute);

            // An anonymous credentials provider is used because the request does not need to be signed.
            VerifyUserAttributeRequestJob::Config config{ VerifyUserAttributeRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username](VerifyUserAttributeRequestJob* job)
            {
                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnVerifyUserAttributeComplete, resultInfo);
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = VerifyUserAttributeRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            job->request.SetAttributeName(attributeName.c_str());
            job->request.SetCode(confirmationCode.c_str());
            job->Start();
        });
    }

    void CloudGemPlayerAccountSystemComponent::DeleteUserAttributes(const AZStd::string& username, const UserAttributeList& attributesToDelete)
    {
        if (attributesToDelete.GetData().size() < 1)
        {
            // Nothing to delete
            BasicResultInfo resultInfo SUCCESSFUL_RESULT_INITIALIZER(username);
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnDeleteUserAttributesComplete, resultInfo);
            return;
        }

        ENSURE_SIGNED_IN(OnDeleteUserAttributesComplete, username)

        RefreshAccessTokensIfExpired(username, [this, username, attributesToDelete](AuthTokenGroup tokenGroup)
        {
            using DeleteUserAttributesRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, DeleteUserAttributes);

            // An anonymous credentials provider is used because the request does not need to be signed.
            DeleteUserAttributesRequestJob::Config config{ DeleteUserAttributesRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username](DeleteUserAttributesRequestJob* job)
            {
                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnDeleteUserAttributesComplete, resultInfo);
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = DeleteUserAttributesRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            auto& toDeleteSet = attributesToDelete.GetData();
            for (auto& attrName : toDeleteSet)
            {
                job->request.AddUserAttributeNames(attrName.c_str());
            }
            job->Start();
        });
    }

    void CloudGemPlayerAccountSystemComponent::UpdateUserAttributes(const AZStd::string& username, const UserAttributeValues& attributes)
    {
        ENSURE_SIGNED_IN_WITH_ARGS(OnUpdateUserAttributesComplete, username, DeliveryDetailsArray())

        RefreshAccessTokensIfExpired(username, [this, username, attributes](AuthTokenGroup tokenGroup)
        {
            using UpdateUserAttributesRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, UpdateUserAttributes);

            // An anonymous credentials provider is used because the request does not need to be signed.
            UpdateUserAttributesRequestJob::Config config{ UpdateUserAttributesRequestJob::GetDefaultConfig() };
            config.credentialsProvider = m_anonymousCredentialsProvider;
            config.region = GetPoolRegion().c_str();

            auto callbackLambda = [this, username](UpdateUserAttributesRequestJob* job)
            {
                BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username);
                auto& detailsList = job->result.GetCodeDeliveryDetailsList();
                EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnUpdateUserAttributesComplete, resultInfo, DeliveryDetailsArray{ detailsList });
                SignOutIfTokenIsInvalid(job, username);
            };

            auto job = UpdateUserAttributesRequestJob::Create(callbackLambda, callbackLambda, &config);

            job->request.SetAccessToken(tokenGroup.accessToken);
            auto& attributeData = attributes.GetData();
            for (auto& attributePair : attributeData)
            {
                Aws::CognitoIdentityProvider::Model::AttributeType attr;
                attr.SetName(attributePair.first.c_str());
                attr.SetValue(attributePair.second.c_str());
                job->request.AddUserAttributes(attr);
            }
            job->Start();
        });
    }

    //////////////////////////////// End Public User Pool Wrapper Functions ////////////////////////////////////////////

    void CloudGemPlayerAccountSystemComponent::GetPlayerAccount()
    {
        auto callbackLambda = [](ServiceAPI::GetAccountRequestJob* job)
        {
            AccountResultInfo resultInfo
            {
                job->WasSuccess(),
                job->WasSuccess() ? "" : "GetAccountFailed",
                job->error.message.c_str()
            };
            PlayerAccount playerAccount;
            playerAccount.SetPlayerName(job->result.PlayerName);
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnGetPlayerAccountComplete, resultInfo, playerAccount);
        };

        ServiceAPI::GetAccountRequestJob* job = ServiceAPI::GetAccountRequestJob::Create(callbackLambda, callbackLambda);
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::UpdatePlayerAccount(const PlayerAccount& playerAccount)
    {
        auto callbackLambda = [playerAccount](ServiceAPI::PutAccountRequestJob* job)
        {
            AccountResultInfo resultInfo
            {
                job->WasSuccess(),
                job->WasSuccess() ? "" : "UpdatePlayerAccountFailed",
                job->error.message.c_str()
            };
            EBUS_EVENT(CloudGemPlayerAccountNotificationBus, OnUpdatePlayerAccountComplete, resultInfo);
        };

        ServiceAPI::PutAccountRequestJob* job = ServiceAPI::PutAccountRequestJob::Create(callbackLambda, callbackLambda);
        job->parameters.UpdateAccountRequest.PlayerName = playerAccount.GetPlayerName();
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::LocalSignOut(const AZStd::string& username)
    {
        {
            std::lock_guard<std::mutex> lock(m_tokenAccessMutex);
            m_usernamesToTokens.erase(username);
        }

        gEnv->pLmbrAWS->GetClientManager()->Logout();
    }

    template<typename Job> void CloudGemPlayerAccountSystemComponent::SignOutIfTokenIsInvalid(const Job& job, const AZStd::string& username)
    {
        // NOT_AUTHORIZED is returned when the auth token has been revoked using global sign out.
        if (!job->WasSuccess() && job->error.GetErrorType() == Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors::NOT_AUTHORIZED)
        {
            // The token is no longer valid so the user is effectively not signed in.  Update the local state to match the server.
            LocalSignOut(username);
        }
    }

    Aws::Auth::LoginAccessTokens CloudGemPlayerAccountSystemComponent::RetrieveLongTermTokenFromAuthToken(const Aws::String& idToken)
    {
        std::lock_guard<std::mutex> locker(m_tokenAccessMutex);

        Aws::Auth::LoginAccessTokens tokenGroup;

        // Search through m_usernamesToTokens for the corresponding access token, then fill in the data from there.
        // Note that m_usernamesToTokens is only as large as the number of logins that have happened locally, so will often be of size 1.
        for (auto& tokenIter : m_usernamesToTokens)
        {
            if (tokenIter.second.idToken == idToken) // The AuthTokenGroup class has a dedicated field for the ID token.
            {
                // Cognito identity pools require the ID token instead of an access token because the ID token has the identity information it needs,
                // however the LoginAccessTokens class doesn't have an idToken field so the ID token has to be stored in the accessToken field.
                tokenGroup.accessToken = tokenIter.second.idToken;
                tokenGroup.longTermToken = tokenIter.second.refreshToken;
                tokenGroup.longTermTokenExpiry = tokenIter.second.GetExpirationTime();

                break;
            }
        }

        return tokenGroup;
    }

    Aws::Auth::LoginAccessTokens CloudGemPlayerAccountSystemComponent::RefreshAccessTokens(const Aws::Auth::LoginAccessTokens& tokens)
    {
        // Authenticate using the refresh token to get an access token and identity token.
        Model::InitiateAuthRequest refreshRequest;
        refreshRequest.SetClientId(GetClientId().c_str());
        refreshRequest.SetAuthFlow(Model::AuthFlowType::REFRESH_TOKEN_AUTH);
        refreshRequest.AddAuthParameters("REFRESH_TOKEN", tokens.longTermToken);

        Model::InitiateAuthOutcome refreshOutcome = GetClient()->InitiateAuth(refreshRequest);

        Aws::Auth::LoginAccessTokens tokenGroup;
        if (!refreshOutcome.IsSuccess())
        {
            // NOT_AUTHORIZED is returned when the auth token has been revoked using global sign out.
            if (refreshOutcome.GetError().GetErrorType() == Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors::NOT_AUTHORIZED)
            {
                // The token is no longer valid so the user is effectively not signed in.  Update the local state to match the server.
                gEnv->pLmbrAWS->GetClientManager()->Logout();
            }

            return tokenGroup;
        }

        auto& authenticationResult = refreshOutcome.GetResult().GetAuthenticationResult();

        tokenGroup.accessToken = authenticationResult.GetIdToken();
        tokenGroup.longTermToken = tokens.longTermToken; // Not included in the auth response when using REFRESH_TOKEN.
        tokenGroup.longTermTokenExpiry = static_cast<long long>(Aws::Utils::DateTime::ComputeCurrentTimestampInAmazonFormat())
            + authenticationResult.GetExpiresIn();

        // If the username was cached, update the cache and return the updated tokens.
        AZStd::string username;
        {
            std::lock_guard<std::mutex> locker(m_tokenAccessMutex);
            for (auto& tokenIter : m_usernamesToTokens)
            {
                if (tokenIter.second.refreshToken == tokens.longTermToken)
                {
                    username = tokenIter.first;
                    break;
                }
            }
        }
        if (!username.empty())
        {
            CacheUserAuthDetails(username, authenticationResult);
            return tokenGroup;
        }

        // Use the access token to get the username.
        Model::GetUserRequest getUserRequest;
        getUserRequest.SetAccessToken(authenticationResult.GetAccessToken());
        Model::GetUserOutcome getUserOutcome = GetClient()->GetUser(getUserRequest);

        if (!getUserOutcome.IsSuccess())
        {
            // The token was refreshed, but it can't be cached locally due to username lookup failure.
            return tokenGroup;
        }

        CacheUserAuthDetails(getUserOutcome.GetResult().GetUsername().c_str(), authenticationResult);
        return tokenGroup;
    }

    void CloudGemPlayerAccountSystemComponent::RefreshAccessTokensIfExpired(const AZStd::string& username, const RefreshAccessTokensHandler& handler)
    {
        AuthTokenGroup existingTokens = GetUserAuthDetails(username);

        if (!existingTokens.IsExpired())
        {
            handler(existingTokens);
            return;
        }

        using InitiateAuthRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, InitiateAuth);

        // An anonymous credentials provider is used because the request does not need to be signed.
        InitiateAuthRequestJob::Config config{ InitiateAuthRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, username, handler, existingTokens](InitiateAuthRequestJob* job)
        {
            if (job->WasSuccess())
            {
                AuthTokenGroup refreshedTokens = CacheUserAuthDetails(username, job->result.GetAuthenticationResult());
                handler(refreshedTokens);
            }
            else
            {
                gEnv->pLog->LogError("Unable to refresh auth tokens for user %s: %s %s", username.c_str(), job->error.GetExceptionName().c_str(), job->error.GetMessage().c_str());
                handler(existingTokens);
            }
            SignOutIfTokenIsInvalid(job, username);
        };

        auto job = InitiateAuthRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetAuthFlow(Model::AuthFlowType::REFRESH_TOKEN_AUTH);
        job->request.AddAuthParameters("REFRESH_TOKEN", existingTokens.refreshToken);
        job->Start();
    }

    AZStd::string CloudGemPlayerAccountSystemComponent::GetTimestamp()
    {
        time_t rawtime;
        struct tm timeinfo;
        const size_t TIME_BUFFER_SIZE = 100;
        char buffer[TIME_BUFFER_SIZE];

        time(&rawtime);
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
        gmtime_s(&timeinfo, &rawtime);
#else
        gmtime_r(&rawtime, &timeinfo);
#endif

        // Matches SimpleDateFormat("EEE MMM d HH:mm:ss z yyyy") in Java. 
        // First, the month and day ("Mon Day ") section
        strftime(buffer, TIME_BUFFER_SIZE, "%a %b ", &timeinfo);
        AZStd::string formattedTimestamp = buffer;

        // Then the day of the month. This must be non-padded, but strftime() only does zero-or-space padded, so use a string format instead.
        formattedTimestamp.append(AZStd::string::format("%d", timeinfo.tm_mday));

        // Finally, the " HH:mm:ss UTC yyyy" part
        strftime(buffer, TIME_BUFFER_SIZE, " %H:%M:%S UTC %Y", &timeinfo);
        formattedTimestamp.append(buffer);

        return formattedTimestamp;
    }

    AuthTokenGroup CloudGemPlayerAccountSystemComponent::CacheUserAuthDetails(const AZStd::string& username, const Model::AuthenticationResultType& authResult)
    {
        std::lock_guard<std::mutex> locker(m_tokenAccessMutex);

        AuthTokenGroup& tokenGroup = m_usernamesToTokens[username];
        if (authResult.GetRefreshToken().size() > 0)
        {
            tokenGroup.refreshToken = authResult.GetRefreshToken();
        }
        if (authResult.GetAccessToken().size() > 0)
        {
            tokenGroup.accessToken = authResult.GetAccessToken();
        }
        if (authResult.GetIdToken().size() > 0)
        {
            tokenGroup.idToken = authResult.GetIdToken();
        }

        int tokenLifetime = authResult.GetExpiresIn();
        tokenGroup.SetExpirationTime(tokenLifetime);

        return tokenGroup;
    }

    AuthTokenGroup CloudGemPlayerAccountSystemComponent::GetUserAuthDetails(const AZStd::string& username)
    {
        std::lock_guard<std::mutex> locker(m_tokenAccessMutex);

        if (m_usernamesToTokens.count(username) > 0)
        {
            return m_usernamesToTokens[username];
        }

        return AuthTokenGroup();
    }

    bool CloudGemPlayerAccountSystemComponent::GetCachedUsernameForRefreshToken(AZStd::string& username, const AZStd::string& refreshToken)
    {
        std::lock_guard<std::mutex> locker(m_tokenAccessMutex);

        Aws::String awsRefreshToken{ refreshToken.c_str() };
        for (auto& tokenIter : m_usernamesToTokens)
        {
            if (tokenIter.second.refreshToken == awsRefreshToken)
            {
                username = tokenIter.first;
                return true;
            }
        }

        return false;
    }

    void CloudGemPlayerAccountSystemComponent::AuthenticateWithRefreshToken(const AuthenticateWithRefreshTokenHandler& handler, const AZStd::string& refreshToken)
    {
        using InitiateAuthRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, InitiateAuth);

        // An anonymous credentials provider is used because the request does not need to be signed.
        InitiateAuthRequestJob::Config config{ InitiateAuthRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [handler, refreshToken](InitiateAuthRequestJob* job)
        {
            BasicResultInfo resultInfo;
            resultInfo.wasSuccessful = job->WasSuccess();

            if (!resultInfo.wasSuccessful)
            {
                resultInfo.errorTypeName = job->error.GetExceptionName().c_str();
                resultInfo.errorTypeValue = static_cast<int>(job->error.GetErrorType());
                resultInfo.errorMessage = job->error.GetMessage().c_str();
            }

            // Make a copy and add the refresh token.  Cognito doesn't return it when the request is REFRESH_TOKEN_AUTH.
            Model::AuthenticationResultType authenticationResult = job->result.GetAuthenticationResult();
            authenticationResult.SetRefreshToken(refreshToken.c_str());

            handler(resultInfo, authenticationResult);

            // NOT_AUTHORIZED is returned when the auth token has been revoked using global sign out.
            if (!resultInfo.wasSuccessful && job->error.GetErrorType() == Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors::NOT_AUTHORIZED)
            {
                // The token is no longer valid so the user is effectively not signed in.  Update the local state to match the server.
                gEnv->pLmbrAWS->GetClientManager()->Logout();
            }
        };

        auto job = InitiateAuthRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetAuthFlow(Model::AuthFlowType::REFRESH_TOKEN_AUTH);
        job->request.AddAuthParameters("REFRESH_TOKEN", refreshToken.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::GetUserForAcccessToken(const GetUserForAccessTokenHandler& handler, const AZStd::string& accessToken)
    {
        using GetUserRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, GetUser);

        // An anonymous credentials provider is used because the request does not need to be signed.
        GetUserRequestJob::Config config{ GetUserRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        auto callbackLambda = [this, handler](GetUserRequestJob* job)
        {
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, job->result.GetUsername().c_str());
            handler(resultInfo);
        };

        auto job = GetUserRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetAccessToken(accessToken.c_str());
        job->Start();
    }

    void CloudGemPlayerAccountSystemComponent::RespondToAuthChallenge(Model::ChallengeNameType challengeName, AZStd::string challengeType,
        const Model::InitiateAuthResult& result, const AZStd::string currentPassword, const AZStd::string newPassword, AuthCallback onComplete)
    {
        using RespondToAuthChallengeRequestJob = AWS_API_REQUEST_JOB(CognitoIdentityProvider, RespondToAuthChallenge);

        // An anonymous credentials provider is used because the request does not need to be signed.
        RespondToAuthChallengeRequestJob::Config config{ RespondToAuthChallengeRequestJob::GetDefaultConfig() };
        config.credentialsProvider = m_anonymousCredentialsProvider;
        config.region = GetPoolRegion().c_str();

        const Aws::Map<Aws::String, Aws::String>& challengeParams = result.GetChallengeParameters();
        auto username = challengeParams.at("USERNAME");

        auto callbackLambda = [this, username, onComplete](RespondToAuthChallengeRequestJob* job)
        {
            BasicResultInfo resultInfo BASIC_RESULT_INITIALIZER(job, username.c_str());

            if (!job->WasSuccess())   // See if first step in auth process failed
            {
                if (std::strstr(resultInfo.errorMessage.c_str(), BLACKLIST_MESSAGE_SUBSTRING))
                {
                    resultInfo.errorTypeValue = NON_STANDARD_ERROR_TYPE;
                    resultInfo.errorTypeName = BLACKLIST_ERROR_TYPE;
                }
                onComplete(resultInfo);
            }
            else
            {
                Model::ChallengeNameType challengeName = job->result.GetChallengeName();
                switch (challengeName)
                {
                case Model::ChallengeNameType::PASSWORD_VERIFIER:
                case Model::ChallengeNameType::CUSTOM_CHALLENGE:
                case Model::ChallengeNameType::DEVICE_SRP_AUTH:
                case Model::ChallengeNameType::DEVICE_PASSWORD_VERIFIER:
                case Model::ChallengeNameType::ADMIN_NO_SRP_AUTH:
                case Model::ChallengeNameType::SMS_MFA:
                    onComplete({ false, "", GENERAL_AUTH_ERROR_TYPE, NON_STANDARD_ERROR_TYPE, GENERAL_AUTH_ERROR_MSG });
                    break;

                case Model::ChallengeNameType::NOT_SET:
                default:
                    // Successfully logged in
                    auto& authResult = job->result.GetAuthenticationResult();
                    AuthTokenGroup tokenGroup = CacheUserAuthDetails(username.c_str(), authResult);
                    bool success = gEnv->pLmbrAWS->GetClientManager()->Login(m_userPoolProviderName.c_str(),
                        tokenGroup.idToken.c_str(), // Note use of ID Token, not Access Token
                        tokenGroup.refreshToken.c_str(),
                        tokenGroup.GetExpirationTime());
                    if (success)
                    {
                        onComplete(SUCCESSFUL_RESULT_INITIALIZER(""));
                    }
                    else
                    {
                        onComplete({ false, "", GENERAL_AUTH_ERROR_TYPE, NON_STANDARD_ERROR_TYPE, GENERAL_AUTH_ERROR_MSG });
                    }
                    break;
                }
            }
        };

        auto job = RespondToAuthChallengeRequestJob::Create(callbackLambda, callbackLambda, &config);

        job->request.SetClientId(GetClientId().c_str());
        job->request.SetSession(result.GetSession().c_str());
        job->request.SetChallengeName(challengeName);
        job->request.AddChallengeResponses("USERNAME", username.c_str());

        if (challengeType == "ForceChangePassword")
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();
            writer.String("password");
            writer.String(currentPassword.c_str());
            writer.String("newPassword");
            writer.String(newPassword.c_str());
            writer.EndObject();
            job->request.AddChallengeResponses("ANSWER", buffer.GetString());
        }
        else
        {
            job->request.AddChallengeResponses("ANSWER", currentPassword.c_str());
        }

        job->Start();
    }

    LmbrAWS::CognitoIdentityProvider::IdentityProviderClient CloudGemPlayerAccountSystemComponent::GetClient()
    {
        LmbrAWS::CognitoIdentityProvider::Manager& idpManager = gEnv->pLmbrAWS->GetClientManager()->GetCognitoIdentityProviderManager();
        return idpManager.CreateIdentityProviderClient(m_userPoolLogicalName.c_str());
    }

    AZStd::string CloudGemPlayerAccountSystemComponent::GetPoolId()
    {
        LmbrAWS::CognitoIdentityProvider::Manager& idpManager = gEnv->pLmbrAWS->GetClientManager()->GetCognitoIdentityProviderManager();
        return idpManager.CreateIdentityProviderClient(m_userPoolLogicalName.c_str()).GetUserPoolId().c_str();
    }

    AZStd::string CloudGemPlayerAccountSystemComponent::GetPoolRegion()
    {
        AZStd::string poolId = GetPoolId();     // String of the form "us-east-1_123456789"
        const auto regionSize = poolId.find('_');
        if (regionSize == AZStd::string::npos)
        {
            gEnv->pLog->LogWarning("CloudGemPlayerAccountSystemComponent: Invalid user pool id, it does not contain a region prefix.");
            return AZStd::string();
        }

        return AZStd::string(poolId, 0, regionSize);
    }

    AZStd::string CloudGemPlayerAccountSystemComponent::GetClientId()
    {
        LmbrAWS::CognitoIdentityProvider::Manager& idpManager = gEnv->pLmbrAWS->GetClientManager()->GetCognitoIdentityProviderManager();
        LmbrAWS::CognitoIdentityProvider::IdentityProviderClientSettings& settings = idpManager.GetIdentityProviderClientSettingsCollection().GetSettings(m_userPoolLogicalName.c_str());
        const LmbrAWS::CognitoIdentityProvider::UserPoolApp& clientInfo = settings.clientApps[m_clientAppName.c_str()];

        return clientInfo.clientId.c_str();
    }
} // namespace CloudGemPlayerAccount
