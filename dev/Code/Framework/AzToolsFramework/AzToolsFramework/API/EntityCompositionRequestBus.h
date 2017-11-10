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

#include <AzCore/base.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace AzToolsFramework
{
    class EntityCompositionRequests
        : public AZ::EBusTraits
    {
    public:

        struct AddComponentsResults
        {
            // This is the original list of components added (whether or not they are pending, in the order of class data requested)
            AZ::Entity::ComponentArrayType m_componentsAdded;
            /*!
            * Adding a component can only cause the following to occur:
            * 1) Component gets added to the pending list
            * 2) Components Gets added to the entity as a valid component
            * 3) Cause pending components to be added to the entity as valid components (by satisfying previously missing services)
            *
            * The following three vectors represent each occurrence, respectively.
            */
            AZ::Entity::ComponentArrayType m_addedPendingComponents;
            AZ::Entity::ComponentArrayType m_addedValidComponents;
            AZ::Entity::ComponentArrayType m_additionalValidatedComponents;
        };

        /*!
         * Stores a map of entity ids to component results that were added during AddComponentsToEntities.
         * You can use this to look up what exactly happened to each entity involved.
         * Components requested to be added will be stored in either addedPendingComponents or addedValidComponents
         * Any other previously pending components that are now valid will be stored in additionalValidatedComponents
         */
        using EntityToAddedComponentsMap = AZStd::unordered_map<AZ::EntityId, AddComponentsResults>;

        /*!
        * Outcome will be true if successful and return the above results structure to indicate what happened
        * Outcome will be false if critical underlying system failure occurred (which is not expected) and an error string will describe the problem
        */
        using AddComponentsOutcome = AZ::Outcome<EntityToAddedComponentsMap, AZStd::string>;

        /*!
        *	Outcome will be true if successful and return one instance of the above AddComponentsResults structure (since only one entity is involved)
        */
        using AddExistingComponentsOutcome = AZ::Outcome<AddComponentsResults, AZStd::string>;

        /*!
        * Add the specified component types to the specified entities.
        *
        * \param entityIds Entities to receive the new components.
        * \param componentsToAdd A list of AZ::Uuid representing the unique id of the type of components to add.
        *
        * \return  Returns a successful outcome if components were added to entities.
        *          If the operation could not be completed then the failed
        *          outcome contains a string describing what went wrong.
        */
        virtual AddComponentsOutcome AddComponentsToEntities(const EntityIdList& entityIds, const AZ::ComponentTypeList& componentsToAdd) = 0;

        /*!
        * Add the specified existing components to the specified entity.
        *
        * \param entity The AZ::Entity* to add the existing components to, with full editor-level checking with pending component support
        * \param componentsToAdd A list of AZ::Component* containing existing components to add. (Note: These components must not already be tied to another entity!)
        *
        * \return  Returns a successful outcome if components were added to entities.
        *          If the operation could not be completed then the failed
        *          outcome contains a string describing what went wrong.
        */
        virtual AddExistingComponentsOutcome AddExistingComponentsToEntity(AZ::Entity* entity, const AZStd::vector<AZ::Component*>& componentsToAdd) = 0;

        // Removing a component can only cause the following to occur:
        // 1) Invalidate other components by removing missing services
        // 2) Validate other components by removing conflicting pending services
        struct RemoveComponentsResults
        {
            AZ::Entity::ComponentArrayType m_invalidatedComponents;
            AZ::Entity::ComponentArrayType m_validatedComponents;
        };
        using EntityToRemoveComponentsResultMap = AZStd::unordered_map<AZ::EntityId, RemoveComponentsResults>;
        using RemoveComponentsOutcome = AZ::Outcome<EntityToRemoveComponentsResultMap, AZStd::string>;
        /*!
        * Removes the specified components from the specified entities.
        *
        * \param componentsToRemove List of component pointers to remove (from their respective entities).
        * \return true if the components were successfully removed or false otherwise.
        */
        virtual RemoveComponentsOutcome RemoveComponents(const AZStd::vector<AZ::Component*>& componentsToRemove) = 0;

        /*!
        * Removes the given components from their respective entities (currently only single entity is supported) and copies the data to the clipboard if successful
        * \param components vector of components to cut (this method will delete the components provided on successful removal)
        */
        virtual void CutComponents(const AZStd::vector<AZ::Component*>& components) = 0;

        /*!
        * Copies the given components from their respective entities (multiple source entities are supported) into mime data on the clipboard for pasting elsewhere
        * \param components vector of components to copy
        */
        virtual void CopyComponents(const AZStd::vector<AZ::Component*>& components) = 0;

        /*!
        * Pastes components from the mime data on the clipboard (assuming it is component data) to the given entity
        * \param entityId the Id of the entity to paste to
        */
        virtual void PasteComponentsToEntity(AZ::EntityId entityId) = 0;

        /*!
        * Checks if there is component data available to paste into an entity
        * \return true if paste is available, false otherwise
        */
        virtual bool HasComponentsToPaste() = 0;

        /*!
        * Enables the given components
        * \param components vector of components to enable
        */
        virtual void EnableComponents(const AZStd::vector<AZ::Component*>& components) = 0;

        /*!
        * Disables the given components
        * \param components vector of components to disable
        */
        virtual void DisableComponents(const AZStd::vector<AZ::Component*>& components) = 0;

        /*!
         *
         */
        using ComponentServicesList = AZStd::vector<AZ::ComponentServiceType>;
        struct PendingComponentInfo
        {
            AZ::Entity::ComponentArrayType m_validComponentsThatAreIncompatible;
            AZ::Entity::ComponentArrayType m_pendingComponentsWithRequiredServices;
            ComponentServicesList m_missingRequiredServices;
        };

        /*
         *
         */
        virtual PendingComponentInfo GetPendingComponentInfo(const AZ::Component* component) = 0;

        /*!
        * Returns a name for the given component Note: This will always dig into the underlying type. e.g. you will never get the GenericComponentWrapper name, but always the actual underlying component
        * \param component the pointer to the component for which you want the name.
        */
        virtual AZStd::string GetComponentName(const AZ::Component* component) = 0;
    };

    using EntityCompositionRequestBus = AZ::EBus<EntityCompositionRequests>;

    //! Return whether component should appear in an entity's "Add Component" menu.
    //! \param entityType The type of entity (ex: "Game", "System")
    static bool AppearsInAddComponentMenu(const AZ::SerializeContext::ClassData& classData, const AZ::Crc32& entityType);

    //! ComponentFilter for components that users can add to game entities.
    static bool AppearsInGameComponentMenu(const AZ::SerializeContext::ClassData&);

    //! ComponentFilter for components that can be added to system entities.
    static bool AppearsInSystemComponentMenu(const AZ::SerializeContext::ClassData&);

    //
    // Implementation
    //

    inline bool AppearsInAddComponentMenu(const AZ::SerializeContext::ClassData& classData, const AZ::Crc32& entityType)
    {
        if (classData.m_editData)
        {
            if (auto editorDataElement = classData.m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
            {
                for (const AZ::Edit::AttributePair& attribPair : editorDataElement->m_attributes)
                {
                    if (attribPair.first == AZ::Edit::Attributes::AppearsInAddComponentMenu)
                    {
                        PropertyAttributeReader reader(nullptr, attribPair.second);
                        AZ::Crc32 classEntityType = 0;
                        if (reader.Read<AZ::Crc32>(classEntityType))
                        {
                            if (static_cast<AZ::u32>(entityType) == classEntityType)
                            {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    inline bool AppearsInGameComponentMenu(const AZ::SerializeContext::ClassData& classData)
    {
        // We don't call AppearsInAddComponentMenu(...) because we support legacy values.
        // AppearsInAddComponentMenu used to be a bool,
        // and it used to only be applied to components on in-game entities.
        if (classData.m_editData)
        {
            if (auto editorDataElement = classData.m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
            {
                for (const AZ::Edit::AttributePair& attribPair : editorDataElement->m_attributes)
                {
                    if (attribPair.first == AZ::Edit::Attributes::AppearsInAddComponentMenu)
                    {
                        PropertyAttributeReader reader(nullptr, attribPair.second);
                        AZ::Crc32 classEntityType;
                        if (reader.Read<AZ::Crc32>(classEntityType))
                        {
                            if (classEntityType == AZ_CRC("Game", 0x232b318c))
                            {
                                return true;
                            }
                        }

                        bool legacyAppearsInComponentMenu = false;
                        if (reader.Read<bool>(legacyAppearsInComponentMenu))
                        {
                            AZ_WarningOnce(classData.m_name, false, "%s %s 'AppearsInAddComponentMenu' uses legacy value 'true', should be 'AZ_CRC(\"Game\")'.",
                                classData.m_name, classData.m_typeId.ToString<AZStd::string>().c_str());
                            return legacyAppearsInComponentMenu;
                        }
                    }
                }
            }
        }
        return false;
    }

    inline bool AppearsInSystemComponentMenu(const AZ::SerializeContext::ClassData& classData)
    {
        return AppearsInAddComponentMenu(classData, AZ_CRC("System", 0xc94d118b));
    }
}