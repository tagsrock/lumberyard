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

#ifndef CRYINCLUDE_CRYPHYSICS_OBBTREE_INFO_H
#define CRYINCLUDE_CRYPHYSICS_OBBTREE_INFO_H
#pragma once

#include "obbtree.h"

STRUCT_INFO_BEGIN(OBBnode)
STRUCT_VAR_INFO(axes, TYPE_ARRAY(3, TYPE_INFO(Vec3)))
STRUCT_VAR_INFO(center, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(size, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(iparent, TYPE_INFO(int))
STRUCT_VAR_INFO(ichild, TYPE_INFO(int))
STRUCT_VAR_INFO(ntris, TYPE_INFO(int))
STRUCT_INFO_END(OBBnode)


#endif // CRYINCLUDE_CRYPHYSICS_OBBTREE_INFO_H
