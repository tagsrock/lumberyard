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

#include <AzQtComponents/Components/DockMainWindow.h>


namespace AzQtComponents
{
    /**
     * Create a dock main window that extends the QMainWindow so we can construct
     * our own custom context popup menu
     */
    DockMainWindow::DockMainWindow(QWidget* parent)
        : QMainWindow(parent)
    {
    }

    /**
     * Override of QMainWindow::createPopupMenu to not show any context menu when
     * right-clicking on the space between our dock widgets
     */
    QMenu* DockMainWindow::createPopupMenu()
    {
        return nullptr;
    }

#include <Components/DockMainWindow.moc>
} // namespace AzQtComponents