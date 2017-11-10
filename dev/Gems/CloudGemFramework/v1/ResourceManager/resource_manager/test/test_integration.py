#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the 'License'). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# $Revision: #1 $

import os
import json
import shutil
import unittest

from copy import deepcopy
from time import sleep

from botocore.exceptions import ClientError

import resource_manager.util
import resource_manager.constant

import lmbr_aws_test_support
import mock_specification

class IntegrationTest_CloudGemFramework_ResourceManager_StackOperations(lmbr_aws_test_support.lmbr_aws_TestCase):

    TEST_RESOURCE_GROUP_2_NAME = 'TestResourceGroup2'
    TEST_DEPLOYMENT_2_NAME = 'TestDeployment2'


    def setUp(self):        
        self.prepare_test_envionment(type(self).__name__)
        

    def test_stack_operations_end_to_end(self):
        self.run_all_tests()


    def __100_no_error_when_listing_resource_groups_when_no_resource_groups(self):
        self.lmbr_aws('resource-group', 'list')


    def __110_no_error_when_listing_deployments_when_no_deployments(self):
        self.lmbr_aws('deployment', 'list')


    def __120_create_project_stack(self):
        self.lmbr_aws(
            'project', 'create', 
            '--stack-name', self.TEST_PROJECT_STACK_NAME, 
            '--confirm-aws-usage',
            '--confirm-security-change', 
            '--region', lmbr_aws_test_support.REGION
        )


    def __130_verify_project_stack(self):
        
        settings = self.load_local_project_settings()
        self.assertTrue('ProjectStackId' in settings)

        project_stack_arn = settings['ProjectStackId']
        self.assertIsNotNone(project_stack_arn)

        self.verify_stack(
            "project stack", 
            project_stack_arn, 
            mock_specification.ok_project_stack()
        )


    def __150_update_project_stack(self):

        settings = self.load_local_project_settings()
        project_stack_arn = settings['ProjectStackId']

        res = self.aws_cloudformation.describe_stacks(StackName = project_stack_arn)
        timestamp_before_update = res['Stacks'][0]['LastUpdatedTime']

        self.lmbr_aws(
            'project', 'update', 
            '--confirm-aws-usage'
        )

        self.verify_stack("project stack", project_stack_arn, mock_specification.ok_project_stack())

        res = self.aws_cloudformation.describe_stacks(StackName = project_stack_arn)
        self.assertNotEqual(timestamp_before_update, res['Stacks'][0]['LastUpdatedTime'], 'update-project-stack did not update the stack')


    def __190_create_deployment_stack_when_no_resource_groups(self):
        self.lmbr_aws(
            'deployment', 'create', 
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--confirm-aws-usage', 
            '--confirm-security-change'
        )            


    def __200_verify_deployment_stack_created_when_no_resource_groups(self):

        project_stack_arn = self.get_project_stack_arn()
        deployment_stack_arn = self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME)
        deployment_access_stack_arn = self.get_deployment_access_stack_arn(self.TEST_DEPLOYMENT_NAME)
        project_resource_handler_arn = self.get_stack_resource_arn(project_stack_arn, 'ProjectResourceHandler')
        configuration_bucket_arn = self.get_stack_resource_arn(project_stack_arn, 'Configuration')

        self.verify_stack("deployment stack", deployment_stack_arn,mock_specification.ok_deployment_stack_empty())
        self.verify_stack("deployment access stack", deployment_access_stack_arn, mock_specification.ok_deployment_access_stack())

        self.verify_user_mappings(self.TEST_DEPLOYMENT_NAME, [])
        self.verify_release_mappings(self.TEST_DEPLOYMENT_NAME, [])


    def __220_list_deployments(self):
        self.lmbr_aws(
            'deployment', 'list'
        )
        self.assertIn(self.TEST_DEPLOYMENT_NAME, self.lmbr_aws_stdout)
        self.assertIn('CREATE_COMPLETE', self.lmbr_aws_stdout)


    def __240_add_resource_group(self):     
        self.lmbr_aws(
            'resource-group', 'add',
            '--resource-group', self.TEST_RESOURCE_GROUP_NAME, 
            '--include-example-resources'
        )
        self.add_bucket_to_resource_group(self.TEST_RESOURCE_GROUP_NAME, 'TestBucket1')
        self.add_bucket_to_resource_group(self.TEST_RESOURCE_GROUP_NAME, 'TestBucket2')
        resource_group_path = os.path.join(self.AWS_DIR, 'resource-group', self.TEST_RESOURCE_GROUP_NAME)
        self.assertTrue(os.path.isfile(os.path.join(resource_group_path, resource_manager.constant.RESOURCE_GROUP_TEMPLATE_FILENAME)))
        self.assertTrue(os.path.isdir(os.path.join(resource_group_path, 'lambda-code', 'SayHello')))


    def __250_list_resource_groups_no_deployment(self):
        self.lmbr_aws(
            'resource-group', 'list'
        )
        self.assertIn(self.TEST_RESOURCE_GROUP_NAME, self.lmbr_aws_stdout)


    def __270_update_deployment_stack_to_create_resource_group(self):
        self.lmbr_aws(
            'deployment', 'upload', 
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--confirm-aws-usage', 
            '--confirm-security-change'
        )


    def __280_verify_resource_group_created_by_updating_deployment_stack(self):
        self.verify_stack("deployment stack", self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME),
            {
                'StackStatus': 'UPDATE_COMPLETE',
                'StackResources': {
                    self.TEST_RESOURCE_GROUP_NAME+'Configuration': {
                        'ResourceType': 'Custom::ResourceGroupConfiguration'
                    },
                    self.TEST_RESOURCE_GROUP_NAME: {
                        'ResourceType': 'AWS::CloudFormation::Stack',
                        'StackStatus': 'CREATE_COMPLETE',
                        'StackResources': {
                            'Messages': {
                                'ResourceType': 'AWS::DynamoDB::Table'
                            },
                            'SayHelloConfiguration': {
                                'ResourceType': 'Custom::LambdaConfiguration'
                            },
                            'SayHello': {
                                'ResourceType': 'AWS::Lambda::Function',
                                'Permissions': [
                                    {
                                        'Resources': [
                                            '$Messages$'
                                        ],
                                        'Allow': [
                                            'dynamodb:PutItem'
                                        ]
                                    }
                                ]
                            },
                            'TestBucket1': {
                                'ResourceType': 'AWS::S3::Bucket'
                            },
                            'TestBucket2': {
                                'ResourceType': 'AWS::S3::Bucket'
                            },
                            'AccessControl': {
                                'ResourceType': 'Custom::AccessControl'
                            }
                        }
                    }
                }
            })

        expected_logical_ids = [ self.TEST_RESOURCE_GROUP_NAME + '.SayHello' ]
        self.verify_user_mappings(self.TEST_DEPLOYMENT_NAME, expected_logical_ids)
        self.verify_release_mappings(self.TEST_DEPLOYMENT_NAME, expected_logical_ids)


    def __290_list_resource_groups_for_deployment(self):
        self.lmbr_aws(
            'resource-group', 'list',
            '--deployment', self.TEST_DEPLOYMENT_NAME
        )
        self.assertIn(self.TEST_RESOURCE_GROUP_NAME, self.lmbr_aws_stdout)
        self.assertIn('CREATE_COMPLETE', self.lmbr_aws_stdout)


    def __310_list_resources(self):
        self.lmbr_aws(
            'deployment', 'list-resources', 
            '--deployment', self.TEST_DEPLOYMENT_NAME
        )
        self.assertIn('TestBucket1', self.lmbr_aws_stdout)
        self.assertIn('TestBucket2', self.lmbr_aws_stdout)
        self.assertIn('SayHello', self.lmbr_aws_stdout)


    def __330_update_deployment_stack_when_no_changes(self):
        self.lmbr_aws(
            'deployment', 'upload-resources', 
            '--deployment', self.TEST_DEPLOYMENT_NAME
        )


    def __410_add_empty_resource_group(self):
        self.lmbr_aws(
            'resource-group', 'add',
            '--resource-group', self.TEST_RESOURCE_GROUP_2_NAME
        )


    def __420_create_deployment_stack_with_resource_groups(self):
        self.lmbr_aws(
            'deployment', 'create',
            '--deployment', self.TEST_DEPLOYMENT_2_NAME, 
            '--confirm-aws-usage', 
            '--confirm-security-change'
        )


    def __430_verify_create_deployment_stack_with_resource_groups(self):
        self.verify_stack("deployment stack", self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_2_NAME),
            {
                'StackStatus': 'CREATE_COMPLETE',
                'StackResources': {
                    self.TEST_RESOURCE_GROUP_NAME+'Configuration': {
                        'ResourceType': 'Custom::ResourceGroupConfiguration'
                    },
                    self.TEST_RESOURCE_GROUP_NAME: {
                        'ResourceType': 'AWS::CloudFormation::Stack',
                        'StackStatus': 'CREATE_COMPLETE',
                        'StackResources': {
                            'Messages': {
                                'ResourceType': 'AWS::DynamoDB::Table'
                            },
                            'SayHelloConfiguration': {
                                'ResourceType': 'Custom::LambdaConfiguration'
                            },
                            'SayHello': {
                                'ResourceType': 'AWS::Lambda::Function',
                                'Permissions': [
                                    {
                                        'Resources': [
                                            '$Messages$'
                                        ],
                                        'Allow': [
                                            'dynamodb:PutItem'
                                        ]
                                    }
                                ]
                            },
                            'TestBucket1': {
                                'ResourceType': 'AWS::S3::Bucket'
                            },
                            'TestBucket2': {
                                'ResourceType': 'AWS::S3::Bucket'
                            },
                            'AccessControl': {
                                'ResourceType': 'Custom::AccessControl'
                            }
                        }
                    },
                    'TestResourceGroup2Configuration': {
                        'ResourceType': 'Custom::ResourceGroupConfiguration'
                    },
                    'TestResourceGroup2': {
                        'ResourceType': 'AWS::CloudFormation::Stack',
                        'StackStatus': 'CREATE_COMPLETE',
                        'StackResources': {
                            'AccessControl': {
                                'ResourceType': 'Custom::AccessControl'
                            }
                        }
                    }
                }
            })

        # check that adding the deployment didn't change the mappings, the old deployment should still be used
        expected_logical_ids = [ self.TEST_RESOURCE_GROUP_NAME + '.SayHello' ]
        self.verify_user_mappings(self.TEST_DEPLOYMENT_NAME, expected_logical_ids)
        self.verify_release_mappings(self.TEST_DEPLOYMENT_NAME, expected_logical_ids)


    def __470_add_test_bucket_to_resource_group_2(self):
        self.add_bucket_to_resource_group(self.TEST_RESOURCE_GROUP_2_NAME, 'TestBucket')


    def __480_create_test_bucket_in_resource_group_2(self):
        self.lmbr_aws(
            'resource-group', 'upload-resources', 
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--resource-group', self.TEST_RESOURCE_GROUP_2_NAME, 
            '--confirm-aws-usage'
        )
        self.lmbr_aws(
            'resource-group', 'upload-resources', 
            '--deployment', self.TEST_DEPLOYMENT_2_NAME, 
            '--resource-group', self.TEST_RESOURCE_GROUP_2_NAME, 
            '--confirm-aws-usage', 
            '--confirm-resource-deletion'
        )

        
    def __490_add_data_to_test_buckets(self):
        deployment_stack_arn = self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME)
        self.refresh_stack_resources(deployment_stack_arn)
        self.add_data_to_bucket(self.TEST_DEPLOYMENT_NAME, self.TEST_RESOURCE_GROUP_NAME, 'TestBucket1')
        self.add_data_to_bucket(self.TEST_DEPLOYMENT_NAME, self.TEST_RESOURCE_GROUP_NAME, 'TestBucket2')
        self.add_data_to_bucket(self.TEST_DEPLOYMENT_NAME, self.TEST_RESOURCE_GROUP_2_NAME, 'TestBucket')
        self.add_data_to_bucket(self.TEST_DEPLOYMENT_2_NAME, self.TEST_RESOURCE_GROUP_NAME, 'TestBucket1')
        self.add_data_to_bucket(self.TEST_DEPLOYMENT_2_NAME, self.TEST_RESOURCE_GROUP_NAME, 'TestBucket2')
        self.add_data_to_bucket(self.TEST_DEPLOYMENT_2_NAME, self.TEST_RESOURCE_GROUP_2_NAME, 'TestBucket')


    def __500_remove_bucket_resource(self):
        self.remove_resource(self.TEST_RESOURCE_GROUP_NAME, 'TestBucket1')


    def __510_update_resources_after_removing_bucket_resources(self):
        # Tests that buckets with data can be deleted when updating a resource group stack
        test_bucket_1_name = self.get_stack_resource_physical_id(
            self.get_resource_group_stack_arn(self.TEST_DEPLOYMENT_NAME, self.TEST_RESOURCE_GROUP_NAME), 
            "TestBucket1"
        )
        self.lmbr_aws(
            'resource-group', 'upload-resources', 
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--resource-group', self.TEST_RESOURCE_GROUP_NAME, 
            '--confirm-aws-usage', 
            '--confirm-resource-deletion'
        )
        self.__verify_bucket_deleted(test_bucket_1_name)


    def __515_upload_lambda_code(self):
        self.lmbr_aws(
            'function', 'upload-code',
            '--resource-group', self.TEST_RESOURCE_GROUP_NAME
        )

    def __520_remove_resource_group(self):
        self.lmbr_aws(
            'resource-group', 'remove', 
            '--resource-group', self.TEST_RESOURCE_GROUP_NAME
        )


    def __530_list_resources_after_removing_resource_group(self):
        self.lmbr_aws(
            'resource-group', 'list-resources', 
            '--resource-group', self.TEST_RESOURCE_GROUP_NAME, 
            '--deployment', self.TEST_DEPLOYMENT_NAME
        )
        self.assertIn('TestBucket2', self.lmbr_aws_stdout)
        self.assertIn('DELETE', self.lmbr_aws_stdout)


    def __540_upload_resources_after_removing_resource_group_with_resource_group_option(self):
        # Also, tests that buckets with data can be deleted when updating a deployment stack to delete a resource group stack
        test_bucket_2_name = self.get_stack_resource_physical_id(
            self.get_resource_group_stack_arn(self.TEST_DEPLOYMENT_NAME, self.TEST_RESOURCE_GROUP_NAME), 
            "TestBucket2"
        )
        self.lmbr_aws(
            'resource-group', 'upload-resources', 
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--resource-group', self.TEST_RESOURCE_GROUP_NAME, 
            '--confirm-aws-usage', 
            '--confirm-resource-deletion'
        )
        self.__verify_bucket_deleted(test_bucket_2_name)


    def __550_verify_resources_after_removing_resource_group_with_resource_group_option(self):
        self.verify_stack("deployment stack", self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME),
            {
                'StackStatus': 'UPDATE_COMPLETE',
                'StackResources': {
                    'TestResourceGroup2Configuration': {
                        'ResourceType': 'Custom::ResourceGroupConfiguration'
                    },
                    'TestResourceGroup2': {
                        'ResourceType': 'AWS::CloudFormation::Stack',
                        'StackStatus': 'CREATE_COMPLETE',
                        'StackResources': {
                            'AccessControl': {
                                'ResourceType': 'Custom::AccessControl'
                            },
                            'TestBucket': {
                                'ResourceType': 'AWS::S3::Bucket'
                            }
                        }
                    }
                }
            })


    def __560_remove_resource_group(self):
        self.lmbr_aws(
            'resource-group', 'remove', 
            '--resource-group', self.TEST_RESOURCE_GROUP_2_NAME
        )


    def __570_upload_resources_after_removing_resource_group_without_resource_group_option(self):
        # Also, tests that buckets with data can be deleted when updating a deployment stack happens to remove a resource group stack
        test_bucket_name = self.get_stack_resource_physical_id(
            self.get_resource_group_stack_arn(self.TEST_DEPLOYMENT_NAME, self.TEST_RESOURCE_GROUP_2_NAME), 
            "TestBucket"
        )
        self.lmbr_aws(
            'deployment', 'upload-resources', 
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--confirm-aws-usage', 
            '--confirm-resource-deletion'
        )
        self.__verify_bucket_deleted(test_bucket_name)


    def __580_verify_resources_after_removing_resource_group_without_resource_group_option(self):
        self.verify_stack("deployment stack", self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME),
            {
                'StackStatus': 'UPDATE_COMPLETE',
                'StackResources': {
                    'EmptyDeployment': {
                        'ResourceType': 'Custom::EmptyDeployment'
                    }
                }
            })


    def __900_delete_project_stack_while_there_are_deployments_stack(self):
        self.lmbr_aws(
            'project', 'delete', 
            '--confirm-resource-deletion',
            expect_failure=True
        )
        self.verify_stack("deployment stack", self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME),
            {
                'StackStatus': 'UPDATE_COMPLETE',
                'StackResources': {
                    'EmptyDeployment': {
                        'ResourceType': 'Custom::EmptyDeployment'
                    }
                }
            })


    def __920_delete_deployment_stack_1(self):

        deployment_stack_arn = self.get_deployment_stack_arn(self.TEST_DEPLOYMENT_NAME)

        self.lmbr_aws(
            'deployment', 'delete',
            '--deployment', self.TEST_DEPLOYMENT_NAME, 
            '--confirm-resource-deletion'
        )

        res = self.aws_cloudformation.describe_stacks(StackName = deployment_stack_arn)
        self.assertEqual(res['Stacks'][0]['StackStatus'], 'DELETE_COMPLETE')

        settings = self.load_cloud_project_settings()

        self.assertTrue('deployment' in settings)
        deployments = settings['deployment']

        self.assertFalse(self.TEST_DEPLOYMENT_NAME in deployments)


    def __930_delete_deployment_stack_2(self):
        # Also, tests that buckets with data can be deleted when deleting a deployment stack
        test_bucket_1_name = self.get_stack_resource_physical_id(
            self.get_resource_group_stack_arn(self.TEST_DEPLOYMENT_2_NAME, self.TEST_RESOURCE_GROUP_NAME), 
            "TestBucket1"
        )
        test_bucket_2_name = self.get_stack_resource_physical_id(
            self.get_resource_group_stack_arn(self.TEST_DEPLOYMENT_2_NAME, self.TEST_RESOURCE_GROUP_NAME), 
            "TestBucket2"
        )
        test_bucket_name = self.get_stack_resource_physical_id(
            self.get_resource_group_stack_arn(self.TEST_DEPLOYMENT_2_NAME, self.TEST_RESOURCE_GROUP_2_NAME), 
            "TestBucket"
        )
        self.lmbr_aws(
            'deployment', 'delete',
            '--deployment', self.TEST_DEPLOYMENT_2_NAME, 
            '--confirm-resource-deletion'
        )
        self.__verify_bucket_deleted(test_bucket_1_name)
        self.__verify_bucket_deleted(test_bucket_2_name)
        self.__verify_bucket_deleted(test_bucket_name)


    def __950_delete_project_stack(self):
        
        project_stack_arn = self.get_project_stack_arn()
        config_bucket_id = self.get_stack_resource_physical_id(project_stack_arn, 'Configuration')

        self.lmbr_aws(
            'project', 'delete', 
            '--confirm-resource-deletion'
        )

        res = self.aws_cloudformation.describe_stacks(StackName = project_stack_arn)
        self.assertEqual(res['Stacks'][0]['StackStatus'], 'DELETE_COMPLETE')

        settings = self.load_local_project_settings()

        self.assertFalse('ProjectStackId' in settings)


    def add_bucket_to_resource_group(self, resource_group_name, bucket_name):
        with self.edit_resource_group_document(resource_group_name, resource_manager.constant.RESOURCE_GROUP_TEMPLATE_FILENAME) as template:
            template['Resources'][bucket_name] = {
                'Type': 'AWS::S3::Bucket'
            }


    def add_data_to_bucket(self, deployment_name, resource_group_name, bucket_name):
        stack_arn = self.get_resource_group_stack_arn(deployment_name, resource_group_name)
        self.refresh_stack_resources(stack_arn)
        bucket_id = self.get_stack_resource_physical_id(stack_arn, bucket_name)
        print 'Writing test data to bucket {}'.format(bucket_id)
        self.aws_s3.put_object(Bucket=bucket_id, Key='TestObject', Body='TestBody')


    def remove_resource(self, resource_group_name, resource_name):
        with self.edit_resource_group_document(resource_group_name, resource_manager.constant.RESOURCE_GROUP_TEMPLATE_FILENAME) as template:
            template['Resources'].pop(resource_name, None)


    def __verify_bucket_deleted(self, bucket_name):
        try:
            res = self.aws_s3.head_bucket(Bucket=bucket_name)
            self.fail('bucket was not deleted: {}. head_bucket returned {}.'.format(bucket_name, res))
        except ClientError as e:
            self.assertEquals(e.response['Error']['Code'], '404')
