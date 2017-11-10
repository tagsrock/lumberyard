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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "stdafx.h"
#include "ResourceSelectorHost.h"

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/AssetBrowser/Search/Filter.h>
#include <AzToolsFramework/AssetBrowser/AssetSelectionModel.h>

#include <QMessageBox>

class CResourceSelectorHost
    : public IResourceSelectorHost
{
public:
    CResourceSelectorHost()
    {
        RegisterModuleResourceSelectors(this);
    }

    dll_string SelectResource(const SResourceSelectorContext& context, const char* previousValue) override
    {
        if (!context.typeName)
        {
            assert(false && "SResourceSelectorContext::typeName is not specified");
            return dll_string();
        }

        if (!previousValue)
        {
            assert(false && "previousValue is null");
            return dll_string();
        }

        TTypeMap::iterator it = m_typeMap.find(context.typeName);
        if (it == m_typeMap.end())
        {
            QMessageBox::critical(QApplication::activeWindow(), QString(), QObject::tr("No Resource Selector is registered for resource type \"%1\"").arg(context.typeName));
            return previousValue;
        }

        dll_string result = previousValue;
        if (it->second->function)
        {
            result = it->second->function(context, previousValue);
        }
        else if (it->second->functionWithContext)
        {
            result = it->second->functionWithContext(context, previousValue, context.contextObject);
        }

        return result;
    }

    const char* ResourceIconPath(const char* typeName) const override
    {
        TTypeMap::const_iterator it = m_typeMap.find(typeName);
        if (it != m_typeMap.end())
        {
            return it->second->iconPath;
        }
        return "";
    }

    Serialization::TypeID ResourceContextType(const char* typeName) const override
    {
        TTypeMap::const_iterator it = m_typeMap.find(typeName);
        if (it != m_typeMap.end())
        {
            return it->second->contextType;
        }
        return Serialization::TypeID();
    }

    void RegisterResourceSelector(const SStaticResourceSelectorEntry* entry) override
    {
        m_typeMap[entry->typeName] = entry;
    }

    void SetGlobalSelection(const char* resourceType, const char* value) override
    {
        if (!resourceType)
        {
            return;
        }
        if (!value)
        {
            return;
        }
        m_globallySelectedResources[resourceType] = value;
    }

    const char* GetGlobalSelection(const char* resourceType) const override
    {
        if (!resourceType)
        {
            return "";
        }
        auto it = m_globallySelectedResources.find(resourceType);
        if (it != m_globallySelectedResources.end())
        {
            return it->second.c_str();
        }
        return "";
    }

private:
    typedef std::map<string, const SStaticResourceSelectorEntry*, stl::less_stricmp<string> > TTypeMap;
    TTypeMap m_typeMap;

    std::map<string, string> m_globallySelectedResources;
};

// ---------------------------------------------------------------------------

IResourceSelectorHost* CreateResourceSelectorHost()
{
    return new CResourceSelectorHost();
}

// ---------------------------------------------------------------------------

dll_string SoundFileSelector(const SResourceSelectorContext& x, const char* previousValue)
{
    AssetSelectionModel selection = AssetSelectionModel::AssetTypeSelection("Audio");
    AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::BrowseForAssets, selection);
    if (selection.IsValid())
    {
        return dll_string(Path::FullPathToGamePath(selection.GetResult()->GetFullPath().c_str()));
    }
    else
    {
        return dll_string(Path::FullPathToGamePath(previousValue));
    }
}
REGISTER_RESOURCE_SELECTOR("Sound", SoundFileSelector, "")

// ---------------------------------------------------------------------------
dll_string ModelFileSelector(const SResourceSelectorContext& x, const char* previousValue)
{
    AssetSelectionModel selection = AssetSelectionModel::AssetGroupSelection("Geometry");
    AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::BrowseForAssets, selection);
    if (selection.IsValid())
    {
        return dll_string(Path::FullPathToGamePath(selection.GetResult()->GetFullPath().c_str()));
    }
    else
    {
        return dll_string(Path::FullPathToGamePath(previousValue));
    }
}
REGISTER_RESOURCE_SELECTOR("Model", ModelFileSelector, "")

// ---------------------------------------------------------------------------
dll_string GeomCacheFileSelector(const SResourceSelectorContext& x, const char* previousValue)
{
    AssetSelectionModel selection = AssetSelectionModel::AssetTypeSelection("Geom Cache");
    AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::BrowseForAssets, selection);
    if (selection.IsValid())
    {
        return dll_string(Path::FullPathToGamePath(selection.GetResult()->GetFullPath().c_str()));
    }
    else
    {
        return dll_string(Path::FullPathToGamePath(previousValue));
    }
}

REGISTER_RESOURCE_SELECTOR("GeomCache", GeomCacheFileSelector, "")


