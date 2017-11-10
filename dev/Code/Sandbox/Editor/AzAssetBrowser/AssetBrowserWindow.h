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

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Math/Uuid.h>

#include <QDialog>

namespace Ui
{
    class AssetBrowserWindowClass;
}

namespace AssetBrowser
{
    namespace UI
    {
        class AssetTreeView;
        class SortFilterProxyModel;
        class AssetBrowserModel;
    }
}

class AzAssetBrowserWindow
    : public QDialog
{
    Q_OBJECT
public:
    AZ_CLASS_ALLOCATOR(AzAssetBrowserWindow, AZ::SystemAllocator, 0);
    AZ_TYPE_INFO(AzAssetBrowserWindow, "{20238D23-2670-44BC-9110-A51374C18B5A}");

    explicit AzAssetBrowserWindow(const QString& name = "default", QWidget* parent = nullptr);
    virtual ~AzAssetBrowserWindow();

    static const AZ::Uuid& GetClassID();

protected Q_SLOTS:
    void OnContextMenu(const QPoint& point);

private:
    QScopedPointer<Ui::AssetBrowserWindowClass> m_ui;
    QScopedPointer<AssetBrowser::UI::AssetBrowserModel> m_assetDatabaseModel;
    QScopedPointer<AssetBrowser::UI::SortFilterProxyModel> m_assetDatabaseSortFilterProxyModel;
    QString m_name;
    AssetBrowser::UI::AssetTreeView* m_assetBrowser;
    AssetBrowser::UI::AssetBrowserModel* m_assetBrowserModel;
};

extern const char* ASSET_BROWSER_PREVIEW_NAME;
