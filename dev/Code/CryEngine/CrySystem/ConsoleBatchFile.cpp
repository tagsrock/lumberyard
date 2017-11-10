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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : Executes an ASCII batch file of console commands...


#include "StdAfx.h"
#include "ConsoleBatchFile.h"
#include "IConsole.h"
#include "ISystem.h"
#include "XConsole.h"
#include <CryPath.h>
#include <stdio.h>
#include "System.h"

IConsole* CConsoleBatchFile::m_pConsole = NULL;

void CConsoleBatchFile::Init()
{
    m_pConsole = gEnv->pConsole;
    REGISTER_COMMAND("exec", (ConsoleCommandFunc)ExecuteFileCmdFunc, 0, "executes a batch file of console commands");
}

//////////////////////////////////////////////////////////////////////////
void CConsoleBatchFile::ExecuteFileCmdFunc(IConsoleCmdArgs* args)
{
    if (!m_pConsole)
    {
        Init();
    }

    if (!args->GetArg(1))
    {
        return;
    }

    ExecuteConfigFile(args->GetArg(1));
}

//////////////////////////////////////////////////////////////////////////
bool CConsoleBatchFile::ExecuteConfigFile(const char* sFilename)
{
    if (!sFilename)
    {
        return false;
    }

    if (!m_pConsole)
    {
        Init();
    }

    string filename;

    if (sFilename[0] != '@') // console config files are actually by default in @root@ instead of @assets@
    {
        filename = PathUtil::Make("@root@", PathUtil::GetFile(sFilename));
    }
    else
    {
        filename = sFilename;
    }

    if (strlen(PathUtil::GetExt(filename)) == 0)
    {
        filename = PathUtil::ReplaceExtension(filename, "cfg");
    }

#if defined(CVARS_WHITELIST)
    bool ignoreWhitelist = true;
    if (_stricmp(sFilename, "autoexec.cfg") == 0)
    {
        ignoreWhitelist = false;
    }
#endif // defined(CVARS_WHITELIST)

    //////////////////////////////////////////////////////////////////////////
    CCryFile file;

    {
        const char* szLog = "Executing console batch file (try game,config,root):";
        string filenameLog;
        string sfn = PathUtil::GetFile(filename);

        if (file.Open(filename, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK))
        {
            filenameLog = string("game/") + sfn;
        }
        else if (file.Open(string("config/") + sfn, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK))
        {
            filenameLog = string("game/config/") + sfn;
        }
        else if (file.Open(string("./") + sfn, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK))
        {
            filenameLog = string("./") + sfn;
        }
        else
        {
            CryLog("%s \"%s\" not found!", szLog, filename.c_str());
            return false;
        }

        CryLog("%s \"%s\" found in %s ...", szLog, PathUtil::GetFile(filenameLog.c_str()), PathUtil::GetPath(filenameLog).c_str());
    }

    int nLen = file.GetLength();
    char* sAllText = new char [nLen + 16];
    file.ReadRaw(sAllText, nLen);
    sAllText[nLen] = '\0';
    sAllText[nLen + 1] = '\0';

    /*
        This can't work properly as ShowConsole() can be called during the execution of the scripts,
        which means bConsoleStatus is outdated and must not be set at the end of the function

        bool bConsoleStatus = ((CXConsole*)m_pConsole)->GetStatus();
        ((CXConsole*)m_pConsole)->SetStatus(false);
    */

    char* strLast = sAllText + nLen;
    char* str = sAllText;
    while (str < strLast)
    {
        char* s = str;
        while (str < strLast && *str != '\n' && *str != '\r')
        {
            str++;
        }
        *str = '\0';
        str++;
        while (str < strLast && (*str == '\n' || *str == '\r'))
        {
            str++;
        }

        string strLine = s;


        //trim all whitespace characters at the beginning and the end of the current line and store its size
        strLine.Trim();
        size_t strLineSize = strLine.size();

        //skip comments, comments start with ";" or "--" but may have preceding whitespace characters
        if (strLineSize > 0)
        {
            if (strLine[0] == ';')
            {
                continue;
            }
            else if (strLine.find("--") == 0)
            {
                continue;
            }
        }
        //skip empty lines
        else
        {
            continue;
        }

#if defined(CVARS_WHITELIST)
        if (ignoreWhitelist || (gEnv->pSystem->GetCVarsWhiteList() && gEnv->pSystem->GetCVarsWhiteList()->IsWhiteListed(strLine, false)))
#endif // defined(CVARS_WHITELIST)
        {
            m_pConsole->ExecuteString(strLine);
        }
#if defined(DEDICATED_SERVER)
#if defined(CVARS_WHITELIST)
        else
        {
            gEnv->pSystem->GetILog()->LogError("Failed to execute command: '%s' as it is not whitelisted\n", strLine.c_str());
        }
#endif // defined(CVARS_WHITELIST)
#endif // defined(DEDICATED_SERVER)
    }
    // See above
    //  ((CXConsole*)m_pConsole)->SetStatus(bConsoleStatus);

    delete []sAllText;
    return true;
}
