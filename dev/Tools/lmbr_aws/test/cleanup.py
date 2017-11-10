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
# $Revision: #2 $

import argparse
import boto3
import time
from botocore.client import Config
from botocore.exceptions import ClientError

DEFAULT_REGION='us-east-1'

DEFAULT_PREFIXES = [ 'cctest' ]

class Cleaner:

    def __init__(self, session):
        self.cf = session.client('cloudformation')
        self.s3 = session.client('s3',config=Config(signature_version='s3v4'))
        self.iam = session.client('iam')
        self.cognito_identity = session.client('cognito-identity')
        self.cognito_idp = session.client('cognito-idp')
        self.logs = session.client('logs')
        self.apigateway = session.client('apigateway')

    def cleanup(self, prefixes, exceptions):
        self.__prefixes = prefixes
        self.__exceptions = exceptions
        self._delete_stacks()
        self._delete_buckets()
        self._delete_identity_pools()
        self._delete_user_pools()
        self._delete_users()
        self._delete_roles()
        self._delete_policies()
        self._delete_log_groups()
        self._delete_api_gateway()

    def _has_prefix(self, name):
        
        name = name.lower()

        for exception in self.__exceptions:
            if name.startswith(exception):
                return False

        for prefix in self.__prefixes:
            if name.startswith(prefix):
                return True

        return False

    def __describe_prefixes(self):
        return str(self.__prefixes) + ' but not ' + str(self.__exceptions)

    def _delete_buckets(self):

        print '\n\nlooking for buckets with names starting with one of', self.__describe_prefixes()

        res = self.s3.list_buckets()
        while True:
            for bucket in res['Buckets']:
                # print 'considering bucket', bucket['Name']
                if not self._has_prefix(bucket['Name']):
                    # print '  not in prefix list'
                    continue
                print '  found bucket', bucket['Name']
                self._clean_bucket(bucket['Name'])
                try:
                    print '    deleting bucket', bucket['Name']
                    self.s3.delete_bucket(Bucket=bucket['Name'])
                    pass
                except ClientError as e:
                    print '      ERROR', e.message
            next_token = res.get('NextToken', None)
            if next_token is None:
                break
            else:
                res = self.s3.list_buckets(NextToken=next_token)

    def _delete_stacks(self):
        
        print '\n\nlooking for stacks with names starting with one of', self.__describe_prefixes()

        stack_list = []
        res = self.cf.list_stacks()
        while True:
            for stack in res['StackSummaries']:
                # print 'Considering stack', stack['StackName']
                if not self._has_prefix(stack['StackName']):
                    # print '  not in prefixes list'
                    continue
                if stack['StackStatus'] == 'DELETE_COMPLETE' or stack['StackStatus'] == 'DELETE_IN_PROGRESS':
                    # print '  already has deleted status'
                    continue
                stack_list.append(stack)
            next_token = res.get('NextToken', None)
            if next_token is None:
                break
            else:
                res = self.cf.list_stacks(NextToken=next_token)

        for stack in stack_list:
            print '  found stack', stack['StackStatus'], stack['StackId']
            retained_resources = self._clean_stack(stack['StackId'])
            try:
                print '    deleting stack', stack['StackId']
                if stack['StackStatus'] == 'DELETE_FAILED':
                    res = self.cf.delete_stack(StackName=stack['StackId'], RetainResources=retained_resources)
                else:
                    res = self.cf.delete_stack(StackName=stack['StackId'])
            except ClientError as e:
                print '      ERROR', e.message

    def _clean_stack(self, stack_id):
        retained_resources = []
        try:
            print '    getting resources for stack', stack_id
            response = self.cf.describe_stack_resources(StackName=stack_id)
        except ClientError as e:
            print '      ERROR', e.response
            return
        for resource in response['StackResources']:
            resource_id = resource.get('PhysicalResourceId', None)
            if resource_id is not None:
                if resource['ResourceType'] == 'AWS::CloudFormation::Stack':
                    self._clean_stack(resource_id)
                if resource['ResourceType'] == 'AWS::S3::Bucket':
                    self._clean_bucket(resource_id)
                if resource['ResourceType'] == 'AWS::IAM::Role':
                    self._clean_role(resource_id)
            if resource['ResourceStatus'] == 'DELETE_FAILED':
                retained_resources.append(resource['LogicalResourceId'])
        return retained_resources

    def _clean_bucket(self, bucket_id):
        print '    cleaning bucket', bucket_id
        list_res = {}
        try:
            list_res = self.s3.list_object_versions(Bucket=bucket_id, MaxKeys=1000)
        except ClientError as e:
            print e
            if e.response['Error']['Code'] == 'NoSuchBucket':
                return
        while 'Versions' in list_res and list_res['Versions']:

            delete_list = []

            for version in list_res['Versions']:
                print '      deleting object', version['Key'], version['VersionId']
                delete_list.append(
                    {
                        'Key': version['Key'],
                        'VersionId': version['VersionId']
                    })

            try:
                delete_res = self.s3.delete_objects(Bucket=bucket_id, Delete={ 'Objects': delete_list, 'Quiet': True })
            except ClientError as e:
                print '        ERROR', e.message
                return

            try:
                list_res = self.s3.list_object_versions(Bucket=bucket_id, MaxKeys=1000)
            except ClientError as e:
                if e.response['Error']['Code'] == 'NoSuchBucket':
                    return

    def _clean_role(self, resource_id):
        
        print '    cleaning role', resource_id

        # delete policies

        try:
            res = self.iam.list_role_policies(RoleName=resource_id)
        except ClientError as e:
            if e.response['Error']['Code'] != 'NoSuchEntity':
                print '      ERROR:', e
            return
        
        for policy_name in res['PolicyNames']:
            print '      deleting policy', policy_name
            try:
                self.iam.delete_role_policy(RoleName=resource_id, PolicyName=policy_name)
            except ClientError as e:
                print '        ERROR:', e

        # detach policies

        try:
            res = self.iam.list_attached_role_policies(RoleName=resource_id)
        except ClientError as e:
            if e.response['Error']['Code'] != 'NoSuchEntity':
                print '      ERROR:', e
            return
        
        for attached_policy in res['AttachedPolicies']:
            print '      detaching policy', attached_policy['PolicyName']
            try:
                self.iam.detach_role_policy(RoleName=resource_id, PolicyArn=attached_policy['PolicyArn'])
            except ClientError as e:
                print '        ERROR:', e


    def _clean_user(self, resource_id):
        
        print '    cleaning user', resource_id

        # delete policies

        try:
            res = self.iam.list_user_policies(UserName=resource_id)
        except ClientError as e:
            if e.response['Error']['Code'] != 'NoSuchEntity':
                print '      ERROR:', e
            return
        
        for policy_name in res['PolicyNames']:
            print '      deleting policy', policy_name
            try:
                self.iam.delete_user_policy(UserName=resource_id, PolicyName=policy_name)
            except ClientError as e:
                print '        ERROR:', e

        # detach policies

        try:
            res = self.iam.list_attached_user_policies(UserName=resource_id)
        except ClientError as e:
            if e.response['Error']['Code'] != 'NoSuchEntity':
                print '      ERROR:', e
            return
        
        for attached_policy in res['AttachedPolicies']:
            print '      detaching policy', attached_policy['PolicyName']
            try:
                self.iam.detach_user_policy(UserName=resource_id, PolicyArn=attached_policy['PolicyArn'])
            except ClientError as e:
                print '        ERROR:', e

        # delete access keys

        try:
            res = self.iam.list_access_keys(UserName=resource_id)
        except ClientError as e:
            if e.response['Error']['Code'] != 'NoSuchEntity':
                print '      ERROR:', e
            return
        
        for access_key_metadata in res['AccessKeyMetadata']:
            print '      deleting access key', access_key_metadata['AccessKeyId']
            try:
                self.iam.delete_access_key(UserName=resource_id, AccessKeyId=access_key_metadata['AccessKeyId'])
            except ClientError as e:
                print '        ERROR:', e

                          
    def _delete_identity_pools(self):

        print '\n\nlooking for cognito identity pools with names starting with one of', self.__describe_prefixes()

        res = self.cognito_identity.list_identity_pools(MaxResults=60)
        while True:
            for pool in res['IdentityPools']:
                # The congitio pools created by CCRM are prefixed with either PlayerAccess or PlayerLogin,
                # followed by the stack name (which is matched against the prefixed passed to this script)

                name = pool['IdentityPoolName']
                for removed in ['PlayerAccess', 'PlayerLogin']:
                    if name.startswith(removed):
                        name = name.replace(removed, '')
                        break;

                if not self._has_prefix(name):
                    continue
                try:
                    print '  deleting identity pool', pool['IdentityPoolName']
                    self.cognito_identity.delete_identity_pool(IdentityPoolId=pool['IdentityPoolId'])
                    pass
                except ClientError as e:
                    print '    ERROR', e.message
            next_token = res.get('NextToken', None)
            if next_token is None:
                break
            else:
                res = self.cognito_identity.list_identity_pools(NextToken=next_token)

    def _delete_user_pools(self):

        print '\n\nlooking for cognito user pools with names starting with one of', self.__describe_prefixes()

        res = self.cognito_idp.list_user_pools(MaxResults=60)
        while True:
            for pool in res['UserPools']:
                # The congitio pools created by CloudGemPlayerAccount are prefixed with PlayerAccess,
                # followed by the stack name (which is matched against the prefixed passed to this script)

                name = pool['Name']
                if name.startswith('PlayerAccess'):
                    name = name.replace('PlayerAccess', '')

                if not self._has_prefix(name):
                    continue
                try:
                    print '  deleting user pool', pool['Name']
                    self.cognito_idp.delete_user_pool(UserPoolId=pool['Id'])
                except ClientError as e:
                    print '    ERROR', e.message
            next_token = res.get('NextToken', None)
            if next_token is None:
                break
            else:
                res = self.cognito_idp.list_user_pools(MaxResults=60, NextToken=next_token)

    def _delete_roles(self):

        print '\n\nlooking for roles with names or paths starting with one of', self.__describe_prefixes()

        res = self.iam.list_roles()
        while True:

            for role in res['Roles']:
                    
                path = role['Path'][1:] # remove leading /

                if not self._has_prefix(path) and not self._has_prefix(role['RoleName']):
                    continue

                print '  cleaning role', role['RoleName']

                self._clean_role(role['RoleName'])

                try:
                    print '    deleting role', role['RoleName']
                    self.iam.delete_role(RoleName=role['RoleName'])
                except ClientError as e:
                    print '      ERROR:', e

            marker = res.get('Marker', None)
            if marker is None:
                break
            else:
                res = self.iam.list_roles(Marker=marker)


    def _delete_users(self):

        print '\n\nlooking for users with names or paths starting with one of', self.__describe_prefixes()

        res = self.iam.list_users()
        while True:

            for user in res['Users']:
                    
                path = user['Path'][1:] # remove leading /

                if not self._has_prefix(path) and not self._has_prefix(user['UserName']):
                    continue

                print '  cleaning user', user['UserName']

                self._clean_user(user['UserName'])

                try:
                    print '    deleting user', user['UserName']
                    self.iam.delete_user(UserName=user['UserName'])
                except ClientError as e:
                    print '      ERROR:', e

            marker = res.get('Marker', None)
            if marker is None:
                break
            else:
                res = self.iam.list_users(Marker=marker)


    def _delete_policies(self):

        print '\n\nlooking for policies with names or paths starting with one of', self.__describe_prefixes()

        res = self.iam.list_policies()
        while True:

            for policy in res['Policies']:
                    
                path = policy['Path'][1:] # remove leading /

                if not self._has_prefix(path) and not self._has_prefix(policy['PolicyName']):
                    continue

                try:
                    print '    deleting policy', policy['Arn']
                    self.iam.delete_policy(PolicyArn=policy['Arn'])
                except ClientError as e:
                    print '      ERROR:', e

            marker = res.get('Marker', None)
            if marker is None:
                break
            else:
                res = self.iam.list_policies(Marker=marker)


    def _delete_log_groups(self):

        print '\n\nlooking for log groups starting with one of', self.__describe_prefixes()

        res = self.logs.describe_log_groups()
        while True:

            for log_group in res['logGroups']:

                name = log_group['logGroupName']
                name = name.replace('/aws/lambda/', '')
                if self._has_prefix(name):

                    try:
                        print '  deleting log group', log_group['logGroupName']
                        self.logs.delete_log_group(logGroupName = log_group['logGroupName'])
                    except ClientError as e:
                        print '      ERROR:', e

            next_token = res.get('nextToken', None)
            if next_token is None:
                break
            else:
                res = self.logs.describe_log_groups(nextToken=next_token)

    def _delete_api_gateway(self):

        print '\n\nlooking for api gateway resources starting with one of', self.__describe_prefixes()

        res = self.apigateway.get_rest_apis()
        while True:

            for rest_api in res['items']:

                name = rest_api['name']
                if self._has_prefix(name):

                    while True:
                        try:
                            print '  deleting rest_api', rest_api['name'], rest_api['id']
                            self.apigateway.delete_rest_api(restApiId = rest_api['id'])
                            break
                        except ClientError as e:
                            if e.response["Error"]["Code"] == "TooManyRequestsException":
                                print '    too many requests, sleeping...'
                                time.sleep(15)
                            else:
                                print '      ERROR:', e
                                break

            position = res.get('position', None)
            if position is None:
                break
            else:
                res = self.apigateway.get_rest_apis(position=position)

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix', dest='prefixes', nargs='+', default=DEFAULT_PREFIXES, help='Any stacks and buckets with names that start with this value will be deleted.')
    parser.add_argument('--except', dest='exceptions', nargs='+', default=[], help='Do not delete anything starting with these prefixes (useful for cleaning up old results while a test is in progress).')
    parser.add_argument('--profile', default='default', help='The AWS profile to use. Defaults to the default AWS profile.')
    parser.add_argument('--aws-access-key', required=False, help='The AWS access key to use.')
    parser.add_argument('--aws-secret-key', required=False, help='The AWS secret key to use.')
    parser.add_argument('--region', default=DEFAULT_REGION, help='The AWS region to use. Defaults to {}.'.format(DEFAULT_REGION))

    args = parser.parse_args()

    prefixes = []
    for prefix in args.prefixes:
        prefixes.append(prefix.lower())

    exceptions = []
    for exception in args.exceptions:
        exceptions.append(exception.lower())

    if args.aws_access_key and args.aws_secret_key:
        session = boto3.Session(aws_access_key_id=args.aws_access_key, aws_secret_access_key=args.aws_secret_key, region_name=args.region)
    else:
        session = boto3.Session(profile_name=args.profile, region_name=args.region)
        
    cleaner = Cleaner(session)
    cleaner.cleanup(prefixes, exceptions)

 

