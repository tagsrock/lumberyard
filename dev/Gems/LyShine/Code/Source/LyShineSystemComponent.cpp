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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include "LyShineSystemComponent.h"
#include "UiSerialize.h"

#include "UiCanvasFileObject.h"
#include "UiCanvasComponent.h"
#include "LyShineDebug.h"
#include "UiElementComponent.h"
#include "UiTransform2dComponent.h"
#include "UiImageComponent.h"
#include "UiTextComponent.h"
#include "UiButtonComponent.h"
#include "UiCheckboxComponent.h"
#include "UiDraggableComponent.h"
#include "UiDropTargetComponent.h"
#include "UiSliderComponent.h"
#include "UiTextInputComponent.h"
#include "UiScrollBarComponent.h"
#include "UiScrollBoxComponent.h"
#include "UiFaderComponent.h"
#include "UiMaskComponent.h"
#include "UiLayoutCellComponent.h"
#include "UiLayoutColumnComponent.h"
#include "UiLayoutRowComponent.h"
#include "UiLayoutGridComponent.h"
#include "UiTooltipComponent.h"
#include "UiTooltipDisplayComponent.h"
#include "UiDynamicLayoutComponent.h"
#include "UiDynamicScrollBoxComponent.h"
#include "UiNavigationSettings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
struct CSystemEventListener_UI
    : public ISystemEventListener
{
public:
    virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
    {
        switch (event)
        {
        case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
        {
            STLALLOCATOR_CLEANUP;
            break;
        }
        }
    }
};
static CSystemEventListener_UI g_system_event_listener_ui;


namespace LyShine
{
    const AZStd::list<AZ::ComponentDescriptor*>* LyShineSystemComponent::m_componentDescriptors = nullptr;

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        UiSerialize::ReflectUiTypes(context);
        UiCanvasFileObject::Reflect(context);
        UiNavigationSettings::Reflect(context);

        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<LyShineSystemComponent, AZ::Component>()
                ->Version(0)
                ->SerializerForEmptyClass();

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<LyShineSystemComponent>("LyShine", "In-game User Interface System")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "UI")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }

        if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<UiCanvasManagerBus>("UiCanvasManagerBus")
                ->Event("CreateCanvas", &UiCanvasManagerBus::Events::CreateCanvas)
                ->Event("LoadCanvas", &UiCanvasManagerBus::Events::LoadCanvas)
                ->Event("UnloadCanvas", &UiCanvasManagerBus::Events::UnloadCanvas)
                ->Event("FindLoadedCanvasByPathName", &UiCanvasManagerBus::Events::FindLoadedCanvasByPathName)
                ;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("LyShineService", 0xae98ab29));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("LyShineService", 0xae98ab29));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        (void)required;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
        dependent.push_back(AZ_CRC("AssetCatalogService", 0xc68ffc57));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::SetLyShineComponentDescriptors(const AZStd::list<AZ::ComponentDescriptor*>* descriptors)
    {
        m_componentDescriptors = descriptors;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::Init()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::Activate()
    {
        LyShineRequestBus::Handler::BusConnect();
        UiSystemBus::Handler::BusConnect();
        UiSystemToolsBus::Handler::BusConnect();

        // register all the component types internal to the LyShine module
        // These are registered in the order we want them to appear in the Add Component menu
        RegisterComponentTypeForMenuOrdering(UiCanvasComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiElementComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiTransform2dComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiImageComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiTextComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiButtonComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiCheckboxComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiSliderComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiTextInputComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiScrollBarComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiScrollBoxComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiDraggableComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiDropTargetComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiFaderComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiMaskComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiLayoutColumnComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiLayoutRowComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiLayoutGridComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiLayoutCellComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiTooltipComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiTooltipDisplayComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiDynamicLayoutComponent::RTTI_Type());
        RegisterComponentTypeForMenuOrdering(UiDynamicScrollBoxComponent::RTTI_Type());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::Deactivate()
    {
        UiSystemBus::Handler::BusDisconnect();
        UiSystemToolsBus::Handler::BusDisconnect();
        LyShineRequestBus::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::InitializeSystem()
    {
        // Not sure if this is still required
        gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_ui);
        
        m_pLyShine = new CLyShine(gEnv->pSystem);
        gEnv->pLyShine = m_pLyShine;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::RegisterComponentTypeForMenuOrdering(const AZ::Uuid& typeUuid)
    {
        m_componentTypes.push_back(typeUuid);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    const AZStd::vector<AZ::Uuid>* LyShineSystemComponent::GetComponentTypesForMenuOrdering()
    {
        return &m_componentTypes;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    const AZStd::list<AZ::ComponentDescriptor*>* LyShineSystemComponent::GetLyShineComponentDescriptors()
    {
        return m_componentDescriptors;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    UiSystemToolsInterface::CanvasAssetHandle* LyShineSystemComponent::LoadCanvasFromStream(AZ::IO::FileIOStream& stream, const AZ::ObjectStream::FilterDescriptor& filterDesc)
    {
        return UiCanvasFileObject::LoadCanvasFromStream(stream, filterDesc);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::SaveCanvasToStream(UiSystemToolsInterface::CanvasAssetHandle* canvas, AZ::IO::FileIOStream& stream)
    {
        UiCanvasFileObject* canvasFileObject = static_cast<UiCanvasFileObject*>(canvas);
        UiCanvasFileObject::SaveCanvasToStream(stream, canvasFileObject);
    }


     AZ::Entity* LyShineSystemComponent::GetRootSliceEntity(CanvasAssetHandle* canvas)
    {
        UiCanvasFileObject* canvasFileObject = static_cast<UiCanvasFileObject*>(canvas);
        return canvasFileObject->m_rootSliceEntity;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    AZ::SliceComponent* LyShineSystemComponent::GetRootSliceSliceComponent(UiSystemToolsInterface::CanvasAssetHandle* canvas)
    {
        UiCanvasFileObject* canvasFileObject = static_cast<UiCanvasFileObject*>(canvas);
        AZ::Entity* rootSliceEntity = canvasFileObject->m_rootSliceEntity;

        if (rootSliceEntity->GetState() == AZ::Entity::ES_CONSTRUCTED)
        {
            rootSliceEntity->Init();
        }

        AZ::SliceComponent* sliceComponent = rootSliceEntity->FindComponent<AZ::SliceComponent>();
        return sliceComponent;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::ReplaceRootSliceSliceComponent(UiSystemToolsInterface::CanvasAssetHandle* canvas, AZ::SliceComponent* newSliceComponent)
    {
        UiCanvasFileObject* canvasFileObject = static_cast<UiCanvasFileObject*>(canvas);
        AZ::Entity* oldRootSliceEntity = canvasFileObject->m_rootSliceEntity;
        AZ::SliceComponent* oldSliceComponent = oldRootSliceEntity->FindComponent<AZ::SliceComponent>();
        delete oldRootSliceEntity;
        AZ::Entity* newRootSliceEntity = aznew AZ::Entity;
        newRootSliceEntity->AddComponent(newSliceComponent);
        canvasFileObject->m_rootSliceEntity = newRootSliceEntity;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void LyShineSystemComponent::DestroyCanvas(CanvasAssetHandle* canvas)
    {
        UiCanvasFileObject* canvasFileObject = static_cast<UiCanvasFileObject*>(canvas);
        delete canvasFileObject->m_canvasEntity;
        delete canvasFileObject->m_rootSliceEntity;
        delete canvasFileObject;
    }

}
