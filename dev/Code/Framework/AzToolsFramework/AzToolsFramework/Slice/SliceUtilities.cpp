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
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/EntityUtils.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/smart_ptr/intrusive_ptr.h>
#include <AzCore/std/containers/unordered_map.h>

#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Entity/EntityContext.h>

#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/SourceControl/SourceControlAPI.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/UI/UICore/ProgressShield.hxx>
#include <AzToolsFramework/UI/Slice/SlicePushWidget.hxx>
#include <AzToolsFramework/UI/PropertyEditor/InstanceDataHierarchy.h>
#include <AzToolsFramework/Slice/SliceUtilities.h>
#include <AzToolsFramework/Slice/SliceTransaction.h>

#include <QtWidgets/QWidget>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QErrorMessage>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QThread>

namespace
{
    /**
    * \brief Checks if the entities in the provided asset have a common root, if not, prompts the user to add one or
    *        cancel the slice creation operation.
    *        Slices inherently do not care if a slice has a single root or multiple roots. We have decided that single
    *        rooted slices are easier for users to understand so we do not allows users to create a slice with multiples
    *        entities at the root level.
    *        This method helps users by interactively adding a new slice root during slice creation IF it is required,
    *        it takes a slice asset and during the pre save step for a slice, inspects this asset for a common root.
    *        If the slice asset has only one common root, then the slice can be created as is.
    *        If the slice has multiple entities at root level [X,Y,Z] and the parents of all these entities is the same [A] (even if that same parent is null).
    *        Then the user is presented with a dialog box that allows them to inject a parent entity [P] that is parent of [X,Y,Z] Child of [A] and is the root
    *        of this new slice.
    * \param selectedEntities Entities that are about to be added to slice
    * \param sliceRootName IF a slice root is added, the slice root entity is set to this name (Currently, the name of the slice)
    * \param sliceRootParentEntityId [OUT] If the common root of all entities in selection is not null then that common root is to be set as the parent of the newly created slice root
    * \param sliceRootPositionAfterReplacement [OUT] Position of the slice root entity wrt its parent after the slice has replaced live entities in the editor
    * \param wasRootAutoCreated [OUT] indicates if a root was auto created and added
    */
    void CheckAndAddSliceRoot(const AzToolsFramework::SliceUtilities::SliceTransaction::SliceAssetPtr& asset, 
                              AZStd::string sliceRootName,
                              AZ::EntityId& sliceRootParentEntityId, 
                              AZ::Vector3& sliceRootPositionAfterReplacement,
                              bool& wasRootAutoCreated)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AzFramework::EntityContext::EntityList sliceEntities;
        asset.Get()->GetComponent()->GetEntities(sliceEntities);

        //////////////////////////////////////////////////////////////////////////

        // Find common root for all selected and referenced entities
        AZStd::unordered_map<AZ::EntityId, AZ::Entity*> sliceEntityMap;
        AzToolsFramework::EntityIdList sliceEntityIdsList;
        for (AZ::Entity* entity : sliceEntities)
        {
            sliceEntityIdsList.push_back(entity->GetId());
            sliceEntityMap.insert(AZStd::make_pair(entity->GetId(), entity));
        }

        AZ::EntityId commonRoot;
        AzToolsFramework::EntityList selectionRootEntities;

        bool result = false;
        AzToolsFramework::ToolsApplicationRequests::Bus::BroadcastResult(result, &AzToolsFramework::ToolsApplicationRequests::FindCommonRootInactive, sliceEntities, commonRoot, &selectionRootEntities);

        // The translation of the new slice root
        AZ::Vector3 sliceRootTranslation(AZ::Vector3::CreateZero());
        wasRootAutoCreated = false;

        if (result)
        {
            if (selectionRootEntities.size() > 1)
            {
                const int response = QMessageBox::warning(QApplication::activeWindow(),
                                                          QStringLiteral("Cannot Create Slice"),
                                                          QString("The slice cannot be created because no single transform root is defined. "
                                                          "Please make sure your slice contains only one root entity.\r\n\r\n"
                                                          "Do you want to create a Transform root entity ?"),
                                                          QMessageBox::Yes | QMessageBox::Cancel);

                if (response == QMessageBox::Cancel)
                {
                    result = false;
                }
                else
                {
                    // Create a new slice root entity
                    AZ::Entity* sliceRootEntity = aznew AZ::Entity();
                    sliceRootEntity->SetName(sliceRootName);

                    wasRootAutoCreated = true;

                    // Add all required editor components
                    AzToolsFramework::EditorEntityContextRequestBus::Broadcast(&AzToolsFramework::EditorEntityContextRequests::AddRequiredComponents, *sliceRootEntity);

                    // Reposition everything so that the new slice root is at the centroid bottom of the top level entities in user selection
                    AZ::VectorFloat sliceZmin = FLT_MAX;

                    int count = 0;
                    for (AZ::Entity* selectionRootEntity : selectionRootEntities)
                    {
                        if (selectionRootEntity)
                        {
                            AzToolsFramework::Components::TransformComponent* transformComponent =
                                selectionRootEntity->FindComponent<AzToolsFramework::Components::TransformComponent>();

                            if (transformComponent)
                            {
                                count++;
                                AZ::Vector3 currentPosition;
                                if (commonRoot.IsValid())
                                {
                                    currentPosition = transformComponent->GetLocalTranslation();
                                }
                                else
                                {
                                    currentPosition = transformComponent->GetWorldTranslation();
                                }

                                sliceRootTranslation += currentPosition;
                                sliceZmin = sliceZmin.GetMin(currentPosition.GetZ());
                            }
                        }
                    }

                    sliceRootTranslation = (sliceRootTranslation / count);

                    sliceRootTranslation.SetZ(sliceZmin);

                    // Re root entities so that the new slice root is the parent of all selection root entities 
                    // and reposition top level entities so that the slice root is at 0,0,0 in the slice
                    for (AZ::Entity* selectionRootEntity : selectionRootEntities)
                    {
                        if (selectionRootEntity)
                        {
                            AzToolsFramework::Components::TransformComponent* transformComponent =
                                selectionRootEntity->FindComponent<AzToolsFramework::Components::TransformComponent>();

                            if (transformComponent)
                            {
                                transformComponent->SetLocalTranslation(transformComponent->GetLocalTranslation() - sliceRootTranslation);
                                transformComponent->SetParent(sliceRootEntity->GetId());
                            }
                        }
                    }

                    // Add new slice root entity to the final asset
                    asset.Get()->GetComponent()->AddEntity(sliceRootEntity);
                }
            }
            else if (selectionRootEntities.size() == 1)
            {
                AzToolsFramework::Components::TransformComponent* transformComponent = 
                    selectionRootEntities.at(0)->FindComponent<AzToolsFramework::Components::TransformComponent>();

                if (transformComponent)
                {
                    sliceRootTranslation = transformComponent->GetWorldTranslation();
                }
            }
            else
            {
                QMessageBox::warning(QApplication::activeWindow(), QStringLiteral("Cannot Save Slice"),
                                     QString("Failed to write slice \"%1\" because transforms could not be rooted."),
                                     QMessageBox::Ok);
            }
        }
        else
        {
            QMessageBox::warning(QApplication::activeWindow(), QStringLiteral("Cannot Save Slice"),
                                 QString("Failed to write slice \"%1\" because transforms could not be rooted."),
                                 QMessageBox::Ok);
        }

        sliceRootParentEntityId = commonRoot;
        sliceRootPositionAfterReplacement = sliceRootTranslation;
    }
}

namespace AzToolsFramework
{
    namespace SliceUtilities
    {
        //=========================================================================
        void PushEntitiesModal(const EntityIdList& entities, 
                               AZ::SerializeContext* serializeContext)
        {
            QDialog* dialog = new QDialog();
            QVBoxLayout* mainLayout = new QVBoxLayout();
            mainLayout->setContentsMargins(5, 5, 5, 5);
            SlicePushWidget* widget = new SlicePushWidget(entities, serializeContext);
            mainLayout->addWidget(widget);
            dialog->setWindowTitle(widget->tr("Push to Slice(s)"));
            dialog->setMinimumSize(QSize(600, 200));
            dialog->resize(QSize(1000, 600));
            dialog->setLayout(mainLayout);

            QWidget::connect(widget, &SlicePushWidget::OnFinished, dialog,
                [dialog] ()
                {
                    dialog->accept();
                }
            );

            QWidget::connect(widget, &SlicePushWidget::OnCanceled, dialog,
                [dialog] ()
                {
                    dialog->reject();
                }
            );

            dialog->exec();
            delete dialog;
        }

        //=========================================================================
        bool MakeNewSlice(const AzToolsFramework::EntityIdList& entities, 
                          const char* targetDirectory, 
                          bool inheritSlices, 
                          AZ::SerializeContext* serializeContext)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            if (entities.empty())
            {
                return false;
            }

            if (!serializeContext)
            {
                EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
                AZ_Assert(serializeContext, "Failed to retrieve application serialize context.");
            }

            // Check for references that don't fall within the selected entity set.
            // Give the user the option to pull in all referenced entities, or to
            // stick with the current selection.
            EntityIdSet selectedHierarchyEntities;
            EBUS_EVENT_RESULT(selectedHierarchyEntities, AzToolsFramework::ToolsApplicationRequests::Bus, GatherEntitiesAndAllDescendents, entities);

            // Expand any entity references on components, and offer to include any excluded entities to the slice.
            EntityIdSet allReferencedEntities = selectedHierarchyEntities;
            SliceUtilities::GatherAllReferencedEntities(allReferencedEntities, *serializeContext);
            
            bool referencedEntitiesNotInSelection = (allReferencedEntities.size() > selectedHierarchyEntities.size());
            if (!referencedEntitiesNotInSelection)
            {
                for (const AZ::EntityId& id : allReferencedEntities)
                {
                    if (selectedHierarchyEntities.find(id) == selectedHierarchyEntities.end())
                    {
                        referencedEntitiesNotInSelection = true;
                        break;
                    }
                }
            }

            if (referencedEntitiesNotInSelection)
            {
                AZStd::string includedEntities;
                AZStd::string missingEntities;
                AZ::u32 totalIncluded = 0;
                AZ::u32 totalMissing = 0;
                enum { kMaxToDisplay = 10 };

                for (const AZ::EntityId& id : allReferencedEntities)
                {
                    AZ::Entity* entity = nullptr;
                    EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, id);
                    if (entity)
                    {
                        if (selectedHierarchyEntities.find(id) != selectedHierarchyEntities.end())
                        {
                            if (++totalIncluded <= kMaxToDisplay)
                            {
                                includedEntities.append("    ");
                                includedEntities.append(entity->GetName());
                                includedEntities.append("\r\n");
                            }
                        }
                        else
                        {
                            if (++totalMissing <= kMaxToDisplay)
                            {
                                missingEntities.append("    ");
                                missingEntities.append(entity->GetName());
                                missingEntities.append("\r\n");
                            }
                        }
                    }
                }

                if (totalIncluded > kMaxToDisplay)
                {
                    includedEntities.append(AZStd::string::format("    (%u more entities...)\r\n", totalIncluded - kMaxToDisplay));
                }

                if (totalMissing > kMaxToDisplay)
                {
                    missingEntities.append(AZStd::string::format( "    (%u more entities...)\r\n", totalMissing - kMaxToDisplay));
                }

                const AZStd::string message = AZStd::string::format(
                        "Some of the selected entities reference entities not contained in the selection.\r\n"
                        "Any references from outside entities to those in the slice will be invalidated.\r\n\r\n"
                        "The following entities are included in your selection:\r\n%s\r\n"
                        "Would you like to include the following referenced entities?\r\n%s",
                        includedEntities.c_str(),
                        missingEntities.c_str());

                const int response = QMessageBox::warning(QApplication::activeWindow(), QStringLiteral("Create Slice"),
                        QString(message.c_str()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

                if (response == QMessageBox::Yes)
                {
                    selectedHierarchyEntities = allReferencedEntities;
                }
                else if (response != QMessageBox::No)
                {
                    return false;
                }
            }

            AZStd::string saveAsStartPath;
            if (!GetSliceSaveLocation(saveAsStartPath))
            {
                saveAsStartPath = targetDirectory;
            }

            AZStd::string suggestedPath;
            if (GenerateSuggestedSlicePath(selectedHierarchyEntities, saveAsStartPath, suggestedPath))
            {
                saveAsStartPath = suggestedPath;
            }

            // Save a reference to our currently active window since it will be
            // temporarily null after the QFileDialog closes, which we need to
            // be able to parent our message dialogs properly
            QWidget* activeWindow = QApplication::activeWindow();
            const QString saveAs = QFileDialog::getSaveFileName(nullptr, QString("Save As..."), saveAsStartPath.c_str(), QString("Slices (*.slice)"));
            QFileInfo fi(saveAs);
            QString sliceName = fi.baseName();
            if (saveAs.isEmpty())
            {
                return false;
            }

            const AZStd::string targetPath = saveAs.toUtf8().constData();

            // If the slice already exists, we instead want to *push* the entities to the existing
            // asset, as opposed to creating a new one.
            AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
            if (fileIO && fileIO->Exists(targetPath.c_str()))
            {
                const AZStd::string message = AZStd::string::format(
                        "You are attempting to overwrite an existing slice: \"%s\".\r\n\r\n"
                        "This will damage instances or cascades of this slice. \r\n\r\n"
                        "Instead, either push entities/fields to the slice, or save to a different location.",
                        targetPath.c_str());

                QMessageBox::warning(activeWindow, QStringLiteral("Unable to Overwrite Slice"),
                    QString(message.c_str()), QMessageBox::Ok, QMessageBox::Ok);

                return false;
            }

            AZStd::string saveDir(fi.absoluteDir().absolutePath().toUtf8().constData());
            SetSliceSaveLocation(saveDir);

            //
            // Setup and execute transaction for the new slice.
            //

            AZ::EntityId parentAfterReplacement;
            AZ::Vector3 positionAfterReplacement(AZ::Vector3::CreateZero());
            bool wasRootAutoCreated = false;

            // PreSaveCallback for slice creation: Before saving slice, we ensure it has a single root by optionally auto-creating one for the user
            SliceTransaction::PreSaveCallback preSaveCallback =
                [&sliceName, &parentAfterReplacement, &positionAfterReplacement, &selectedHierarchyEntities, &wasRootAutoCreated]
                (SliceTransaction::TransactionPtr transaction, const char* /*fullPath*/, const SliceTransaction::SliceAssetPtr& asset) -> SliceTransaction::Result
                {
                    CheckAndAddSliceRoot(asset, sliceName.toUtf8().constData(), parentAfterReplacement, positionAfterReplacement, wasRootAutoCreated);
                    return AZ::Success();
                };

            // PostSaveCallback for slice creation: kick off async replacement of source entities with an instance of the new slice.
            SliceTransaction::PostSaveCallback postSaveCallback = 
                [&parentAfterReplacement, &positionAfterReplacement, &selectedHierarchyEntities, &wasRootAutoCreated]
                (SliceTransaction::TransactionPtr transaction, const char* fullPath, const SliceTransaction::SliceAssetPtr& /*asset*/) -> void
                {
                    // Once the asset is processed and ready, we can replace the source entities with an instance of the new slice.
                    EditorEntityContextRequestBus::Broadcast(
                        &EditorEntityContextRequests::QueueSliceReplacement,
                        fullPath, transaction->GetLiveToAssetEntityIdMap(), selectedHierarchyEntities,
                        parentAfterReplacement, positionAfterReplacement, wasRootAutoCreated);
                };

            SliceTransaction::TransactionPtr transaction = SliceTransaction::BeginNewSlice(nullptr, serializeContext);

            // Add entities
            for (const AZ::EntityId& entityId : selectedHierarchyEntities)
            {
                SliceTransaction::Result addResult = transaction->AddEntity(entityId, !inheritSlices ? SliceTransaction::SliceAddEntityFlags::DiscardSliceAncestry : 0);
                        if (!addResult)
                        {
                    QMessageBox::warning(activeWindow, QStringLiteral("Slice Save Failed"),
                                QString(addResult.GetError().c_str()), QMessageBox::Ok);
                            return false;
                        }
            }

            SliceTransaction::Result result = transaction->Commit(targetPath.c_str(), preSaveCallback, postSaveCallback, SliceTransaction::SliceCommitFlags::ApplyWorldSliceTransformRules);

            if (!result)
            {
                QMessageBox::warning(activeWindow, QStringLiteral("Slice Save Failed"),
                                     QString(result.GetError().c_str()), QMessageBox::Ok);
                return false;
            }

            return true;
        }

        //=========================================================================
        void GatherAllReferencedEntities(AZStd::unordered_set<AZ::EntityId>& entitiesWithReferences,
                                         AZ::SerializeContext& serializeContext)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            AZStd::vector<AZ::EntityId> floodQueue;
            floodQueue.reserve(entitiesWithReferences.size());

            auto visitChild = [&floodQueue, &entitiesWithReferences](const AZ::EntityId& childId) -> void
            {
                if (entitiesWithReferences.insert(childId).second)
                {
                    floodQueue.push_back(childId);
                }
            };

            // Seed with all provided entity Ids and their transform descendants.
            AZStd::vector<AZ::EntityId> children;
            for (const AZ::EntityId& entityId : entitiesWithReferences)
            {
                floodQueue.push_back(entityId);

                children.clear();
                EBUS_EVENT_ID_RESULT(children, entityId, AZ::TransformBus, GetAllDescendants);
                for (const AZ::EntityId& child : children)
                {
                    visitChild(child);
                }
            }

            // Flood-fill via outgoing entity references and gather all unique visited entities.
            while (!floodQueue.empty())
            {
                const AZ::EntityId id = floodQueue.back();
                floodQueue.pop_back();

                AZ::Entity* entity = nullptr;
                EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, id);

                if (entity)
                {
                    AZStd::vector<const AZ::SerializeContext::ClassData*> parentStack;
                    parentStack.reserve(30);
                    auto beginCB = [&](void* ptr, const AZ::SerializeContext::ClassData* classData, const AZ::SerializeContext::ClassElement* elementData) -> bool
                    {
                        parentStack.push_back(classData);
                            if (elementData && elementData->m_editData)
                            {
                                AZ::Edit::Attribute* attribute = elementData->m_editData->FindAttribute(AZ::Edit::Attributes::SliceFlags);
                                if (attribute)
                                {
                                    AZ::u32 flags = 0;
                                    AzToolsFramework::PropertyAttributeReader reader(nullptr, attribute);
                                    reader.Read<AZ::u32>(flags);
                                    if (0 != (flags & AZ::Edit::SliceFlags::DontGatherReference))
                                    {
                                    return false;
                                    }
                                }
                            }

                        if (classData->m_typeId == AZ::SerializeTypeInfo<AZ::EntityId>::GetUuid())
                        {
                            if (!parentStack.empty() && parentStack.back()->m_typeId == AZ::SerializeTypeInfo<AZ::Entity>::GetUuid())
                            {
                                // Ignore the entity's actual Id field. We're only looking for references.
                            }
                            else
                            {
                                AZ::EntityId* entityIdPtr = (elementData->m_flags & AZ::SerializeContext::ClassElement::FLG_POINTER) ?
                                    *reinterpret_cast<AZ::EntityId**>(ptr) : reinterpret_cast<AZ::EntityId*>(ptr);
                                if (entityIdPtr)
                                {
                                    const AZ::EntityId id = *entityIdPtr;
                                    if (id.IsValid())
                                    {
                            visitChild(id);
                        }
                                }
                            }
                        }

                        // Keep recursing.
                        return true;
                    };

                    auto endCB = [&]() -> bool
                    {
                        parentStack.pop_back();
                        return true;
                    };

                    serializeContext.EnumerateInstanceConst(
                        entity,
                        azrtti_typeid<AZ::Entity>(),
                        beginCB,
                        endCB,
                        AZ::SerializeContext::ENUM_ACCESS_FOR_READ,
                        nullptr,
                        nullptr,
                        nullptr
                    );
                }
            }
        }

        //=========================================================================
        AZStd::unique_ptr<AZ::Entity> CloneSliceEntityForComparison(const AZ::Entity& sourceEntity,
                                                                    const AZ::SliceComponent::SliceInstance& instance,
                                                                    AZ::SerializeContext& serializeContext)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            AZ_Assert(instance.GetEntityIdMap().find(sourceEntity.GetId()) != instance.GetEntityIdMap().end(), "Provided source entity is not a member of the provided slice instance.");

            AZ::Entity* clone = serializeContext.CloneObject<AZ::Entity>(&sourceEntity);

            // Prior to comparison, remap asset entity's Id references to the instance values, so we don't see instance-remapped Ids as differences.
            const AZ::SliceComponent::EntityIdToEntityIdMap& assetToInstanceIdMap = instance.GetEntityIdMap();
            AZ::EntityUtils::ReplaceEntityRefs(clone,
                [&assetToInstanceIdMap](const AZ::EntityId& originalId, bool isEntityId) -> AZ::EntityId
                {
                    if (!isEntityId)
                    {
                        auto findIter = assetToInstanceIdMap.find(originalId);
                        if (findIter != assetToInstanceIdMap.end())
                        {
                            return findIter->second;
                        }
                    }

                    return originalId;
                },
                &serializeContext);

            return AZStd::unique_ptr<AZ::Entity>(clone);
        }

        //=========================================================================
        bool CheckSliceAdditionCyclicDependencySafe(const AZ::SliceComponent::SliceInstanceAddress& instanceToAdd,
                                                    const AZ::SliceComponent::SliceInstanceAddress& targetInstanceToAddTo)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            AZ_Assert(instanceToAdd.first && instanceToAdd.second, "Invalid instanceToAdd passed to CheckSliceADditionCyclicDependencySafe.");
            AZ_Assert(targetInstanceToAddTo.first && targetInstanceToAddTo.second, "Invalid targetInstanceToAddTo passed to CheckSliceADditionCyclicDependencySafe.");

            // Cannot add a slice instance to the very same instance
            if (instanceToAdd == targetInstanceToAddTo)
            {
                return false;
            }

            // Cannot add an asset reference to itself - "directly cyclic" check
            if (instanceToAdd.first->GetSliceAsset().GetId() == targetInstanceToAddTo.first->GetSliceAsset().GetId())
            {
                return false;
            }

            // If the instanceToAdd already has a dependency on the targetInstanceToAddTo's asset before adding, if we added it,
            // the targetInstanceToAddTo would then depend on instanceToAdd would depend on targetInstanceToAddTo, and on, cyclic!
            AZ::SliceComponent::AssetIdSet referencedSliceAssetIds;
            instanceToAdd.first->GetSliceAsset().Get()->GetComponent()->GetReferencedSliceAssets(referencedSliceAssetIds);
            if (referencedSliceAssetIds.find(targetInstanceToAddTo.first->GetSliceAsset().GetId()) != referencedSliceAssetIds.end())
            {
                return false;
            }

            return true;
        }

        //=========================================================================
        bool IsRootEntity(const AZ::Entity& entity)
        {
            auto* transformComponent = entity.FindComponent<AzToolsFramework::Components::TransformComponent>();
            return (transformComponent && !transformComponent->GetParentId().IsValid());
        }

        //=========================================================================
        bool GenerateSuggestedSlicePath(const AzToolsFramework::EntityIdSet& entitiesInSlice, const AZStd::string& targetDirectory, AZStd::string& suggestedFullPath)
        {
            // Determine suggested save name for slice based on included root/top-level entity names
            // For example, with top-level entities Entity0, Entity1, and Entity2, we would end up with
            // Entity0Entity1Entity2_001.slice, and if that already existed we would suggest
            // the first unused number value
            suggestedFullPath = targetDirectory;
            if (suggestedFullPath.at(suggestedFullPath.size() - 1) != '/')
            {
                suggestedFullPath += "/";
            }

            AZStd::string sliceName;
            size_t sliceNameCutoffLength = 32; ///< When naming a slice after its entities, we stop appending additional names once we've reached this cutoff length

            AZ::EntityId commonRoot;
            AzToolsFramework::EntityIdList sliceRootEntities;
            bool hasCommonRoot = false;
            AzToolsFramework::ToolsApplicationRequests::Bus::BroadcastResult(hasCommonRoot, &AzToolsFramework::ToolsApplicationRequests::FindCommonRoot, entitiesInSlice, commonRoot, &sliceRootEntities);

            AzToolsFramework::EntityIdSet usedNameEntities;
            auto appendToSliceName = [&sliceName, sliceNameCutoffLength, &usedNameEntities](const AZ::EntityId& id) -> bool
                {
                    if (usedNameEntities.find(id) == usedNameEntities.end())
                    {
                        AZ::Entity* entity = nullptr;
                        EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, id);
                        if (entity)
                        {
                            AZStd::string entityNameFiltered = entity->GetName();

                            // Convert spaces in entity names to underscores
                            for (size_t i = 0; i < entityNameFiltered.size(); ++i)
                            {
                                char& character = entityNameFiltered.at(i);
                                if (character == ' ')
                                {
                                    character = '_';
                                }
                            }

                            sliceName.append(entityNameFiltered);
                            usedNameEntities.insert(id);
                            if (sliceName.size() > sliceNameCutoffLength)
                            {
                                return false;
                            }
                        }
                    }
                    return true;
                };

            bool sliceNameFull = false;

            // Append common root entity to name
            if (hasCommonRoot && commonRoot.IsValid() && entitiesInSlice.find(commonRoot) != entitiesInSlice.end())
            {
                if (!appendToSliceName(commonRoot))
                {
                    sliceNameFull = true;
                }
            }

            // Append top level entity names to name
            if (!sliceNameFull)
            {
                for (const AZ::EntityId& id : sliceRootEntities)
                {
                    if (!appendToSliceName(id))
                    {
                        sliceNameFull = true;
                        break;
                    }
                }
            }

            if (sliceName.size() == 0)
            {
                sliceName = "NewSlice";
            }

            AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
            AZStd::string possiblePath;
            const AZ::u32 maxSliceNumber = 1000;
            for (AZ::u32 sliceNumber = 1; sliceNumber < maxSliceNumber; ++sliceNumber)
            {
                possiblePath = AZStd::string::format("%s%s_%3.3u.slice", suggestedFullPath.c_str(), sliceName.c_str(), sliceNumber);
                if (!fileIO || !fileIO->Exists(possiblePath.c_str()))
                {
                    suggestedFullPath = possiblePath;
                    break;
                }
            }

            return true;
        }

        void SetSliceSaveLocation(const AZStd::string& path)
        {
            auto settings = AZ::UserSettings::CreateFind<SliceUserSettings>(AZ_CRC("SliceUserSettings", 0x055b32eb), AZ::UserSettings::CT_LOCAL);
            settings->m_saveLocation = path;
        }

        bool GetSliceSaveLocation(AZStd::string& path)
        {
            auto settings = AZ::UserSettings::Find<SliceUserSettings>(AZ_CRC("SliceUserSettings", 0x055b32eb), AZ::UserSettings::CT_LOCAL);
            if (settings)
            {
                path = settings->m_saveLocation;
                return true;
            }

            return false;
        }

        void SliceUserSettings::Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<SliceUserSettings, AZ::UserSettings>()
                    ->Version(1)
                    ->Field("m_saveLocation", &SliceUserSettings::m_saveLocation);
            }
        }

        void Reflect(AZ::SerializeContext* context)
        {
            SliceUserSettings::Reflect(context);
        }
    } // namespace SliceUtilities
} // AzToolsFramework
