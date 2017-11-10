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

#include <AzTest/AzTest.h>

#include <CGFContent.h>

class MockIAssetWriter
    : public IAssetWriter
{
public:
    ~MockIAssetWriter() override = default;
    MOCK_METHOD1(WriteCGF,
        bool(CContentCGF* content));
    MOCK_METHOD2(WriteCHR,
        bool(CContentCGF* content, IConvertContext* convertContext));
    MOCK_METHOD3(WriteSKIN,
        bool(CContentCGF* content, IConvertContext* convertContext, bool exportMorphTargets));
    MOCK_METHOD4(WriteCAF,
        bool(CContentCGF* content, const AZ::SceneAPI::DataTypes::IAnimationGroup* animationGroup, CInternalSkinningInfo* controllerSkinningInfo, IConvertContext* convertContext));
};
