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

#ifndef CRYINCLUDE_CRYSYSTEM_XML_SERIALIZEXMLWRITER_H
#define CRYINCLUDE_CRYSYSTEM_XML_SERIALIZEXMLWRITER_H
#pragma once


#include <ISystem.h>
#include <ITimer.h>
#include <IXml.h>
#include "IValidator.h"
#include "SimpleSerialize.h"

class CSerializeXMLWriterImpl
    : public CSimpleSerializeImpl<false, eST_SaveGame>
{
public:
    CSerializeXMLWriterImpl(const XmlNodeRef& nodeRef);
    ~CSerializeXMLWriterImpl();

    template <class T_Value>
    bool Value(const char* name, T_Value& value)
    {
        AddValue(name, value);
        return true;
    }

    template <class T_Value, class T_Policy>
    bool Value(const char* name, T_Value& value, const T_Policy& policy)
    {
        return Value(name, value);
    }

    bool Value(const char* name, CTimeValue value);
    bool Value(const char* name, ScriptAnyValue& value);
    bool Value(const char* name, XmlNodeRef& value);

    void BeginGroup(const char* szName);
    bool BeginOptionalGroup(const char* szName, bool condition);
    void EndGroup();

    void GetMemoryUsage(ICrySizer* pSizer) const;

private:
    //////////////////////////////////////////////////////////////////////////
    // Vars.
    //////////////////////////////////////////////////////////////////////////
    CTimeValue m_curTime;

    std::vector<XmlNodeRef> m_nodeStack;
    //bool m_bCheckEntityOnScript;
    std::vector<IScriptTable*> m_savedTables;
    std::vector<const char*> m_luaSaveStack;
    //////////////////////////////////////////////////////////////////////////


    ILINE const XmlNodeRef& CurNode()
    {
        assert(!m_nodeStack.empty());
        if (m_nodeStack.empty())
        {
            static XmlNodeRef temp = GetISystem()->CreateXmlNode("Error");
            return temp;
        }
        return m_nodeStack.back();
    }

    XmlNodeRef CreateNodeNamed(const char* name);

    template <class T>
    void AddValue(const char* name, const T& value)
    {
        if (strchr(name, ' ') != 0)
        {
            assert(0 && "Spaces in Value name not supported");
            CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!Spaces in Value name not supported: %s in Group %s", name, GetStackInfo());
            return;
        }
        if (GetISystem()->IsDevMode() && CurNode())
        {
            // Check if this attribute already added.
            if (CurNode()->haveAttr(name))
            {
                assert(0);
                CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!Duplicate tag Value( \"%s\" ) in Group %s", name, GetStackInfo());
            }
        }

        if (!IsDefaultValue(value))
        {
            CurNode()->setAttr(name, value);
        }
    }
    void AddValue(const char* name, const SSerializeString& value)
    {
        AddValue(name, value.c_str());
    }
    void AddValue(const char* name, const SNetObjectID& value)
    {
        assert(false);
    }
    template <class T>
    void AddTypedValue(const char* name, const T& value, const char* type)
    {
        if (!IsDefaultValue(value))
        {
            XmlNodeRef newNode = CreateNodeNamed(name);
            newNode->setAttr("v", value);
            newNode->setAttr("t", type);
        }
    }

    void WriteTable(XmlNodeRef addTo, SmartScriptTable tbl, bool bCheckEntityOnScript);
    void ScriptValue(XmlNodeRef addTo, const char* tag, const char* name, const ScriptAnyValue& value, bool bCheckEntityOnScript);
    // Used for printing currebnt stack info for warnings.
    const char* GetStackInfo() const;
    const char* GetLuaStackInfo() const;

    static bool ShouldSkipValue(const char* name, const ScriptAnyValue& value);
    static bool IsVector(SmartScriptTable tbl);
    bool IsEntity(SmartScriptTable tbl, EntityId& entityId);

    //////////////////////////////////////////////////////////////////////////
    // Check For Defaults.
    //////////////////////////////////////////////////////////////////////////
    bool IsDefaultValue(bool v) const { return v == false; };
    bool IsDefaultValue(float v) const { return v == 0; };
    bool IsDefaultValue(double v) const { return v == 0; };
    bool IsDefaultValue(int8 v) const { return v == 0; };
    bool IsDefaultValue(uint8 v) const { return v == 0; };
    bool IsDefaultValue(int16 v) const { return v == 0; };
    bool IsDefaultValue(uint16 v) const { return v == 0; };
    bool IsDefaultValue(int32 v) const { return v == 0; };
    bool IsDefaultValue(uint32 v) const { return v == 0; };
    bool IsDefaultValue(int64 v) const { return v == 0; };
    bool IsDefaultValue(uint64 v) const { return v == 0; };
    bool IsDefaultValue(const Vec2& v) const { return v.x == 0 && v.y == 0; };
    bool IsDefaultValue(const Vec3& v) const { return v.x == 0 && v.y == 0 && v.z == 0; };
    bool IsDefaultValue(const Ang3& v) const { return v.x == 0 && v.y == 0 && v.z == 0; };
    bool IsDefaultValue(const Quat& v) const { return v.w == 1.0f && v.v.x == 0 && v.v.y == 0 && v.v.z == 0; };
    bool IsDefaultValue(const ScriptAnyValue& v) const { return false; };
    bool IsDefaultValue(const CTimeValue& v) const { return v.GetValue() == 0; };
    bool IsDefaultValue(const char* str) const { return !str || !*str; };
    bool IsDefaultValue(const string& str) const { return str.empty(); };
    bool IsDefaultValue(const SSerializeString& str) const { return str.empty(); };
    //////////////////////////////////////////////////////////////////////////

    /*

    template <class T>
    bool IsDefaultValue( const T& v ) const { return false; };
    */
};

#endif // CRYINCLUDE_CRYSYSTEM_XML_SERIALIZEXMLWRITER_H
