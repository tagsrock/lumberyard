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

import account_utils
import botocore
import errors
import service

@service.api
def post(request, cognitoUsername):
    try:
        account_utils.get_user_pool_client().admin_confirm_sign_up(UserPoolId=account_utils.get_user_pool_id(), Username=cognitoUsername)
    except botocore.exceptions.ClientError as e:
        code = e.response.get('Error', {}).get('Code')
        message = e.response.get('Error', {}).get('Message')

        if code == 'NotAuthorizedException' and 'CONFIRMED' in message:
            print 'The user is already confirmed: {}'.format(message)
            return {}

        raise

    return {}
