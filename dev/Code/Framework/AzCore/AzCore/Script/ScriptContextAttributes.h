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
#ifndef AZCORE_SCRIPT_CONTEXT_ATTRIBUTES_H
#define AZCORE_SCRIPT_CONTEXT_ATTRIBUTES_H

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/Math/Crc.h>

namespace AZ
{
    namespace Script
    {
        namespace Attributes
        {
            const static AZ::Crc32 Ignore = AZ_CRC("ScriptIgnore", 0xeb7615e1); ///< Don't use the element in the script reflection
            const static AZ::Crc32 ClassNameOverride = AZ_CRC("ScriptClassNameOverride", 0x891238a3); ///< Provide a custom name for script reflection, that doesn't match the behavior Context name
            const static AZ::Crc32 MethodOverride = AZ_CRC("ScriptFunctionOverride", 0xf89a7882); ///< Use a custom function in the attribute instead of the function  
            const static AZ::Crc32 ConstructorOverride = AZ_CRC("ConstructorOverride", 0xef5ce4aa); ///< You can provide a custom constructor to be called when created from script
            const static AZ::Crc32 ReaderWriterOverride = AZ_CRC("ReaderWriterOverride", 0x1ad9ce2a); ///< paired with \ref ScriptContext::CustomReaderWriter allows you to customize read/write to Lua VM
            const static AZ::Crc32 ConstructibleFromNil = AZ_CRC("ConstructibleFromNil", 0x23908169); ///< Applied to classes. Value (bool) specifies if the class be default constructed when nil is provided.
            
            const static AZ::Crc32 Storage = AZ_CRC("ScriptStorage", 0xcd95b44d);
            enum class StorageType
            {
                ScriptOwn, // default
                RuntimeOwn, // C++
                Value, // Object is stored by value in the VM
            };

            const static AZ::Crc32 Operator = AZ_CRC("ScriptOperator", 0xfee681b6);
            enum class OperatorType
            {
                // note storage policy can be T*,T (only if we store raw pointers), shared_ptr<T>, intrusive pointer<T>
                Add,       // '+'      StoragePolicy<T> add(const StoragePolicy<T>& rhs)
                Sub,       // '-'      --"--
                Mul,       // '*'      --"--
                Div,       // '/'      --"--
                Mod,       // '%'      --"--
                Pow,       // '^'      --"--
                Unary,     // '- (unary)'  StoragePolicy<T> unary(const StoragePolicy<T>&);  // const T& usually not used so it can skipped from the signature
                Concat,    // '..'     StoragePolicy<T> concat(const StoragePolicy<T>& rhs);
                Length,    // '#'      int length(const StoragePolicy<T>&); // const T& not used normally so it can be skipped from the signature
                Equal,     // '=='     bool eq(const StoragePolicy<T>& rhs)
                LessThan,  // '<'      --"--
                LessEqualThan, // '<='     --"---
                ToString,  // converts object to string AZStd::string tostring()
                IndexRead, // given a key/index, you should return a value at this key/index. Keep in mind that IndexRead/Write functions can't take string as index as it's reserved for functions and properties
                IndexWrite, // given a key/index and a value, you can store it in the class
            };
        } // Attributes
    } // Script

    // set type for the helper enums
    AZ_TYPE_INFO_SPECIALIZE(Script::Attributes::OperatorType, "{26B98C03-7E07-4E3E-9E31-03DA2168E896}");
    AZ_TYPE_INFO_SPECIALIZE(Script::Attributes::StorageType, "{57FED71F-B590-4002-9599-A48CB50B0F8E}");
} // AZ


#endif // AZCORE_SCRIPT_CONTEXT_ATTRIBUTES_H
