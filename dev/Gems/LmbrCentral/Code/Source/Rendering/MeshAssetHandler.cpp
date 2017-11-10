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

#include <AzCore/std/parallel/conditional_variable.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <LmbrCentral/Rendering/MeshAsset.h>
#include "MeshAssetHandler.h"

#include <ICryAnimation.h>

namespace LmbrCentral
{   
    //////////////////////////////////////////////////////////////////////////

    MeshAssetHandlerBase::MeshAssetHandlerBase()
        : m_asyncLoadCvar(nullptr)
    {
    }

    ICVar* MeshAssetHandlerBase::GetAsyncLoadCVar()
    {
        if (!m_asyncLoadCvar)
        {
            m_asyncLoadCvar = gEnv->pConsole->GetCVar(s_meshAssetHandler_AsyncCvar);
        }

        return m_asyncLoadCvar;
    }

    //////////////////////////////////////////////////////////////////////////
    // Static Mesh Asset Handler
    //////////////////////////////////////////////////////////////////////////

    void AsyncStatObjLoadCallback(const AZ::Data::Asset<StaticMeshAsset>& asset, AZStd::condition_variable* loadVariable, _smart_ptr<IStatObj> statObj)
    {
        if (statObj)
        {
            asset.Get()->m_statObj = statObj;
        }
        else
        {
#if defined(AZ_ENABLE_TRACING)
            AZStd::string assetDescription = asset.GetId().ToString<AZStd::string>();
            EBUS_EVENT_RESULT(assetDescription, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, asset.GetId());
            AZ_Error("MeshAssetHandler", false, "Failed to load mesh asset \"%s\".", assetDescription.c_str());
#endif // AZ_ENABLE_TRACING
        }

        loadVariable->notify_one();
    }

    StaticMeshAssetHandler::~StaticMeshAssetHandler()
    {
        Unregister();
    }

    AZ::Data::AssetPtr StaticMeshAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        (void)type;

        AZ_Assert(type == AZ::AzTypeInfo<StaticMeshAsset>::Uuid(), "Invalid asset type! We handle only 'StaticMeshAsset'");

        AZStd::string assetPath;
        EBUS_EVENT_RESULT(assetPath, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, id);

        return aznew StaticMeshAsset();
    }

    bool StaticMeshAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& /*asset*/, AZ::IO::GenericStream* /*stream*/, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        // Load from preloaded stream.
        AZ_Assert(false, "Favor loading through custom stream override of LoadAssetData, in order to load through CryPak.");
        return false;
    }

    bool StaticMeshAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        AZ_Assert(asset.GetType() == AZ::AzTypeInfo<StaticMeshAsset>::Uuid(), "Invalid asset type! We only load 'StaticMeshAsset'");
        if (StaticMeshAsset* meshAsset = asset.GetAs<StaticMeshAsset>())
        {
            AZ_Assert(!meshAsset->m_statObj.get(), "Attempting to create static mesh without cleaning up the old one.");

            // Strip the alias. StatObj instances are stored in a dictionary by their path,
            // so to share instances with legacy cry entities, we need to use the same un-aliased format.
            static const char assetAliasToken[] = "@assets@/";
            static const size_t assetAliasTokenLen = AZ_ARRAY_SIZE(assetAliasToken) - 1;
            if (0 == strncmp(assetPath, assetAliasToken, assetAliasTokenLen))
            {
                assetPath += assetAliasTokenLen;
            }

            // Temporary cvar guard while async loading of legacy mesh formats is stabilized.
            ICVar* cvar = GetAsyncLoadCVar();
            if (!cvar || cvar->GetIVal() == 0)
            {
                AZStd::mutex loadMutex;
                AZStd::condition_variable loadVariable;

                auto callback = [&asset, &loadVariable](IStatObj* obj)
                {
                    AsyncStatObjLoadCallback(asset, &loadVariable, obj);
                };

                AZStd::unique_lock<AZStd::mutex> loadLock(loadMutex);
                gEnv->p3DEngine->LoadStatObjAsync(callback, assetPath);

                // Block the job thread on a signal variable until notified of completion (by the main thread).
                loadVariable.wait(loadLock);
            }
            else
            {
                _smart_ptr<IStatObj> statObj = gEnv->p3DEngine->LoadStatObjAutoRef(assetPath);

                if (statObj)
                {
                    meshAsset->m_statObj = statObj;
                }
                else
                {
#if defined(AZ_ENABLE_TRACING)
                    AZStd::string assetDescription = asset.GetId().ToString<AZStd::string>();
                    EBUS_EVENT_RESULT(assetDescription, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, asset.GetId());
                    AZ_Error("MeshAssetHandler", false, "Failed to load mesh asset \"%s\".", assetDescription.c_str());
#endif // AZ_ENABLE_TRACING
                }
            }

            return true;
        }
        return false;
    }

    void StaticMeshAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void StaticMeshAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());
    }

    void StaticMeshAssetHandler::Register()
    {
        AZ_Assert(AZ::Data::AssetManager::IsReady(), "Asset manager isn't ready!");
        AZ::Data::AssetManager::Instance().RegisterHandler(this, AZ::AzTypeInfo<StaticMeshAsset>::Uuid());

        AZ::AssetTypeInfoBus::Handler::BusConnect(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());
    }

    void StaticMeshAssetHandler::Unregister()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());

        if (AZ::Data::AssetManager::IsReady())
        {
            AZ::Data::AssetManager::Instance().UnregisterHandler(this);
        }
    }

    AZ::Data::AssetType StaticMeshAssetHandler::GetAssetType() const
    {
        return AZ::AzTypeInfo<StaticMeshAsset>::Uuid();
    }

    const char* StaticMeshAssetHandler::GetAssetTypeDisplayName() const
    {
        return "Static Mesh";
    }

    const char* StaticMeshAssetHandler::GetGroup() const
    {
        return "Geometry";
    }

    const char* StaticMeshAssetHandler::GetBrowserIcon() const
    {
        return "Editor/Icons/Components/StaticMesh.png";
    }

    AZ::Uuid StaticMeshAssetHandler::GetComponentTypeId() const
    {
        return AZ::Uuid("{FC315B86-3280-4D03-B4F0-5553D7D08432}");
    }

    void StaticMeshAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        extensions.push_back(CRY_GEOMETRY_FILE_EXT);
    }

    //////////////////////////////////////////////////////////////////////////
    // Skinned Mesh Asset Handler
    //////////////////////////////////////////////////////////////////////////
    
    void AsyncCharacterInstanceLoadCallback(const AZ::Data::Asset<CharacterDefinitionAsset>& asset, AZStd::condition_variable* loadVariable, ICharacterInstance* instance)
    {
        if (instance)
        {
            asset.Get()->m_characterInstance = instance;
        }
        else
        {
#if defined(AZ_ENABLE_TRACING)
            AZStd::string assetDescription = asset.GetId().ToString<AZStd::string>();
            EBUS_EVENT_RESULT(assetDescription, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, asset.GetId());
            AZ_Error("MeshAssetHandler", false, "Failed to load character instance asset \"%s\".", asset.GetId().ToString<AZStd::string>().c_str());
#endif // AZ_ENABLE_TRACING
        }

        loadVariable->notify_one();
    }

    CharacterDefinitionAssetHandler::~CharacterDefinitionAssetHandler()
    {
        Unregister();
    }

    AZ::Data::AssetPtr CharacterDefinitionAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        (void)type;

        AZ_Assert(type == AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid(), "Invalid asset type! We handle only 'CharacterDefinitionAsset'");

        AZStd::string assetPath;
        EBUS_EVENT_RESULT(assetPath, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, id);
        
        return aznew CharacterDefinitionAsset();
    }

    bool CharacterDefinitionAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& /*asset*/, AZ::IO::GenericStream* /*stream*/, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        // Load from preloaded stream.
        AZ_Assert(false, "Favor loading through custom stream override of LoadAssetData, in order to load through CryPak.");
        return false;
    }

    bool CharacterDefinitionAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        AZ_Assert(asset.GetType() == AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid(), "Invalid asset type! We only load 'CharacterDefinitionAsset'");
        if (CharacterDefinitionAsset* meshAsset = asset.GetAs<CharacterDefinitionAsset>())
        {
            AZ_Assert(!meshAsset->m_characterInstance.get(), "Attempting to create character instance without cleaning up the old one.");

            // Strip the alias. Character instances are stored in a dictionary by their path,
            // so to share instances with legacy cry entities, we need to use the same un-aliased format.
            static const char assetAliasToken[] = "@assets@/";
            static const size_t assetAliasTokenLen = AZ_ARRAY_SIZE(assetAliasToken) - 1;
            if (0 == strncmp(assetPath, assetAliasToken, assetAliasTokenLen))
            {
                assetPath += assetAliasTokenLen;
            }
            
            // Temporary cvar guard while async loading of legacy mesh formats is stabilized.
            ICVar* cvar = GetAsyncLoadCVar();
            if (!cvar || cvar->GetIVal() == 0)
            {
                AZStd::mutex loadMutex;
                AZStd::condition_variable loadVariable;

                auto callback = [&asset, &loadVariable](ICharacterInstance* instance)
                {
                    AsyncCharacterInstanceLoadCallback(asset, &loadVariable, instance);
                };

                AZStd::unique_lock<AZStd::mutex> loadLock(loadMutex);
                gEnv->pCharacterManager->CreateInstanceAsync(callback, assetPath);

                // Block the job thread on a signal variable until notified of completion (by the main thread).
                loadVariable.wait(loadLock);
            }
            else
            {
                ICharacterInstance* instance = gEnv->pCharacterManager->CreateInstance(assetPath);

                if (instance)
                {
                    meshAsset->m_characterInstance = instance;
                }
                else
                {
#if defined(AZ_ENABLE_TRACING)
                    AZStd::string assetDescription = asset.GetId().ToString<AZStd::string>();
                    EBUS_EVENT_RESULT(assetDescription, AZ::Data::AssetCatalogRequestBus, GetAssetPathById, asset.GetId());
                    AZ_Error("MeshAssetHandler", false, "Failed to load character instance asset \"%s\".", asset.GetId().ToString<AZStd::string>().c_str());
#endif // AZ_ENABLE_TRACING
                }
            }

            return true;
        }

        return false;
    }

    void CharacterDefinitionAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void CharacterDefinitionAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid());
    }

    void CharacterDefinitionAssetHandler::Register()
    {
        AZ_Assert(AZ::Data::AssetManager::IsReady(), "Asset manager isn't ready!");
        AZ::Data::AssetManager::Instance().RegisterHandler(this, AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid());

        AZ::AssetTypeInfoBus::Handler::BusConnect(AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid());
    }

    void CharacterDefinitionAssetHandler::Unregister()
    {
        AZ::AssetTypeInfoBus::Handler::BusDisconnect(AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid());

        if (AZ::Data::AssetManager::IsReady())
        {
            AZ::Data::AssetManager::Instance().UnregisterHandler(this);
        }
    }

    AZ::Data::AssetType CharacterDefinitionAssetHandler::GetAssetType() const
    {
        return AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid();
    }

    const char* CharacterDefinitionAssetHandler::GetAssetTypeDisplayName() const
    {
        return "Character Definition";
    }

    const char * CharacterDefinitionAssetHandler::GetGroup() const
    {
        return "Geometry";
    }

    const char* CharacterDefinitionAssetHandler::GetBrowserIcon() const
    {
        return "Editor/Icons/Components/SkinnedMesh.png";
    }

    AZ::Uuid CharacterDefinitionAssetHandler::GetComponentTypeId() const
    {
        return AZ::Uuid("{D3E1A9FC-56C9-4997-B56B-DA186EE2D62A}");
    }

    void CharacterDefinitionAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        extensions.push_back(CRY_CHARACTER_DEFINITION_FILE_EXT);
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace LmbrCentral
