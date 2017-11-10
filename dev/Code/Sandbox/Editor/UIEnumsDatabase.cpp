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

#include "StdAfx.h"
#include "UIEnumsDatabase.h"

//////////////////////////////////////////////////////////////////////////
QString CUIEnumsDatabase_SEnum::NameToValue(const QString& name)
{
    int n = (int)strings.size();
    for (int i = 0; i < n; i++)
    {
        if (name == strings[i])
        {
            return values[i];
        }
    }
    return name;
}

//////////////////////////////////////////////////////////////////////////
QString CUIEnumsDatabase_SEnum::ValueToName(const QString& value)
{
    int n = (int)strings.size();
    for (int i = 0; i < n; i++)
    {
        if (value == values[i])
        {
            return strings[i];
        }
    }
    return value;
}

//////////////////////////////////////////////////////////////////////////
CUIEnumsDatabase::CUIEnumsDatabase()
{
}

//////////////////////////////////////////////////////////////////////////
CUIEnumsDatabase::~CUIEnumsDatabase()
{
    // Free enums.
    for (Enums::iterator it = m_enums.begin(); it != m_enums.end(); ++it)
    {
        delete it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
void CUIEnumsDatabase::SetEnumStrings(const QString& enumName, const QStringList& sStringsArray)
{
    int nStringCount = sStringsArray.size();

    CUIEnumsDatabase_SEnum* pEnum = stl::find_in_map(m_enums, enumName, 0);
    if (!pEnum)
    {
        pEnum = new CUIEnumsDatabase_SEnum;
        pEnum->name = enumName;
        m_enums[enumName] = pEnum;
    }
    pEnum->strings.clear();
    pEnum->values.clear();
    for (int i = 0; i < nStringCount; i++)
    {
        QString str = sStringsArray[i];
        QString value = str;
        int pos = str.indexOf('=');
        if (pos >= 0)
        {
            value = str.mid(pos + 1);
            str = str.mid(0, pos);
        }
        pEnum->strings.push_back(str);
        pEnum->values.push_back(value);
    }
}

//////////////////////////////////////////////////////////////////////////
CUIEnumsDatabase_SEnum* CUIEnumsDatabase::FindEnum(const QString& enumName) const
{
    CUIEnumsDatabase_SEnum* pEnum = stl::find_in_map(m_enums, enumName, 0);
    return pEnum;
}
