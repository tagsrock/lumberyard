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
 * Header file for the editor component base class.
 * Derive from this class to create a version of a component to use in the 
 * editor, as opposed to the version of the component that is used during run time. 
 * To learn more about editor components, see the [Lumberyard Developer Guide] 
 * (http://docs.aws.amazon.com/lumberyard/latest/developerguide/component-entity-system-pg-editor-components.html).
 */

#ifndef EDITOR_COMPONENT_BASE_H
#define EDITOR_COMPONENT_BASE_H

#include <AzCore/base.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Math/transform.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/Entity.h>

namespace AZ
{
    class Vector2;
}

namespace AzToolsFramework
{
    namespace Components
    {
        class SelectionComponent;

        /**
         * A base class for all editor components.
         * Derive from this class to create a version of a component to use in the
         * editor, as opposed to the version of the component that is used during runtime.
         *
         * **Important:** Game components must not inherit from EditorComponentBase. 
         * To create one or more game components to represent your editor component 
         * in runtime, use BuildGameEntity(). 
         *
         * To learn more about editor components, see the [Lumberyard Developer Guide]
         * (http://docs.aws.amazon.com/lumberyard/latest/developerguide/component-entity-system-pg-editor-components.html).
         */
        class EditorComponentBase
            : public AZ::Component
        {
            friend class EditorEntityActionComponent;
            friend class EditorDisabledCompositionComponent;
            friend class EditorPendingCompositionComponent;
        public:

            /**
             * Adds run-time type information to the component.
             */
            AZ_RTTI(EditorComponentBase, "{D5346BD4-7F20-444E-B370-327ACD03D4A0}", AZ::Component);

            /**
             * Creates an instance of this class.
             */
            EditorComponentBase();

            /**
             * Sets a flag on the entire entity to indicate that the entity's properties 
             * were modified. 
             * Call this function whenever you alter an entity in an unexpected manner. 
             * For example, edits that you make to one entity might affect other entities,  
             * so the affected entities need to know that something changed.
             * You do not need to call this function when editing an entity's property   
             * in the Property Editor, because that scenario automatically sets the flag. 
             * You need to call this function only when your entity's properties are  
             * modified outside the Property Editor, such as when a script loops over  
             * all lights and alters their radii.
             */
            void SetDirty();

            //////////////////////////////////////////////////////////////////////////
            // AZ::Component
            /** 
             * Initializes the component's resources.
             * Overrides AZ::Component::Init(). 
             *
             * **Important:** %Components derived from EditorComponentBase must 
             * call the Init() function of the base class. 
             *
             * (Optional) You can override this function to initialize  
             * resources that the component needs.
             */
            virtual void Init() override;

            /**
             * Gets the transform component and selection component of the 
             * entity that the component belongs to, if the entity has them.
             * Overrides AZ::Component::Activate().
             *
             * **Important:** %Components derived from EditorComponentBase must
             * call the Activate() function of the base class.
             */
            virtual void Activate() override;

            /**
             * Sets the component's pointers to the transform component  
             * and selection component to null.
             * Overrides AZ::Component::Deactivate().
             *
             * **Important:** %Components derived from EditorComponentBase must
             * call the Deactivate() function of the base class.
             */
            virtual void Deactivate() override;
            //////////////////////////////////////////////////////////////////////////

            /**
             * Gets the transform interface of the entity that the component   
             * belongs to, if the entity has a transform component.
             * A transform positions, rotates, and scales an entity in 3D space.
             * @return A pointer to the transform interface. Might be null if  
             * you did not include "TransformService" in the component's 
             * AZ::ComponentDescriptor::GetRequiredServices().
             */
            AZ::TransformInterface* GetTransform() const;

            /**
             * Gets the selection component of the entity that the component 
             * belongs to, if the entity has a selection component.
             * A selection component keeps track of whether the entity is 
             * selected in the editor.
             * @return A pointer to the selection component. Might be null if 
             * you did not include "SelectionService" in the component's 
             * AZ::ComponentDescriptor::GetRequiredServices().
             */
            SelectionComponent* GetSelection() const;

            /**
             * Gets the world transform of the entity that the component belongs  
             * to, if the entity has a transform component.
             * An entity's world transform is the entity's position within the  
             * entire game space.
             * @return The world transform, if the entity has one. Otherwise, returns 
             * the identity transform, which is the equivalent of no transform.
             */
            AZ::Transform GetWorldTM() const;
           
            /**
             * Gets the local transform of the entity that the component belongs
             * to, if the entity has a transform component.
             * An entity's local transform is the entity's position relative to its 
             * parent entity. 
             * @return The local transform, if the entity has one. Otherwise, returns
             * the identity transform, which is the equivalent of no transform.
             */
            AZ::Transform GetLocalTM() const;

            /**
             * Identifies whether the component is selected in the editor.
             * @return True if the component is selected in the editor. 
             * Otherwise, false.
             */
            bool IsSelected() const;

            /**
             * Identifies whether the component is the primary selection in the editor. 
             * @return True if the component is the primary selection in the editor.
             * Otherwise, false.
             */
            bool IsPrimarySelection() const;

            /// @cond EXCLUDE_DOCS
            void UnregisterIcon();
            /// @endcond

            /**
             * Determines if the entity that the component belongs to 
             * has a selection component.
             * A selection component keeps track of whether the entity is 
             * selected in the editor.
             * @return True if the entity has a selection component. 
             * Otherwise, false.
             */
            bool HasSelectionComponent() const { return m_selection != nullptr; }

            /**
             * Override this function to create one or more game components 
             * to represent your editor component in runtime. 
             *
             * **Important:** If your entity has a game component, you must implement this function.
             *
             * This function is called by the slice builder. Any game components  
             * that you create should be attached to the game entity that is 
             * provided to this function. If you do not need to create a game  
             * component, you do not need to override this function. 
             * @param gameEntity A pointer to the game entity.
             */
            virtual void BuildGameEntity(AZ::Entity* /*gameEntity*/) {}

            /**
             * Called by the slice builder after the game entity is saved, right  
             * before deleting the game entity. If you want to retain ownership   
             * of a component that you gave to the game entity using BuildGameEntity(),  
             * remove the component from the game entity here.
             * @param gameEntity A pointer to the game entity.
             */
            virtual void FinishedBuildingGameEntity(AZ::Entity* /*gameEntity*/) {}

            /**
             * Implement this function to support dragging and dropping an asset 
             * onto this component.
             * @param assetId A reference to the ID of the asset to drag and drop.
             */
             virtual void SetPrimaryAsset(const AZ::Data::AssetId& /*assetId*/) { }

            /**
             * Reflects component data into a variety of contexts (script, serialize, 
             * edit, and so on).
             * @param context A pointer to the reflection context.
             */
            static void Reflect(AZ::ReflectContext* context);

        private:
            AZ::TransformInterface* m_transform;
            SelectionComponent* m_selection;
        };

        /// @cond EXCLUDE_DOCS
        
        /**
         * Interface for AzToolsFramework::Components::EditorComponentDescriptorBus, 
         * which handles requests to the editor component regarding editor-only functionality.
         * Do not assume that all editor components have it.
         */        
        class EditorComponentDescriptor
        {
        public:

            /**
             * Checks the equality of components. 
             *
             * If you want your component to have a custom DoComponentsMatch() 
             * function, you need to do the following:
             * - Put the AZ_EDITOR_COMPONENT macro in your class, instead of AZ_COMPONENT.
             * - Define a static DoComponentsMatch() function with the following signature:
             *
             * `bool DoComponentsMatch(const ComponentClass*, const ComponentClass*); // where ComponentClass is the type of the class containing this function.`
             *
             * For example, ScriptComponents have a custom DoComponentsMatch() function 
             * so that two ScriptComponents, which are in C++, are determined to be equal   
             * only if they use the same Lua file to define their behavior.
             *
             * @param thisComponent The first component to compare.
             * @param otherComponent The component to compare with the first component.
             * @return True if the components are the same. Otherwise, false.
             */
            virtual bool DoComponentsMatch(const AZ::Component* thisComponent, const AZ::Component* otherComponent) const = 0;

            /**
             * Returns the editor component descriptor of the current component.
             * @return A pointer to the editor component descriptor.
             */
            virtual EditorComponentDescriptor* GetEditorDescriptor() { return this; }
        };

        /**
         * The properties of the editor component descriptor EBus.
         */
        using EditorComponentDescriptorBusTraits = AZ::ComponentDescriptorBusTraits;

        /**
         * An EBus for requests to the editor component.
         * The events are defined in the AzToolsFramework::Components::EditorComponentDescriptor class.
         */
        using EditorComponentDescriptorBus = AZ::EBus<EditorComponentDescriptor, EditorComponentDescriptorBusTraits>;

        /**
         * The default editor component descriptor. The editor component descriptor is the  
         * interface for AzToolsFramework::Components::EditorComponentDescriptorBus, 
         * which handles requests to the component regarding editor-only functionality. 
         * @tparam ComponentClass The type of component.
         */
        template <class ComponentClass>
        class EditorComponentDescriptorDefault
            : public AZ::ComponentDescriptorDefault<ComponentClass>
            , public EditorComponentDescriptorBus::Handler
        {
        public:
            /**
             * Specifies that this class should use AZ::SystemAllocator for memory
             * management by default.
             */
            AZ_CLASS_ALLOCATOR(EditorComponentDescriptorDefault<ComponentClass>, AZ::SystemAllocator, 0);

            AZ_HAS_STATIC_MEMBER(EditorComponentMatching, DoComponentsMatch, bool, (const ComponentClass* thisComponent, const ComponentClass* otherComponent));
            
            /**
             * Creates an instance of this class.
             */
            EditorComponentDescriptorDefault()
            {
                EditorComponentDescriptorBus::Handler::BusConnect(AZ::AzTypeInfo<ComponentClass>::Uuid());
            }

            /**
             * Checks whether two components are the same.
             * @param thisComponent The first component to compare.
             * @param otherComponent The component to compare with the first component.
             * @return True if the components are the same. Otherwise, false.
             */
            bool DoComponentsMatch(const AZ::Component* thisComponent, const AZ::Component* otherComponent) const override
            {
                auto thisActualComponent = azrtti_cast<const ComponentClass*>(thisComponent);
                AZ_Assert(thisActualComponent, "Used the wrong descriptor to check if components match");

                auto otherActualComponent = azrtti_cast<const ComponentClass*>(otherComponent);
                if (!otherActualComponent)
                {
                    return false;
                }

                return CallDoComponentsMatch(thisActualComponent, otherActualComponent, typename HasEditorComponentMatching<ComponentClass>::type());
            }

        private:
            bool CallDoComponentsMatch(const ComponentClass* thisComponent, const ComponentClass* otherComponent, const AZStd::true_type&) const
            {
                return ComponentClass::DoComponentsMatch(thisComponent, otherComponent);
            }

            bool CallDoComponentsMatch(const ComponentClass* /*thisComponent*/, const ComponentClass* /*outerComponent*/, const AZStd::false_type&) const
            {
                return true;
            }
        };
        
        /**
         * Declares an editor component descriptor class.
         * Unless you are implementing very advanced internal functionality, we recommend 
         * using AZ_EDITOR_COMPONENT instead of this macro. You can use this macro to implement 
         * a static function in the component class instead of writing a descriptor. It defines 
         * a CreateDescriptorFunction that you can call to register a descriptor. 
         * (Only one descriptor can exist per environment.) This macro fails silently if you 
         * implement the functions with the wrong signatures.
         */
        #define AZ_EDITOR_COMPONENT_INTRUSIVE_DESCRIPTOR_TYPE(_ComponentClass)                                  \
        friend class AZ::ComponentDescriptorDefault<_ComponentClass>;                                           \
        friend class AzToolsFramework::Components::EditorComponentDescriptorDefault<_ComponentClass>;           \
        typedef AzToolsFramework::Components::EditorComponentDescriptorDefault<_ComponentClass> DescriptorType;


        /**
         * Declares an editor component with the default settings.
         * The component derives from AzToolsFramework::Components::EditorComponentBase,  
         * is not templated, uses AZ::SystemAllocator, and so on. 
         * AZ_EDITOR_COMPONENT(_ComponentClass, _ComponentId, OtherBaseClasses... EditorComponentBase) 
         * is included automatically.
         * @note Editor components use a separate descriptor than the underlying component system.
         */
        #define AZ_EDITOR_COMPONENT(_ComponentClass, ...)                                       \
        AZ_RTTI(_ComponentClass, __VA_ARGS__, AzToolsFramework::Components::EditorComponentBase)\
        AZ_EDITOR_COMPONENT_INTRUSIVE_DESCRIPTOR_TYPE(_ComponentClass)                          \
        AZ_COMPONENT_BASE(_ComponentClass, __VA_ARGS__);
        /// @endcond

    } // namespace Components
} // namespace AzToolsFramework



#endif
