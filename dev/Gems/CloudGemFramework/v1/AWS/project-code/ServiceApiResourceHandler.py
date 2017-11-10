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

import json
import boto3
import hashlib

import custom_resource_response
import properties
from resource_manager_common import stack_info
import role_utils
import patch
from resource_manager_common import aws_utils

s3 = aws_utils.ClientWrapper(boto3.client('s3'))
api_gateway = aws_utils.ClientWrapper(boto3.client('apigateway'))

API_GATEWAY_SERVICE_NAME = 'apigateway.amazonaws.com'
STAGE_NAME = 'api'

PROPERTY_SCHEMA = {
    'ConfigurationBucket': properties.String(),
    'ConfigurationKey': properties.String(),
    'CacheClusterEnabled': properties.Boolean( default = False ),
    'CacheClusterSize': properties.String( default = '0.5' ),
    'SwaggerSettings': properties.Dictionary( default={} ),
    'MethodSettings': properties.Object( default={},
        schema={
            '*': properties.Object( default={}, # path, can be *
                schema={
                    '*': properties.Object( default={}, # method, can be *
                        schema={
                            'CacheDataEncrypted': properties.Boolean( default = False ),
                            'CacheTtlInSeconds': properties.Integer( default = 60 ),
                            'CachingEnable': properties.Boolean( default = False ),
                            'DataTraceEnabled': properties.Boolean( default = False ),
                            'LoggingLevel': properties.String( default='OFF' ),
                            'MetricsEnabled': properties.Boolean( default = False ),
                            'ThrottlingBurstLimit': properties.Integer( default = 2000 ),
                            'ThrottlingRateLimit': properties.Integer( default = 1000 ),
                        }
                    )
                }
            )
        }
    ),
    'StageVariables': properties.Dictionary( default = {} )
}

def handler(event, context):

    request_type = event['RequestType']
    logical_resource_id = event['LogicalResourceId']
    logical_role_name = logical_resource_id
    owning_stack_info = stack_info.get_stack_info(event['StackId'])
    rest_api_resource_name = owning_stack_info.stack_name + '-' + logical_resource_id
    id_data = aws_utils.get_data_from_custom_physical_resource_id(event.get('PhysicalResourceId', None))

    response_data = {}

    if request_type  == 'Create':
        
        props = properties.load(event, PROPERTY_SCHEMA)
        role_arn = role_utils.create_access_control_role(id_data, owning_stack_info.stack_arn, logical_role_name, API_GATEWAY_SERVICE_NAME)
        swagger_content = get_configured_swagger_content(owning_stack_info, props, role_arn, rest_api_resource_name)
        rest_api_id = create_api_gateway(props, swagger_content)
        response_data['Url'] = get_api_url(rest_api_id, owning_stack_info.region)
        id_data['RestApiId'] = rest_api_id
            
    elif request_type == 'Update':

        rest_api_id = id_data.get('RestApiId', None)
        if not rest_api_id:
            raise RuntimeError('No RestApiId found in id_data: {}'.format(id_data))

        props = properties.load(event, PROPERTY_SCHEMA)
        role_arn = role_utils.get_access_control_role_arn(id_data, logical_role_name)
        swagger_content = get_configured_swagger_content(owning_stack_info, props, role_arn, rest_api_resource_name)
        update_api_gateway(rest_api_id, props, swagger_content)
        response_data['Url'] = get_api_url(rest_api_id, owning_stack_info.region)

    elif request_type == 'Delete':

        if not id_data:

            # The will be no data in the id if Cloud Formation cancels a resource creation 
            # (due to a failure in another resource) before it processes the resource create 
            # response. Appearently Cloud Formation has an internal temporary id for the 
            # resource and uses it for the delete request. 
            #
            # Unfortunalty there isn't a good way to deal with this case. We don't have the 
            # id data, so we can't clean up the things it identifies. At best we can allow the 
            # stack cleanup to continue, leaving the rest API behind and role behind.

            print 'WARNING: No id_data provided on delete.'.format(id_data)

        else:

            rest_api_id = id_data.get('RestApiId', None)
            if not rest_api_id:
                raise RuntimeError('No RestApiId found in id_data: {}'.format(id_data))
            
            delete_api_gateway(rest_api_id)
            del id_data['RestApiId']

            role_utils.delete_access_control_role(id_data, logical_role_name)

    else:

        raise RuntimeError('Invalid RequestType: {}'.format(request_type))

    physical_resource_id = aws_utils.construct_custom_physical_resource_id_with_data(event['StackId'], logical_resource_id, id_data)

    custom_resource_response.succeed(event, context, response_data, physical_resource_id)


def get_configured_swagger_content(owning_stack_info, props, role_arn, rest_api_resource_name):
    content = get_input_swagger_content(props)
    content = configure_swagger_content(owning_stack_info, props, role_arn, rest_api_resource_name, content)
    return content


def get_input_swagger_content(props):

    swagger_key = props.ConfigurationKey + '/swagger.json'

    res = s3.get_object(Bucket = props.ConfigurationBucket, Key = swagger_key)
    content = res['Body'].read()

    return content


def configure_swagger_content(owning_stack_info , props, role_arn, rest_api_resource_name, content):

    print 'provided swagger', json.dumps(content)

    deployment_name = "NONE"
    resource_group_name = "NONE"
    project_name = "NONE"
    if owning_stack_info.stack_type == stack_info.StackInfo.STACK_TYPE_RESOURCE_GROUP:
        deployment_name = owning_stack_info.deployment.deployment_name
        resource_group_name = owning_stack_info.resource_group_name
        project_name = owning_stack_info.deployment.project.project_name
    elif owning_stack_info.stack_type == stack_info.StackInfo.STACK_TYPE_DEPLOYMENT:
        deployment_name = owning_stack_info.deployment.deployment_name
        project_name = owning_stack_info.project.project_name
    elif owning_stack_info.stack_type == stack_info.StackInfo.STACK_TYPE_PROJECT:
        project_name = owning_stack_info.project_name

    settings = props.SwaggerSettings
    settings['ResourceGroupName'] = resource_group_name
    settings['DeploymentName'] = deployment_name
    settings['RoleArn'] = role_arn
    settings['Region'] = owning_stack_info.region
    settings['RestApiResourceName'] = rest_api_resource_name
    settings['ProjectName'] = project_name

    print 'settings', settings

    for key, value in settings.iteritems():
        content = content.replace('$' + key + '$', value)

    print 'configured swagger', content

    return content


def create_api_gateway(props, swagger_content):
    rest_api_id = import_rest_api(swagger_content)
    try:
        swagger_digest = compute_swagger_digest(swagger_content)
        create_rest_api_deployment(rest_api_id, swagger_digest)
        update_rest_api_stage(rest_api_id, props)
    except:
        delete_rest_api(rest_api_id)
        raise
    return rest_api_id


def update_api_gateway(rest_api_id, props, swagger_content):
    rest_api_deployment_id = get_rest_api_deployment_id(rest_api_id)
    new_swagger_digest = detect_swagger_changes(rest_api_id, rest_api_deployment_id, swagger_content)
    if new_swagger_digest:
        put_rest_api(rest_api_id, swagger_content)
        create_rest_api_deployment(rest_api_id, new_swagger_digest)
    update_rest_api_stage(rest_api_id, props)


def delete_api_gateway(rest_api_id):
    delete_rest_api(rest_api_id)


def delete_rest_api(rest_api_id):
    res = api_gateway.delete_rest_api(restApiId = rest_api_id)


def detect_swagger_changes(rest_api_id, rest_api_deployment_id, swagger_content):

    new_digest = compute_swagger_digest(swagger_content)
    old_digest = get_rest_api_deployment_swagger_digest(rest_api_id, rest_api_deployment_id)

    if new_digest == old_digest:
        return None
    else:
        return new_digest


def compute_swagger_digest(swagger_content):
    return hashlib.sha224(swagger_content).hexdigest()


def get_rest_api_deployment_swagger_digest(rest_api_id, rest_api_deployment_id):
    
    kwargs = {
        'restApiId': rest_api_id, 
        'deploymentId': rest_api_deployment_id
    }

    res = api_gateway.get_deployment(**kwargs)
    return res.get('description', '')
        

def import_rest_api(swagger_content):

    kwargs = {
        'failOnWarnings': True,
        'body': swagger_content
    }

    res = api_gateway.import_rest_api(**kwargs)
    return res['id']


def put_rest_api(rest_api_id, swagger_content):

    kwargs = {
        'failOnWarnings': True,
        'restApiId': rest_api_id,
        'mode': 'overwrite',
        'body': swagger_content
    }

    res = api_gateway.put_rest_api(**kwargs)


def create_rest_api_deployment(rest_api_id, swagger_digest):

    kwargs = {
        'restApiId': rest_api_id,
        'stageName': STAGE_NAME,
        'description': swagger_digest
    }

    res = api_gateway.create_deployment(**kwargs)


def update_rest_api_stage(rest_api_id, props):

    kwargs = {
        'restApiId': rest_api_id,
        'stageName': STAGE_NAME,
    }

    current_stage = api_gateway.get_stage(**kwargs)

    patch_operations = get_rest_api_stage_update_patch_operations(current_stage, props)
    
    if patch_operations:

        kwargs = {
            'restApiId': rest_api_id,
            'stageName': STAGE_NAME,
            'patchOperations': patch_operations
        }

        res = api_gateway.update_stage(**kwargs)


def get_rest_api_stage_update_patch_operations(current_stage, props):

    builder = patch.OperationListBuilder()

    if current_stage.get('cacheClusterEnabled', False) != props.CacheClusterEnabled:
        builder.replace('/cacheClusterEnabled', props.CacheClusterEnabled)

    if current_stage.get('cacheClusterSize', '') != props.CacheClusterSize:
        builder.replace('/cacheClusterSize', props.CacheClusterSize)

    builder.diff('/variables/', current_stage.get('variables', {}), props.StageVariables)

    # TODO: support MethodSettings. https://issues.labcollab.net/browse/LMBR-32185
    #
    # See https://docs.aws.amazon.com/apigateway/api-reference/resource/stage/#methodSettings
    # for the paths needed to update these values. Note that the paths for these values have 
    # the form /a~1b~1c/GET (slashes in the resource path replaced by ~1, and GET is any HTTP
    # verb). 
    #
    # We can't remove individual values, we can only replace them with default values if they 
    # aren't specified. However, if a path/method exists in current_stage but not in props, then 
    # all the settings should be removed using a path like /a~1b~1c/GET.
    #
    # The wildcards may need some special handling.

    return builder.get()


def get_rest_api_deployment_id(rest_api_id):
    
    kwargs = {
        'restApiId': rest_api_id,
        'stageName': STAGE_NAME
    }

    res = api_gateway.get_stage(**kwargs)
    return res['deploymentId']


def get_api_url(rest_api_id, region):
    return 'https://{rest_api_id}.execute-api.{region}.amazonaws.com/{stage_name}'.format(
        rest_api_id = rest_api_id,
        region = region,
        stage_name = STAGE_NAME)

