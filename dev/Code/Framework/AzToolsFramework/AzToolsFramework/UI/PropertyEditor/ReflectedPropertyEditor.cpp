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
#include "ReflectedPropertyEditor.hxx"
#include "PropertyRowWidget.hxx"
#include <AzCore/UserSettings/UserSettings.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Sfmt.h>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QMenu>
#include <QtCore/QTimer>

namespace AzToolsFramework
{
    class ReflectedPropertyEditorState
        : public AZ::UserSettings
    {
    public:
        AZ_CLASS_ALLOCATOR(ReflectedPropertyEditorState, AZ::SystemAllocator, 0);
        AZ_RTTI(ReflectedPropertyEditorState, "{A229B615-622B-4C0B-A17C-A1F5C3144D6E}", AZ::UserSettings);

        AZStd::unordered_set<AZ::u32> m_expandedElements; // crc of them + their parents.
        AZStd::unordered_set<AZ::u32> m_savedElements; // Elements we are aware of and have a valid ExpandedElements structure for

        ReflectedPropertyEditorState()
        {
        }

        // the Reflected Property Editor remembers which properties are expanded:
        static void Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<ReflectedPropertyEditorState>()
                    ->Version(3)
                    ->Field("m_expandedElements", &ReflectedPropertyEditorState::m_expandedElements)
                    ->Field("m_savedElements", &ReflectedPropertyEditorState::m_savedElements);
            }
        }

        void SetExpandedState(const AZ::u32 key, const bool state)
        {
            if (key != 0)
            {
                m_savedElements.insert(key);

                if (state)
                {
                    m_expandedElements.insert(key);
                }
                else
                {
                    m_expandedElements.erase(key);
                }
            }
        }

        bool GetExpandedState(const AZ::u32 key) const
        {
            return m_expandedElements.find(key) != m_expandedElements.end();
        }

        bool HasExpandedState(const AZ::u32 key) const
        {
            return m_savedElements.find(key) != m_savedElements.end();
        }
    };

    ReflectedPropertyEditor::ReflectedPropertyEditor(QWidget* pParent)
        : QFrame(pParent)
    {
        m_propertyLabelWidth = 200;

        m_expansionDepth = 0;
        m_savedStateKey = 0;
        setLayout(aznew QVBoxLayout());
        layout()->setSpacing(0);
        layout()->setContentsMargins(0, 0, 0, 0);

        m_mainScrollArea = nullptr; // we only create this if needed

        m_containerWidget = aznew QWidget(this);
        m_containerWidget->setObjectName("ContainerForRows");
        m_rowLayout = aznew QVBoxLayout(m_containerWidget);
        m_rowLayout->setContentsMargins(0, 0, 0, 0);
        m_rowLayout->setSpacing(0);

        BusConnect();
        m_queuedRefreshLevel = Refresh_None;
        m_ptrNotify = nullptr;
        m_readOnly = false;
        m_hideRootProperties = false;
        m_queuedTabOrderRefresh = false;
        m_spacer = nullptr;
        m_context = nullptr;
        m_selectedRow = nullptr;
        m_selectionEnabled = false;
        m_autoResizeLabels = false;
    }

    ReflectedPropertyEditor::~ReflectedPropertyEditor()
    {
        BusDisconnect();
    }

    void ReflectedPropertyEditor::Setup(AZ::SerializeContext* context, IPropertyEditorNotify* pnotify, bool enableScrollbars, int propertyLabelWidth)
    {
        m_ptrNotify = pnotify;
        m_context = context;

        m_propertyLabelWidth = propertyLabelWidth;

        if (!enableScrollbars)
        {
            // NO SCROLL BARS LAYOUT:
            // this (VBoxLayout)   - created in constructor
            //      - Container Widget (VBoxLayout) - created in constructor
            //      - Spacer to eat up remaining Space
            qobject_cast<QVBoxLayout*>(layout())->insertWidget(0, m_containerWidget);
        }
        else
        {
            // SCROLL BARS layout
            // this (VBoxLayout)
            //      - Scroll Area
            //          - Container Widget (VBoxLayout)
            //      - Spacer to eat up remaining space
            m_mainScrollArea = aznew QScrollArea(this);
            m_mainScrollArea->setWidgetResizable(true);
            m_mainScrollArea->setFocusPolicy(Qt::ClickFocus);
            m_containerWidget->setParent(m_mainScrollArea);
            m_mainScrollArea->setWidget(m_containerWidget);
            qobject_cast<QVBoxLayout*>(layout())->insertWidget(0, m_mainScrollArea);
        }
    }

    void ReflectedPropertyEditor::SetReadOnly(bool readOnly)
    {
        m_readOnly = readOnly;

        for (auto widget : m_widgets)
        {
            widget.second->SetReadOnly(readOnly);
        }

        for (auto widget : m_groupWidgets)
        {
            widget.second->SetReadOnly(readOnly);
        }
    }

    void ReflectedPropertyEditor::SetHideRootProperties(bool hideRootProperties)
    {
        m_hideRootProperties = hideRootProperties;
    }

    bool ReflectedPropertyEditor::AddInstance(void* instance, const AZ::Uuid& classId, void* aggregateInstance, void* compareInstance)
    {
#if defined(_DEBUG)
        for (auto testIt = m_instances.begin(); testIt != m_instances.end(); ++testIt)
        {
            for (size_t idx = 0; idx < testIt->GetNumInstances(); ++idx)
            {
                AZ_Assert(testIt->GetInstance(idx) != instance, "Attempt to add a duplicate instance to a property editor.");
            }
        }
#endif
        if (aggregateInstance)
        {
            // find aggregate instance
            for (InstanceDataHierarchyList::iterator it = m_instances.begin(); it != m_instances.end(); ++it)
            {
                if (it->ContainsRootInstance(aggregateInstance))
                {
                    it->AddRootInstance(instance, classId);

                    if (compareInstance)
                    {
                        it->AddComparisonInstance(compareInstance, classId);
                    }

                    return true;
                }
            }
        }
        else
        {
            m_instances.push_back();
            m_instances.back().SetValueComparisonFunction(m_valueComparisonFunction);
            m_instances.back().AddRootInstance(instance, classId);

            if (compareInstance)
            {
                m_instances.back().AddComparisonInstance(compareInstance, classId);
            }

            return true;
        }
        return false;
    }

    void ReflectedPropertyEditor::EnumerateInstances(InstanceDataHierarchyCallBack enumerationCallback)
    {
        for (auto& instance : m_instances)
        {
            enumerationCallback(instance);
        }
    }

    void ReflectedPropertyEditor::SetValueComparisonFunction(const InstanceDataHierarchy::ValueComparisonFunction& valueComparisonFunction)
    {
        m_valueComparisonFunction = valueComparisonFunction;
    }

    void ReflectedPropertyEditor::ClearInstances()
    {
        SaveExpansion();
        ReturnAllToPool();
        m_instances.clear();
        m_selectedRow = nullptr;
    }

    PropertyRowWidget* ReflectedPropertyEditor::GetOrCreateLogicalGroupWidget(InstanceDataNode* node, PropertyRowWidget* parent, int depth)
    {
        // Locate the logical group closest to this node in the hierarchy, if one exists.
        // This will be the most recently-encountered "Group" ClassElement prior to our DataElement.

        if (node && node->GetParent() && node->GetParent()->GetClassMetadata())
        {
            const AZ::Edit::ElementData* groupElementData = node->GetGroupElementMetadata();
            // if the node is in a group then create the widget for the group
            if (groupElementData)
            {
                const char* groupName = groupElementData->m_description;
                const AZ::Crc32 groupCrc(groupName);

                PropertyRowWidget*& widgetEntry = m_groupWidgets[groupCrc];

                // Create the group's widget if we haven't already.
                if (!widgetEntry)
                {
                    widgetEntry = CreateOrPullFromPool();
                    widgetEntry->Initialize(groupName, parent, depth, m_propertyLabelWidth);
                    widgetEntry->setObjectName(groupName);

                    for (const AZ::Edit::AttributePair& attribute : groupElementData->m_attributes)
                    {
                        PropertyAttributeReader reader(node->GetParent()->FirstInstance(), attribute.second);
                        QString descriptionOut;
                        bool foundDescription = false;
                        widgetEntry->ConsumeAttribute(attribute.first, reader, true, &descriptionOut, &foundDescription);
                        if (foundDescription)
                        {
                            widgetEntry->SetDescription(descriptionOut);
                        }
                    }

                    if (parent)
                    {
                        parent->AddedChild(widgetEntry);
                    }

                    m_widgetsInDisplayOrder.push_back(widgetEntry);
                }

                // If we don't have a saved state and we are set to auto-expand, then expand
                // OR if we have a saved state and it's true, then also expand
                widgetEntry->SetExpanded(ShouldRowAutoExpand(widgetEntry));

                return widgetEntry;
            }
        }

        return nullptr;
    }

    size_t ReflectedPropertyEditor::CountRowsInAllDescendents(PropertyRowWidget* pParent)
    {
        // Recursively sum the number of property row widgets beneath the given parent
        auto& children = pParent->GetChildrenRows();
        size_t childCount = children.size();

        size_t result = childCount;

        for (size_t i = 0; i < childCount; ++i)
        {
            result += CountRowsInAllDescendents(children[i]);
        }

        return result;
    }

    void ReflectedPropertyEditor::AddProperty(InstanceDataNode* node, PropertyRowWidget* pParent, int depth)
    {
        // Removal markers should not be displayed in the property grid.
        if (node->IsRemovedVersusComparison())
        {
            return;
        }

        // Evaluate editor reflection and visibility attributes for the node.
        const NodeDisplayVisibility visibility = CalculateNodeDisplayVisibility(*node);
        if (visibility == NodeDisplayVisibility::NotVisible)
        {
            return;
        }

        setUpdatesEnabled(false);

        PropertyRowWidget* pWidget = nullptr;
        if (visibility == NodeDisplayVisibility::Visible)
        {
            auto widgetDisplayOrder = m_widgetsInDisplayOrder.end();

            // Handle anchoring to logical groups defined by ClassElement(AZ::Edit::ClassElements::Group,...).
            PropertyRowWidget* groupWidget = GetOrCreateLogicalGroupWidget(node, pParent, depth);
            if (groupWidget)
            {
                groupWidget->show();
                pParent = groupWidget;
                depth = groupWidget->GetDepth() + 1;

                // Insert this node's widget after all existing properties within the group.
                for (auto iter = m_widgetsInDisplayOrder.begin(); iter != m_widgetsInDisplayOrder.end(); ++iter)
                {
                    if (*iter == groupWidget)
                    {
                        widgetDisplayOrder = iter;

                        // We have to allow for containers in the group so we add the total number of descendant rows
                        AZStd::advance(widgetDisplayOrder, CountRowsInAllDescendents(groupWidget) + 1);
                    }
                }
            }

            pWidget = CreateOrPullFromPool();
            if (!pParent)
            {
                pWidget->show();
            }
            pWidget->Initialize(pParent, node, depth, m_propertyLabelWidth);
            pWidget->setObjectName(pWidget->label());
            pWidget->SetSelectionEnabled(m_selectionEnabled);

            pWidget->setProperty("Root", pParent == nullptr);

            m_widgets[node] = pWidget;
            m_widgetsInDisplayOrder.insert(widgetDisplayOrder, pWidget);

            if (pParent)
            {
                pParent->AddedChild(pWidget);
            }

            pParent = pWidget;
            depth += 1;
        }

        for (auto& childNode : node->GetChildren())
        {
            AddProperty(&childNode, pParent, depth);
        }

        if (pWidget)
        {
            // If this row is at the root it will not have a parent to expand it so
            //      the edit field will never be initialized, therefore do it here.
            if (!pWidget->GetParentRow() && !pWidget->HasChildWidgetAlready())
            {
                PropertyHandlerBase* pHandler = pWidget->GetHandler();
                if (pHandler)
                {
                    QWidget* rootWidget = pHandler->CreateGUI(pWidget);
                    if (rootWidget)
                    {
                        m_userWidgetsToData[rootWidget] = node;
                        pHandler->ConsumeAttributes_Internal(rootWidget, node);
                        pHandler->ReadValuesIntoGUI_Internal(rootWidget, node);
                        pWidget->ConsumeChildWidget(rootWidget);
                        pWidget->OnValuesUpdated();
                    }
                }
            }

            // Auto-expand only if no saved expand state and we are set to auto-expand
            pWidget->SetExpanded(ShouldRowAutoExpand(pWidget));
            if (pWidget->IsExpanded())
            {
                for (auto pChain = pWidget->GetParentRow(); pChain; pChain = pChain->GetParentRow())
                {
                    pChain->SetExpanded(true);
                }
            }

            if (!pWidget->GetParentRow() && m_hideRootProperties)
            {
                pWidget->HideContent();
            }

            pWidget->SetReadOnly(m_readOnly);
        }

        setUpdatesEnabled(true);
    }

    /// Must call after Add/Remove instance for the change to be applied
    void ReflectedPropertyEditor::InvalidateAll()
    {
        setUpdatesEnabled(false);

        m_selectedRow = nullptr;
        if (m_ptrNotify && m_selectedRow)
        {
            m_ptrNotify->PropertySelectionChanged(GetNodeFromWidget(m_selectedRow), false);
        }

        ReturnAllToPool();
        ++m_expansionDepth;

        for (auto& instance : m_instances)
        {
            instance.Build(m_context, AZ::SerializeContext::ENUM_ACCESS_FOR_READ);
            AddProperty(instance.GetRootNode(), NULL, 0);
        }

        for (PropertyRowWidget* widget : m_widgetsInDisplayOrder)
        {
            widget->RefreshStyle();
            m_containerWidget->layout()->addWidget(widget);
        }

        --m_expansionDepth;
        if (m_expansionDepth == 0)
        {
            if (m_mainScrollArea)
            {
                // if we're responsible for our own scrolling, then add the spacer.
                if (!m_spacer)
                {
                    m_spacer = aznew QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
                }
                else
                {
                    m_containerWidget->layout()->removeItem(m_spacer);
                }

                m_containerWidget->layout()->addItem(m_spacer);
            }

            layout()->setEnabled(true);
            layout()->update();
            layout()->activate();
            emit OnExpansionContractionDone();
        }

        // Active property editors should all support transient state saving for the current session, at a minimum.
        // A key must still be manually provided for persistent saving across sessions.
        if (0 == m_savedStateKey)
        {
            if (!m_instances.empty() && m_instances.begin()->GetRootNode())
            {
                // Based on instance type name; persists when editing any object of this type.
                SetSavedStateKey(AZ::Crc32(m_instances.begin()->GetRootNode()->GetClassMetadata()->m_name));
            }
            else
            {
                // Random key; valid for lifetime of control instance.
                SetSavedStateKey(AZ::Sfmt().Rand32());
            }
        }

        SaveExpansion();

        m_queuedRefreshLevel = Refresh_None;

        AdjustLabelWidth();

        setUpdatesEnabled(true);
    }

    void ReflectedPropertyEditor::AdjustLabelWidth()
    {
        if (m_autoResizeLabels)
        {
            SetLabelWidth(GetMaxLabelWidth() + 10);
        }
    }

    void ReflectedPropertyEditor::InvalidateAttributesAndValues()
    {
        for (InstanceDataHierarchy& instance : m_instances)
        {
            instance.RefreshComparisonData(AZ::SerializeContext::ENUM_ACCESS_FOR_READ);
        }

        for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it)
        {
            PropertyRowWidget* pWidget = it->second;

            QWidget* childWidget = pWidget->GetChildWidget();

            if (pWidget->GetHandler() && childWidget)
            {
                pWidget->GetHandler()->ConsumeAttributes_Internal(childWidget, it->first);
                pWidget->GetHandler()->ReadValuesIntoGUI_Internal(childWidget, it->first);
                pWidget->OnValuesUpdated();
            }
            pWidget->RefreshAttributesFromNode(false);
        }

        m_queuedRefreshLevel = Refresh_None;
    }

    void ReflectedPropertyEditor::InvalidateValues()
    {
        for (InstanceDataHierarchy& instance : m_instances)
        {
            instance.RefreshComparisonData(AZ::SerializeContext::ENUM_ACCESS_FOR_READ);
        }

        for (auto it = m_userWidgetsToData.begin(); it != m_userWidgetsToData.end(); ++it)
        {
            auto rowWidget = m_widgets.find(it->second);

            if (rowWidget != m_widgets.end())
            {
                PropertyRowWidget* pWidget = rowWidget->second;
                if (pWidget->GetHandler())
                {
                    pWidget->GetHandler()->ReadValuesIntoGUI_Internal(it->first, rowWidget->first);
                    pWidget->OnValuesUpdated();
                }
            }
        }

        m_queuedRefreshLevel = Refresh_None;
    }

    PropertyRowWidget* ReflectedPropertyEditor::CreateOrPullFromPool()
    {
        PropertyRowWidget* newWidget = NULL;
        if (m_widgetPool.empty())
        {
            newWidget = aznew PropertyRowWidget(m_containerWidget);
            connect(newWidget, &PropertyRowWidget::onRequestedContainerClear,
                [=](InstanceDataNode* node)
            {
                this->OnPropertyRowRequestClear(newWidget, node);
            }
            );
            connect(newWidget, &PropertyRowWidget::onRequestedContainerElementRemove,
                [=](InstanceDataNode* node)
            {
                this->OnPropertyRowRequestContainerRemoveItem(newWidget, node);
            }
            );

            connect(newWidget, &PropertyRowWidget::onRequestedContainerAdd,
                [=](InstanceDataNode* node)
            {
                this->OnPropertyRowRequestContainerAddItem(newWidget, node);
            }
            );

            connect(newWidget, &PropertyRowWidget::onExpandedOrContracted,
                [=](InstanceDataNode* node, bool expanded, bool fromUserInteraction)
            {
                this->OnPropertyRowExpandedOrContracted(newWidget, node, expanded, fromUserInteraction);
            }
            );

            connect(newWidget, &PropertyRowWidget::onRequestedContextMenu, this,
                [=](InstanceDataNode* node, const QPoint& point)
            {
                if (m_ptrNotify)
                {
                    PropertyRowWidget* widget = GetWidgetFromNode(node);
                    if (widget)
                    {
                        m_ptrNotify->RequestPropertyContextMenu(node, point);
                    }
                }
            }
            );

            connect(newWidget, &PropertyRowWidget::onRequestedSelection, this, &ReflectedPropertyEditor::SelectInstance);
        }
        else
        {
            newWidget = m_widgetPool.back();
            m_widgetPool.erase(m_widgetPool.begin() + m_widgetPool.size() - 1);
        }
        return newWidget;
    }

    void ReflectedPropertyEditor::ReturnAllToPool()
    {
        layout()->setEnabled(false);
        for (auto& widget : m_widgets)
        {
            widget.second->hide();
            widget.second->Clear();
            m_containerWidget->layout()->removeWidget(widget.second);
            m_widgetPool.push_back(widget.second);
        }

        for (auto& widget : m_groupWidgets)
        {
            widget.second->hide();
            widget.second->Clear();
            m_containerWidget->layout()->removeWidget(widget.second);
            m_widgetPool.push_back(widget.second);
        }

        m_userWidgetsToData.clear();
        m_widgets.clear();
        m_widgetsInDisplayOrder.clear();
        m_groupWidgets.clear();
    }

    void ReflectedPropertyEditor::OnPropertyRowExpandedOrContracted(PropertyRowWidget* widget, InstanceDataNode* /*node*/, bool expanded, bool fromUserInteraction)
    {
        setUpdatesEnabled(false);

        // save the expansion state - only if its from the user actually clicking:
        if (m_expansionDepth == 0)
        {
            layout()->setEnabled(false);
        }
        ++m_expansionDepth;

        // Create a saved state if a user interaction occurred and there is no existing saved state and we have a saved state key to use
        if (m_savedState && fromUserInteraction)
        {
            m_savedState->SetExpandedState(CreatePathKey(widget), expanded);
        }

        // we need to walk its children and expand them, too...
        for (auto widgetForChild : widget->GetChildrenRows())
        {
            if (expanded)
            {
                // expand all the children, show them in whatever state they're already in:
                widgetForChild->show();
                widgetForChild->SetExpanded(ShouldRowAutoExpand(widgetForChild));
                // might want to manufacture the inner gui here!

                if (!widgetForChild->HasChildWidgetAlready())
                {
                    PropertyHandlerBase* pHandler = widgetForChild->GetHandler();
                    if (pHandler)
                    {
                        // create widget gui here.
                        QWidget* newChildWidget = pHandler->CreateGUI(widgetForChild);
                        if (newChildWidget)
                        {
                            m_userWidgetsToData[newChildWidget] = widgetForChild->GetNode();
                            pHandler->ConsumeAttributes_Internal(newChildWidget, widgetForChild->GetNode());
                            pHandler->ReadValuesIntoGUI_Internal(newChildWidget, widgetForChild->GetNode());
                            widgetForChild->ConsumeChildWidget(newChildWidget);
                            widgetForChild->OnValuesUpdated();

                            if (!m_queuedTabOrderRefresh)
                            {
                                QTimer::singleShot(0, this, SLOT(RecreateTabOrder()));
                            }
                            m_queuedTabOrderRefresh = true;
                        }
                    }
                }
            }
            else
            {
                OnPropertyRowExpandedOrContracted(widgetForChild, widgetForChild->GetNode(), false, false);

                // contract all children!
                widgetForChild->hide();
            }

            widgetForChild->SetReadOnly(m_readOnly);
        }

        --m_expansionDepth;
        if (m_expansionDepth == 0)
        {
            layout()->setEnabled(true);
            layout()->update();
            layout()->activate();
            emit OnExpansionContractionDone();

            AdjustLabelWidth();
        }

        setUpdatesEnabled(true);
    }

    void ReflectedPropertyEditor::RecreateTabOrder()
    {
        // re-create the tab order, based on vertical position in the list.

        QWidget* pLastWidget = NULL;

        for (AZStd::size_t pos = 0; pos < m_widgetsInDisplayOrder.size(); ++pos)
        {
            if (pLastWidget)
            {
                QWidget* pFirst = m_widgetsInDisplayOrder[pos]->GetFirstTabWidget();
                setTabOrder(pLastWidget, pFirst);
                m_widgetsInDisplayOrder[pos]->UpdateWidgetInternalTabbing();
            }

            pLastWidget = m_widgetsInDisplayOrder[pos]->GetLastTabWidget();
        }
        m_queuedTabOrderRefresh = false;
    }

    void ReflectedPropertyEditor::SetSavedStateKey(AZ::u32 key)
    {
        if (m_savedStateKey != key)
        {
            m_savedStateKey = key;
            m_savedState = key ? AZ::UserSettings::CreateFind<ReflectedPropertyEditorState>(m_savedStateKey, AZ::UserSettings::CT_GLOBAL) : nullptr;
        }
    }

    bool ReflectedPropertyEditor::CheckSavedExpandState(AZ::u32 pathKey) const
    {
        return m_savedState ? m_savedState->GetExpandedState(pathKey) : false;
    }

    bool ReflectedPropertyEditor::HasSavedExpandState(AZ::u32 pathKey) const
    {
        // We are only considered to have a saved expanded state if we are a saved element
        return m_savedState ? m_savedState->HasExpandedState(pathKey) : false;
    }

    // given a widget, create a key which includes its parent(s)
    AZ::u32 ReflectedPropertyEditor::CreatePathKey(PropertyRowWidget* widget) const
    {
        if (widget)
        {
            AZ::Crc32 crc = AZ::Crc32(widget->GetIdentifier());

            auto parentWidget = widget->GetParentRow();
            if (parentWidget != nullptr)
            {
                AZ::u32 parentCRC = CreatePathKey(parentWidget);
                crc.Add(&parentCRC, sizeof(AZ::u32), false);
            }
            return (AZ::u32)crc;
        }

        return 0;
    }

    void ReflectedPropertyEditor::SaveExpansion()
    {
        if (m_savedState)
        {
            for (const auto& widgetPair : m_widgets )
            {
                auto widget = widgetPair.second;
                m_savedState->SetExpandedState(CreatePathKey(widget), widget->IsExpanded());
            }
        }
    }

    bool ReflectedPropertyEditor::ShouldRowAutoExpand(PropertyRowWidget* widget) const
    {
        auto parent = widget->GetParentRow();
        if (!parent && m_hideRootProperties)
        {
            return true;
        }

        if (widget->IsForbidExpansion())
        {
            return false;
        }

        const auto& key = CreatePathKey(widget);
        if (HasSavedExpandState(key))
        {
            return CheckSavedExpandState(key);
        }

        return widget->AutoExpand();
    }

    void ReflectedPropertyEditor::RequestWrite(QWidget* editorGUI)
    {
        auto it = m_userWidgetsToData.find(editorGUI);
        if (it == m_userWidgetsToData.end())
        {
            return;
        }

        // get the property editor
        auto rowWidget = m_widgets.find(it->second);
        if (rowWidget != m_widgets.end())
        {
            PropertyHandlerBase* handler = rowWidget->second->GetHandler();
            if (handler)
            {
                if (m_ptrNotify)
                {
                    m_ptrNotify->BeforePropertyModified(rowWidget->first);
                }

                handler->WriteGUIValuesIntoProperty_Internal(editorGUI, rowWidget->first);

                // once we've written our values, we need to potentially callback:
                PropertyModificationRefreshLevel level = rowWidget->second->DoPropertyNotify();

                if (m_ptrNotify)
                {
                    m_ptrNotify->AfterPropertyModified(rowWidget->first);
                }

                if (level < Refresh_Values)
                {
                    for (InstanceDataHierarchy& instance : m_instances)
                    {
                        instance.RefreshComparisonData(AZ::SerializeContext::ENUM_ACCESS_FOR_READ);
                        rowWidget->second->OnValuesUpdated();
                    }
                }

                QueueInvalidation(level);
            }
        }
    }

    void ReflectedPropertyEditor::RequestRefresh(PropertyModificationRefreshLevel level)
    {
        QueueInvalidation(level);
    }

    void ReflectedPropertyEditor::RequestPropertyNotify(QWidget* editorGUI)
    {
        auto it = m_userWidgetsToData.find(editorGUI);
        if (it == m_userWidgetsToData.end())
        {
            return;
        }

        auto rowWidget = m_widgets.find(it->second);
        if (rowWidget != m_widgets.end())
        {
            // once we've written our values, we need to potentially callback:
            PropertyModificationRefreshLevel level = rowWidget->second->DoPropertyNotify();

            if (level < Refresh_Values)
            {
                for (InstanceDataHierarchy& instance : m_instances)
                {
                    instance.RefreshComparisonData(AZ::SerializeContext::ENUM_ACCESS_FOR_READ);
                    rowWidget->second->OnValuesUpdated();
                }
            }

            QueueInvalidation(level);
        }
    }

    void ReflectedPropertyEditor::OnEditingFinished(QWidget* editorGUI)
    {
        auto it = m_userWidgetsToData.find(editorGUI);
        if (it == m_userWidgetsToData.end())
        {
            return;
        }

        // get the property editor
        auto rowWidget = m_widgets.find(it->second);
        if (rowWidget != m_widgets.end())
        {
            PropertyHandlerBase* handler = rowWidget->second->GetHandler();
            if (handler)
            {
                if (m_ptrNotify)
                {
                    m_ptrNotify->SetPropertyEditingComplete(rowWidget->first);
                }
            }
        }
    }

    void ReflectedPropertyEditor::OnPropertyRowRequestClear(PropertyRowWidget* /*widget*/, InstanceDataNode* node)
    {
        // get the property editor
        if (m_ptrNotify)
        {
            m_ptrNotify->BeforePropertyModified(node);
        }

        AZ::SerializeContext::IDataContainer* container = node->GetClassMetadata()->m_container;
        AZ_Assert(container->IsFixedSize() == false || container->IsSmartPointer(), "We clear elements in static containers");

        // clear all elements
        for (size_t instanceIndex = 0; instanceIndex < node->GetNumInstances(); ++instanceIndex)
        {
            container->ClearElements(node->GetInstance(instanceIndex), node->GetSerializeContext());
        }

        if (m_ptrNotify)
        {
            m_ptrNotify->AfterPropertyModified(node);
            m_ptrNotify->SealUndoStack();
        }

        QueueInvalidation(Refresh_EntireTree);
    }

    void ReflectedPropertyEditor::OnPropertyRowRequestContainerRemoveItem(PropertyRowWidget* /*widget*/, InstanceDataNode* node)
    {
        // Locate the owning container. There may be a level of indirection due to wrappers, such as DynamicSerializableField.
        InstanceDataNode* pContainerNode = node->GetParent();
        while (pContainerNode && !pContainerNode->GetClassMetadata()->m_container)
        {
            pContainerNode = pContainerNode->GetParent();
        }

        AZ_Assert(pContainerNode, "Failed to locate parent container for element \"%s\" of type %s.",
            node->GetElementMetadata() ? node->GetElementMetadata()->m_name : node->GetClassMetadata()->m_name,
            node->GetClassMetadata()->m_typeId.ToString<AZStd::string>().c_str());

        // remove it from all containers:
        if (m_ptrNotify)
        {
            m_ptrNotify->BeforePropertyModified(pContainerNode);
        }

        void* elementPtr = (node->GetElementMetadata()->m_flags & AZ::SerializeContext::ClassElement::FLG_POINTER) ? node->GetInstanceAddress(0) : node->FirstInstance();


        AZ::SerializeContext::IDataContainer* container = pContainerNode->GetClassMetadata()->m_container;
        AZ_Assert(container->IsFixedSize() == false ||
            container->IsSmartPointer(), "We can't remove elements from a fixed size container!");

        // pass the context as the last parameter to actually delete the related data.
        for (AZStd::size_t instanceIndex = 0; instanceIndex < pContainerNode->GetNumInstances(); ++instanceIndex)
        {
            container->RemoveElement(pContainerNode->GetInstance(instanceIndex), elementPtr, pContainerNode->GetSerializeContext());
        }

        if (m_ptrNotify)
        {
            m_ptrNotify->AfterPropertyModified(pContainerNode);
            m_ptrNotify->SealUndoStack();
        }

        for (const AZ::Edit::AttributePair& attribute : pContainerNode->GetElementEditMetadata()->m_attributes)
        {
            if (attribute.first == AZ_CRC("RemoveNotify", 0x16ec95f5))
            {
                AZ::Edit::AttributeFunction<void()>* func = azdynamic_cast<AZ::Edit::AttributeFunction<void()>*>(attribute.second);
                if (func)
                {
                    InstanceDataNode* pParent = pContainerNode->GetParent();
                    if (pParent)
                    {
                        for (size_t idx = 0; idx < pParent->GetNumInstances(); ++idx)
                        {
                            func->Invoke(pParent->GetInstance(idx));
                        }
                    }
                }
            }
        }

        QueueInvalidation(Refresh_EntireTree);
    }

    void ReflectedPropertyEditor::OnPropertyRowRequestContainerAddItem(PropertyRowWidget* widget, InstanceDataNode* pContainerNode)
    {
        // Do expansion before modifying container as container modifications will invalidate and disallow the expansion until a later queued refresh
        OnPropertyRowExpandedOrContracted(widget, pContainerNode, true, true);

        // Locate the owning container. There may be a level of indirection due to wrappers, such as DynamicSerializableField.
        while (pContainerNode && !pContainerNode->GetClassMetadata()->m_container)
        {
            pContainerNode = pContainerNode->GetParent();
        }

        AZ_Assert(pContainerNode && pContainerNode->GetClassMetadata()->m_container, "Failed to locate container node for element \"%s\" of type %s.",
            pContainerNode->GetElementMetadata() ? pContainerNode->GetElementMetadata()->m_name : pContainerNode->GetClassMetadata()->m_name,
            pContainerNode->GetClassMetadata()->m_typeId.ToString<AZStd::string>().c_str());

        AZ::SerializeContext::IDataContainer* container = pContainerNode->GetClassMetadata()->m_container;

        //If the container is at capacity, we do not want to add another item. 
        if (container->IsFixedCapacity() && !container->IsSmartPointer())
        {
            if (container->Size(pContainerNode) == container->Capacity(pContainerNode))
            {
                return;
            }
        }

        if (m_ptrNotify)
        {
            m_ptrNotify->BeforePropertyModified(pContainerNode);
        }
        // add element
        //data.m_dataNode->WriteStart();

        AZ_Assert(container->IsFixedSize() == false || container->IsSmartPointer(), "We can't add elements to static containers");

        pContainerNode->CreateContainerElement(
            [this, widget](const AZ::Uuid& classId, const AZ::Uuid& typeId, AZ::SerializeContext* context) -> const AZ::SerializeContext::ClassData*
        {
            AZStd::vector<const AZ::SerializeContext::ClassData*> derivedClasses;
            context->EnumerateDerived(
                    [this, &derivedClasses, widget]
                    (const AZ::SerializeContext::ClassData* classData, const AZ::Uuid& /*knownType*/) -> bool
                    {
                        derivedClasses.push_back(classData);
                        return true;
                    },
                    classId,
                    typeId
                );

            if (derivedClasses.empty())
            {
                const AZ::SerializeContext::ClassData* classData = context->FindClassData(typeId);
                const char* className = classData ?
                    (classData->m_editData ? classData->m_editData->m_name : classData->m_name)
                    : "<unknown>";

                QMessageBox mb(QMessageBox::Information,
                    "Select Class",
                    QString("No classes could be found that derive from \"%1\".").arg(className),
                    QMessageBox::Ok);
                mb.exec();
                return nullptr;
            }

            QStringList items;
            for (size_t i = 0; i < derivedClasses.size(); ++i)
            {
                const char* className = derivedClasses[i]->m_editData ? derivedClasses[i]->m_editData->m_name : derivedClasses[i]->m_name;
                items.push_back(className);
            }

            bool ok;
            QString item = QInputDialog::getItem(nullptr, tr("Class to create"), tr("Classes"), items, 0, false, &ok);
            if (!ok)
            {
                return nullptr;
            }

            for (int i = 0; i < items.size(); ++i)
            {
                if (items[i] == item)
                {
                    return derivedClasses[i];
                }
            }

            return nullptr;
        },
            [this](void* dataPtr, const AZ::SerializeContext::ClassElement* classElement, bool noDefaultData, AZ::SerializeContext*) -> bool
        {
            (void)noDefaultData;

#define HANDLE_NUMERIC(Type, defaultValue)                  \
    if (classElement->m_typeId == azrtti_typeid<Type>()) {  \
        *reinterpret_cast<Type*>(dataPtr) = defaultValue;   \
    }

            // In the case of primitive numbers, initialize to 0.
            HANDLE_NUMERIC(double, 0.0)
    else HANDLE_NUMERIC(float, 0.0f)
        else HANDLE_NUMERIC(AZ::u8, 0)
        else HANDLE_NUMERIC(char, 0)
                else HANDLE_NUMERIC(AZ::u16, 0)
                else HANDLE_NUMERIC(AZ::s16, 0)
                else HANDLE_NUMERIC(AZ::u32, 0)
                else HANDLE_NUMERIC(AZ::s32, 0)
                else HANDLE_NUMERIC(AZ::u64, 0)
                else HANDLE_NUMERIC(AZ::s64, 0)
                else HANDLE_NUMERIC(bool, false)
                else
                {
                // copy default data from provided attribute or pop a dialog
                // if "noDefaultData" is set, this means the container requires valid data (like hash_tables, need a key so they can push the element).
                AZ_Warning("PropertyManager", !noDefaultData, "Support for adding elements to this type of container via the property editor is not yet implemented.");
                return false;
                }

#undef HANDLE_NUMERIC

                return true;
        }
        );

        // Fire any add notifications for the container widget.
        for (const AZ::Edit::AttributePair& attribute : pContainerNode->GetElementEditMetadata()->m_attributes)
        {
            if (attribute.first == AZ::Edit::Attributes::AddNotify)
            {
                AZ::Edit::AttributeFunction<void()>* func = azdynamic_cast<AZ::Edit::AttributeFunction<void()>*>(attribute.second);
                if (func)
                {
                    InstanceDataNode* pParent = pContainerNode->GetParent();
                    if (pParent)
                    {
                        for (size_t idx = 0; idx < pParent->GetNumInstances(); ++idx)
                        {
                            func->Invoke(pParent->GetInstance(idx));
                        }
                    }
                }
            }
        }

        // Fire general change notifications for the container widget.
        if (widget)
        {
            widget->DoPropertyNotify();
        }

        // Only seal the undo stack once all modifications have been completed
        if (m_ptrNotify)
        {
            m_ptrNotify->AfterPropertyModified(pContainerNode);
            m_ptrNotify->SealUndoStack();
        }

        QueueInvalidation(Refresh_EntireTree);
    }

    void ReflectedPropertyEditor::SetAutoResizeLabels(bool autoResizeLabels)
    {
        m_autoResizeLabels = autoResizeLabels;
    }

    void ReflectedPropertyEditor::QueueInvalidation(PropertyModificationRefreshLevel level)
    {
        if ((int)level > m_queuedRefreshLevel)
        {
            // the callback told us that we need to do something more drastic than we're already scheduled to do (which might be nothing)
            bool rerequest = (m_queuedRefreshLevel == Refresh_None); // if we haven't scheduled a refresh, then we will schedule one.
            m_queuedRefreshLevel = level;
            if (rerequest)
            {
                QTimer::singleShot(0, this, SLOT(DoRefresh()));
            }
        }
    }

    void ReflectedPropertyEditor::DoRefresh()
    {
        if (m_queuedRefreshLevel == Refresh_None)
        {
            return;
        }

        setUpdatesEnabled(false);

        // we've been asked to refresh the gui.
        switch (m_queuedRefreshLevel)
        {
        case Refresh_Values:
            InvalidateValues();
            break;
        case Refresh_AttributesAndValues:
            InvalidateAttributesAndValues();
            break;
        case Refresh_EntireTree:
        case Refresh_EntireTree_NewContent:
            InvalidateAll();
            break;
        }

        m_queuedRefreshLevel = Refresh_None;

        setUpdatesEnabled(true);
    }

    void ReflectedPropertyEditor::paintEvent(QPaintEvent* event)
    {
        QWidget::paintEvent(event);
    }

    void ReflectPropertyEditor(AZ::ReflectContext* context)
    {
        ReflectedPropertyEditorState::Reflect(context);
    }

    InstanceDataNode* ReflectedPropertyEditor::GetNodeFromWidget(QWidget* pTarget) const
    {
        PropertyRowWidget* pRow = nullptr;
        while ((pTarget != nullptr) && (pTarget != this))
        {
            if ((pRow = qobject_cast<AzToolsFramework::PropertyRowWidget*>(pTarget)) != nullptr)
            {
                break;
            }
            pTarget = pTarget->parentWidget();
        }

        if (pRow)
        {
            return pRow->GetNode();
        }

        return nullptr;
    }

    PropertyRowWidget* ReflectedPropertyEditor::GetWidgetFromNode(InstanceDataNode* node) const
    {
        auto widgetIter = m_widgets.find(node);
        if (widgetIter != m_widgets.end())
        {
            return widgetIter->second;
        }

        return nullptr;
    }

    void ReflectedPropertyEditor::ExpandAll()
    {
        for (const auto& widget : m_widgets)
        {
            widget.second->SetExpanded(!widget.second->IsForbidExpansion());
        }

        for (const auto& widget : m_groupWidgets)
        {
            widget.second->SetExpanded(!widget.second->IsForbidExpansion());
        }
    }
    void ReflectedPropertyEditor::CollapseAll()
    {
        for (const auto& widget : m_widgets)
        {
            widget.second->SetExpanded(false);
        }

        for (const auto& widget : m_groupWidgets)
        {
            if (widget.second->GetParentRow() == nullptr)
            {
                widget.second->SetExpanded(false);
            }
        }
    }

    int ReflectedPropertyEditor::GetContentHeight() const
    {
        return m_containerWidget->layout()->sizeHint().height();
    }

    int ReflectedPropertyEditor::GetMaxLabelWidth() const
    {
        int width = 0;
        for (auto widget : m_widgetsInDisplayOrder)
        {
            width = std::max(width, widget->LabelSizeHint().width());
        }
        return width;
    }


    void ReflectedPropertyEditor::SetLabelWidth(int labelWidth)
    {
        m_propertyLabelWidth = labelWidth;
        for (auto widget : m_widgetsInDisplayOrder)
        {
            widget->SetLabelWidth(labelWidth);
        }
    }


    void ReflectedPropertyEditor::SetSelectionEnabled(bool selectionEnabled)
    {
        m_selectionEnabled = selectionEnabled;
        for (auto widget : m_widgetsInDisplayOrder)
        {
            widget->SetSelectionEnabled(selectionEnabled);
        }
        if (!m_selectionEnabled)
        {
            m_selectedRow = nullptr;
        }
    }

    void ReflectedPropertyEditor::SelectInstance(InstanceDataNode* node)
    {
        PropertyRowWidget* widget = GetWidgetFromNode(node);
        PropertyRowWidget *deselected = m_selectedRow;
        if (deselected)
        {
            deselected->SetSelected(false);
            m_selectedRow = nullptr;
            if (m_ptrNotify)
            {
                InstanceDataNode *prevNode = GetNodeFromWidget(deselected);
                m_ptrNotify->PropertySelectionChanged(prevNode, false);
            }
        }
        //if we selected a new row
        if (widget && widget != deselected)
        {
            m_selectedRow = widget;
            m_selectedRow->SetSelected(true);
            if (m_ptrNotify)
            {
                m_ptrNotify->PropertySelectionChanged(node, true);
            }
        }
    }

    AzToolsFramework::InstanceDataNode* ReflectedPropertyEditor::GetSelectedInstance() const
    {
        return GetNodeFromWidget(m_selectedRow);
    }

    QSize ReflectedPropertyEditor::sizeHint() const
    {
        return m_containerWidget->sizeHint() + QSize(5,5);
    }
}

#include <UI/PropertyEditor/ReflectedPropertyEditor.moc>
#include <UI/PropertyEditor/Resources/rcc_Icons.h>
