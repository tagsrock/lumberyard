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
#include "stdafx.h"
#include "PropertyEditorAPI.h"
#include <QtWidgets/QDoubleSpinBox>

#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/DynamicSerializableField.h>

#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>

namespace AzToolsFramework
{
    PropertyHandlerBase::PropertyHandlerBase()
    {
    }

    PropertyHandlerBase::~PropertyHandlerBase()
    {
    }

    // default impl deletes.
    void PropertyHandlerBase::DestroyGUI(QWidget* pTarget)
    {
        delete pTarget;
    }

    //-----------------------------------------------------------------------------
    NodeDisplayVisibility CalculateNodeDisplayVisibility(const InstanceDataNode& node)
    {
        NodeDisplayVisibility visibility = NodeDisplayVisibility::NotVisible;

        // If parent is a dynamic serializable field with edit reflection, default to visible.
        if (node.GetElementMetadata() && 0 != (node.GetElementMetadata()->m_flags & AZ::SerializeContext::ClassElement::FLG_DYNAMIC_FIELD))
        {
            if (node.GetParent() && node.GetParent()->GetElementEditMetadata())
            {
                visibility = NodeDisplayVisibility::Visible;
            }
        }

        // Use class meta data as opposed to parent's reflection data if this is a root node or a container element.
        if (visibility == NodeDisplayVisibility::NotVisible && (!node.GetParent() || node.GetParent()->GetClassMetadata()->m_container))
        {
            if (node.GetClassMetadata() && node.GetClassMetadata()->m_editData)
            {
                visibility = NodeDisplayVisibility::Visible;
            }
        }

        // Use class meta data as opposed to parent's reflection data if this is a base class element,
        // which isn't explicitly reflected by the containing class.
        if (   (visibility == NodeDisplayVisibility::NotVisible && node.GetElementMetadata())
            && (node.GetElementMetadata()->m_flags & AZ::SerializeContext::ClassElement::FLG_BASE_CLASS))
        {
            if (node.GetClassMetadata() && node.GetClassMetadata()->m_editData)
            {
                visibility = NodeDisplayVisibility::Visible;
            }
        }

        // Child nodes must have edit data in their parent's reflection.
        if (visibility == NodeDisplayVisibility::NotVisible && node.GetElementEditMetadata())
        {
            visibility = NodeDisplayVisibility::Visible;
        }

        // Finally, check against reflection attributes.
        if (visibility == NodeDisplayVisibility::Visible)
        {
            const AZ::Crc32 visibilityAttribute = ResolveVisibilityAttribute(node);

            if (visibilityAttribute == AZ::Edit::PropertyVisibility::Hide)
            {
                visibility = NodeDisplayVisibility::NotVisible;
            }
            else if (visibilityAttribute == AZ::Edit::PropertyVisibility::Show)
            {
                visibility = NodeDisplayVisibility::Visible;
            }
            else if (visibilityAttribute == AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            {
                visibility = NodeDisplayVisibility::ShowChildrenOnly;
            }
        }

        return visibility;
    }

    //-----------------------------------------------------------------------------
    AZStd::string GetNodeDisplayName(const AzToolsFramework::InstanceDataNode& node)
    {
        // Introspect template for generic component wrappers.
        if (node.GetClassMetadata() && 
            node.GetClassMetadata()->m_typeId == AZ::AzTypeInfo<AzToolsFramework::Components::GenericComponentWrapper>::Uuid())
        {
            if (node.GetNumInstances() > 0)
            {
                const Components::GenericComponentWrapper* componentWrapper =
                    static_cast<Components::GenericComponentWrapper*>(node.FirstInstance());
                return componentWrapper->GetDisplayName();
            }
            else
            {
                auto comparisonNode = node.GetComparisonNode();
                if (comparisonNode && comparisonNode->GetNumInstances() > 0)
                {
                    const Components::GenericComponentWrapper* componentWrapper =
                        static_cast<Components::GenericComponentWrapper*>(comparisonNode->FirstInstance());
                    return GetFriendlyComponentName(componentWrapper->GetTemplate());
                }
            }
        }

        // Otherwise use friendly reflection name.
        AZStd::string displayName;
        if (node.GetElementEditMetadata())
        {
            displayName = node.GetElementEditMetadata()->m_name;
        }
        else if (node.GetClassMetadata()->m_editData)
        {
            displayName = node.GetClassMetadata()->m_editData->m_name;
        }
        else if (node.GetElementMetadata() && node.GetElementMetadata()->m_nameCrc != AZ_CRC("element", 0x41405e39))
        {
            displayName = node.GetElementMetadata()->m_name;
        }
        else
        {
            displayName = node.GetClassMetadata()->m_name;
        }
        return displayName;
    }
    
    bool ReadVisibilityAttribute(void* instance, AZ::Edit::Attribute* attr, AZ::Crc32& visibility)
    {
        PropertyAttributeReader reader(instance, attr);
        if (reader.Read<AZ::Crc32>(visibility))
        {
            return true;
        }
        else
        {
            AZ::u32 valueCrc = 0;
            if (reader.Read<AZ::u32>(valueCrc))
            {
                // Assume crc returned as u32.
                visibility = valueCrc;

                // Support 0|1 return values.
                if (valueCrc == 0)
                {
                    visibility = AZ::Edit::PropertyVisibility::Hide;
                }
                else if (valueCrc == 1)
                {
                    visibility = AZ::Edit::PropertyVisibility::Show;
                }
                return true;
            }

            bool visible = false;
            if (reader.Read<bool>(visible))
            {
                visibility = visible ? AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
                return true;
            }
        }

        return false;
    }

    //-----------------------------------------------------------------------------
    AZ::Crc32 ResolveVisibilityAttribute(const InstanceDataNode& node)
    {
        // First check the data element metadata in the reflecting class
        if (const auto* editElement = node.GetElementEditMetadata())
        {
            for (size_t attrIndex = 0; attrIndex < editElement->m_attributes.size(); ++attrIndex)
            {
                const auto& attrPair = editElement->m_attributes[attrIndex];
                // Ensure the visibility attribute isn't intended for child elements
                if (attrPair.first == AZ::Edit::Attributes::Visibility && !attrPair.second->m_describesChildren)
                {
                    if (auto* parentInstance = node.GetParent())
                    {
                        AZ::Crc32 visibility;
                        for (size_t instIndex = 0; instIndex < parentInstance->GetNumInstances(); ++instIndex)
                        {
                            if (ReadVisibilityAttribute(parentInstance->GetInstance(instIndex), attrPair.second, visibility))
                            {
                                return visibility;
                            }
                        }
                    }
                }
            }
        }

        // Check for any element attributes on the parent container (if there is one)
        if (node.GetParent() && node.GetParent()->GetClassMetadata()->m_container)
        {
            if (const auto* editElement = node.GetParent()->GetElementEditMetadata())
            {
                for (size_t attrIndex = 0; attrIndex < editElement->m_attributes.size(); ++attrIndex)
                {
                    const auto& attrPair = editElement->m_attributes[attrIndex];
                    // Parent attributes must describe children to apply here
                    if (attrPair.first == AZ::Edit::Attributes::Visibility && attrPair.second->m_describesChildren)
                    {
                        if (auto* parentInstance = node.GetParent())
                        {
                            AZ::Crc32 visibility;
                            for (size_t instIndex = 0; instIndex < parentInstance->GetNumInstances(); ++instIndex)
                            {
                                if (ReadVisibilityAttribute(parentInstance->GetInstance(instIndex), attrPair.second, visibility))
                                {
                                    return visibility;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Check class editor metadata
        if (const auto* classElement = node.GetClassMetadata())
        {
            if (const auto* editElement = classElement->m_editData)
            {
                if (!editElement->m_elements.empty())
                {
                    const auto& element = *editElement->m_elements.begin();
                    for (const auto& attrPair : element.m_attributes)
                    {
                        if (attrPair.first == AZ::Edit::Attributes::Visibility)
                        {
                            AZ::Crc32 visibility;
                            for (size_t instIndex = 0; instIndex < node.GetNumInstances(); ++instIndex)
                            {
                                if (ReadVisibilityAttribute(node.GetInstance(instIndex), attrPair.second, visibility))
                                {
                                    return visibility;
                                }
                            }
                        }
                    }
                }
            }
        }

        // No one said no, show by default
        return AZ::Edit::PropertyVisibility::Show;
    }
}
