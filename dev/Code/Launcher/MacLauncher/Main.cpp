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

#include <MacLumberyardApplication.h>

#include <AppleLauncher.h>

#include <AzFramework/Input/System/InputSystemComponent.h>
#if !defined(AZ_FRAMEWORK_INPUT_ENABLED)
#   include <SDL.h>
#endif

#if defined(AZ_TESTS_ENABLED)
#include <AzTest/AzTest.h>
DECLARE_AZ_UNIT_TEST_MAIN()
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
#if defined(AZ_TESTS_ENABLED)
    // If "--unittest" is present on the command line, run unit testing
    // and return immediately. Otherwise, continue as normal.
    INVOKE_AZ_UNIT_TEST_MAIN();
#endif

#if defined(AZ_FRAMEWORK_INPUT_ENABLED)
    // Ensure the process is a foreground application. Must be done before creating the application.
    ProcessSerialNumber processSerialNumber;
    if (!GetCurrentProcess(&processSerialNumber))
    {
        TransformProcessType(&processSerialNumber, kProcessTransformToForegroundApplication);
    }

    // Create a memory pool, a custom AppKit application, and a custom AppKit application delegate.
    NSAutoreleasePool* autoreleasePool = [[NSAutoreleasePool alloc] init];
    [MacLumberyardApplication sharedApplication];
    [NSApp setDelegate: [[MacLumberyardApplicationDelegate alloc] init]];

    // Register some default application behaviours
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [[NSDictionary alloc] initWithObjectsAndKeys:
            [NSNumber numberWithBool: FALSE], @"AppleMomentumScrollSupported",
            [NSNumber numberWithBool: FALSE], @"ApplePressAndHoldEnabled",
            nil]];

    // Launch the AppKit application and release the memory pool.
    [NSApp finishLaunching];
    [autoreleasePool release];
#else // !defined(AZ_FRAMEWORK_INPUT_ENABLED)
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
#endif // defined(AZ_FRAMEWORK_INPUT_ENABLED)

    // Launch the Lumberyard application.
    return AppleLauncher::Launch("");
}
