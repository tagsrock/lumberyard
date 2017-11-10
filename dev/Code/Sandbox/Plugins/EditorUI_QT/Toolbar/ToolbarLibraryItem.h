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
#ifndef CRYINCLUDE_EDITORUI_QT_TOOLBAR_LIBRARY_ITEM_H
#define CRYINCLUDE_EDITORUI_QT_TOOLBAR_LIBRARY_ITEM_H

#include "../Toolbar.h"

class CToolbarLibraryItem
    : public IToolbar
{
public:
    CToolbarLibraryItem(QMainWindow* mainMindow);
    virtual ~CToolbarLibraryItem();

    virtual void init();
};

#endif // CRYINCLUDE_EDITORUI_QT_TOOLBAR_LIBRARY_ITEM_H
