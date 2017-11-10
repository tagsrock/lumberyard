/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "StdAfx.h"
#include "LmbrCentral.h"

#if !defined(AZ_MONOLITHIC_BUILD)
#include <platform_impl.h> // must be included once per DLL so things from CryCommon will function
#endif

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/std/containers/list.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzFramework/Metrics/MetricsPlainTextNameRegistration.h>

// Component descriptors
#include "Animation/AttachmentComponent.h"
#include "Audio/AudioAreaEnvironmentComponent.h"
#include "Audio/AudioEnvironmentComponent.h"
#include "Audio/AudioListenerComponent.h"
#include "Audio/AudioProxyComponent.h"
#include "Audio/AudioRtpcComponent.h"
#include "Audio/AudioSwitchComponent.h"
#include "Audio/AudioTriggerComponent.h"
#include "Cinematics/SequenceComponent.h"
#include "Cinematics/SequenceAgentComponent.h"
#include "Rendering/DecalComponent.h"
#include "Rendering/StereoRendererComponent.h"
#include "Scripting/FlowGraphComponent.h"
#include "Rendering/LensFlareComponent.h"
#include "Rendering/LightComponent.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/SkinnedMeshComponent.h"
#include "Ai/BehaviorTreeComponent.h"
#include "Ai/NavigationComponent.h"
#include "Rendering/ParticleComponent.h"
#include "Physics/ConstraintComponent.h"
#include "Physics/PhysicsSystemComponent.h"
#include "Physics/CharacterPhysicsComponent.h"
#include "Physics/RagdollComponent.h"
#include "Physics/RigidPhysicsComponent.h"
#include "Physics/StaticPhysicsComponent.h"
#include "Animation/SimpleAnimationComponent.h"
#include "Scripting/TagComponent.h"
#include "Scripting/TriggerAreaComponent.h"
#include "Scripting/SimpleStateComponent.h"
#include "Scripting/SpawnerComponent.h"
#include "Scripting/LookAtComponent.h"
#include "Animation/MannequinScopeComponent.h"
#include "Animation/MannequinComponent.h"
#include "Animation/MotionParameterSmoothingComponent.h"
#include "Animation/CharacterAnimationManagerComponent.h"
#include "Ai/NavigationSystemComponent.h"

// Unhandled asset types
// Animation
#include "Unhandled/Animation/AnimationEventsAssetTypeInfo.h"
#include "Unhandled/Animation/DbaAssetTypeInfo.h"
#include "Unhandled/Animation/MannequinControllerAssetTypeInfo.h"
#include "Unhandled/Animation/MannequinPreviewAssetTypeInfo.h"
#include "Unhandled/Animation/SimpleAnimationAssetTypeInfo.h"
// Material
#include "Unhandled/Material/MaterialAssetTypeInfo.h"
// Other
#include "Unhandled/Other/AudioAssetTypeInfo.h"
#include "Unhandled/Other/CharacterPhysicsAssetTypeInfo.h"
#include "Unhandled/Other/CharacterRigAssetTypeInfo.h"
#include "Unhandled/Other/GeomCacheAssetTypeInfo.h"
#include "Unhandled/Other/GroupAssetTypeInfo.h"
#include "Unhandled/Other/PrefabsLibraryAssetTypeInfo.h"
#include "Unhandled/Other/SkeletonAssetTypeInfo.h"
#include "Unhandled/Other/SkeletonParamsAssetTypeInfo.h"
#include "Unhandled/Other/GameTokenAssetTypeInfo.h"
#include "Unhandled/Other/EntityPrototypeLibraryAssetTypeInfo.h"
#include "Unhandled/Other/SkinnedMeshAssetTypeInfo.h"

// Texture
#include "Unhandled/Texture/SubstanceAssetTypeInfo.h"
#include "Unhandled/Texture/TextureAssetTypeInfo.h"
// Hidden
#include "Unhandled/Hidden/TextureMipmapAssetTypeInfo.h"
//UI
#include <Unhandled/UI/EntityIconAssetTypeInfo.h>
#include "Unhandled/UI/FontAssetTypeInfo.h"
#include "Unhandled/UI/UICanvasAssetTypeInfo.h"

#include <LoadScreenComponent.h>

#include "Physics/Colliders/MeshColliderComponent.h"
#include "Physics/Colliders/PrimitiveColliderComponent.h"

// Asset types
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/Script/ScriptAsset.h>
#include <LmbrCentral/Ai/BehaviorTreeAsset.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>
#include <LmbrCentral/Rendering/LensFlareAsset.h>
#include <LmbrCentral/Rendering/MeshAsset.h>
#include <LmbrCentral/Rendering/ParticleAsset.h>
#include <LmbrCentral/Animation/MannequinAsset.h>

// Asset handlers
#include "Rendering/LensFlareAssetHandler.h"
#include "Rendering/MeshAssetHandler.h"
#include "Rendering/ParticleAssetHandler.h"
#include "Ai/BehaviorTreeAssetHandler.h"

// Scriptable Ebus Registration
#include "Events/ReflectScriptableEvents.h"

// Shape components
#include "Shape/SphereShapeComponent.h"
#include "Shape/BoxShapeComponent.h"
#include "Shape/CylinderShapeComponent.h"
#include "Shape/CapsuleShapeComponent.h"
#include "Shape/CompoundShapeComponent.h"

// Cry interfaces.
#include <ICryAnimation.h>
#include <I3DEngine.h>

namespace LmbrCentral
{
    static const char* s_assetCatalogFilename = "assetcatalog.xml";

    ////////////////////////////////////////////////////////////////////////////
    // LmbrCentral::LmbrCentralModule
    ////////////////////////////////////////////////////////////////////////////

    //! Create ComponentDescriptors and add them to the list.
    //! The descriptors will be registered at the appropriate time.
    //! The descriptors will be destroyed (and thus unregistered) at the appropriate time.
    LmbrCentralModule::LmbrCentralModule()
        : AZ::Module()
    {
        m_descriptors.insert(m_descriptors.end(), {
            AttachmentComponent::CreateDescriptor(),
            AudioAreaEnvironmentComponent::CreateDescriptor(),
            AudioEnvironmentComponent::CreateDescriptor(),
            AudioListenerComponent::CreateDescriptor(),
            AudioProxyComponent::CreateDescriptor(),
            AudioRtpcComponent::CreateDescriptor(),
            AudioSwitchComponent::CreateDescriptor(),
            AudioTriggerComponent::CreateDescriptor(),
            BehaviorTreeComponent::CreateDescriptor(),
            ConstraintComponent::CreateDescriptor(),
            DecalComponent::CreateDescriptor(),
            FlowGraphComponent::CreateDescriptor(),
            LensFlareComponent::CreateDescriptor(),
            LightComponent::CreateDescriptor(),
            LmbrCentralSystemComponent::CreateDescriptor(),
            StaticMeshComponent::CreateDescriptor(),
            SkinnedMeshComponent::CreateDescriptor(),
            NavigationComponent::CreateDescriptor(),
            ParticleComponent::CreateDescriptor(),
            PhysicsSystemComponent::CreateDescriptor(),
            CharacterPhysicsComponent::CreateDescriptor(),
            RagdollComponent::CreateDescriptor(),
            RigidPhysicsComponent::CreateDescriptor(),
            SimpleAnimationComponent::CreateDescriptor(),
            SimpleStateComponent::CreateDescriptor(),
            SpawnerComponent::CreateDescriptor(),
            StaticPhysicsComponent::CreateDescriptor(),
            LookAtComponent::CreateDescriptor(),
            TriggerAreaComponent::CreateDescriptor(),
            TagComponent::CreateDescriptor(),
            MeshColliderComponent::CreateDescriptor(),
            MannequinScopeComponent::CreateDescriptor(),
            MannequinComponent::CreateDescriptor(),
            MotionParameterSmoothingComponent::CreateDescriptor(),
            CharacterAnimationManagerComponent::CreateDescriptor(),
            SphereShapeComponent::CreateDescriptor(),
            BoxShapeComponent::CreateDescriptor(),
            CylinderShapeComponent::CreateDescriptor(),
            CapsuleShapeComponent::CreateDescriptor(),
            PrimitiveColliderComponent::CreateDescriptor(),
            SequenceComponent::CreateDescriptor(),
            SequenceAgentComponent::CreateDescriptor(),
            CompoundShapeComponent::CreateDescriptor(),
            StereoRendererComponent::CreateDescriptor(),
            NavigationSystemComponent::CreateDescriptor(),
#if AZ_LOADSCREENCOMPONENT_ENABLED
            LoadScreenComponent::CreateDescriptor(),
#endif // if AZ_LOADSCREENCOMPONENT_ENABLED
        });

        // This is an internal Amazon gem, so register it's components for metrics tracking, otherwise the name of the component won't get sent back.
        // IF YOU ARE A THIRDPARTY WRITING A GEM, DO NOT REGISTER YOUR COMPONENTS WITH EditorMetricsComponentRegistrationBus
        AZStd::vector<AZ::Uuid> typeIds;
        typeIds.reserve(m_descriptors.size());
        for (AZ::ComponentDescriptor* descriptor : m_descriptors)
        {
            typeIds.emplace_back(descriptor->GetUuid());
        }
        EBUS_EVENT(AzFramework::MetricsPlainTextNameRegistrationBus, RegisterForNameSending, typeIds);
    }

    //! Request system components on the system entity.
    //! These components' memory is owned by the system entity.
    AZ::ComponentTypeList LmbrCentralModule::GetRequiredSystemComponents() const
    {
        return {
            azrtti_typeid<LmbrCentralSystemComponent>(),
            azrtti_typeid<PhysicsSystemComponent>(),
            azrtti_typeid<CharacterAnimationManagerComponent>(),
            azrtti_typeid<StereoRendererComponent>(),
            azrtti_typeid<NavigationSystemComponent>(),
#if AZ_LOADSCREENCOMPONENT_ENABLED
            azrtti_typeid<LoadScreenComponent>(),
#endif // if AZ_LOADSCREENCOMPONENT_ENABLED
        };
    }

    ////////////////////////////////////////////////////////////////////////////
    // LmbrCentralSystemComponent

    void LmbrCentralSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            AzFramework::SimpleAssetReference<MaterialAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<TextureAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<MannequinControllerDefinitionAsset>::Register(*serializeContext);
            AzFramework::SimpleAssetReference<MannequinAnimationDatabaseAsset>::Register(*serializeContext);

            serializeContext->Class<LmbrCentralSystemComponent, AZ::Component>()
                ->Version(1)
                ->SerializerForEmptyClass()
            ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<LmbrCentralSystemComponent>(
                    "LmbrCentral", "Coordinates initialization of systems within LmbrCentral")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Game")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                    ;
            }
        }

        if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            ReflectScriptableEvents::Reflect(behaviorContext);
        }
    }

    void LmbrCentralSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("LmbrCentralService", 0xc3a02410));
    }

    void LmbrCentralSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("LmbrCentralService", 0xc3a02410));
    }

    void LmbrCentralSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
    }

    void LmbrCentralSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("AssetCatalogService", 0xc68ffc57));
    }

    void LmbrCentralSystemComponent::Activate()
    {
        // Register asset handlers. Requires "AssetDatabaseService"
        AZ_Assert(AZ::Data::AssetManager::IsReady(), "Asset manager isn't ready!");

        auto lensFlareAssetHandler = aznew LensFlareAssetHandler;
        lensFlareAssetHandler->Register(); // registers self with AssetManager
        m_assetHandlers.emplace_back(lensFlareAssetHandler);

        auto staticMeshAssetHandler = aznew StaticMeshAssetHandler();
        staticMeshAssetHandler->Register(); // registers self with AssetManager
        m_assetHandlers.emplace_back(staticMeshAssetHandler);

        auto characterDefinitionAssetHandler = aznew CharacterDefinitionAssetHandler();
        characterDefinitionAssetHandler->Register(); // registers self with AssetManager
        m_assetHandlers.emplace_back(characterDefinitionAssetHandler);

        auto particleAssetHandler = aznew ParticleAssetHandler;
        particleAssetHandler->Register(); // registers self with AssetManager
        m_assetHandlers.emplace_back(particleAssetHandler);

        auto behaviorTreeAssetHandler = aznew BehaviorTreeAssetHandler;
        behaviorTreeAssetHandler->Register(); // registers self with AssetManager
        m_assetHandlers.emplace_back(behaviorTreeAssetHandler);

        // Add asset types and extensions to AssetCatalog. Uses "AssetCatalogService".
        auto assetCatalog = AZ::Data::AssetCatalogRequestBus::FindFirstHandler();
        if (assetCatalog)
        {
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<AZ::ScriptAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<LensFlareAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<MaterialAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<StaticMeshAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<CharacterDefinitionAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<ParticleAsset>::Uuid());
            assetCatalog->EnableCatalogForAsset(AZ::AzTypeInfo<BehaviorTreeAsset>::Uuid());

            assetCatalog->AddExtension("cgf");
            assetCatalog->AddExtension("chr");
            assetCatalog->AddExtension("cdf");
            assetCatalog->AddExtension("dds");
            assetCatalog->AddExtension("caf");
            assetCatalog->AddExtension("xml");
            assetCatalog->AddExtension("mtl");
            assetCatalog->AddExtension("lua");
            assetCatalog->AddExtension("sprite");
        }

        CrySystemEventBus::Handler::BusConnect();
        AZ::Data::AssetManagerNotificationBus::Handler::BusConnect();


        // Register unhandled asset type info
        // Animation
        auto animEventsAssetTypeInfo = aznew AnimationEventsAssetTypeInfo();
        animEventsAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(animEventsAssetTypeInfo);

        auto dbaAssetTypeInfo = aznew DbaAssetTypeInfo();
        dbaAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(dbaAssetTypeInfo);

        auto mannequinControllerAssetTypeInfo = aznew MannequinControllerAssetTypeInfo();
        mannequinControllerAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(mannequinControllerAssetTypeInfo);

        auto mannequinPreviewAssetTypeInfo = aznew MannequinPreviewAssetTypeInfo();
        mannequinPreviewAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(mannequinPreviewAssetTypeInfo);

        auto simpleAnimationAssetTypeInfo = aznew SimpleAnimationAssetTypeInfo();
        simpleAnimationAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(simpleAnimationAssetTypeInfo);
        // Geometry
        auto skinnedMeshAssetTypeInfo = aznew SkinnedMeshAssetTypeInfo();
        skinnedMeshAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(skinnedMeshAssetTypeInfo);
        // Material
        auto materialAssetTypeInfo = aznew MaterialAssetTypeInfo();
        materialAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(materialAssetTypeInfo);
        // Other
        auto audioAssetTypeInfo = aznew AudioAssetTypeInfo();
        audioAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(audioAssetTypeInfo);

        auto characterPhysicsAssetTypeInfo = aznew CharacterPhysicsAssetTypeInfo();
        characterPhysicsAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(characterPhysicsAssetTypeInfo);

        auto characterRigAssetTypeInfo = aznew CharacterRigAssetTypeInfo();
        characterRigAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(characterRigAssetTypeInfo);

        auto geomCacheAssetTypeInfo = aznew GeomCacheAssetTypeInfo();
        geomCacheAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(geomCacheAssetTypeInfo);

        auto groupAssetTypeInfo = aznew GroupAssetTypeInfo();
        groupAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(groupAssetTypeInfo);

        auto prefabsLibraryAssetTypeInfo = aznew PrefabsLibraryAssetTypeInfo();
        prefabsLibraryAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(prefabsLibraryAssetTypeInfo);

        auto skeletonAssetTypeInfo = aznew SkeletonAssetTypeInfo();
        skeletonAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(skeletonAssetTypeInfo);

        auto skeletonParamsAssetTypeInfo = aznew SkeletonParamsAssetTypeInfo();
        skeletonParamsAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(skeletonParamsAssetTypeInfo);

        auto entityPrototypeAssetTypeInfo = aznew EntityPrototypeLibraryAssetTypeInfo();
        entityPrototypeAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(entityPrototypeAssetTypeInfo);

        auto gameTokenAssetTypeInfo = aznew GameTokenAssetTypeInfo();
        gameTokenAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(gameTokenAssetTypeInfo);

        // Texture
        auto substanceAssetTypeInfo = aznew SubstanceAssetTypeInfo();
        substanceAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(substanceAssetTypeInfo);

        auto textureAssetTypeInfo = aznew TextureAssetTypeInfo();
        textureAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(textureAssetTypeInfo);

        // Hidden
        auto textureMipmapAssetTypeInfo = aznew TextureMipmapAssetTypeInfo();
        textureMipmapAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(textureMipmapAssetTypeInfo);

        // UI
        auto fontAssetTypeInfo = aznew FontAssetTypeInfo();
        fontAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(fontAssetTypeInfo);

        auto uiCanvasAssetTypeInfo = aznew UICanvasAssetTypeInfo();
        uiCanvasAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(uiCanvasAssetTypeInfo);

        auto entityIconAssetTypeInfo = aznew EntityIconAssetTypeInfo();
        entityIconAssetTypeInfo->Register();
        m_unhandledAssetInfo.emplace_back(entityIconAssetTypeInfo);
    }

    void LmbrCentralSystemComponent::Deactivate()
    {
        AZ::Data::AssetManagerNotificationBus::Handler::BusDisconnect();
        CrySystemEventBus::Handler::BusDisconnect();

        // AssetHandler's destructor calls Unregister()
        m_assetHandlers.clear();
    }

    void LmbrCentralSystemComponent::OnAssetEventsDispatched()
    {
        // Pump deferred engine loading events.
        if (gEnv && gEnv->mMainThreadId == CryGetCurrentThreadId())
        {
            if (gEnv->pCharacterManager)
            {
                gEnv->pCharacterManager->ProcessAsyncLoadRequests();
            }

            if (gEnv->p3DEngine)
            {
                gEnv->p3DEngine->ProcessAsyncStaticObjectLoadRequests();
            }
        }
    }

    void LmbrCentralSystemComponent::OnCrySystemPreInitialize(ISystem& system, const SSystemInitParams& systemInitParams)
    {
        EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, StartMonitoringAssets);
    }

    void LmbrCentralSystemComponent::OnCrySystemInitialized(ISystem& system, const SSystemInitParams& systemInitParams)
    {
#if !defined(AZ_MONOLITHIC_BUILD)
        // When module is linked dynamically, we must set our gEnv pointer.
        // When module is linked statically, we'll share the application's gEnv pointer.
        gEnv = system.GetGlobalEnvironment();
#endif

        REGISTER_INT(s_meshAssetHandler_AsyncCvar, 0, 0, "Enables asynchronous loading of legacy mesh formats");

        // Update the application's asset root.
        // Requires @assets@ alias which is set during CrySystem initialization.
        AZStd::string assetRoot;
        EBUS_EVENT_RESULT(assetRoot, AzFramework::ApplicationRequests::Bus, GetAssetRoot);

        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (fileIO)
        {
            const char* aliasPath = fileIO->GetAlias("@assets@");
            if (aliasPath && aliasPath[0] != '\0')
            {
                assetRoot = aliasPath;
            }
        }

        if (!assetRoot.empty())
        {
            EBUS_EVENT(AzFramework::ApplicationRequests::Bus, SetAssetRoot, assetRoot.c_str());
        }

        // Enable catalog now that application's asset root is set.
        if (system.GetGlobalEnvironment()->IsEditor())
        {
            // In the editor, we build the catalog by scanning the disk.
            if (systemInitParams.pUserCallback)
            {
                systemInitParams.pUserCallback->OnInitProgress("Refreshing asset catalog...");
            }
        }

        // load the catalog from disk (supported over VFS).
        EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, LoadCatalog, AZStd::string::format("@assets@/%s", s_assetCatalogFilename).c_str());
    }

    void LmbrCentralSystemComponent::OnCrySystemShutdown(ISystem& system)
    {
        if (gEnv->pConsole)
        {
            gEnv->pConsole->UnregisterVariable(s_meshAssetHandler_AsyncCvar, true);
        }

        EBUS_EVENT(AZ::Data::AssetCatalogRequestBus, StopMonitoringAssets);

#if !defined(AZ_MONOLITHIC_BUILD)
        gEnv = nullptr;
#endif
    }
} // namespace LmbrCentral

#if !defined(LMBR_CENTRAL_EDITOR)
AZ_DECLARE_MODULE_CLASS(LmbrCentral_ff06785f7145416b9d46fde39098cb0c, LmbrCentral::LmbrCentralModule)
#endif
