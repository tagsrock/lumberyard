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

#include <AzCore/EBus/EBus.h>
#include <InAppPurchases/InAppPurchasesInterface.h>

namespace InAppPurchases
{
    class InAppPurchasesResponse
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual void ProductInfoRetrieved(const AZStd::vector<AZStd::unique_ptr<ProductDetails const> >& productDetails) { (void)productDetails; }
        virtual void PurchasedProductsRetrieved(const AZStd::vector<AZStd::unique_ptr<PurchasedProductDetails const> >& purchasedProductDetails) { (void)purchasedProductDetails;  }
        virtual void PurchasedProductsRestored(const AZStd::vector<AZStd::unique_ptr<PurchasedProductDetails const> >& purchasedProductDetails) { (void)purchasedProductDetails;  }
        virtual void NewProductPurchased(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void PurchaseCancelled(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void PurchaseRefunded(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void PurchaseFailed(const PurchasedProductDetails* purchasedProductDetails) { (void)purchasedProductDetails; }
        virtual void HostedContentDownloadComplete(const AZStd::string& transactionId, const AZStd::string& downloadedFileLocation) { (void)downloadedFileLocation; (void)transactionId; }
        virtual void HostedContentDownloadFailed(const AZStd::string& transactionId, const AZStd::string& contentId) { (void)transactionId; (void)contentId; }
    };

    using InAppPurchasesResponseBus = AZ::EBus<InAppPurchasesResponse>;

#if defined(AZ_PLATFORM_ANDROID)
    class PurchasedProductDetailsAndroid
        : public PurchasedProductDetails
    {
    public:
        AZ_RTTI(PurchasedProductDetailsAndroid, "{86A7072A-4661-4DAA-A811-F9279B089859}", PurchasedProductDetails);

        const AZStd::string& GetPurchaseSignature() const { return m_purchaseSignature; }
        const AZStd::string& GetPackageName() const { return m_packageName; }
        const AZStd::string& GetPurchaseToken() const { return m_purchaseToken; }
        bool GetIsAutoRenewing() const { return m_autoRenewing; }

        void SetPurchaseSignature(const AZStd::string& purchaseSignature) { m_purchaseSignature = purchaseSignature; }
        void SetPackageName(const AZStd::string& packageName) { m_packageName = packageName; }
        void SetPurchaseToken(const AZStd::string& purchaseToken) { m_purchaseToken = purchaseToken; }
        void SetIsAutoRenewing(bool autoRenewing) { m_autoRenewing = autoRenewing; }

    protected:
        AZStd::string m_purchaseSignature;
        AZStd::string m_packageName;
        AZStd::string m_purchaseToken;
        bool m_autoRenewing;
    };

#elif defined(AZ_PLATFORM_APPLE_IOS)
    class PurchasedProductDetailsApple
        : public PurchasedProductDetails
    {
    public:
        AZ_RTTI(PurchasedProductDetailsApple, "{31C108A3-9676-457A-9F1E-B752DBF96BC6}", PurchasedProductDetails);

        const AZStd::string& GetRestoredOrderId() const { return m_restoredOrderId; }
        AZ::u64 GetSubscriptionExpirationTime() const { return m_subscriptionExpirationTime; }
        AZ::u64 GetRestoredPurchaseTime() const { return m_restoredPurchaseTime; }
        bool GetHasDownloads() const { return m_hasDownloads; }

        void SetRestoredOrderId(const AZStd::string& restoredOrderId) { m_restoredOrderId = restoredOrderId; }
        void SetSubscriptionExpirationTime(AZ::u64 subscriptionExpirationTime) { m_subscriptionExpirationTime = subscriptionExpirationTime; }
        void SetRestoredPurchaseTime(AZ::u64 restoredPurchaseTime) { m_restoredPurchaseTime = restoredPurchaseTime; }
        void SetHasDownloads(bool hasDownloads) { m_hasDownloads = hasDownloads; }

    protected:
        AZStd::string m_restoredOrderId;
        AZ::u64 m_subscriptionExpirationTime;
        AZ::u64 m_restoredPurchaseTime;
        bool m_hasDownloads;
    };
#endif
}
