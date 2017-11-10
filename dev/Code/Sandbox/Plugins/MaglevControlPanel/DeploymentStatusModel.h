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

#include <AWSResourceManager.h>
#include <StackStatusModel.h>

class DeploymentStatusModel
    : public StackStatusModel<IDeploymentStatusModel>
{
    Q_OBJECT

public:

    DeploymentStatusModel(AWSResourceManager* resourceManager, const QString& deployment);

    void ProcessOutputDeploymentStackDescription(const QVariantMap& map);

    void Refresh(bool force = false) override;
    bool UpdateStack() override;
    bool DeleteStack() override;

    QString GetUpdateButtonText() const override;
    QString GetUpdateButtonToolTip() const override;
    QString GetUpdateConfirmationTitle() const override;
    QString GetUpdateConfirmationMessage() const override;
    QString GetStatusTitleText() const override;
    QString GetMainMessageText() const override;
    QString GetDeleteButtonText() const override;
    QString GetDeleteButtonToolTip() const override;
    QString GetDeleteConfirmationTitle() const override;
    QString GetDeleteConfirmationMessage() const override;

    QString GetExportMappingButtonText() const override;
    QString GetExportMappingToolTipText() const override;

    QString GetProtectedMappingCheckboxText() const override;
    QString GetProtectedMappingToolTipText() const override;

signals:

    void DeploymentUpdateInProgress(const QString& deploymentName);

private:

    AWSResourceManager::RequestId m_requestId;

};
