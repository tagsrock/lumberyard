#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
from __future__ import print_function
import service
import CloudCanvas
import boto3
from errors import ClientError

def invoke_lambda(client, function_name):
    response = client.invoke(FunctionName=function_name, InvocationType='Event')
    if "FunctionError" in response:
        raise ClientError("Failed to invoke {} -- {}".format(function_name, response["FunctionError"]))

@service.api
def get(request):
    lambda_client = boto3.client("lambda")
    invoke_lambda(lambda_client, CloudCanvas.get_setting("RecordSetProcessor"))
