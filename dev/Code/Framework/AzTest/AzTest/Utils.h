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
#pragma once
#include <string>
#include <vector>

namespace AZ
{
    namespace Test
    {
        /*! Command line parameter functions */
        //! Check to see if the parameters includes the specified parameter
        bool ContainsParameter(int argc, char** argv, const std::string& param);

        //! Performs a deep copy of an array of char* parameters into a new array
        //! New parameters are dynamically allocated
        //! This does not set the size for the output array (assumed to be same as argc)
        void CopyParameters(int argc, char** target, char** source);

        //! Get index of the specified parameter
        int GetParameterIndex(int argc, char** argv, const std::string& param);

        //! Get multi-value parameter list based on a flag (and remove from argv if specified)
        //! Returns a string vector for ease of use and cleanup
        std::vector<std::string> GetParameterList(int& argc, char** argv, const std::string& param, bool removeOnReturn = false);

        //! Get value of the specified parameter (and remove from argv if specified)
        //! This assumes that the value is the next argument after the specified parameter
        std::string GetParameterValue(int& argc, char** argv, const std::string& param, bool removeOnReturn = false);

        //! Remove parameters and shift remaining down, startIndex and endIndex are inclusive
        void RemoveParameters(int& argc, char** argv, int startIndex, int endIndex);

        //! Split a C-string command line into an array of char* parameters (argv/argc)
        //! Char* array is dynamically allocated
        char** SplitCommandLine(int& size /* out */, char* const cmdLine);

        /*! General string functions */
        //! Check if a string has a specific ending substring
        bool EndsWith(const std::string& s, const std::string& ending);

        //! Check if a string has a specific beginning substring
        bool StartsWith(const std::string& s, const std::string& beginning);
    }
}
