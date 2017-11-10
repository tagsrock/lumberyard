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
import os
import subprocess
import sys
import argparse
import xml.etree.ElementTree as ET

from waflib.Tools import c_aliases, c

from waflib import Logs, Options, Utils, Configure, ConfigSet
from waflib.Configure import conf, ConfigurationContext
from waflib.Build import BuildContext, CleanContext, Context

from waf_branch_spec import SUBFOLDERS, VERSION_NUMBER_PATTERN, PLATFORM_CONFIGURATION_FILTER, PLATFORMS, CONFIGURATIONS

UNSUPPORTED_PLATFORM_CONFIGURATIONS = set()

CURRENT_WAF_EXECUTABLE = '{0} ./Tools/build/waf-1.7.13/lmbr_waf'.format(os.path.relpath(sys.executable))

WAF_TOOL_ROOT_DIR = 'Tools/build/waf-1.7.13'
LMBR_WAF_TOOL_DIR = WAF_TOOL_ROOT_DIR + '/lmbrwaflib'
GENERAL_WAF_TOOL_DIR = WAF_TOOL_ROOT_DIR + '/waflib'

# some files write a corresponding .timestamp file in BinTemp/ as part of the configuration process
TIMESTAMP_CHECK_FILES = ['SetupAssistantConfig.json']
# additional files for android builds
ANDROID_TIMESTAMP_CHECK_FILES = ['_WAF_/android/android_settings.json']
# successful configure generates these timestamps
CONFIGURE_TIMESTAMP_FILES = ['SetupAssistantConfig.timestamp', 'android_settings.timestamp']
# if configure is run explicitly, delete these files to force them to rerun
CONFIGURE_FORCE_TIMESTAMP_FILES = ['generate_uber_files.timestamp', 'project_gen.timestamp']

# List of extra non-build (non project building) commands.
NON_BUILD_COMMANDS = [
    'generate_uber_files',
    'generate_module_def_files',
    'generate_game_project',
    'msvs',
    'eclipse',
    'android_studio',
    'xcode_ios',
    'xcode_appletv',
    'xcode_mac'
]


# Table of lmbr waf modules that need to be loaded by the root waf script, grouped by tooldir where the modules need to be loaded from.
# The module can be restricted to an un-versioned system platform value (win32, darwin, linux) by adding a ':' + the un-versioned system platform value.
# The '.' key represents the default waf tooldir (Code/Tools/waf-1.7.13/waflib)
LMBR_WAFLIB_MODULES = {

GENERAL_WAF_TOOL_DIR : [
        'c_config',
        'c',
        'cxx',
        'c_osx:darwin'
    ],
LMBR_WAF_TOOL_DIR : [
        'cry_utils',
        'project_settings',
        'branch_spec',
        'copy_tasks',
        'lumberyard_sdks',
        'gems',
        'az_code_generator',
        'crcfix',
        'doxygen',

        'generate_uber_files',          # Load support for Uber Files
        'generate_module_def_files',    # Load support for Module Definition Files

        # Visual Studio support
        'msvs:win32',
        'msvs_override_handling:win32',
        'mscv_helper:win32',
        'vscode:win32',

        'xcode:darwin',
        'eclipse:linux',

        'android_studio',
        'android',

        'gui_tasks:win32',
        'gui_tasks:darwin',

        'msvcdeps:win32',
        'gccdeps',
        'clangdeps',

        'wwise',

        'qt5:win32',
        'qt5:darwin',

        'third_party',
        'cry_utils',
        'winres',

        'bootstrap'
    ]
}

#
LMBR_WAFLIB_DATA_DRIVEN_MODULES = {
    LMBR_WAF_TOOL_DIR : [
        'default_settings',
        'cryengine_modules',
        'incredibuild'
    ]
}

# List of all compile settings configuration modules
PLATFORM_COMPILE_SETTINGS = [
    'compile_settings_cryengine',
    'compile_settings_msvc',
    'compile_settings_gcc',
    'compile_settings_clang',
    'compile_settings_windows',
    'compile_settings_linux',
    'compile_settings_linux_x64',
    'compile_settings_darwin',
    'compile_settings_durango',
    'compile_settings_orbis',
    'compile_settings_dedicated',
    'compile_settings_test',
    'compile_settings_android',
    'compile_settings_android_armv7',
    'compile_settings_android_gcc',
    'compile_settings_android_clang',
    'compile_settings_ios',
    'compile_settings_appletv',
]

def load_lmbr_waf_modules(conf, module_table):
    """
    Load a list of modules (with optional platform restrictions)

    :param module_list: List of modules+platform restrictions to load
    """
    host_platform = Utils.unversioned_sys_platform()

    for tool_dir in module_table:
        module_list = module_table[tool_dir]

        for lmbr_waflib_module in module_list:

            if ':' in lmbr_waflib_module:
                module_platform = lmbr_waflib_module.split(':')
                module = module_platform[0]
                restricted_platform = module_platform[1]
                if restricted_platform != host_platform:
                    continue
            else:
                module = lmbr_waflib_module

            try:
                if tool_dir == GENERAL_WAF_TOOL_DIR:
                    conf.load(module)
                else:
                    conf.load(module, tooldir=tool_dir)
            except:
                conf.fatal("[Error] Unable to load required module '{}.py'".format(module))


@conf
def load_lmbr_general_modules(conf):
    """
    Load all of the required waf lmbr modules
    :param conf:    Configuration Context
    """
    load_lmbr_waf_modules(conf, LMBR_WAFLIB_MODULES)


@conf
def load_lmbr_data_driven_modules(conf):
    """
    Load all of the data driven waf lmbr modules
    :param conf:    Configuration Context
    """
    load_lmbr_waf_modules(conf, LMBR_WAFLIB_DATA_DRIVEN_MODULES)

@conf
def check_module_load_options(conf):

    if hasattr(check_module_load_options, 'did_check'):
        return

    check_module_load_options.did_check = True

    crash_reporter_file = 'crash_reporting'
    crash_reporter_path = os.path.join(LMBR_WAF_TOOL_DIR,crash_reporter_file + '.py')

    if conf.options.external_crash_reporting:
        if os.path.exists(crash_reporter_path):
            conf.load(crash_reporter_file, tooldir=LMBR_WAF_TOOL_DIR)

def configure_general_compile_settings(conf):
    """
    Perform all the necessary configurations
    :param conf:        Configuration context
    """
    # Load general compile settings
    load_setting_count = 0
    absolute_lmbr_waf_tool_path = conf.path.make_node(LMBR_WAF_TOOL_DIR).abspath()
    for compile_settings in PLATFORM_COMPILE_SETTINGS:
        if os.path.exists(os.path.join(absolute_lmbr_waf_tool_path, "{}.py".format(compile_settings))):
            conf.load(compile_settings, tooldir=LMBR_WAF_TOOL_DIR)
            load_setting_count += 1
    if load_setting_count == 0:
        conf.fatal('[ERROR] Unable to load any general compile settings modules')


def load_compile_rules_for_host(conf, waf_host_platform):
    """
    Load host specific compile rules

    :param conf:                Configuration context
    :param waf_host_platform:   The current waf host platform
    :return:                    The host function name to call for initialization
    """
    host_module_file = 'compile_rules_{}_host'.format(waf_host_platform)
    try:
        conf.load(host_module_file, tooldir=LMBR_WAF_TOOL_DIR)
    except:
        conf.fatal("[ERROR] Unable to load compile rules module file '{}'".format(host_module_file))

    host_function_name = 'load_{}_host_settings'.format(waf_host_platform)
    if not hasattr(conf, host_function_name):
        conf.fatal('[ERROR] Required Configuration Function \'{}\' not found in configuration file {}'.format(host_function_name, host_module_file))

    return host_function_name


@conf
def add_lmbr_waf_options(opt, az_test_supported):
    """
    Add custom lumberyard options

    :param opt:                 The Context to add the options to
    :param az_test_supported:   Flag that indicates if az_test is supported
    """
    ###########################################
    # Add custom cryengine options
    opt.add_option('-p', '--project-spec', dest='project_spec', action='store', default='', help='Spec to use when building the project')
    opt.add_option('--profile-execution', dest='profile_execution', action='store', default='', help='!INTERNAL ONLY! DONT USE')
    opt.add_option('--task-filter', dest='task_filter', action='store', default='', help='!INTERNAL ONLY! DONT USE')
    opt.add_option('--update-settings', dest='update_user_settings', action='store', default='False',
                   help='Option to update the user_settings.options file with any values that are modified from the command line')
    opt.add_option('--copy-3rd-party-pdbs', dest='copy_3rd_party_pdbs', action='store', default='False',
                   help='Option to copy pdbs from 3rd party libraries for debugging.  Warning: This will increase the memory usage of your visual studio development environmnet.')

    # Add special command line option to prevent recursive execution of WAF
    opt.add_option('--internal-dont-check-recursive-execution', dest='internal_dont_check_recursive_execution', action='store', default='False', help='!INTERNAL ONLY! DONT USE')
    opt.add_option('--internal-using-ib-dta', dest='internal_using_ib_dta', action='store', default='False', help='!INTERNAL ONLY! DONT USE')

    # Add options primarily used by the Visual Studio WAF Addin
    waf_addin_group = opt.add_option_group('Visual Studio WAF Addin Options')
    waf_addin_group.add_option('-a', '--ask-for-user-input', dest='ask_for_user_input', action='store', default='False', help='Disables all user prompts in WAF')
    waf_addin_group.add_option('--file-filter', dest='file_filter', action='store', default="", help='Only compile files matching this filter')
    waf_addin_group.add_option('--show-includes', dest='show_includes', action='store', default='False', help='Show all files included (requires a file_filter)')
    waf_addin_group.add_option('--show-preprocessed-file', dest='show_preprocessed_file', action='store', default='False', help='Generate only Preprocessor Output (requires a file_filter)')
    waf_addin_group.add_option('--show-disassembly', dest='show_disassembly', action='store', default='False', help='Generate only Assembler Output (requires a file_filter)')
    waf_addin_group.add_option('--use-asan', dest='use_asan', action='store', default='False', help='Enables the use of /GS or AddressSanitizer, depending on platform')
    waf_addin_group.add_option('--use-aslr', dest='use_aslr', action='store', default='False', help='Enables the use of Address Space Layout Randomization, if supported on target platform')

    # DEPRECATED OPTIONS, only used to keep backwards compatibility
    waf_addin_group.add_option('--output-file', dest='output_file', action='store', default="", help='*DEPRECATED* Specify Output File for Disassemly or Preprocess option (requieres a file_filter)')
    waf_addin_group.add_option('--use-overwrite-file', dest='use_overwrite_file', action='store', default="False",
                               help='*DEPRECATED* Use special BinTemp/lmbr_waf.configuration_overwrites to specify per target configurations')
    waf_addin_group.add_option('--console-mode', dest='console_mode', action='store', default="True", help='No Gui. Display console only')

    # Test specific options
    if az_test_supported:
        test_group = opt.add_option_group('AzTestScanner Options')
        test_group.add_option('--test-params', dest='test_params', action='store', default='', help='Test parameters to send to the scanner (encapsulate with quotes)')


@conf
def configure_settings(conf):
    """
    Perform all the necessary configurations

    :param conf:        Configuration context
    """
    configure_general_compile_settings(conf)


@conf
def run_bootstrap(conf):
    """
    Execute the bootstrap process

    :param conf:                Configuration context
    """
    # Bootstrap the build environment with bootstrap tool
    bootstrap_param = getattr(conf.options, "bootstrap_tool_param", "")
    bootstrap_third_party_override = getattr(conf.options, "bootstrap_third_party_override", "")
    conf.run_bootstrap_tool(bootstrap_param, bootstrap_third_party_override)


@conf
def filter_target_platforms(conf):
    """
    Filter out any target platforms based on settings or options from the configuration/options

    :param conf:                Configuration context
    """

    # handle disabling android here to avoid having the same block of code in each of the compile_rules
    # for all the current and future android targets
    android_enabled = conf.get_env_file_var('ENABLE_ANDROID', required=False, silent=True)
    if android_enabled == 'True':
        # We need to validate the JDK path from SetupAssistant before loading the javaw tool.
        # This way we don't introduce a dependency on lmbrwaflib in the core waflib.
        jdk_home = conf.get_env_file_var('LY_JDK', required = True)
        if not jdk_home:
            conf.fatal('[ERROR] Missing JDK path from Setup Assistant detected.  Please re-run Setup Assistant with "Compile For Android" enabled and run the configure command again.')

        conf.env['JAVA_HOME'] = jdk_home

        conf.load('javaw')
        conf.load('android', tooldir=LMBR_WAF_TOOL_DIR)
    else:
        android_targets = [target for target in conf.get_available_platforms() if 'android' in target]
        Logs.warn('[WARN] Removing the following Android target platforms due to "Compile For Android" not checked in Setup Assistant.\n'
                  '\t-> {}'.format(', '.join(android_targets)))
        for android in android_targets:
            conf.remove_platform_from_available_platforms(android)


@conf
def load_compile_rules_for_supported_platforms(conf, platform_configuration_filter):
    """
    Load the compile rules for all the supported target platforms for the current host platform

    :param conf:                            Configuration context
    :param platform_configuration_filter:   List of target platforms to filter out
    """

    host_platform = conf.get_waf_host_platform()

    absolute_lmbr_waf_tool_path = conf.path.make_node(LMBR_WAF_TOOL_DIR).abspath()

    vanilla_conf = conf.env.derive()  # grab a snapshot of conf before you pollute it.

    host_function_name = load_compile_rules_for_host(conf, host_platform)

    installed_platforms = []

    for platform in conf.get_available_platforms():

        platform_spec_vanilla_conf = vanilla_conf.derive()
        platform_spec_vanilla_conf.detach()

        # Determine the compile rules module file and remove it and its support if it does not exist
        compile_rule_script = 'compile_rules_' + host_platform + '_' + platform
        if not os.path.exists(os.path.join(absolute_lmbr_waf_tool_path, compile_rule_script + '.py')):
            conf.remove_platform_from_available_platforms(platform)
            continue

        Logs.info('[INFO] Configure "%s - [%s]"' % (platform, ', '.join(conf.get_supported_configurations(platform))))
        conf.load(compile_rule_script, tooldir=LMBR_WAF_TOOL_DIR)

        # platform installed
        installed_platforms.append(platform)

        # Keep track of uselib's that we found in the 3rd party config files
        conf.env['THIRD_PARTY_USELIBS'] = [uselib_name for uselib_name in conf.read_and_mark_3rd_party_libs()]

        for configuration in conf.get_supported_configurations():
            # if the platform isn't going to generate a build command, don't require that the configuration exists either
            if platform in platform_configuration_filter:
                if configuration not in platform_configuration_filter[platform]:
                    continue

            conf.setenv(platform + '_' + configuration, platform_spec_vanilla_conf.derive())
            conf.init_compiler_settings()

            # add the host settings into the current env
            getattr(conf, host_function_name)()

            # make a copy of the config for certain variant loading redirection (e.g. test, dedicated)
            # this way we can pass the raw configuration to the third pary reader to properly configure
            # each library
            config_redirect = configuration

            # Use the normal configurations as a base for dedicated server
            is_dedicated = False
            if config_redirect.endswith('_dedicated'):
                config_redirect = config_redirect.replace('_dedicated', '')
                is_dedicated = True

            # Use the normal configurations as a base for test
            is_test = False
            if '_test' in config_redirect:
                config_redirect = config_redirect.replace('_test', '')
                is_test = True

            # Use the specialized function to load platform specifics
            function_name = 'load_%s_%s_%s_settings' % ( config_redirect, host_platform, platform )
            if not hasattr(conf, function_name):
                conf.fatal('[ERROR] Required Configuration Function \'%s\' not found' % function_name)

            # Try to load the function
            getattr(conf, function_name)()

            # Apply specific dedicated server settings
            if is_dedicated:
                getattr(conf, 'load_dedicated_settings')()

            # Apply specific test settings
            if is_test:
                getattr(conf, 'load_test_settings')()

            if platform in conf.get_supported_platforms():
                # If the platform is still supported (it will be removed if the load settings function fails), then
                # continue to attempt to load the 3rd party uselib defs for the platform
                path_alias_map = {'ROOT': conf.srcnode.abspath()}

                config_3rdparty_folder_legacy = conf.root.make_node(Context.launch_dir).make_node('_WAF_/3rd_party')
                config_3rdparty_folder_legacy_path = config_3rdparty_folder_legacy.abspath()

                config_3rdparty_folder = conf.root.make_node(Context.launch_dir).make_node('_WAF_/3rdParty')
                config_3rdparty_folder_path = config_3rdparty_folder.abspath()

                if os.path.exists(config_3rdparty_folder_legacy_path) and os.path.exists(config_3rdparty_folder_path):

                    has_legacy_configs = len(os.listdir(config_3rdparty_folder_legacy_path)) > 0

                    # Both legacy and current 3rd party exists.  Print a warning and use the current 3rd party
                    if has_legacy_configs:
                        conf.warn_once('Legacy 3rd Party configuration path ({0}) will be ignored in favor of ({1}).  '
                                       'Merge & remove the configuration files from the legacy path ({0}) to the current path ({1})'
                                       .format(config_3rdparty_folder_legacy_path, config_3rdparty_folder_path))
                    thirdparty_error_msgs, uselib_names = conf.detect_all_3rd_party_libs(config_3rdparty_folder, platform, configuration, path_alias_map)

                elif os.path.exists(config_3rdparty_folder_legacy_path):

                    # Only the legacy 3rd party config folder exists.
                    thirdparty_error_msgs, uselib_names = conf.detect_all_3rd_party_libs(config_3rdparty_folder_legacy, platform, configuration, path_alias_map)

                elif os.path.exists(config_3rdparty_folder_path):

                    # Only the current 3rd party config folder exists.
                    thirdparty_error_msgs, uselib_names = conf.detect_all_3rd_party_libs(config_3rdparty_folder, platform, configuration, path_alias_map)

                else:
                    # Neither folder exists, report a warning
                    thirdparty_error_msgs = ['Unable to find 3rd party configuration path ({}).  No 3rd party libraries will '
                                             'be configured.'.format(config_3rdparty_folder_path)]

                for thirdparty_error_msg in thirdparty_error_msgs:
                    conf.warn_once(thirdparty_error_msg)


@conf
def process_custom_configure_commands(conf):
    """
    Add any additional custom commands that need to be run during the configure phase

    :param conf:                Configuration context
    """

    host = Utils.unversioned_sys_platform()

    if host == 'win32':
        # Win32 platform optional commands

        # Generate the visual studio projects & solution if specified
        if conf.is_option_true('generate_vs_projects_automatically'):
            Options.commands.insert(0, 'msvs')

    elif host == 'darwin':

        # Darwin/Mac platform optional commands

        # Create Xcode-iOS-Projects automatically during configure when running on mac
        if conf.is_option_true('generate_ios_projects_automatically'):
            # Workflow improvement: for all builds generate projects after the build
            # except when using the default build target 'utilities' then do it before
            if 'build' in Options.commands:
                build_cmd_idx = Options.commands.index('build')
                Options.commands.insert(build_cmd_idx, 'xcode_ios')
            else:
                Options.commands.append('xcode_ios')

        # Create Xcode-AppleTV-Projects automatically during configure when running on mac
        if conf.is_option_true('generate_appletv_projects_automatically'):
            # Workflow improvement: for all builds generate projects after the build
            # except when using the default build target 'utilities' then do it before
            if 'build' in Options.commands:
                build_cmd_idx = Options.commands.index('build')
                Options.commands.insert(build_cmd_idx, 'xcode_appletv')
            else:
                Options.commands.append('xcode_appletv')

        # Create Xcode-darwin-Projects automatically during configure when running on mac
        if conf.is_option_true('generate_mac_projects_automatically'):
            # Workflow improvement: for all builds generate projects after the build
            # except when using the default build target 'utilities' then do it before
            if 'build' in Options.commands:
                build_cmd_idx = Options.commands.index('build')
                Options.commands.insert(build_cmd_idx, 'xcode_mac')
            else:
                Options.commands.append('xcode_mac')

    # Android target platform commands
    if any(platform for platform in conf.get_supported_platforms() if 'android' in platform):

        # this is required for building any android projects. It adds the Android launchers
        # to the list of build directories
        android_builder_func = getattr(conf, 'create_and_add_android_launchers_to_build', None)
        if android_builder_func != None and android_builder_func():
            SUBFOLDERS.append(conf.get_android_project_relative_path())

        # rebuild the project if invoked from android studio or sepcifically requested to do so
        if conf.options.from_android_studio or conf.is_option_true('generate_android_studio_projects_automatically'):
            if 'build' in Options.commands:
                build_cmd_idx = Options.commands.index('build')
                Options.commands.insert(build_cmd_idx, 'android_studio')
            else:
                Options.commands.append('android_studio')

        # generate header
        def _indent_text(indent_level, text, *args):
            indent_space = ' ' * indent_level * 4
            return str.format('{}{}', indent_space, text % args)

        recordingMode = [
            'AZ::Debug::AllocationRecords::Mode::RECORD_NO_RECORDS',
            'AZ::Debug::AllocationRecords::Mode::RECORD_STACK_NEVER',
            'AZ::Debug::AllocationRecords::Mode::RECORD_STACK_IF_NO_FILE_LINE',
            'AZ::Debug::AllocationRecords::Mode::RECORD_FULL',
            'AZ::Debug::AllocationRecords::Mode::RECORD_MAX',
        ]

        outputString = ""

        outputString += "////////////////////////////////////////////////////////////////\n"
        outputString += "// This file was automatically created by WAF\n"
        outputString += "// WARNING! All modifications will be lost!\n"
        outputString += "////////////////////////////////////////////////////////////////\n\n"

        outputString += "void SetupAndroidDescriptor(const char* gameName, AZ::ComponentApplication::Descriptor &desc)\n{\n"

        for project in conf.get_enabled_game_project_list():
            targetFile = os.path.join(conf.path.abspath(), project, "Config", "Game.xml")

            tree = ET.parse(targetFile)
            root = tree.getroot()
            descriptor = root[0]

            outputString += _indent_text(1, "if(stricmp(gameName, \"%s\") == 0)\n", project)
            outputString += _indent_text(1, "{\n")
            outputString += _indent_text(2, "desc.m_useExistingAllocator = %s;\n", descriptor.findall("*[@field='useExistingAllocator']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_grabAllMemory = %s;\n", descriptor.findall("*[@field='grabAllMemory']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_allocationRecords = %s;\n", descriptor.findall("*[@field='allocationRecords']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_autoIntegrityCheck = %s;\n", descriptor.findall("*[@field='autoIntegrityCheck']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_markUnallocatedMemory = %s;\n", descriptor.findall("*[@field='markUnallocatedMemory']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_doNotUsePools = %s;\n", descriptor.findall("*[@field='doNotUsePools']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_pageSize = %s;\n", descriptor.findall("*[@field='pageSize']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_poolPageSize = %s;\n", descriptor.findall("*[@field='poolPageSize']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_memoryBlockAlignment = %s;\n", descriptor.findall("*[@field='blockAlignment']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_memoryBlocksByteSize = %s;\n", descriptor.findall("*[@field='blockSize']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_reservedOS = %s;\n", descriptor.findall("*[@field='reservedOS']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_reservedDebug = %s;\n", descriptor.findall("*[@field='reservedDebug']")[0].get("value"))

            if descriptor.find("*[@field='recordingMode']") is not None:
                field = "recordingMode"
            else:
                field = "recordsMode"

            id = int(descriptor.findall(str.format("*[@field='{}']", field))[0].get("value"))
            outputString += _indent_text(2, "desc.m_recordingMode = %s;\n", recordingMode[id])

            outputString += _indent_text(2, "desc.m_stackRecordLevels = %s;\n", descriptor.findall("*[@field='stackRecordLevels']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_enableDrilling = %s;\n", descriptor.findall("*[@field='enableDrilling']")[0].get("value"))
            outputString += _indent_text(2, "desc.m_x360IsPhysicalMemory = %s;\n", descriptor.findall("*[@field='x360PhysicalMemory']")[0].get("value"))

            modulesElement = descriptor.findall("*[@field='modules']")[0]
            for moduleEntry in modulesElement.findall("*[@field='element']"):
                outputString += _indent_text(2, "desc.m_modules.push_back();\n")
                outputString += _indent_text(2, "desc.m_modules.back().m_dynamicLibraryPath = \"%s\";\n", moduleEntry.findall("*[@field='dynamicLibraryPath']")[0].get("value"))
            outputString += _indent_text(1, "}\n")

        outputString += "}\n"

        filePath = os.path.join(conf.path.abspath(), "Code", "Launcher", "AndroidLauncher", "android_descriptor.h")
        fp = open(filePath, 'w')
        fp.write(outputString)
        fp.close()

    # Make sure the intermediate files are generated and updated
    if len(Options.commands) == 0:
        Options.commands.insert(0, 'generate_uber_files')
        Options.commands.insert(1, 'generate_module_def_files')
    else:
        has_generate_uber_files = 'generate_uber_files' in Options.commands
        has_generate_module_def_files = 'generate_module_def_files' in Options.commands
        if not has_generate_uber_files:
            Options.commands.insert(0, 'generate_uber_files')
        if not has_generate_module_def_files:
            Options.commands.insert(1, 'generate_module_def_files')


@conf
def configure_game_projects(conf):
    """
    Perform the configuration processing for any enabled game project

    :param conf:
    """
    # Load Game Specific parts
    for project in conf.get_enabled_game_project_list():
        conf.game_project = project

        # If the project contains the 'code_folder', then assume that it is a legacy project loads a game-specific dll
        legacy_game_code_folder = conf.legacy_game_code_folder(project)
        if legacy_game_code_folder is not None:
            conf.recurse(legacy_game_code_folder)
        else:
            # Perform a validation of the gems.json configuration to make sure
            conf.game_gem(project)


@conf
def clear_waf_timestamp_files(conf):
    """
    Remove timestamp files to force builds of generate_uber_files and project gen even if
    some command after configure failes

    :param conf:
    """

    # Remove timestamp files to force builds even if some command after configure fail
    force_timestamp_files = CONFIGURE_FORCE_TIMESTAMP_FILES
    for force_timestamp_file in force_timestamp_files:
        try:
            force_timestamp_node = conf.get_bintemp_folder_node().make_node(force_timestamp_file)
            os.stat(force_timestamp_node.abspath())
        except OSError:
            pass
        else:
            force_timestamp_node.delete()

    # Add timestamp files for files generated by configure
    check_timestamp_files = CONFIGURE_TIMESTAMP_FILES
    for check_timestamp_file in check_timestamp_files:
        check_timestamp_node = conf.get_bintemp_folder_node().make_node(check_timestamp_file)
        check_timestamp_node.write('')

@conf
def validate_build_command(bld):
    """
    Validate the build command and build context

    :param bld: The BuildContext
    """

    # Only Build Context objects are valid here
    if not isinstance(bld, BuildContext):
        bld.fatal("[Error] Invalid build command: '{}'.  Type in '{} --help' for more information"
                  .format(bld.cmd if hasattr(bld, 'cmd') else str(bld), CURRENT_WAF_EXECUTABLE))

    # If this is a build or clean command, see if this is an unsupported platform+config
    if bld.cmd.startswith('build_'):
        bld_clean_platform_config_key = bld.cmd[6:]
    elif bld.cmd.startswith('clean_'):
        bld_clean_platform_config_key = bld.cmd[5:]
    else:
        bld_clean_platform_config_key = None

    if bld_clean_platform_config_key is not None:
        if bld_clean_platform_config_key in UNSUPPORTED_PLATFORM_CONFIGURATIONS:
            # This is not a supported build_ or  clean_ operation for a platform + configuration
            raise conf.fatal("[Error] This is an unsupported platform and configuration")

    # Check if a valid spec is defined for build commands
    if bld.cmd not in NON_BUILD_COMMANDS:

        # project spec is a mandatory parameter for build commands that perform monolithic builds
        if bld.is_cmd_monolithic():
            Logs.debug('lumberyard: Processing monolithic build command ')
            # For monolithic builds, the project spec is required
            if bld.options.project_spec == '':
                bld.fatal('[ERROR] No Project Spec defined. Project specs are required for monolithic builds.  Use "-p <spec_name>" command line option to specify project spec. Valid specs are "%s". ' % bld.loaded_specs())

            # Ensure that the selected spec is supported on this platform
            if not bld.options.project_spec in bld.loaded_specs():
                bld.fatal('[ERROR] Invalid Project Spec (%s) defined, valid specs are %s' % (bld.options.project_spec, bld.loaded_specs()))

        # Check for valid single file compilation flag pairs
        if bld.is_option_true('show_preprocessed_file') and bld.options.file_filter == "":
            bld.fatal('--show-preprocessed-file can only be used in conjunction with --file-filter')
        elif bld.is_option_true('show_disassembly') and bld.options.file_filter == "":
            bld.fatal('--show-disassembly can only be used in conjunction with --file-filter')

    # Validate the game project version
    if VERSION_NUMBER_PATTERN.match(bld.options.version) is None:
        bld.fatal("[Error] Invalid game version number format ({})".format(bld.options.version))


@conf
def check_special_command_timestamps(bld):
    """
    Check timestamps for special commands and see if the current build needs to be short circuited

    :param bld: The BuildContext
    :return: False to short circuit to restart the command chain, True if not and continue the build process
    """
    # Only Build Context objects are valid here
    if not isinstance(bld, BuildContext):
        bld.fatal("[Error] Invalid build command: '{}'.  Type in '{} --help' for more information"
                  .format(bld.cmd if hasattr(bld, 'cmd') else str(bld), CURRENT_WAF_EXECUTABLE))

    if not 'generate_uber_files' in Options.commands and bld.cmd != 'generate_uber_files':
        generate_uber_files_timestamp = bld.get_bintemp_folder_node().make_node('generate_uber_files.timestamp')
        try:
            os.stat(generate_uber_files_timestamp.abspath())
        except OSError:
            # No generate_uber file timestamp, restart command chain, prefixed with gen uber files cmd
            Options.commands = ['generate_uber_files'] + [bld.cmd] + Options.commands
            return False

    return True


@conf
def prepare_build_environment(bld):
    """
    Prepare the build environment for the current build command and setup optional additional commands if necessary

    :param bld: The BuildContext
    """

    if bld.cmd in NON_BUILD_COMMANDS:
        bld.env['PLATFORM'] = 'project_generator'
        bld.env['CONFIGURATION'] = 'project_generator'
    else:
        if not bld.variant:
            bld.fatal("[ERROR] Invalid build variant.  Please use a valid build configuration. "
                      "Type in '{} --help' for more information".format(CURRENT_WAF_EXECUTABLE))

        (platform, configuration) = bld.get_platform_and_configuration()
        bld.env['PLATFORM'] = platform
        bld.env['CONFIGURATION'] = configuration

        if platform in PLATFORM_CONFIGURATION_FILTER:
            if configuration not in PLATFORM_CONFIGURATION_FILTER[platform]:
                bld.fatal('[ERROR] Configuration ({}) for platform ({}) currently not supported'.format(configuration, platform))

        if not platform in bld.get_supported_platforms():
            bld.fatal('[ERROR] Target platform "%s" not supported. [on host platform: %s]' % (platform, Utils.unversioned_sys_platform()))

        # make sure the android launchers are included in the build
        if bld.env['PLATFORM'] in ('android_armv7_gcc', 'android_armv7_clang'):
            android_path = os.path.join(bld.path.abspath(), bld.get_android_project_relative_path(), 'wscript')
            if not os.path.exists(android_path):
                bld.fatal('[ERROR] Android launchers not correctly configured. Run \'configure\' again')
            SUBFOLDERS.append(bld.get_android_project_relative_path())

        # If a spec was supplied, check for platform limitations
        if bld.options.project_spec != '':
            validated_platforms = bld.preprocess_platform_list(bld.spec_platforms(), True)
            if platform not in validated_platforms:
                bld.fatal('[ERROR] Target platform "{}" not supported for spec {}'.format(platform, bld.options.project_spec))
            validated_configurations = bld.preprocess_configuration_list(None, platform, bld.spec_configurations(), True)
            if configuration not in validated_configurations:
                bld.fatal('[ERROR] Target configuration "{}" not supported for spec {}'.format(configuration, bld.options.project_spec))

        if bld.is_cmd_monolithic():
            if len(bld.spec_modules()) == 0:
                bld.fatal('[ERROR] no available modules to build for that spec "%s" in "%s|%s"' % (bld.options.project_spec, platform, configuration))

            # Ensure that, if specified, target is supported in this spec
            if bld.options.targets:
                for target in bld.options.targets.split(','):
                    if not target in bld.spec_modules():
                        bld.fatal('[ERROR] Module "%s" is not configured to build in spec "%s" in "%s|%s"' % (target, bld.options.project_spec, platform, configuration))

        deploy_cmd = 'deploy_' + platform + '_' + configuration
        # Only deploy to specific target platforms
        if 'build' in bld.cmd and platform in ['durango', 'orbis', 'android_armv7_gcc', 'android_armv7_clang'] and deploy_cmd not in Options.commands:
            # Only deploy if we are not trying to rebuild 3rd party libraries for the platforms above
            if '3rd_party' != getattr(bld.options, 'project_spec', ''):
                Options.commands.append(deploy_cmd)


@conf
def setup_game_projects(bld):
    """
    Setup the game projects for non-build commands

    :param bld:     The BuildContext
    """

    previous_game_project = bld.game_project
    # Load Game Specific parts, but only if the current spec is a game...
    if bld.env['CONFIGURATION'] == 'project_generator' or not bld.spec_disable_games():
        # If this bld command is to generate the use maps, then recurse all of the game projects, otherwise
        # only recurse the enabled game project
        game_project_list = bld.game_projects() if bld.cmd == 'generate_module_def_files' \
            else bld.get_enabled_game_project_list()
        for project in game_project_list:

            bld.game_project = project
            legacy_game_code_folder = bld.legacy_game_code_folder(project)
            # If there exists a code_folder attribute in the project configuration, then see if it can be included in the build
            if legacy_game_code_folder is not None:
                must_exist = project in bld.get_enabled_game_project_list()
                # projects that are not currently enabled don't actually cause an error if their code folder is missing - we just don't compile them!
                try:
                    bld.recurse(legacy_game_code_folder, mandatory=True)
                except Exception as err:
                    if must_exist:
                        bld.fatal('[ERROR] Project "%s" is enabled, but failed to execute its wscript at (%s) - consider disabling it in your _WAF_/user_settings.options file.\nException: %s' % (
                        project, legacy_game_code_folder, err))

    elif bld.env['CONFIGURATION'] != 'project_generator':
        Logs.debug('lumberyard:  Not recursing into game directories since no games are set up to build in current spec')

    # restore project in case!
    bld.game_project = previous_game_project


# Create Build Context Commands for multiple platforms/configurations
for platform in PLATFORMS[Utils.unversioned_sys_platform()]:
    for configuration in CONFIGURATIONS:
        platform_config_key = platform + '_' + configuration
        # for platform/for configuration generates invalid configurations
        # if a filter exists, don't generate all combinations
        if platform in PLATFORM_CONFIGURATION_FILTER:
            if configuration not in PLATFORM_CONFIGURATION_FILTER[platform]:
                # Dont add the command but track it to report that this platform + configuration is explicitly not supported (yet)
                UNSUPPORTED_PLATFORM_CONFIGURATIONS.add(platform_config_key)
                continue
        # Create new class to execute clean with variant
        name = CleanContext.__name__.replace('Context','').lower()
        class tmp_clean(CleanContext):
            cmd = name + '_' + platform_config_key
            variant = platform_config_key

            def __init__(self, **kw):
                super(CleanContext, self).__init__(**kw)
                self.top_dir = kw.get('top_dir', Context.top_dir)

            def execute(self):
                if Configure.autoconfig:
                    env = ConfigSet.ConfigSet()

                    do_config = False
                    try:
                        env.load(os.path.join(Context.lock_dir, Options.lockfile))
                    except Exception:
                        Logs.warn('Configuring the project')
                        do_config = True
                    else:
                        if env.run_dir != Context.run_dir:
                            do_config = True
                        else:
                            h = 0
                            for f in env['files']:
                                try:
                                    h = hash((h, Utils.readf(f, 'rb')))
                                except (IOError, EOFError):
                                    pass # ignore missing files (will cause a rerun cause of the changed hash)
                            do_config = h != env.hash

                    if do_config:
                        Options.commands.insert(0, self.cmd)
                        Options.commands.insert(0, 'configure')
                        return

                # Execute custom clear command
                self.restore()
                if not self.all_envs:
                    self.load_envs()
                self.recurse([self.run_dir])

                if self.options.targets:
                    self.target_clean()
                else:
                    try:
                        self.clean_output_targets()
                        self.clean()
                    finally:
                        self.store()


        # Create new class to execute build with variant
        name = BuildContext.__name__.replace('Context','').lower()
        class tmp_build(BuildContext):
            cmd = name + '_' + platform_config_key
            variant = platform_config_key

            def compare_timestamp_file_modified(self, path):
                modified = False

                # create a src node
                src_node = self.path.make_node(path)
                timestamp_file = os.path.splitext(os.path.split(path)[1])[0] + '.timestamp'
                timestamp_node = self.get_bintemp_folder_node().make_node(timestamp_file)
                timestamp_stat = 0
                try:
                    timestamp_stat = os.stat(timestamp_node.abspath())
                except OSError:
                    Logs.info('%s timestamp file not found.' % timestamp_node.abspath())
                    modified = True
                else:
                    try:
                        src_file_stat = os.stat(src_node.abspath())
                    except OSError:
                        Logs.warn('%s not found' % src_node.abspath())
                    else:
                        if src_file_stat.st_mtime > timestamp_stat.st_mtime:
                            modified = True

                return modified

            def add_configure_command(self):
                Options.commands.insert(0, self.cmd)
                Options.commands.insert(0, 'configure')
                self.skip_finish_message = True

            def do_auto_configure(self):
                timestamp_check_files = TIMESTAMP_CHECK_FILES
                if 'android' in platform:
                    timestamp_check_files += ANDROID_TIMESTAMP_CHECK_FILES

                for timestamp_check_file in timestamp_check_files:
                    if self.compare_timestamp_file_modified(timestamp_check_file):
                        self.add_configure_command()
                        return True
                return False

        # Create derived build class to execute host tools build for host + profile only
        host = Utils.unversioned_sys_platform()
        if platform in ["win_x64", "linux_x64", "darwin_x64"] and configuration == 'profile':
            class tmp_build_host_tools(tmp_build):
                cmd = 'build_host_tools'
                variant = platform + '_' + configuration
                def execute(self):
                    original_project_spec = self.options.project_spec
                    original_targets = self.targets
                    self.options.project_spec = 'host_tools'
                    self.targets = []
                    super(tmp_build_host_tools, self).execute()
                    self.options.project_spec = original_project_spec
                    self.targets = original_targets


@conf
def get_enabled_capabilities(ctx):
    """
    Get the capabilities that were set through setup assistant
    :param ctx: Context
    :return: List of capabilities that were set by the setup assistant
    """

    try:
        return ctx.parsed_capabilities
    except AttributeError:
        pass

    raw_capability_string = getattr(ctx.options,'bootstrap_tool_param',None)

    if raw_capability_string:
        capability_parser = argparse.ArgumentParser()
        capability_parser.add_argument('--enablecapability', action='append')
        params = [token.strip() for token in raw_capability_string.split()]
        parsed_capabilities, _ = capability_parser.parse_known_args(params)
        parsed_capabilities = parsed_capabilities.enablecapability
    else:
        parsed_capabilities = []

    setattr(ctx,'parsed_capabilities',parsed_capabilities)

    return parsed_capabilities








