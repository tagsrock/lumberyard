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

namespace ComponentHelpers
{
    QList<QAction*> CreateAddComponentActions(HierarchyWidget* hierarchy,
        QTreeWidgetItemRawPtrQList& selectedItems,
        QWidget* parent);
    QList<QAction*> CreateRemoveComponentActions(HierarchyWidget* hierarchy,
        QTreeWidgetItemRawPtrQList& selectedItems,
        const AZ::Component* optionalOnlyThisComponentType);

    struct ComponentTypeData
    {
        const AZ::SerializeContext::ClassData* classData;
        bool isLyShineComponent;
    };
    AZStd::vector<ComponentTypeData> GetAllComponentTypesThatCanAppearInAddComponentMenu();

}   // namespace ComponentHelpers
