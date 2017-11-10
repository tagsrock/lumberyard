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
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>

#include <QtWidgets/QWidget>

#include "PropertyAudioCtrlTypes.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QHBoxLayout;


namespace AzToolsFramework
{
    //=============================================================================
    // Audio Control Selector Widget
    //=============================================================================
    class AudioControlSelectorWidget
        : public QWidget
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR(AudioControlSelectorWidget, AZ::SystemAllocator, 0);
        AudioControlSelectorWidget(QWidget* parent = nullptr);

        void SetControlName(const QString& controlName);
        QString GetControlName() const;
        void SetPropertyType(AudioPropertyType type);
        AudioPropertyType GetPropertyType() const
        {
            return m_propertyType;
        }
        void SetEnableEdit(bool enabled);

        QWidget* GetFirstInTabOrder();
        QWidget* GetLastInTabOrder();
        void UpdateTabOrder();

        // todo: enable drag-n-drop from Audio Controls Editor
        bool eventFilter(QObject* obj, QEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragLeaveEvent(QDragLeaveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    signals:
        void ControlNameChanged(const QString& path);

        public slots:
        void OnClearControl();
        void OnOpenAudioControlSelector();

    protected:
        bool IsCorrectMimeData(const QMimeData* pData) const;
        void focusInEvent(QFocusEvent* event) override;

        QLineEdit* m_controlEdit;
        QPushButton* m_browseButton;
        QPushButton* m_clearButton;
        QHBoxLayout* m_mainLayout;

        AudioPropertyType m_propertyType;
        QString m_controlName;

    private:
        void UpdateWidget();
        static AZStd::string GetResourceSelectorNameFromType(AudioPropertyType propertyType);
    };


    //=============================================================================
    // Property Handler
    //=============================================================================
    class AudioControlSelectorWidgetHandler
        : QObject
        , public AzToolsFramework::PropertyHandler<CReflectedVarAudioControl, AudioControlSelectorWidget>
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR(AudioControlSelectorWidgetHandler, AZ::SystemAllocator, 0);

        AZ::u32 GetHandlerName() const override
        {
            return AZ_CRC("AudioControl", 0x16e7ca6e);
        }

        bool IsDefaultHandler() const override
        {
            return true;
        }

        QWidget* GetFirstInTabOrder(widget_t* widget) override
        {
            return widget->GetFirstInTabOrder();
        }

        QWidget* GetLastInTabOrder(widget_t* widget) override
        {
            return widget->GetLastInTabOrder();
        }

        void UpdateWidgetInternalTabbing(widget_t* widget) override
        {
            return widget->UpdateTabOrder();
        }

        QWidget* CreateGUI(QWidget* parent) override;
        void ConsumeAttribute(widget_t* gui, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
        void WriteGUIValuesIntoProperty(size_t index, widget_t* gui, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
        bool ReadValuesIntoGUI(size_t index, widget_t* gui, const property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
    };


    void RegisterAudioPropertyHandler();

} // namespace AzToolsFramework
