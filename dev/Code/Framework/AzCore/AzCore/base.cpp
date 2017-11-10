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

#include <AzCore/base.h>

namespace AZ
{
    const char* GetPlatformName(PlatformID platform)
    {
        switch (platform)
        {
        case PLATFORM_WINDOWS_32:
            return "Win32";
        case PLATFORM_WINDOWS_64:
            return "Win64";
        case PLATFORM_XBOX_360:
            return "X360";
        case PLATFORM_XBONE:
            return "XB1";
        case PLATFORM_PS3:
            return "PS3";
        case PLATFORM_PS4:
            return "PS4";
        case PLATFORM_WII:
            return "WII";
        case PLATFORM_LINUX_64:
            return "Linux";
        case PLATFORM_ANDROID:
            return "Android";
        case PLATFORM_APPLE_IOS:
            return "iOS";
        case PLATFORM_APPLE_OSX:
            return "OSX";
        case PLATFORM_APPLE_TV:
            return "AppleTV";
        default:
            AZ_Assert(false, "Platform %u is unknown.", static_cast<u32>(platform));
            return "";
        }
    }
} // namespace AZ
