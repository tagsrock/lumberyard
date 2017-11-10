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
#include "PropertyButtonCtrl.hxx"
#include "PropertyQTConstants.h"
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>

namespace AzToolsFramework
{
    PropertyButtonCtrl::PropertyButtonCtrl(QWidget* pParent)
        : QWidget(pParent)
    {
        QHBoxLayout* pLayout = new QHBoxLayout(this);
        pLayout->setAlignment(Qt::AlignLeft);

        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_button = new QPushButton(this);
        m_button->setSizePolicy(sizePolicy);

        setLayout(pLayout);

        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->addWidget(m_button);

        this->setFocusProxy(m_button);
        setFocusPolicy(m_button->focusPolicy());

        connect(m_button, &QPushButton::released, this, [this]() { Q_EMIT buttonPressed(); });
    }

    PropertyButtonCtrl::~PropertyButtonCtrl()
    {
    }

    void PropertyButtonCtrl::SetButtonText(const char* text)
    {
        m_button->setText(QString::fromUtf8(text));
    }

    QString PropertyButtonCtrl::GetButtonText() const
    {
        return m_button->text();
    }

    QWidget* ButtonHandlerCommon::CreateGUICommon(QWidget* pParent)
    {
        PropertyButtonCtrl* newCtrl = aznew PropertyButtonCtrl(pParent);
        QWidget::connect(newCtrl, &PropertyButtonCtrl::buttonPressed, this, [newCtrl]()
            {
                PropertyEditorGUIMessages::Bus::Broadcast(&PropertyEditorGUIMessages::Bus::Events::RequestPropertyNotify, newCtrl);
            });

        return newCtrl;
    }

    void ButtonHandlerCommon::ConsumeAttributeCommon(PropertyButtonCtrl* GUI, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* /*debugName*/)
    {
        if (attrib == AZ::Edit::Attributes::ButtonText)
        {
            AZStd::string text;
            attrValue->Read<AZStd::string>(text);
            if (!text.empty())
            {
                GUI->SetButtonText(text.c_str());
            }
        }
    }
    
    QWidget* ButtonBoolHandler::CreateGUI(QWidget* pParent)
    {
        return CreateGUICommon(pParent);
    }

    void ButtonBoolHandler::ConsumeAttribute(PropertyButtonCtrl* widget, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName)
    {
        ConsumeAttributeCommon(widget, attrib, attrValue, debugName);
    }
    
    void ButtonBoolHandler::WriteGUIValuesIntoProperty(size_t /*index*/, PropertyButtonCtrl* /*GUI*/, property_t& /*instance*/, InstanceDataNode* /*node*/)
    {
    }

    bool ButtonBoolHandler::ReadValuesIntoGUI(size_t /*index*/, PropertyButtonCtrl* GUI, const property_t& /*instance*/, InstanceDataNode* node)
    {
        if (GUI->GetButtonText().isEmpty())
        {
            GUI->SetButtonText(node->GetElementEditMetadata()->m_name);
        }

        return true;
    }
        
    QWidget* ButtonStringHandler::CreateGUI(QWidget* pParent)
    {
        return CreateGUICommon(pParent);
    }

    void ButtonStringHandler::ConsumeAttribute(PropertyButtonCtrl* widget, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName)
    {
        ConsumeAttributeCommon(widget, attrib, attrValue, debugName);
    }
    
    void ButtonStringHandler::WriteGUIValuesIntoProperty(size_t /*index*/, PropertyButtonCtrl* /*GUI*/, property_t& /*instance*/, InstanceDataNode* /*node*/)
    {
    }

    bool ButtonStringHandler::ReadValuesIntoGUI(size_t /*index*/, PropertyButtonCtrl* GUI, const property_t& instance, InstanceDataNode* node)
    {
        if (!instance.empty())
        {
            GUI->SetButtonText(instance.c_str());
        }
        else
        {
            if (GUI->GetButtonText().isEmpty())
            {
                GUI->SetButtonText(node->GetElementEditMetadata()->m_name);
            }
        }

        return true;
    }

    void RegisterButtonPropertyHandlers()
    {
        EBUS_EVENT(PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew ButtonBoolHandler());
        EBUS_EVENT(PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew ButtonStringHandler());
    }
}

#include <UI/PropertyEditor/PropertyButtonCtrl.moc>
