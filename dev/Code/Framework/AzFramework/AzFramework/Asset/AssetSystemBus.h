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
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Math/Crc.h> // ensure that AZ_CRC is available to all users of this header

#include <AzFramework/Asset/AssetSystemTypes.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>

namespace AzFramework
{
    namespace AssetSystem
    {
        //! AssetSystemBusTraits
        //! This bus is for events that concern individual assets and is addressed by file extension
        class AssetSystemNotifications
            : public AZ::EBusTraits
        {
        public:
            typedef AZStd::recursive_mutex MutexType;
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple; // multiple listeners
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
            static const bool EnableEventQueue = true; // enabled queued events, asset messages come from any thread

            virtual ~AssetSystemNotifications() = default;

            //! Called by the AssetProcessor when an asset in the cache has been modified.
            virtual void AssetChanged(AzFramework::AssetSystem::AssetNotificationMessage /*message*/) {}
            //! Called by the AssetProcessor when an asset in the cache has been removed.
            virtual void AssetRemoved(AzFramework::AssetSystem::AssetNotificationMessage /*message*/) {}
        };

        //! AssetSystemInfoBusTraits
        //! This bus is for events that occur in the asset system in general, and has no address
        class AssetSystemInfoNotifications
            : public AZ::EBusTraits
        {
        public:
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple; // multi listener
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;   // single bus

            virtual ~AssetSystemInfoNotifications() = default;

            //! Notifies listeners the compilation of an asset has started.
            virtual void AssetCompilationStarted(const AZStd::string& /*assetPath*/) {}
            //! Notifies listeners the compilation of an asset has succeeded.
            virtual void AssetCompilationSuccess(const AZStd::string& /*assetPath*/) {}
            //! Notifies listeners the compilation of an asset has failed.
            virtual void AssetCompilationFailed(const AZStd::string& /*assetPath*/) {}
            //! Returns the number of assets in queue for processing.
            virtual void CountOfAssetsInQueue(const int& /*count*/) {}
            //! Notifies listeners an error has occurred in the asset system
            virtual void OnError(AssetSystemErrors /*error*/) {}
        };

        //! AssetSystemRequestBusTraits
        //! This bus is for making requests to the asset system
        class AssetSystemRequests
            : public AZ::EBusTraits
        {
        public:
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;   // single listener
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;   // single bus

            virtual ~AssetSystemRequests() = default;

            //! Configure the underlying socket connection
            virtual bool ConfigureSocketConnection(const AZStd::string& branch, const AZStd::string& platform, const AZStd::string& identifier) = 0;
            //! Configure the underlying socket connection and try to connect to the Asset processor
            virtual bool Connect(const char* identifier) = 0;
            //! Compile an asset synchronously
            virtual AssetStatus CompileAssetSync(const AZStd::string& assetPath) = 0;
            //! Retrieve the status of an asset synchronously
            virtual AssetStatus GetAssetStatus(const AZStd::string& assetPath) = 0;
            //! Send out queued events
            virtual void UpdateQueuedEvents() = 0;
            //! Show the AssetProcessor App
            virtual void ShowAssetProcessor() = 0;
            //! Sets the asset processor port to use when connecting
            virtual void SetAssetProcessorPort(AZ::u16 port) = 0;

            //! Compute the ping time between this client and the Asset Processor that's actually handling our requests (proxy relaying is included in the time)
            virtual float GetAssetProcessorPingTimeMilliseconds() = 0;
        };

        //! AssetSystemNegotiationBusTraits
        //! This bus is for events that occur during negotiation
        class AssetSystemConnectionNotifications
            : public AZ::EBusTraits
        {
        public:
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple; // multi listener
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;   // single bus

            virtual ~AssetSystemConnectionNotifications() = default;

            //! Notifies listeners negotiation with Asset Processor failed
            virtual void NegotiationFailed() {};

            //! Notifies listeners that connection to the Asset Processor failed
            virtual void ConnectionFailed() {};
        };
    } // namespace AssetSystem
    using AssetSystemBus = AZ::EBus<AssetSystem::AssetSystemNotifications>;
    using AssetSystemInfoBus = AZ::EBus<AssetSystem::AssetSystemInfoNotifications>;
    using AssetSystemRequestBus = AZ::EBus<AssetSystem::AssetSystemRequests>;
    using AssetSystemConnectionNotificationsBus = AZ::EBus<AssetSystem::AssetSystemConnectionNotifications>;
} // namespace AzFramework
