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
#include "UiCanvasComponent.h"

#include "UiElementComponent.h"
#include "UiTransform2dComponent.h"
#include "UiSerialize.h"
#include "UiCanvasFileObject.h"
#include "UiGameEntityContext.h"
#include "UiNavigationHelpers.h"

#include <IRenderer.h>
#include <IHardwareMouse.h>
#include <CryPath.h>
#include <LyShine/Bus/UiInteractableBus.h>
#include <LyShine/Bus/UiInitializationBus.h>
#include <LyShine/Bus/UiNavigationBus.h>
#include <LyShine/Bus/UiTooltipDisplayBus.h>
#include <LyShine/Bus/UiLayoutBus.h>
#include <LyShine/Bus/UiEntityContextBus.h>
#include <LyShine/IUiRenderer.h>
#include <LyShine/UiSerializeHelpers.h>

#include <AzCore/Math/Crc.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/EntityUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/time.h>
#include <AzCore/std/string/conversions.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

#include "Animation/UiAnimationSystem.h"

#include <LyShine/Bus/World/UiCanvasOnMeshBus.h>
#include <LyShine/Bus/World/UiCanvasRefBus.h>

// This is the memory allocation for the static data member used for the debug console variable
AllocateConstIntCVar(UiCanvasComponent, CV_ui_DisplayElemBounds);

////////////////////////////////////////////////////////////////////////////////////////////////////
//! UiCanvasNotificationBus Behavior context handler class 
class UiCanvasNotificationBusBehaviorHandler : public UiCanvasNotificationBus::Handler, public AZ::BehaviorEBusHandler
{
public:
    AZ_EBUS_BEHAVIOR_BINDER(UiCanvasNotificationBusBehaviorHandler, "{64014B4F-E12F-4839-99B0-426B5717DB44}", AZ::SystemAllocator,
        OnAction);

    void OnAction(AZ::EntityId entityId, const LyShine::ActionName& actionName) override
    {
        Call(FN_OnAction, entityId, actionName);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//! UiAnimationNotificationBus Behavior context handler class 
class UiAnimationNotificationBusBehaviorHandler : public UiAnimationNotificationBus::Handler, public AZ::BehaviorEBusHandler
{
public:
    AZ_EBUS_BEHAVIOR_BINDER(UiAnimationNotificationBusBehaviorHandler, "{35D19FE8-5F31-426E-877A-8EEF3A42F99F}", AZ::SystemAllocator,
        OnUiAnimationEvent);

    void OnUiAnimationEvent(IUiAnimationListener::EUiAnimationEvent uiAnimationEvent, AZStd::string animSequenceName) override
    {
        Call(FN_OnUiAnimationEvent, uiAnimationEvent, animSequenceName);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Anonymous namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
    LyShine::CanvasId s_lastCanvasId = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // test if the given text file starts with the given text string
    bool TestFileStartString(const string& pathname, const char* expectedStart)
    {
        // Open the file using CCryFile, this supports it being in the pak file or a standalone file
        CCryFile file;
        if (!file.Open(pathname, "r"))
        {
            return false;
        }

        // get the size of the file and the length of the expected start string
        size_t fileSize = file.GetLength();
        size_t expectedStartLen = strlen(expectedStart);

        // if the file is smaller than the expected start string then it is not a valid file
        if (fileSize < expectedStartLen)
        {
            return false;
        }

        // read in the length of the expected start string
        char* buffer = new char[expectedStartLen];
        size_t bytesRead = file.ReadRaw(buffer, expectedStartLen);

        // match is true if the string read from the file matches the expected start string
        bool match = strncmp(expectedStart, buffer, expectedStartLen) == 0;
        delete [] buffer;
        return match;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Check if the given file was saved using AZ serialization
    bool IsValidAzSerializedFile(const string& pathname)
    {
        return TestFileStartString(pathname, "<ObjectStream");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// STATIC MEMBER DATA
////////////////////////////////////////////////////////////////////////////////////////////////////

const AZ::Vector2 UiCanvasComponent::s_defaultCanvasSize(1280.0f, 720.0f);

bool UiCanvasComponent::s_handleHoverInputEvents = true;

////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent::UiCanvasComponent()
    : m_uniqueId(0)
    , m_rootElement()
    , m_lastElementId(0)
    , m_isPixelAligned(true)
    , m_canvasToViewportMatrix(AZ::Matrix4x4::CreateIdentity())
    , m_viewportToCanvasMatrix(AZ::Matrix4x4::CreateIdentity())
    , m_activeInteractableShouldStayActive(false)
    , m_allowInvalidatingHoverInteractableOnHoverInput(true)
    , m_lastMousePosition(-1.0f, -1.0f)
    , m_id(++s_lastCanvasId)
    , m_drawOrder(0)
    , m_canvasSize(UiCanvasComponent::s_defaultCanvasSize)
    , m_targetCanvasSize(m_canvasSize)
    , m_uniformDeviceScale(1.0f)
    , m_isLoadedInGame(false)
    , m_keepLoadedOnLevelUnload(false)
    , m_enabled(true)
    , m_renderToTexture(false)
    , m_isSnapEnabled(false)
    , m_snapDistance(10.0f)
    , m_snapRotationDegrees(10.0f)
    , m_entityContext(nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent::~UiCanvasComponent()
{
    m_uiAnimationSystem.RemoveAllSequences();

    if (m_entityContext)
    {
        m_entityContext->DestroyUiContext();
    }

    if (m_isLoadedInGame)
    {
        delete m_entityContext;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::UpdateCanvas(float deltaTime, bool isInGame)
{
    if (isInGame)
    {
        // Ignore update if we're not enabled
        if (!m_enabled)
        {
            return;
        }

        EBUS_EVENT_ID(m_rootElement, UiElementBus, UpdateElement);

        // update the animation system
        m_uiAnimationSystem.PreUpdate(deltaTime);
        m_uiAnimationSystem.PostUpdate(deltaTime);
    }

    SendRectChangeNotificationsAndRecomputeLayouts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::RenderCanvas(bool isInGame, AZ::Vector2 viewportSize, bool displayBounds)
{
    // Ignore render ops if we're not enabled
    if (!m_enabled)
    {
        return;
    }

    IUiRenderer::Get()->BeginCanvasRender(viewportSize);
    EBUS_EVENT_ID(m_rootElement, UiElementBus, RenderElement, isInGame, displayBounds);
    IUiRenderer::Get()->EndCanvasRender();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const string& UiCanvasComponent::GetPathname()
{
    return m_pathname;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LyShine::CanvasId UiCanvasComponent::GetCanvasId()
{
    return m_id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::u64 UiCanvasComponent::GetUniqueCanvasId()
{
    return m_uniqueId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int UiCanvasComponent::GetDrawOrder()
{
    return m_drawOrder;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetDrawOrder(int drawOrder)
{
    m_drawOrder = drawOrder;
    EBUS_EVENT(UiCanvasOrderNotificationBus, OnCanvasDrawOrderChanged, GetEntityId());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetKeepLoadedOnLevelUnload()
{
    return m_keepLoadedOnLevelUnload;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetKeepLoadedOnLevelUnload(bool keepLoaded)
{
    m_keepLoadedOnLevelUnload = keepLoaded;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::RecomputeChangedLayouts()
{
    SendRectChangeNotificationsAndRecomputeLayouts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int UiCanvasComponent::GetNumChildElements()
{
    int numChildElements = 0;
    EBUS_EVENT_ID_RESULT(numChildElements, m_rootElement, UiElementBus, GetNumChildElements);
    return numChildElements;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::GetChildElement(int index)
{
    AZ::Entity* child = nullptr;
    EBUS_EVENT_ID_RESULT(child, m_rootElement, UiElementBus, GetChildElement, index);
    return child;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::GetChildElementEntityId(int index)
{
    AZ::EntityId childEntityId;
    EBUS_EVENT_ID_RESULT(childEntityId, m_rootElement, UiElementBus, GetChildEntityId, index);
    return childEntityId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LyShine::EntityArray UiCanvasComponent::GetChildElements()
{
    LyShine::EntityArray childElements;
    EBUS_EVENT_ID_RESULT(childElements, m_rootElement, UiElementBus, GetChildElements);
    return childElements;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZStd::vector<AZ::EntityId> UiCanvasComponent::GetChildElementEntityIds()
{
    AZStd::vector<AZ::EntityId> childElementEntityIds;
    EBUS_EVENT_ID_RESULT(childElementEntityIds, m_rootElement, UiElementBus, GetChildEntityIds);
    return childElementEntityIds;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::CreateChildElement(const LyShine::NameType& name)
{
    AZ::Entity* child = nullptr;
    EBUS_EVENT_ID_RESULT(child, m_rootElement, UiElementBus, CreateChildElement, name);
    return child;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::FindElementById(LyShine::ElementId id)
{
    AZ::Entity* element = nullptr;
    EBUS_EVENT_ID_RESULT(element, m_rootElement, UiElementBus, FindDescendantById, id);
    return element;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::FindElementByName(const LyShine::NameType& name)
{
    AZ::Entity* entity = nullptr;
    EBUS_EVENT_ID_RESULT(entity, m_rootElement, UiElementBus, FindDescendantByName, name);
    return entity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::FindElementEntityIdByName(const LyShine::NameType& name)
{
    AZ::EntityId entityId;
    EBUS_EVENT_ID_RESULT(entityId, m_rootElement, UiElementBus, FindDescendantEntityIdByName, name);
    return entityId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::FindElementsByName(const LyShine::NameType& name, LyShine::EntityArray& result)
{
    // find all elements with the given name
    EBUS_EVENT_ID(m_rootElement, UiElementBus, FindDescendantElements,
        [&name](const AZ::Entity* entity) { return name == entity->GetName(); },
        result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::FindElementByHierarchicalName(const LyShine::NameType& name)
{
    // start at the root
    AZ::Entity* currentEntity = GetRootElement();
    bool found = false;

    std::size_t lastPos = 0;
    while (currentEntity)
    {
        std::size_t pos = name.find('/', lastPos);
        if (pos == lastPos)
        {
            // skip over any double '/' characters or '/' characters at the start
            lastPos++;
        }
        else if (pos == LyShine::NameType::npos)
        {
            // '/' not found, use whole remaining string
            AZ::Entity* entity = nullptr;
            EBUS_EVENT_ID_RESULT(entity, currentEntity->GetId(), UiElementBus,
                FindChildByName, name.substr(lastPos));
            currentEntity = entity;

            if (currentEntity)
            {
                found = true;
            }
            break;
        }
        else
        {
            // use the part of the string between lastPos and pos (between the '/' characters)
            AZ::Entity* entity = nullptr;
            EBUS_EVENT_ID_RESULT(entity, currentEntity->GetId(), UiElementBus,
                FindChildByName, name.substr(lastPos, pos - lastPos));
            currentEntity = entity;
            lastPos = pos + 1;
        }
    }

    return (found) ? currentEntity : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::FindElements(std::function<bool(const AZ::Entity*)> predicate, LyShine::EntityArray& result)
{
    // find all matching elements
    EBUS_EVENT_ID(m_rootElement, UiElementBus, FindDescendantElements, predicate, result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::PickElement(AZ::Vector2 point)
{
    AZ::Entity* element = nullptr;
    EBUS_EVENT_ID_RESULT(element, m_rootElement,
        UiElementBus, FindFrontmostChildContainingPoint, point, m_isLoadedInGame);
    return element;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LyShine::EntityArray UiCanvasComponent::PickElements(const AZ::Vector2& bound0, const AZ::Vector2& bound1)
{
    LyShine::EntityArray elements;
    EBUS_EVENT_ID_RESULT(elements, m_rootElement,
        UiElementBus, FindAllChildrenIntersectingRect, bound0, bound1, m_isLoadedInGame);
    return elements;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::FindInteractableToHandleEvent(AZ::Vector2 point)
{
    AZ::EntityId interactable;
    EBUS_EVENT_ID_RESULT(interactable, m_rootElement, UiElementBus, FindInteractableToHandleEvent, point);
    return interactable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::SaveToXml(const string& assetIdPathname, const string& sourceAssetPathname)
{
    PrepareAnimationSystemForCanvasSave();

    // We are saving to the dev assets (source) not the cache so we use the sourceAssetPathname to save
    // the file
    bool result = SaveCanvasToFile(sourceAssetPathname, AZ::ObjectStream::ST_XML);

    if (result)
    {
        // We store the asset ID so that we can tell if the same file is being loaded from the game
        m_pathname = assetIdPathname;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasInterface::ErrorCode UiCanvasComponent::CheckElementValidToSaveAsPrefab(AZ::Entity* entity)
{
    AZ_Assert(entity, "null entity ptr passed to SaveAsPrefab");

    // Check that none of the EntityId's in this entity or its children reference entities that
    // are not part of the prefab.
    // First make a list of all entityIds that will be in the prefab
    AZStd::vector<AZ::EntityId> entitiesInPrefab = GetEntityIdsOfElementAndDescendants(entity);

    // Next check all entity refs in the element to see if any are externel
    // We use ReplaceEntityRefs even though we don't want to change anything
    bool foundRefOutsidePrefab = false;
    AZ::SerializeContext* context = nullptr;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialization context found");
    AZ::EntityUtils::ReplaceEntityRefs(entity, [&](const AZ::EntityId& key, bool /*isEntityId*/) -> AZ::EntityId
        {
            if (key.IsValid())
            {
                auto iter = AZStd::find(entitiesInPrefab.begin(), entitiesInPrefab.end(), key);
                if (iter == entitiesInPrefab.end())
                {
                    foundRefOutsidePrefab = true;
                }
            }
            return key; // always leave key unchanged
        }, context);

    if (foundRefOutsidePrefab)
    {
        return UiCanvasInterface::ErrorCode::PrefabContainsExternalEntityRefs;
    }

    return UiCanvasInterface::ErrorCode::NoError;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::SaveAsPrefab(const string& pathname, AZ::Entity* entity)
{
    AZ_Assert(entity, "null entity ptr passed to SaveAsPrefab");

    AZ::SerializeContext* context = nullptr;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialization context found");

    // To be sure that we do not save an invalid prefab, if this entity contains entity references
    // outside of the prefab set them to invalid references
    // First make a list of all entityIds that will be in the prefab
    AZStd::vector<AZ::EntityId> entitiesInPrefab = GetEntityIdsOfElementAndDescendants(entity);

    // Next make a serializable object containing all the entities to save (in order to check for invalid refs)
    AZ::SliceComponent::InstantiatedContainer sourceObjects;
    for (const AZ::EntityId& id : entitiesInPrefab)
    {
        AZ::Entity* entity = nullptr;
        EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, id);
        if (entity)
        {
            sourceObjects.m_entities.push_back(entity);
        }
    }

    // clone all the objects in order to replace external references
    AZ::SliceComponent::InstantiatedContainer* clonedObjects = context->CloneObject(&sourceObjects);
    AZ::Entity* clonedRootEntity = clonedObjects->m_entities[0];

    // clear sourceObjects so that its destructor doesn't delete the objects that we cloned from
    sourceObjects.m_entities.clear();

    // use ReplaceEntityRefs to replace external references with invalid IDs
    // Note that we are not generating new IDs so we do not need to fixup internal references
    AZ::EntityUtils::ReplaceEntityRefs(clonedObjects, [&](const AZ::EntityId& key, bool /*isEntityId*/) -> AZ::EntityId
        {
            if (key.IsValid())
            {
                auto iter = AZStd::find(entitiesInPrefab.begin(), entitiesInPrefab.end(), key);
                if (iter == entitiesInPrefab.end())
                {
                    return AZ::EntityId();
                }
            }
            return key; // leave key unchanged
        }, context);

    // make a wrapper object around the prefab entity so that we have an opportunity to change what
    // is in a prefab file in future.
    UiSerialize::PrefabFileObject fileObject;
    fileObject.m_rootEntityId = clonedRootEntity->GetId();

    // add all of the entities that are not the root entity to a childEntities list
    for (auto descendant : clonedObjects->m_entities)
    {
        fileObject.m_entities.push_back(descendant);
    }

    bool result = AZ::Utils::SaveObjectToFile(pathname.c_str(), AZ::ObjectStream::ST_XML, &fileObject);

    // now delete the cloned entities we created, fixed up and saved
    delete clonedObjects;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::LoadFromPrefab(const string& pathname, bool makeUniqueName, AZ::Entity* optionalInsertionPoint)
{
    AZ::Entity* newEntity = nullptr;

    // Currently LoadObjectFromFile will hang if the file cannot be parsed
    // (LMBR-10078). So first check that it is in the right format
    if (!IsValidAzSerializedFile(pathname))
    {
        return nullptr;
    }

    // The top level object in the file is a wrapper object called PrefabFileObject
    // this is to give us more protection against changes to what we store in the file in future
    // NOTE: this read doesn't support pak files but that is OK because prefab files are an
    // editor only feature.
    UiSerialize::PrefabFileObject* fileObject =
        AZ::Utils::LoadObjectFromFile<UiSerialize::PrefabFileObject>(pathname.c_str());
    AZ_Assert(fileObject, "Failed to load prefab");

    if (fileObject)
    {
        // We want new IDs so generate them and fixup all references within the list of entities
        {
            AZ::SerializeContext* context = nullptr;
            EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
            AZ_Assert(context, "No serialization context found");

            AZ::SliceComponent::EntityIdToEntityIdMap entityIdMap;
            AZ::EntityUtils::GenerateNewIdsAndFixRefs(fileObject, entityIdMap, context);
        }

        // add all of the entities to this canvases EntityContext
        m_entityContext->AddUiEntities(fileObject->m_entities);

        EBUS_EVENT_RESULT(newEntity, AZ::ComponentApplicationBus, FindEntity, fileObject->m_rootEntityId);

        delete fileObject;    // we do not keep the file wrapper object around

        if (makeUniqueName)
        {
            AZ::EntityId parentEntityId;
            if (optionalInsertionPoint)
            {
                parentEntityId = optionalInsertionPoint->GetId();
            }
            AZStd::string uniqueName = GetUniqueChildName(parentEntityId, newEntity->GetName(), nullptr);
            newEntity->SetName(uniqueName);
        }

        UiElementComponent* elementComponent = newEntity->FindComponent<UiElementComponent>();
        AZ_Assert(elementComponent, "No element component found on prefab entity");

        AZ::Entity* parent = (optionalInsertionPoint) ? optionalInsertionPoint : GetRootElement();

        // recursively visit all the elements and set their canvas and parent pointers
        elementComponent->FixupPostLoad(newEntity, this, parent, true);

        // add this new entity as a child of the parent (insertionPoint or root)
        UiElementComponent* parentElementComponent = parent->FindComponent<UiElementComponent>();
        AZ_Assert(parentElementComponent, "No element component found on parent entity");
        parentElementComponent->AddChild(newEntity);
    }

    return newEntity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::FixupCreatedEntities(LyShine::EntityArray topLevelEntities, bool makeUniqueNamesAndIds, AZ::Entity* optionalInsertionPoint)
{
    if (makeUniqueNamesAndIds)
    {
        AZ::EntityId parentEntityId;
        if (optionalInsertionPoint)
        {
            parentEntityId = optionalInsertionPoint->GetId();
        }

        LyShine::EntityArray namedChildren;
        for (auto entity : topLevelEntities)
        {
            AZStd::string uniqueName = GetUniqueChildName(parentEntityId, entity->GetName(), &namedChildren);
            entity->SetName(uniqueName);
            namedChildren.push_back(entity);
        }
    }

    AZ::Entity* parent = (optionalInsertionPoint) ? optionalInsertionPoint : GetRootElement();

    for (auto entity : topLevelEntities)
    {
        UiElementComponent* elementComponent = entity->FindComponent<UiElementComponent>();
        AZ_Assert(elementComponent, "No element component found on prefab entity");

        // recursively visit all the elements and set their canvas and parent pointers
        elementComponent->FixupPostLoad(entity, this, parent, makeUniqueNamesAndIds);
    }

    if (m_isLoadedInGame)
    {
        // Call InGamePostActivate on all the created entities
        for (auto entity : topLevelEntities)
        {
            EBUS_EVENT_ID(entity->GetId(), UiInitializationBus, InGamePostActivate);

            LyShine::EntityArray descendantElements;
            EBUS_EVENT_ID(entity->GetId(), UiElementBus, FindDescendantElements,
                [](const AZ::Entity*) { return true; },
                descendantElements);

            for (AZ::Entity* child : descendantElements)
            {
                EBUS_EVENT_ID(child->GetId(), UiInitializationBus, InGamePostActivate);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::AddElement(AZ::Entity* element, AZ::Entity* parent, AZ::Entity* insertBefore)
{
    if (!parent)
    {
        parent = GetRootElement();
    }

    // add this new entity as a child of the parent (insertionPoint or root)
    UiElementComponent* parentElementComponent = parent->FindComponent<UiElementComponent>();
    AZ_Assert(parentElementComponent, "No element component found on parent entity");
    parentElementComponent->AddChild(element, insertBefore);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ReinitializeElements()
{
    AZ::Entity* rootElement = GetRootElement();

    UiElementComponent* elementComponent = rootElement->FindComponent<UiElementComponent>();
    AZ_Assert(elementComponent, "No element component found on root element entity");

    elementComponent->FixupPostLoad(rootElement, this, nullptr, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZStd::string UiCanvasComponent::SaveToXmlString()
{
    PrepareAnimationSystemForCanvasSave();

    AZStd::string charBuffer;
    AZ::IO::ByteContainerStream<AZStd::string > charStream(&charBuffer);
    bool success = SaveCanvasToStream(charStream, AZ::ObjectStream::ST_XML);

    AZ_Assert(success, "Failed to serialize canvas entity to XML");
    return charBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZStd::string UiCanvasComponent::GetUniqueChildName(AZ::EntityId parentEntityId, AZStd::string baseName, const LyShine::EntityArray* includeChildren)
{
    // Get a list of children that the name needs to be unique to
    LyShine::EntityArray children;
    if (parentEntityId.IsValid())
    {
        EBUS_EVENT_ID_RESULT(children, parentEntityId, UiElementBus, GetChildElements);
    }
    else
    {
        children = GetChildElements();
    }

    if (includeChildren)
    {
        children.push_back(*includeChildren);
    }

    // First, check if base name is unique
    if (IsElementNameUnique(baseName, children))
    {
        return baseName;
    }

    // Count trailing digits in base name
    int i;
    for (i = baseName.length() - 1; i >= 0; i--)
    {
        if (!isdigit(baseName[i]))
        {
            break;
        }
    }
    int startDigitIndex = i + 1;
    int numDigits = baseName.length() - startDigitIndex;

    int suffix = 1;
    if (numDigits > 0)
    {
        // Set starting suffix
        suffix = AZStd::stoi(baseName.substr(startDigitIndex, numDigits));

        // Trim the digits from the base name
        baseName.erase(startDigitIndex, numDigits);
    }

    // Keep incrementing suffix until a unique name is found
    // NOTE: This could cause a performance issue when large copies are being made in a large canvas
    AZStd::string proposedChildName;
    do
    {
        ++suffix;

        proposedChildName = baseName;

        AZStd::string suffixString = AZStd::string::format("%d", suffix);

        // Append leading zeros
        int numLeadingZeros = (suffixString.length() < numDigits) ? numDigits - suffixString.length() : 0;
        for (int i = 0; i < numLeadingZeros; i++)
        {
            proposedChildName.push_back('0');
        }

        // Append suffix
        proposedChildName.append(suffixString);
    } while (!IsElementNameUnique(proposedChildName, children));

    return proposedChildName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::CloneElement(AZ::Entity* sourceEntity, AZ::Entity* parentEntity)
{
    return CloneAndAddElementInternal(sourceEntity, parentEntity, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::CloneElementEntityId(AZ::EntityId sourceEntityId, AZ::EntityId parentEntityId, AZ::EntityId insertBeforeId)
{
    AZ::EntityId result;

    AZ::Entity* sourceEntity = nullptr;
    EBUS_EVENT_RESULT(sourceEntity, AZ::ComponentApplicationBus, FindEntity, sourceEntityId);
    if (!sourceEntity)
    {
        AZ_Warning("UI", false, "CloneElementEntityId: Cannot find entity to clone.");
        return result;
    }

    AZ::Entity* parentEntity = nullptr;
    if (parentEntityId.IsValid())
    {
        EBUS_EVENT_RESULT(parentEntity, AZ::ComponentApplicationBus, FindEntity, parentEntityId);
        if (!parentEntity)
        {
            AZ_Warning("UI", false, "CloneElementEntityId: Cannot find parent entity.");
            return result;
        }
    }
    else
    {
        parentEntity = GetRootElement();
    }

    AZ::Entity* insertBeforeEntity = nullptr;
    if (insertBeforeId.IsValid())
    {
        EBUS_EVENT_RESULT(insertBeforeEntity, AZ::ComponentApplicationBus, FindEntity, insertBeforeId);
        if (!insertBeforeEntity)
        {
            AZ_Warning("UI", false, "CloneElementEntityId: Cannot find insertBefore entity.");
            return result;
        }
    }

    AZ::Entity* clonedEntity = CloneAndAddElementInternal(sourceEntity, parentEntity, insertBeforeEntity);

    if (clonedEntity)
    {
        result = clonedEntity->GetId();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::CloneCanvas(const AZ::Vector2& canvasSize)
{
    UiGameEntityContext* entityContext = new UiGameEntityContext();

    UiCanvasComponent* canvasComponent = CloneAndInitializeCanvas(entityContext, m_pathname, &canvasSize);
    AZ::Entity* newCanvasEntity = nullptr;
    if (canvasComponent)
    {
        newCanvasEntity = canvasComponent->GetEntity();
        canvasComponent->m_isLoadedInGame = true;

        // The game entity context needs to know its corresponding canvas entity for instantiating dynamic slices
        entityContext->SetCanvasEntity(newCanvasEntity->GetId());
    }
    else
    {
        delete entityContext;
    }

    return newCanvasEntity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetCanvasToViewportMatrix(const AZ::Matrix4x4& matrix)
{
    if (!m_canvasToViewportMatrix.IsClose(matrix))
    {
        m_canvasToViewportMatrix = matrix;
        m_viewportToCanvasMatrix = m_canvasToViewportMatrix.GetInverseTransform();
        EBUS_EVENT_ID(GetRootElement()->GetId(), UiTransformBus, SetRecomputeTransformFlag);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const AZ::Matrix4x4& UiCanvasComponent::GetCanvasToViewportMatrix()
{
    return m_canvasToViewportMatrix;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::GetViewportToCanvasMatrix(AZ::Matrix4x4& matrix)
{
    matrix = m_viewportToCanvasMatrix;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiCanvasComponent::GetCanvasSize()
{
    return m_targetCanvasSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetCanvasSize(const AZ::Vector2& canvasSize)
{
    m_canvasSize = canvasSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetTargetCanvasSize(bool isInGame, const AZ::Vector2& targetCanvasSize)
{
    if (m_renderToTexture)
    {
        // when a canvas is set to render to texture the target canvas size is always the authored canvas size
        SetTargetCanvasSizeAndUniformScale(isInGame, m_canvasSize);
    }
    else
    {
        SetTargetCanvasSizeAndUniformScale(isInGame, targetCanvasSize);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float UiCanvasComponent::GetUniformDeviceScale()
{
    return m_uniformDeviceScale;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetIsPixelAligned()
{
    return m_isPixelAligned;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetIsPixelAligned(bool isPixelAligned)
{
    m_isPixelAligned = isPixelAligned;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IUiAnimationSystem* UiCanvasComponent::GetAnimationSystem()
{
    return &m_uiAnimationSystem;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetEnabled()
{
    return m_enabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetIsRenderToTexture()
{
    return m_renderToTexture;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetIsRenderToTexture(bool isRenderToTexture)
{
    m_renderToTexture = isRenderToTexture;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZStd::string UiCanvasComponent::GetRenderTargetName()
{
    return m_renderTargetName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetRenderTargetName(const AZStd::string& name)
{
    if (name != m_renderTargetName && !name.empty())
    {
        DestroyRenderTarget();
        m_renderTargetName = name;
        CreateRenderTarget();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetIsPositionalInputSupported()
{
    return m_isPositionalInputSupported;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetIsPositionalInputSupported(bool isSupported)
{
    m_isPositionalInputSupported = isSupported;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetIsNavigationSupported()
{
    return m_isNavigationSupported;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetIsNavigationSupported(bool isSupported)
{
    m_isNavigationSupported = isSupported;
    SetFirstHoverInteractable();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleInputEvent(const SInputEvent& event)
{
    // Ignore input events if we're not enabled
    if (!m_enabled)
    {
        return false;
    }

    if (((event.deviceType == eIDT_Keyboard) && (event.state != eIS_UI)) || (event.deviceType == eIDT_Gamepad))
    {
        return HandleKeyInputEvent(event);
    }
    else
    {
        if (!m_renderToTexture && m_isPositionalInputSupported)
        {
            if (HandleInputPositionalEvent(event, AZ::Vector2(event.screenPosition.x, event.screenPosition.y)))
            {
                return true;
            }
        }

        // NOTE: in the Editor the keyboard events come through this path
        if (event.state == eIS_UI)
        {
            if (m_activeInteractable.IsValid())
            {
                EBUS_EVENT_ID(m_activeInteractable, UiInteractableBus, HandleCharacterInput, event.inputChar);
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleKeyboardEvent(const SUnicodeEvent& event)
{
    // Ignore input events if we're not enabled
    if (!m_enabled)
    {
        return false;
    }

    if (m_activeInteractable.IsValid())
    {
        EBUS_EVENT_ID(m_activeInteractable, UiInteractableBus, HandleCharacterInput, event.inputChar);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleInputPositionalEvent(const SInputEvent& event, AZ::Vector2 viewportPos)
{
    if (event.deviceType == eIDT_Mouse)
    {
        if (m_lastMousePosition != viewportPos)
        {
            // Check if the mouse position has been initialized
            if (m_lastMousePosition.GetX() >= 0.0f && m_lastMousePosition.GetY() >= 0.0f)
            {
                // Mouse moved, resume handling hover input events if there is no active interactable
                if (!m_activeInteractable.IsValid())
                {
                    s_handleHoverInputEvents = true;
                }
            }
            m_lastMousePosition = viewportPos;
        }
    }

    if (event.deviceType == eIDT_Mouse || event.keyId == eKI_Touch0)
    {
        if (s_handleHoverInputEvents)
        {
            HandleHoverInputEvent(viewportPos);
        }
    }

    // Currently we are just interested in mouse button 1 events and UI events here
    if (event.keyId == eKI_Mouse1 || event.keyId == eKI_Touch0)
    {
        if (event.state == eIS_Down)
        {
            // Currently we handle dragging in HandleHardwareMouseEvent
            return false;
        }

        if (event.state == eIS_Pressed || event.state == eIS_Released)
        {
            const AZ::Vector2& point = viewportPos;

            if (event.state == eIS_Pressed)
            {
                return HandlePrimaryPress(point);
            }
            else if (event.state == eIS_Released)
            {
                return HandlePrimaryRelease(point, event.keyId);
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiCanvasComponent::GetMousePosition()
{
    return m_lastMousePosition;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::GetTooltipDisplayElement()
{
    return m_tooltipDisplayElement;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetTooltipDisplayElement(AZ::EntityId entityId)
{
    m_tooltipDisplayElement = entityId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::GetIsSnapEnabled()
{
    return m_isSnapEnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetIsSnapEnabled(bool enabled)
{
    m_isSnapEnabled = enabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float UiCanvasComponent::GetSnapDistance()
{
    return m_snapDistance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetSnapDistance(float distance)
{
    m_snapDistance = distance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float UiCanvasComponent::GetSnapRotationDegrees()
{
    return m_snapRotationDegrees;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetSnapRotationDegrees(float degrees)
{
    m_snapRotationDegrees = degrees;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ForceActiveInteractable(AZ::EntityId interactableId, bool shouldStayActive, AZ::Vector2 point)
{
    SetHoverInteractable(interactableId);
    SetActiveInteractable(interactableId, shouldStayActive);
    m_lastMousePosition = point;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetHoverInteractable(AZ::EntityId newHoverInteractable)
{
    if (m_hoverInteractable != newHoverInteractable)
    {
        ClearHoverInteractable();

        m_hoverInteractable = newHoverInteractable;
        if (m_hoverInteractable.IsValid())
        {
            EBUS_EVENT_ID(m_hoverInteractable, UiInteractableBus, HandleHoverStart);

            // we want to know if this entity is deactivated or destroyed
            // (unlikely: while hovered over we can't be in edit mode, could happen from C++ interface though)
            AZ::EntityBus::Handler::BusConnect(m_hoverInteractable);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::OnEntityDeactivated(const AZ::EntityId& entityId)
{
    AZ::EntityBus::Handler::BusDisconnect(entityId);

    if (entityId == m_hoverInteractable)
    {
        m_hoverInteractable.SetInvalid();

        // If we are using keybord/gamepad navigation we should set a new hover interactable
        SetFirstHoverInteractable();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::StartSequence(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        m_uiAnimationSystem.AddUiAnimationListener(sequence, this);
        m_uiAnimationSystem.PlaySequence(sequence, nullptr, false, false);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::StopSequence(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        m_uiAnimationSystem.StopSequence(sequence);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::AbortSequence(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        m_uiAnimationSystem.AbortSequence(sequence);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::PauseSequence(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        sequence->Pause();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ResumeSequence(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        sequence->Resume();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ResetSequence(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        sequence->Reset(true);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float UiCanvasComponent::GetSequencePlayingSpeed(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    return m_uiAnimationSystem.GetPlayingSpeed(sequence);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetSequencePlayingSpeed(const AZStd::string& sequenceName, float speed)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    m_uiAnimationSystem.SetPlayingSpeed(sequence, speed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float UiCanvasComponent::GetSequencePlayingTime(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    return m_uiAnimationSystem.GetPlayingTime(sequence);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::IsSequencePlaying(const AZStd::string& sequenceName)
{
    IUiAnimSequence* sequence = m_uiAnimationSystem.FindSequence(sequenceName.c_str());
    if (sequence)
    {
        return m_uiAnimationSystem.IsPlaying(sequence);
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ActiveCancelled()
{
    // currently we are only connected to one UiInteractableActiveNotificationBus so we know it is the
    // pressed interactable. If we could be connected to several we would need to change
    // the ActiveCancelled method to pass the EntityId.
    if (m_activeInteractable.IsValid())
    {
        UiInteractableActiveNotificationBus::Handler::BusDisconnect(m_activeInteractable);
        m_activeInteractable.SetInvalid();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// change the active interactable to the given one
void UiCanvasComponent::ActiveChanged(AZ::EntityId m_newActiveInteractable, bool shouldStayActive)
{
    // There should always be an active interactable at this point, disconnect from it
    if (m_activeInteractable.IsValid())
    {
        UiInteractableActiveNotificationBus::Handler::BusDisconnect(m_activeInteractable);
        m_activeInteractable.SetInvalid();
    }

    // The m_newActiveInteractable should always be valid but check anyway
    if (m_newActiveInteractable.IsValid())
    {
        m_activeInteractable = m_newActiveInteractable;
        UiInteractableActiveNotificationBus::Handler::BusConnect(m_activeInteractable);
        m_activeInteractableShouldStayActive = shouldStayActive;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::OnPreRender()
{
    bool displayBounds = false;
#ifndef EXCLUDE_DOCUMENTATION_PURPOSE
    // If the console variable is set then display the element bounds
    // We use deferred render for the bounds so that they draw on top of everything else
    // this only works when running in-game
    if (CV_ui_DisplayElemBounds)
    {
        displayBounds = true;
    }
#endif

    RenderCanvasToTexture(displayBounds);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::OnUiAnimationEvent(EUiAnimationEvent uiAnimationEvent, IUiAnimSequence* pAnimSequence)
{
    // Queue the event to prevent deletions during the canvas update
    EBUS_QUEUE_EVENT_ID(GetEntityId(), UiAnimationNotificationBus, OnUiAnimationEvent, uiAnimationEvent, pAnimSequence->GetName());

    // Stop listening to events
    if ((uiAnimationEvent == EUiAnimationEvent::eUiAnimationEvent_Stopped) || (uiAnimationEvent == EUiAnimationEvent::eUiAnimationEvent_Aborted))
    {
        m_uiAnimationSystem.RemoveUiAnimationListener(pAnimSequence, this);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::GetRootElement() const
{
    AZ::Entity* rootEntity = nullptr;
    EBUS_EVENT_RESULT(rootEntity, AZ::ComponentApplicationBus, FindEntity, m_rootElement);
    return rootEntity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LyShine::ElementId UiCanvasComponent::GenerateId()
{
    return ++m_lastElementId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Vector2 UiCanvasComponent::GetTargetCanvasSize()
{
    return m_targetCanvasSize;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC STATIC MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

void UiCanvasComponent::Reflect(AZ::ReflectContext* context)
{
    AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

    if (serializeContext)
    {
        UiAnimationSystem::Reflect(serializeContext);

        serializeContext->Class<UiCanvasComponent, AZ::Component>()
            ->Version(3, &VersionConverter)
            // Not in properties pane
            ->Field("UniqueId", &UiCanvasComponent::m_uniqueId)
            ->Field("RootElement", &UiCanvasComponent::m_rootElement)
            ->Field("LastElement", &UiCanvasComponent::m_lastElementId)
            ->Field("DrawOrder", &UiCanvasComponent::m_drawOrder)
            ->Field("CanvasSize", &UiCanvasComponent::m_canvasSize)
            ->Field("IsSnapEnabled", &UiCanvasComponent::m_isSnapEnabled)
             // Rendering group
            ->Field("IsPixelAligned", &UiCanvasComponent::m_isPixelAligned)
            ->Field("RenderToTexture", &UiCanvasComponent::m_renderToTexture)
            ->Field("RenderTargetName", &UiCanvasComponent::m_renderTargetName)
            // Input group
            ->Field("IsPosInputSupported", &UiCanvasComponent::m_isPositionalInputSupported)
            ->Field("IsNavigationSupported", &UiCanvasComponent::m_isNavigationSupported)
            ->Field("FirstHoverElement", &UiCanvasComponent::m_firstHoverInteractable)
            ->Field("AnimSystem", &UiCanvasComponent::m_uiAnimationSystem)
            ->Field("AnimationData", &UiCanvasComponent::m_serializedAnimationData)
            // Tooltips group
            ->Field("TooltipDisplayElement", &UiCanvasComponent::m_tooltipDisplayElement)
            // Editor settings
            ->Field("SnapDistance", &UiCanvasComponent::m_snapDistance)
            ->Field("SnapRotationDegrees", &UiCanvasComponent::m_snapRotationDegrees);

        AZ::EditContext* ec = serializeContext->GetEditContext();
        if (ec)
        {
            auto editInfo = ec->Class<UiCanvasComponent>("UI Canvas", "These are the properties of the UI canvas.");

            editInfo->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/UiCanvas.png")
                ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/UiCanvas.png")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->ClassElement(AZ::Edit::ClassElements::Group, "Rendering")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement(AZ::Edit::UIHandlers::CheckBox, &UiCanvasComponent::m_isPixelAligned, "Is pixel aligned", "When checked, all corners of all elements will be rounded to the nearest pixel.");
            editInfo->DataElement(AZ::Edit::UIHandlers::CheckBox, &UiCanvasComponent::m_renderToTexture, "Render to texture", "When checked, the canvas is rendered to a texture instead of the full screen.")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ_CRC("RefreshEntireTree", 0xefbc823c));
            editInfo->DataElement(0, &UiCanvasComponent::m_renderTargetName, "Render target", "The name of the texture that is created when this canvas renders to a texture.")
                ->Attribute(AZ::Edit::Attributes::Visibility, &UiCanvasComponent::m_renderToTexture);

            editInfo->ClassElement(AZ::Edit::ClassElements::Group, "Input")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement(AZ::Edit::UIHandlers::CheckBox, &UiCanvasComponent::m_isPositionalInputSupported, "Handle positional", "When checked, positional input (mouse/touch) will automatically be handled.");
            editInfo->DataElement(AZ::Edit::UIHandlers::CheckBox, &UiCanvasComponent::m_isNavigationSupported, "Handle navigation", "When checked, keyboard/gamepad events will automatically be used for navigation.");
            editInfo->DataElement(AZ::Edit::UIHandlers::ComboBox, &UiCanvasComponent::m_firstHoverInteractable, "First focus elem", "The element to receive focus when the canvas loads.")
                ->Attribute("EnumValues", &UiCanvasComponent::PopulateNavigableEntityList);

            editInfo->ClassElement(AZ::Edit::ClassElements::Group, "Tooltips")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement(AZ::Edit::UIHandlers::ComboBox, &UiCanvasComponent::m_tooltipDisplayElement, "Tooltip display elem", "The element to be displayed when hovering over an interactable.")
                ->Attribute("EnumValues", &UiCanvasComponent::PopulateTooltipDisplayEntityList);

            editInfo->ClassElement(AZ::Edit::ClassElements::Group, "Editor settings")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true);

            editInfo->DataElement(AZ::Edit::UIHandlers::Default, &UiCanvasComponent::m_snapDistance, "Snap distance", "The snap grid spacing.")
                ->Attribute(AZ::Edit::Attributes::Min, 1.0f);
            editInfo->DataElement(AZ::Edit::UIHandlers::Default, &UiCanvasComponent::m_snapRotationDegrees, "Snap rotation", "The degrees of rotation to snap to.")
                ->Attribute(AZ::Edit::Attributes::Min, 1.0f)
                ->Attribute(AZ::Edit::Attributes::Max, 359.0f)
                ->Attribute(AZ::Edit::Attributes::Suffix, " degrees");
        }
    }

    AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
    if (behaviorContext)
    {
        behaviorContext->EBus<UiCanvasBus>("UiCanvasBus")
            ->Event("GetDrawOrder", &UiCanvasBus::Events::GetDrawOrder)
            ->Event("SetDrawOrder", &UiCanvasBus::Events::SetDrawOrder)
            ->Event("GetKeepLoadedOnLevelUnload", &UiCanvasBus::Events::GetKeepLoadedOnLevelUnload)
            ->Event("SetKeepLoadedOnLevelUnload", &UiCanvasBus::Events::SetKeepLoadedOnLevelUnload)
            ->Event("RecomputeChangedLayouts", &UiCanvasBus::Events::RecomputeChangedLayouts)
            ->Event("GetNumChildElements", &UiCanvasBus::Events::GetNumChildElements)
            ->Event("GetChildElement", &UiCanvasBus::Events::GetChildElementEntityId)
            ->Event("GetChildElements", &UiCanvasBus::Events::GetChildElementEntityIds)
            ->Event("FindElementByName", &UiCanvasBus::Events::FindElementEntityIdByName)
            ->Event("CloneElement", &UiCanvasBus::Events::CloneElementEntityId)
            ->Event("GetIsPixelAligned", &UiCanvasBus::Events::GetIsPixelAligned)
            ->Event("SetIsPixelAligned", &UiCanvasBus::Events::SetIsPixelAligned)
            ->Event("GetEnabled", &UiCanvasBus::Events::GetEnabled)
            ->Event("SetEnabled", &UiCanvasBus::Events::SetEnabled)
            ->Event("GetIsRenderToTexture", &UiCanvasBus::Events::GetIsRenderToTexture)
            ->Event("SetIsRenderToTexture", &UiCanvasBus::Events::SetIsRenderToTexture)
            ->Event("GetRenderTargetName", &UiCanvasBus::Events::GetRenderTargetName)
            ->Event("SetRenderTargetName", &UiCanvasBus::Events::SetRenderTargetName)
            ->Event("GetIsPositionalInputSupported", &UiCanvasBus::Events::GetIsPositionalInputSupported)
            ->Event("SetIsPositionalInputSupported", &UiCanvasBus::Events::SetIsPositionalInputSupported)
            ->Event("GetIsNavigationSupported", &UiCanvasBus::Events::GetIsNavigationSupported)
            ->Event("SetIsNavigationSupported", &UiCanvasBus::Events::SetIsNavigationSupported)
            ->Event("GetTooltipDisplayElement", &UiCanvasBus::Events::GetTooltipDisplayElement)
            ->Event("SetTooltipDisplayElement", &UiCanvasBus::Events::SetTooltipDisplayElement)
            ->Event("SetHoverInteractable", &UiCanvasBus::Events::SetHoverInteractable);

        behaviorContext->EBus<UiCanvasNotificationBus>("UiCanvasNotificationBus")
            ->Handler<UiCanvasNotificationBusBehaviorHandler>();

        behaviorContext->EBus<UiAnimationBus>("UiAnimationBus")
            ->Event("StartSequence", &UiAnimationBus::Events::StartSequence)
            ->Event("StopSequence", &UiAnimationBus::Events::StopSequence)
            ->Event("AbortSequence", &UiAnimationBus::Events::AbortSequence)
            ->Event("PauseSequence", &UiAnimationBus::Events::PauseSequence)
            ->Event("ResumeSequence", &UiAnimationBus::Events::ResumeSequence)
            ->Event("ResetSequence", &UiAnimationBus::Events::ResetSequence)
            ->Event("GetSequencePlayingSpeed", &UiAnimationBus::Events::GetSequencePlayingSpeed)
            ->Event("SetSequencePlayingSpeed", &UiAnimationBus::Events::SetSequencePlayingSpeed)
            ->Event("GetSequencePlayingTime", &UiAnimationBus::Events::GetSequencePlayingTime)
            ->Event("IsSequencePlaying", &UiAnimationBus::Events::IsSequencePlaying);

        behaviorContext->Enum<(int)IUiAnimationListener::EUiAnimationEvent::eUiAnimationEvent_Started>("eUiAnimationEvent_Started")
            ->Enum<(int)IUiAnimationListener::EUiAnimationEvent::eUiAnimationEvent_Stopped>("eUiAnimationEvent_Stopped")
            ->Enum<(int)IUiAnimationListener::EUiAnimationEvent::eUiAnimationEvent_Aborted>("eUiAnimationEvent_Aborted")
            ->Enum<(int)IUiAnimationListener::EUiAnimationEvent::eUiAnimationEvent_Updated>("eUiAnimationEvent_Updated");

        behaviorContext->EBus<UiAnimationNotificationBus>("UiAnimationNotificationBus")
            ->Handler<UiAnimationNotificationBusBehaviorHandler>();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::Initialize()
{
    s_handleHoverInputEvents = true;

#ifndef EXCLUDE_DOCUMENTATION_PURPOSE
    // Define a debug console variable that controls display of all element bounds when in game
    DefineConstIntCVar3("ui_DisplayElemBounds", CV_ui_DisplayElemBounds, 0, VF_CHEAT,
        "0=off, 1=display the UI element bounding boxes");
#endif // EXCLUDE_DOCUMENTATION_PURPOSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::Shutdown()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PROTECTED MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

void UiCanvasComponent::Init()
{
    // We don't know whether we're in editor or game yet, but if we're in the editor
    // we need to know the authored canvas size to ensure certain properties are displayed
    // correctly in the editor window. If we're in game, the target canvas size will be
    // initialized to the viewport on the first render loop.
    m_targetCanvasSize = m_canvasSize;

    if (m_uniqueId == 0)
    {
        // Initialize unique Id
        m_uniqueId = CreateUniqueId();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::Activate()
{
    UiCanvasBus::Handler::BusConnect(m_entity->GetId());
    UiAnimationBus::Handler::BusConnect(m_entity->GetId());

    // Note: this will create a render target even when the canvas is being used in the editor which is
    // unnecessary but harmless. It will not actually be used as a render target unless we are running in game.
    // An alternative would be to create in on first use.
    if (m_renderToTexture)
    {
        CreateRenderTarget();
    }

    m_layoutManager = new UiLayoutManager(GetEntityId());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::Deactivate()
{
    UiCanvasBus::Handler::BusDisconnect();
    UiAnimationBus::Handler::BusDisconnect();

    if (m_renderToTexture)
    {
        DestroyRenderTarget();
    }

    delete m_layoutManager;
    m_layoutManager = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleHoverInputEvent(AZ::Vector2 point)
{
    bool result = false;

    // We don't change the active interactable here. Some interactables may want to still be considered
    // pressed if the mouse moves outside their bounds while they are pressed.

    // However, the active interactable does influence how hover works, if there is an active
    // interactable then that is the only one that can be the hoverInteractable
    AZ::EntityId latestHoverInteractable;
    if (m_activeInteractable.IsValid())
    {
        // check if the mouse is hovering over the active interactable
        bool hoveringOnActive = false;
        EBUS_EVENT_ID_RESULT(hoveringOnActive, m_activeInteractable, UiTransformBus, IsPointInRect, point);

        if (hoveringOnActive)
        {
            latestHoverInteractable = m_activeInteractable;
        }
    }
    else
    {
        // there is no active interactable
        // find the interactable that the mouse is hovering over (if any)
        EBUS_EVENT_ID_RESULT(latestHoverInteractable, m_rootElement, UiElementBus, FindInteractableToHandleEvent, point);
    }

    if (latestHoverInteractable.IsValid())
    {
        m_allowInvalidatingHoverInteractableOnHoverInput = true;
    }

    if (m_hoverInteractable.IsValid() && m_hoverInteractable != latestHoverInteractable)
    {
        // we were hovering over an interactable but now we are hovering over nothing or a different interactable
        if (m_allowInvalidatingHoverInteractableOnHoverInput)
        {
            ClearHoverInteractable();
        }
    }

    if (latestHoverInteractable.IsValid() && !m_hoverInteractable.IsValid())
    {
        // we are now hovering over something and we aren't tracking that yet
        SetHoverInteractable(latestHoverInteractable);

        EBUS_EVENT_ID_RESULT(result, m_hoverInteractable, UiInteractableBus, IsHandlingEvents);
    }

    // if there is an active interactable then we send mouse position updates to that interactable
    if (m_activeInteractable.IsValid())
    {
        EBUS_EVENT_ID(m_activeInteractable, UiInteractableBus, InputPositionUpdate, point);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleKeyInputEvent(const SInputEvent& event)
{
    bool result = false;

    // Allow the active interactable to handle the key input first
    if (m_activeInteractable.IsValid())
    {
        if (event.state == eIS_Pressed)
        {
            EKeyId keyId = MapGamepadKeysToKeyboardKeys(event);

            EBUS_EVENT_ID_RESULT(result, m_activeInteractable, UiInteractableBus, HandleKeyInput, keyId, event.modifiers);
        }
    }

    if (!result && m_isNavigationSupported)
    {
        // Handle navigation input if there is no active interactable
        if (!m_activeInteractable.IsValid())
        {
            AZ::EntityId oldHoverInteractable = m_hoverInteractable;
            result = HandleNavigationInputEvent(event);
            if (m_hoverInteractable != oldHoverInteractable)
            {
                s_handleHoverInputEvents = false;
                m_allowInvalidatingHoverInteractableOnHoverInput = false;
            }
        }

        if (!result)
        {
            // Handle enter input
            result = HandleEnterInputEvent(event);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleEnterInputEvent(const SInputEvent& event)
{
    bool result = false;

    EKeyId keyId = MapGamepadKeysToKeyboardKeys(event);

    if (keyId == eKI_Enter)
    {
        // The key is the Enter key. If there is any active or hover interactable then we consider this event handled.
        // Otherwise we can end up sending Enter events to underlying canvases even though there is an interactable
        // in this canvas that should block the events
        if (m_activeInteractable.IsValid() || m_hoverInteractable.IsValid())
        {
            result = true;
        }

        if (event.state == eIS_Pressed)
        {
            if (m_activeInteractable.IsValid() && m_activeInteractableShouldStayActive)
            {
                // Clear the active interactable
                AZ::EntityId prevActiveInteractable = m_activeInteractable;
                ClearActiveInteractable();

                if (event.deviceType == eIDT_Gamepad)
                {
                    SetHoverInteractable(prevActiveInteractable);

                    s_handleHoverInputEvents = false;
                    m_allowInvalidatingHoverInteractableOnHoverInput = false;
                }
            }
            else
            {
                if (m_hoverInteractable.IsValid())
                {
                    // clear any active interactable
                    ClearActiveInteractable();

                    // if the hover interactable can handle enter pressed events then
                    // it becomes the currently pressed interactable for the canvas
                    bool handled = false;
                    bool shouldStayActive = false;
                    EBUS_EVENT_ID_RESULT(handled, m_hoverInteractable, UiInteractableBus, HandleEnterPressed, shouldStayActive);

                    if (handled)
                    {
                        SetActiveInteractable(m_hoverInteractable, shouldStayActive);

                        s_handleHoverInputEvents = false;
                        m_allowInvalidatingHoverInteractableOnHoverInput = false;
                    }
                }
            }
        }
        else if (event.state == eIS_Released)
        {
            if (m_activeInteractable.IsValid() && (m_activeInteractable == m_hoverInteractable))
            {
                EBUS_EVENT_ID(m_activeInteractable, UiInteractableBus, HandleEnterReleased);

                if (!m_activeInteractableShouldStayActive)
                {
                    ClearActiveInteractable();
                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandleNavigationInputEvent(const SInputEvent& event)
{
    bool result = false;

    EKeyId keyId = MapGamepadKeysToKeyboardKeys(event);

    if (keyId == eKI_Up || keyId == eKI_Down || keyId == eKI_Left || keyId == eKI_Right)
    {
        if (event.state == eIS_Pressed)
        {
            AZ::EntityId firstHoverInteractable = GetFirstHoverInteractable();

            // Find the interactable to navigate to
            if (!m_hoverInteractable.IsValid())
            {
                SetHoverInteractable(firstHoverInteractable);
            }
            else
            {
                LyShine::EntityArray navigableElements;
                FindTopLevelNavigableInteractables(m_hoverInteractable, navigableElements);

                auto isValidInteractable = [](AZ::EntityId entityId)
                    {
                        bool isEnabled = false;
                        EBUS_EVENT_ID_RESULT(isEnabled, entityId, UiElementBus, IsEnabled);

                        bool canHandleEvents = false;
                        if (isEnabled)
                        {
                            EBUS_EVENT_ID_RESULT(canHandleEvents, entityId, UiInteractableBus, IsHandlingEvents);
                        }

                        return canHandleEvents;
                    };

                AZ::EntityId nextEntityId = UiNavigationHelpers::GetNextElement(m_hoverInteractable, keyId,
                    navigableElements, firstHoverInteractable, isValidInteractable);

                if (nextEntityId.IsValid() && (nextEntityId != m_hoverInteractable))
                {
                    SetHoverInteractable(nextEntityId);
                }
            }

            result = m_hoverInteractable.IsValid();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandlePrimaryPress(AZ::Vector2 point)
{
    bool result = false;

    // use the pressed position to select the interactable being pressed
    AZ::EntityId interactableEntity;
    EBUS_EVENT_ID_RESULT(interactableEntity, m_rootElement, UiElementBus, FindInteractableToHandleEvent, point);

    // Clear the previous active interactable if it's different from the new active interactable
    if (!interactableEntity.IsValid() || (interactableEntity != m_activeInteractable))
    {
        if (m_activeInteractable.IsValid())
        {
            ClearActiveInteractable();
        }
    }

    if (interactableEntity.IsValid())
    {
        // if there is an interactable at that point and it can handle pressed events then
        // it becomes the currently pressed interactable for the canvas
        bool handled = false;
        bool shouldStayActive = false;
        EBUS_EVENT_ID_RESULT(handled, interactableEntity, UiInteractableBus, HandlePressed, point, shouldStayActive);

        if (handled)
        {
            SetActiveInteractable(interactableEntity, shouldStayActive);
            result = true;
        }
    }

    // Resume handling hover input events
    s_handleHoverInputEvents = true;
    m_allowInvalidatingHoverInteractableOnHoverInput = true;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::HandlePrimaryRelease(AZ::Vector2 point, EKeyId keyId)
{
    if (keyId == eKI_Touch0)
    {
        ClearHoverInteractable();
    }

    // touch was released, if there is a currently pressed interactable let it handle the release
    if (m_activeInteractable.IsValid())
    {
        EBUS_EVENT_ID(m_activeInteractable, UiInteractableBus, HandleReleased, point);

        if (!m_activeInteractableShouldStayActive)
        {
            UiInteractableActiveNotificationBus::Handler::BusDisconnect(m_activeInteractable);
            m_activeInteractable.SetInvalid();
        }

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
EKeyId UiCanvasComponent::MapGamepadKeysToKeyboardKeys(const SInputEvent& event)
{
    EKeyId keyId = event.keyId;

    if (event.deviceType == eIDT_Gamepad)
    {
        switch (event.keyId)
        {
        case eKI_XI_DPadUp:
        case eKI_Orbis_Up:
        case eKI_XI_ThumbLUp:
            keyId = eKI_Up;
            break;

        case eKI_XI_DPadDown:
        case eKI_Orbis_Down:
        case eKI_XI_ThumbLDown:
            keyId = eKI_Down;
            break;

        case eKI_XI_DPadLeft:
        case eKI_Orbis_Left:
        case eKI_XI_ThumbLLeft:
            keyId = eKI_Left;
            break;

        case eKI_XI_DPadRight:
        case eKI_Orbis_Right:
        case eKI_XI_ThumbLRight:
            keyId = eKI_Right;
            break;

        case eKI_XI_A:
        case eKI_Orbis_Cross:
            keyId = eKI_Enter;
            break;
        }
    }

    return keyId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ClearHoverInteractable()
{
    if (m_hoverInteractable.IsValid())
    {
        EBUS_EVENT_ID(m_hoverInteractable, UiInteractableBus, HandleHoverEnd);
        AZ::EntityBus::Handler::BusDisconnect(m_hoverInteractable);
        m_hoverInteractable.SetInvalid();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetActiveInteractable(AZ::EntityId newActiveInteractable, bool shouldStayActive)
{
    if (m_activeInteractable != newActiveInteractable)
    {
        ClearActiveInteractable();

        m_activeInteractable = newActiveInteractable;
        if (m_activeInteractable.IsValid())
        {
            UiInteractableActiveNotificationBus::Handler::BusConnect(m_activeInteractable);
            m_activeInteractableShouldStayActive = shouldStayActive;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ClearActiveInteractable()
{
    if (m_activeInteractable.IsValid())
    {
        EBUS_EVENT_ID(m_activeInteractable, UiInteractableBus, LostActiveStatus);
        UiInteractableActiveNotificationBus::Handler::BusDisconnect(m_activeInteractable);
        m_activeInteractable.SetInvalid();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::GetFirstHoverInteractable()
{
    AZ::EntityId hoverInteractable;

    if (m_firstHoverInteractable.IsValid())
    {
        // Make sure that this interactable exists
        AZ::Entity* hoverEntity = nullptr;
        EBUS_EVENT_RESULT(hoverEntity, AZ::ComponentApplicationBus, FindEntity, m_firstHoverInteractable);

        if (hoverEntity)
        {
            // Make sure this element handles navigation events
            UiNavigationInterface::NavigationMode navigationMode = UiNavigationInterface::NavigationMode::None;
            EBUS_EVENT_ID_RESULT(navigationMode, hoverEntity->GetId(), UiNavigationBus, GetNavigationMode);
            bool handlesNavigationEvents = (navigationMode != UiNavigationInterface::NavigationMode::None);
            if (handlesNavigationEvents)
            {
                // Make sure this element is enabled
                bool isEnabled = false;
                EBUS_EVENT_ID_RESULT(isEnabled, hoverEntity->GetId(), UiElementBus, IsEnabled);
                if (isEnabled)
                {
                    // Make sure this element is handling events
                    bool isHandlingEvents = false;
                    EBUS_EVENT_ID_RESULT(isHandlingEvents, hoverEntity->GetId(), UiInteractableBus, IsHandlingEvents);
                    if (isHandlingEvents)
                    {
                        hoverInteractable = m_firstHoverInteractable;
                    }
                }
            }
        }
    }

    if (!hoverInteractable.IsValid())
    {
        hoverInteractable = FindFirstHoverInteractable();
    }

    return hoverInteractable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::FindFirstHoverInteractable()
{
    LyShine::EntityArray navigableElements;
    FindTopLevelNavigableInteractables(AZ::EntityId(), navigableElements);

    // Go through the navigable elements and find the closest element to the top left of the screen
    float shortestDist = FLT_MAX;
    float shortestOutsideDist = FLT_MAX;
    AZ::EntityId closestElement;
    AZ::EntityId closestOutsideElement;
    for (auto navigableElement : navigableElements)
    {
        UiTransformInterface::RectPoints points;
        EBUS_EVENT_ID(navigableElement->GetId(), UiTransformBus, GetViewportSpacePoints, points);

        AZ::Vector2 topLeft = points.GetAxisAlignedTopLeft();
        AZ::Vector2 center = points.GetCenter();

        float dist = topLeft.GetLength();

        bool inside = (center.GetX() >= 0.0f &&
                       center.GetX() <= m_targetCanvasSize.GetX() &&
                       center.GetY() >= 0.0f &&
                       center.GetY() <= m_targetCanvasSize.GetY());

        if (inside)
        {
            // Calculate a value from 0 to 1 representing how close the element is to the top of the screen
            float yDistValue = topLeft.GetY() / m_targetCanvasSize.GetY();

            // Calculate final distance value biased by y distance value
            const float distMultConstant = 1.0f;
            dist += dist * distMultConstant * yDistValue;

            if (dist < shortestDist)
            {
                shortestDist = dist;
                closestElement = navigableElement->GetId();
            }
        }
        else
        {
            if (dist < shortestOutsideDist)
            {
                shortestOutsideDist = dist;
                closestOutsideElement = navigableElement->GetId();
            }
        }
    }

    if (!closestElement.IsValid())
    {
        closestElement = closestOutsideElement;
    }

    return closestElement;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::EntityId UiCanvasComponent::FindNextHoverInteractable(AZ::EntityId curHoverInteractable, EKeyId keyId)
{
    LyShine::EntityArray navigableElements;
    FindTopLevelNavigableInteractables(curHoverInteractable, navigableElements);

    return UiNavigationHelpers::SearchForNextElement(curHoverInteractable, keyId, navigableElements);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::FindTopLevelNavigableInteractables(AZ::EntityId ignoreElement, LyShine::EntityArray& result)
{
    LyShine::EntityArray elements = GetChildElements();
    std::list<AZ::Entity*> elementList(elements.begin(), elements.end());
    while (!elementList.empty())
    {
        auto& entity = elementList.front();

        // Check if the element handles navigation events, we are specifically looking for interactables
        bool handlesNavigationEvents = false;
        if (UiInteractableBus::FindFirstHandler(entity->GetId()))
        {
            UiNavigationInterface::NavigationMode navigationMode = UiNavigationInterface::NavigationMode::None;
            EBUS_EVENT_ID_RESULT(navigationMode, entity->GetId(), UiNavigationBus, GetNavigationMode);
            handlesNavigationEvents = (navigationMode != UiNavigationInterface::NavigationMode::None);
        }

        // Check if the element is enabled
        bool isEnabled = false;
        EBUS_EVENT_ID_RESULT(isEnabled, entity->GetId(), UiElementBus, IsEnabled);

        bool navigable = false;
        if (handlesNavigationEvents && isEnabled && (!ignoreElement.IsValid() || entity->GetId() != ignoreElement))
        {
            // Check if the element is handling events
            bool isHandlingEvents = false;
            EBUS_EVENT_ID_RESULT(isHandlingEvents, entity->GetId(), UiInteractableBus, IsHandlingEvents);
            navigable = isHandlingEvents;
        }

        if (navigable)
        {
            result.push_back(entity);
        }

        if (!handlesNavigationEvents && isEnabled)
        {
            LyShine::EntityArray childElements;
            EBUS_EVENT_ID_RESULT(childElements, entity->GetId(), UiElementBus, GetChildElements);
            elementList.insert(elementList.end(), childElements.begin(), childElements.end());
        }

        elementList.pop_front();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetFirstHoverInteractable()
{
    bool setHoverInteractable = false;

    if (s_handleHoverInputEvents)
    {
        // Check if there is a mouse or touch input device
        IInput* pIInput = gEnv->pInput;
        if (pIInput && !pIInput->HasInputDeviceOfType(eIDT_Mouse) && !pIInput->HasInputDeviceOfType(eIDT_TouchScreen))
        {
            // No mouse or touch input device available so set a hover interactable
            setHoverInteractable = true;
        }
    }
    else
    {
        // Not handling hover input events so set a hover interactable
        setHoverInteractable = true;
    }

    if (setHoverInteractable)
    {
        AZ::EntityId hoverInteractable = GetFirstHoverInteractable();

        if (hoverInteractable.IsValid())
        {
            SetHoverInteractable(hoverInteractable);

            s_handleHoverInputEvents = false;
            m_allowInvalidatingHoverInteractableOnHoverInput = false;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::ReplaceEntityRefs(AZ::Entity* entity, const UiElementComponent::EntityIdMap& entityIdMap,
    AZ::SerializeContext* context)
{
    // USE AZ util to scan and entity and all its components and child entities
    // and fixed up any entity refs using the given map
    AZ::EntityUtils::ReplaceEntityRefs(entity, [&](const AZ::EntityId& key, bool /*isEntityId*/) -> AZ::EntityId
        {
            auto iter = entityIdMap.find(key);
            if (iter != entityIdMap.end())
            {
                return iter->second;
            }
            else
            {
                return key; //leave unchanged if not in our map
            }
        }, context);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::PrepareAnimationSystemForCanvasSave()
{
    m_serializedAnimationData.m_serializeData.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::RestoreAnimationSystemAfterCanvasLoad(bool remapIds, UiElementComponent::EntityIdMap* entityIdMap)
{
    // NOTE: this is legacy code for loading old format animation data. The latest canvas format
    // uses the AZ serialization system for animation data.
    const char* buffer = m_serializedAnimationData.m_serializeData.c_str();
    size_t size = m_serializedAnimationData.m_serializeData.length();
    if (size > 0)
    {
        // found old format annimation data
        // serialize back from loaded string and then clear string
        XmlNodeRef xmlNode = gEnv->pSystem->LoadXmlFromBuffer(buffer, size);

        m_uiAnimationSystem.Serialize(xmlNode, true);
        m_serializedAnimationData.m_serializeData.clear();
    }

    // go through the sequences and fixup the entity Ids
    // NOTE: for a latest format canvas these have probably already been remapped by ReplaceEntityRefs
    // This function will leave them alone if that are not in the remap table
    m_uiAnimationSystem.InitPostLoad(remapIds, entityIdMap);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent* UiCanvasComponent::CloneAndInitializeCanvas(UiEntityContext* entityContext, const string& assetIdPathname, const AZ::Vector2* canvasSize)
{
    UiCanvasComponent* canvasComponent = nullptr;

    // Clone the root slice entity
    // Do this in a way that handles this canvas being a Editor canvas.
    // If it is an editor canvas then slices will be flattened and Editor components will be
    // replaced with runtime components.
    AZ::Entity* clonedRootSliceEntity = nullptr;
    AZStd::string prefabBuffer;
    AZ::IO::ByteContainerStream<AZStd::string > prefabStream(&prefabBuffer);
    if (m_entityContext->SaveToStreamForGame(prefabStream, AZ::ObjectStream::ST_XML))
    {
        prefabStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        clonedRootSliceEntity = AZ::Utils::LoadObjectFromStream<AZ::Entity>(prefabStream);
    }

    // Clone the canvas entity
    AZ::Entity* clonedCanvasEntity = nullptr;
    AZ::Entity* sourceCanvasEntity = GetEntity();
    AZStd::string canvasBuffer;
    AZ::IO::ByteContainerStream<AZStd::string > canvasStream(&canvasBuffer);
    if (AZ::Utils::SaveObjectToStream<AZ::Entity>(canvasStream, AZ::ObjectStream::ST_XML, sourceCanvasEntity))
    {
        canvasStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        clonedCanvasEntity = AZ::Utils::LoadObjectFromStream<AZ::Entity>(canvasStream);
    }

    AZ_Assert(clonedCanvasEntity && clonedRootSliceEntity, "Failed to clone canvas");

    if (clonedCanvasEntity && clonedRootSliceEntity)
    {
        // complete initialization of cloned entities, we assume this is NOT for editor
        // since we only do this when using canvas in game that is already loaded in editor
        canvasComponent = FixupPostLoad(clonedCanvasEntity, clonedRootSliceEntity, false, entityContext, canvasSize);
    }

    if (canvasComponent)
    {
        canvasComponent->m_pathname = assetIdPathname;
        canvasComponent->m_isLoadedInGame = true;
    }

    return canvasComponent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZStd::vector<AZ::EntityId> UiCanvasComponent::GetEntityIdsOfElementAndDescendants(AZ::Entity* entity)
{
    AZStd::vector<AZ::EntityId> entitiesInPrefab;
    entitiesInPrefab.push_back(entity->GetId());

    LyShine::EntityArray descendantEntities;
    EBUS_EVENT_ID(entity->GetId(), UiElementBus, FindDescendantElements,
        [](const AZ::Entity*) { return true; }, descendantEntities);

    for (auto descendant : descendantEntities)
    {
        entitiesInPrefab.push_back(descendant->GetId());
    }

    return entitiesInPrefab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SetTargetCanvasSizeAndUniformScale(bool isInGame, AZ::Vector2 canvasSize)
{
    AZ::Vector2 oldTargetCanvasSize = m_targetCanvasSize;
    float oldUniformDeviceScale = m_uniformDeviceScale;

    if (isInGame)
    {
        // Set the target canvas size to the canvas size specified by the caller
        m_targetCanvasSize = canvasSize;

        // set the uniform scale
        float viewportAspectRatio = m_targetCanvasSize.GetX() / m_targetCanvasSize.GetY();
        float canvasAspectRatio = m_canvasSize.GetX() / m_canvasSize.GetY();
        if (viewportAspectRatio > canvasAspectRatio)
        {
            // viewport is more wide-screen that the canvas. So scale so that the y dimensions fit
            m_uniformDeviceScale = m_targetCanvasSize.GetY() / m_canvasSize.GetY();
        }
        else
        {
            // viewport is less wide-screen that the canvas. So scale so that the x dimensions fit
            m_uniformDeviceScale = m_targetCanvasSize.GetX() / m_canvasSize.GetX();
        }
    }
    else
    {
        // While in the editor, the only resolution we care about is the canvas' authored
        // size, so we set that as our target size for display purposes.
        m_targetCanvasSize = m_canvasSize;
    }

    // if the target canvas size or the uniform device scale changed then this will affect the
    // element transforms so force them to recompute
    if (oldTargetCanvasSize != m_targetCanvasSize || oldUniformDeviceScale != m_uniformDeviceScale)
    {
        EBUS_EVENT_ID(GetRootElement()->GetId(), UiTransformBus, SetRecomputeTransformFlag);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::IsElementNameUnique(const AZStd::string& elementName, const LyShine::EntityArray& elements)
{
    for (auto element : elements)
    {
        if (element->GetName() == elementName)
        {
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent::EntityComboBoxVec UiCanvasComponent::PopulateNavigableEntityList()
{
    EntityComboBoxVec result;

    // Add a first entry for "None"
    result.push_back(AZStd::make_pair(AZ::EntityId(), "<None>"));

    // Get a list of all navigable elements
    LyShine::EntityArray navigableElements;

    auto checkNavigable = [](const AZ::Entity* entity)
        {
            UiNavigationInterface::NavigationMode navigationMode = UiNavigationInterface::NavigationMode::None;
            EBUS_EVENT_ID_RESULT(navigationMode, entity->GetId(), UiNavigationBus, GetNavigationMode);
            return (navigationMode != UiNavigationInterface::NavigationMode::None);
        };

    FindElements(checkNavigable, navigableElements);

    // Sort the elements by name
    AZStd::sort(navigableElements.begin(), navigableElements.end(),
        [](const AZ::Entity* e1, const AZ::Entity* e2) { return e1->GetName() < e2->GetName(); });

    // Add their names to the StringList and their IDs to the id list
    for (auto navigableEntity : navigableElements)
    {
        result.push_back(AZStd::make_pair(navigableEntity->GetId(), navigableEntity->GetName()));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent::EntityComboBoxVec UiCanvasComponent::PopulateTooltipDisplayEntityList()
{
    EntityComboBoxVec result;

    // Add a first entry for "None"
    result.push_back(AZStd::make_pair(AZ::EntityId(), "<None>"));

    // Get a list of all tooltip display elements
    LyShine::EntityArray tooltipDisplayElements;

    auto checkTooltipDisplay = [](const AZ::Entity* entity)
    {
        // Check for component on entity
        return UiTooltipDisplayBus::FindFirstHandler(entity->GetId()) ? true : false;
    };

    FindElements(checkTooltipDisplay, tooltipDisplayElements);

    // Sort the elements by name
    AZStd::sort(tooltipDisplayElements.begin(), tooltipDisplayElements.end(),
        [](const AZ::Entity* e1, const AZ::Entity* e2) { return e1->GetName() < e2->GetName(); });

    // Add their names to the StringList and their IDs to the id list
    for (auto tooltipDisplayEntity : tooltipDisplayElements)
    {
        result.push_back(AZStd::make_pair(tooltipDisplayEntity->GetId(), tooltipDisplayEntity->GetName()));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::CreateRenderTarget()
{
    if (m_canvasSize.GetX() <= 0 || m_canvasSize.GetY() <= 0)
    {
        gEnv->pSystem->Warning(VALIDATOR_MODULE_SHINE, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,
            m_pathname.c_str(),
            "Invalid render target width/height for UI canvas: %s",
            m_pathname.c_str());
        return;
    }

    // Create a render target that this canvas will be rendered to.
    // The render target size is the canvas size.
    m_renderTargetHandle = gEnv->pRenderer->CreateRenderTarget(m_renderTargetName.c_str(),
            static_cast<int>(m_canvasSize.GetX()), static_cast<int>(m_canvasSize.GetY()), Clr_Empty, eTF_R8G8B8A8);

    if (m_renderTargetHandle <= 0)
    {
        gEnv->pSystem->Warning(VALIDATOR_MODULE_SHINE, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,
            m_pathname.c_str(),
            "Failed to create render target for UI canvas: %s",
            m_pathname.c_str());
    }
    else
    {
        // Also create a depth surface to render the canvas to, we need depth for masking
        // since that uses the stencil buffer
        m_renderTargetDepthSurface = gEnv->pRenderer->CreateDepthSurface(
                static_cast<int>(m_canvasSize.GetX()), static_cast<int>(m_canvasSize.GetY()), false);

        // Register this canvas component as a game framework listener so that we can render
        // it to a texture on the PreRender event
        gEnv->pGame->GetIGameFramework()->RegisterListener(this, "UiCanvasComponent", FRAMEWORKLISTENERPRIORITY_HUD);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::DestroyRenderTarget()
{
    if (m_renderTargetHandle > 0)
    {
        gEnv->pGame->GetIGameFramework()->UnregisterListener(this);
        gEnv->pRenderer->DestroyDepthSurface(m_renderTargetDepthSurface);
        m_renderTargetDepthSurface = nullptr;
        gEnv->pRenderer->DestroyRenderTarget(m_renderTargetHandle);
        m_renderTargetHandle = -1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::RenderCanvasToTexture(bool displayBounds)
{
    if (m_renderTargetHandle <= 0)
    {
        return;
    }

    ISystem* system = gEnv->pSystem;
    if (system && !gEnv->IsDedicated())
    {
        gEnv->pRenderer->SetRenderTarget(m_renderTargetHandle, m_renderTargetDepthSurface);

        // clear the render target before rendering to it
        // NOTE: the FRT_CLEAR_IMMEDIATE is required since we will have already set the render target
        // In theory we could call this before setting the render target without the immediate flag
        // but that doesn't work. Perhaps because FX_Commit is not called.
        ColorF viewportBackgroundColor(0, 0, 0, 0); // if clearing color we want to set alpha to zero also
        gEnv->pRenderer->ClearTargetsImmediately(FRT_CLEAR, viewportBackgroundColor);

        // we are writing to a linear texture
        gEnv->pRenderer->SetSrgbWrite(false);

        RenderCanvas(true, m_canvasSize, displayBounds);

        gEnv->pRenderer->SetRenderTarget(0); // restore render target
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::SaveCanvasToFile(const string& pathname, AZ::DataStream::StreamType streamType)
{
    // Note: This is ok for saving in tools, but we should use the streamer to write objects directly (no memory store)
    AZStd::vector<AZ::u8> dstData;
    AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8> > dstByteStream(&dstData);

    if (!SaveCanvasToStream(dstByteStream, streamType))
    {
        return false;
    }

    AZ::IO::SystemFile file;
    file.Open(pathname.c_str(), AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_CREATE_PATH | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY);
    if (!file.IsOpen())
    {
        file.Close();
        return false;
    }

    file.Write(dstData.data(), dstData.size());

    file.Close();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::SaveCanvasToStream(AZ::IO::GenericStream& stream, AZ::DataStream::StreamType streamType)
{
    UiCanvasFileObject fileObject;
    fileObject.m_canvasEntity = this->GetEntity();

    fileObject.m_rootSliceEntity = m_entityContext->GetRootAssetEntity();

    if (!AZ::Utils::SaveObjectToStream<UiCanvasFileObject>(stream, streamType, &fileObject))
    {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SendRectChangeNotificationsAndRecomputeLayouts()
{
    // Send canvas space rect change notifications. Handlers may mark
    // layouts for a recompute
    SendRectChangeNotifications();

    // Recompute invalid layouts
    m_layoutManager->RecomputeMarkedLayouts();

    // The layout recompute may have caused child size changes, so
    // send canvas space rect change notifications again
    SendRectChangeNotifications();

    // Remove the newly marked layouts since they have been marked due
    // to their parents recomputing them
    m_layoutManager->UnmarkAllLayouts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::SendRectChangeNotifications()
{
    // Get a list of elements who's canvas space rect has changed
    auto FindChangedElements = [](const AZ::Entity* entity)
    {
        bool rectChanged = false;
        EBUS_EVENT_ID_RESULT(rectChanged, entity->GetId(), UiTransformBus, HasCanvasSpaceRectChanged);
        return rectChanged;
    };

    LyShine::EntityArray changedElements;
    EBUS_EVENT_ID(m_rootElement, UiElementBus, FindDescendantElements, FindChangedElements, changedElements);

    // Notify of rect changes. The listeners could cause new rect changes, so loop until there are no elements
    // with changed rects
    while (!changedElements.empty())
    {
        for (auto changedElement : changedElements)
        {
            // Notify rect change and reset
            EBUS_EVENT_ID(changedElement->GetId(), UiTransformBus, NotifyAndResetCanvasSpaceRectChange);
        }

        // Check for new element rect changes
        changedElements.clear();
        EBUS_EVENT_ID(m_rootElement, UiElementBus, FindDescendantElements, FindChangedElements, changedElements);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UiCanvasComponent::InitializeLayouts()
{
    m_layoutManager->ComputeLayoutForElementAndDescendants(GetRootElement()->GetId());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::Entity* UiCanvasComponent::CloneAndAddElementInternal(AZ::Entity* sourceEntity, AZ::Entity* parentEntity, AZ::Entity* insertBeforeEntity)
{
    // first check that the given entity really is a UI element - i.e. it has a UiElementComponent
    UiElementComponent* sourceElementComponent = sourceEntity->FindComponent<UiElementComponent>();
    if (!sourceElementComponent)
    {
        AZ_Warning("UI", false, "CloneElement: The entity to be cloned must have an element component");
        return nullptr;
    }

    // also check that the given parent entity is part of this canvas (if one is specified)
    if (parentEntity)
    {
        AZ::EntityId parentCanvasId;
        EBUS_EVENT_ID_RESULT(parentCanvasId, parentEntity->GetId(), UiElementBus, GetCanvasEntityId);
        if (parentCanvasId != GetEntityId())
        {
            AZ_Warning("UI", false, "CloneElement: The parent entity must belong to this canvas");
            return nullptr;
        }
    }

    // If no parent entity specified then the parent is the root element
    AZ::Entity* parent = (parentEntity) ? parentEntity : GetRootElement();

    // also check that the given InsertBefore entity is a child of the parent
    if (insertBeforeEntity)
    {
        AZ::Entity* insertBeforeParent;
        EBUS_EVENT_ID_RESULT(insertBeforeParent, insertBeforeEntity->GetId(), UiElementBus, GetParent);
        if (insertBeforeParent != parent)
        {
            AZ_Warning("UI", false, "CloneElement: The insertBefore entity must be a child of the parent");
            return nullptr;
        }
    }

    AZ::SerializeContext* context = nullptr;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialization context found");

    AZStd::vector<AZ::EntityId> entitiesToClone = GetEntityIdsOfElementAndDescendants(sourceEntity);

    AzFramework::EntityContext::EntityList clonedEntities;
    m_entityContext->CloneUiEntities(entitiesToClone, clonedEntities);

    AZ::Entity* clonedRootEntity = clonedEntities[0];

    UiElementComponent* elementComponent = clonedRootEntity->FindComponent<UiElementComponent>();
    AZ_Assert(elementComponent, "The cloned entity must have an element component");

    // recursively set the canvas and parent pointers
    elementComponent->FixupPostLoad(clonedRootEntity, this, parent, true);

    // add this new entity as a child of the parent (parentEntity or root)
    UiElementComponent* parentElementComponent = parent->FindComponent<UiElementComponent>();
    AZ_Assert(parentElementComponent, "No element component found on parent entity");
    parentElementComponent->AddChild(clonedRootEntity, insertBeforeEntity);

    if (m_isLoadedInGame)
    {
        // Call InGamePostActivate on all the created entities
        for (AZ::Entity* entity : clonedEntities)
        {
            EBUS_EVENT_ID(entity->GetId(), UiInitializationBus, InGamePostActivate);
        }
    }

    return clonedRootEntity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE STATIC MEMBER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
AZ::u64 UiCanvasComponent::CreateUniqueId()
{
    AZ::u64 utcTime = AZStd::GetTimeUTCMilliSecond();
    uint32 r = cry_random_uint32();

    AZ::u64 id = utcTime << 32 | r;
    return id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent* UiCanvasComponent::CreateCanvasInternal(UiEntityContext* entityContext, bool forEditor)
{
    // create a new empty canvas, give it a name to avoid serialization generating one based on
    // the ID (which in some cases caused diffs to fail in the editor)
    AZ::Entity* canvasEntity = aznew AZ::Entity("UiCanvasEntity");
    UiCanvasComponent* canvasComponent = canvasEntity->CreateComponent<UiCanvasComponent>();

    // Initialize the UiEntityContext
    canvasComponent->m_entityContext = entityContext;
    canvasComponent->m_entityContext->InitUiContext();

    // Give the canvas a unique identifier. Used for canvas metrics
    canvasComponent->m_uniqueId = CreateUniqueId();

    // This is the dummy root node of the canvas.
    // It needs an element component and a transform component.
    AZ::Entity* rootEntity = canvasComponent->m_entityContext->CreateEntity("_root");
    canvasComponent->m_rootElement = rootEntity->GetId();
    AZ_Assert(rootEntity, "Failed to create root element entity");

    rootEntity->Deactivate(); // so we can add components

    UiElementComponent* elementComponent = rootEntity->CreateComponent<UiElementComponent>();
    AZ_Assert(elementComponent, "Failed to add UiElementComponent to entity");
    elementComponent->SetCanvas(canvasComponent, canvasComponent->GenerateId());
    AZ::Component* transformComponent = rootEntity->CreateComponent<UiTransform2dComponent>();
    AZ_Assert(transformComponent, "Failed to add transform2d component to entity");

    rootEntity->Activate();  // re-activate

    // init the canvas entity (the canvas entity is not part of the EntityContext so is not automatically initialized)
    canvasEntity->Init();
    canvasEntity->Activate();

    canvasComponent->m_isLoadedInGame = !forEditor;

    return canvasComponent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent*  UiCanvasComponent::LoadCanvasInternal(const string& pathnameToOpen, bool forEditor, const string& assetIdPathname, UiEntityContext* entityContext)
{
    UiCanvasComponent* canvasComponent = nullptr;

    // Currently LoadObjectFromFile will hang if the file cannot be parsed
    // (LMBR-10078). So first check that it is in the right format
    if (IsValidAzSerializedFile(pathnameToOpen))
    {
        // Open a stream on the input path
        AZ::IO::FileIOStream stream(pathnameToOpen.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);
        if (!stream.IsOpen())
        {
            AZ_Warning("UI", false, "Cannot open UI canvas file \"%s\".", pathnameToOpen.c_str());
        }
        else
        {
            // Read in the canvas from the stream
            UiCanvasFileObject* canvasFileObject = UiCanvasFileObject::LoadCanvasFromStream(stream);
            AZ_Assert(canvasFileObject, "Failed to load canvas");

            if (canvasFileObject)
            {
                AZ::Entity* canvasEntity = canvasFileObject->m_canvasEntity;
                AZ::Entity* rootSliceEntity = canvasFileObject->m_rootSliceEntity;
                AZ_Assert(canvasEntity && rootSliceEntity, "Failed to load canvas");

                if (canvasEntity && rootSliceEntity)
                {
                    // file loaded OK

                    // no need to check if a canvas with this EntityId is already loaded since we are going
                    // to generate new entity IDs for all entities loaded from the file.

                    // complete initialization of loaded entities
                    canvasComponent = FixupPostLoad(canvasEntity, rootSliceEntity, forEditor, entityContext);
                    if (canvasComponent)
                    {
                        // The canvas size may get reset on the first call to RenderCanvas to set the size to
                        // viewport size. So we'll recompute again on first render.
                        EBUS_EVENT_ID(canvasComponent->GetRootElement()->GetId(), UiTransformBus, SetRecomputeTransformFlag);

                        canvasComponent->m_pathname = assetIdPathname;
                        canvasComponent->m_isLoadedInGame = !forEditor;
                    }
                    else
                    {
                        // cleanup, don't delete rootSliceEntity, deleting the canvasEntity cleans up the EntityContext and root slice
                        delete canvasEntity;
                    }
                }

                // UiCanvasFileObject is a simple container for the canvas pointers, its destructor
                // doesn't destroy the canvas, but we need to delete it nonetheless to avoid leaking.
                delete canvasFileObject;
            }

            
        }
    }
    else
    {
        // this file is not a valid canvas file
        gEnv->pSystem->Warning(VALIDATOR_MODULE_SHINE, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,
            pathnameToOpen.c_str(),
            "Invalid XML format or couldn't load file for UI canvas file: %s",
            pathnameToOpen.c_str());
    }

    return canvasComponent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent* UiCanvasComponent::FixupReloadedCanvasForEditorInternal(AZ::Entity* newCanvasEntity,
    AZ::Entity* rootSliceEntity, UiEntityContext* entityContext,
    LyShine::CanvasId existingId, const string& existingPathname)
{
    UiCanvasComponent* newCanvasComponent = FixupPostLoad(newCanvasEntity, rootSliceEntity, true, entityContext);
    if (newCanvasComponent)
    {
        newCanvasComponent->m_id = existingId;
        newCanvasComponent->m_pathname = existingPathname;
    }
    return newCanvasComponent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UiCanvasComponent* UiCanvasComponent::FixupPostLoad(AZ::Entity* canvasEntity, AZ::Entity* rootSliceEntity, bool forEditor, UiEntityContext* entityContext, const AZ::Vector2* canvasSize)
{
    // when we load in the editor we do not create new entity IDs. A canvas can only be open
    // once in the editor. When we load in game we always generate new entity IDs.
    bool makeNewEntityIds = (forEditor) ? false : true;

    UiCanvasComponent* canvasComponent = canvasEntity->FindComponent<UiCanvasComponent>();
    AZ_Assert(canvasComponent, "No canvas component found on loaded entity");
    if (!canvasComponent)
    {
        return nullptr;     // unlikely to happen but perhaps possible if a non-canvas file was opened
    }

    AZ::SliceComponent::EntityIdToEntityIdMap idRemapTable;

    // Initialize the entity context for the new canvas and init and activate all the entities
    // in the root slice
    canvasComponent->m_entityContext = entityContext;
    canvasComponent->m_entityContext->InitUiContext();
    if (!canvasComponent->m_entityContext->HandleLoadedRootSliceEntity(rootSliceEntity, makeNewEntityIds, &idRemapTable))
    {
        return nullptr;
    }

    // For the canvas entity itself, handle ID mapping and initialization
    {
        if (makeNewEntityIds)
        {
            AZ::EntityId newId = AZ::Entity::MakeId();
            canvasEntity->SetId(newId);
        }
        canvasEntity->Init();
        canvasEntity->Activate();

        // remap entity IDs such as m_rootElement and any entity IDs in the animation data
        if (makeNewEntityIds)
        {
            // new IDs were generated so we should fix up any internal EntityRefs
            AZ::SerializeContext* context = nullptr;
            EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
            AZ_Assert(context, "No serialization context found");

            canvasComponent->ReplaceEntityRefs(canvasEntity, idRemapTable, context);
        }
    }

    AZ::Entity* rootElement = canvasComponent->GetRootElement();

    UiElementComponent* elementComponent = rootElement->FindComponent<UiElementComponent>();
    AZ_Assert(elementComponent, "No element component found on root element entity");

    // Need to remapIds too (actually I don't think this needs to remap anymore)
    canvasComponent->RestoreAnimationSystemAfterCanvasLoad(makeNewEntityIds, &idRemapTable);

    bool fixupSuccess = elementComponent->FixupPostLoad(rootElement, canvasComponent, nullptr, false);
    if (!fixupSuccess)
    {
        return nullptr;
    }

    AZ::SliceComponent::EntityList entities;
    AZ::SliceComponent* rootSlice = canvasComponent->m_entityContext->GetRootSlice();

    bool result = rootSlice->GetEntities(entities);

    // Initialize the target canvas size and uniform scale
    // This should be done before calling InGamePostActivate so that the
    // canvas space rects of the elements are accurate
    AZ_Assert(gEnv->pRenderer, "Attempting to access IRenderer before it has been initialized");
    if (gEnv->pRenderer)
    {
        AZ::Vector2 targetCanvasSize;
        if (canvasSize)
        {
            targetCanvasSize = *canvasSize;
        }
        else
        {
            targetCanvasSize.SetX(static_cast<float>(gEnv->pRenderer->GetOverlayWidth()));
            targetCanvasSize.SetY(static_cast<float>(gEnv->pRenderer->GetOverlayHeight()));
        }
        canvasComponent->SetTargetCanvasSizeAndUniformScale(!forEditor, targetCanvasSize);
    }

    // Initialize transform properties of children of layout elements
    canvasComponent->InitializeLayouts();

    if (!forEditor)
    {
        // Call InGamePostActivate on all the created entities when loading in game
        for (AZ::Entity* entity : entities)
        {
            EBUS_EVENT_ID(entity->GetId(), UiInitializationBus, InGamePostActivate);
        }
    }

    // Set the first hover interactable
    if (canvasComponent->m_isNavigationSupported)
    {
        canvasComponent->SetFirstHoverInteractable();
    }

    return canvasComponent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool UiCanvasComponent::VersionConverter(AZ::SerializeContext& context,
    AZ::SerializeContext::DataElementNode& classElement)
{
    // conversion from version 1 to 2:
    if (classElement.GetVersion() < 2)
    {
        // No need to actually convert anything because the CanvasFileObject takes care of it
        // But it makes sense to bump the version number because the m_rootElement is now an EntityId
        // rather than an Entity*
    }

    // conversion from version 2 to 3:
    // - Need to convert Vec2 to AZ::Vector2
    if (classElement.GetVersion() < 3)
    {
        if (!LyShine::ConvertSubElementFromVec2ToVector2(context, classElement, "CanvasSize"))
        {
            return false;
        }
    }

    return true;
}
