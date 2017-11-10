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

#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>

using namespace AzToolsFramework::AssetBrowser;

class QMimeData;

namespace AZ
{
    class AssetBrowserContextProvider
        : public AssetBrowserInteractionNotificationsBus::Handler
    {
    public:
        AssetBrowserContextProvider();
        ~AssetBrowserContextProvider() override;

        void StartDrag(QMimeData* data) override;
        void AddContextMenuActions(QWidget* caller, QMenu* menu, const AZStd::vector<AssetBrowserEntry*>& entries) override;
    };
}