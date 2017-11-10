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

#ifndef ENTITY_ID_QLABEL_HXX
#define ENTITY_ID_QLABEL_HXX

#include <AzCore/base.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Memory/SystemAllocator.h>

#include <QtWidgets/QLabel>

#pragma once

class QSpinBox;
class QPushButton;

namespace AzToolsFramework
{
    class EntityIdQLabel
        : public QLabel
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(EntityIdQLabel, AZ::SystemAllocator, 0);

        explicit EntityIdQLabel(QWidget* parent = 0);
        ~EntityIdQLabel() override;

        void SetEntityId(AZ::EntityId newId);
        AZ::EntityId GetEntityId() const { return m_entityId; }

    signals:
        void RequestPickObject();

    protected:
        virtual void mousePressEvent(QMouseEvent* e) override;
        virtual void mouseDoubleClickEvent(QMouseEvent* e) override;

    private:
        AZ::EntityId m_entityId;
    };
}

#endif
