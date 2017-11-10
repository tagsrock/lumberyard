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
#ifndef AZCORE_RAPIDJSON_DOCUMENT
#define AZCORE_RAPIDJSON_DOCUMENT

#include <AzCore/JSON/rapidjson.h>

// Make you have available rapidjson/include folder. Currently 3rdParty\rapidjson\rapidjson-1.0.2\include
#include <rapidjson/document.h>

// Check if a Value is a valid object, has the specified key with correct type.
inline bool IsValidMember(const rapidjson::Value& val, const char* key, bool (rapidjson::Value::*func)() const)
{
    return val.IsObject() && val.HasMember(key) && (val[key].*func)();
}
#define RAPIDJSON_IS_VALID_MEMBER(node, key, isTypeFuncPtr) (IsValidMember(node, key, &rapidjson::Value::isTypeFuncPtr))

#endif // AZCORE_RAPIDJSON_DOCUMENT
