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

#ifndef AZTOOLSFRAMEWORK_ASSETDATABASEAPI_H
#define AZTOOLSFRAMEWORK_ASSETDATABASEAPI_H

#include <AzCore/base.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Asset/AssetCommon.h>

namespace AzToolsFramework
{
    namespace AssetDatabase
    {
        /**
        * Bus used by the Tools Asset Database itself to talk to the running application environment
        * Functions on this bus could be implemented by different parts of the application
        * and are thus not result-based, but instead passed in params which are expected to not be touched
        * unless you have an answer to give.
        */
        class AssetDatabaseRequests
            : public AZ::EBusTraits
        {
        public:
            using Bus = AZ::EBus<AssetDatabaseRequests>;

            /*!
             * Used to retrieve the current database location from the running environment
             */
            virtual bool GetAssetDatabaseLocation(AZStd::string& location) = 0;
        };

        using AssetDatabaseRequestsBus = AZ::EBus<AssetDatabaseRequests>;
    } // namespace AssetDatabase
} // namespace AzToolsFramework

#endif // AZTOOLSFRAMEWORK_ASSETDATABASEAPI_H
