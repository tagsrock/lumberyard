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

#ifndef INC_TARGETSELECTORBUTTON_H
#define INC_TARGETSELECTORBUTTON_H

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include "AzFramework/TargetManagement/TargetManagementAPI.h"
#include <QtWidgets/QPushButton>
#include <qwidgetaction.h>

#pragma once

namespace AzToolsFramework
{
    class TargetSelectorButton
        : public QPushButton
        , private AzFramework::TargetManagerClient::Bus::Handler
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(TargetSelectorButton, AZ::SystemAllocator, 0);

        TargetSelectorButton(QWidget* pParent = 0);
        virtual ~TargetSelectorButton();

        // implement AzFramework::TargetManagerClient::Bus::Handler
        void DesiredTargetConnected(bool connected);

    private:
        void UpdateStatus();
        void ConstructDisplayTargetString(QString& outputString, const AzFramework::TargetInfo& info);

    private slots:
        void DoPopup();
    };


    class TargetSelectorButtonAction
        : public QWidgetAction
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(TargetSelectorButtonAction, AZ::SystemAllocator, 0);

        TargetSelectorButtonAction(QObject* pParent);                                     // create default action

    protected:
        virtual QWidget* createWidget(QWidget* pParent);
    };
}

#endif
