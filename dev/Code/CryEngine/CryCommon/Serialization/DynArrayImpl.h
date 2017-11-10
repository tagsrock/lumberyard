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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYCOMMON_SERIALIZATION_DYNARRAYIMPL_H
#define CRYINCLUDE_CRYCOMMON_SERIALIZATION_DYNARRAYIMPL_H
#pragma once

#include "IArchive.h"
#include "STLImpl.h"

template<class T, class I, class S>
bool Serialize(Serialization::IArchive& ar, DynArray<T, I, S>& container, const char* name, const char* label)
{
    Serialization::ContainerSTL<DynArray<T, I, S>, T> ser(&container);
    return ar(static_cast<Serialization::IContainer&>(ser), name, label);
}

#endif // CRYINCLUDE_CRYCOMMON_SERIALIZATION_DYNARRAYIMPL_H
