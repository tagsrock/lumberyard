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

import service
import stats_settings

@service.api
def get(request):
    return {
        "stats": stats_settings.get_leaderboard_stats()
        }

@service.api
def post(request, stat_def = None):
    if stat_def:
        stats_settings.add_stat(stat_def)
    return {
        "stats": stats_settings.get_leaderboard_stats()
        }

@service.api
def delete(request, stat_name = None):
    if stat_name:
        stats_settings.remove_stat(stat_name)
    return {
        "stats": stats_settings.get_leaderboard_stats()
        }
