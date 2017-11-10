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
#include <Util/FlowSystem/BaseMaglevFlowNode.h>

namespace LmbrAWS
{
    class FlowNode_ApplyConfiguration
        : public BaseMaglevFlowNode<eNCT_Singleton>
    {
    public:

        FlowNode_ApplyConfiguration(IFlowNode::SActivationInfo* activationInfo);

        virtual void GetMemoryUsage(ICrySizer* sizer) const override { sizer->Add(*this); }

        const char* GetFlowNodeDescription() const override { return _HELP("Apply AWS configuration to all managed clients."); }
        Aws::Vector<SInputPortConfig> GetInputPorts() const override;
        Aws::Vector<SOutputPortConfig> GetOutputPorts() const override;

        virtual void ProcessEvent_Internal(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* activationInfo) override;

    protected:
        virtual const char* GetClassTag() const override;

    private:

        enum
        {
            EIP_Apply = EIP_StartIndex
        };
    };
} // namespace Amazon

