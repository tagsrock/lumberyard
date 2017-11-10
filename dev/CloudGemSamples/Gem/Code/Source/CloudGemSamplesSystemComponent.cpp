
#include "StdAfx.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include "CloudGemSamplesSystemComponent.h"
#include "Core/EditorGame.h"
#include "System/GameStartup.h"

namespace LYGame
{
    void CloudGemSamplesSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<CloudGemSamplesSystemComponent, AZ::Component>()
                ->Version(0)
                ->SerializerForEmptyClass();

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<CloudGemSamplesSystemComponent>("CloudGemSamples", "[Description of functionality provided by this System Component]")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        // ->Attribute(AZ::Edit::Attributes::Category, "") Set a category
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void CloudGemSamplesSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("CloudGemSamplesService"));
    }

    void CloudGemSamplesSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("CloudGemSamplesService"));
    }

    void CloudGemSamplesSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("CryLegacyService"));
    }

    void CloudGemSamplesSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        (void)dependent;
    }


    IGameStartup* CloudGemSamplesSystemComponent::CreateGameStartup()
    {
        static char buffer[sizeof(GameStartup)];
        return static_cast<IGameStartup*>(new(buffer)GameStartup());
    }

    IEditorGame* CloudGemSamplesSystemComponent::CreateEditorGame()
    {
        return new EditorGame();
    }

    const char* CloudGemSamplesSystemComponent::GetGameName() const
    {
        return GAME_WINDOW_CLASSNAME;
    }

    void CloudGemSamplesSystemComponent::Init()
    {
    }

    void CloudGemSamplesSystemComponent::Activate()
    {
        EditorGameRequestBus::Handler::BusConnect();
        CloudGemSamplesRequestBus::Handler::BusConnect();
    }

    void CloudGemSamplesSystemComponent::Deactivate()
    {
        EditorGameRequestBus::Handler::BusDisconnect();
        CloudGemSamplesRequestBus::Handler::BusDisconnect();
    }
}

#ifdef AZ_MONOLITHIC_BUILD
extern "C"
{
    IGameStartup* CreateGameStartup()
    {
        IGameStartup* pGameStartup = nullptr;
        EditorGameRequestBus::BroadcastResult(pGameStartup, &EditorGameRequestBus::Events::CreateGameStartup);
        return pGameStartup;
    }
}
#endif
