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

#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/Asset/AssetCommon.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Class to build and represent a hierarchical tree view of files and folders containing
//! assets of a given type under a given path
class AssetTreeEntry
{
public: // types
    using FolderMap = AZStd::map<AZStd::string, AssetTreeEntry*>;
    using FileMap = AZStd::map<AZStd::string, AZ::Data::AssetId>;

public: // methods
    ~AssetTreeEntry();

    static AssetTreeEntry* BuildAssetTree(const AZ::Data::AssetType& assetType, const AZStd::string& pathToSearch);

public: // data

    FileMap m_files;
    FolderMap m_folders;

protected:
    void Insert(const AZStd::string& path, const AZStd::string& menuName, const AZ::Data::AssetId& assetId);
};