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

#include <AzToolsFramework/AssetBrowser/Search/Filter.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

#include <QScopedPointer>
#include <QSharedPointer>
#include <QWidgetAction>
#include <QCheckBox>
#include <QString>

class QMenu;
class QAction;

namespace Ui
{
    class SearchAssetTypeSelectorWidgetClass;
}

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        class SearchAssetTypeSelectorWidget
            : public QWidget
        {
            Q_OBJECT
        public:
            AZ_CLASS_ALLOCATOR(SearchAssetTypeSelectorWidget, AZ::SystemAllocator, 0);

            explicit SearchAssetTypeSelectorWidget(QWidget* parent = nullptr);
            ~SearchAssetTypeSelectorWidget() override;

            void ClearAll() const;
            FilterConstType GetFilter() const;
            bool IsLocked() const;

        private:
            QScopedPointer<Ui::SearchAssetTypeSelectorWidgetClass> m_ui;
            QSharedPointer<CompositeFilter> m_filter;
            QCheckBox* m_allCheckbox;
            AZStd::vector<QCheckBox*> m_assetTypeCheckboxes;
            AZStd::unordered_map<QCheckBox*, FilterConstType> m_actionFiltersMapping;
            bool m_locked;

            void AddAssetTypeGroup(QMenu* menu, const QString& group);
            void AddAllAction(QMenu* menu);
        };
    } // namespace AssetBrowser
} // namespace AzToolsFramework
