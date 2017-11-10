
#include "StdAfx.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include "LyShineExamplesSystemComponent.h"
#include "UiDynamicContentDatabase.h"

namespace LyShineExamples
{
    void LyShineExamplesSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        UiDynamicContentDatabase::Reflect(context);

        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<LyShineExamplesSystemComponent, AZ::Component>()
                ->Version(0)
                ->SerializerForEmptyClass();

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<LyShineExamplesSystemComponent>("LyShineExamples", "This provides example code using LyShine and code used by sample UI canvases and levels")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "UI")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void LyShineExamplesSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("LyShineExamplesService"));
    }

    void LyShineExamplesSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("LyShineExamplesService"));
    }

    void LyShineExamplesSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("LyShineService"));;
    }

    void LyShineExamplesSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        (void)dependent;
    }

    UiDynamicContentDatabase* LyShineExamplesSystemComponent::GetUiDynamicContentDatabase()
    {
        return m_uiDynamicContentDatabase;
    }

    void LyShineExamplesSystemComponent::Init()
    {
    }

    void LyShineExamplesSystemComponent::Activate()
    {
        m_uiDynamicContentDatabase = new UiDynamicContentDatabase();

        LyShineExamplesRequestBus::Handler::BusConnect();
        LyShineExamplesInternalBus::Handler::BusConnect();
    }

    void LyShineExamplesSystemComponent::Deactivate()
    {
        LyShineExamplesRequestBus::Handler::BusDisconnect();
        LyShineExamplesInternalBus::Handler::BusDisconnect();

        SAFE_DELETE(m_uiDynamicContentDatabase);
    }
}
