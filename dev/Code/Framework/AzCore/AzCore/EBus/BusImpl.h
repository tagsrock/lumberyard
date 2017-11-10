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

/**
 * @file
 * Header file for internal EBus classes.
 * For more information about EBuses, see AZ::EBus and AZ::EBusTraits in this guide and
 * [Event Bus](http://docs.aws.amazon.com/lumberyard/latest/developerguide/asset-pipeline-ebus.html)
 * in the *Lumberyard Developer Guide*.
 */

#pragma once

#include <AzCore/EBus/BusContainer.h>
#include <AzCore/EBus/HandlerContainer.h>
#include <AzCore/EBus/Policies.h>

#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/conditional.h>

namespace AZ
{
    /**
     * A dummy mutex that performs no locking.
     * EBuses that do not support multithreading use this mutex 
     * as their EBusTraits::MutexType.
     */
    struct NullMutex
    {
        AZ_FORCE_INLINE void lock() {}
        AZ_FORCE_INLINE bool try_lock() { return true; }
        AZ_FORCE_INLINE void unlock() {}
    };

    /**
     * Indicates that EBusTraits::BusIdType is not set.
     * EBuses with multiple addresses must set the EBusTraits::BusIdType.
     */
    struct NullBusId
    {
        AZ_FORCE_INLINE NullBusId() {};
        AZ_FORCE_INLINE NullBusId(int) {};
    };

    /// @cond EXCLUDE_DOCS
    AZ_FORCE_INLINE bool operator==(const NullBusId&, const NullBusId&) { return true; }
    AZ_FORCE_INLINE bool operator!=(const NullBusId&, const NullBusId&) { return false; }
    /// @endcond

    /**
     * Indicates that EBusTraits::BusIdOrderCompare is not set.
     * EBuses with ordered address IDs must specify a function for 
     * EBusTraits::BusIdOrderCompare.
     */
    struct NullBusIdCompare;

    namespace BusInternal
    {

        /**
         * Internal class that contains data about EBusTraits.
         * @tparam Interface A class whose virtual functions define the events that are 
         *                   dispatched or received by the EBus.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter is optional if the `Interface` class inherits from 
         *                   EBusTraits.
         */
        template <class Interface, class BusTraits>
        struct EBusImplTraits
        {
            /**
             * Properties that you use to configure an EBus.
             * For more information, see EBusTraits.
             */
            using Traits = BusTraits;

            /**
             * Allocator used by the EBus.
             * The default setting is AZStd::allocator, which uses AZ::SystemAllocator.
             */
            using AllocatorType = typename Traits::AllocatorType;

            /**
             * The class that defines the interface of the EBus.
             */
            using InterfaceType = Interface;

            /**
             * The events defined by the EBus interface.
             */
            using Events = Interface;

            /**
             * The type of ID that is used to address the EBus.
             * Used only when the address policy is AZ::EBusAddressPolicy::ById
             * or AZ::EBusAddressPolicy::ByIdAndOrdered.
             * The type must support `AZStd::hash<ID>` and
             * `bool operator==(const ID&, const ID&)`.
             */
            using BusIdType = typename Traits::BusIdType;

            /**
             * Sorting function for EBus address IDs.
             * Used only when the AddressPolicy is AZ::EBusAddressPolicy::ByIdAndOrdered.
             * If an event is dispatched without an ID, this function determines
             * the order in which each address receives the event.
             * The function must satisfy `AZStd::binary_function<BusIdType, BusIdType, bool>`.
             *
             * The following example shows a sorting function that meets these requirements.
             * @code{.cpp}
             * using BusIdOrderCompare = AZStd::less<BusIdType>; // Lesser IDs first.
             * @endcode
             */
            using BusIdOrderCompare = typename Traits::BusIdOrderCompare;

            /**
             * Locking primitive that is used when connecting handlers to the EBus or executing events.
             * By default, all access is assumed to be single threaded and no locking occurs.
             * For multithreaded access, specify a mutex of the following type.
             * - For simple multithreaded cases, use AZStd::mutex.
             * - For multithreaded cases where an event handler sends a new event on the same bus
             *   or connects/disconnects while handling an event on the same bus, use AZStd::recursive_mutex.
             */
            using MutexType = typename Traits::MutexType;

            /**
             * An address on the EBus.
             */
            using EBNode = typename AZStd::Utils::if_c<Traits::HandlerPolicy == EBusHandlerPolicy::Single, typename AZ::EBECSingle<Interface, Traits>,
                typename AZStd::Utils::if_c<Traits::HandlerPolicy == EBusHandlerPolicy::MultipleAndOrdered, typename AZ::EBECMultiOrdered<Interface, Traits>,
                typename AZ::EBECMulti<Interface, Traits> >::type>::type;

            /**
             * Contains all of the addresses on the EBus.
             */
            using BusesContainer = typename AZStd::Utils::if_c<Traits::AddressPolicy == EBusAddressPolicy::Single, typename AZ::EBBusSingle<EBNode>,
                typename AZStd::Utils::if_c<Traits::AddressPolicy == EBusAddressPolicy::ById, typename AZ::EBBusMulti<EBNode>,
                typename AZ::EBBusMultiOrdered<EBNode> >::type>::type;

            /**
             * Locking primitive that is used when executing events in the event queue.
             */
            using EventQueueMutexType = typename AZStd::Utils::if_c<AZStd::is_same<typename Traits::EventQueueMutexType, NullMutex>::value, // if EventQueueMutexType==NullMutex use MutexType otherwise EventQueueMutexType
                MutexType, typename Traits::EventQueueMutexType>::type;

            /**
             * Pointer to an address on the bus.
             */
            using BusPtr = typename EBNode::pointer;

            /**
             * Pointer to a handler node.
             */
            using HandlerNode = typename EBNode::HandlerNode;

            /**
             * Specifies whether the EBus supports an event queue.
             * You can use the event queue to execute events at a later time.
             * To execute the queued events, you must call
             * `<BusName>::ExecuteQueuedEvents()`.
             * By default, the event queue is disabled.
             */
            static const bool EnableEventQueue = Traits::EnableEventQueue;

            /**
             * True if the EBus supports more than one address. Otherwise, false.
             */
            static const bool HasId = Traits::AddressPolicy != EBusAddressPolicy::Single;
        };

        /**
         * Dispatches events to handlers that are connected to a specific address on an EBus.
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusEventer
        {
            /**
             * The type of ID that is used to address the EBus.
             * Used only when the address policy is AZ::EBusAddressPolicy::ById
             * or AZ::EBusAddressPolicy::ByIdAndOrdered.
             * The type must support `AZStd::hash<ID>` and
             * `bool operator==(const ID&, const ID&)`.
             */
            using BusIdType = typename Traits::BusIdType;

            /**
             * Pointer to an address on the bus.
             */
            using BusPtr = typename Traits::BusPtr;

            /**
             * Dispatches an event to handlers at a specific address.
             * @param id            Address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void Event(const BusIdType& id, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a specific address and receives results.
             * @param[out] results  Return value from the event.
             * @param id            Address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Results, class Function, class ... InputArgs>
            static void EventResult(Results& results, const BusIdType& id, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a cached address.
             * @param ptr           Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void Event(const BusPtr& ptr, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a cached address and receives results.
             * @param[out] results  Return value from the event.
             * @param ptr           Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Results, class Function, class ... InputArgs>
            static void EventResult(Results& results, const BusPtr& ptr, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a specific address in reverse order.
             * @param id            Address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void EventReverse(const BusIdType& id, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a specific address in reverse order and receives results.
             * @param[out] results  Return value from the event.
             * @param id            Address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Results, class Function, class ... InputArgs>
            static void EventResultReverse(Results& results, const BusIdType& id, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a cached address in reverse order.
             * @param ptr           Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void EventReverse(const BusPtr& ptr, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to handlers at a cached address in reverse order and receives results.
             * @param[out] results  Return value from the event.
             * @param ptr           Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Results, class Function, class ... InputArgs>
            static void EventResultReverse(Results& results, const BusPtr& ptr, Function func, InputArgs&& ... args);
        };

        /**
         * Provides functionality that requires enumerating over handlers that are connected 
         * to an EBus. It can enumerate over all handlers or just the handlers that are connected 
         * to a specific address on an EBus. 
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusEventEnumerator
        {
            /**
             * The type of ID that is used to address the EBus.
             * Used only when the address policy is AZ::EBusAddressPolicy::ById
             * or AZ::EBusAddressPolicy::ByIdAndOrdered.
             * The type must support `AZStd::hash<ID>` and
             * `bool operator==(const ID&, const ID&)`.
             */
            using BusIdType = typename Traits::BusIdType;

            /**
             * Pointer to an address on the bus.
             */
            using BusPtr = typename Traits::BusPtr;

            /**
             * Calls a user-defined function on all handlers that are connected to the EBus.
             * The function signature must be `bool callback(InterfaceType* handler)`.
             * The function must return true to continue enumerating handlers, or return false to stop.
             * @param callback  Function to call.
             */
            template <class Callback>
            static void EnumerateHandlers(Callback callback);

            /**
             * Calls a user-defined function on handlers that are connected to a specific address on the EBus.
             * The function signature must be `bool callback(InterfaceType* handler)`.
             * The function must return true to continue enumerating handlers, or return false to stop.
             * @param id        Address ID. Function will be called on handlers that are connected to this ID.
             * @param callback  Function to call.
             */
            template <class Callback>
            static void EnumerateHandlersId(const BusIdType& id, Callback callback);

            /**
             * Calls a user-defined function on handlers at a cached address.
             * The function signature must be `bool callback(InterfaceType* handler)`.
             * The function must return true to continue enumerating handlers, or return false to stop.
             * @param ptr       Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param callback  Function to call.
             */
            template <class Callback>
            static void EnumerateHandlersPtr(const BusPtr& ptr, Callback callback);

            /**
             * Finds the first handler that is connected to a specific address on the EBus.
             * This function is only for special cases where you know that a particular 
             * component's handler is guaranteed to exist.
             * Even if the returned pointer is valid (not null), it might point to a handler 
             * that was deleted. Prefer dispatching events using EBusEventer. 
             * @param id        Address ID. 
             * @return          A pointer to the first handler on the EBus, even if the handler 
             *                  was deleted. 
             */
            static typename Traits::InterfaceType* FindFirstHandler(const BusIdType& id);

            /**
             * Finds the first handler at a cached address on the EBus.
             * This function is only for special cases where you know that a particular
             * component's handler is guaranteed to exist.
             * Even if the returned pointer is valid (not null), it might point to a handler
             * that was deleted. Prefer dispatching events using EBusEventer.
             * @param ptr       Cached address ID.
             * @return          A pointer to the first handler on the specified EBus address, 
             *                  even if the handler was deleted.
             */
            static typename Traits::InterfaceType* FindFirstHandler(const BusPtr& ptr);
            
            /**
             * Returns the total number of event handlers that are connected to a specific 
             * address on the EBus.
             * @param id    Address ID. 
             * @return      The total number of handlers that are connected to the EBus address.
             */
            static size_t GetNumOfEventHandlers(const BusIdType& id);
        };

        /**
         * Dispatches an event to all handlers that are connected to an EBus.
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusBroadcaster
        {
            /**
             * Pointer to an address on the bus.
             */
            using BusPtr = typename Traits::BusPtr;
           
            /**
             * Dispatches an event to all handlers.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void Broadcast(Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to all handlers and receives results.
             * @param[out] results  Return value from the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Results, class Function, class ... InputArgs>
            static void BroadcastResult(Results& results, Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to all handlers in reverse order.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void BroadcastReverse(Function func, InputArgs&& ... args);

            /**
             * Dispatches an event to all handlers in reverse order and receives results.
             * @param[out] results  Return value from the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Results, class Function, class ... InputArgs>
            static void BroadcastResultReverse(Results& results, Function func, InputArgs&& ... args);
        };

        /**
         * Data type that is used when an EBus doesn't support queuing.
         */
        struct EBusNullQueue
        {
        };

        /**
         * EBus functionality related to the queuing of events and functions. 
         * This is specifically for queuing events and functions that will 
         * be broadcast to all handlers on the EBus.
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusBroadcastQueue
        {
            /**
             * Pointer to an address on the bus.
             */
            using BusPtr = typename Traits::BusPtr;

            /**
             * Executes queued events and functions.
             * Execution will occur on the thread that calls this function.
             * @see QueueBroadcast(), EBusEventQueue::QueueEvent(), QueueFunction(), ClearQueuedEvents()
             */
            static void ExecuteQueuedEvents() { Bus::GetContext().m_queue.Execute(); Bus::GetContext().m_functionQueue.Execute(); }

            /**
             * Clears the queue without calling events or functions.
             * Use in situations where memory must be freed immediately, such as shutdown.
             * Use with care. Cleared queued events will never be executed, and those events might have been expected.
             */
            static void ClearQueuedEvents() { Bus::GetContext().m_queue.Clear(); Bus::GetContext().m_functionQueue.Clear(); }

            /**
             * Sets whether function queuing is allowed.
             * This does not affect event queuing.
             * Function queuing is allowed by default when EBusTraits::EnableEventQueue
             * is true. It is never allowed when EBusTraits::EnableEventQueue is false.
             * @see QueueFunction, EBusTraits::EnableEventQueue
             * @param isAllowed Set to true to allow function queuing. Otherwise, set to false.
             */
            static void AllowFunctionQueuing(bool isAllowed) { Bus::GetContext().m_functionQueue.SetActive(isAllowed); }

            /**
             * Returns whether function queuing is allowed.
             * @return True if function queuing is allowed. Otherwise, false.
             * @see QueueFunction, AllowFunctionQueuing
             */
            static bool IsFunctionQueuing() { return Bus::GetContext().m_functionQueue.IsActive(); }

            /**
             * Enqueues an asynchronous event to dispatch to all handlers.
             * The event is not executed until ExecuteQueuedEvents() is called.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void QueueBroadcast(Function func, InputArgs&& ... args);

            /**
             * Enqueues an asynchronous event to dispatch to all handlers in reverse order.
             * The event is not executed until ExecuteQueuedEvents() is called.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void QueueBroadcastReverse(Function func, InputArgs&& ... args);

            /**
             * Enqueues an arbitrary callable function to be executed asynchronously.
             * The function is not executed until ExecuteQueuedEvents() is called.
             * The function might be unrelated to this EBus or any handlers.
             * Examples of callable functions are static functions, lambdas, and 
             * bound-member functions.
             *
             * One use case is to determine when a batch of queued events has finished.
             * When the function is executed, we know that all events that were queued   
             * before the function have finished executing.
             *
             * @param func Callable function.
             * @param args Arguments for `func`.
             */
            template <class Function, class ... InputArgs>
            static void QueueFunction(Function func, InputArgs&& ... args);
        };

        /**
         * Enqueues asynchronous events to dispatch to handlers that are connected to 
         * a specific address on an EBus. 
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusEventQueue
            : public EBusBroadcastQueue<Bus, Traits>
        {

            /**
             * The type of ID that is used to address the EBus.
             * Used only when the address policy is AZ::EBusAddressPolicy::ById
             * or AZ::EBusAddressPolicy::ByIdAndOrdered.
             * The type must support `AZStd::hash<ID>` and
             * `bool operator==(const ID&, const ID&)`.
             */
            using BusIdType = typename Traits::BusIdType;

            /**
             * Pointer to an address on the bus.
             */
            using BusPtr = typename Traits::BusPtr;

            /**
             * Enqueues an asynchronous event to dispatch to handlers at a specific address.
             * The event is not executed until ExecuteQueuedEvents() is called.
             * @param id            Address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void QueueEvent(const BusIdType& id, Function func, InputArgs&& ... args);

            /**
             * Enqueues an asynchronous event to dispatch to handlers at a cached address.
             * The event is not executed until ExecuteQueuedEvents() is called.
             * @param ptr           Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void QueueEvent(const BusPtr& ptr, Function func, InputArgs&& ... args);

            /**
             * Enqueues an asynchronous event to dispatch to handlers at a specific address in reverse order.
             * The event is not executed until ExecuteQueuedEvents() is called.
             * @param id            Address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void QueueEventReverse(const BusIdType& id, Function func, InputArgs&& ... args);

            /**
             * Enqueues an asynchronous event to dispatch to handlers at a cached address in reverse order.
             * The event is not executed until ExecuteQueuedEvents() is called.
             * @param ptr           Cached address ID. Handlers that are connected to this ID will receive the event.
             * @param func          Function pointer of the event to dispatch.
             * @param args          Function arguments that are passed to each handler.
             */
            template <class Function, class ... InputArgs>
            static void QueueEventReverse(const BusPtr& ptr, Function func, InputArgs&& ... args);
        };

        /**
         * Provides functionality that requires enumerating over all handlers that are 
         * connected to an EBus.
         * To enumerate over handlers that are connected to a specific address
         * on the EBus, use a function from EBusEventEnumerator.
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusBroadcastEnumerator
        {
             /**
              * Calls a user-defined function on all handlers that are connected to the EBus.
              * The function signature must be `bool callback(InterfaceType* handler)`.
              * The function must return true to continue enumerating handlers, or return false to stop.
              * @param callback  Function to call.
              */
            template <class Callback>
            static void EnumerateHandlers(Callback callback);

            /**
             * Finds the first handler that is connected to the EBus.
             * This function is only for special cases where you know that a particular
             * component's handler is guaranteed to exist.
             * Even if the returned pointer is valid (not null), it might point to a handler
             * that was deleted. Prefer dispatching events using EBusEventer.
             * @return          A pointer to the first handler on the EBus, even if the handler
             *                  was deleted.
             */
            static typename Traits::InterfaceType* FindFirstHandler();
        };

        /**
         * Base class that provides eventing, queueing, and enumeration functionality
         * for EBuses that dispatch events to handlers. Supports accessing handlers 
         * that are connected to specific addresses.
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         * @tparam BusIdType The type of ID that is used to address the EBus.
         */
        template <class Bus, class Traits, class BusIdType>
        struct EBusImpl
            : public EBusBroadcaster<Bus, Traits>
            , public EBusEventer<Bus, Traits>
            , public EBusEventEnumerator<Bus, Traits>
            , public AZStd::Utils::if_c<Traits::EnableEventQueue, EBusEventQueue<Bus, Traits>, EBusNullQueue>::type
        {
        };

        /**
         * Base class that provides eventing, queueing, and enumeration functionality 
         * for EBuses that dispatch events to all of their handlers. 
         * For a base class that can access handlers at specific addresses, use EBusImpl.
         * @tparam Bus       The EBus type.
         * @tparam Traits    A class that inherits from EBusTraits and configures the EBus.
         *                   This parameter may be left unspecified if the `Interface` class
         *                   inherits from EBusTraits.
         */
        template <class Bus, class Traits>
        struct EBusImpl<Bus, Traits, NullBusId>
            : public EBusBroadcaster<Bus, Traits>
            , public EBusBroadcastEnumerator<Bus, Traits>
            , public AZStd::Utils::if_c<Traits::EnableEventQueue, EBusBroadcastQueue<Bus, Traits>, EBusNullQueue>::type
        {
        };

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::Event(const BusIdType& id, Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();

                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&id, false, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }

                typename Traits::BusesContainer::iterator ebIter = context.m_buses.find(id);
                if (ebIter != context.m_buses.end())
                {
                    auto& ebBus = *Traits::BusesContainer::toNodePtr(ebIter);
                    if (ebBus.size())
                    {
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();
                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Results, class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::EventResult(Results& results, const BusIdType& id, Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();

                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&id, false, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }

                typename Traits::BusesContainer::iterator ebIter = context.m_buses.find(id);
                if (ebIter != context.m_buses.end())
                {
                    auto& ebBus = *Traits::BusesContainer::toNodePtr(ebIter);
                    if (ebBus.size())
                    {
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();
                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            results = ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::Event(const BusPtr& ptr, Function func, InputArgs&& ... args)
        {
            if (ptr)
            {
                auto& ebBus = *ptr;
                auto& context = Bus::GetContext();
                if (ebBus.size() || context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    if (context.m_routing.m_routers.size())
                    {
                        // Route the event and skip processing if RouteEvent returns true.
                        if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&ebBus.m_busId, false, false, func, args...))
                        {
                            context.m_mutex.unlock();
                            return;
                        }
                    }
                    typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                    typename Traits::EBNode::iterator ebEnd = ebBus.end();
                    while (ebCurrentEvent.m_iterator != ebEnd)
                    {
                        ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                    }
                    context.m_mutex.unlock();
                }
            }
        }

        template <class Bus, class Traits>
        template <class Results, class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::EventResult(Results& results, const BusPtr& ptr, Function func, InputArgs&& ... args)
        {
            if (ptr)
            {
                auto& ebBus = *ptr;
                auto& context = Bus::GetContext();
                if (ebBus.size() || context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    if (context.m_routing.m_routers.size())
                    {
                        // Route the event and skip processing if RouteEvent returns true.
                        if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&ebBus.m_busId, false, false, func, args...))
                        {
                            context.m_mutex.unlock();
                            return;
                        }
                    }
                    typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                    typename Traits::EBNode::iterator ebEnd = ebBus.end();
                    while (ebCurrentEvent.m_iterator != ebEnd)
                    {
                        results = ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                    }
                    context.m_mutex.unlock();
                }
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::EventReverse(const BusIdType& id, Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();
                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&id, false, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }
                typename Traits::BusesContainer::iterator ebIter = context.m_buses.find(id);
                if (ebIter != context.m_buses.end())
                {
                    auto& ebBus = *Traits::BusesContainer::toNodePtr(ebIter);
                    if (ebBus.size())
                    {
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::reverse_iterator> ebCurrentEvent(ebBus.rbegin(), &ebBus.m_busId);
                        while (ebCurrentEvent.m_iterator != ebBus.rend())
                        {
                            ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Results, class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::EventResultReverse(Results& results, const BusIdType& id, Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();
                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&id, false, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }
                typename Traits::BusesContainer::iterator ebIter = context.m_buses.find(id);
                if (ebIter != context.m_buses.end())
                {
                    auto& ebBus = *Traits::BusesContainer::toNodePtr(ebIter);
                    if (ebBus.size())
                    {
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::reverse_iterator> ebCurrentEvent(ebBus.rbegin(), &ebBus.m_busId);
                        while (ebCurrentEvent.m_iterator != ebBus.rend())
                        {
                            results = ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::EventReverse(const BusPtr& ptr, Function func, InputArgs&& ... args)
        {
            if (ptr)
            {
                auto& ebBus = *ptr;
                auto& context = Bus::GetContext();
                if (ebBus.size() || context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    if (context.m_routing.m_routers.size())
                    {
                        // Route the event and skip processing if RouteEvent returns true.
                        if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&ebBus.m_busId, false, true, func, args...))
                        {
                            context.m_mutex.unlock();
                            return;
                        }
                    }
                    typename Bus::template CallstackEntryIterator<typename Traits::EBNode::reverse_iterator> ebCurrentEvent(ebBus.rbegin(), &ebBus.m_busId);
                    while (ebCurrentEvent.m_iterator != ebBus.rend())
                    {
                        ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                    }
                    context.m_mutex.unlock();
                }
            }
        }

        template <class Bus, class Traits>
        template <class Results, class Function, class ... InputArgs>
        inline void EBusEventer<Bus, Traits>::EventResultReverse(Results& results, const BusPtr& ptr, Function func, InputArgs&& ... args)
        {
            if (ptr)
            {
                auto& ebBus = *ptr;
                auto& context = Bus::GetContext();
                if (ebBus.size() || context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    if (context.m_routing.m_routers.size())
                    {
                        // Route the event and skip processing if RouteEvent returns true.
                        if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&ebBus.m_busId, false, true, func, args...))
                        {
                            context.m_mutex.unlock();
                            return;
                        }
                    }
                    typename Bus::template CallstackEntryIterator<typename Traits::EBNode::reverse_iterator> ebCurrentEvent(ebBus.rbegin(), &ebBus);
                    while (ebCurrentEvent.m_iterator != ebBus.rend())
                    {
                        results = ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                    }
                    context.m_mutex.unlock();
                }
            }
        }

        template <class Bus, class Traits>
        template <class Callback>
        void EBusEventEnumerator<Bus, Traits>::EnumerateHandlers(Callback callback)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size())
            {
                context.m_mutex.lock();
                auto ebFirstBus = context.m_buses.begin();
                auto ebLastBus = context.m_buses.end();
                bool isAbortEnum = false;
                for (; ebFirstBus != ebLastBus && !isAbortEnum; )
                {
                    auto& ebBus = *ebFirstBus;
                    if (ebBus.size())
                    {
                        ebBus.add_ref();
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();

                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            if (!callback(*ebCurrentEvent.m_iterator++))
                            {
                                isAbortEnum = true;
                                break;
                            }
                        }
                        ++ebFirstBus;
                        ebBus.release();
                    }
                    else
                    {
                        ++ebFirstBus;
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Callback>
        void EBusEventEnumerator<Bus, Traits>::EnumerateHandlersId(const BusIdType& id, Callback callback)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size())
            {
                context.m_mutex.lock();
                auto ebIter = context.m_buses.find(id);
                if (ebIter != context.m_buses.end())
                {
                    auto& ebBus = *Traits::BusesContainer::toNodePtr(ebIter);
                    if (ebBus.size())
                    {
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();
                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            if (!callback(*ebCurrentEvent.m_iterator++))
                            {
                                break;
                            }
                        }
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Callback>
        void EBusEventEnumerator<Bus, Traits>::EnumerateHandlersPtr(const BusPtr& ptr, Callback callback)
        {
            if (ptr)
            {
                auto& ebBus = *ptr;
                if (ebBus.size())
                {
                    auto& context = Bus::GetContext();
                    context.m_mutex.lock();
                    typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                    typename Traits::EBNode::iterator ebEnd = ebBus.end();
                    while (ebCurrentEvent.m_iterator != ebEnd)
                    {
                        if (!callback(*ebCurrentEvent.m_iterator++))
                        {
                            break;
                        }
                    }
                    context.m_mutex.unlock();
                }
            }
        }

        template <class Bus, class Traits>
        typename Traits::InterfaceType * EBusEventEnumerator<Bus, Traits>::FindFirstHandler(const BusIdType& id)
        {
            typename Traits::InterfaceType* result = nullptr;
            EnumerateHandlersId(id, [&result](typename Traits::InterfaceType* handler)
            {
                result = handler;
                return false;
            });
            return result;
        }

        template <class Bus, class Traits>
        typename Traits::InterfaceType * EBusEventEnumerator<Bus, Traits>::FindFirstHandler(const BusPtr& ptr)
        {
            typename Traits::InterfaceType* result = nullptr;
            EnumerateHandlersPtr(ptr, [&result](typename Traits::InterfaceType* handler)
            {
                result = handler;
                return false;
            });
            return result;
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventQueue<Bus, Traits>::QueueEvent(const BusIdType& id, Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::MessageQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus doesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                if (context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&id, true, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                    context.m_mutex.unlock();
                }

                context.m_queue.m_messagesMutex.lock();
                context.m_queue.m_messages.push();
                typename Bus::MessageQueuePolicy::BusMessage& ebMsg = context.m_queue.m_messages.back();
                ebMsg.m_id = id;
                ebMsg.m_isUseId = true;
                ebMsg.m_isForward = true;
                ebMsg.m_invoke = typename Bus::MessageQueuePolicy::BusMessageCall(AZStd::bind(func, AZStd::placeholders::_1, args...), typename Traits::AllocatorType());
                context.m_queue.m_messagesMutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventQueue<Bus, Traits>::QueueEvent(const BusPtr& ptr, Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::MessageQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus doesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                if (context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&(*ptr).m_busId, true, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                    context.m_mutex.unlock();
                }

                context.m_queue.m_messagesMutex.lock();
                context.m_queue.m_messages.push();
                typename Bus::MessageQueuePolicy::BusMessage& ebMsg = context.m_queue.m_messages.back();
                ebMsg.m_isUseId = false;
                ebMsg.m_ptr = ptr;
                ebMsg.m_isForward = true;
                ebMsg.m_invoke = typename Bus::MessageQueuePolicy::BusMessageCall(AZStd::bind(func, AZStd::placeholders::_1, args...), typename Traits::AllocatorType());
                context.m_queue.m_messagesMutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventQueue<Bus, Traits>::QueueEventReverse(const BusIdType& id, Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::MessageQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus dBus::oesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                if (context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&id, true, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                    context.m_mutex.unlock();
                }
                context.m_queue.m_messagesMutex.lock();
                context.m_queue.m_messages.push();
                typename Bus::MessageQueuePolicy::BusMessage& ebMsg = context.m_queue.m_messages.back();
                ebMsg.m_id = id;
                ebMsg.m_isUseId = true;
                ebMsg.m_isForward = false;
                ebMsg.m_invoke = Bus::MessageQueuePolicy::BusMessageCall(AZStd::bind(func, AZStd::placeholders::_1, args...), typename Traits::AllocatorType());
                context.m_queue.m_messagesMutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusEventQueue<Bus, Traits>::QueueEventReverse(const BusPtr& ptr, Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::MessageQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus doesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_buses.size())
            {
                if (context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(&(*ptr).m_busId, true, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                    context.m_mutex.unlock();
                }

                context.m_queue.m_messagesMutex.lock();
                context.m_queue.m_messages.push();
                typename Bus::MessageQueuePolicy::BusMessage& ebMsg = context.m_queue.m_messages.back();
                ebMsg.m_isUseId = false;
                ebMsg.m_ptr = ptr;
                ebMsg.m_isForward = false;
                ebMsg.m_invoke = typename Bus::MessageQueuePolicy::BusMessageCall(AZStd::bind(func, AZStd::placeholders::_1, args...), typename Traits::AllocatorType());
                context.m_queue.m_messagesMutex.unlock();
            }
        }

        //=========================================================================
        // GetNumOfEventHandlers
        //=========================================================================
        template <class Bus, class Traits>
        size_t EBusEventEnumerator<Bus, Traits>::GetNumOfEventHandlers(const BusIdType& id)
        {
            size_t size = 0;
            auto& context = Bus::GetContext();
            context.m_mutex.lock();
            auto iter = context.m_buses.find(id);
            if (iter != context.m_buses.end())
            {
                size = Traits::BusesContainer::toNodePtr(iter)->size();
            }
            context.m_mutex.unlock();
            return size;
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusBroadcaster<Bus, Traits>::Broadcast(Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();
                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(nullptr, false, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }
                auto ebFirstBus = context.m_buses.begin();
                auto ebLastBus = context.m_buses.end();
                for (; ebFirstBus != ebLastBus; )
                {
                    auto& ebBus = *ebFirstBus;
                    if (ebBus.size())
                    {
                        ebBus.add_ref();
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();
                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                        ++ebFirstBus;
                        ebBus.release();
                    }
                    else
                    {
                        ++ebFirstBus;
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Results, class Function, class ... InputArgs>
        inline void EBusBroadcaster<Bus, Traits>::BroadcastResult(Results& results, Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();

                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(nullptr, false, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }

                auto ebFirstBus = context.m_buses.begin();
                auto ebLastBus = context.m_buses.end();
                for (; ebFirstBus != ebLastBus; )
                {
                    auto& ebBus = *ebFirstBus;
                    if (ebBus.size())
                    {
                        ebBus.add_ref();
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();
                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            results = ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                        ++ebFirstBus;
                        ebBus.release();
                    }
                    else
                    {
                        ++ebFirstBus;
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusBroadcaster<Bus, Traits>::BroadcastReverse(Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();
                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(nullptr, false, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }
                auto ebCurrentBus = context.m_buses.rbegin();
                for (; ebCurrentBus != context.m_buses.rend(); )
                {
                    auto& ebBus = *ebCurrentBus;
                    if (ebBus.size())
                    {
                        typename Traits::EBNode* iteratorLockedBus = nullptr;
                        if (ebCurrentBus != context.m_buses.rbegin())
                        {
                            // The reverse iterator (ebCurrentBus) internally points to the previous element (the next forward iterator).
                            // It's important to make sure that this iterator stays valid, so we hold a lock on that element until we 
                            // have moved to the next one. Moving to the next element will happen after processing current bus. While 
                            // processing, we can change the bus container (add/remove listeners).
                            iteratorLockedBus = &*AZStd::prior(ebCurrentBus);
                            iteratorLockedBus->add_ref();
                        }

                        ebBus.add_ref(); // Hold a reference to the bus we are processing, in case it gets deleted (remove all handlers).
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::reverse_iterator> ebCurrentEvent(ebBus.rbegin(), &ebBus.m_busId);
                        while (ebCurrentEvent.m_iterator != ebBus.rend())
                        {
                            ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                        ebBus.release(); // Release the reference to the bus we are processing. It will be deleted if needed.

                        if (iteratorLockedBus == nullptr) // If iteratorLockedBus is null, ebCurrentBus is rbegin. Make sure we update the iterator.
                        {
                            ebCurrentBus = context.m_buses.rbegin();
                        }

                        // Since rend() internally points to begin(), it might have changed during the message (e.g., bus was removed).  
                        // Make sure that we are not at the end before moving to the next element, which would be an invalid operation.
                        if (ebCurrentBus != context.m_buses.rend())
                        {
                            // During message processing we could have removed/added buses. Since the reverse iterator points
                            // to the next forward iterator, the elements it points to can change. In this case, we don't need
                            // to move the reverse iterator.
                            if (&*ebCurrentBus == &ebBus)
                            {
                                ++ebCurrentBus; // If we did not remove the bus we just processed, move to the next one.
                            }
                        }

                        if (iteratorLockedBus)
                        {
                            // We are done moving the ebCurrentBus iterator. It's safe to release the lock on the element the iterator was pointing to.
                            iteratorLockedBus->release();
                        }
                    }
                    else
                    {
                        ++ebCurrentBus;
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Results, class Function, class ... InputArgs>
        inline void EBusBroadcaster<Bus, Traits>::BroadcastResultReverse(Results& results, Function func, InputArgs&& ... args)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                context.m_mutex.lock();
                if (context.m_routing.m_routers.size())
                {
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(nullptr, false, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                }
                auto ebCurrentBus = context.m_buses.rbegin();
                for (; ebCurrentBus != context.m_buses.rend(); )
                {
                    auto& ebBus = *ebCurrentBus;
                    if (ebBus.size())
                    {
                        typename Traits::EBNode* iteratorLockedBus = nullptr;
                        if (ebCurrentBus != context.m_buses.rbegin())
                        {
                            // The reverse iterator (ebCurrentBus) internally points to the previous element (the next forward iterator).
                            // It's important to make sure that this iterator stays valid, so we hold a lock on that element until we 
                            // have moved to the next one. Moving to the next element will happen after processing current bus. While 
                            // processing, we can change the bus container (add/remove listeners).
                            iteratorLockedBus = &*AZStd::prior(ebCurrentBus);
                            iteratorLockedBus->add_ref();
                        }

                        ebBus.add_ref(); // Hold a reference to the bus we are processing, in case it gets deleted (remove all handlers).
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::reverse_iterator> ebCurrentEvent(ebBus.rbegin(), &ebBus.m_busId);
                        while (ebCurrentEvent.m_iterator != ebBus.rend())
                        {
                            results = ((*ebCurrentEvent.m_iterator++)->*func)(args...);
                        }
                        ebBus.release(); // Release the reference to the bus we are processing. It will be deleted if needed.

                        if (iteratorLockedBus == nullptr) // If iteratorLockedBus is null, ebCurrentBus is rbegin. Make sure we update the iterator.
                        {
                            ebCurrentBus = context.m_buses.rbegin();
                        }

                        // Since rend() internally points to begin(), it might have changed during the message (e.g., bus was removed).  
                        // Make sure that we are not at the end before moving to the next element, which would be an invalid operation.
                        if (ebCurrentBus != context.m_buses.rend())
                        {
                            // During message processing we could have removed/added buses. Since the reverse iterator points
                            // to the next forward iterator, the elements it points to can change. In this case, we don't need
                            // to move the reverse iterator.
                            if (&*ebCurrentBus == &ebBus)
                            {
                                ++ebCurrentBus; // If we did not remove the bus we just processed, move to the next one.
                            }
                        }

                        if (iteratorLockedBus)
                        {
                            // We are done moving the ebCurrentBus iterator. It's safe to release the lock on the element the iterator was pointing to.
                            iteratorLockedBus->release();
                        }
                    }
                    else
                    {
                        ++ebCurrentBus;
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Callback>
        void EBusBroadcastEnumerator<Bus, Traits>::EnumerateHandlers(Callback callback)
        {
            auto& context = Bus::GetContext();
            if (context.m_buses.size())
            {
                context.m_mutex.lock();
                auto ebFirstBus = context.m_buses.begin();
                auto ebLastBus = context.m_buses.end();
                bool isAbortEnum = false;
                for (; ebFirstBus != ebLastBus && !isAbortEnum; )
                {
                    typename Traits::EBNode& ebBus = *ebFirstBus;
                    if (ebBus.size())
                    {
                        ebBus.add_ref();
                        typename Bus::template CallstackEntryIterator<typename Traits::EBNode::iterator> ebCurrentEvent(ebBus.begin(), &ebBus.m_busId);
                        typename Traits::EBNode::iterator ebEnd = ebBus.end();

                        while (ebCurrentEvent.m_iterator != ebEnd)
                        {
                            if (!callback(*ebCurrentEvent.m_iterator++))
                            {
                                isAbortEnum = true;
                                break;
                            }
                        }
                        ++ebFirstBus;
                        ebBus.release();
                    }
                    else
                    {
                        ++ebFirstBus;
                    }
                }
                context.m_mutex.unlock();
            }
        }

        template <class Bus, class Traits>
        typename Traits::InterfaceType * EBusBroadcastEnumerator<Bus, Traits>::FindFirstHandler()
        {
            typename Traits::InterfaceType* result = nullptr;
            EnumerateHandlers([&result](typename Traits::InterfaceType* handler)
            {
                result = handler;
                return false;
            });
            return result;
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusBroadcastQueue<Bus, Traits>::QueueBroadcast(Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::MessageQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus doesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                if (context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(nullptr, true, false, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                    context.m_mutex.unlock();
                }

                context.m_queue.m_messagesMutex.lock();
                context.m_queue.m_messages.push();
                typename Bus::MessageQueuePolicy::BusMessage& ebMsg = context.m_queue.m_messages.back();
                ebMsg.m_ptr = nullptr;
                ebMsg.m_isUseId = false;
                ebMsg.m_isForward = true;
                ebMsg.m_invoke = typename Bus::MessageQueuePolicy::BusMessageCall(AZStd::bind(func, AZStd::placeholders::_1, args...), typename Traits::AllocatorType());
                context.m_queue.m_messagesMutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusBroadcastQueue<Bus, Traits>::QueueBroadcastReverse(Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::MessageQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus doesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_buses.size() || context.m_routing.m_routers.size())
            {
                if (context.m_routing.m_routers.size())
                {
                    context.m_mutex.lock();
                    // Route the event and skip processing if RouteEvent returns true.
                    if (context.m_routing.template RouteEvent<typename Bus::RouterCallstackEntry>(nullptr, true, true, func, args...))
                    {
                        context.m_mutex.unlock();
                        return;
                    }
                    context.m_mutex.unlock();
                }

                context.m_queue.m_messagesMutex.lock();
                context.m_queue.m_messages.push();
                typename Bus::MessageQueuePolicy::BusMessage& ebMsg = context.m_queue.m_messages.back();
                ebMsg.m_ptr = nullptr;
                ebMsg.m_isUseId = false;
                ebMsg.m_isForward = false;
                ebMsg.m_invoke = typename Bus::MessageQueuePolicy::BusMessageCall(AZStd::bind(func, AZStd::placeholders::_1, args...), typename Traits::AllocatorType());
                context.m_queue.m_messagesMutex.unlock();
            }
        }

        template <class Bus, class Traits>
        template <class Function, class ... InputArgs>
        inline void EBusBroadcastQueue<Bus, Traits>::QueueFunction(Function func, InputArgs&& ... args)
        {
            AZ_STATIC_ASSERT((AZStd::is_same<typename Bus::FunctionQueuePolicy::BusMessageCall, typename AZ::Internal::NullBusMessageCall>::value == false),
                "This EBus doesn't support queued events! Check 'EnableEventQueue'");
            auto& context = Bus::GetContext();
            if (context.m_functionQueue.IsActive())
            {
                context.m_functionQueue.m_messagesMutex.lock();
                context.m_functionQueue.m_messages.push(typename Bus::FunctionQueuePolicy::BusMessageCall(AZStd::bind(func, args...), typename Traits::AllocatorType()));
                context.m_functionQueue.m_messagesMutex.unlock();
            }
            else
            {
                AZ_Warning("System", false, "You are trying to queue function on an EBus, but function queuing is NOT enabled! The function will not be executed/called!");
            }
        }
    }
}
