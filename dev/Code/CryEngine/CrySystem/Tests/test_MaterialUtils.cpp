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
#include "stdafx.h"
#include <AzTest/AzTest.h>

#include <AzCore/base.h>
#include <AzCore/IO/SystemFile.h>
#include "MaterialUtils.h"


TEST(CrySystemMaterialUtilsTests, MaterialUtilsTestBasics)
{
    char tempBuffer[AZ_MAX_PATH_LEN] = { 0 };
    // call to ensure that it handles nullptr without crashing
    MaterialUtils::UnifyMaterialName(nullptr);
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(tempBuffer[0] == 0);
}

TEST(CrySystemMaterialUtilsTests, MaterialUtilsTestExtensions)
{
    char tempBuffer[AZ_MAX_PATH_LEN];
    strcpy(tempBuffer, "blahblah.mtl");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "blahblah") == 0);

    strcpy(tempBuffer, "blahblah.mat.mat.abc.test.mtl");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "blahblah.mat.mat.abc.test") == 0);

    strcpy(tempBuffer, "test/.mat.mat/blahblah.mat.mat.abc.test.mtl");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "test/.mat.mat/blahblah.mat.mat.abc.test") == 0);

    strcpy(tempBuffer, ".mat.mat.blahblah.mat.mat.abc.test.mtl");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, ".mat.mat.blahblah.mat.mat.abc.test") == 0);
}

TEST(CrySystemMaterialUtilsTests, MaterialUtilsTestPrefixes)
{
    char tempBuffer[AZ_MAX_PATH_LEN];
    strcpy(tempBuffer, ".\\blahblah.mat");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "blahblah") == 0);

    strcpy(tempBuffer, "./materials/blahblah.mat.mat.abc.test");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "materials/blahblah.mat.mat.abc") == 0);

    strcpy(tempBuffer, ".\\engine\\materials\\blahblah.mat.mat.abc.test");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "materials/blahblah.mat.mat.abc") == 0);

    strcpy(tempBuffer, "engine/materials/blahblah.mat.mat.abc.test");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "materials/blahblah.mat.mat.abc") == 0);

    strcpy(tempBuffer, "materials/blahblah.mat");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "materials/blahblah") == 0);
}

TEST(CrySystemMaterialUtilsTests, MaterialUtilsTestGameName)
{
    char tempBuffer[AZ_MAX_PATH_LEN];
    strcpy(tempBuffer, ".\\SamplesProject\\materials\\blahblah.mat.mat.abc.test");
    MaterialUtils::UnifyMaterialName(tempBuffer);
    EXPECT_TRUE(strcmp(tempBuffer, "materials/blahblah.mat.mat.abc") == 0);
}