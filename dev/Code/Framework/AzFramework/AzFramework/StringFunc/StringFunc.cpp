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
#include <ctype.h>

#include <AzCore/std/string/conversions.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/IO/SystemFile.h> // AZ_MAX_PATH_LEN
#include "StringFunc.h"

#ifndef AZ_COMPILER_MSVC

//Have to declare this typedef for Android & Linux to minimise changes elsewhere in this file
#if defined(AZ_PLATFORM_ANDROID) || defined(AZ_PLATFORM_LINUX)
typedef int errno_t;
#endif //AZ_PLATFORM_ANDROID

void ClearToEmptyStr(char* buffer)
{
    if (buffer != nullptr)
    {
        *buffer = '\0';
    }
}

// Microsoft defines _splitpath_s but no one else so define it ourselves
errno_t _splitpath_s (const char* path,
    char* drive, size_t driveBufferSize,
    char* dir, size_t dirBufferSize,
    char* fname, size_t nameBufferSize,
    char* ext, size_t extBufferSize)
{
    // Defined the same value error values as windows...
#undef EINVAL
#undef ERANGE
    static const errno_t EINVAL = 22;
    static const errno_t ERANGE = 34;

    // Error checking first
    if ((path == nullptr) ||
        (drive == nullptr && driveBufferSize > 0) || (drive != nullptr && driveBufferSize == 0) ||
        (dir == nullptr && dirBufferSize > 0) || (dir != nullptr && dirBufferSize == 0) ||
        (fname == nullptr && nameBufferSize > 0) || (fname != nullptr && nameBufferSize == 0) ||
        (ext == nullptr && extBufferSize > 0) || (ext != nullptr && extBufferSize == 0))
    {
        return EINVAL;
    }

    // Clear all output buffers
    ClearToEmptyStr(drive);
    ClearToEmptyStr(dir);
    ClearToEmptyStr(fname);
    ClearToEmptyStr(ext);

    const char* lastBackSlashLocation = strrchr(path, '\\');
    if (lastBackSlashLocation == nullptr)
    {
        lastBackSlashLocation = path;
    }

    const char* lastForwardSlashLocation = strrchr(path, '/');
    if (lastForwardSlashLocation == nullptr)
    {
        lastForwardSlashLocation = path;
    }

    const char* lastPathSeparatorLocation = AZStd::max(lastBackSlashLocation, lastForwardSlashLocation);
    const char* fileNameLocation = lastPathSeparatorLocation != path ?
        lastPathSeparatorLocation + 1 :
        lastPathSeparatorLocation;

    const char* pathEndLocation = path + strlen(path);
    const char* extensionLocation = strrchr(path, '.');
    if (extensionLocation == nullptr ||
        extensionLocation < lastPathSeparatorLocation)
    {
        // No extension
        extensionLocation = pathEndLocation;
    }

    if (ext != nullptr)
    {
        const size_t extLength = pathEndLocation - extensionLocation;
        if (extLength >= extBufferSize) // account for null terminator
        {
            return ERANGE;
        }
        azstrcpy(ext, extBufferSize, extensionLocation);
    }

    if (extensionLocation == path)
    {
        // The entire path is the extension
        return 0;
    }

    if (fname != nullptr)
    {
        const size_t fileNameLength = extensionLocation - fileNameLocation;
        if (fileNameLength >= nameBufferSize) // account for null terminator
        {
            // Clear buffers set previously to maintain consistency with msvc implementation:
            // https://msdn.microsoft.com/en-us/library/8e46eyt7.aspx
            ClearToEmptyStr(ext);
            return ERANGE;
        }
        azstrncpy(fname, nameBufferSize, fileNameLocation, fileNameLength);
        fname[fileNameLength] = '\0';
    }

    if (fileNameLocation == path)
    {
        // The entire path is the filename (+ possible extension handled above)
        return 0;
    }

    if (dir != nullptr)
    {
        const size_t dirLength = fileNameLocation - path;
        if (dirLength >= dirBufferSize) // account for null terminator
        {
            // Clear buffers set previously to maintain consistency with msvc implementation:
            // https://msdn.microsoft.com/en-us/library/8e46eyt7.aspx
            ClearToEmptyStr(ext);
            ClearToEmptyStr(fname);
            return ERANGE;
        }
        azstrncpy(dir, dirBufferSize, path, dirLength);
        dir[dirLength] = '\0';
    }

    // 'drive' was already set to an empty string above.
    // While windows is the only supported platform with
    // the concept of drives, not sure whether we expect
    // input of the form C:\dir\filename.ext to function
    // the same as it would on windows?

    return 0;
}
#endif

namespace AzFramework
{
    namespace StringFunc
    {
        bool Equal(const char* inA, const char* inB, bool bCaseSensitive /*= false*/, size_t n /*= 0*/)
        {
            if (!inA || !inB)
            {
                return false;
            }

            if (inA == inB)
            {
                return true;
            }

            if (bCaseSensitive)
            {
                if (n)
                {
                    return !strncmp(inA, inB, n);
                }
                else
                {
                    return !strcmp(inA, inB);
                }
            }
            else
            {
                if (n)
                {
                    return !azstrnicmp(inA, inB, n);
                }
                else
                {
                    return !azstricmp(inA, inB);
                }
            }
        }

        size_t Find(const char* in, char c, size_t pos /*= 0*/, bool bReverse /*= false*/, bool bCaseSensitive /*= false*/)
        {
            if (!in)
            {
                return AZStd::string::npos;
            }

            if (pos == AZStd::string::npos)
            {
                pos = 0;
            }

            size_t inLen = strlen(in);
            if (inLen < pos)
            {
                return AZStd::string::npos;
            }

            if (!bCaseSensitive)
            {
                c = (char)tolower(c);
            }

            if (bReverse)
            {
                pos = inLen - pos - 1;
            }

            char character;

            do
            {
                if (!bCaseSensitive)
                {
                    character = (char)tolower(in[pos]);
                }
                else
                {
                    character = in[pos];
                }

                if (character == c)
                {
                    return pos;
                }

                if (bReverse)
                {
                    pos = pos > 0 ? pos-1 : pos;
                }
                else
                {
                    pos++;
                }
            } while (bReverse ? pos : character != '\0');

            return AZStd::string::npos;
        }

        size_t Find(const char* in, const char* s, size_t offset /*= 0*/, bool bReverse /*= false*/, bool bCaseSensitive /*= false*/)
        {
            if (!in || !s)
            {
                return AZStd::string::npos;
            }

            size_t inlen = strlen(in);
            if (!inlen)
            {
                return AZStd::string::npos;
            }

            if (offset == AZStd::string::npos)
            {
                offset = 0;
            }

            size_t slen = strlen(s);
            if (slen == 0)
            {
                return AZStd::string::npos;
            }

            if (offset + slen > inlen)
            {
                return AZStd::string::npos;
            }

            const char* pCur;

            if (bReverse)
            {
                // Start at the end (- pos)
                pCur = in + inlen - slen - offset;
            }
            else
            {
                // Start at the beginning (+ pos)
                pCur = in + offset;
            }

            do
            {
                if (bCaseSensitive)
                {
                    if (!strncmp(pCur, s, slen))
                    {
                        return static_cast<size_t>(pCur - in);
                    }
                }
                else
                {
                    if (!azstrnicmp(pCur, s, slen))
                    {
                        return static_cast<size_t>(pCur - in);
                    }
                }

                if (bReverse)
                {
                    pCur--;
                }
                else
                {
                    pCur++;
                }
            } while (bReverse ? pCur >= in : pCur - in <= static_cast<ptrdiff_t>(inlen));

            return AZStd::string::npos;
        }

        bool Replace(AZStd::string& inout, const char replaceA, const char withB, bool bCaseSensitive /*= false*/, bool bReplaceFirst /*= false*/, bool bReplaceLast /*= false*/)
        {
            bool bSomethingWasReplaced = false;
            size_t pos = 0;

            if (!bReplaceFirst && !bReplaceLast)
            {
                //replace all
                if (bCaseSensitive)
                {
                    bSomethingWasReplaced = false;
                    while ((pos = inout.find(replaceA, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasReplaced = true;
                        inout.replace(pos, 1, 1, withB);
                        pos++;
                    }
                }
                else
                {
                    AZStd::string lowercaseIn(inout);
                    AZStd::to_lower(lowercaseIn.begin(), lowercaseIn.end());

                    char lowercaseReplaceA = (char)tolower(replaceA);

                    while ((pos = lowercaseIn.find(lowercaseReplaceA, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasReplaced = true;
                        inout.replace(pos, 1, 1, withB);
                        pos++;
                    }
                }
            }
            else
            {
                if (bCaseSensitive)
                {
                    if (bReplaceFirst)
                    {
                        //replace first
                        if ((pos = inout.find_first_of(replaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, 1, 1, withB);
                        }
                    }

                    if (bReplaceLast)
                    {
                        //replace last
                        if ((pos = inout.find_last_of(replaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, 1, 1, withB);
                        }
                    }
                }
                else
                {
                    AZStd::string lowercaseIn(inout);
                    AZStd::to_lower(lowercaseIn.begin(), lowercaseIn.end());

                    char lowercaseReplaceA = (char)tolower(replaceA);

                    if (bReplaceFirst)
                    {
                        //replace first
                        if ((pos = lowercaseIn.find_first_of(lowercaseReplaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, 1, 1, withB);
                            if (bReplaceLast)
                            {
                                lowercaseIn.replace(pos, 1, 1, withB);
                            }
                        }
                    }

                    if (bReplaceLast)
                    {
                        //replace last
                        if ((pos = lowercaseIn.find_last_of(lowercaseReplaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, 1, 1, withB);
                        }
                    }
                }
            }

            return bSomethingWasReplaced;
        }

        bool Replace(AZStd::string& inout, const char* replaceA, const char* withB, bool bCaseSensitive /*= false*/, bool bReplaceFirst /*= false*/, bool bReplaceLast /*= false*/)
        {
            if (!replaceA) //withB can be nullptr
            {
                return false;
            }

            size_t lenA = strlen(replaceA);
            if (!lenA)
            {
                return false;
            }

            const char* emptystring = "";
            if (!withB)
            {
                withB = emptystring;
            }

            size_t lenB = strlen(withB);

            bool bSomethingWasReplaced = false;

            size_t pos = 0;

            if (!bReplaceFirst && !bReplaceLast)
            {
                //replace all
                if (bCaseSensitive)
                {
                    while ((pos = inout.find(replaceA, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasReplaced = true;
                        inout.replace(pos, lenA, withB, lenB);
                        pos += lenB;
                    }
                }
                else
                {
                    AZStd::string lowercaseIn(inout);
                    AZStd::to_lower(lowercaseIn.begin(), lowercaseIn.end());

                    AZStd::string lowercaseReplaceA(replaceA);
                    AZStd::to_lower(lowercaseReplaceA.begin(), lowercaseReplaceA.end());

                    while ((pos = lowercaseIn.find(lowercaseReplaceA, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasReplaced = true;
                        lowercaseIn.replace(pos, lenA, withB, lenB);
                        inout.replace(pos, lenA, withB, lenB);
                        pos += lenB;
                    }
                }
            }
            else
            {
                if (bCaseSensitive)
                {
                    if (bReplaceFirst)
                    {
                        //replace first
                        if ((pos = inout.find(replaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, lenA, withB, lenB);
                        }
                    }

                    if (bReplaceLast)
                    {
                        //replace last
                        if ((pos = inout.rfind(replaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, lenA, withB, lenB);
                        }
                    }
                }
                else
                {
                    AZStd::string lowercaseIn(inout);
                    AZStd::to_lower(lowercaseIn.begin(), lowercaseIn.end());

                    AZStd::string lowercaseReplaceA(replaceA);
                    AZStd::to_lower(lowercaseReplaceA.begin(), lowercaseReplaceA.end());

                    if (bReplaceFirst)
                    {
                        //replace first
                        if ((pos = lowercaseIn.find(lowercaseReplaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, lenA, withB, lenB);
                            if (bReplaceLast)
                            {
                                lowercaseIn.replace(pos, lenA, withB, lenB);
                            }
                        }
                    }

                    if (bReplaceLast)
                    {
                        //replace last
                        if ((pos = lowercaseIn.rfind(lowercaseReplaceA)) != AZStd::string::npos)
                        {
                            bSomethingWasReplaced = true;
                            inout.replace(pos, lenA, withB, lenB);
                        }
                    }
                }
            }

            return bSomethingWasReplaced;
        }

        bool Strip(AZStd::string& inout, const char stripCharacter /*= ' '*/, bool bCaseSensitive /*= false*/, bool bStripBeginning /*= false*/, bool bStripEnding /*= false*/)
        {
            bool bSomethingWasStripped = false;
            size_t pos = 0;
            if (!bStripBeginning && !bStripEnding)
            {
                //strip all
                if (bCaseSensitive)
                {
                    while ((pos = inout.find_first_of(stripCharacter, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasStripped = true;
                        inout.erase(pos, 1);
                    }
                }
                else
                {
                    char lowerStripCharacter = (char)tolower(stripCharacter);
                    char upperStripCharacter = (char)toupper(stripCharacter);
                    while ((pos = inout.find_first_of(lowerStripCharacter, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasStripped = true;
                        inout.erase(pos, 1);
                    }
                    pos = 0;
                    while ((pos = inout.find_first_of(upperStripCharacter, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasStripped = true;
                        inout.erase(pos, 1);
                    }
                }
            }
            else
            {
                if (bCaseSensitive)
                {
                    if (bStripBeginning)
                    {
                        //strip beginning
                        if ((pos = inout.find_first_not_of(stripCharacter)) != AZStd::string::npos)
                        {
                            if (pos != 0)
                            {
                                bSomethingWasStripped = true;
                                RKeep(inout, pos, true);
                            }
                        }
                    }

                    if (bStripEnding)
                    {
                        //strip ending
                        if ((pos = inout.find_last_not_of(stripCharacter)) != AZStd::string::npos)
                        {
                            if (pos != inout.length())
                            {
                                bSomethingWasStripped = true;
                                LKeep(inout, pos, true);
                            }
                        }
                    }
                }
                else
                {
                    AZStd::string combinedStripCharacters;
                    combinedStripCharacters += (char)tolower(stripCharacter);
                    combinedStripCharacters += (char)toupper(stripCharacter);

                    if (bStripBeginning)
                    {
                        //strip beginning
                        if ((pos = inout.find_first_not_of(combinedStripCharacters)) != AZStd::string::npos)
                        {
                            if (pos != 0)
                            {
                                bSomethingWasStripped = true;
                                RKeep(inout, pos, true);
                            }
                        }
                    }

                    if (bStripEnding)
                    {
                        //strip ending
                        if ((pos = inout.find_last_not_of(combinedStripCharacters)) != AZStd::string::npos)
                        {
                            if (pos != inout.length())
                            {
                                bSomethingWasStripped = true;
                                LKeep(inout, pos, true);
                            }
                        }
                    }
                }
            }
            return bSomethingWasStripped;
        }

        bool Strip(AZStd::string& inout, const char* stripCharacters /*= " "*/, bool bCaseSensitive /*= false*/, bool bStripBeginning /*= false*/, bool bStripEnding /*= false*/)
        {
            bool bSomethingWasStripped = false;
            size_t pos = 0;
            if (!bStripBeginning && !bStripEnding)
            {
                //strip all
                if (bCaseSensitive)
                {
                    while ((pos = inout.find_first_of(stripCharacters, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasStripped = true;
                        inout.erase(pos, 1);
                    }
                }
                else
                {
                    AZStd::string lowercaseStripCharacters(stripCharacters);
                    AZStd::to_lower(lowercaseStripCharacters.begin(), lowercaseStripCharacters.end());
                    AZStd::string combinedStripCharacters(stripCharacters);
                    AZStd::to_upper(combinedStripCharacters.begin(), combinedStripCharacters.end());
                    combinedStripCharacters += lowercaseStripCharacters;

                    while ((pos = inout.find_first_of(combinedStripCharacters, pos)) != AZStd::string::npos)
                    {
                        bSomethingWasStripped = true;
                        inout.erase(pos, 1);
                    }
                }
            }
            else
            {
                if (bCaseSensitive)
                {
                    if (bStripBeginning)
                    {
                        //strip beginning
                        if ((pos = inout.find_first_not_of(stripCharacters)) != AZStd::string::npos)
                        {
                            if (pos != 0)
                            {
                                bSomethingWasStripped = true;
                                RKeep(inout, pos, true);
                            }
                        }
                    }

                    if (bStripEnding)
                    {
                        //strip ending
                        if ((pos = inout.find_last_not_of(stripCharacters)) != AZStd::string::npos)
                        {
                            if (pos != inout.length())
                            {
                                bSomethingWasStripped = true;
                                LKeep(inout, pos, true);
                            }
                        }
                    }
                }
                else
                {
                    AZStd::string lowercaseStripCharacters(stripCharacters);
                    AZStd::to_lower(lowercaseStripCharacters.begin(), lowercaseStripCharacters.end());
                    AZStd::string combinedStripCharacters(stripCharacters);
                    AZStd::to_upper(combinedStripCharacters.begin(), combinedStripCharacters.end());
                    combinedStripCharacters += lowercaseStripCharacters;

                    if (bStripBeginning)
                    {
                        //strip beginning
                        if ((pos = inout.find_first_not_of(combinedStripCharacters)) != AZStd::string::npos)
                        {
                            if (pos != 0)
                            {
                                bSomethingWasStripped = true;
                                RKeep(inout, pos, true);
                            }
                        }
                    }

                    if (bStripEnding)
                    {
                        //strip ending
                        if ((pos = inout.find_last_not_of(combinedStripCharacters)) != AZStd::string::npos)
                        {
                            if (pos != inout.length())
                            {
                                bSomethingWasStripped = true;
                                LKeep(inout, pos, true);
                            }
                        }
                    }
                }
            }
            return bSomethingWasStripped;
        }

        void Tokenize(const char* instr, AZStd::vector<AZStd::string>& tokens, const char delimiter /* = ','*/, bool keepEmptyStrings /*= false*/, bool keepSpaceStrings /*= false*/)
        {
            if (!instr)
            {
                return;
            }

            AZStd::string in(instr);
            if (in.empty())
            {
                return;
            }

            size_t pos, lastPos = 0;
            bool bDone = false;
            while (!bDone)
            {
                if ((pos = in.find_first_of(delimiter, lastPos)) == AZStd::string::npos)
                {
                    bDone = true;
                    pos = in.length();
                }

                AZStd::string newElement(AZStd::string(in.data() + lastPos, pos - lastPos));
                bool bIsEmpty = newElement.empty();
                bool bIsSpaces = false;
                if (!bIsEmpty)
                {
                    bIsSpaces = Strip(newElement, ' ') && newElement.empty();
                }

                if ((!bIsEmpty && !bIsSpaces) ||
                    (bIsEmpty && keepEmptyStrings) ||
                    (bIsSpaces && keepSpaceStrings))
                {
                    tokens.push_back(AZStd::string(in.data() + lastPos, pos - lastPos));
                }

                lastPos = pos + 1;
            }
        };

        void Tokenize(const char* instr, AZStd::vector<AZStd::string>& tokens, const char* delimiters /* = ", "*/, bool keepEmptyStrings /*= false*/, bool keepSpaceStrings /*= false*/)
        {
            if (!instr || !delimiters)
            {
                return;
            }

            AZStd::string in(instr);
            if (in.empty())
            {
                return;
            }

            size_t pos, lastPos = 0;
            bool bDone = false;
            while (!bDone)
            {
                if ((pos = in.find_first_of(delimiters, lastPos)) == AZStd::string::npos)
                {
                    bDone = true;
                    pos = in.length();
                }

                AZStd::string newElement(AZStd::string(in.data() + lastPos, pos - lastPos));
                bool bIsEmpty = newElement.empty();
                bool bIsSpaces = false;
                if (!bIsEmpty)
                {
                    bIsSpaces = Strip(newElement, ' ') && newElement.empty();
                }

                if ((bIsEmpty && keepEmptyStrings) ||
                    (bIsSpaces && keepSpaceStrings) ||
                    (!bIsSpaces && !bIsEmpty))
                {
                    tokens.push_back(AZStd::string(in.data() + lastPos, pos - lastPos));
                }

                lastPos = pos + 1;
            }
        };

        size_t UniqueCharacters(const char* in, bool bCaseSensitive /* = false*/)
        {
            if (!in)
            {
                return 0;
            }

            AZStd::set<char> norepeats;

            size_t len = strlen(in);
            for (size_t i = 0; i < len; ++i)
            {
                norepeats.insert(bCaseSensitive ? in[i] : (char)tolower(in[i]));
            }

            return norepeats.size();
        }

        size_t CountCharacters(const char* in, char c, bool bCaseSensitive /* = false*/)
        {
            if (!in)
            {
                return 0;
            }

            size_t len = strlen(in);
            if (!len)
            {
                return 0;
            }

            size_t count = 0;
            for (size_t i = 0; i < len; ++i)
            {
                if (bCaseSensitive)
                {
                    if (in[i] == c)
                    {
                        count++;
                    }
                }
                else
                {
                    char lowerc = (char)tolower(c);
                    if (tolower(in[i]) == lowerc)
                    {
                        count++;
                    }
                }
            }

            return count;
        }

        bool LooksLikeInt(const char* in, int* pInt /*=nullptr*/)
        {
            if (!in)
            {
                return false;
            }

            //if pos is past then end of the string false
            size_t len = strlen(in);
            if (!len)//must at least 1 characters to work with "1"
            {
                return false;
            }

            const char* pStr = in;

            size_t countNeg = 0;
            while (*pStr != '\0' &&
                   (isdigit(*pStr) ||
                    *pStr == '-'))
            {
                if (*pStr == '-')
                {
                    countNeg++;
                }
                pStr++;
            }

            if (*pStr == '\0' &&
                countNeg < 2)
            {
                if (pInt)
                {
                    *pInt = ToInt(in);
                }

                return true;
            }
            return false;
        }

        bool LooksLikeFloat(const char* in, float* pFloat /* = nullptr */)
        {
            if (!in)
            {
                return false;
            }

            size_t len = strlen(in);
            if (len < 2)//must have at least 2 characters to work with "1."
            {
                return false;
            }

            const char* pStr = in;

            size_t countDot = 0;
            size_t countNeg = 0;
            while (*pStr != '\0' &&
                   (isdigit(*pStr) ||
                    (*pStr == '-' ||
                     *pStr == '.')))
            {
                if (*pStr == '.')
                {
                    countDot++;
                }
                if (*pStr == '-')
                {
                    countNeg++;
                }
                pStr++;
            }

            if (*pStr == '\0' &&
                countDot == 1 &&
                countNeg < 2)
            {
                if (pFloat)
                {
                    *pFloat = ToFloat(in);
                }

                return true;
            }

            return false;
        }

        bool LooksLikeBool(const char* in, bool* pBool /* = nullptr */)
        {
            if (!in)
            {
                return false;
            }

            size_t len = strlen(in);
            if (len < 4) //cant be less than 4 characters and match "true", "false" is even more
            {
                return false;
            }

            if (!azstricmp(in, "true"))
            {
                if (pBool)
                {
                    *pBool = true;
                }
                return true;
            }
            if (!azstricmp(in, "false"))
            {
                if (pBool)
                {
                    *pBool = false;
                }
                return true;
            }

            return false;
        }

        bool ToHexDump(const char* in, AZStd::string& out)
        {
            struct TInline
            {
                static void ByteToHex(char* pszHex, unsigned char bValue)
                {
                    pszHex[0] = bValue / 16;

                    if (pszHex[0] < 10)
                    {
                        pszHex[0] += '0';
                    }
                    else
                    {
                        pszHex[0] -= 10;
                        pszHex[0] += 'A';
                    }

                    pszHex[1] = bValue % 16;

                    if (pszHex[1] < 10)
                    {
                        pszHex[1] += '0';
                    }
                    else
                    {
                        pszHex[1] -= 10;
                        pszHex[1] += 'A';
                    }
                }
            };

            size_t len = strlen(in);
            if (len < 1) //must be at least 1 character to work with
            {
                return false;
            }

            size_t nBytes = len;

            char* pszData = reinterpret_cast<char*>(azmalloc((nBytes * 2) + 1));

            for (size_t ii = 0; ii < nBytes; ++ii)
            {
                TInline::ByteToHex(&pszData[ii * 2], in[ii]);
            }

            pszData[nBytes * 2] = 0x00;
            out = pszData;
            azfree(pszData);

            return true;
        }

        bool FromHexDump(const char* in, AZStd::string& out)
        {
            struct TInline
            {
                static unsigned char HexToByte(const char* pszHex)
                {
                    unsigned char bHigh = 0;
                    unsigned char bLow = 0;

                    if ((pszHex[0] >= '0') && (pszHex[0] <= '9'))
                    {
                        bHigh = pszHex[0] - '0';
                    }
                    else if ((pszHex[0] >= 'A') && (pszHex[0] <= 'F'))
                    {
                        bHigh = (pszHex[0] - 'A') + 10;
                    }

                    bHigh = bHigh << 4;

                    if ((pszHex[1] >= '0') && (pszHex[1] <= '9'))
                    {
                        bLow = pszHex[1] - '0';
                    }
                    else if ((pszHex[1] >= 'A') && (pszHex[1] <= 'F'))
                    {
                        bLow = (pszHex[1] - 'A') + 10;
                    }

                    return bHigh | bLow;
                }
            };

            size_t len = strlen(in);
            if (len < 2) //must be at least 2 characters to work with
            {
                return false;
            }

            size_t nBytes = len / 2;
            char* pszData = reinterpret_cast<char*>(azmalloc(nBytes + 1));

            for (size_t ii = 0; ii < nBytes; ++ii)
            {
                pszData[ii] = TInline::HexToByte(&in[ii * 2]);
            }

            pszData[nBytes] = 0x00;
            out = pszData;
            azfree(pszData);

            return true;
        }

        namespace AssetDatabasePath
        {
            bool Normalize(AZStd::string& inout)
            {
                Strip(inout, AZ_DATABASE_INVALID_CHARACTERS);

    #ifndef AZ_FILENAME_ALLOW_SPACES
                Strip(inout, AZ_SPACE_CHARACTERS);
    #endif // AZ_FILENAME_ALLOW_SPACES

                //too small to be a path
                AZStd::size_t len = inout.length();
                if (!len)
                {
                    return false;
                }

                //too big to be a path fail
                if (len > AZ_MAX_PATH_LEN)
                {
                    return false;
                }

                AZStd::replace(inout.begin(), inout.end(), AZ_WRONG_DATABASE_SEPARATOR, AZ_CORRECT_DATABASE_SEPARATOR);
                Replace(inout, AZ_DOUBLE_CORRECT_DATABASE_SEPARTOR, AZ_CORRECT_DATABASE_SEPARATOR_STRING);

                return IsValid(inout.c_str());
            }

            bool IsValid(const char* in)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                if (Find(in, AZ_DATABASE_INVALID_CHARACTERS) != AZStd::string::npos)
                {
                    return false;
                }

                if (Find(in, AZ_WRONG_DATABASE_SEPARATOR) != AZStd::string::npos)
                {
                    return false;
                }

    #ifndef AZ_FILENAME_ALLOW_SPACES
                if (Find(in, AZ_SPACE_CHARACTERS) != AZStd::string::npos)
                {
                    return false;
                }
    #endif // AZ_FILENAME_ALLOW_SPACES

                if (LastCharacter(in) == AZ_CORRECT_DATABASE_SEPARATOR)
                {
                    return false;
                }

                return true;
            }

            bool ConstructFull(const char* pProjectRoot, const char* pDatabaseRoot, const char* pDatabasePath, const char* pDatabaseFile, const char* pFileExtension, AZStd::string& out)
            {
                if (!pProjectRoot)
                {
                    return false;
                }

                if (!pDatabaseRoot)
                {
                    return false;
                }

                if (!pDatabasePath)
                {
                    return false;
                }

                if (!pDatabaseFile)
                {
                    return false;
                }

                if (!strlen(pProjectRoot))
                {
                    return false;
                }

                if (!strlen(pDatabaseRoot))
                {
                    return false;
                }

                if (!strlen(pDatabaseFile))
                {
                    return false;
                }

                if (pFileExtension && strlen(pFileExtension))
                {
                    if (Find(pFileExtension, AZ_CORRECT_DATABASE_SEPARATOR) != AZStd::string::npos)
                    {
                        return false;
                    }

                    if (Find(pFileExtension, AZ_WRONG_DATABASE_SEPARATOR) != AZStd::string::npos)
                    {
                        return false;
                    }
                }

                AZStd::string tempProjectRoot;
                AZStd::string tempDatabaseRoot;
                AZStd::string tempDatabasePath;
                AZStd::string tempDatabaseFile;
                AZStd::string tempFileExtension;
                if (pProjectRoot == out.c_str() || pDatabaseRoot == out.c_str() || pDatabasePath == out.c_str() || pDatabaseFile == out.c_str() || pFileExtension == out.c_str())
                {
                    tempProjectRoot = pProjectRoot;
                    tempDatabaseRoot = pDatabaseRoot;
                    tempDatabasePath = pDatabasePath;
                    tempDatabaseFile = pDatabaseFile;
                    tempFileExtension = pFileExtension;
                    pProjectRoot = tempProjectRoot.c_str();
                    pDatabaseRoot = tempDatabaseRoot.c_str();
                    pDatabasePath = tempDatabasePath.c_str();
                    pDatabaseFile = tempDatabaseFile.c_str();
                    pFileExtension = tempFileExtension.c_str();
                }

                AZStd::string projectRoot = pProjectRoot;
                if (!Root::Normalize(projectRoot))
                {
                    return false;
                }

                AZStd::string databasePath = pDatabasePath;
                if (!RelativePath::Normalize(databasePath))
                {
                    return false;
                }

                if (!Path::Join(projectRoot.c_str(), pDatabaseRoot, out))
                {
                    return false;
                }

                if (!databasePath.empty())
                {
                    if (!Path::Join(out.c_str(), databasePath.c_str(), out))
                    {
                        return false;
                    }
                }

                if (!Path::Join(out.c_str(), pDatabaseFile, out))
                {
                    return false;
                }

                if (pFileExtension)
                {
                    Path::ReplaceExtension(out, pFileExtension);
                }

                return Path::IsValid(out.c_str());
            }

            bool Split(const char* in, AZStd::string* pDstProjectRootOut /*= nullptr*/, AZStd::string* pDstDatabaseRootOut /*= nullptr*/, AZStd::string* pDstDatabasePathOut /*= nullptr*/, AZStd::string* pDstFileOut /*= nullptr*/, AZStd::string* pDstFileExtensionOut /*= nullptr*/)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string temp(in);
                if (!Normalize(temp))
                {
                    return false;
                }

                if (temp.empty())
                {
                    return false;
                }

                if (pDstProjectRootOut)
                {
                    pDstProjectRootOut->clear();
                }

                if (pDstDatabaseRootOut)
                {
                    pDstDatabaseRootOut->clear();
                }

                if (pDstDatabasePathOut)
                {
                    pDstDatabasePathOut->clear();
                }

                if (pDstFileOut)
                {
                    pDstFileOut->clear();
                }

                if (pDstFileExtensionOut)
                {
                    pDstFileExtensionOut->clear();
                }
                //////////////////////////////////////////////////////////////////////////
                AZStd::size_t lastExt = AZStd::string::npos;
                if ((lastExt = Find(temp.c_str(), AZ_DATABASE_EXTENSION_SEPARATOR, AZStd::string::npos, true)) != AZStd::string::npos)
                {
                    AZStd::size_t lastSep = AZStd::string::npos;
                    if ((lastSep = Find(temp.c_str(), AZ_CORRECT_DATABASE_SEPARATOR, AZStd::string::npos, true)) != AZStd::string::npos)
                    {
                        if (lastSep > lastExt)
                        {
                            lastExt = AZStd::string::npos;
                        }
                    }

                    if (lastExt != AZStd::string::npos)
                    {
                        //we found a file extension
                        if (pDstFileExtensionOut)
                        {
                            *pDstFileExtensionOut = temp;
                            RKeep(*pDstFileExtensionOut, lastExt, true);
                        }

                        LKeep(temp, lastExt);
                    }
                }

                AZStd::size_t lastSep = AZStd::string::npos;
                if ((lastSep = Find(temp.c_str(), AZ_CORRECT_DATABASE_SEPARATOR, AZStd::string::npos, true)) != AZStd::string::npos)
                {
                    if (pDstFileOut)
                    {
                        *pDstFileOut = temp;
                        RKeep(*pDstFileOut, lastSep);
                    }
                    LKeep(temp, lastSep, true);
                }
                else if (!temp.empty())
                {
                    if (pDstFileOut)
                    {
                        *pDstFileOut = temp;
                    }
                    temp.clear();
                }

                if (pDstDatabasePathOut)
                {
                    *pDstDatabasePathOut = temp;
                }

                return true;
            }

            bool Join(const char* pFirstPart, const char* pSecondPart, AZStd::string& out, bool bJoinOverlapping /*= false*/, bool bCaseInsenitive /*= true*/, bool bNormalize /*= true*/)
            {
                //null && null
                if (!pFirstPart && !pSecondPart)
                {
                    return false;
                }

                size_t firstPartLen = 0;
                size_t secondPartLen = 0;
                if (pFirstPart)
                {
                    firstPartLen = strlen(pFirstPart);
                }
                if (pSecondPart)
                {
                    secondPartLen = strlen(pSecondPart);
                }

                //0 && 0
                if (!firstPartLen && !secondPartLen)
                {
                    return false;
                }

                //null good
                if (!pFirstPart && pSecondPart)
                {
                    //null 0
                    if (!secondPartLen)
                    {
                        return false;
                    }

                    out = pSecondPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                //good null
                if (!pSecondPart && pFirstPart)
                {
                    //0 null
                    if (!firstPartLen)
                    {
                        return false;
                    }

                    out = pFirstPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                //0 good
                if (!firstPartLen && pSecondPart)
                {
                    out = pSecondPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                //good 0
                if (pFirstPart && !secondPartLen)
                {
                    out = pFirstPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                if (Path::HasDrive(pSecondPart))
                {
                    return false;
                }

                AZStd::string tempFirst;
                AZStd::string tempSecond;
                if (pFirstPart == out.c_str() || pSecondPart == out.c_str())
                {
                    tempFirst = pFirstPart;
                    tempSecond = pSecondPart;
                    pFirstPart = tempFirst.c_str();
                    pSecondPart = tempSecond.c_str();
                }

                out = pFirstPart;

                if (bJoinOverlapping)
                {
                    AZStd::string firstPart(pFirstPart);
                    AZStd::string secondPart(pSecondPart);
                    if (bCaseInsenitive)
                    {
                        AZStd::to_lower(firstPart.begin(), firstPart.end());
                        AZStd::to_lower(secondPart.begin(), secondPart.end());
                    }

                    AZStd::vector<AZStd::string> firstPartDelimited;
                    Strip(firstPart, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Tokenize(firstPart.c_str(), firstPartDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);
                    AZStd::vector<AZStd::string> secondPartDelimited;
                    Strip(secondPart, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Tokenize(secondPart.c_str(), secondPartDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);
                    AZStd::vector<AZStd::string> secondPartNormalDelimited;

                    for (size_t i = 0; i < firstPartDelimited.size(); ++i)
                    {
                        if (firstPartDelimited[i].length() == secondPartDelimited[0].length())
                        {
                            if (!strncmp(firstPartDelimited[i].c_str(), secondPartDelimited[0].c_str(), firstPartDelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of firstPart Must Be equal or we fail
                                bool bFailed = false;
                                size_t jj = 1;
                                for (size_t ii = i + 1; !bFailed && ii < firstPartDelimited.size(); ++ii)
                                {
                                    if (firstPartDelimited[ii].length() != secondPartDelimited[jj].length())
                                    {
                                        bFailed = true;
                                    }
                                    else if (strncmp(firstPartDelimited[ii].c_str(), secondPartDelimited[jj].c_str(), firstPartDelimited[ii].length()))
                                    {
                                        bFailed = true;
                                    }

                                    jj++;
                                }

                                if (!bFailed)
                                {
                                    if (LastCharacter(out.c_str()) != AZ_CORRECT_DATABASE_SEPARATOR)
                                    {
                                        out += AZ_CORRECT_DATABASE_SEPARATOR;
                                    }

                                    AZStd::string secondPartNormal(pSecondPart);
                                    Strip(secondPartNormal, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                                    Tokenize(secondPartNormal.c_str(), secondPartNormalDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);

                                    for (; jj < secondPartNormalDelimited.size(); ++jj)
                                    {
                                        out += secondPartNormalDelimited[jj];
                                        out += AZ_CORRECT_DATABASE_SEPARATOR;
                                    }

                                    if (LastCharacter(pSecondPart) != AZ_CORRECT_DATABASE_SEPARATOR)
                                    {
                                        RChop(out);
                                    }

                                    return true;
                                }
                            }
                        }
                    }
                }

                if (LastCharacter(pFirstPart) == AZ_CORRECT_DATABASE_SEPARATOR && FirstCharacter(pSecondPart) == AZ_CORRECT_DATABASE_SEPARATOR)
                {
                    Strip(out, AZ_CORRECT_DATABASE_SEPARATOR, false, false, true);
                }

                if (LastCharacter(pFirstPart) != AZ_CORRECT_DATABASE_SEPARATOR && FirstCharacter(pSecondPart) != AZ_CORRECT_DATABASE_SEPARATOR)
                {
                    Append(out, AZ_CORRECT_DATABASE_SEPARATOR);
                }

                out += pSecondPart;

                if (bNormalize)
                {
                    return Normalize(out);
                }

                return true;
            }

            bool IsASuperFolderOfB(const char* pathA, const char* pathB, bool bCaseInsenitive /*= true*/, bool bIgnoreStartingPath /*= true*/)
            {
                if (!pathA || !pathB)
                {
                    return false;
                }

                if (pathA == pathB)
                {
                    return false;
                }

                AZStd::string strPathA(pathA);
                if (!strPathA.length())
                {
                    return false;
                }

                AZStd::string strPathB(pathB);
                if (!strPathB.length())
                {
                    return false;
                }

                if (bIgnoreStartingPath)
                {
                    Path::StripDrive(strPathA);
                    Path::StripDrive(strPathB);

                    Strip(strPathA, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                }
                else
                {
                    Strip(strPathA, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);

                    size_t lenA = strPathA.length();
                    size_t lenB = strPathB.length();
                    if (lenA >= lenB)
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strPathA.c_str(), strPathB.c_str(), lenA))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strPathA.c_str(), strPathB.c_str(), lenA))
                        {
                            return false;
                        }
                    }
                }

                AZStd::vector<AZStd::string> pathADelimited;
                Tokenize(strPathA.c_str(), pathADelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);
                AZStd::vector<AZStd::string> pathBDelimited;
                Tokenize(strPathB.c_str(), pathBDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);

                //EX: A= p4/Main/Source/GameAssets/gameinfo
                //    B= p4/Main/Source/GameAssets/gameinfo/Characters
                if (bIgnoreStartingPath)
                {
                    for (size_t i = 0; i < pathADelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[0].length())
                        {
                        }
                        else if (bCaseInsenitive)
                        {
                            if (!azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of A Must Be equal or we fail

                                //if the number of components after this match in A are more than
                                //the remaining in B then fail as it can not be a super folder
                                if (pathADelimited.size() - i >= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t jj = 1;
                                for (size_t ii = i + 1; ii < pathADelimited.size(); ++ii)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    else if (azstrnicmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    jj++;
                                }
                                return true;
                            }
                        }
                        else
                        {
                            if (!strncmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of A Must Be equal or we fail

                                //if the number of components after this match in A are more than
                                //the remaining in B then fail as it can not be a super folder
                                if (pathADelimited.size() - i >= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t jj = 1;
                                for (size_t ii = i + 1; ii < pathADelimited.size(); ++ii)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    if (strncmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    jj++;
                                }
                                return true;
                            }
                        }
                    }
                    return false;
                }
                else
                {
                    //if the number of components after this match in A are more than
                    //the remaining in B then fail as it can not be a super folder
                    if (pathADelimited.size() >= pathBDelimited.size())
                    {
                        return false;
                    }

                    for (size_t i = 0; i < pathADelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[i].length())
                        {
                            return false;
                        }
                        else if (bCaseInsenitive)
                        {
                            if (azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            if (strncmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                    }
                    return true;
                }
            }

            bool IsASubFolderOfB(const char* pathA, const char* pathB, bool bCaseInsenitive /*= true*/, bool bIgnoreStartingPath /*= true*/)
            {
                if (!pathA || !pathB)
                {
                    return false;
                }

                if (pathA == pathB)
                {
                    return false;
                }

                AZStd::string strPathA(pathA);
                if (!strPathA.length())
                {
                    return false;
                }

                AZStd::string strPathB(pathB);
                if (!strPathB.length())
                {
                    return false;
                }

                if (bIgnoreStartingPath)
                {
                    Path::StripDrive(strPathA);
                    Path::StripDrive(strPathB);

                    Strip(strPathA, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                }
                else
                {
                    Strip(strPathA, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);

                    size_t lenA = strPathA.length();
                    size_t lenB = strPathB.length();
                    if (lenA < lenB)
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strPathA.c_str(), strPathB.c_str(), lenB))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strPathA.c_str(), strPathB.c_str(), lenB))
                        {
                            return false;
                        }
                    }
                }

                AZStd::vector<AZStd::string> pathADelimited;
                Tokenize(strPathA.c_str(), pathADelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);
                AZStd::vector<AZStd::string> pathBDelimited;
                Tokenize(strPathB.c_str(), pathBDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);

                //EX: A= p4/Main/Source/GameAssets/gameinfo/Characters
                //    B= p4/Main/Source/GameAssets/gameinfo
                if (bIgnoreStartingPath)
                {
                    for (size_t i = 0; i < pathADelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[0].length())
                        {
                        }
                        else if (bCaseInsenitive)
                        {
                            if (!azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of B Must Be equal or we fail

                                //if the number of components after this match in A has to be greater
                                //then B or it can not be a sub folder
                                if (pathADelimited.size() - i <= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t ii = i + 1;
                                for (size_t jj = 1; jj < pathBDelimited.size(); ++jj)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    else if (azstrnicmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    ii++;
                                }
                                return true;
                            }
                        }
                        else
                        {
                            if (!strncmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of A Must Be equal or we fail

                                //if the number of components after this match in A has to be greater
                                //then B or it can not be a sub folder
                                if (pathADelimited.size() - i <= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t ii = i + 1;
                                for (size_t jj = 1; jj < pathBDelimited.size(); ++jj)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    else if (azstrnicmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    ii++;
                                }
                                return true;
                            }
                        }
                    }
                    return false;
                }
                else
                {
                    if (pathADelimited.size() <= pathBDelimited.size())
                    {
                        return false;
                    }

                    for (size_t i = 0; i < pathBDelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[i].length())
                        {
                            return false;
                        }
                        else if (bCaseInsenitive)
                        {
                            if (azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            if (strncmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                    }
                    return true;
                }
            }

            bool IsFileInFolder(const char* pFilePath, const char* pFolder, bool bIncludeSubTree /*= false*/, bool bCaseInsenitive /*= true*/, bool bIgnoreStartingPath /*= true*/)
            {
                (void)bIncludeSubTree;

                if (!pFilePath || !pFolder)
                {
                    return false;
                }

                AZStd::string strFilePath(pFilePath);
                if (!strFilePath.length())
                {
                    return false;
                }

                AZStd::string strFolder(pFolder);
                if (!strFolder.length())
                {
                    return false;
                }

                Path::StripFullName(strFilePath);

                if (bIgnoreStartingPath)
                {
                    Path::StripDrive(strFilePath);
                    Path::StripDrive(strFolder);

                    Strip(strFilePath, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Strip(strFolder, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                }
                else
                {
                    Strip(strFilePath, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);
                    Strip(strFolder, AZ_CORRECT_DATABASE_SEPARATOR, false, true, true);

                    size_t lenFilePath = strFilePath.length();
                    size_t lenFolder = strFolder.length();
                    if (lenFilePath < lenFolder)
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strFilePath.c_str(), strFolder.c_str(), lenFolder))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strFilePath.c_str(), strFolder.c_str(), lenFolder))
                        {
                            return false;
                        }
                    }
                }

                AZStd::vector<AZStd::string> strFilePathDelimited;
                Tokenize(strFilePath.c_str(), strFilePathDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);
                AZStd::vector<AZStd::string> strFolderDelimited;
                Tokenize(strFolder.c_str(), strFolderDelimited, AZ_CORRECT_DATABASE_SEPARATOR, true);

                //EX: strFilePath= "p4/Main/Source/GameAssets/gameinfo/character"
                //    strFolder= "Main/Source/GameAssets/gameinfo"
                //    = true
                if (bIgnoreStartingPath)
                {
                    bool bFound = false;
                    size_t i = 0;
                    for (i = 0; !bFound && i < strFilePathDelimited.size(); ++i)
                    {
                        if (strFilePathDelimited[i].length() != strFolderDelimited[0].length())
                        {
                        }
                        else if (bCaseInsenitive)
                        {
                            if (!azstrnicmp(strFilePathDelimited[i].c_str(), strFolderDelimited[0].c_str(), strFilePathDelimited[i].length()))
                            {
                                //we found the first component that is equal
                                bFound = true;
                            }
                        }
                        else
                        {
                            if (!strncmp(strFilePathDelimited[i].c_str(), strFolderDelimited[0].c_str(), strFilePathDelimited[i].length()))
                            {
                                //we found the first component that is equal
                                bFound = true;
                            }
                        }
                    }

                    if (bFound)
                    {
                        //we found a match, modify the file path delimited
                        if (i)
                        {
                            strFilePathDelimited.erase(strFilePathDelimited.begin(), strFilePathDelimited.begin() + (i - 1));
                        }
                    }
                    else
                    {
                        for (i = 0; !bFound && i < strFolderDelimited.size(); ++i)
                        {
                            if (strFolderDelimited[i].length() != strFilePathDelimited[0].length())
                            {
                            }
                            else if (bCaseInsenitive)
                            {
                                if (!azstrnicmp(strFolderDelimited[i].c_str(), strFilePathDelimited[0].c_str(), strFolderDelimited[i].length()))
                                {
                                    //we found the first component that is equal
                                    bFound = true;
                                }
                            }
                            else
                            {
                                if (!strncmp(strFolderDelimited[i].c_str(), strFilePathDelimited[0].c_str(), strFolderDelimited[i].length()))
                                {
                                    //we found the first component that is equal
                                    bFound = true;
                                }
                            }
                        }

                        if (bFound)
                        {
                            //we found a match, modify the folder delimited
                            if (i)
                            {
                                strFolderDelimited.erase(strFolderDelimited.begin(), strFolderDelimited.begin() + (i - 1));
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                }

                //EX: strFilePath= "Main/Source/GameAssets/gameinfo/character"
                //    strFolder= "Main/Source/GameAssets/gameinfo"
                //    = true

                if (!bIncludeSubTree && strFilePathDelimited.size() != strFolderDelimited.size())
                {
                    return false;
                }

                if (strFilePathDelimited.size() < strFolderDelimited.size())
                {
                    return false;
                }

                for (size_t i = 0; i < strFolderDelimited.size(); ++i)
                {
                    if (strFilePathDelimited[i].length() != strFolderDelimited[i].length())
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strFilePathDelimited[i].c_str(), strFolderDelimited[i].c_str(), strFilePathDelimited[i].length()))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strFilePathDelimited[i].c_str(), strFolderDelimited[i].c_str(), strFilePathDelimited[i].length()))
                        {
                            return false;
                        }
                    }
                }
                return true;
            }
        }//namespace AssetDatabasePath

        namespace Root
        {
            bool Normalize(AZStd::string& inout)
            {
                Strip(inout, AZ_FILESYSTEM_INVALID_CHARACTERS);

    #ifndef AZ_FILENAME_ALLOW_SPACES
                Strip(inout, AZ_SPACE_CHARACTERS);
    #endif // AZ_FILENAME_ALLOW_SPACES

                AZStd::replace(inout.begin(), inout.end(), AZ_WRONG_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR);

                size_t pos = AZStd::string::npos;
                if ((pos = inout.find(AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR)) == 0)
                {
                    AZStd::string temp = inout;
                    if (!Path::GetDrive(temp.c_str(), inout))
                    {
                        return false;
                    }
                    if (!Path::StripDrive(temp))
                    {
                        return false;
                    }

                    Replace(temp, AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING);
                    inout += temp;
                }
                else if (pos != AZStd::string::npos)
                {
                    Replace(inout, AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING);
                }

                if (LastCharacter(inout.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    Append(inout, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                }

                return IsValid(inout.c_str());
            }

            bool IsValid(const char* in)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                if (Find(in, AZ_FILESYSTEM_INVALID_CHARACTERS) != AZStd::string::npos)
                {
                    return false;
                }

                if (Find(in, AZ_WRONG_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                {
                    return false;
                }

    #ifndef AZ_FILENAME_ALLOW_SPACES
                if (Find(in, AZ_SPACE_CHARACTERS) != AZStd::string::npos)
                {
                    return false;
                }
    #endif // AZ_FILENAME_ALLOW_SPACES

                if (!Path::HasDrive(in))
                {
                    return false;
                }

                if (LastCharacter(in) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    return false;
                }

                return true;
            }
        }//namespace Root

        namespace RelativePath
        {
            bool Normalize(AZStd::string& inout)
            {
                Strip(inout, AZ_FILESYSTEM_INVALID_CHARACTERS);

    #ifndef AZ_FILENAME_ALLOW_SPACES
                Strip(inout, AZ_SPACE_CHARACTERS);
    #endif // AZ_FILENAME_ALLOW_SPACES

                AZStd::replace(inout.begin(), inout.end(), AZ_WRONG_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR);

                size_t pos = AZStd::string::npos;
                if ((pos = inout.find(AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR)) == 0)
                {
                    AZStd::string temp = inout;
                    if (!Path::GetDrive(temp.c_str(), inout))
                    {
                        return false;
                    }
                    if (!Path::StripDrive(temp))
                    {
                        return false;
                    }

                    Replace(temp, AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING);
                    inout += temp;
                }
                else if (pos != AZStd::string::npos)
                {
                    Replace(inout, AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING);
                }

                if (LastCharacter(inout.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    Append(inout, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                }

                if (FirstCharacter(inout.c_str()) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    LChop(inout);
                }

                return IsValid(inout.c_str());
            }

            bool IsValid(const char* in)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return true;
                }

                if (Find(in, AZ_FILESYSTEM_INVALID_CHARACTERS) != AZStd::string::npos)
                {
                    return false;
                }

                if (Find(in, AZ_WRONG_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                {
                    return false;
                }

    #ifndef AZ_FILENAME_ALLOW_SPACES
                if (Find(in, AZ_SPACE_CHARACTERS) != AZStd::string::npos)
                {
                    return false;
                }
    #endif // AZ_FILENAME_ALLOW_SPACES

                if (Path::HasDrive(in))
                {
                    return false;
                }

                if (FirstCharacter(in) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    return false;
                }

                if (LastCharacter(in) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    return false;
                }

                return true;
            }
        }//namespace RelativePath

        namespace Path
        {
            bool Normalize(AZStd::string& inout)
            {
                Strip(inout, AZ_FILESYSTEM_INVALID_CHARACTERS);

    #ifndef AZ_FILENAME_ALLOW_SPACES
                Strip(inout, AZ_SPACE_CHARACTERS);
    #endif // AZ_FILENAME_ALLOW_SPACES

                //too big to be a path fail
                if (inout.length() > AZ_MAX_PATH_LEN)
                {
                    return false;
                }

                AZStd::replace(inout.begin(), inout.end(), AZ_WRONG_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR);

                size_t pos = AZStd::string::npos;
                if ((pos = inout.find(AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR)) == 0)
                {
                    AZStd::string temp = inout;
                    if (!Path::GetDrive(temp.c_str(), inout))
                    {
                        return false;
                    }
                    if (!Path::StripDrive(temp))
                    {
                        return false;
                    }

                    Replace(temp, AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING);
                    inout += temp;
                }
                else if (pos != AZStd::string::npos)
                {
                    Replace(inout, AZ_DOUBLE_CORRECT_FILESYSTEM_SEPARATOR, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING);
                }

                return IsValid(inout.c_str());
            }

            bool IsValid(const char* in, bool bHasDrive /*= false*/, bool bHasExtension /*= false*/, AZStd::string* errors /*= nullptr*/)
            {
                //if they gave us a error reporting string empty it.
                if (errors)
                {
                    errors->clear();
                }

                //empty is not a valid path
                if (!in)
                {
                    if (errors)
                    {
                        *errors += "The path is Empty.";
                    }
                    return false;
                }

                //empty is not a valid path
                size_t length = strlen(in);
                if (!length)
                {
                    if (errors)
                    {
                        *errors += "The path is Empty.";
                    }
                    return false;
                }

                //invalid characters
                const char* inEnd = in + length;
                const char* invalidCharactersBegin = AZ_FILESYSTEM_INVALID_CHARACTERS;
                const char* invalidCharactersEnd = invalidCharactersBegin + AZ_ARRAY_SIZE(AZ_FILESYSTEM_INVALID_CHARACTERS);
                if (AZStd::find_first_of(in, inEnd, invalidCharactersBegin, invalidCharactersEnd) != inEnd)
                {
                    if (errors)
                    {
                        *errors += "The path has invalid characters.";
                    }
                    return false;
                }

                //invalid characters
                if (Find(in, AZ_WRONG_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                {
                    if (errors)
                    {
                        *errors += "The path has wrong separator.";
                    }
                    return false;
                }

    #ifndef AZ_FILENAME_ALLOW_SPACES
                const char* spaceCharactersBegin = AZ_SPACE_CHARACTERS;
                const char* spaceCharactersEnd = spaceCharactersBegin + AZ_ARRAY_SIZE(AZ_SPACE_CHARACTERS);
                if (AZStd::find_first_of(in, inEnd, spaceCharactersBegin, spaceCharactersEnd) != inEnd)
                {
                    if (errors)
                    {
                        *errors += "The path has space characters.";
                    }
                    return false;
                }
    #endif // AZ_FILENAME_ALLOW_SPACES

                //does it have a drive if specified
                if (bHasDrive && !HasDrive(in))
                {
                    if (errors)
                    {
                        *errors += "The path should have a drive. The path [";
                        *errors += in;
                        *errors += "] is invalid.";
                    }
                    return false;
                }

                //does it have and extension if specified
                if (bHasExtension && !HasExtension(in))
                {
                    if (errors)
                    {
                        *errors += "The path should have the a file extension. The path [";
                        *errors += in;
                        *errors += "] is invalid.";
                    }
                    return false;
                }

                //start at the beginning and walk down the characters of the path
                const char* elementStart = in;
                const char* walk = elementStart;
                while (*walk)
                {
                    if (*walk == AZ_CORRECT_FILESYSTEM_SEPARATOR) //is this the correct separator
                    {
                        elementStart = walk;
                    }
    #if defined(AZ_PLATFORM_WINDOWS) || defined (AZ_PLATFORM_X360) || defined (AZ_PLATFORM_XBONE)

                    else if (*walk == AZ_FILESYSTEM_DRIVE_SEPARATOR) //is this the drive separator
                    {
                        //A AZ_FILESYSTEM_DRIVE_SEPARATOR character con only occur in the first
                        //component of a valid path. If the elementStart is not GetBufferPtr()
                        //then we have past the first component
                        if (elementStart != in)
                        {
                            if (errors)
                            {
                                *errors += "There is a stray AZ_FILESYSTEM_DRIVE_SEPARATOR = ";
                                *errors += AZ_FILESYSTEM_DRIVE_SEPARATOR;
                                *errors += " found after the first component. The path [";
                                *errors += in;
                                *errors += "] is invalid.";
                            }
                            return false;
                        }
                    }
    #endif
    #ifndef AZ_FILENAME_ALLOW_SPACES
                    else if (*walk == ' ') //is this a space
                    {
                        if (errors)
                        {
                            *errors += "The component [";
                            for (const char* c = elementStart + 1; c != walk; ++c)
                            {
                                *errors += *c;
                            }
                            *errors += "] has a SPACE character. The path [";
                            *errors += in;
                            *errors += "] is invalid.";
                        }
                        return false;
                    }
    #endif
                    //is this component (ie. directory or full file name) larger than the allowed MAX_FILE_LEN characters?
                    if (walk - elementStart - 1 > MAX_PATH_COMPONENT_LEN)
                    {
                        if (errors)
                        {
                            *errors += "The component [";
                            for (const char* c = elementStart + 1; c != walk; ++c)
                            {
                                *errors += *c;
                            }
                            *errors += "] has hit the MAX_PATH_COMPONENT_LEN = ";

    #if !defined(AZ_PLATFORM_PS4) && !defined(AZ_PLATFORM_APPLE) && !defined(AZ_PLATFORM_ANDROID) && !defined(AZ_PLATFORM_LINUX)
                            char buf[64];
                            _itoa_s(MAX_PATH_COMPONENT_LEN, buf, 10);
                            *errors += buf;
    #endif
                            *errors += " character limit. The path [";
                            *errors += in;
                            *errors += "] is invalid.";
                        }
                        return false;
                    }

                    ++walk;
                }

                //is this full path longer than AZ_MAX_PATH_LEN (The longest a path with all components can possibly be)?
                if (walk - in > AZ_MAX_PATH_LEN)
                {
                    if (errors != 0)
                    {
                        *errors += "The path [";
                        *errors += in;
                        *errors += "] is over the AZ_MAX_PATH_LEN = ";

    #if !defined(AZ_PLATFORM_PS4) && !defined(AZ_PLATFORM_APPLE) && !defined(AZ_PLATFORM_ANDROID) && !defined(AZ_PLATFORM_LINUX)
                        char buf[64];
                        _itoa_s(AZ_MAX_PATH_LEN, buf, 10);
                        *errors += buf;
    #endif
                        *errors += " characters total length limit.";
                    }
                    return false;
                }

                return true;
            }

            bool ConstructFull(const char* pRootPath, const char* pFileName, AZStd::string& out, bool bNormalize /* = false*/)
            {
                if (!pRootPath)
                {
                    return false;
                }

                if (!pFileName)
                {
                    return false;
                }

                if (!strlen(pRootPath))
                {
                    return false;
                }

                if (!strlen(pFileName))
                {
                    return false;
                }

                if (!HasDrive(pRootPath))
                {
                    return false;
                }

                if (HasDrive(pFileName))
                {
                    return false;
                }

                AZStd::string tempRoot;
                AZStd::string tempFileName;
                if (pRootPath == out.c_str() || pFileName == out.c_str())
                {
                    tempRoot = pRootPath;
                    tempFileName = pFileName;
                    pRootPath = tempRoot.c_str();
                    pFileName = tempFileName.c_str();
                }

                if (bNormalize)
                {
                    AZStd::string rootPath = pRootPath;
                    Root::Normalize(rootPath);

                    AZStd::string fileName = pFileName;
                    Path::Normalize(fileName);

                    Strip(fileName, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);

                    if (!IsRelative(fileName.c_str()))
                    {
                        return false;
                    }

                    out = rootPath;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    out += fileName;
                }
                else
                {
                    if (!IsRelative(pFileName))
                    {
                        return false;
                    }

                    out = pRootPath;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    Append(out, pFileName);
                }

                if (bNormalize)
                {
                    return Normalize(out);
                }
                else
                {
                    return IsValid(out.c_str());
                }
            }

            bool ConstructFull(const char* pRootPath, const char* pFileName, const char* pFileExtension, AZStd::string& out, bool bNormalize /* = false*/)
            {
                if (!pRootPath)
                {
                    return false;
                }

                if (!pFileName)
                {
                    return false;
                }

                if (!strlen(pRootPath))
                {
                    return false;
                }

                if (!strlen(pFileName))
                {
                    return false;
                }

                if (!HasDrive(pRootPath))
                {
                    return false;
                }

                if (HasDrive(pFileName))
                {
                    return false;
                }

                AZStd::string tempRoot;
                AZStd::string tempFileName;
                AZStd::string tempExtension;
                if (pRootPath == out.c_str() || pFileName == out.c_str() || pFileExtension == out.c_str())
                {
                    tempRoot = pRootPath;
                    tempFileName = pFileName;
                    tempExtension = pFileExtension;
                    pRootPath = tempRoot.c_str();
                    pFileName = tempFileName.c_str();
                    pFileExtension = tempExtension.c_str();
                }

                if (pFileExtension && strlen(pFileExtension))
                {
                    if (Find(pFileExtension, AZ_CORRECT_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                    {
                        return false;
                    }

                    if (Find(pFileExtension, AZ_WRONG_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                    {
                        return false;
                    }
                }

                if (bNormalize)
                {
                    AZStd::string rootPath = pRootPath;
                    Root::Normalize(rootPath);

                    AZStd::string fileName = pFileName;
                    Path::Normalize(fileName);
                    Strip(fileName, AZ_CORRECT_AND_WRONG_FILESYSTEM_SEPARATOR, false, true, true);

                    if (!IsRelative(fileName.c_str()))
                    {
                        return false;
                    }

                    out = rootPath;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    out += fileName;
                }
                else
                {
                    if (!IsRelative(pFileName))
                    {
                        return false;
                    }

                    out = pRootPath;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    Append(out, pFileName);
                }

                if (pFileExtension)
                {
                    ReplaceExtension(out, pFileExtension);
                }

                if (bNormalize)
                {
                    return Normalize(out);
                }
                else
                {
                    return IsValid(out.c_str());
                }
            }

            bool ConstructFull(const char* pRoot, const char* pRelativePath, const char* pFileName, const char* pFileExtension, AZStd::string& out, bool bNormalize /* = false*/)
            {
                if (!pRoot)
                {
                    return false;
                }

                if (!pRelativePath)
                {
                    return false;
                }

                if (!pFileName)
                {
                    return false;
                }

                if (!strlen(pRoot))
                {
                    return false;
                }

                if (!strlen(pFileName))
                {
                    return false;
                }

                if (!HasDrive(pRoot))
                {
                    return false;
                }

                if (HasDrive(pRelativePath))
                {
                    return false;
                }

                if (HasDrive(pFileName))
                {
                    return false;
                }

                AZStd::string tempRoot;
                AZStd::string tempRelativePath;
                AZStd::string tempFileName;
                AZStd::string tempExtension;
                if (pRoot == out.c_str() || pFileName == out.c_str() || pFileExtension == out.c_str())
                {
                    tempRoot = pRoot;
                    tempRelativePath = pRelativePath;
                    tempFileName = pFileName;
                    tempExtension = pFileExtension;
                    pRoot = tempRoot.c_str();
                    pFileName = tempFileName.c_str();
                    pFileExtension = tempExtension.c_str();
                    pRelativePath = tempRelativePath.c_str();
                }

                if (pFileExtension && strlen(pFileExtension))
                {
                    if (Find(pFileExtension, AZ_CORRECT_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                    {
                        return false;
                    }

                    if (Find(pFileExtension, AZ_WRONG_FILESYSTEM_SEPARATOR) != AZStd::string::npos)
                    {
                        return false;
                    }
                }

                if (bNormalize)
                {
                    AZStd::string root = pRoot;
                    Root::Normalize(root);

                    AZStd::string relativePath = pRelativePath;
                    RelativePath::Normalize(relativePath);

                    if (!IsRelative(relativePath.c_str()))
                    {
                        return false;
                    }

                    AZStd::string fileName = pFileName;
                    Path::Normalize(fileName);

                    Strip(fileName, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);

                    if (!IsRelative(fileName.c_str()))
                    {
                        return false;
                    }

                    out = root;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    out += relativePath;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    out += fileName;
                }
                else
                {
                    if (!IsRelative(pRelativePath))
                    {
                        return false;
                    }

                    if (!IsRelative(pFileName))
                    {
                        return false;
                    }

                    out = pRoot;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    out += pRelativePath;
                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                    {
                        Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    }
                    out += pFileName;
                }

                if (pFileExtension)
                {
                    ReplaceExtension(out, pFileExtension);
                }

                if (bNormalize)
                {
                    return Normalize(out);
                }
                else
                {
                    return IsValid(out.c_str());
                }
            }

            bool Split(const char* in, AZStd::string* pDstDrive /*= nullptr*/, AZStd::string* pDstPath /*= nullptr*/, AZStd::string* pDstName /*= nullptr*/, AZStd::string* pDstExtension /*= nullptr*/)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string temp(in);
                if (HasDrive(temp.c_str()))
                {
                    StripDrive(temp);
                    if (pDstDrive)
                    {
                        GetDrive(in, *pDstDrive);
                    }
                }
                else
                {
                    if (pDstDrive)
                    {
                        pDstDrive->clear();
                    }
                }

                char b2[256], b3[256], b4[256];
                _splitpath_s(temp.c_str(), nullptr, 0, b2, 256, b3, 256, b4, 256);
                if (pDstPath)
                {
                    *pDstPath = b2;
                }
                if (pDstName)
                {
                    *pDstName = b3;
                }
                if (pDstExtension)
                {
                    *pDstExtension = b4;
                }

                return true;
            }

            bool Join(const char* pFirstPart, const char* pSecondPart, AZStd::string& out, bool bJoinOverlapping /*= false*/, bool bCaseInsenitive /*= true*/, bool bNormalize /*= true*/)
            {
                //null && null
                if (!pFirstPart && !pSecondPart)
                {
                    return false;
                }

                size_t firstPartLen = 0;
                size_t secondPartLen = 0;
                if (pFirstPart)
                {
                    firstPartLen = strlen(pFirstPart);
                }
                if (pSecondPart)
                {
                    secondPartLen = strlen(pSecondPart);
                }

                //0 && 0
                if (!firstPartLen && !secondPartLen)
                {
                    return false;
                }

                //null good
                if (!pFirstPart && pSecondPart)
                {
                    //null 0
                    if (!secondPartLen)
                    {
                        return false;
                    }

                    out = pSecondPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                //good null
                if (!pSecondPart && pFirstPart)
                {
                    //0 null
                    if (!firstPartLen)
                    {
                        return false;
                    }

                    out = pFirstPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                //0 good
                if (!firstPartLen && pSecondPart)
                {
                    out = pSecondPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                //good 0
                if (pFirstPart && !secondPartLen)
                {
                    out = pFirstPart;
                    if (bNormalize)
                    {
                        return Normalize(out);
                    }

                    return true;
                }

                if (HasDrive(pSecondPart))
                {
                    return false;
                }

                AZStd::string tempFirst;
                AZStd::string tempSecond;
                if (pFirstPart == out.c_str() || pSecondPart == out.c_str())
                {
                    tempFirst = pFirstPart;
                    tempSecond = pSecondPart;
                    pFirstPart = tempFirst.c_str();
                    pSecondPart = tempSecond.c_str();
                }

                out = pFirstPart;

                if (bJoinOverlapping)
                {
                    AZStd::string firstPart(pFirstPart);
                    AZStd::string secondPart(pSecondPart);
                    if (bCaseInsenitive)
                    {
                        AZStd::to_lower(firstPart.begin(), firstPart.end());
                        AZStd::to_lower(secondPart.begin(), secondPart.end());
                    }

                    AZStd::vector<AZStd::string> firstPartDelimited;
                    Strip(firstPart, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Tokenize(firstPart.c_str(), firstPartDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);
                    AZStd::vector<AZStd::string> secondPartDelimited;
                    Strip(secondPart, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Tokenize(secondPart.c_str(), secondPartDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);
                    AZStd::vector<AZStd::string> secondPartNormalDelimited;

                    for (size_t i = 0; i < firstPartDelimited.size(); ++i)
                    {
                        if (firstPartDelimited[i].length() == secondPartDelimited[0].length())
                        {
                            if (!strncmp(firstPartDelimited[i].c_str(), secondPartDelimited[0].c_str(), firstPartDelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of firstPart Must Be equal or we fail
                                bool bFailed = false;
                                size_t jj = 1;
                                for (size_t ii = i + 1; !bFailed && ii < firstPartDelimited.size(); ++ii)
                                {
                                    if (firstPartDelimited[ii].length() != secondPartDelimited[jj].length())
                                    {
                                        bFailed = true;
                                    }
                                    else if (strncmp(firstPartDelimited[ii].c_str(), secondPartDelimited[jj].c_str(), firstPartDelimited[ii].length()))
                                    {
                                        bFailed = true;
                                    }

                                    jj++;
                                }

                                if (!bFailed)
                                {
                                    if (LastCharacter(out.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                                    {
                                        out += AZ_CORRECT_FILESYSTEM_SEPARATOR;
                                    }

                                    AZStd::string secondPartNormal(pSecondPart);
                                    Strip(secondPartNormal, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                                    Tokenize(secondPartNormal.c_str(), secondPartNormalDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);

                                    for (; jj < secondPartNormalDelimited.size(); ++jj)
                                    {
                                        out += secondPartNormalDelimited[jj];
                                        out += AZ_CORRECT_FILESYSTEM_SEPARATOR;
                                    }

                                    if (LastCharacter(pSecondPart) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                                    {
                                        RChop(out);
                                    }

                                    return true;
                                }
                            }
                        }
                    }
                }

                if (LastCharacter(pFirstPart) == AZ_CORRECT_FILESYSTEM_SEPARATOR && FirstCharacter(pSecondPart) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    Strip(out, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, false, true);
                }

                if (LastCharacter(pFirstPart) != AZ_CORRECT_FILESYSTEM_SEPARATOR && FirstCharacter(pSecondPart) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    Append(out, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                }

                out += pSecondPart;

                if (bNormalize)
                {
                    return Path::Normalize(out);
                }

                return true;
            }

            bool HasDrive(const char* in)
            {
                //no drive if empty
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

    #if defined (AZ_PLATFORM_X360) || defined (AZ_PLATFORM_WINDOWS) || defined (AZ_PLATFORM_XBONE)

                //find the first AZ_FILESYSTEM_DRIVE_SEPARATOR
                if (const char* pFirstDriveSep = strchr(in, AZ_FILESYSTEM_DRIVE_SEPARATOR))
                {
                    //fail if the drive separator is not the second character
                    if (pFirstDriveSep != in + 1)
                    {
                        return false;
                    }

                    //fail if the first character, the drive letter, is not a letter
                    if (!isalpha(in[0]))
                    {
                        return false;
                    }

                    //find the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                    if (const char* pFirstSep = strchr(in, AZ_CORRECT_FILESYSTEM_SEPARATOR))
                    {
                        //fail if the first AZ_FILESYSTEM_DRIVE_SEPARATOR occurs after the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                        if (pFirstDriveSep > pFirstSep)
                        {
                            return false;
                        }
                    }
                    return true;
                }
                else if (!strncmp(in, AZ_NETWORK_PATH_START, AZ_NETWORK_PATH_START_SIZE))//see if it has a network start
                {
                    //find the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                    if (const char* pFirstSep = strchr(in + AZ_NETWORK_PATH_START_SIZE, AZ_CORRECT_FILESYSTEM_SEPARATOR))
                    {
                        //fail if the first AZ_CORRECT_FILESYSTEM_SEPARATOR is first character
                        if (pFirstSep == in + AZ_NETWORK_PATH_START_SIZE)
                        {
                            return false;
                        }

                        //fail if the first character after the NETWORK_START isn't alphanumeric
                        if (!isalnum(*(in + AZ_NETWORK_PATH_START_SIZE + 1)))
                        {
                            return false;
                        }
                    }
                    return true;
                }

    #else
                // on other platforms, its got a root if it starts with '/'
                if (in[0] == '/')
                {
                    return true;
                }
    #endif
                return false;
            }

            bool HasPath(const char* in)
            {
                //no path to strip
                if (!in)
                {
                    return false;
                }

                size_t len = strlen(in);
                if (!len)
                {
                    return false;
                }

                //find the last AZ_CORRECT_FILESYSTEM_SEPARATOR
                if (const char* pLastSep = strrchr(in, AZ_CORRECT_FILESYSTEM_SEPARATOR))
                {
                    if (*(pLastSep + 1) != '\0')
                    {
                        return true;
                    }
                }

                return false;
            }


            bool HasExtension(const char* in)
            {
                //it doesn't have an extension if it's empty
                if (!in)
                {
                    return false;
                }

                size_t len = strlen(in);
                if (!len)
                {
                    return false;
                }

                char b1[256];
                _splitpath_s(in, nullptr, 0, nullptr, 0, nullptr, 0, b1, 256);

                size_t lenExt = strlen(b1);
                if (lenExt == 0 || lenExt > AZ_MAX_EXTENTION_LEN)
                {
                    return false;
                }

                return true;
            }

            bool IsExtension(const char* in, const char* pExtension, bool bCaseInsenitive /*= false*/)
            {
                //it doesn't have an extension if it's empty
                if (!in)
                {
                    return false;
                }

                char b1[256] = "";
                _splitpath_s(in, nullptr, 0, nullptr, 0, nullptr, 0, b1, 256);
                char* pExt = b1;

                if (FirstCharacter(pExt) == AZ_FILESYSTEM_EXTENSION_SEPARATOR)
                {
                    pExt++;
                }

                size_t lenExt = strlen(pExt);
                if (lenExt > AZ_MAX_EXTENTION_LEN)
                {
                    return false;
                }

                if (pExtension)
                {
                    if (FirstCharacter(pExtension) == AZ_FILESYSTEM_EXTENSION_SEPARATOR)
                    {
                        pExtension++;
                    }

                    size_t lenExtCmp = strlen(pExtension);
                    if (lenExtCmp > AZ_MAX_EXTENTION_LEN || lenExt != lenExtCmp)
                    {
                        return false;
                    }
                }
                else
                {
                    return lenExt == 0;
                }

                if (bCaseInsenitive)
                {
                    return !azstrnicmp(pExt, pExtension, lenExt);
                }
                else
                {
                    return !strncmp(pExt, pExtension, lenExt);
                }
            }

            bool IsRelative(const char* in)
            {
                //not relative if empty
                if (!in)
                {
                    return false;
                }

                size_t len = strlen(in);
                if (!len)
                {
                    return true;
                }

                //not relative if it has a drive
                if (HasDrive(in))
                {
                    return false;
                }

                //not relative if it starts with a AZ_CORRECT_FILESYSTEM_SEPARATOR
                if (FirstCharacter(in) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    return false;
                }

                return true;
            }

            bool IsASuperFolderOfB(const char* pathA, const char* pathB, bool bCaseInsenitive /*= true*/, bool bIgnoreStartingPath /*= true*/)
            {
                if (!pathA || !pathB)
                {
                    return false;
                }

                if (pathA == pathB)
                {
                    return false;
                }

                AZStd::string strPathA(pathA);
                if (!strPathA.length())
                {
                    return false;
                }

                AZStd::string strPathB(pathB);
                if (!strPathB.length())
                {
                    return false;
                }

                if (bIgnoreStartingPath)
                {
                    StripDrive(strPathA);
                    StripDrive(strPathB);

                    Strip(strPathA, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                }
                else
                {
                    Strip(strPathA, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);

                    size_t lenA = strPathA.length();
                    size_t lenB = strPathB.length();
                    if (lenA >= lenB)
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strPathA.c_str(), strPathB.c_str(), lenA))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strPathA.c_str(), strPathB.c_str(), lenA))
                        {
                            return false;
                        }
                    }
                }

                AZStd::vector<AZStd::string> pathADelimited;
                Tokenize(strPathA.c_str(), pathADelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);
                AZStd::vector<AZStd::string> pathBDelimited;
                Tokenize(strPathB.c_str(), pathBDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);

                //EX: A= p4\\Main\\Source\\GameAssets\\gameinfo
                //    B= p4\\Main\\Source\\GameAssets\\gameinfo\\Characters
                if (bIgnoreStartingPath)
                {
                    for (size_t i = 0; i < pathADelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[0].length())
                        {
                        }
                        else if (bCaseInsenitive)
                        {
                            if (!azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of A Must Be equal or we fail

                                //if the number of components after this match in A are more than
                                //the remaining in B then fail as it can not be a super folder
                                if (pathADelimited.size() - i >= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t jj = 1;
                                for (size_t ii = i + 1; ii < pathADelimited.size(); ++ii)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    else if (azstrnicmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    jj++;
                                }
                                return true;
                            }
                        }
                        else
                        {
                            if (!strncmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of A Must Be equal or we fail

                                //if the number of components after this match in A are more than
                                //the remaining in B then fail as it can not be a super folder
                                if (pathADelimited.size() - i >= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t jj = 1;
                                for (size_t ii = i + 1; ii < pathADelimited.size(); ++ii)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    if (strncmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    jj++;
                                }
                                return true;
                            }
                        }
                    }
                    return false;
                }
                else
                {
                    //if the number of components after this match in A are more than
                    //the remaining in B then fail as it can not be a super folder
                    if (pathADelimited.size() >= pathBDelimited.size())
                    {
                        return false;
                    }

                    for (size_t i = 0; i < pathADelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[i].length())
                        {
                            return false;
                        }
                        else if (bCaseInsenitive)
                        {
                            if (azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            if (strncmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                    }
                    return true;
                }
            }

            bool IsASubFolderOfB(const char* pathA, const char* pathB, bool bCaseInsenitive /*= true*/, bool bIgnoreStartingPath /*= true*/)
            {
                if (!pathA || !pathB)
                {
                    return false;
                }

                if (pathA == pathB)
                {
                    return false;
                }

                AZStd::string strPathA(pathA);
                if (!strPathA.length())
                {
                    return false;
                }

                AZStd::string strPathB(pathB);
                if (!strPathB.length())
                {
                    return false;
                }

                if (bIgnoreStartingPath)
                {
                    StripDrive(strPathA);
                    StripDrive(strPathB);

                    Strip(strPathA, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                }
                else
                {
                    Strip(strPathA, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Strip(strPathB, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);

                    size_t lenA = strPathA.length();
                    size_t lenB = strPathB.length();
                    if (lenA < lenB)
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strPathA.c_str(), strPathB.c_str(), lenB))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strPathA.c_str(), strPathB.c_str(), lenB))
                        {
                            return false;
                        }
                    }
                }

                AZStd::vector<AZStd::string> pathADelimited;
                Tokenize(strPathA.c_str(), pathADelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);
                AZStd::vector<AZStd::string> pathBDelimited;
                Tokenize(strPathB.c_str(), pathBDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);

                //EX: A= p4\\Main\\Source\\GameAssets\\gameinfo\\Characters
                //    B= p4\\Main\\Source\\GameAssets\\gameinfo
                if (bIgnoreStartingPath)
                {
                    for (size_t i = 0; i < pathADelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[0].length())
                        {
                        }
                        else if (bCaseInsenitive)
                        {
                            if (!azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of B Must Be equal or we fail

                                //if the number of components after this match in A has to be greater
                                //then B or it can not be a sub folder
                                if (pathADelimited.size() - i <= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t ii = i + 1;
                                for (size_t jj = 1; jj < pathBDelimited.size(); ++jj)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    else if (azstrnicmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    ii++;
                                }
                                return true;
                            }
                        }
                        else
                        {
                            if (!strncmp(pathADelimited[i].c_str(), pathBDelimited[0].c_str(), pathADelimited[i].length()))
                            {
                                //we found the first component that is equal
                                //now every component for the rest of A Must Be equal or we fail

                                //if the number of components after this match in A has to be greater
                                //then B or it can not be a sub folder
                                if (pathADelimited.size() - i <= pathBDelimited.size())
                                {
                                    return false;
                                }

                                size_t ii = i + 1;
                                for (size_t jj = 1; jj < pathBDelimited.size(); ++jj)
                                {
                                    if (pathADelimited[ii].length() != pathBDelimited[jj].length())
                                    {
                                        return false;
                                    }
                                    else if (azstrnicmp(pathADelimited[ii].c_str(), pathBDelimited[jj].c_str(), pathADelimited[ii].length()))
                                    {
                                        return false;
                                    }
                                    ii++;
                                }
                                return true;
                            }
                        }
                    }
                    return false;
                }
                else
                {
                    if (pathADelimited.size() <= pathBDelimited.size())
                    {
                        return false;
                    }

                    for (size_t i = 0; i < pathBDelimited.size(); ++i)
                    {
                        if (pathADelimited[i].length() != pathBDelimited[i].length())
                        {
                            return false;
                        }
                        else if (bCaseInsenitive)
                        {
                            if (azstrnicmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            if (strncmp(pathADelimited[i].c_str(), pathBDelimited[i].c_str(), pathADelimited[i].length()))
                            {
                                return false;
                            }
                        }
                    }
                    return true;
                }
            }

            bool IsFileInFolder(const char* pFilePath, const char* pFolder, bool bIncludeSubTree /*= false*/, bool bCaseInsenitive /*= true*/, bool bIgnoreStartingPath /*= true*/)
            {
                (void)bIncludeSubTree;

                if (!pFilePath || !pFolder)
                {
                    return false;
                }

                AZStd::string strFilePath(pFilePath);
                if (!strFilePath.length())
                {
                    return false;
                }

                AZStd::string strFolder(pFolder);
                if (!strFolder.length())
                {
                    return false;
                }

                StripFullName(strFilePath);

                if (bIgnoreStartingPath)
                {
                    StripDrive(strFilePath);
                    StripDrive(strFolder);

                    Strip(strFilePath, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Strip(strFolder, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                }
                else
                {
                    Strip(strFilePath, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);
                    Strip(strFolder, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);

                    size_t lenFilePath = strFilePath.length();
                    size_t lenFolder = strFolder.length();
                    if (lenFilePath < lenFolder)
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strFilePath.c_str(), strFolder.c_str(), lenFolder))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strFilePath.c_str(), strFolder.c_str(), lenFolder))
                        {
                            return false;
                        }
                    }
                }

                AZStd::vector<AZStd::string> strFilePathDelimited;
                Tokenize(strFilePath.c_str(), strFilePathDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);
                AZStd::vector<AZStd::string> strFolderDelimited;
                Tokenize(strFolder.c_str(), strFolderDelimited, AZ_CORRECT_FILESYSTEM_SEPARATOR, true);

                //EX: strFilePath= "p4\\Main\\Source\\GameAssets\\gameinfo\\character"
                //    strFolder= "Main\\Source\\GameAssets\\gameinfo"
                //    = true
                if (bIgnoreStartingPath)
                {
                    bool bFound = false;
                    size_t i = 0;
                    for (i = 0; !bFound && i < strFilePathDelimited.size(); ++i)
                    {
                        if (strFilePathDelimited[i].length() != strFolderDelimited[0].length())
                        {
                        }
                        else if (bCaseInsenitive)
                        {
                            if (!azstrnicmp(strFilePathDelimited[i].c_str(), strFolderDelimited[0].c_str(), strFilePathDelimited[i].length()))
                            {
                                //we found the first component that is equal
                                bFound = true;
                            }
                        }
                        else
                        {
                            if (!strncmp(strFilePathDelimited[i].c_str(), strFolderDelimited[0].c_str(), strFilePathDelimited[i].length()))
                            {
                                //we found the first component that is equal
                                bFound = true;
                            }
                        }
                    }

                    if (bFound)
                    {
                        //we found a match, modify the file path delimited
                        if (i)
                        {
                            strFilePathDelimited.erase(strFilePathDelimited.begin(), strFilePathDelimited.begin() + (i - 1));
                        }
                    }
                    else
                    {
                        for (i = 0; !bFound && i < strFolderDelimited.size(); ++i)
                        {
                            if (strFolderDelimited[i].length() != strFilePathDelimited[0].length())
                            {
                            }
                            else if (bCaseInsenitive)
                            {
                                if (!azstrnicmp(strFolderDelimited[i].c_str(), strFilePathDelimited[0].c_str(), strFolderDelimited[i].length()))
                                {
                                    //we found the first component that is equal
                                    bFound = true;
                                }
                            }
                            else
                            {
                                if (!strncmp(strFolderDelimited[i].c_str(), strFilePathDelimited[0].c_str(), strFolderDelimited[i].length()))
                                {
                                    //we found the first component that is equal
                                    bFound = true;
                                }
                            }
                        }

                        if (bFound)
                        {
                            //we found a match, modify the folder delimited
                            if (i)
                            {
                                strFolderDelimited.erase(strFolderDelimited.begin(), strFolderDelimited.begin() + (i - 1));
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                }

                //EX: strFilePath= "Main\\Source\\GameAssets\\gameinfo\\character"
                //    strFolder= "Main\\Source\\GameAssets\\gameinfo"
                //    = true

                if (!bIncludeSubTree && strFilePathDelimited.size() != strFolderDelimited.size())
                {
                    return false;
                }

                if (strFilePathDelimited.size() < strFolderDelimited.size())
                {
                    return false;
                }

                for (size_t i = 0; i < strFolderDelimited.size(); ++i)
                {
                    if (strFilePathDelimited[i].length() != strFolderDelimited[i].length())
                    {
                        return false;
                    }
                    else if (bCaseInsenitive)
                    {
                        if (azstrnicmp(strFilePathDelimited[i].c_str(), strFolderDelimited[i].c_str(), strFilePathDelimited[i].length()))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (strncmp(strFilePathDelimited[i].c_str(), strFolderDelimited[i].c_str(), strFilePathDelimited[i].length()))
                        {
                            return false;
                        }
                    }
                }
                return true;
            }

            bool StripDrive(AZStd::string& inout)
            {
                //no drive to strip
                if (!HasDrive(inout.c_str()))
                {
                    return false;
                }

    #if defined (AZ_PLATFORM_X360) || defined (AZ_PLATFORM_WINDOWS) || defined (AZ_PLATFORM_XBONE)

                //find the first AZ_FILESYSTEM_DRIVE_SEPARATOR
                size_t pos = inout.find_first_of(AZ_FILESYSTEM_DRIVE_SEPARATOR);
                if (pos != AZStd::string::npos)
                {
                    //make a new string that starts after the first AZ_FILESYSTEM_DRIVE_SEPARATOR
                    RKeep(inout, pos);
                }
                else
                {
                    pos = inout.find_first_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, AZ_NETWORK_PATH_START_SIZE);
                    if (pos != AZStd::string::npos)
                    {
                        //make a new string that starts at the first AZ_CORRECT_FILESYSTEM_SEPARATOR after the NETWORK_START
                        //note we don't have to check the pointer as it has already past the HasDrive() check
                        RKeep(inout, pos, true);
                    }
                    else
                    {
                        AZ_Assert(false, "Passed HasDrive() test but found no drive...");
                    }
                }
    #endif
                return true;
            }

            void StripPath(AZStd::string& inout)
            {
                char b1[256], b2[256];
                _splitpath_s(inout.c_str(), nullptr, 0, nullptr, 0, b1, 256, b2, 256);
                inout = AZStd::string::format("%s%s", b1, b2);
            }

            void StripFullName(AZStd::string& inout)
            {
                char b1[256], b2[256];
                _splitpath_s(inout.c_str(), b1, 256, b2, 256, nullptr, 0, nullptr, 0);
                inout = AZStd::string::format("%s%s", b1, b2);
            }

            void StripExtension(AZStd::string& inout)
            {
                char b1[256], b2[256], b3[256];
                _splitpath_s(inout.c_str(), b1, 256, b2, 256, b3, 256, nullptr, 0);
                inout = AZStd::string::format("%s%s%s", b1, b2, b3);
            }

            bool StripComponent(AZStd::string& inout, bool bLastComponent /* = false*/)
            {
                if (!bLastComponent)
                {
                    //Note: Directories can have any legal filename character including
                    //AZ_FILESYSTEM_EXTENSION_SEPARATOR 's in their names
                    //we define the first component of a path as anything
                    //before and including the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                    //i.e. "c:\\root\\parent\\child\\name<.ext>" => "c:\\"

                    //strip starting separators
                    size_t pos = inout.find_first_not_of(AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    pos = inout.find_first_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, pos);
                    if (pos != AZStd::string::npos)
                    {
                        //the next component starts at the next AZ_CORRECT_FILESYSTEM_SEPARATOR
                        RKeep(inout, pos);

                        //take care of the case when only a AZ_CORRECT_FILESYSTEM_SEPARATOR remains, it should just clear
                        if (inout.length() == 1 && FirstCharacter(inout.c_str()) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                        {
                            inout.clear();
                        }

                        return true;
                    }

                    if (inout.length())
                    {
                        inout.clear();
                        return true;
                    }

                    return false;
                }
                else
                {
                    //Note: Directories can have any legal filename character including
                    //AZ_FILESYSTEM_EXTENSION_SEPARATOR 's in their names
                    //we define the last component of a path as the Name
                    //so anything after and including the last AZ_CORRECT_FILESYSTEM_SEPARATOR
                    //i.e. root\\parent\\child\\name<.ext> => name<.ext>

                    //strip ending separators
                    Strip(inout, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, false, true);

                    //the next component starts after the next last AZ_CORRECT_FILESYSTEM_SEPARATOR
                    size_t pos = inout.find_last_of(AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    if (pos != AZStd::string::npos)
                    {
                        LKeep(inout, pos);
                        //take care of the case when only a AZ_CORRECT_FILESYSTEM_SEPARATOR remains, it should just clear
                        if (inout.length() == 1 && FirstCharacter(inout.c_str()) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                        {
                            inout.clear();
                        }

                        return true;
                    }

                    //it doesn't have a AZ_CORRECT_FILESYSTEM_SEPARATOR, empty the string
                    if (inout.length())
                    {
                        inout.clear();
                        return true;
                    }

                    return false;
                }
            }

            bool GetDrive(const char* in, AZStd::string& out)
            {
                out.clear();
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

    #if defined (AZ_PLATFORM_X360) || defined (AZ_PLATFORM_WINDOWS) || defined (AZ_PLATFORM_XBONE)

                //find the first AZ_FILESYSTEM_DRIVE_SEPARATOR
                if (const char* pFirstDriveSep = strchr(in, AZ_FILESYSTEM_DRIVE_SEPARATOR))
                {
                    //find the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                    if (const char* pFirstSep = strchr(in, AZ_CORRECT_FILESYSTEM_SEPARATOR))
                    {
                        //fail if the first AZ_FILESYSTEM_DRIVE_SEPARATOR occurs after the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                        if (pFirstDriveSep > pFirstSep)
                        {
                            return false;
                        }

                        //resize to the drive
                        out = in;
                        out.resize(pFirstSep - in);
                    }
                    return true;
                }
                else if (!strncmp(in, AZ_NETWORK_PATH_START, AZ_NETWORK_PATH_START_SIZE))//see if it has a network start
                {
                    //find the first AZ_CORRECT_FILESYSTEM_SEPARATOR
                    if (const char* pFirstSep = strchr(in + AZ_NETWORK_PATH_START_SIZE, AZ_CORRECT_FILESYSTEM_SEPARATOR))
                    {
                        //fail if the first AZ_CORRECT_FILESYSTEM_SEPARATOR is first character
                        if (pFirstSep == in + AZ_NETWORK_PATH_START_SIZE)
                        {
                            return false;
                        }

                        //fail if the first character after the NETWORK_START isn't alphanumeric
                        if (!isalnum(*(in + AZ_NETWORK_PATH_START_SIZE + 1)))
                        {
                            return false;
                        }

                        //resize to the drive
                        out = in;
                        out.resize(pFirstSep - in);
                    }
                    return true;
                }
    #else
                AZ_Assert(false, "Not implemented on this platform!");
    #endif
                return false;
            }

            bool GetFullPath(const char* in, AZStd::string& out)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

                char b1[256], b2[256];
                _splitpath_s(in, b1, 256, b2, 256, nullptr, 0, nullptr, 0);
                out = AZStd::string::format("%s%s", b1, b2);
                return !out.empty();
            }

            bool GetFolderPath(const char* in, AZStd::string& out)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

                char b1[256];
                _splitpath_s(in, nullptr, 0, b1, 256, nullptr, 0, nullptr, 0);
                out = b1;
                return !out.empty();
            }

            bool GetFolder(const char* in, AZStd::string& out, bool bFirst /* = false*/)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

                if (!bFirst)
                {
                    out = in;
                    StripFullName(out);
                    Strip(out, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, false, true);
                    RKeep(out, out.find_last_of(AZ_CORRECT_FILESYSTEM_SEPARATOR));
                    return !out.empty();
                }
                else
                {
                    //EX: "C:\\p4\\game\\info\\some.file"
                    out = in;
                    StripDrive(out);

                    //EX: "\\p4\\game\\info\\some.file"
                    //EX: "p4\\game\\info\\some.file"
                    size_t posFirst = out.find_first_not_of(AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    if (posFirst != AZStd::string::npos)
                    {
                        if (posFirst)
                        {
                            RKeep(out, posFirst, true);
                        }
                        //EX: "p4\\game\\info\\some.file"

                        size_t posSecond = out.find_first_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, posFirst);
                        if (posSecond != AZStd::string::npos)
                        {
                            LKeep(out, posSecond);
                        }

                        return !out.empty();
                    }

                    //no characters except perhaps AZ_CORRECT_FILESYSTEM_SEPARATOR ia a fail clear it and return
                    out.clear();
                    return false;
                }
            }

            bool GetFullFileName(const char* in, AZStd::string& out)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

                char b1[256], b2[256];
                _splitpath_s(in, nullptr, 0, nullptr, 0, b1, 256, b2, 256);
                out = AZStd::string::format("%s%s", b1, b2);
                return !out.empty();
            }

            bool GetFileName(const char* in, AZStd::string& out)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

                char b1[256];
                _splitpath_s(in, nullptr, 0, nullptr, 0, b1, 256, nullptr, 0);
                out = b1;
                return !out.empty();
            }

            bool GetExtension(const char* in, AZStd::string& out, bool includeDot)
            {
                if (!in)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                AZStd::string tempIn;
                if (in == out.c_str())
                {
                    tempIn = in;
                    in = tempIn.c_str();
                }

                char b1[256];
                _splitpath_s(in, nullptr, 0, nullptr, 0, nullptr, 0, b1, 256);
                if (includeDot)
                {
                    out = b1;
                }
                else if (b1[0] != '\0')
                {
                    out = b1 + 1; // skip dot
                }
                return !out.empty();
            }

            void ReplaceDrive(AZStd::string& inout, const char* newDrive)
            {
                StripDrive(inout);
                if (!newDrive)
                {
                    return;
                }

                if (!strlen(newDrive))
                {
                    return;
                }

                if (FirstCharacter(inout.c_str()) == AZ_CORRECT_FILESYSTEM_SEPARATOR && LastCharacter(newDrive) == AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    Strip(inout, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, false);
                }

                if (FirstCharacter(inout.c_str()) != AZ_CORRECT_FILESYSTEM_SEPARATOR && LastCharacter(newDrive) != AZ_CORRECT_FILESYSTEM_SEPARATOR)
                {
                    Prepend(inout, AZ_CORRECT_FILESYSTEM_SEPARATOR);
                }

                Prepend(inout, newDrive);
            }

            void ReplaceFullName(AZStd::string& inout, const char* pFileName /* = nullptr*/, const char* pFileExtension /* = nullptr*/)
            {
                //strip the full file name if it has one
                StripFullName(inout);
                if (pFileName)
                {
                    Append(inout, pFileName);
                }
                if (pFileExtension)
                {
                    ReplaceExtension(inout, pFileExtension);
                }
            }

            void ReplaceExtension(AZStd::string& inout, const char* newExtension /* = nullptr*/)
            {
                //strip the extension if it has one
                StripExtension(inout);

                //treat this as a strip
                if (!newExtension)
                {
                    return;
                }

                if (!strlen(newExtension))
                {
                    return;
                }

                //tolerate not having a AZ_FILESYSTEM_EXTENSION_SEPARATOR
                if (FirstCharacter(newExtension) != AZ_FILESYSTEM_EXTENSION_SEPARATOR)
                {
                    Append(inout, AZ_FILESYSTEM_EXTENSION_SEPARATOR);
                }

                //append the new extension
                Append(inout, newExtension);
            }

            size_t NumComponents(const char* in)
            {
                //0 components if its empty
                if (!in)
                {
                    return 0;
                }

                AZStd::string temp = in;
                if (temp.empty())
                {
                    return 0;
                }

                //strip AZ_CORRECT_FILESYSTEM_SEPARATOR(s) from the end
                Strip(temp, AZ_CORRECT_FILESYSTEM_SEPARATOR, false, true, true);

                //pass starting separators
                //if there is nothing but separators there are 0 components
                size_t pos = temp.find_first_not_of(AZ_CORRECT_FILESYSTEM_SEPARATOR);

                //inc every time we hit a AZ_CORRECT_FILESYSTEM_SEPARATOR
                size_t componentCount = 0;
                while (pos != AZStd::string::npos)
                {
                    componentCount++;
                    pos = temp.find_first_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, pos);
                    if (pos != AZStd::string::npos)
                    {
                        pos++;
                    }
                }

                return componentCount;
            }

            bool GetComponent(const char* in, AZStd::string& out, size_t nthComponent /*= 1*/, bool bReverse /*= false*/)
            {
                if (!in)
                {
                    return false;
                }

                if (!nthComponent)
                {
                    return false;
                }

                if (!strlen(in))
                {
                    return false;
                }

                out = in;

                if (!bReverse)
                {
                    //pass starting separators to the first character
                    //if it doesn't have anything but separators then fail
                    size_t startPos = out.find_first_not_of(AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    if (startPos == AZStd::string::npos)
                    {
                        return false;
                    }

                    //find the next separator after the first non separator
                    //if it doesn't have one then its a file name
                    //don't alter anything and return true
                    size_t endPos = out.find_first_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, startPos);
                    if (endPos == AZStd::string::npos)
                    {
                        return true;
                    }

                    //start and end represent the start and end of the first component
                    size_t componentCount = 1;
                    while (componentCount < nthComponent)
                    {
                        startPos = endPos + 1;

                        //inc every time we hit a AZ_CORRECT_FILESYSTEM_SEPARATOR
                        endPos = out.find_first_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, startPos);
                        if (endPos == AZStd::string::npos)
                        {
                            if (componentCount == nthComponent - 1)
                            {
                                RKeep(out, startPos, true);
                                return true;
                            }
                            else
                            {
                                return false; //nth component does not exists
                            }
                        }

                        componentCount++;
                    }

                    out = out.substr(startPos, endPos - startPos + 1);
                }
                else
                {
                    //pass starting separators
                    //if it doesn't have anything but separators then fail
                    size_t endPos = out.find_last_not_of(AZ_CORRECT_FILESYSTEM_SEPARATOR);
                    if (endPos == AZStd::string::npos)
                    {
                        return false;
                    }

                    //find the next separator before the last non separator
                    //if it doesn't have one then its a file name
                    //don't alter anything and return true
                    size_t startPos = out.find_last_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, endPos);
                    if (startPos == AZStd::string::npos)
                    {
                        return true;
                    }

                    //start and end represent the start and end of the last component
                    size_t componentCount = 1;
                    while (componentCount < nthComponent)
                    {
                        endPos = startPos - 1;

                        //inc every time we hit a AZ_CORRECT_FILESYSTEM_SEPARATOR
                        startPos = out.find_last_of(AZ_CORRECT_FILESYSTEM_SEPARATOR, endPos);
                        if (startPos == AZStd::string::npos)
                        {
                            if (componentCount == nthComponent - 1)
                            {
                                LKeep(out, endPos + 1, true);
                                return true;
                            }
                            else
                            {
                                return false; //nth component does not exists
                            }
                        }

                        componentCount++;
                    }

                    out = out.substr(startPos + 1, endPos - startPos + 1);
                }
                return true;
            }
        } // namespace Path

        namespace Json
        {
            /*
            According to http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf:
            A string is a sequence of Unicode code points wrapped with quotation marks (U+0022). All characters may be
            placed within the quotation marks except for the characters that must be escaped: quotation mark (U+0022),
            reverse solidus (U+005C), and the control characters U+0000 to U+001F.
            */
            AZStd::string& ToEscapedString(AZStd::string& inout)
            {
                size_t strSize = inout.size();

                for (size_t i = 0; i < strSize; ++i)
                {
                    char character = inout[i];

                    // defaults to 1 if it hits any cases except default
                    size_t jumpChar = 1;
                    switch (character)
                    {
                    case '"':
                        inout.insert(i, "\\");
                        break;

                    case '\\':
                        inout.insert(i, "\\");
                        break;

                    case '/':
                        inout.insert(i, "\\");
                        break;

                    case '\b':
                        inout.replace(i, i + 1, "\\b");
                        break;

                    case '\f':
                        inout.replace(i, i + 1, "\\f");
                        break;

                    case '\n':
                        inout.replace(i, i + 1, "\\n");
                        break;

                    case '\r':
                        inout.replace(i, i + 1, "\\r");
                        break;

                    case '\t':
                        inout.replace(i, i + 1, "\\t");
                        break;

                    default:
                        /*
                        Control characters U+0000 to U+001F may be represented as a six - character sequence : a reverse solidus,
                        followed by the lowercase letter u, followed by four hexadecimal digits that encode the code point.
                        */
                        if (character >= '\x0000' && character <= '\x001f')
                        {
                            // jumping "\uXXXX" characters
                            jumpChar = 6;

                            AZStd::string hexStr = AZStd::string::format("\\u%04x", static_cast<int>(character));
                            inout.replace(i, i + 1, hexStr);
                        }
                        else
                        {
                            jumpChar = 0;
                        }
                    }

                    i += jumpChar;
                    strSize += jumpChar;
                }

                return inout;
            }
        } // namespace Json
    } // namespace StringFunc
} // namespace AzFramework
