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

#include "StdAfx.h"
#include "Include/IPlugin.h"
#include "PluginManager.h"
#include "Util/FileUtil.h"
#include "AZCore/Debug/Trace.h"

#include <QLibrary>

typedef IPlugin* (* TPfnCreatePluginInstance)(PLUGIN_INIT_PARAM* pInitParam);
typedef void (* TPfnQueryPluginSettings)(SPluginSettings&);

CPluginManager::CPluginManager()
{
    m_currentUUID = 0;
}

CPluginManager::~CPluginManager()
{
    ReleaseAllPlugins();
    UnloadAllPlugins();
}


void CPluginManager::ReleaseAllPlugins()
{
    CLogFile::WriteLine("[Plugin Manager] Releasing all previous plugins");

    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        if (it->pPlugin)
        {
            it->pPlugin->Release();
            it->pPlugin = nullptr;
        }
    }

    m_pluginEventMap.clear();
    m_uuidPluginMap.clear();
}

void CPluginManager::UnloadAllPlugins()
{
    CLogFile::WriteLine("[Plugin Manager] Unloading all previous plugins");

    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        if (it->pPlugin)
        {
            it->pPlugin->Release();
            it->pPlugin = nullptr;
        }

        if (it->hLibrary)
        {
            it->hLibrary->unload();
            delete it->hLibrary;
        }
    }

    m_plugins.clear();
    m_pluginEventMap.clear();
    m_uuidPluginMap.clear();
}


namespace
{
    IPlugin* SafeCallFactory(
        TPfnCreatePluginInstance pfnFactory,
        PLUGIN_INIT_PARAM* pInitParam,
        const char* szFilePath)
    {
        IPlugin* pIPlugin = 0;

        try
        {
            pIPlugin = pfnFactory(pInitParam);
        }
        catch(...)
        {
            if (AZ::Debug::Trace::IsDebuggerPresent())
            {
                AZ::Debug::Trace::Break();
            }

            CLogFile::FormatLine("Can't initialize plugin '%s'! Possible binary version incompatibility. Please reinstall this plugin.", szFilePath);
            return 0;
        }

        return pIPlugin;
    }
}

namespace
{
    struct SPlugin
    {
        QString m_path;
        QString m_name;
        QStringList m_dependencies;
    };

    void ParseManifest(const QString& manifestPath, SPlugin& plugin)
    {
        if (!CFileUtil::FileExists(manifestPath))
        {
            return;
        }

        XmlNodeRef manifest = XmlHelpers::LoadXmlFromFile(manifestPath.toLatin1().data());

        if (manifest)
        {
            const uint numElements = manifest->getChildCount();
            for (uint i = 0; i < numElements; ++i)
            {
                XmlNodeRef element = manifest->getChild(i);
                if (_stricmp(element->getTag(), "Dependency") == 0)
                {
                    const char* dependencyName = element->getContent();
                    stl::push_back_unique(plugin.m_dependencies, dependencyName);
                }
            }
        }
    }

    // This does a topological sort on the plugin list. It will also remove plugins that have
    // missing dependencies or there is a cycle in the dependency tree.
    void SortPluginsByDependency(std::list<SPlugin>& plugins)
    {
        std::list<SPlugin> finalList;
        std::set<QString, stl::less_stricmp<QString> > loadedPlugins;

        while (!plugins.empty())
        {
            bool bCantReduce = true;

            for (auto iter = plugins.begin(); iter != plugins.end(); )
            {
                auto& dependencies = iter->m_dependencies;
                auto newEnd = std::remove_if(dependencies.begin(), dependencies.end(), [&](const QString& dependency)
                        {
                            return (loadedPlugins.find(dependency) != loadedPlugins.end());
                        });

                dependencies.erase(newEnd, dependencies.end());

                if (dependencies.empty())
                {
                    bCantReduce = false;
                    finalList.push_back(*iter);
                    loadedPlugins.insert(iter->m_name);
                    iter = plugins.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }

            if (bCantReduce)
            {
                for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
                {
                    CLogFile::FormatLine("[Plugin Manager] Can't load plugin DLL '%s' because of missing or cyclic dependencies", iter->m_path.toLatin1().data());
                }
                break;
            }
        }

        plugins = finalList;
    }
}

bool CPluginManager::LoadPlugins(const char* pPathWithMask)
{
    QString strPath = QtUtil::ToQString(PathUtil::GetPath(pPathWithMask));
    QString strMask = QString::fromLatin1(PathUtil::GetFile(pPathWithMask));

    CLogFile::WriteLine("[Plugin Manager] Loading plugins...");

    if (!QFileInfo::exists(strPath))
    {
        CLogFile::FormatLine("[Plugin Manager] Cannot find plugin directory '%s'", strPath.toLatin1().data());
        return false;
    }

    std::list<SPlugin> plugins;
    {
        CFileEnum cDLLFiles;
        QFileInfo sFile;
        QString szFilePath;

        if (cDLLFiles.StartEnumeration(strPath, strMask, &sFile))
        {
            do
            {
                // Construct the full filepath of the current file
                szFilePath = sFile.absoluteFilePath();

                SPlugin plugin;
                plugin.m_path = sFile.filePath();
                plugin.m_name = sFile.fileName();
                plugins.push_back(plugin);
            } while (cDLLFiles.GetNextFile(&sFile));
        }
    }

    const QString pluginsDir = Path::GetPath(Path::GetExecutableFullPath()) + "EditorPlugins\\";

    // Loop over found plugins and parse their manifests
    for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
    {
        QString maniFestPath = pluginsDir + iter->m_name + ".mf";
        ParseManifest(maniFestPath, *iter);
    }

    // Sort plugins by dependency
    SortPluginsByDependency(plugins);

    for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
    {
        // Load the plugin's DLL
        QLibrary *hPlugin = new QLibrary(iter->m_path);

        if (!hPlugin->load())
        {
            CLogFile::FormatLine("[Plugin Manager] Can't load plugin DLL '%s' message '%s' !", iter->m_path.toLatin1().data(), hPlugin->errorString().toLatin1().data());
            delete hPlugin;
            continue;
        }

        // Lumberyard:
        // Query the plugin settings, check for manual load...
        TPfnQueryPluginSettings pfnQuerySettings = reinterpret_cast<TPfnQueryPluginSettings>(hPlugin->resolve("QueryPluginSettings"));

        if (pfnQuerySettings != nullptr)
        {
            SPluginSettings settings {
                0
            };
            pfnQuerySettings(settings);
            if (!settings.autoLoad)
            {
                CLogFile::FormatLine("[Plugin Manager] Skipping plugin DLL '%s' because it is marked as non-autoLoad!", iter->m_path.toLatin1().data());
                hPlugin->unload();
                delete hPlugin;
                continue;
            }
        }

        // Query the factory pointer
        TPfnCreatePluginInstance pfnFactory = reinterpret_cast<TPfnCreatePluginInstance>(hPlugin->resolve("CreatePluginInstance"));

        if (!pfnFactory)
        {
            CLogFile::FormatLine("[Plugin Manager] Cannot query plugin DLL '%s' factory pointer (is it a Sandbox plugin?)", iter->m_path.toLatin1().data());
            hPlugin->unload();
            delete hPlugin;
            continue;
        }

        IPlugin* pPlugin = NULL;
        PLUGIN_INIT_PARAM sInitParam =
        {
            GetIEditor(),
            SANDBOX_PLUGIN_SYSTEM_VERSION,
            IPlugin::eError_None
        };

        try
        {
            // Create an instance of the plugin
            pPlugin = SafeCallFactory(pfnFactory, &sInitParam, iter->m_path.toLatin1().data());
        }

        catch (...)
        {
            CLogFile::FormatLine("[Plugin Manager] Cannot initialize plugin '%s'! Possible binary version incompatibility. Please reinstall this plugin.", iter->m_path);
            assert(pPlugin);
            hPlugin->unload();
            delete hPlugin;
            continue;
        }

        if (!pPlugin)
        {
            if (sInitParam.outErrorCode == IPlugin::eError_VersionMismatch)
            {
                CLogFile::FormatLine("[Plugin Manager] Cannot create instance of plugin DLL '%s'! Version mismatch. Please update the plugin.", iter->m_path);
            }
            else
            {
                CLogFile::FormatLine("[Plugin Manager] Cannot create instance of plugin DLL '%s'! Error code %u.", iter->m_path, sInitParam.outErrorCode);
            }

            assert(pPlugin);
            hPlugin->unload();
            delete hPlugin;

            continue;
        }

        RegisterPlugin(hPlugin, pPlugin);

        // Write log string about plugin
        CLogFile::FormatLine("[Plugin Manager] Successfully loaded plugin '%s', version '%i' (GUID: %s)",
            pPlugin->GetPluginName(), pPlugin->GetPluginVersion(), pPlugin->GetPluginGUID());
    }

    return true;
}

void CPluginManager::RegisterPlugin(QLibrary* dllHandle, IPlugin* pPlugin)
{
    SPluginEntry entry;

    entry.hLibrary = dllHandle;
    entry.pPlugin = pPlugin;
    m_plugins.push_back(entry);
    m_uuidPluginMap[m_currentUUID] = pPlugin;
    ++m_currentUUID;
}

IPlugin* CPluginManager::GetPluginByGUID(const char* pGUID)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        const char* pPluginGuid = it->pPlugin->GetPluginGUID();

        if (pPluginGuid && !strcmp(pPluginGuid, pGUID))
        {
            return it->pPlugin;
        }
    }

    return NULL;
}

IPlugin* CPluginManager::GetPluginByUIID(uint8 iUserInterfaceID)
{
    TUIIDPluginIt it;

    it = m_uuidPluginMap.find(iUserInterfaceID);

    if (it == m_uuidPluginMap.end())
    {
        return NULL;
    }

    return (*it).second;
}

IUIEvent* CPluginManager::GetEventByIDAndPluginID(uint8 aPluginID, uint8 aEventID)
{
    // Return the event interface of a user interface element which is
    // specified by its ID and the user interface ID of the plugin which
    // created the UI element

    IPlugin* pPlugin = NULL;
    TEventHandlerIt eventIt;
    TPluginEventIt pluginIt;

    pPlugin = GetPluginByUIID(aPluginID);

    if (!pPlugin)
    {
        return NULL;
    }

    pluginIt = m_pluginEventMap.find(pPlugin);

    if (pluginIt == m_pluginEventMap.end())
    {
        return NULL;
    }

    eventIt = (*pluginIt).second.find(aEventID);

    if (eventIt == (*pluginIt).second.end())
    {
        return NULL;
    }

    return (*eventIt).second;
}

bool CPluginManager::CanAllPluginsExitNow()
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        if (it->pPlugin)
        {
            if (!it->pPlugin->CanExitNow())
            {
                return false;
            }
        }
    }

    return true;
}

void CPluginManager::AddHandlerForCmdID(IPlugin* pPlugin, uint8 aCmdID, IUIEvent* pEvent)
{
    m_pluginEventMap[pPlugin][aCmdID] = pEvent;
}

void CPluginManager::NotifyPlugins(EEditorNotifyEvent aEventId)
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        if (it->pPlugin)
        {
            it->pPlugin->OnEditorNotify(aEventId);
        }
    }
}
