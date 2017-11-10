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

// Description : Facilities for defining and combining general-purpose or specific options, for functions or structs.


#ifndef CRYINCLUDE_CRYCOMMON_OPTIONS_H
#define CRYINCLUDE_CRYCOMMON_OPTIONS_H
#pragma once


// Facilities for collecting options in a struct, and quickly constructing them.
// Used, for example, as construction argument. Safer and more informative than bool arguments.
//
// Example:
//      struct FObjectOpts;
//      struct CObject { CObject(FObjectOpts = ZERO); };
//      CObject object_def();
//      CObject object( FObjectOpts().Size(8).AllowGrowth(true) );

template<class TOpt, class T, class TContainer, int NInit = 0>
struct TOptVar
{
    typedef T   TValue;

    TOptVar()
        : _val(T(NInit))
    {
        offset(this);
    }
    TOptVar(T val)
        : _val(val) {}

    operator T () const
    {
        return _val;
    }
    T operator ()() const
    { return _val; }
    T operator +() const
    { return _val; }
    bool operator !() const
    { return !_val; }

    TContainer& operator()(T val)
    {
        _val = val;
        return *(TContainer*)((char*)this - offset());
    }

private:
    T       _val;

    static size_t offset(void* self = 0)
    {
        static size_t _offset = TContainer::static_offset(self);
        return _offset;
    }
};

#define OPT_STRUCT(TOpts)                                                                                    \
    typedef TOpts TThis; typedef uint TInt;                                                                  \
    static size_t static_offset(void* var) { static void* _first = var; return (char*)var - (char*)_first; } \
    TOpts(void* p = 0) {}                                                                                    \
    TOpts operator()() const { return TOpts(*this); }                                                        \

#define OPT_VAR(Type, Var)                            \
    enum E##Var {}; TOptVar<E##Var, Type, TThis> Var; \

#define OPT_VAR_INIT(Type, Var, init)                       \
    enum E##Var {}; TOptVar<E##Var, Type, TThis, init> Var; \

#define BIT_STRUCT(Struc, Int)                                                                  \
    typedef Struc TThis; typedef Int TInt;                                                      \
    TInt Mask() const { return *(const TInt*)this; }                                            \
    TInt& Mask() { return *(TInt*)this; }                                                       \
    Struc(TInt init = 0) { COMPILE_TIME_ASSERT(sizeof(TThis) == sizeof(TInt)); Mask() = init; } \

#define BIT_VAR(Var)                                     \
    TInt _##Var : 1;                                     \
    bool Var() const { return _##Var; }                  \
    TThis& Var(bool val) { _##Var = val; return *this; } \

#endif // CRYINCLUDE_CRYCOMMON_OPTIONS_H
