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

/** @file
 * Header file that defines event buses for the component application interface.
 */

#ifndef AZCORE_COMPONENT_APPLICATION_BUS_H
#define AZCORE_COMPONENT_APPLICATION_BUS_H

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/std/parallel/mutex.h> // For recursive_mutex due to GetSerializeContext().

namespace AZ
{
    class ComponentApplication;
    class ComponentDescriptor;

    class Entity;
    class EntityId;

    class Module;
    class DynamicModuleHandle;

    class Component;

    namespace Internal
    {
        class ComponentFactoryInterface;
    }

    namespace Debug
    {
        class DrillerManager;
    }

    /**
     * Event bus for dispatching component application events to listeners.
     */
    class ComponentApplicationEvents
        : public AZ::EBusTraits
    {
    public:

        /**
         * Destroys a component application event bus.
         */
        virtual ~ComponentApplicationEvents() {}

        /**
         * Notifies listeners that an entity was added to the application.
         * @param entity The entity that was added to the application.
         */
        virtual void OnEntityAdded(AZ::Entity* entity) { (void)entity; }

        /**
         * Notifies listeners that an entity was removed from the application.
         * @param entity The entity that was removed from the application.
         */
        virtual void OnEntityRemoved(const AZ::EntityId& entityId) { (void)entityId; }
    };

    /**
     * Used when dispatching a component application event. 
     */
    typedef AZ::EBus<ComponentApplicationEvents> ComponentApplicationEventBus;

    /**
     * Event bus that components use to make requests of the main application.
     * Only one application can exist at a time, which is why this bus 
     * supports only one listener.
     */
    class ComponentApplicationRequests
        : public AZ::EBusTraits
    {
    public:

        /**
         * Destroys the event bus that components use to make requests of the main application.
         */
        virtual ~ComponentApplicationRequests() {}

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides - application is a singleton
        /**
         * Overrides the default AZ::EBusTraits handler policy to allow one
         * listener only, because only one application can exist at a time.
         */
        static const AZ::EBusHandlerPolicy HandlerPolicy = EBusHandlerPolicy::Single;  // We sort components on m_initOrder.
        /**
         * Overrides the default AZ::EBusTraits mutex type to the AZStd implementation of 
         * a recursive mutex with exclusive ownership semantics. A mutex prevents multiple 
         * threads from accessing shared data simultaneously.
         */
        typedef AZStd::recursive_mutex MutexType;
        //////////////////////////////////////////////////////////////////////////
        /**
         * Registers a component descriptor with the application.
         * @param descriptor A component descriptor.
         */
        virtual void RegisterComponentDescriptor(const ComponentDescriptor* descriptor) = 0;
        /**
         * Unregisters a component descriptor with the application.
         * @param descriptor A component descriptor.
         */
        virtual void UnregisterComponentDescriptor(const ComponentDescriptor* descriptor) = 0;
        /**
         * Gets a pointer to the application.
         * @return A pointer to the application.
         */
        virtual ComponentApplication*   GetApplication() = 0;

        /**
         * Adds an entity to the application's registry.
         * Calling Init() on an entity automatically performs this operation.
         * @param entity A pointer to the entity to add to the application's registry.
         * @return True if the operation succeeded. False if the operation failed.
         */
        virtual bool                    AddEntity(Entity* entity) = 0;
        /**
         * Removes the specified entity from the application's registry.
         * Deleting an entity automatically performs this operation.
         * @param entity A pointer to the entity that will be removed from the application's registry.
         * @return True if the operation succeeded. False if the operation failed.
         */
        virtual bool                    RemoveEntity(Entity* entity) = 0;
        /**
         * Unregisters and deletes the specified entity.
         * @param entity A reference to the entity that will be unregistered and deleted.
         * @return True if the operation succeeded. False if the operation failed.
         */
        virtual bool                    DeleteEntity(const EntityId& id) = 0;
        /**
         * Returns the entity with the matching ID, if the entity is registered with the application.
         * @param entity A reference to the entity that you are searching for.
         * @return A pointer to the entity with the specified entity ID.
         */
        virtual Entity*                 FindEntity(const EntityId& id) = 0;
        /**
         * Returns the name of the entity that has the specified entity ID.
         * Entity names are not unique.
         * This method exists to facilitate better debugging messages.
         * @param entity A reference to the entity whose name you are seeking.
         * @return The name of the entity with the specified entity ID. 
         * If no entity is found for the specified ID, it returns an empty string. 
         */
        virtual AZStd::string   GetEntityName(const EntityId& id) { (void)id; return AZStd::string(); };

        /**
         * The type that AZ::ComponentApplicationRequests::EnumerateEntities uses to
         * pass entity callbacks to the application for enumeration.
         */
        using EntityCallback = AZStd::function<void(Entity*)>;
        /**
         * Enumerates all registered entities and invokes the specified callback for each entity.
         * @param callback A reference to the callback that is invoked for each entity.
         */
        virtual void                    EnumerateEntities(const EntityCallback& callback) = 0;
        /**
         * Returns the serialize context that was registered with the app.
         * @return The serialize context, if there is one. SerializeContext is a class that contains reflection data
         * for serialization and construction of objects.
         */
        virtual class SerializeContext* GetSerializeContext() = 0;
        /**
         * Returns the behavior context that was registered with the app.
         * @return The behavior context, if there is one. BehaviorContext is a class that reflects classes, methods,
         * and EBuses for runtime interaction.
         */
        virtual class BehaviorContext*  GetBehaviorContext() = 0;
        /**
         * Gets the name of the working root folder that was registered with the app.
         * @return A pointer to the name of the app's root folder, if a root folder was registered.
         */
        virtual const char*             GetAppRoot() = 0;
        /**
         * Gets the path to the directory that contains the application's executable.
         * @return A pointer to the name of the path that contains the application's executable.
         */
        virtual const char*             GetExecutableFolder() = 0;
        /**
         * Returns a pointer to the driller manager, if driller is enabled.
         * The driller manager manages all active driller sessions and driller factories.
         * @return A pointer to the driller manager. If driller is not enabled,
         * this function returns null.
         */
        virtual Debug::DrillerManager*  GetDrillerManager() = 0;

        /// Requests reload of a dynamic application module.
        virtual void ReloadModule(const char* moduleFullPath) = 0;

        using EnumerateModulesCallback = AZStd::function<bool(Module* /*module*/, DynamicModuleHandle* /*handle*/)>;
        /// Calls cb on all loaded modules
        virtual void EnumerateModules(EnumerateModulesCallback cb) { (void)cb; };
    };

    /**
     * Used by components to make requests of the component application.
     */
    typedef AZ::EBus<ComponentApplicationRequests>  ComponentApplicationBus;
}

#endif // AZCORE_COMPONENT_APPLICATION_BUS_H
#pragma once