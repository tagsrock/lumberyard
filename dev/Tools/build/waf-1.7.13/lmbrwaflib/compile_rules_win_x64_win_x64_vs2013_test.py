﻿#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
from waflib.Configure import conf, Logs

import compile_rules_win_x64_win_x64_vs2013

def load_common_win_x64_vs2013_test_settings(conf):
    conf.env['DEFINES'] += ['AZ_TESTS_ENABLED']

@conf
def load_debug_win_x64_win_x64_vs2013_test_settings(conf):
    load_common_win_x64_vs2013_test_settings(conf)
    return compile_rules_win_x64_win_x64_vs2013.load_debug_win_x64_win_x64_vs2013_settings(conf)

@conf
def load_profile_win_x64_win_x64_vs2013_test_settings(conf):
    load_common_win_x64_vs2013_test_settings(conf)
    return compile_rules_win_x64_win_x64_vs2013.load_profile_win_x64_win_x64_vs2013_settings(conf)

