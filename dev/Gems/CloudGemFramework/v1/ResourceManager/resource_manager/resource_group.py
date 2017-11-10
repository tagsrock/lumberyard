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
# $Revision: #1 $

from errors import HandledError
import os
import util
import json
import copy
import urllib2
import constant
import time
import file_util

from zipfile import ZipFile
from StringIO import StringIO

import mappings
import project
import security
import common_code

from botocore.exceptions import NoCredentialsError

from uploader import ProjectUploader, ResourceGroupUploader, Phase

class ResourceGroup(object):

    def __init__(self, context, resource_group_name):
        '''Initialize an ResourceGroup object.'''

        self.__context = context
        self.__name = resource_group_name

        gem = self.__context.gem.get_by_name(resource_group_name)
        
        if gem:
            self.__gem = gem
            self.__directory_path = gem.aws_directory_path
            self.__game_cpp_code_path = gem.cpp_directory_path
        else: # project local resource group
            self.__gem = None
            self.__directory_path = os.path.join(self.__context.config.base_resource_group_directory_path, resource_group_name)
            self.__game_cpp_code_path = os.path.join(self.__context.config.root_directory_path, constant.PROJECT_CODE_DIRECTORY_NAME, self.__context.config.game_directory_name, constant.GEM_AWS_DIRECTORY_NAME, resource_group_name)

        self.__template_path = os.path.join(self.__directory_path, constant.RESOURCE_GROUP_TEMPLATE_FILENAME)
        self.__template = None
        self.__cli_plugin_code_path = os.path.join(self.__directory_path, 'cli-plugin-code')
        self.__cgp_code_path = os.path.join(self.__directory_path, 'cgp-resource-code')
        self.__base_settings_file_path = os.path.join(self.__directory_path, constant.RESOURCE_GROUP_SETTINGS)
        self.__game_settings_file_path = os.path.join(self.__context.config.game_directory_path, 'AWS', 'resource-group', self.__name, constant.RESOURCE_GROUP_SETTINGS)
        self.__base_settings = None
        self.__game_settings = None
        

    @property
    def name(self):
        return self.__name

    @property
    def directory_path(self):
        return self.__directory_path

    @property
    def game_cpp_code_path(self):
        return self.__game_cpp_code_path

    @property
    def is_gem(self):
        return self.__gem is not None

    @property
    def gem(self):
        return self.__gem

    def verify_gem_enabled(self):
        if self.gem:
            if not self.gem.is_defined:
                raise HandledError('The resource group {resource_group_name} relies on a gem from {gem_path} which does not exist.'.format(
                    resource_group_name = self.name, 
                    gem_path = self.gem.root_directory_path))
            if not self.gem.is_enabled:
                raise HandledError('The resource group {resource_group_name} relies on gem {gem_name} version {gem_version} (from {gem_path}) which is not enabled for the project. Please enable the gem {gem_name} in the project configurator.'.format(
                    resource_group_name = self.name, 
                    gem_name = self.gem.name,
                    gem_version = self.gem.version,
                    gem_path = self.gem.root_directory_path))

    @property
    def template_path(self):
        return self.__template_path

    @property
    def template(self):
        if self.__template is None:
            self.__template = util.load_json(self.template_path, optional=False)
        return self.__template

    def get_template_with_parameters(self, deployment_name):

        resource_group_template = self.template

        # override default parameter values if a deployment is speciified

        if deployment_name:

            project_settings = self.__context.config.project_settings
            default_resource_group_settings = project_settings.get_default_resource_group_settings()
            resource_group_settings = project_settings.get_resource_group_settings(deployment_name)

            resource_group_default_parameters = self.__find_setting(default_resource_group_settings, self.name, 'parameter')
            resource_group_parameters = self.__find_setting(resource_group_settings, self.name, 'parameter')

            if 'Parameters' in resource_group_template:
                resource_group_template_parameters = resource_group_template['Parameters']

                for paramName, paramValue in resource_group_template_parameters.iteritems():
                    newParamValue = self.__find_setting(resource_group_parameters, paramName)

                    if newParamValue == None:
                        newParamValue = self.__find_setting(resource_group_default_parameters, paramName)

                    if newParamValue != None:
                        resource_group_template_parameters[paramName]['Default'] = newParamValue

        return resource_group_template


    def __find_setting(self, dictionary, *levels):

        if dictionary == None:
            return None

        current = dictionary

        for level in levels:
            if level in current:
                current = current[level]
            else:
                return None

        return current

    def save_template(self):
        self.__context.config.save_json(self.template_path, self.template)

    @property
    def cli_plugin_code_path(self):
        return self.__cli_plugin_code_path

    @property
    def cgp_code_path(self):
        return self.__cgp_code_path

    def update_cgp_code(self, resource_group_uploader):

        content_path = self.cgp_code_path
        if not os.path.isdir(content_path):
            return

        resource_group_uploader.upload_dir(None, content_path, alternate_root=constant.GEM_CGP_DIRECTORY_NAME)

    @property
    def base_settings_file_path(self):
        return self.__base_settings_file_path

    @property
    def game_settings_file_path(self):
        return self.__game_settings_file_path

    def get_stack_id(self, deployment_name, optional = False):
        return self.__context.config.get_resource_group_stack_id(deployment_name, self.name, optional = optional)

    def get_stack_parameters(self, deployment_name, uploader = None):

        if deployment_name:
            deployment_stack_arn = self.__context.config.get_deployment_stack_id(deployment_name, optional = True)
        else:
            deployment_stack_arn = None

        return {
            'ConfigurationBucket' :  uploader.bucket if uploader else None,
            'ConfigurationKey': uploader.key if uploader else None,
            'ProjectResourceHandler': self.__context.config.project_resource_handler_id if self.__context.config.project_initialized else None,
            'DeploymentStackArn': deployment_stack_arn,
            'DeploymentName': deployment_name,
            'ResourceGroupName': self.name
        }

    def get_pending_resource_status(self, deployment_name):

        if deployment_name:
            resource_group_stack_id = self.get_stack_id(deployment_name, optional = True)
        else:
            resource_group_stack_id = None

        template = self.get_template_with_parameters(deployment_name)
        parameters = self.get_stack_parameters(deployment_name, uploader = None)

        lambda_function_content_paths = []

        resources = self.template["Resources"]
        for name, description in  resources.iteritems():

            if not description["Type"] == "AWS::Lambda::Function":
                continue

            code_path, imported_paths = ResourceGroupUploader.get_lambda_function_code_paths(self.__context, self.name, name)

            lambda_function_content_paths.append(code_path)
            lambda_function_content_paths.extend(imported_paths)

        # TODO: need to support swagger.json IN the lambda directory.
        service_api_content_paths = [ os.path.join(self.directory_path, 'swagger.json') ]

        # TODO: get_pending_resource_status's new_content_paths parameter needs to support
        # a per-resource mapping instead of an per-type mapping. As is, a change in any lambda
        # directory makes all lambdas look like they need to be updated.
        return self.__context.stack.get_pending_resource_status(
            resource_group_stack_id, 
            new_template = template,
            new_parameter_values = parameters,
            new_content_paths = {
                'AWS::Lambda::Function': lambda_function_content_paths,
                'Custom::ServiceApi': service_api_content_paths
            }
        )


    def add_output(self, logical_id, description, value, force=False):
        '''Adds an output to a resource group's resource-template.json file.

        Args:

            logical_id: the name of the output

            description: a description of the output

            value: the output value. May be of the form { "Ref": "..." } or
            { "Gn::GetAtt": [ "...", "..." ] } or any other construct allowed
            by Cloud Formation.

            force (named): Determine if existing definitions are replaced. Default
            is False.

        Returns:

            True if a change was made.
        '''
        changed = False

        outputs = util.dict_get_or_add(self.template, 'Outputs', {})
        if logical_id not in outputs or force:
            self.__context.view.adding_output(self.template_path, logical_id)
            outputs[logical_id] = {
                'Description': description,
                'Value': value
            }
            changed = True
        else:
            self.__context.view.output_exists(self.template_path, logical_id)

        return changed


    def remove_output(self, logical_id):
        '''Removes an output to a resource group's resource-template.json file.

        Args:

            logical_id: the name of the output

        Returns:

            True if a change was made.
        '''
        changed = False

        outputs = util.dict_get_or_add(self.template, 'Outputs', {})
        if logical_id in outputs:
            self.__context.view.removing_output(self.template_path, logical_id)
            del outputs[logical_id]
            changed = True
        else:
            self.__context.view.output_not_found(self.template_path, logical_id)

        return changed



    def add_resources(self, resource_definitions, force=False, dependencies=None):
        '''Adds resource definitions to a resource group's resource-template.json file.

        Args:

            resource_definitions: dictionary containing resource definitions.

            force (named): indicates if resource and parameter definitions replace existing definitions. Default is False.

            dependencies (named): a dictionary that provides updates to the DepondsOn property of existing resources:

              {
                  '<dependent-resource-name>': [ '<dependency-resource-name>', ... ],
                  ...
              }

            resutls in:

                "<dependent-resource-name>": {
                   "DependsOn": [ "<dependency-resource-name>" ]
                   ...
                }

        Returns:

            True if any definitions were added.
        '''

        changed = False

        if dependencies is None:
            dependencies = {}

        resources = util.dict_get_or_add(self.template, 'Resources', {})
        for resource_name, resource_definition in resource_definitions.iteritems():
            if resource_name in resources and not force:
                self.__context.view.resource_exists(self.template_path, resource_name)
            else:
                self.__context.view.adding_resource(self.template_path, resource_name)
                resources[resource_name] = resource_definition
                if self.__has_access_control_metadata(resource_definition):
                    dependency_list = dependencies.setdefault('AccessControl', [])
                    dependency_list.append(resource_name)
                changed = True

        if dependencies:

            for dependent_name, dependency_list in dependencies.iteritems():

                if dependent_name == 'AccessControl':
                    dependent_definition = resources.setdefault('AccessControl', security.DEFAULT_ACCESS_CONTROL_RESOURCE_DEFINITION)
                else:
                    dependent_definition = resources.get(dependent_name)
                    if dependent_definition is None:
                        raise ValueError('The dependent resource {} does not exist.'.format(dependent_name))

            dependencies = dependent_definition.setdefault('DependsOn', [])
            if not isinstance(dependencies, type([])):
                dependencies = [ dependencies ]
                dependent_definition['DependsOn'] = dependencies

            if not isinstance(dependency_list, type([])):
                dependency_list = [ dependency_list ]

            dependencies.extend(set(dependency_list))

        return changed

    def __has_access_control_metadata(self, resource_definition):
        return util.get_cloud_canvas_metadata(resource_definition, 'Permissions') or util.get_cloud_canvas_metadata(resource_definition, 'RoleMappings')

    def remove_resources(self, resource_names):
        '''Removes resource definitions from a resource group's resource-template.json file.

        Args:

            resource_names: list containing resource names.

        Returns:

            True if any definitions were removed.
        '''

        changed = False

        resources = util.dict_get_or_add(self.template, 'Resources', {})
        for resource_name in resource_names:
            if resource_name not in resources:
                self.__context.view.resource_not_found(self.template_path, resource_name)
            else:
                self.__context.view.removing_resource(self.template_path, resource_name)
                del resources[resource_name]
                changed = True

        for resource_definition in resources.values():
            depends_on = resource_definition.get('DependsOn')
            if depends_on:
                if isinstance(depends_on, type([])):
                    for resource_name in resource_names:
                        while resource_name in depends_on:
                            depends_on.remove(resource_name)
                            changed = True
                else:
                    if depends_on in resource_names:
                        resource_definition['DependsOn'] = []
                        changed = True


        return changed


    def add_parameters(self, parameter_definitions, force=False):
        '''Adds resource and parameter definitions to a resource group's resource-template.json file.

        Args:

            parameter_definitions: dictionary containing parameter definitions.

            force (named): indicates if resource and parameter definitions replace existing definitions. Default is False.

        Returns:

            True if any definitions were added.
        '''

        changed = False

        parameters = util.dict_get_or_add(self.template, 'Parameters', {})
        for parameter_name, parameter_definition in parameter_definitions.iteritems():
            if parameter_name in parameters and not force:
                self.__context.view.parameter_exists(self.template_path, parameter_name)
            else:
                self.__context.view.adding_parameter(self.template_path, parameter_name)
                parameters[parameter_name] = parameter_definition
                changed = True

        return changed


    def remove_parameters(self, parameter_names):
        '''Removes resource and parameter definitions from a resource group's resource-template.json file.

        Args:

            parameter_names: list containing parameter names.

        Returns:

            True if any definitions were removed.
        '''

        changed = False

        parameters = util.dict_get_or_add(self.template, 'Parameters', {})
        for parameter_name in parameter_names:
            if parameter_name not in parameters:
                self.__context.view.parameter_not_found(self.template_path, parameter_name)
            else:
                self.__context.view.removing_parameter(self.template_path, parameter_name)
                del parameters[parameter_name]
                changed = True

        return changed


    def copy_directory(self, source_path, relative_destination_path = '.', force=False):
        '''Adds a copy of the contents of a directory to a resource group. Subdirectories are recursively merged.

        Arguments:

            source_path: the directory to copy.

            relative_destination_path (named): the name of the resource group relative directory
            where the source directory contents will be copied. Defaults to the resource group
            directory itself.

            force (named): if True, overwrite destination files that already exists.
            The default is False.

        '''

        destination_path = os.path.abspath(os.path.join(self.directory_path, relative_destination_path))

        file_util.copy_directory_content(self.__context, destination_path, source_path, overwrite_existing = force)


    def copy_file(self, source_path, relative_destination_path, force=False):
        '''Adds a copy of a file to a resource group.

        Arguments:

            source_path - path and name of the file to copy.

            relative_destination_path - path and name of the destination file, relative to the
            resource group directory.

            force (named) - if True, existing files will be overwitten. Default is False.

        '''

        destination_path = os.path.abspath(os.path.join(self.directory_path, relative_destination_path))

        file_util.copy_file(self.__context, destination_path, source_path, everwrite_existing = force)


    def create_file(self, relative_destination_path, initial_content, force=False):
        '''Creates a file in a resource group.

        Args:

            relative_destination_path: the path and name of the file relative to
            the resource group directory.

            initial_content: The file's initial content.

            force (named): Overwite existing files. Default is False.

        Returns:

            True if the file was created.
        '''

        destination_path = os.path.join(self.directory_path, relative_destination_path)

        return file_util.create_ignore_filter_function(self.__context, destination_path, initial_content, overwrite_existing = force)


    def get_editor_setting(self, setting_name, preference = 'game_or_base'):

        if self.__base_settings is None:
            self.__base_settings = self.__context.config.load_json(self.__base_settings_file_path)

        if self.__game_settings is None:
            self.__game_settings = self.__context.config.load_json(self.__game_settings_file_path)

        setting = None
        if preference == 'game_or_base':
            setting = self.__game_settings.get(setting_name)
            if setting is None:
                setting = self.__base_settings.get(setting_name)
        elif preference == 'base_or_game':
            setting = self.__base_settings.get(setting_name)
            if setting is None:
                setting = self.__game_settings.get(setting_name)
        elif preference == 'base':
            setting = self.__base_settings.get(setting_name)
        elif preference == 'game':
            setting = self.__game_settings.get(setting_name)

        return setting

def add(context, args):

    if args.resource_group in context.resource_groups:
        raise HandledError('The project already has a {} resource group.'.format(args.resource_group))

    util.validate_stack_name(args.resource_group)

    context.config.validate_writable(context.config.local_project_settings.path)

    # Get resource group path. If --gem was specified, it identifies the directory
    # that contains the gem which defines the resource group. 
    if args.gem is not None:

        if args.gem == '': # --gem was provided with no argument
            gem_path = os.path.join('Gems', args.resource_group)
        else: # --gem PATH was provided
            gem_path = args.gem

        gem_path = os.path.abspath(os.path.join(context.config.root_directory_path, gem_path))
        gem = context.gem.get_by_root_directory_path(gem_path)

        if gem is None:
            raise HandledError('The gem at {} is not enabled for the project or does not exist.'.format(gem_path))

        # is OK If it doesn't exist, it will be populated below (same as for 
        # project local resource groups)
        resource_group_path = gem.aws_directory_path

        # Only include gem name in metrics for the gems that ship with Lumberyard.
        if args.resource_group in ['CloudGemFramework', 'CloudGemDynamicContent', 'CloudGemMessageOfTheDay', 'CloudGemLeaderboard', 'CloudGemPlayerAccount']:
            context.metrics.add_attribute('gem', args.resource_group)

    else:

        resource_group_path = os.path.join(context.config.base_resource_group_directory_path, args.resource_group)

    if args.include_example_resources:
        context.config.copy_example_resource_group_content(resource_group_path)
    else:
        context.config.copy_default_resource_group_content(resource_group_path)

    resource_group_list = context.config.local_project_settings.setdefault(constant.ENABLED_RESOURCE_GROUPS_KEY, [])
    if args.resource_group not in resource_group_list:
        resource_group_list.append(args.resource_group)
        context.config.local_project_settings.save()

    context.view.resource_group_added(args.resource_group)


def remove(context, args):

    resource_group = context.resource_groups.get(args.resource_group)

    context.config.validate_writable(context.config.local_project_settings.path)

    resource_group_list = context.config.local_project_settings.setdefault(constant.ENABLED_RESOURCE_GROUPS_KEY, [])
    if args.resource_group in resource_group_list:
        resource_group_list.remove(args.resource_group)
        context.config.local_project_settings.save()

    context.view.resource_group_removed(args.resource_group)


def update_stack(context, args):

    deployment_name = args.deployment
    resource_group_name = args.resource_group

    # Use default deployment if necessary
    if deployment_name is None:
        if context.config.default_deployment is None:
            raise HandledError('No default deployment has been set. Provide the --deployment parameter or use the default-deployment command to set a default deployment.')
        deployment_name = context.config.default_deployment

    # Get needed data, verifies the resource group stack exists

    resource_group = context.resource_groups.get(resource_group_name)
    resource_group_stack_id = resource_group.get_stack_id(deployment_name)
    pending_resource_status = resource_group.get_pending_resource_status(deployment_name)

    # Is it ok to do this?

    capabilities = context.stack.confirm_stack_operation(
        resource_group_stack_id,
        'deployment {} resource group {}'.format(deployment_name, resource_group_name),
        args,
        pending_resource_status
    )

    # Update the stack...

    project_uploader = ProjectUploader(context)
    deployment_uploader = project_uploader.get_deployment_uploader(deployment_name)

    resource_group_uploader, resource_group_template_url = before_update(
        deployment_uploader, 
        resource_group_name
    )

    parameters = resource_group.get_stack_parameters(
        deployment_name, 
        uploader = resource_group_uploader
    )

    # wait a bit for S3 to help insure that templates can be read by cloud formation
    time.sleep(constant.STACK_UPDATE_DELAY_TIME)

    context.stack.update(
        resource_group_stack_id, 
        resource_group_template_url, 
        parameters = parameters, 
        pending_resource_status = pending_resource_status,
        capabilities = capabilities
    )

    after_update(deployment_uploader, resource_group_name)

    # Deprecated in 1.9 - TODO remove
    context.hooks.call_module_handlers('cli-plugin-code/resource_group_hooks.py', 'on_post_update', 
        args=[deployment_name, resource_group_name], 
        deprecated=True
    )

    # Update mappings...

    if deployment_name == context.config.default_deployment:
        mappings.update(context, util.Args())

    if deployment_name == context.config.release_deployment:
        temp_args = util.Args()
        temp_args.release = True
        mappings.update(context, temp_args)


def update_lambda_code(context, args):
    # Has the project been initialized?
    if not context.config.project_initialized:
        raise HandledError('The project has not been initialized.')

    # The deployment exists
    if args.deployment not in context.config.deployment_names:
        HandledError('There is no {} deployment.'.format(args.deployment))

    # The resource group exists?
    context.resource_groups.get(args.resource_group)

    deployment_name = args.deployment
    resource_group_name = args.resource_group
    function_name = args.function

    project_uploader = ProjectUploader(context)
    deployment_uploader = project_uploader.get_deployment_uploader(deployment_name)
    resource_group_uploader = deployment_uploader.get_resource_group_uploader(resource_group_name)

    #Create a client representing the lambda resource
    deployment_stack_id = context.config.get_deployment_stack_id(args.deployment)
    resource_region = util.get_region_from_arn(deployment_stack_id)
    client = context.aws.client('lambda', region = resource_region)

    #Get the resources description in the stack
    resource_group_stack_id = context.config.get_resource_group_stack_id(deployment_name, resource_group_name)
    resources = context.stack.describe_resources(resource_group_stack_id)

    lambda_function_descriptions = []
    if function_name:
        description = resources.get(function_name, None)
        if description is not None and description['ResourceType'] == 'AWS::Lambda::Function':
            lambda_function_descriptions.append(description)
        else:
            raise HandledError('Lambda function {} does not exist.'.format(function_name))
    else:
        #If the function name isn't given, find the descriptions for all the Lambda functions
        for logical_name, description in resources.iteritems():
            if description['ResourceType'] == 'AWS::Lambda::Function':
                lambda_function_descriptions.append(description)

    for lambda_function_description in lambda_function_descriptions:
        __update_function_code(context, resource_group_uploader, lambda_function_description, client)


def __update_function_code(context, resource_group_uploader, lambda_function_description, client):
    
    # get settings content
    settings_path, settings_content = __get_settings_content(context, client, lambda_function_description)
    aggregated_content = {}
    if settings_path:
        aggregated_content[settings_path] = settings_content

    # zip and send it to s3 in preparation for lambdas
    function_name = lambda_function_description['LogicalResourceId']
    key = resource_group_uploader.zip_and_upload_lambda_function_code(function_name, aggregated_content = aggregated_content)

    # update the lambda function
    client.update_function_code(FunctionName = lambda_function_description['PhysicalResourceId'], S3Bucket = resource_group_uploader.bucket, S3Key = key)


def create_stack(context, args):

    # Does a "safe" create of a resource group stack. The existing deployment
    # template is modified to add the stack and config resources and used
    # to update the deployment stack. This prevents unexpected changes to other
    # resource groups as a side effect of the deployment update.

    resource_group = context.resource_groups.get(args.resource_group)
    pending_resource_status = resource_group.get_pending_resource_status(args.deployment)

    # Is it ok to do this?

    capabilities = context.stack.confirm_stack_operation(
        None, # stack id
        'deployment {} resource group {}'.format(args.deployment, args.resource_group),
        args,
        pending_resource_status
    )

    # Do the create...

    project_uploader = ProjectUploader(context)
    deployment_uploader = project_uploader.get_deployment_uploader(args.deployment)

    before_update(
        deployment_uploader, 
        args.resource_group
    )

    context.view.processing_template('{} deployment'.format(args.deployment))

    deployment_stack_id = context.config.get_deployment_stack_id(args.deployment)
    deployment_template = context.stack.get_current_template(deployment_stack_id)
    deployment_parameters = context.stack.get_current_parameters(deployment_stack_id)

    deployment_resources = deployment_template.get('Resources', {})

    effective_deployment_resources = context.config.deployment_template_aggregator.effective_template.get('Resources',{})

    resource_group_stack_resource = deployment_resources.get(args.resource_group, None)
    if resource_group_stack_resource is None:
        resource_group_stack_resource = copy.deepcopy(effective_deployment_resources.get(args.resource_group, {}))
        deployment_resources[args.resource_group] = resource_group_stack_resource

    resource_group_config_name = args.resource_group + 'Configuration'
    resource_group_config_resource = deployment_resources.get(resource_group_config_name, None)
    if resource_group_config_resource is None:
        resource_group_config_resource = copy.deepcopy(effective_deployment_resources.get(resource_group_config_name, {}))
        resource_group_config_resource.get('Properties', {})['ConfigurationKey'] = deployment_uploader.key
        deployment_resources[resource_group_config_name] = resource_group_config_resource

    if 'EmptyDeployment' in deployment_resources:
        del deployment_resources['EmptyDeployment']

    deployment_template_url = deployment_uploader.upload_content(constant.DEPLOYMENT_TEMPLATE_FILENAME, json.dumps(deployment_template),
                                                                 'deployment template with resource group definitions')

    # wait a bit for S3 to help insure that templates can be read by cloud formation
    time.sleep(constant.STACK_UPDATE_DELAY_TIME)

    try:
        context.stack.update(
            deployment_stack_id, 
            deployment_template_url, 
            deployment_parameters,
            pending_resource_status = __nest_pending_resource_status(args.deployment, pending_resource_status),
            capabilities = capabilities
        )
    except:
        context.config.force_gui_refresh()
        raise

    context.config.force_gui_refresh()

    context.view.resource_group_stack_created(args.deployment, args.resource_group)

    after_update(deployment_uploader, args.resource_group)

    # Deprecated in 1.9 - TODO remove
    context.hooks.call_module_handlers('cli-plugin-code/resource_group_hooks.py', 'on_post_update', 
        args=[args.deployment, args.resource_group], 
        deprecated=True
    )



def delete_stack(context, args):

    resource_group_stack_id = context.config.get_resource_group_stack_id(args.deployment, args.resource_group)
    pending_resource_status = context.stack.get_pending_resource_status(
        resource_group_stack_id, 
        new_template = {}
    )

    # Is it ok to do this?

    capabilities = context.stack.confirm_stack_operation(
        None, # stack id
        'deployment {} resource group {}'.format(args.deployment, args.resource_group),
        args,
        pending_resource_status
    )

    # Does a "safe" delete of a resource group stack. The existing deployment
    # template is modified to remove the stack and config resources and used
    # to update the deployment. This prevents unexpected changes to other resource
    # groups as a side effect of the deployment update.

    project_uploader = ProjectUploader(context)
    deployment_uploader = project_uploader.get_deployment_uploader(args.deployment)

    context.view.processing_template('{} deployment'.format(args.deployment))

    deployment_stack_id = context.config.get_deployment_stack_id(args.deployment)
    deployment_template = context.stack.get_current_template(deployment_stack_id)
    deployment_parameters = context.stack.get_current_parameters(deployment_stack_id)

    deployment_resources = deployment_template.get('Resources', {})

    resource_group_stack_resource = deployment_resources.get(args.resource_group, None)
    if resource_group_stack_resource is not None:
        del deployment_resources[args.resource_group]

    resource_group_config_resource = deployment_resources.get(args.resource_group + 'Configuration', None)
    if resource_group_config_resource is not None:
        del deployment_resources[args.resource_group + 'Configuration']

    if resource_group_stack_resource is None and resource_group_config_resource is None:
        raise HandledError('Definitions for {} resource group related resources where not found in the current {} deployment template.'.format(args.resource_group, args.deployment))

    if not deployment_resources:
        deployment_resources['EmptyDeployment'] = {
            "Type": "Custom::EmptyDeployment",
            "Properties": {
                "ServiceToken": { "Ref": "ProjectResourceHandler" }
            }
        }

    deployment_template_url = deployment_uploader.upload_content(constant.DEPLOYMENT_TEMPLATE_FILENAME, json.dumps(deployment_template),
                                                                 'deployment template without resource group definitions')

    resource_group_stack_id = context.stack.get_physical_resource_id(deployment_stack_id, args.resource_group)

    # wait a bit for S3 to help insure that templates can be read by cloud formation
    time.sleep(constant.STACK_UPDATE_DELAY_TIME)

    # Tell stack.update that a child stack is being deleted so that it
    # cleans up any resources that stack contains.
    pending_resource_status = {
        args.resource_group: {
            'OldDefinition': {
                'Type': 'AWS::CloudFormation::Stack'
            },
            'PendingAction': context.stack.PENDING_DELETE
        }
    }

    try:
        context.stack.update(
            deployment_stack_id, 
            deployment_template_url, 
            deployment_parameters, 
            pending_resource_status = pending_resource_status,
            capabilities = capabilities
        )
    except:
        context.config.force_gui_refresh()
        raise

    context.config.force_gui_refresh()

    context.view.resource_group_stack_deleted(args.deployment, args.resource_group)


def __nest_pending_resource_status(deployment_name, pending_resource_status):
    return { deployment_name + '.' + k:v for k,v in pending_resource_status.iteritems() }


def before_update(deployment_uploader, resource_group_name):

    context = deployment_uploader.context
    deployment_name = deployment_uploader.deployment_name

    resource_group_uploader = deployment_uploader.get_resource_group_uploader(resource_group_name)

    group = context.resource_groups.get(resource_group_name)

    context.view.processing_template('{} resoruce group'.format(resource_group_name))

    resource_group_template_with_parameters = group.get_template_with_parameters(deployment_name)

    resource_group_template_url = resource_group_uploader.upload_content(
        constant.RESOURCE_GROUP_TEMPLATE_FILENAME,
        json.dumps(resource_group_template_with_parameters, indent=4, sort_keys=True),
        'processed resource group template')

    __zip_individual_lambda_code_folders(group, resource_group_uploader)

    # Deprecated in 1.9. TODO: remove.
    resource_group_uploader.execute_uploader_pre_hooks()

    context.hooks.call_single_module_handler('resource-manager-code/update.py', 'before_this_resource_group_updated', resource_group_name,
        kwargs = {
            'deployment_name': deployment_name, 
            'resource_group_name': resource_group_name, 
            'resource_group_uploader': resource_group_uploader
        }
    )

    context.hooks.call_module_handlers('resource-manager-code/update.py', 'before_resource_group_updated', 
        kwargs = {
            'deployment_name': deployment_name, 
            'resource_group_name': resource_group_name, 
            'resource_group_uploader': resource_group_uploader
        }
    )

    return (resource_group_uploader, resource_group_template_url)


def after_update(deployment_uploader, resource_group_name):

    context = deployment_uploader.context
    deployment_name = deployment_uploader.deployment_name
    group = context.resource_groups.get(resource_group_name)
    resource_group_uploader = deployment_uploader.get_resource_group_uploader(resource_group_name)
    group.update_cgp_code(resource_group_uploader)

    # Deprecated in 1.9 - TODO remove
    resource_group_uploader.execute_uploader_post_hooks()

    context.hooks.call_single_module_handler('resource-manager-code/update.py', 'after_this_resource_group_updated', resource_group_name,
        kwargs = {
            'deployment_name': deployment_name, 
            'resource_group_name': resource_group_name, 
            'resource_group_uploader': resource_group_uploader
        }
    )

    context.hooks.call_module_handlers('resource-manager-code/update.py', 'after_resource_group_updated', 
        kwargs = {
            'deployment_name': deployment_name, 
            'resource_group_name': resource_group_name, 
            'resource_group_uploader': resource_group_uploader
        }
    )


def __zip_individual_lambda_code_folders(group, uploader):

    resources = group.template["Resources"]
    for name, description in  resources.iteritems():

        if not description["Type"] == "AWS::Lambda::Function":
            continue

        uploader.zip_and_upload_lambda_function_code(name)


def __get_settings_content(context, client, lambda_function_description):

    settings_name = None
    settings_content = None

    context.view.downloading(lambda_function_description['LogicalResourceId'] + ' Lambda Function code to retrieve current settings')

    res = client.get_function(FunctionName=lambda_function_description['PhysicalResourceId'])
    location = res.get('Code', {}).get('Location', None)
    if location:

        zip_content = urllib2.urlopen(location)
        zip_file = ZipFile(StringIO(zip_content.read()), 'r')

        for name in zip_file.namelist():
            if name in ['CloudCanvas/settings.py', 'CloudCanvas/settings.js']:
                settings_name = name
                settings_content = zip_file.open(settings_name, 'r').read()
                break

    return settings_name, settings_content


def list(context, args):

    resource_groups = []

    for group in context.resource_groups.values():
        resource_group_description = {
                'Name': group.name,
                'ResourceGroupTemplateFilePath': group.template_path,
                'CliPluginCodeDirectoryPath': group.cli_plugin_code_path,
                'CGPResourceCodePath': group.cgp_code_path,
                'BaseSettingsFilePath': group.base_settings_file_path,
                'GameSettingsFilePath': group.game_settings_file_path
            }
        resource_group_description['LambdaFunctionCodeDirectoryPaths'] = __gather_additional_code_directories(context, group)

        resource_groups.append(resource_group_description)

    stack_checked = False
    deployment_name = None
    if context.config.project_initialized:

        deployment_name = args.deployment or context.config.default_deployment
        if deployment_name is not None:

            deployment_stack_id = context.config.get_deployment_stack_id(deployment_name)

            try:
                resources = context.stack.describe_resources(deployment_stack_id, recursive=False)
            except NoCredentialsError:
                resources = {}
                pass

            for resource_group in resource_groups:
                resource = resources.get(resource_group['Name'], None)
                if resource is None:
                    resource = {
                        'ResourceStatus': '',
                        'PendingAction': context.stack.PENDING_CREATE,
                        'PendingReason': context.stack.PENDING_CREATE_REASON
                    }
                resource_group.update(resource)

            # find stack resources in deployment stack that don't exist in the template
            for name, resource in resources.iteritems():
                if resource['ResourceType'] == 'AWS::CloudFormation::Stack':
                    found = False
                    for resource_group in resource_groups:
                        if resource_group['Name'] == name:
                            found = True
                            break
                    if not found:
                        resource['Name'] = name
                        resource.update(
                            {
                                'Name': name,
                                'PendingAction': context.stack.PENDING_DELETE,
                                'PendingReason': context.stack.PENDING_DELETE_REASON
                            }
                        )
                        resource_groups.append(resource)

            stack_checked = True

    if not stack_checked:
        for resource_group in resource_groups:
            resource = {
                'ResourceStatus': '',
                'PendingAction': context.stack.PENDING_CREATE,
                'PendingReason': context.stack.PENDING_CREATE_REASON
            }
            resource_group.update(resource)

    context.view.resource_group_list(deployment_name, resource_groups)

def __gather_additional_code_directories(context, group):
    
    additional_dirs = []

    # do any individual folders exist?
    for name, description in group.template.get("Resources", {}).iteritems():

        if description == None: # This can happen with a malformed template
            continue

        if not description.get("Type", "") == "AWS::Lambda::Function":
            continue
        
        code_path = ResourceGroupUploader.get_lambda_function_code_path(context, group.name, name)
        additional_dirs.append(code_path)

    # TODO: should this list include common-code directories as well?

    return additional_dirs

def describe_stack(context, args):

    stack_id = context.config.get_resource_group_stack_id(args.deployment, args.resource_group, optional=True)

    group = context.resource_groups.get(args.resource_group, optional=True)
    if(stack_id is None):
        stack_description = {
            'StackStatus': '',
            'PendingAction': context.stack.PENDING_CREATE,
            'PendingReason': context.stack.PENDING_CREATE_REASON
        }
    else:
        stack_description = context.stack.describe_stack(stack_id)

        if not group:
            stack_description.update(
                {
                    'PendingAction': context.stack.PENDING_DELETE,
                    'PendingReason': context.stack.PENDING_DELETE_REASON
                }
            )

    user_defined_resource_count = 0

    this_template = {}
    if group:
        this_template = group.template

    for key, resource in this_template.get('Resources', {}).iteritems():
        if key != 'AccessControl':
            user_defined_resource_count += 1

    context.view.resource_group_stack_description(args.deployment, args.resource_group, stack_description, user_defined_resource_count)


def list_parameters(context, args):

    if not context.config.project_initialized:
        raise HandledError('A project stack must be created before parameters can be listed.')

    project_settings = context.config.project_settings

    parameters = []

    for deployment_name, deployment_settings in project_settings.get('deployment', {}).iteritems():
        if not args.deployment or deployment_name == args.deployment or deployment_name == '*':
            for resource_group_name, resource_group_settings in deployment_settings.get('resource-group', {}).iteritems():
                if not args.resource_group or resource_group_name == args.resource_group or resource_group_name == '*':
                    for parameter_name, parameter_value in resource_group_settings.get('parameter', {}).iteritems():
                        if not args.parameter or parameter_name == args.parameter:
                            parameters.append(
                                {
                                    'parameter_name': parameter_name,
                                    'parameter_value': parameter_value,
                                    'deployment_name': deployment_name,
                                    'resource_group_name': resource_group_name
                                })

    context.view.parameter_list(parameters)

def set_parameter(context, args):

    if not context.config.project_initialized:
        raise HandledError('A project stack must be created before parameters can be listed.')

    if args.deployment != '*' and args.deployment not in context.config.deployment_names:
        context.view.no_such_deployment_parameter_warning(args.deployment)

    if args.resource_group != '*' and args.resource_group not in context.resource_groups:
        context.view.no_such_resource_group_parameter_warning(args.resource_group)

    project_settings = context.config.project_settings
    deployment_settings = project_settings.setdefault('deployment', {}).setdefault(args.deployment, {})
    resource_group_settings = deployment_settings.setdefault('resource-group', {}).setdefault(args.resource_group, {})
    parameters = resource_group_settings.setdefault('parameter', {})
    old_value = parameters.get(args.parameter, None)
    parameters[args.parameter] = args.value

    context.view.parameter_changed(args.deployment, args.resource_group, args.parameter, args.value, old_value)

    context.config.save_project_settings()


def clear_parameter(context, args):

    if not context.config.project_initialized:
        raise HandledError('A project stack must be created before parameters can be listed.')

    project_settings = context.config.project_settings

    change_list = []
    for deployment_name, deployment_settings in project_settings.get('deployment', {}).iteritems():
        if not args.deployment or deployment_name == args.deployment:
            for resource_group_name, resource_group_settings in deployment_settings.get('resource-group', {}).iteritems():
                if not args.resource_group or resource_group_name == args.resource_group:
                    parameters = resource_group_settings.get('parameter', {})
                    if args.parameter in parameters:
                        change_list.append(
                            {
                                'deployment_name': deployment_name,
                                'resource_group_name': resource_group_name,
                                'parameter_name': args.parameter,
                                'parameter_value': parameters[args.parameter]
                            })

    if change_list:
        ok = context.view.confirm_parameter_clear(change_list, args.confirm_clear)
        if ok:
            for change in change_list:
                deployment_settings = project_settings.get('deployment', {}).get(change['deployment_name'], None)
                if deployment_settings:
                    resource_group_settings = deployment_settings.get('resource-group', {}).get(change['resource_group_name'], None)
                    if resource_group_settings:
                        parameters = resource_group_settings.get('parameter', {})
                        if change['parameter_name'] in parameters:
                            del parameters[change['parameter_name']]
            context.config.save_project_settings()
    else:
        context.view.parameter_not_found(args.deployment, args.resource_group, args.parameter)


def list_resource_group_resources(context, args):

    deployment_name = args.deployment
    resource_group_name = args.resource_group

    if deployment_name is None:
        deployment_name = context.config.default_deployment

    resource_group = context.resource_groups.get(resource_group_name, optional = True)
    if resource_group:

        if deployment_name:
            resource_group_stack_id = resource_group.get_stack_id(deployment_name, optional=True)
        else:
            resource_group_stack_id = None

        pending_resource_status = resource_group.get_pending_resource_status(deployment_name)

    else:

        # resource group may have been removed but there is still a stack

        if deployment_name:
            resource_group_stack_id = context.config.get_resource_group_stack_id(deployment_name, resource_group_name, optional=True) 
        else:
            resource_group_stack_id = None
        
        if not resource_group_stack_id:
            raise HandledError('The resource group {} does not exist.'.format(resource_group_name))

        pending_resource_status = context.stack.get_pending_resource_status(
            resource_group_stack_id, 
            new_template = {} # resource status will be pending DELETE
        )

    context.view.resource_group_resource_list(
        resource_group_stack_id, 
        deployment_name, 
        resource_group_name, 
        pending_resource_status
    )

def add_player_access(context, args):
    # Add player access to the resource permissions
    security.add_permission_to_role(context, args.resource_group, args.resource, 'Player', args.action)

    # Add the dependency to access control resource
    group = context.resource_groups.get(args.resource_group)
    if security.ensure_access_control(group.template, args.resource):
        context.config.save_resource_group_template(args.resource_group)
        context.view.access_control_dependency_changed(args.resource_group, args.resource)


def create_function_folder(context, args):
    group = context.resource_groups.get(args.resource_group)
    function_path = os.path.join(group.directory_path, 'lambda-code', args.function)
    if not os.path.exists(function_path):
        # if function folder does not already exist add it
        context.config.copy_default_lambda_code_content(function_path)
