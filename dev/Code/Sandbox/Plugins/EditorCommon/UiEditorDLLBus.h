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

#include <AzCore/EBus/EBus.h>
#include <LyShine/UiBase.h>

class UndoStack;

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Interface class that the UI Editor needs to implement
class UiEditorDLLInterface
    : public AZ::EBusTraits
{
public: // member functions

    virtual ~UiEditorDLLInterface(){}

    //! Get the selected elements in the UiEditor
    virtual LyShine::EntityArray GetSelectedElements() = 0;

    //! Get the id of the active Canvas the UiEditor
    virtual AZ::EntityId GetActiveCanvasId() = 0;

    //! Get the active undo stack for the UI Editor
    virtual UndoStack* GetActiveUndoStack() = 0;

public: // static member functions

    static const char* GetUniqueName() { return "UiEditorDLLInterface"; }
};

typedef AZ::EBus<UiEditorDLLInterface> UiEditorDLLBus;
