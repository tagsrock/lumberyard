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
#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <EditorCommonAPI.h>

namespace AZ
{
    // Stores the error output from save actions. Pairs error messages with a "details" context. That way if you could
    //  do something like:
    //      output->AddError("Failed to save file", fileName);
    //
    //  Then if that error gets added a few times with different files, the final error message will be aggregated as
    //  follows:
    //     Failed to save file:
    //          thing1.cdf
    //          thing2.chr
    class EDITOR_COMMON_API ActionOutput
    {
    public:
        using DetailList = AZStd::vector<AZStd::string>;
        using IssueToDetails = AZStd::map<AZStd::string, DetailList>;

        ActionOutput();

        void AddError(const AZStd::string& error);
        void AddError(const AZStd::string& error, const AZStd::string& details);
        bool HasAnyErrors() const;
        AZStd::string BuildErrorMessage() const;

        void AddWarning(const AZStd::string& error);
        void AddWarning(const AZStd::string& error, const AZStd::string& details);
        bool HasAnyWarnings() const;
        AZStd::string BuildWarningMessage() const;

    private:
        AZStd::string BuildMessage(const IssueToDetails& issues) const;

        IssueToDetails m_errorToDetails;
        IssueToDetails m_warningToDetails;
        int m_errorCount;
        int m_warningCount;
    };
}