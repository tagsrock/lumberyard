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
#include "CategoriesList.h"

ComponentCategoryList::ComponentCategoryList(QWidget* parent /*= nullptr*/) 
    : QTreeWidget(parent)
{
}

void ComponentCategoryList::Init()
{
    setColumnCount(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setDragDropMode(QAbstractItemView::DragDropMode::DragOnly);
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAllColumnsShowFocus(true);
    setStyleSheet("QTreeWidget { selection-background-color: rgba(255,255,255,0.2); }");

    QStringList headers;
    headers << tr("Categories");
    setHeaderLabels(headers);

    const QString parentCategoryIconPath = QString("Editor/Icons/PropertyEditor/browse_on.png");
    const QString categoryIconPath = QString("Editor/Icons/PropertyEditor/browse.png");

    QTreeWidgetItem* allCategory = new QTreeWidgetItem(this);
    allCategory->setText(0, "All");
    allCategory->setIcon(0, QIcon(categoryIconPath));

    // Need this briefly to collect the list of available categories.
    ComponentDataModel dataModel(this);
    for (const auto& category : dataModel.GetCategories())
    {
        QString categoryString = QString(category.c_str());
        QStringList categories = categoryString.split('/', QString::SkipEmptyParts);

        QTreeWidgetItem* parent = nullptr;
        QTreeWidgetItem* category = nullptr;

        for (const auto& categoryName : categories)
        {
            if (parent)
            {
                category = new QTreeWidgetItem(parent);
                category->setIcon(0, QIcon(categoryIconPath));
                
                // Store the full category path in a user role because we'll need it to locate the actual category
                category->setData(0, Qt::UserRole, QVariant::fromValue(categoryString));
            }
            else
            {
                auto existingCategory = findItems(categoryName, Qt::MatchExactly);
                if (existingCategory.empty())
                {
                    category = new QTreeWidgetItem(this);
                    category->setIcon(0, QIcon(parentCategoryIconPath));
                }
                else
                {
                    category = static_cast<QTreeWidgetItem*>(existingCategory.first());
                    category->setIcon(0, QIcon(parentCategoryIconPath));
                }
            }

            parent = category;

            category->setText(0, categoryName);
        }
    }

    expandAll();

    connect(this, &QTreeWidget::itemClicked, this, &ComponentCategoryList::OnItemClicked);
}

void ComponentCategoryList::OnItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    QVariant userData = item->data(0, Qt::UserRole);
    if (userData.isValid())
    {
        // Send in the full category path, not just the child category name
        emit OnCategoryChange(userData.value<QString>().toStdString().c_str());
    }
    else
    {
        emit OnCategoryChange(item->text(0).toStdString().c_str());
    }
}

#include <UI/ComponentPalette/CategoriesList.moc>
