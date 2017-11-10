#pragma once

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

#include <AzCore/std/string/string.h>
#include <AzCore/JSON/document.h>
#include <SceneAPI/SceneCore/SceneCoreConfiguration.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneCore
        {
            // PatternMatcher stores a pattern and a matching approach for later use.
            //      Strings can then be checked against the stored pattern.
            //      The supported approaches are:
            //      Prefix - Matches if the string starts with the stored pattern.
            //      Postfix - Matches if the string ends with the stored pattern.
            //      Regex - Matches if the string matches the given regular expression.
            class PatternMatcher
            {
            public:
                enum class MatchApproach
                {
                    PreFix,
                    PostFix,
                    Regex
                };

                PatternMatcher() = default;
                SCENE_CORE_API PatternMatcher(const char* pattern, MatchApproach matcher);
                SCENE_CORE_API PatternMatcher(const AZStd::string& pattern, MatchApproach matcher);
                SCENE_CORE_API PatternMatcher(AZStd::string&& pattern, MatchApproach matcher);
                SCENE_CORE_API PatternMatcher(const PatternMatcher& rhs) = default;
                SCENE_CORE_API PatternMatcher(PatternMatcher&& rhs);

                SCENE_CORE_API PatternMatcher& operator=(const PatternMatcher& rhs) = default;
                SCENE_CORE_API PatternMatcher& operator=(PatternMatcher&& rhs);

                SCENE_CORE_API bool LoadFromJson(rapidjson::Document::ConstMemberIterator member);

                SCENE_CORE_API bool MatchesPattern(const char* name, size_t nameLength) const;
                SCENE_CORE_API bool MatchesPattern(const AZStd::string& name) const;

                SCENE_CORE_API const AZStd::string& GetPattern() const;
                SCENE_CORE_API MatchApproach GetMatchApproach() const;

            private:
                AZStd::string m_pattern;
                MatchApproach m_matcher;
            };
        } // SceneCore
    } // SceneAPI
} // AZ
