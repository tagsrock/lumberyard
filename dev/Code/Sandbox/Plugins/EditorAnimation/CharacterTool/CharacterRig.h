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

#pragma once
#include <vector>
#include "Strings.h"

struct ICharacterInstance;
namespace Serialization {
    class IArchive;
}

namespace CharacterTool
{
    using std::vector;
    struct SCharacterRigDummyComponent
    {
        string name;

        void Serialize(Serialization::IArchive& ar);
    };

    struct SCharacterRigContent
    {
        vector<SCharacterRigDummyComponent> m_components;

        void Serialize(Serialization::IArchive& ar);
        void ApplyToCharacter(ICharacterInstance* instance);

        void Reset() { *this = SCharacterRigContent(); }
    };
}
