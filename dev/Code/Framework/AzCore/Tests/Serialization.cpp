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

#include "TestTypes.h"
#include "FileIOBaseTestTypes.h"

#include <AzCore/Component/ComponentApplicationBus.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/DataOverlayProviderMsgs.h>
#include <AzCore/Serialization/DataOverlayInstanceMsgs.h>
#include <AzCore/Serialization/DynamicSerializableField.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/DataPatch.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/list.h>
#include <AzCore/std/containers/forward_list.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/bitset.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/smart_ptr/intrusive_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/Quaternion.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Plane.h>

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/IO/Streamer.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/ByteContainerStream.h>

#if   defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE_OSX)
#   define AZ_ROOT_TEST_FOLDER  "./"
#elif defined(AZ_PLATFORM_ANDROID)
#   define AZ_ROOT_TEST_FOLDER  "/sdcard/"
#elif defined(AZ_PLATFORM_APPLE_IOS)
#   define AZ_ROOT_TEST_FOLDER "/Documents/"
#elif defined(AZ_PLATFORM_APPLE_TV)
#   define AZ_ROOT_TEST_FOLDER "/Library/Caches/"
#else
#   define AZ_ROOT_TEST_FOLDER  ""
#endif

namespace // anonymous
{
#if defined(AZ_PLATFORM_APPLE_IOS) || defined(AZ_PLATFORM_APPLE_TV)
    AZStd::string GetTestFolderPath()
    {
        return AZStd::string(getenv("HOME")) + AZ_ROOT_TEST_FOLDER;
    }
#else
    AZStd::string GetTestFolderPath()
    {
        return AZ_ROOT_TEST_FOLDER;
    }
#endif
} // anonymous namespace

namespace SerializeTestClasses {
    class MyClassBase1
    {
    public:
        AZ_RTTI(MyClassBase1, "{AA882C72-C7FB-4D19-A167-44BAF96C7D79}");

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassBase1>()->
                Version(1)->
                Field("data", &MyClassBase1::m_data);
        }

        virtual ~MyClassBase1() {}
        virtual void Set(float v) = 0;

        float m_data{ 0.0f };
    };

    class MyClassBase2
    {
    public:
        AZ_RTTI(MyClassBase2, "{E2DE87D8-15FD-417B-B7E4-5BDF05EA7088}");

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassBase2>()->
                Version(1)->
                Field("data", &MyClassBase2::m_data);
        }

        virtual ~MyClassBase2() {}
        virtual void Set(float v) = 0;

        float m_data{ 0.0f };
    };

    class MyClassBase3
    {
    public:
        AZ_RTTI(MyClassBase3, "{E9308B39-14B9-4760-A141-EBECFE8891D5}");

        // enum class EnumField : char // Test C++11
        enum EnumField
        {
            Option1,
            Option2,
            Option3,
        };

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassBase3>()->
                Version(1)->
                Field("data", &MyClassBase3::m_data)->
                Field("enum", &MyClassBase3::m_enum);
        }

        virtual ~MyClassBase3() {}
        virtual void Set(float v) = 0;

        float       m_data{ 0.f };
        EnumField   m_enum{ Option1 };
    };

    class MyClassMix
        : public MyClassBase1
        , public MyClassBase2
        , public MyClassBase3
    {
    public:
        AZ_RTTI(MyClassMix, "{A15003C6-797A-41BB-9D21-716DF0678D02}", MyClassBase1, MyClassBase2, MyClassBase3);
        AZ_CLASS_ALLOCATOR(MyClassMix, AZ::SystemAllocator, 0);

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassMix, MyClassBase1, MyClassBase2, MyClassBase3>()->
                Field("dataMix", &MyClassMix::m_dataMix);
        }

        void Set(float v) override
        {
            m_dataMix = v;
            MyClassBase1::m_data = v * 2;
            MyClassBase2::m_data = v * 3;
            MyClassBase3::m_data = v * 4;
        }

        bool operator==(const MyClassMix& rhs) const
        {
            return m_dataMix == rhs.m_dataMix
                   && MyClassBase1::m_data == rhs.MyClassBase1::m_data
                   && MyClassBase2::m_data == rhs.MyClassBase2::m_data
                   && MyClassBase3::m_data == rhs.MyClassBase3::m_data;
        }

        double m_dataMix{ 0. };
    };

    class MyClassMixNew
        : public MyClassBase1
        , public MyClassBase2
        , public MyClassBase3
    {
    public:
        AZ_RTTI(MyClassMixNew, "{A15003C6-797A-41BB-9D21-716DF0678D02}", MyClassBase1, MyClassBase2, MyClassBase3); // Use the same UUID as MyClassMix for conversion test
        AZ_CLASS_ALLOCATOR(MyClassMixNew, AZ::SystemAllocator, 0);

        static bool ConvertOldVersions(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            if (classElement.GetVersion() == 0)
            {
                // convert from version 0
                float sum = 0.f;
                for (int i = 0; i < classElement.GetNumSubElements(); )
                {
                    AZ::SerializeContext::DataElementNode& elementNode = classElement.GetSubElement(i);
                    if (elementNode.GetName() == AZ_CRC("dataMix", 0x041bcc8d))
                    {
                        classElement.RemoveElement(i);
                        continue;
                    }
                    else
                    {
                        // go through our base classes adding their data members
                        for (int j = 0; j < elementNode.GetNumSubElements(); ++j)
                        {
                            AZ::SerializeContext::DataElementNode& dataNode = elementNode.GetSubElement(j);
                            if (dataNode.GetName() == AZ_CRC("data", 0xadf3f363))
                            {
                                float data;
                                bool result = dataNode.GetData(data);
                                EXPECT_TRUE(result);
                                sum += data;
                                break;
                            }
                        }
                    }
                    ++i;
                }

                // add a new element
                int newElement = classElement.AddElement(context, "baseSum", AZ::SerializeTypeInfo<float>::GetUuid());
                if (newElement != -1)
                {
                    classElement.GetSubElement(newElement).SetData(context, sum);
                }

                return true;
            }

            return false; // just discard unknown versions
        }

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassMixNew, MyClassBase1, MyClassBase2, MyClassBase3>()->
                Version(1, &MyClassMixNew::ConvertOldVersions)->
                Field("baseSum", &MyClassMixNew::m_baseSum);
        }

        void Set(float v) override
        {
            MyClassBase1::m_data = v * 2;
            MyClassBase2::m_data = v * 3;
            MyClassBase3::m_data = v * 4;
            m_baseSum = v * 2 + v * 3 + v * 4;
        }

        bool operator==(const MyClassMixNew& rhs)
        {
            return m_baseSum == rhs.m_baseSum
                   && MyClassBase1::m_data == rhs.MyClassBase1::m_data
                   && MyClassBase2::m_data == rhs.MyClassBase2::m_data
                   && MyClassBase3::m_data == rhs.MyClassBase3::m_data;
        }

        float m_baseSum;
    };

    class MyClassMix2
        : public MyClassBase2
        , public MyClassBase3
        , public MyClassBase1
    {
    public:
        AZ_RTTI(MyClassMix2, "{D402F58C-812C-4c20-ABE5-E4AF43D66A71}", MyClassBase2, MyClassBase3, MyClassBase1);
        AZ_CLASS_ALLOCATOR(MyClassMix2, AZ::SystemAllocator, 0);

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassMix2, MyClassBase2, MyClassBase3, MyClassBase1>()->
                Field("dataMix", &MyClassMix2::m_dataMix);
        }

        void Set(float v) override
        {
            m_dataMix = v;
            MyClassBase1::m_data = v * 2;
            MyClassBase2::m_data = v * 3;
            MyClassBase3::m_data = v * 4;
        }

        bool operator==(const MyClassMix2& rhs)
        {
            return m_dataMix == rhs.m_dataMix
                   && MyClassBase1::m_data == rhs.MyClassBase1::m_data
                   && MyClassBase2::m_data == rhs.MyClassBase2::m_data
                   && MyClassBase3::m_data == rhs.MyClassBase3::m_data;
        }

        double m_dataMix;
    };

    class MyClassMix3
        : public MyClassBase3
        , public MyClassBase1
        , public MyClassBase2
    {
    public:
        AZ_RTTI(MyClassMix3, "{4179331A-F4AB-49D2-A14B-06B80CE5952C}", MyClassBase3, MyClassBase1, MyClassBase2);
        AZ_CLASS_ALLOCATOR(MyClassMix3, AZ::SystemAllocator, 0);

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<MyClassMix3, MyClassBase3, MyClassBase1, MyClassBase2>()->
                Field("dataMix", &MyClassMix3::m_dataMix);
        }

        void Set(float v) override
        {
            m_dataMix = v;
            MyClassBase1::m_data = v * 2;
            MyClassBase2::m_data = v * 3;
            MyClassBase3::m_data = v * 4;
        }

        bool operator==(const MyClassMix3& rhs)
        {
            return m_dataMix == rhs.m_dataMix
                   && MyClassBase1::m_data == rhs.MyClassBase1::m_data
                   && MyClassBase2::m_data == rhs.MyClassBase2::m_data
                   && MyClassBase3::m_data == rhs.MyClassBase3::m_data;
        }

        double m_dataMix;
    };

    struct UnregisteredBaseClass
    {
        AZ_RTTI(UnregisteredBaseClass, "{19C26D43-4512-40D8-B5F5-1A69872252D4}");
        virtual ~UnregisteredBaseClass() {}

        virtual void Func() = 0;
    };

    struct ChildOfUndeclaredBase
        : public UnregisteredBaseClass
    {
        AZ_CLASS_ALLOCATOR(ChildOfUndeclaredBase, AZ::SystemAllocator, 0);
        AZ_RTTI(ChildOfUndeclaredBase, "{85268A9C-1CC1-49C6-9E65-9B5089EBC4CD}", UnregisteredBaseClass);
        ChildOfUndeclaredBase()
            : m_data(0) {}

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<ChildOfUndeclaredBase>()->
                Field("data", &ChildOfUndeclaredBase::m_data);
        }

        void Func() override {}

        int m_data;
    };

    struct PolymorphicMemberPointers
    {
        AZ_CLASS_ALLOCATOR(PolymorphicMemberPointers, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(PolymorphicMemberPointers, "{06864A72-A2E2-40E1-A8F9-CC6C59BFBF2D}")

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<PolymorphicMemberPointers>()->
                Field("base1Mix", &PolymorphicMemberPointers::m_pBase1MyClassMix)->
                Field("base1Mix2", &PolymorphicMemberPointers::m_pBase1MyClassMix2)->
                Field("base1Mix3", &PolymorphicMemberPointers::m_pBase1MyClassMix3)->
                Field("base2Mix", &PolymorphicMemberPointers::m_pBase2MyClassMix)->
                Field("base2Mix2", &PolymorphicMemberPointers::m_pBase2MyClassMix2)->
                Field("base2Mix3", &PolymorphicMemberPointers::m_pBase2MyClassMix3)->
                Field("base3Mix", &PolymorphicMemberPointers::m_pBase3MyClassMix)->
                Field("base3Mix2", &PolymorphicMemberPointers::m_pBase3MyClassMix2)->
                Field("base3Mix3", &PolymorphicMemberPointers::m_pBase3MyClassMix3)->
                Field("memberWithUndeclaredBase", &PolymorphicMemberPointers::m_pMemberWithUndeclaredBase);
        }

        PolymorphicMemberPointers()
        {
            m_pBase1MyClassMix = nullptr;
            m_pBase1MyClassMix2 = nullptr;
            m_pBase1MyClassMix3 = nullptr;
            m_pBase2MyClassMix = nullptr;
            m_pBase2MyClassMix2 = nullptr;
            m_pBase2MyClassMix3 = nullptr;
            m_pBase3MyClassMix = nullptr;
            m_pBase3MyClassMix2 = nullptr;
            m_pBase3MyClassMix3 = nullptr;
            m_pMemberWithUndeclaredBase = nullptr;
        }

        virtual ~PolymorphicMemberPointers()
        {
            if (m_pBase1MyClassMix)
            {
                Unset();
            }
        }

        void Set()
        {
            (m_pBase1MyClassMix = aznew MyClassMix)->Set(10.f);
            (m_pBase1MyClassMix2 = aznew MyClassMix2)->Set(20.f);
            (m_pBase1MyClassMix3 = aznew MyClassMix3)->Set(30.f);
            (m_pBase2MyClassMix = aznew MyClassMix)->Set(100.f);
            (m_pBase2MyClassMix2 = aznew MyClassMix2)->Set(200.f);
            (m_pBase2MyClassMix3 = aznew MyClassMix3)->Set(300.f);
            (m_pBase3MyClassMix = aznew MyClassMix)->Set(1000.f);
            (m_pBase3MyClassMix2 = aznew MyClassMix2)->Set(2000.f);
            (m_pBase3MyClassMix3 = aznew MyClassMix3)->Set(3000.f);
            (m_pMemberWithUndeclaredBase = aznew ChildOfUndeclaredBase)->m_data = 1234;
        }

        void Unset()
        {
            delete m_pBase1MyClassMix; m_pBase1MyClassMix = nullptr;
            delete m_pBase1MyClassMix2;
            delete m_pBase1MyClassMix3;
            delete m_pBase2MyClassMix;
            delete m_pBase2MyClassMix2;
            delete m_pBase2MyClassMix3;
            delete m_pBase3MyClassMix;
            delete m_pBase3MyClassMix2;
            delete m_pBase3MyClassMix3;
            delete m_pMemberWithUndeclaredBase;
        }

        MyClassBase1*               m_pBase1MyClassMix;
        MyClassBase1*               m_pBase1MyClassMix2;
        MyClassBase1*               m_pBase1MyClassMix3;
        MyClassBase2*               m_pBase2MyClassMix;
        MyClassBase2*               m_pBase2MyClassMix2;
        MyClassBase2*               m_pBase2MyClassMix3;
        MyClassBase2*               m_pBase3MyClassMix;
        MyClassBase2*               m_pBase3MyClassMix2;
        MyClassBase2*               m_pBase3MyClassMix3;
        ChildOfUndeclaredBase*  m_pMemberWithUndeclaredBase;
    };

    struct BaseNoRtti
    {
        AZ_CLASS_ALLOCATOR(BaseNoRtti, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(BaseNoRtti, "{E57A19BA-EF68-4AFF-A534-2C90B9583781}")

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<BaseNoRtti>()->
                Field("data", &BaseNoRtti::m_data);
        }

        void Set() { m_data = false; }
        bool operator==(const BaseNoRtti& rhs) const { return m_data == rhs.m_data; }
        bool m_data;
    };

    struct BaseRtti
    {
        AZ_RTTI(BaseRtti, "{2581047D-26EC-4969-8354-BA0A4510C51A}");
        AZ_CLASS_ALLOCATOR(BaseRtti, AZ::SystemAllocator, 0);

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<BaseRtti>()->
                Field("data", &BaseRtti::m_data);
        }

        virtual ~BaseRtti() {}

        void Set() { m_data = true; }
        bool operator==(const BaseRtti& rhs) const { return m_data == rhs.m_data; }
        bool m_data;
    };

    struct DerivedNoRtti
        : public BaseNoRtti
    {
        AZ_CLASS_ALLOCATOR(DerivedNoRtti, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(DerivedNoRtti, "{B5E77A22-9C6F-4755-A074-FEFD8AC2C971}")

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<DerivedNoRtti, BaseNoRtti>()->
                Field("basesRtti", &DerivedNoRtti::m_basesRtti)->
                Field("basesNoRtti", &DerivedNoRtti::m_basesNoRtti);
        }

        void Set() { m_basesRtti = 0; m_basesNoRtti = 1; BaseNoRtti::Set(); }
        bool operator==(const DerivedNoRtti& rhs) const { return m_basesRtti == rhs.m_basesRtti && m_basesNoRtti == rhs.m_basesNoRtti && BaseNoRtti::operator==(static_cast<const BaseNoRtti&>(rhs)); }
        int m_basesRtti;
        int m_basesNoRtti;
    };

    struct DerivedRtti
        : public BaseRtti
    {
        AZ_RTTI(DerivedRtti, "{A14C419C-6F25-46A6-8D17-7777893073EF}", BaseRtti);
        AZ_CLASS_ALLOCATOR(DerivedRtti, AZ::SystemAllocator, 0);

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<DerivedRtti, BaseRtti>()->
                Field("basesRtti", &DerivedRtti::m_basesRtti)->
                Field("basesNoRtti", &DerivedRtti::m_basesNoRtti);
        }

        void Set() { m_basesRtti = 1; m_basesNoRtti = 0; BaseRtti::Set(); }
        bool operator==(const DerivedRtti& rhs) const { return m_basesRtti == rhs.m_basesRtti && m_basesNoRtti == rhs.m_basesNoRtti && BaseRtti::operator==(static_cast<const BaseRtti&>(rhs)); }
        int m_basesRtti;
        int m_basesNoRtti;
    };

    struct DerivedMix
        : public BaseNoRtti
        , public BaseRtti
    {
        AZ_RTTI(DerivedMix, "{BED5293B-3B80-4CEC-BB0F-2E56F921F550}", BaseRtti);
        AZ_CLASS_ALLOCATOR(DerivedMix, AZ::SystemAllocator, 0);

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<DerivedMix, BaseNoRtti, BaseRtti>()->
                Field("basesRtti", &DerivedMix::m_basesRtti)->
                Field("basesNoRtti", &DerivedMix::m_basesNoRtti);
        }

        void Set() { m_basesRtti = 1; m_basesNoRtti = 1; BaseNoRtti::Set(); BaseRtti::Set(); }
        bool operator==(const DerivedMix& rhs) const { return m_basesRtti == rhs.m_basesRtti && m_basesNoRtti == rhs.m_basesNoRtti && BaseNoRtti::operator==(static_cast<const BaseNoRtti&>(rhs)) && BaseRtti::operator==(static_cast<const BaseRtti&>(rhs)); }
        int m_basesRtti;
        int m_basesNoRtti;
    };

    struct BaseProtected
    {
        AZ_TYPE_INFO(BaseProtected, "{c6e244d8-ffd8-4710-900b-1d3dc4043ffe}");

        int m_pad; // Make sure there is no offset assumptions, for base members and we offset properly with in the base class.
        int m_data;

    protected:
        BaseProtected(int data = 0)
            : m_data(data) {}
    };

    struct DerivedWithProtectedBase
        : public BaseProtected
    {
        AZ_TYPE_INFO(DerivedWithProtectedBase, "{ad736023-a491-440a-84e3-5c507c969673}");

        DerivedWithProtectedBase(int data = 0)
            : BaseProtected(data)
        {}

        static void Reflect(AZ::SerializeContext& context)
        {
            // Expose base class field without reflecting the class
            context.Class<DerivedWithProtectedBase>()
                ->FieldFromBase<DerivedWithProtectedBase>("m_data", &DerivedWithProtectedBase::m_data);
        }
    };

    struct SmartPtrClass
    {
        AZ_CLASS_ALLOCATOR(SmartPtrClass, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(SmartPtrClass, "{A0A2D0A8-8D5D-454D-BE92-684C92C05B06}")

        SmartPtrClass(int data = 0)
            : m_counter(0)
            , m_data(data) {}

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<SmartPtrClass>()->
                Field("data", &SmartPtrClass::m_data);
        }

        //////////////////////////////////////////////////////////////////////////
        // For intrusive pointers
        void add_ref()  { ++m_counter; }
        void release()
        {
            --m_counter;
            if (m_counter == 0)
            {
                delete this;
            }
        }
        int m_counter;
        //////////////////////////////////////////////////////////////////////////

        int m_data;
    };

    struct Generics
    {
        AZ_CLASS_ALLOCATOR(Generics, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(Generics, "{ACA50B82-D04B-4ACF-9FF6-F780040C9EB9}")

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<Generics>()->
                Field("emptyTextData", &Generics::m_emptyTextData)->
                Field("textData", &Generics::m_textData)->
                Field("vectorInt", &Generics::m_vectorInt)->
                Field("vectorIntVector", &Generics::m_vectorIntVector)->
                Field("fixedVectorInt", &Generics::m_fixedVectorInt)->
                Field("listInt", &Generics::m_listInt)->
                Field("forwardListInt", &Generics::m_forwardListInt)->
                Field("setInt", &Generics::m_setInt)->
                Field("usetInt", &Generics::m_usetInt)->
                Field("umultisetInt", &Generics::m_umultisetInt)->
                Field("mapIntFloat", &Generics::m_mapIntFloat)->
                Field("umapIntFloat", &Generics::m_umapIntFloat)->
                Field("umultimapIntFloat", &Generics::m_umultimapIntFloat)->
                Field("umapPolymorphic", &Generics::m_umapPolymorphic)->
                Field("byteStream", &Generics::m_byteStream)->
                Field("bitSet", &Generics::m_bitSet)->
                Field("sharedPtr", &Generics::m_sharedPtr)->
                Field("intrusivePtr", &Generics::m_intrusivePtr)->
                Field("uniquePtr", &Generics::m_uniquePtr)->
                Field("emptyInitTextData", &Generics::m_emptyInitTextData);
        }

        Generics()
        {
            m_emptyInitTextData = "Some init text";
        }

        ~Generics()
        {
            if (m_umapPolymorphic.size() > 0)
            {
                Unset();
            }
        }

    private:
        // Workaround for VS2013 - Delete the copy constructor and make it private
        // https://connect.microsoft.com/VisualStudio/feedback/details/800328/std-is-copy-constructible-is-broken
        Generics(const Generics&) = delete;
    public:

        void Set()
        {
            m_emptyInitTextData = ""; // data was initialized, we set it to nothing (make sure we write empty strings)
            m_textData = "Random Text";
            m_vectorInt.push_back(1);
            m_vectorInt.push_back(2);
            m_vectorIntVector.push_back();
            m_vectorIntVector.back().push_back(5);
            m_fixedVectorInt.push_back(1000);
            m_fixedVectorInt.push_back(2000);
            m_fixedVectorInt.push_back(3000);
            m_fixedVectorInt.push_back(4000);
            m_fixedVectorInt.push_back(5000);
            m_listInt.push_back(10);
            m_forwardListInt.push_back(15);
            m_setInt.insert(20);
            m_usetInt.insert(20);
            m_umultisetInt.insert(20);
            m_umultisetInt.insert(20);
            m_mapIntFloat.insert(AZStd::make_pair(1, 5.f));
            m_mapIntFloat.insert(AZStd::make_pair(2, 10.f));
            m_umapIntFloat.insert(AZStd::make_pair(1, 5.f));
            m_umapIntFloat.insert(AZStd::make_pair(2, 10.f));
            m_umultimapIntFloat.insert(AZStd::make_pair(1, 5.f));
            m_umultimapIntFloat.insert(AZStd::make_pair(2, 10.f));
            m_umultimapIntFloat.insert(AZStd::make_pair(2, 20.f));
            m_umapPolymorphic.insert(AZStd::make_pair(1, aznew MyClassMix)).first->second->Set(100.f);
            m_umapPolymorphic.insert(AZStd::make_pair(2, aznew MyClassMix2)).first->second->Set(200.f);
            m_umapPolymorphic.insert(AZStd::make_pair(3, aznew MyClassMix3)).first->second->Set(300.f);

            AZ::u32 binaryData = 0xbad0f00d;
            m_byteStream.assign((AZ::u8*)&binaryData, (AZ::u8*)(&binaryData + 1));
            m_bitSet = AZStd::bitset<32>(AZStd::string("01011"));

            m_sharedPtr = AZStd::shared_ptr<SmartPtrClass>(aznew SmartPtrClass(122));
            m_intrusivePtr = AZStd::intrusive_ptr<SmartPtrClass>(aznew SmartPtrClass(233));
            m_uniquePtr = AZStd::unique_ptr<SmartPtrClass>(aznew SmartPtrClass(4242));
        }

        void Unset()
        {
            m_emptyTextData.set_capacity(0);
            m_emptyInitTextData.set_capacity(0);
            m_textData.set_capacity(0);
            m_vectorInt.set_capacity(0);
            m_vectorIntVector.set_capacity(0);
            m_listInt.clear();
            m_forwardListInt.clear();
            m_setInt.clear();
            m_mapIntFloat.clear();
            for (AZStd::unordered_map<int, MyClassBase1*>::iterator it = m_umapPolymorphic.begin(); it != m_umapPolymorphic.end(); ++it)
            {
                delete it->second;
            }
            m_umapPolymorphic.clear();
            m_byteStream.set_capacity(0);
            m_bitSet.reset();
            m_sharedPtr.reset();
            m_intrusivePtr.reset();
            m_uniquePtr.reset();
        }

        AZStd::string                               m_emptyTextData;
        AZStd::string                               m_emptyInitTextData;
        AZStd::string                               m_textData;
        AZStd::vector<int>                          m_vectorInt;
        AZStd::vector<AZStd::vector<int> >          m_vectorIntVector;
        AZStd::fixed_vector<int, 5>                 m_fixedVectorInt;
        AZStd::list<int>                            m_listInt;
        AZStd::forward_list<int>                    m_forwardListInt;
        AZStd::set<int>                             m_setInt;
        AZStd::map<int, float>                      m_mapIntFloat;
        AZStd::unordered_set<int>                   m_usetInt;
        AZStd::unordered_multiset<int>              m_umultisetInt;
        AZStd::unordered_map<int, float>            m_umapIntFloat;
        AZStd::unordered_map<int, MyClassBase1*>    m_umapPolymorphic;
        AZStd::unordered_multimap<int, float>       m_umultimapIntFloat;
        AZStd::vector<AZ::u8>                       m_byteStream;
        AZStd::bitset<32>                           m_bitSet;
        AZStd::shared_ptr<SmartPtrClass>            m_sharedPtr;
        AZStd::intrusive_ptr<SmartPtrClass>         m_intrusivePtr;
        AZStd::unique_ptr<SmartPtrClass>            m_uniquePtr;
    };

    struct GenericsNew
    {
        AZ_CLASS_ALLOCATOR(GenericsNew, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(GenericsNew, "{ACA50B82-D04B-4ACF-9FF6-F780040C9EB9}") // Match Generics ID for conversion test

        static bool ConvertOldVersions(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            if (classElement.GetVersion() == 0)
            {
                // convert from version 0
                for (int i = 0; i < classElement.GetNumSubElements(); )
                {
                    AZ::SerializeContext::DataElementNode& elementNode = classElement.GetSubElement(i);
                    if (elementNode.GetName() == AZ_CRC("textData", 0xf322c69d))
                    {
                        AZStd::string text;
                        bool result = elementNode.GetData(text);
                        EXPECT_TRUE(result);

                        int memberIdx = classElement.AddElement<AZStd::string>(context, "string");
                        if (memberIdx != -1)
                        {
                            AZ::SerializeContext::DataElementNode& memberNode = classElement.GetSubElement(memberIdx);
                            memberNode.SetData(context, text);
                        }
                        classElement.RemoveElement(i);
                    }
                    else if (elementNode.GetName() == AZ_CRC("emptyTextData", 0x61d55942))
                    {
                        AZStd::string text;
                        bool result = elementNode.GetData(text);
                        EXPECT_TRUE(result);
                        EXPECT_TRUE(text.empty()); // this should be empty

                        classElement.RemoveElement(i);
                    }
                    else if (elementNode.GetName() == AZ_CRC("vectorInt", 0xe61292a9))
                    {
                        int memberIdx = classElement.AddElement<AZStd::vector<int> >(context, "vectorInt2");
                        if (memberIdx != -1)
                        {
                            AZ::SerializeContext::DataElementNode& memberNode = classElement.GetSubElement(memberIdx);
                            for (int j = 0; j < elementNode.GetNumSubElements(); ++j)
                            {
                                AZ::SerializeContext::DataElementNode& vecElemNode = elementNode.GetSubElement(j);
                                int val;
                                bool result = vecElemNode.GetData(val);
                                EXPECT_TRUE(result);
                                int elemIdx = memberNode.AddElement<int>(context, AZ::SerializeContext::IDataContainer::GetDefaultElementName());
                                if (elemIdx != -1)
                                {
                                    memberNode.GetSubElement(elemIdx).SetData(context, val * 2);
                                }
                            }
                        }
                        classElement.RemoveElement(i);
                    }
                    else if (elementNode.GetName() == AZ_CRC("vectorIntVector", 0xd9c44f0b))
                    {
                        // add a new element
                        int newListIntList = classElement.AddElement<AZStd::list<AZStd::list<int> > >(context, "listIntList");
                        if (newListIntList != -1)
                        {
                            AZ::SerializeContext::DataElementNode& listIntListNode = classElement.GetSubElement(newListIntList);
                            for (int j = 0; j < elementNode.GetNumSubElements(); ++j)
                            {
                                AZ::SerializeContext::DataElementNode& subVecNode = elementNode.GetSubElement(j);
                                int newListInt = listIntListNode.AddElement<AZStd::list<int> >(context, AZ::SerializeContext::IDataContainer::GetDefaultElementName());
                                if (newListInt != -1)
                                {
                                    AZ::SerializeContext::DataElementNode& listIntNode = listIntListNode.GetSubElement(newListInt);
                                    for (int k = 0; k < subVecNode.GetNumSubElements(); ++k)
                                    {
                                        AZ::SerializeContext::DataElementNode& intNode = subVecNode.GetSubElement(k);
                                        int val;
                                        bool result = intNode.GetData(val);
                                        EXPECT_TRUE(result);
                                        int newInt = listIntNode.AddElement<int>(context, AZ::SerializeContext::IDataContainer::GetDefaultElementName());
                                        if (newInt != -1)
                                        {
                                            listIntNode.GetSubElement(newInt).SetData(context, val);
                                        }
                                    }
                                }
                            }
                        }
                        classElement.RemoveElement(i);
                    }
                    else if (elementNode.GetName() == AZ_CRC("emptyInitTextData", 0x17b55a4f)
                             || elementNode.GetName() == AZ_CRC("listInt", 0x4fbe090a)
                             || elementNode.GetName() == AZ_CRC("setInt", 0x62eb1299)
                             || elementNode.GetName() == AZ_CRC("usetInt")
                             || elementNode.GetName() == AZ_CRC("umultisetInt")
                             || elementNode.GetName() == AZ_CRC("mapIntFloat", 0xb558ac3f)
                             || elementNode.GetName() == AZ_CRC("umapIntFloat")
                             || elementNode.GetName() == AZ_CRC("umultimapIntFloat")
                             || elementNode.GetName() == AZ_CRC("byteStream", 0xda272a22)
                             || elementNode.GetName() == AZ_CRC("bitSet", 0x9dd4d1cb)
                             || elementNode.GetName() == AZ_CRC("sharedPtr", 0x033de7f0)
                             || elementNode.GetName() == AZ_CRC("intrusivePtr", 0x20733e45)
                             || elementNode.GetName() == AZ_CRC("uniquePtr", 0xdb6f5bd3)
                             || elementNode.GetName() == AZ_CRC("forwardListInt", 0xf54c1600)
                             || elementNode.GetName() == AZ_CRC("fixedVectorInt", 0xf7108293))
                    {
                        classElement.RemoveElement(i);
                    }
                    else
                    {
                        ++i;
                    }
                }

                // add a new element
                int newElement = classElement.AddElement(context, "newInt", AZ::SerializeTypeInfo<int>::GetUuid());
                if (newElement != -1)
                {
                    classElement.GetSubElement(newElement).SetData(context, 50);
                }

                return true;
            }

            return false; // just discard unknown versions
        }

        static void Reflect(AZ::SerializeContext& sc)
        {
            sc.Class<GenericsNew>()->
                Version(1, &GenericsNew::ConvertOldVersions)->
                Field("string", &GenericsNew::m_string)->
                Field("vectorInt2", &GenericsNew::m_vectorInt2)->
                Field("listIntList", &GenericsNew::m_listIntList)->
                Field("umapPolymorphic", &GenericsNew::m_umapPolymorphic)->
                Field("newInt", &GenericsNew::m_newInt);
        }

        ~GenericsNew()
        {
            if (m_umapPolymorphic.size() > 0)
            {
                Unset();
            }
        }

        void Set()
        {
            m_string = "Random Text";
            m_vectorInt2.push_back(1 * 2);
            m_vectorInt2.push_back(2 * 2);
            m_listIntList.push_back();
            m_listIntList.back().push_back(5);
            m_umapPolymorphic.insert(AZStd::make_pair(1, aznew MyClassMixNew)).first->second->Set(100.f);
            m_umapPolymorphic.insert(AZStd::make_pair(2, aznew MyClassMix2)).first->second->Set(200.f);
            m_umapPolymorphic.insert(AZStd::make_pair(3, aznew MyClassMix3)).first->second->Set(300.f);
            m_newInt = 50;
        }

        void Unset()
        {
            m_string.set_capacity(0);
            m_vectorInt2.set_capacity(0);
            m_listIntList.clear();
            for (AZStd::unordered_map<int, MyClassBase1*>::iterator it = m_umapPolymorphic.begin(); it != m_umapPolymorphic.end(); ++it)
            {
                delete it->second;
            }
            m_umapPolymorphic.clear();
        }

        AZStd::string                               m_string;           // rename m_textData to m_string
        AZStd::vector<int>                          m_vectorInt2;       // rename m_vectorInt to m_vectorInt2 and multiply all values by 2
        AZStd::list<AZStd::list<int> >              m_listIntList;      // convert vector<vector<int>> to list<list<int>>
        AZStd::unordered_map<int, MyClassBase1*>    m_umapPolymorphic;   // using new version of MyClassMix
        int                                         m_newInt;           // added new member
    };
}   // namespace SerializeTestClasses

namespace AZ {
    struct GenericClass
    {
        AZ_RTTI(GenericClass, "{F2DAA5D8-CA20-4DD4-8942-356458AF23A1}");
        virtual ~GenericClass() {}
    };

    class NullFactory
        : public SerializeContext::IObjectFactory
    {
    public:
        void* Create(const char* name) override
        {
            (void)name;
            AZ_Assert(false, "We cannot 'new' %s class, it should be used by value in a parent class!", name);
            return nullptr;
        }

        void Destroy(void*) override
        {
            // do nothing...
        }
    };

    template<>
    struct SerializeGenericTypeInfo<GenericClass>
    {
        class GenericClassGenericInfo
            : public GenericClassInfo
        {
        public:
            GenericClassGenericInfo()
            {
                m_classData = SerializeContext::ClassData::Create<GenericClass>("GenericClass", "{7A26F864-DADC-4bdf-8C4C-A162349031C6}", &m_factory);
            }

            SerializeContext::ClassData* GetClassData() override
            {
                return &m_classData;
            }

            size_t GetNumTemplatedArguments() override
            {
                return 1;
            }

            const Uuid& GetTemplatedTypeId(size_t element) override
            {
                (void)element;
                return SerializeGenericTypeInfo<GenericClass>::GetClassTypeId();
            }

            const Uuid& GetSpecializedTypeId() override
            {
                return azrtti_typeid<GenericClass>();
            }

            static GenericClassGenericInfo* Instance()
            {
                static GenericClassGenericInfo s_instance;
                return &s_instance;
            }

            NullFactory m_factory;
            SerializeContext::ClassData m_classData;
        };

        static GenericClassInfo* GetGenericInfo()
        {
            return GenericClassGenericInfo::Instance();
        }

        static const Uuid& GetClassTypeId()
        {
            return GenericClassGenericInfo::Instance()->m_classData.m_typeId;
        }
    };
    struct GenericChild
        : public GenericClass
    {
        AZ_RTTI(GenericChild, "{086E933D-F3F9-41EA-9AA9-BA80D3DCF90A}", GenericClass);
        virtual ~GenericChild() {}
    };
    template<>
    struct SerializeGenericTypeInfo<GenericChild>
    {
        class GenericClassGenericInfo
            : public GenericClassInfo
        {
        public:
            GenericClassGenericInfo()
            {
                m_classData = SerializeContext::ClassData::Create<GenericChild>("GenericChild", "{D1E1ACC0-7B90-48e9-999B-5825D4D4E397}", &m_factory);
            }

            SerializeContext::ClassData* GetClassData() override
            {
                return &m_classData;
            }

            size_t GetNumTemplatedArguments() override
            {
                return 1;
            }

            const Uuid& GetTemplatedTypeId(size_t element) override
            {
                (void)element;
                return SerializeGenericTypeInfo<GenericClass>::GetClassTypeId();
            }

            const Uuid& GetSpecializedTypeId() override
            {
                return azrtti_typeid<GenericChild>();
            }

            static GenericClassGenericInfo* Instance()
            {
                static GenericClassGenericInfo s_instance;
                return &s_instance;
            }

            NullFactory m_factory;
            SerializeContext::ClassData m_classData;
        };

        static GenericClassInfo* GetGenericInfo()
        {
            return GenericClassGenericInfo::Instance();
        }

        static const Uuid& GetClassTypeId()
        {
            return GenericClassGenericInfo::Instance()->m_classData.m_typeId;
        }
    };
}

using namespace SerializeTestClasses;
using namespace AZ;

namespace UnitTest
{
    /*
    * Base class for all serialization unit tests
    */
    class Serialization
        : public AllocatorsFixture
        , public ComponentApplicationBus::Handler
    {
    public:

        //////////////////////////////////////////////////////////////////////////
        // ComponentApplicationMessages
        ComponentApplication* GetApplication() override { return nullptr; }
        void RegisterComponentDescriptor(const ComponentDescriptor*) override { }
        void UnregisterComponentDescriptor(const ComponentDescriptor*) override { }
        bool AddEntity(Entity*) override { return false; }
        bool RemoveEntity(Entity*) override { return false; }
        bool DeleteEntity(const EntityId&) override { return false; }
        Entity* FindEntity(const EntityId&) override { return nullptr; }
        SerializeContext* GetSerializeContext() override { return m_serializeContext.get(); }
        BehaviorContext*  GetBehaviorContext() override { return nullptr; }
        const char* GetExecutableFolder() override { return nullptr; }
        const char* GetAppRoot() override { return nullptr; }
        Debug::DrillerManager* GetDrillerManager() override { return nullptr; }
        void ReloadModule(const char* moduleFullPath) override { (void)moduleFullPath; }
        void EnumerateEntities(const EntityCallback& /*callback*/) override {}
        void EnumerateModules(AZ::ComponentApplicationRequests::EnumerateModulesCallback) override {}
        //////////////////////////////////////////////////////////////////////////

        void SetUp() override
        {
            AllocatorsFixture::SetUp();

            m_serializeContext.reset(aznew AZ::SerializeContext());

            ComponentApplicationBus::Handler::BusConnect();

            AZ::AllocatorInstance<AZ::PoolAllocator>::Create();
            AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Create();

            IO::Streamer::Descriptor streamerDesc;
            AZStd::string testFolder  = GetTestFolderPath();
            if (testFolder.length() > 0)
            {
                streamerDesc.m_fileMountPoint = testFolder.c_str();
            }
            IO::Streamer::Create(streamerDesc);
        }

        void TearDown() override
        {
            m_serializeContext.reset();

            AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Destroy();
            AZ::AllocatorInstance<AZ::PoolAllocator>::Destroy();

            IO::Streamer::Destroy();

            ComponentApplicationBus::Handler::BusDisconnect();
            AllocatorsFixture::TearDown();
        }

    protected:
        AZStd::unique_ptr<AZ::SerializeContext> m_serializeContext;
    };

    /*
    * Test serialization of built-in types
    */
    class SerializeBasicTest
        : public Serialization
    {
    protected:
        enum ClassicEnum
        {
            CE_A = 0,
            CR_B = 1,
        };
        enum class ClassEnum : char
        {
            A = 0,
            B = 1,
        };

        AZStd::unique_ptr<SerializeContext> m_context;

        char            m_char;
        short           m_short;
        int             m_int;
        long            m_long;
        AZ::s64         m_s64;
        unsigned char   m_uchar;
        unsigned short  m_ushort;
        unsigned int    m_uint;
        unsigned long   m_ulong;
        AZ::u64         m_u64;
        float           m_float;
        double          m_double;
        bool            m_true;
        bool            m_false;

        // Math
        AZ::Uuid        m_uuid;
        VectorFloat     m_vectorFloat;
        Vector2         m_vector2;
        Vector3         m_vector3;
        Vector4         m_vector4;

        Transform       m_transform;
        Matrix3x3       m_matrix3x3;
        Matrix4x4       m_matrix4x4;

        Quaternion      m_quaternion;

        Aabb            m_aabb;
        Plane           m_plane;

        ClassicEnum     m_classicEnum;
        ClassEnum       m_classEnum;

    public:
        void SetUp() override
        {
            Serialization::SetUp();

            m_context.reset(aznew SerializeContext());
        }

        void TearDown() override
        {
            m_context.reset();

            Serialization::TearDown();
        }

        void OnLoadedClassReady(void* classPtr, const Uuid& classId, int* callCount)
        {
            switch ((*callCount)++)
            {
            case 0:
                EXPECT_EQ( SerializeTypeInfo<char>::GetUuid(), classId );
                EXPECT_EQ( m_char, *reinterpret_cast<char*>(classPtr) );
                delete reinterpret_cast<char*>(classPtr);
                break;
            case 1:
                EXPECT_EQ( SerializeTypeInfo<short>::GetUuid(), classId );
                EXPECT_EQ( m_short, *reinterpret_cast<short*>(classPtr) );
                delete reinterpret_cast<short*>(classPtr);
                break;
            case 2:
                EXPECT_EQ( SerializeTypeInfo<int>::GetUuid(), classId );
                EXPECT_EQ( m_int, *reinterpret_cast<int*>(classPtr) );
                delete reinterpret_cast<int*>(classPtr);
                break;
            case 3:
                EXPECT_EQ( SerializeTypeInfo<long>::GetUuid(), classId );
                EXPECT_EQ( m_long, *reinterpret_cast<long*>(classPtr) );
                delete reinterpret_cast<long*>(classPtr);
                break;
            case 4:
                EXPECT_EQ( SerializeTypeInfo<AZ::s64>::GetUuid(), classId );
                EXPECT_EQ( m_s64, *reinterpret_cast<AZ::s64*>(classPtr) );
                delete reinterpret_cast<AZ::s64*>(classPtr);
                break;
            case 5:
                EXPECT_EQ( SerializeTypeInfo<unsigned char>::GetUuid(), classId );
                EXPECT_EQ( m_uchar, *reinterpret_cast<unsigned char*>(classPtr) );
                delete reinterpret_cast<unsigned char*>(classPtr);
                break;
            case 6:
                EXPECT_EQ( SerializeTypeInfo<unsigned short>::GetUuid(), classId );
                EXPECT_EQ( m_ushort, *reinterpret_cast<unsigned short*>(classPtr) );
                delete reinterpret_cast<unsigned short*>(classPtr);
                break;
            case 7:
                EXPECT_EQ( SerializeTypeInfo<unsigned int>::GetUuid(), classId );
                EXPECT_EQ( m_uint, *reinterpret_cast<unsigned int*>(classPtr) );
                delete reinterpret_cast<unsigned int*>(classPtr);
                break;
            case 8:
                EXPECT_EQ( SerializeTypeInfo<unsigned long>::GetUuid(), classId );
                EXPECT_EQ( m_ulong, *reinterpret_cast<unsigned long*>(classPtr) );
                delete reinterpret_cast<unsigned long*>(classPtr);
                break;
            case 9:
                EXPECT_EQ( SerializeTypeInfo<AZ::u64>::GetUuid(), classId );
                EXPECT_EQ( m_u64, *reinterpret_cast<AZ::u64*>(classPtr) );
                delete reinterpret_cast<AZ::u64*>(classPtr);
                break;
            case 10:
                EXPECT_EQ( SerializeTypeInfo<float>::GetUuid(), classId );
                EXPECT_TRUE(fabsf(*reinterpret_cast<float*>(classPtr) - m_float) < 0.001f);
                delete reinterpret_cast<float*>(classPtr);
                break;
            case 11:
                EXPECT_EQ( SerializeTypeInfo<double>::GetUuid(), classId );
                EXPECT_TRUE(fabs(*reinterpret_cast<double*>(classPtr) - m_double) < 0.00000001);
                delete reinterpret_cast<double*>(classPtr);
                break;
            case 12:
                EXPECT_EQ( SerializeTypeInfo<bool>::GetUuid(), classId );
                EXPECT_EQ( m_true, *reinterpret_cast<bool*>(classPtr) );
                delete reinterpret_cast<bool*>(classPtr);
                break;
            case 13:
                EXPECT_EQ( SerializeTypeInfo<bool>::GetUuid(), classId );
                EXPECT_EQ( m_false, *reinterpret_cast<bool*>(classPtr) );
                delete reinterpret_cast<bool*>(classPtr);
                break;
            case 14:
                EXPECT_EQ( SerializeTypeInfo<AZ::Uuid>::GetUuid(), classId );
                EXPECT_EQ( m_uuid, *reinterpret_cast<AZ::Uuid*>(classPtr) );
                delete reinterpret_cast<AZ::Uuid*>(classPtr);
                break;
            case 15:
                EXPECT_EQ( SerializeTypeInfo<AZ::VectorFloat>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::VectorFloat*>(classPtr)->IsClose(m_vectorFloat, g_fltEps));
                delete reinterpret_cast<AZ::VectorFloat*>(classPtr);
                break;
            case 16:
                EXPECT_EQ( SerializeTypeInfo<AZ::Vector2>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Vector2*>(classPtr)->IsClose(m_vector2, g_fltEps));
                delete reinterpret_cast<AZ::Vector2*>(classPtr);
                break;
            case 17:
                EXPECT_EQ( SerializeTypeInfo<AZ::Vector3>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Vector3*>(classPtr)->IsClose(m_vector3, g_fltEps));
                delete reinterpret_cast<AZ::Vector3*>(classPtr);
                break;
            case 18:
                EXPECT_EQ( SerializeTypeInfo<AZ::Vector4>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Vector4*>(classPtr)->IsClose(m_vector4, g_fltEps));
                delete reinterpret_cast<AZ::Vector4*>(classPtr);
                break;
            case 19:
                EXPECT_EQ( SerializeTypeInfo<AZ::Transform>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Transform*>(classPtr)->IsClose(m_transform, g_fltEps));
                delete reinterpret_cast<AZ::Transform*>(classPtr);
                break;
            case 20:
                EXPECT_EQ( SerializeTypeInfo<AZ::Matrix3x3>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Matrix3x3*>(classPtr)->IsClose(m_matrix3x3, g_fltEps));
                delete reinterpret_cast<AZ::Matrix3x3*>(classPtr);
                break;
            case 21:
                EXPECT_EQ( SerializeTypeInfo<AZ::Matrix4x4>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Matrix4x4*>(classPtr)->IsClose(m_matrix4x4, g_fltEps));
                delete reinterpret_cast<AZ::Matrix4x4*>(classPtr);
                break;
            case 22:
                EXPECT_EQ( SerializeTypeInfo<AZ::Quaternion>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Quaternion*>(classPtr)->IsClose(m_quaternion, g_fltEps));
                delete reinterpret_cast<AZ::Quaternion*>(classPtr);
                break;
            case 23:
                EXPECT_EQ( SerializeTypeInfo<AZ::Aabb>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Aabb*>(classPtr)->GetMin().IsClose(m_aabb.GetMin(), g_fltEps));
                EXPECT_TRUE(reinterpret_cast<AZ::Aabb*>(classPtr)->GetMax().IsClose(m_aabb.GetMax(), g_fltEps));
                delete reinterpret_cast<AZ::Aabb*>(classPtr);
                break;
            case 24:
                EXPECT_EQ( SerializeTypeInfo<AZ::Plane>::GetUuid(), classId );
                EXPECT_TRUE(reinterpret_cast<AZ::Plane*>(classPtr)->GetPlaneEquationCoefficients().IsClose(m_plane.GetPlaneEquationCoefficients(), g_fltEps));
                delete reinterpret_cast<AZ::Plane*>(classPtr);
                break;
            case 25:
                EXPECT_EQ( SerializeTypeInfo<ClassicEnum>::GetUuid(), classId );
                EXPECT_EQ( CE_A, *reinterpret_cast<ClassicEnum*>(classPtr) );
                delete reinterpret_cast<ClassicEnum*>(classPtr);
                break;
            case 26:
                EXPECT_EQ( SerializeTypeInfo<ClassEnum>::GetUuid(), classId );
                EXPECT_EQ( ClassEnum::B, *reinterpret_cast<ClassEnum*>(classPtr) );
                delete reinterpret_cast<ClassEnum*>(classPtr);
                break;
            }
        }

        void SaveObjects(ObjectStream* writer)
        {
            bool success = true;

            success = writer->WriteClass(&m_char);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_short);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_int);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_long);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_s64);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_uchar);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_ushort);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_uint);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_ulong);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_u64);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_float);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_double);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_true);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_false);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_uuid);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_vectorFloat);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_vector2);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_vector3);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_vector4);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_transform);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_matrix3x3);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_matrix4x4);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_quaternion);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_aabb);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_plane);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_classicEnum);
            EXPECT_TRUE(success);
            success = writer->WriteClass(&m_classEnum);
            EXPECT_TRUE(success);
        }

        void OnDone(ObjectStream::Handle handle, bool success, bool* done)
        {
            (void)handle;
            EXPECT_TRUE(success);
            *done = true;
        }

        void TestSave(IO::GenericStream* stream, ObjectStream::StreamType format)
        {
            ObjectStream* objStream = ObjectStream::Create(stream, *m_context, format);
            SaveObjects(objStream);
            bool done = objStream->Finalize();
            EXPECT_TRUE(done);
        }

        void TestLoad(IO::GenericStream* stream)
        {
            int cbCount = 0;
            bool done = false;
            ObjectStream::ClassReadyCB readyCB(AZStd::bind(&SerializeBasicTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
            ObjectStream::CompletionCB doneCB(AZStd::bind(&SerializeBasicTest::OnDone, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &done));
            ObjectStream::LoadBlocking(stream, *m_context, readyCB);
            EXPECT_EQ( 27, cbCount );
        }

        void run()
        {
            m_char = -1;
            m_short = -2;
            m_int = -3;
            m_long = -4;
            m_s64 = -5;
            m_uchar = 1;
            m_ushort = 2;
            m_uint = 3;
            m_ulong = 4;
            m_u64 = 5;
            m_float = 2.f;
            m_double = 20.0000005;
            m_true = true;
            m_false = false;

            // Math
            m_uuid = AZ::Uuid::CreateString("{16490FB4-A7CE-4a8a-A882-F98DDA6A788F}");
            m_vectorFloat = 11.0f;
            m_vector2 = Vector2(1.0f, 2.0f);
            m_vector3 = Vector3(3.0f, 4.0f, 5.0f);
            m_vector4 = Vector4(6.0f, 7.0f, 8.0f, 9.0f);

            m_quaternion = Quaternion::CreateRotationZ(0.7f);
            m_transform = Transform::CreateRotationX(1.1f);
            m_matrix3x3 = Matrix3x3::CreateRotationY(0.5f);
            m_matrix4x4 = Matrix4x4::CreateFromQuaternionAndTranslation(m_quaternion, m_vector3);

            m_aabb.Set(-m_vector3, m_vector3);
            m_plane.Set(m_vector4);

            m_classicEnum = CE_A;
            m_classEnum = ClassEnum::B;

            TestFileIOBase fileIO;
            SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

            // XML version
            {
                AZ_TracePrintf("SerializeBasicTest", "\nWriting as XML...\n");
                IO::StreamerStream stream("serializebasictest.xml", IO::OpenMode::ModeWrite);
                TestSave(&stream, ObjectStream::ST_XML);
            }
            {
                AZ_TracePrintf("SerializeBasicTest", "Loading as XML...\n");
                IO::StreamerStream stream("serializebasictest.xml", IO::OpenMode::ModeRead);
                TestLoad(&stream);
            }

            // JSON version
            {
                AZ_TracePrintf("SerializeBasicTest", "\nWriting as JSON...\n");
                IO::StreamerStream stream("serializebasictest.json", IO::OpenMode::ModeWrite);
                TestSave(&stream, ObjectStream::ST_JSON);
            }
            {
                AZ_TracePrintf("SerializeBasicTest", "Loading as JSON...\n");
                IO::StreamerStream stream("serializebasictest.json", IO::OpenMode::ModeRead);
                TestLoad(&stream);
            }

            // Binary version
            {
                AZ_TracePrintf("SerializeBasicTest", "Writing as Binary...\n");
                IO::StreamerStream stream("serializebasictest.bin", IO::OpenMode::ModeWrite);
                TestSave(&stream, ObjectStream::ST_BINARY);
            }
            {
                AZ_TracePrintf("SerializeBasicTest", "Loading as Binary...\n");
                IO::StreamerStream stream("serializebasictest.bin", IO::OpenMode::ModeRead);
                TestLoad(&stream);
            }
        }
    };

    TEST_F(Serialization, BasicTest)
    {
        class SerializeBasicTest
        {
        protected:
            enum ClassicEnum
            {
                CE_A = 0,
                CR_B = 1,
            };
            enum class ClassEnum : char
            {
                A = 0,
                B = 1,
            };

            AZStd::unique_ptr<SerializeContext> m_context;

            char            m_char;
            short           m_short;
            int             m_int;
            long            m_long;
            AZ::s64         m_s64;
            unsigned char   m_uchar;
            unsigned short  m_ushort;
            unsigned int    m_uint;
            unsigned long   m_ulong;
            AZ::u64         m_u64;
            float           m_float;
            double          m_double;
            bool            m_true;
            bool            m_false;

            // Math
            AZ::Uuid        m_uuid;
            VectorFloat     m_vectorFloat;
            Vector2         m_vector2;
            Vector3         m_vector3;
            Vector4         m_vector4;

            Transform       m_transform;
            Matrix3x3       m_matrix3x3;
            Matrix4x4       m_matrix4x4;

            Quaternion      m_quaternion;

            Aabb            m_aabb;
            Plane           m_plane;

            ClassicEnum     m_classicEnum;
            ClassEnum       m_classEnum;

        public:
            void OnLoadedClassReady(void* classPtr, const Uuid& classId, int* callCount)
            {
                switch ((*callCount)++)
                {
                case 0:
                    EXPECT_EQ( SerializeTypeInfo<char>::GetUuid(), classId );
                    EXPECT_EQ( m_char, *reinterpret_cast<char*>(classPtr) );
                    delete reinterpret_cast<char*>(classPtr);
                    break;
                case 1:
                    EXPECT_EQ( SerializeTypeInfo<short>::GetUuid(), classId );
                    EXPECT_EQ( m_short, *reinterpret_cast<short*>(classPtr) );
                    delete reinterpret_cast<short*>(classPtr);
                    break;
                case 2:
                    EXPECT_EQ( SerializeTypeInfo<int>::GetUuid(), classId );
                    EXPECT_EQ( m_int, *reinterpret_cast<int*>(classPtr) );
                    delete reinterpret_cast<int*>(classPtr);
                    break;
                case 3:
                    EXPECT_EQ( SerializeTypeInfo<long>::GetUuid(), classId );
                    EXPECT_EQ( m_long, *reinterpret_cast<long*>(classPtr) );
                    delete reinterpret_cast<long*>(classPtr);
                    break;
                case 4:
                    EXPECT_EQ( SerializeTypeInfo<AZ::s64>::GetUuid(), classId );
                    EXPECT_EQ( m_s64, *reinterpret_cast<AZ::s64*>(classPtr) );
                    delete reinterpret_cast<AZ::s64*>(classPtr);
                    break;
                case 5:
                    EXPECT_EQ( SerializeTypeInfo<unsigned char>::GetUuid(), classId );
                    EXPECT_EQ( m_uchar, *reinterpret_cast<unsigned char*>(classPtr) );
                    delete reinterpret_cast<unsigned char*>(classPtr);
                    break;
                case 6:
                    EXPECT_EQ( SerializeTypeInfo<unsigned short>::GetUuid(), classId );
                    EXPECT_EQ( m_ushort, *reinterpret_cast<unsigned short*>(classPtr) );
                    delete reinterpret_cast<unsigned short*>(classPtr);
                    break;
                case 7:
                    EXPECT_EQ( SerializeTypeInfo<unsigned int>::GetUuid(), classId );
                    EXPECT_EQ( m_uint, *reinterpret_cast<unsigned int*>(classPtr) );
                    delete reinterpret_cast<unsigned int*>(classPtr);
                    break;
                case 8:
                    EXPECT_EQ( SerializeTypeInfo<unsigned long>::GetUuid(), classId );
                    EXPECT_EQ( m_ulong, *reinterpret_cast<unsigned long*>(classPtr) );
                    delete reinterpret_cast<unsigned long*>(classPtr);
                    break;
                case 9:
                    EXPECT_EQ( SerializeTypeInfo<AZ::u64>::GetUuid(), classId );
                    EXPECT_EQ( m_u64, *reinterpret_cast<AZ::u64*>(classPtr) );
                    delete reinterpret_cast<AZ::u64*>(classPtr);
                    break;
                case 10:
                    EXPECT_EQ( SerializeTypeInfo<float>::GetUuid(), classId );
                    EXPECT_TRUE(fabsf(*reinterpret_cast<float*>(classPtr) - m_float) < 0.001f);
                    delete reinterpret_cast<float*>(classPtr);
                    break;
                case 11:
                    EXPECT_EQ( SerializeTypeInfo<double>::GetUuid(), classId );
                    EXPECT_TRUE(fabs(*reinterpret_cast<double*>(classPtr) - m_double) < 0.00000001);
                    delete reinterpret_cast<double*>(classPtr);
                    break;
                case 12:
                    EXPECT_EQ( SerializeTypeInfo<bool>::GetUuid(), classId );
                    EXPECT_EQ( m_true, *reinterpret_cast<bool*>(classPtr) );
                    delete reinterpret_cast<bool*>(classPtr);
                    break;
                case 13:
                    EXPECT_EQ( SerializeTypeInfo<bool>::GetUuid(), classId );
                    EXPECT_EQ( m_false, *reinterpret_cast<bool*>(classPtr) );
                    delete reinterpret_cast<bool*>(classPtr);
                    break;
                case 14:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Uuid>::GetUuid(), classId );
                    EXPECT_EQ( m_uuid, *reinterpret_cast<AZ::Uuid*>(classPtr) );
                    delete reinterpret_cast<AZ::Uuid*>(classPtr);
                    break;
                case 15:
                    EXPECT_EQ( SerializeTypeInfo<AZ::VectorFloat>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::VectorFloat*>(classPtr)->IsClose(m_vectorFloat, g_fltEps));
                    delete reinterpret_cast<AZ::VectorFloat*>(classPtr);
                    break;
                case 16:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Vector2>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Vector2*>(classPtr)->IsClose(m_vector2, g_fltEps));
                    delete reinterpret_cast<AZ::Vector2*>(classPtr);
                    break;
                case 17:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Vector3>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Vector3*>(classPtr)->IsClose(m_vector3, g_fltEps));
                    delete reinterpret_cast<AZ::Vector3*>(classPtr);
                    break;
                case 18:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Vector4>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Vector4*>(classPtr)->IsClose(m_vector4, g_fltEps));
                    delete reinterpret_cast<AZ::Vector4*>(classPtr);
                    break;
                case 19:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Transform>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Transform*>(classPtr)->IsClose(m_transform, g_fltEps));
                    delete reinterpret_cast<AZ::Transform*>(classPtr);
                    break;
                case 20:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Matrix3x3>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Matrix3x3*>(classPtr)->IsClose(m_matrix3x3, g_fltEps));
                    delete reinterpret_cast<AZ::Matrix3x3*>(classPtr);
                    break;
                case 21:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Matrix4x4>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Matrix4x4*>(classPtr)->IsClose(m_matrix4x4, g_fltEps));
                    delete reinterpret_cast<AZ::Matrix4x4*>(classPtr);
                    break;
                case 22:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Quaternion>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Quaternion*>(classPtr)->IsClose(m_quaternion, g_fltEps));
                    delete reinterpret_cast<AZ::Quaternion*>(classPtr);
                    break;
                case 23:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Aabb>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Aabb*>(classPtr)->GetMin().IsClose(m_aabb.GetMin(), g_fltEps));
                    EXPECT_TRUE(reinterpret_cast<AZ::Aabb*>(classPtr)->GetMax().IsClose(m_aabb.GetMax(), g_fltEps));
                    delete reinterpret_cast<AZ::Aabb*>(classPtr);
                    break;
                case 24:
                    EXPECT_EQ( SerializeTypeInfo<AZ::Plane>::GetUuid(), classId );
                    EXPECT_TRUE(reinterpret_cast<AZ::Plane*>(classPtr)->GetPlaneEquationCoefficients().IsClose(m_plane.GetPlaneEquationCoefficients(), g_fltEps));
                    delete reinterpret_cast<AZ::Plane*>(classPtr);
                    break;
                case 25:
                    EXPECT_EQ( SerializeTypeInfo<ClassicEnum>::GetUuid(), classId );
                    EXPECT_EQ( CE_A, *reinterpret_cast<ClassicEnum*>(classPtr) );
                    delete reinterpret_cast<ClassicEnum*>(classPtr);
                    break;
                case 26:
                    EXPECT_EQ( SerializeTypeInfo<ClassEnum>::GetUuid(), classId );
                    EXPECT_EQ( ClassEnum::B, *reinterpret_cast<ClassEnum*>(classPtr) );
                    delete reinterpret_cast<ClassEnum*>(classPtr);
                    break;
                }
            }

            void SaveObjects(ObjectStream* writer)
            {
                bool success = true;

                success = writer->WriteClass(&m_char);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_short);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_int);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_long);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_s64);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_uchar);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_ushort);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_uint);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_ulong);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_u64);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_float);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_double);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_true);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_false);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_uuid);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_vectorFloat);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_vector2);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_vector3);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_vector4);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_transform);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_matrix3x3);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_matrix4x4);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_quaternion);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_aabb);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_plane);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_classicEnum);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_classEnum);
                EXPECT_TRUE(success);
            }

            void OnDone(ObjectStream::Handle handle, bool success, bool* done)
            {
                (void)handle;
                EXPECT_TRUE(success);
                *done = true;
            }

            void TestSave(IO::GenericStream* stream, ObjectStream::StreamType format)
            {
                ObjectStream* objStream = ObjectStream::Create(stream, *m_context, format);
                SaveObjects(objStream);
                bool done = objStream->Finalize();
                EXPECT_TRUE(done);
            }

            void TestLoad(IO::GenericStream* stream)
            {
                int cbCount = 0;
                bool done = false;
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&SerializeBasicTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
                ObjectStream::CompletionCB doneCB(AZStd::bind(&SerializeBasicTest::OnDone, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &done));
                ObjectStream::LoadBlocking(stream, *m_context, readyCB);
                EXPECT_EQ( 27, cbCount );
            }

            void run()
            {
                m_context.reset(aznew SerializeContext());

                m_char = -1;
                m_short = -2;
                m_int = -3;
                m_long = -4;
                m_s64 = -5;
                m_uchar = 1;
                m_ushort = 2;
                m_uint = 3;
                m_ulong = 4;
                m_u64 = 5;
                m_float = 2.f;
                m_double = 20.0000005;
                m_true = true;
                m_false = false;

                // Math
                m_uuid = AZ::Uuid::CreateString("{16490FB4-A7CE-4a8a-A882-F98DDA6A788F}");
                m_vectorFloat = 11.0f;
                m_vector2 = Vector2(1.0f, 2.0f);
                m_vector3 = Vector3(3.0f, 4.0f, 5.0f);
                m_vector4 = Vector4(6.0f, 7.0f, 8.0f, 9.0f);

                m_quaternion = Quaternion::CreateRotationZ(0.7f);
                m_transform = Transform::CreateRotationX(1.1f);
                m_matrix3x3 = Matrix3x3::CreateRotationY(0.5f);
                m_matrix4x4 = Matrix4x4::CreateFromQuaternionAndTranslation(m_quaternion, m_vector3);

                m_aabb.Set(-m_vector3, m_vector3);
                m_plane.Set(m_vector4);

                m_classicEnum = CE_A;
                m_classEnum = ClassEnum::B;

                TestFileIOBase fileIO;
                SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

                // XML version
                {
                    AZ_TracePrintf("SerializeBasicTest", "\nWriting as XML...\n");
                    IO::StreamerStream stream("serializebasictest.xml", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_XML);
                }
                {
                    AZ_TracePrintf("SerializeBasicTest", "Loading as XML...\n");
                    IO::StreamerStream stream("serializebasictest.xml", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // JSON version
                {
                    AZ_TracePrintf("SerializeBasicTest", "\nWriting as JSON...\n");
                    IO::StreamerStream stream("serializebasictest.json", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_JSON);
                }
                {
                    AZ_TracePrintf("SerializeBasicTest", "Loading as JSON...\n");
                    IO::StreamerStream stream("serializebasictest.json", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // Binary version
                {
                    AZ_TracePrintf("SerializeBasicTest", "Writing as Binary...\n");
                    IO::StreamerStream stream("serializebasictest.bin", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_BINARY);
                }
                {
                    AZ_TracePrintf("SerializeBasicTest", "Loading as Binary...\n");
                    IO::StreamerStream stream("serializebasictest.bin", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                m_context.reset();
            }
        };

        SerializeBasicTest test;
        test.run();
    }

    namespace AdvancedTest
    {
        class EmptyClass
        {
        public:
            AZ_CLASS_ALLOCATOR(EmptyClass, SystemAllocator, 0);
            AZ_TYPE_INFO(EmptyClass, "{7B2AA956-80A9-4996-B750-7CE8F7F79A29}")

                EmptyClass()
                : m_data(101)
            {
            }

            static void Reflect(SerializeContext& context)
            {
                context.Class<EmptyClass>()->
                    Version(1)->
                    SerializerForEmptyClass();
            }

            int m_data;
        };

        // We don't recommend using this pattern as it can be tricky to track why some objects are stored, we
        // wecommend that you have fully symetrical save/load.
        class ConditionalSave
        {
        public:
            AZ_CLASS_ALLOCATOR(ConditionalSave, SystemAllocator, 0);
            AZ_TYPE_INFO(ConditionalSave, "{E1E6910F-C029-492A-8163-026F6F69FC53}");

            ConditionalSave()
                : m_doSave(true)
                , m_data(201)
            {
            }

            static void Reflect(SerializeContext& context)
            {
                context.Class<ConditionalSave>()->
                    Version(1)->
                    SerializerDoSave([](const void* instance) { return reinterpret_cast<const ConditionalSave*>(instance)->m_doSave; })->
                    Field("m_data", &ConditionalSave::m_data);
            }

            bool m_doSave;
            int m_data;
        };
    }

    /*
    * Test serialization advanced features
    */
    TEST_F(Serialization, AdvancedTest)
    {
        using namespace AdvancedTest;
        class SerializeAdvancedTest
        {
        protected:
            AZStd::unique_ptr<SerializeContext> m_context;
            // TODO: Intrusive automatic events


            void TestSave(IO::GenericStream* stream, ObjectStream::StreamType format)
            {
                ObjectStream* objStream = ObjectStream::Create(stream, *m_context, format);
                EXPECT_TRUE(objStream->WriteClass(&m_emptyClass));
                EXPECT_TRUE(objStream->WriteClass(&m_conditionalSave));
                EXPECT_TRUE(objStream->Finalize());
            }

            void OnLoadedClassReady(void* classPtr, const Uuid& classId)
            {
                if (classId == SerializeTypeInfo<EmptyClass>::GetUuid())
                {
                    EmptyClass* emptyClass = reinterpret_cast<EmptyClass*>(classPtr);
                    EXPECT_EQ( m_emptyClass.m_data, emptyClass->m_data );
                    delete emptyClass;
                }
                else if (classId == SerializeTypeInfo<ConditionalSave>::GetUuid())
                {
                    ConditionalSave* conditionalSave = reinterpret_cast<ConditionalSave*>(classPtr);
                    EXPECT_EQ( true, m_conditionalSave.m_doSave ); // we should save the class only if we have enable it
                    EXPECT_EQ( m_conditionalSave.m_data, conditionalSave->m_data );
                    delete conditionalSave;
                }
            }

            void TestLoad(IO::GenericStream* stream)
            {
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&SerializeAdvancedTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2));
                EXPECT_TRUE(ObjectStream::LoadBlocking(stream, *m_context, readyCB));
            }

            EmptyClass      m_emptyClass;
            ConditionalSave m_conditionalSave;

        public:
            void run()
            {
                m_context.reset(aznew SerializeContext());
                EmptyClass::Reflect(*m_context);
                ConditionalSave::Reflect(*m_context);

                TestFileIOBase fileIO;
                SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

                // save and load
                {
                    AZ_TracePrintf("SerializeAdvancedTest", "\nWriting as XML...\n");
                    IO::StreamerStream stream("serializeadvancedtest.xml", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_XML);
                }
                {
                    AZ_TracePrintf("SerializeAdvancedTest", "Loading as XML...\n");
                    IO::StreamerStream stream("serializeadvancedtest.xml", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // save and load with conditional save
                m_conditionalSave.m_doSave = false;

                {
                    AZ_TracePrintf("SerializeAdvancedTest", "\nWriting as XML...\n");
                    IO::StreamerStream stream("serializeadvancedtest.xml", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_XML);
                }
                {
                    AZ_TracePrintf("SerializeAdvancedTest", "Loading as XML...\n");
                    IO::StreamerStream stream("serializeadvancedtest.xml", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                m_context.reset();
            }
        };

        SerializeAdvancedTest test;
        test.run();
    }

    namespace ContainersTest
    {
        struct ContainersStruct
        {
            AZ_TYPE_INFO(ContainersStruct, "{E88A592D-5221-49DE-9DFD-6E25B39C65C7}")
            AZStd::vector<int>                  m_vector;
            AZStd::fixed_vector<int, 5>         m_fixedVector;
            AZStd::array<int, 5>                m_array;
            AZStd::list<int>                    m_list;
            AZStd::forward_list<int>            m_forwardList;
            AZStd::unordered_set<int>           m_unorderedSet;
            AZStd::unordered_map<int, float>    m_unorderedMap;
            AZStd::bitset<10>                   m_bitset;
        };
    }

    /*
    * Test serialization of built-in container types
    */
    TEST_F(Serialization, ContainersTest)
    {
        using namespace ContainersTest;
        class ContainersTest
        {
        public:
            void VerifyLoad(void* classPtr, const Uuid& classId, ContainersStruct* controlData)
            {
                EXPECT_EQ( SerializeTypeInfo<ContainersStruct>::GetUuid(), classId );
                ContainersStruct* data = reinterpret_cast<ContainersStruct*>(classPtr);
                EXPECT_EQ( controlData->m_vector, data->m_vector );
                EXPECT_EQ( controlData->m_fixedVector, data->m_fixedVector );
                EXPECT_EQ( controlData->m_array[0], data->m_array[0] );
                EXPECT_EQ( controlData->m_array[1], data->m_array[1] );
                EXPECT_EQ( controlData->m_list, data->m_list );
                EXPECT_EQ( controlData->m_forwardList, data->m_forwardList );
                EXPECT_EQ( controlData->m_unorderedSet.size(), data->m_unorderedSet.size() );
                for (AZStd::unordered_set<int>::const_iterator it = data->m_unorderedSet.begin(), ctrlIt = controlData->m_unorderedSet.begin(); it != data->m_unorderedSet.end(); ++it, ++ctrlIt)
                {
                    EXPECT_EQ( *ctrlIt, *it );
                }
                EXPECT_EQ( controlData->m_unorderedMap.size(), data->m_unorderedMap.size() );
                for (AZStd::unordered_map<int, float>::const_iterator it = data->m_unorderedMap.begin(), ctrlIt = controlData->m_unorderedMap.begin(); it != data->m_unorderedMap.end(); ++it, ++ctrlIt)
                {
                    EXPECT_EQ( *ctrlIt, *it );
                }
                EXPECT_EQ( controlData->m_bitset, data->m_bitset );
                delete data;
            }

            void run()
            {
                SerializeContext serializeContext;
                serializeContext.Class<ContainersStruct>()
                    ->Field("m_vector", &ContainersStruct::m_vector)
                    ->Field("m_fixedVector", &ContainersStruct::m_fixedVector)
                    ->Field("m_array", &ContainersStruct::m_array)
                    ->Field("m_list", &ContainersStruct::m_list)
                    ->Field("m_forwardList", &ContainersStruct::m_forwardList)
                    ->Field("m_unorderedSet", &ContainersStruct::m_unorderedSet)
                    ->Field("m_unorderedMap", &ContainersStruct::m_unorderedMap)
                    ->Field("m_bitset", &ContainersStruct::m_bitset);

                ContainersStruct testData;
                testData.m_vector.push_back(1);
                testData.m_vector.push_back(2);
                testData.m_fixedVector.push_back(3);
                testData.m_fixedVector.push_back(4);
                testData.m_array[0] = 5;
                testData.m_array[1] = 6;
                testData.m_list.push_back(7);
                testData.m_list.push_back(8);
                testData.m_forwardList.push_back(9);
                testData.m_forwardList.push_back(10);
                testData.m_unorderedSet.insert(11);
                testData.m_unorderedSet.insert(12);
                testData.m_unorderedMap.insert(AZStd::make_pair(13, 13.f));
                testData.m_unorderedMap.insert(AZStd::make_pair(14, 14.f));
                testData.m_bitset.set(0);
                testData.m_bitset.set(9);

                // XML
                AZStd::vector<char> xmlBuffer;
                IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBuffer);
                ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, serializeContext, ObjectStream::ST_XML);
                xmlObjStream->WriteClass(&testData);
                xmlObjStream->Finalize();

                AZ::IO::SystemFile tmpOut;
                tmpOut.Open("SerializeContainersTest.xml", AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY);
                tmpOut.Write(xmlStream.GetData()->data(), xmlStream.GetLength());
                tmpOut.Close();

                xmlStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&ContainersTest::VerifyLoad, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &testData));
                ObjectStream::LoadBlocking(&xmlStream, serializeContext, readyCB);
            }
        };

        ContainersTest test;
        test.run();
    }

    /*
    * Test serialization of derived classes
    */
    TEST_F(Serialization, InheritanceTest)
    {
        class InheritanceTest
        {
            AZStd::unique_ptr<SerializeContext> m_context;
            BaseNoRtti                  m_baseNoRtti;
            BaseRtti                    m_baseRtti;
            DerivedNoRtti               m_derivedNoRtti;
            DerivedRtti                 m_derivedRtti;
            DerivedMix                  m_derivedMix;
            MyClassMix                  m_multiRtti;
            ChildOfUndeclaredBase       m_childOfUndeclaredBase;
            PolymorphicMemberPointers   m_morphingMemberPointers;
            DerivedWithProtectedBase    m_exposeBaseClassWithoutReflectingIt;
            int                         m_enumHierarchyCount;

        public:
            void OnLoadedClassReady(void* classPtr, const Uuid& classId, int* callCount)
            {
                switch ((*callCount)++)
                {
                case 0:
                    EXPECT_EQ( SerializeTypeInfo<BaseNoRtti>::GetUuid(), classId );
                    EXPECT_EQ( m_baseNoRtti, *reinterpret_cast<BaseNoRtti*>(classPtr) );
                    delete reinterpret_cast<BaseNoRtti*>(classPtr);
                    break;
                case 1:
                    EXPECT_EQ( SerializeTypeInfo<BaseRtti>::GetUuid(), classId );
                    EXPECT_EQ( m_baseRtti, *reinterpret_cast<BaseRtti*>(classPtr) );
                    delete reinterpret_cast<BaseRtti*>(classPtr);
                    break;
                case 2:
                    EXPECT_EQ( SerializeTypeInfo<DerivedNoRtti>::GetUuid(), classId );
                    EXPECT_EQ( m_derivedNoRtti, *reinterpret_cast<DerivedNoRtti*>(classPtr) );
                    delete reinterpret_cast<DerivedNoRtti*>(classPtr);
                    break;
                case 3:
                    EXPECT_EQ( SerializeTypeInfo<DerivedRtti>::GetUuid(), classId );
                    EXPECT_EQ( m_derivedRtti, *reinterpret_cast<DerivedRtti*>(classPtr) );
                    delete reinterpret_cast<DerivedRtti*>(classPtr);
                    break;
                case 4:
                    EXPECT_EQ( SerializeTypeInfo<DerivedMix>::GetUuid(), classId );
                    EXPECT_EQ( m_derivedMix, *reinterpret_cast<DerivedMix*>(classPtr) );
                    delete reinterpret_cast<DerivedMix*>(classPtr);
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                    EXPECT_EQ( SerializeTypeInfo<MyClassMix>::GetUuid(), classId );
                    EXPECT_TRUE( m_multiRtti == *reinterpret_cast<MyClassMix*>(classPtr) );
                    delete reinterpret_cast<MyClassMix*>(classPtr);
                    break;
                case 9:
                case 10:
                    EXPECT_EQ( SerializeTypeInfo<ChildOfUndeclaredBase>::GetUuid(), classId );
                    EXPECT_EQ( m_childOfUndeclaredBase.m_data, reinterpret_cast<ChildOfUndeclaredBase*>(classPtr)->m_data );
                    delete reinterpret_cast<ChildOfUndeclaredBase*>(classPtr);
                    break;
                case 11:
                {
                    EXPECT_EQ( SerializeTypeInfo<PolymorphicMemberPointers>::GetUuid(), classId );
                    PolymorphicMemberPointers* obj = reinterpret_cast<PolymorphicMemberPointers*>(classPtr);
                    AZ_TEST_ASSERT(obj->m_pBase1MyClassMix && obj->m_pBase1MyClassMix->RTTI_GetType() == SerializeTypeInfo<MyClassMix>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix*>(obj->m_pBase1MyClassMix) == *static_cast<MyClassMix*>(m_morphingMemberPointers.m_pBase1MyClassMix));
                    AZ_TEST_ASSERT(obj->m_pBase1MyClassMix2 && obj->m_pBase1MyClassMix2->RTTI_GetType() == SerializeTypeInfo<MyClassMix2>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix2*>(obj->m_pBase1MyClassMix2) == *static_cast<MyClassMix2*>(m_morphingMemberPointers.m_pBase1MyClassMix2));
                    AZ_TEST_ASSERT(obj->m_pBase1MyClassMix3 && obj->m_pBase1MyClassMix3->RTTI_GetType() == SerializeTypeInfo<MyClassMix3>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix3*>(obj->m_pBase1MyClassMix3) == *static_cast<MyClassMix3*>(m_morphingMemberPointers.m_pBase1MyClassMix3));
                    AZ_TEST_ASSERT(obj->m_pBase2MyClassMix && obj->m_pBase2MyClassMix->RTTI_GetType() == SerializeTypeInfo<MyClassMix>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix*>(obj->m_pBase2MyClassMix) == *static_cast<MyClassMix*>(m_morphingMemberPointers.m_pBase2MyClassMix));
                    AZ_TEST_ASSERT(obj->m_pBase2MyClassMix2 && obj->m_pBase2MyClassMix2->RTTI_GetType() == SerializeTypeInfo<MyClassMix2>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix2*>(obj->m_pBase2MyClassMix2) == *static_cast<MyClassMix2*>(m_morphingMemberPointers.m_pBase2MyClassMix2));
                    AZ_TEST_ASSERT(obj->m_pBase2MyClassMix3 && obj->m_pBase2MyClassMix3->RTTI_GetType() == SerializeTypeInfo<MyClassMix3>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix3*>(obj->m_pBase2MyClassMix3) == *static_cast<MyClassMix3*>(m_morphingMemberPointers.m_pBase2MyClassMix3));
                    AZ_TEST_ASSERT(obj->m_pBase3MyClassMix && obj->m_pBase3MyClassMix->RTTI_GetType() == SerializeTypeInfo<MyClassMix>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix*>(obj->m_pBase3MyClassMix) == *static_cast<MyClassMix*>(m_morphingMemberPointers.m_pBase3MyClassMix));
                    AZ_TEST_ASSERT(obj->m_pBase3MyClassMix2 && obj->m_pBase3MyClassMix2->RTTI_GetType() == SerializeTypeInfo<MyClassMix2>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix2*>(obj->m_pBase3MyClassMix2) == *static_cast<MyClassMix2*>(m_morphingMemberPointers.m_pBase3MyClassMix2));
                    AZ_TEST_ASSERT(obj->m_pBase3MyClassMix3 && obj->m_pBase3MyClassMix3->RTTI_GetType() == SerializeTypeInfo<MyClassMix3>::GetUuid());
                    AZ_TEST_ASSERT(*static_cast<MyClassMix3*>(obj->m_pBase3MyClassMix3) == *static_cast<MyClassMix3*>(m_morphingMemberPointers.m_pBase3MyClassMix3));
                    AZ_TEST_ASSERT(obj->m_pMemberWithUndeclaredBase && obj->m_pMemberWithUndeclaredBase->RTTI_GetType() == SerializeTypeInfo<ChildOfUndeclaredBase>::GetUuid());
                    AZ_TEST_ASSERT(static_cast<ChildOfUndeclaredBase*>(obj->m_pMemberWithUndeclaredBase)->m_data == static_cast<ChildOfUndeclaredBase*>(m_morphingMemberPointers.m_pMemberWithUndeclaredBase)->m_data);
                    delete obj;
                }
                break;
                case 12:
                {
                    EXPECT_EQ( SerializeTypeInfo<DerivedWithProtectedBase>::GetUuid(), classId );
                    DerivedWithProtectedBase* obj = reinterpret_cast<DerivedWithProtectedBase*>(classPtr);
                    EXPECT_EQ( m_exposeBaseClassWithoutReflectingIt.m_data, obj->m_data );
                    delete obj;
                }
                break;
                }
                ;
            }

            void SaveObjects(ObjectStream* writer)
            {
                bool success = writer->WriteClass(&m_baseNoRtti);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_baseRtti);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_derivedNoRtti);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_derivedRtti);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_derivedMix);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_multiRtti);
                EXPECT_TRUE(success);
                success = writer->WriteClass(static_cast<MyClassBase1*>(&m_multiRtti));                     // serialize with pointer to base 1
                EXPECT_TRUE(success);
                success = writer->WriteClass(static_cast<MyClassBase2*>(&m_multiRtti));                     // serialize with pointer to base 2
                EXPECT_TRUE(success);
                success = writer->WriteClass(static_cast<MyClassBase3*>(&m_multiRtti));                     // serialize with pointer to base 3
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_childOfUndeclaredBase);
                EXPECT_TRUE(success);
                success = writer->WriteClass(static_cast<UnregisteredBaseClass*>(&m_childOfUndeclaredBase));// serialize with pointer to unregistered base
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_morphingMemberPointers);
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_exposeBaseClassWithoutReflectingIt);
            }

            void OnDone(ObjectStream::Handle handle, bool success, bool* done)
            {
                (void)handle;
                EXPECT_TRUE(success);
                *done = true;
            }

            bool EnumNoDhrttiDerived(const SerializeContext::ClassData* classData, const Uuid& typeId)
            {
                EXPECT_TRUE(typeId.IsNull());
                // Check if the derived classes are correct
                AZ_TEST_ASSERT(classData->m_typeId == AzTypeInfo<MyClassMix>::Uuid() ||
                    classData->m_typeId == AzTypeInfo<MyClassMix2>::Uuid() ||
                    classData->m_typeId == AzTypeInfo<MyClassMix3>::Uuid());
                ++m_enumHierarchyCount;
                return true;
            }

            bool EnumNoDhrttiBase(const SerializeContext::ClassData* classData, const Uuid& typeId)
            {
                EXPECT_TRUE(typeId.IsNull());
                // Check if the derived classes are correct
                AZ_TEST_ASSERT(classData->m_typeId == AzTypeInfo<MyClassBase1>::Uuid() ||
                    classData->m_typeId == AzTypeInfo<MyClassBase2>::Uuid() ||
                    classData->m_typeId == AzTypeInfo<MyClassBase3>::Uuid());
                ++m_enumHierarchyCount;
                return true;
            }

            void TestSave(IO::GenericStream* stream, ObjectStream::StreamType format)
            {
                ObjectStream* objStream = ObjectStream::Create(stream, *m_context, format);
                SaveObjects(objStream);
                bool done = objStream->Finalize();
                EXPECT_TRUE(done);
            }

            void TestLoad(IO::GenericStream* stream)
            {
                int cbCount = 0;
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&InheritanceTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
                ObjectStream::LoadBlocking(stream, *m_context, readyCB);
                EXPECT_EQ( 13, cbCount );
            }

            void run()
            {
                m_context.reset(aznew SerializeContext());

                BaseNoRtti::Reflect(*m_context);
                BaseRtti::Reflect(*m_context);
                DerivedNoRtti::Reflect(*m_context);
                DerivedRtti::Reflect(*m_context);
                DerivedMix::Reflect(*m_context);
                MyClassBase1::Reflect(*m_context);
                MyClassBase2::Reflect(*m_context);
                MyClassBase3::Reflect(*m_context);
                MyClassMix::Reflect(*m_context);
                MyClassMix2::Reflect(*m_context);
                MyClassMix3::Reflect(*m_context);
                ChildOfUndeclaredBase::Reflect(*m_context);
                PolymorphicMemberPointers::Reflect(*m_context);
                // Expose base class field without reflecting the class
                DerivedWithProtectedBase::Reflect(*m_context);

                //////////////////////////////////////////////////////////////////////////
                // check reflection enumeration
                m_enumHierarchyCount = 0;
                m_context->EnumerateDerived<MyClassBase1>(AZStd::bind(&InheritanceTest::EnumNoDhrttiDerived, this, AZStd::placeholders::_1, AZStd::placeholders::_2));
                EXPECT_EQ( 3, m_enumHierarchyCount );

                m_enumHierarchyCount = 0;
                m_context->EnumerateBase<MyClassMix>(AZStd::bind(&InheritanceTest::EnumNoDhrttiBase, this, AZStd::placeholders::_1, AZStd::placeholders::_2));
                EXPECT_EQ( 3, m_enumHierarchyCount );

                //////////////////////////////////////////////////////////////////////////

                m_baseNoRtti.Set();
                m_baseRtti.Set();
                m_derivedNoRtti.Set();
                m_derivedRtti.Set();
                m_derivedMix.Set();
                m_multiRtti.Set(100.f);
                m_childOfUndeclaredBase.m_data = 1234;
                m_morphingMemberPointers.Set();
                m_exposeBaseClassWithoutReflectingIt.m_data = 203;

                TestFileIOBase fileIO;
                SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);
                // XML
                {
                    AZ_TracePrintf("InheritanceTest", "\nWriting XML...\n");
                    IO::StreamerStream stream("serializeinheritancetest.xml", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_XML);
                }
                {
                    AZ_TracePrintf("InheritanceTest", "Loading XML...\n");
                    IO::StreamerStream stream("serializeinheritancetest.xml", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // JSON
                {
                    AZ_TracePrintf("InheritanceTest", "\nWriting JSON...\n");
                    IO::StreamerStream stream("serializeinheritancetest.json", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_JSON);
                }
                {
                    AZ_TracePrintf("InheritanceTest", "Loading JSON...\n");
                    IO::StreamerStream stream("serializeinheritancetest.json", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // Binary
                {
                    AZ_TracePrintf("InheritanceTest", "Writing Binary...\n");
                    IO::StreamerStream stream("serializeinheritancetest.bin", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_BINARY);
                }
                {
                    AZ_TracePrintf("InheritanceTest", "Loading Binary...\n");
                    IO::StreamerStream stream("serializeinheritancetest.bin", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                m_morphingMemberPointers.Unset();
                m_context.reset();
            }
        };

        InheritanceTest test;
        test.run();
    }

    /*
    * Test serialization of generic members
    */
    TEST_F(Serialization, GenericsTest)
    {
        class GenericsTest
        {
            AZStd::unique_ptr<SerializeContext>    m_context;
            Generics            m_generics;

        public:
            void OnLoadedClassReady(void* classPtr, const Uuid& classId, int* callCount)
            {
                (*callCount)++;
                EXPECT_EQ( SerializeTypeInfo<Generics>::GetUuid(), classId );
                Generics* obj = reinterpret_cast<Generics*>(classPtr);
                EXPECT_EQ( m_generics.m_emptyTextData, obj->m_emptyTextData );
                EXPECT_EQ( m_generics.m_emptyInitTextData, obj->m_emptyInitTextData );
                EXPECT_EQ( m_generics.m_textData, obj->m_textData );
                EXPECT_EQ( m_generics.m_vectorInt.size(), obj->m_vectorInt.size() );
                for (size_t i = 0; i < obj->m_vectorInt.size(); ++i)
                {
                    EXPECT_EQ( m_generics.m_vectorInt[i], obj->m_vectorInt[i] );
                }
                EXPECT_EQ( m_generics.m_vectorIntVector.size(), obj->m_vectorIntVector.size() );
                for (size_t i = 0; i < obj->m_vectorIntVector.size(); ++i)
                {
                    EXPECT_EQ( m_generics.m_vectorIntVector[i].size(), obj->m_vectorIntVector[i].size() );
                    for (size_t j = 0; j < obj->m_vectorIntVector[i].size(); ++j)
                    {
                        EXPECT_EQ( m_generics.m_vectorIntVector[i][j], obj->m_vectorIntVector[i][j] );
                    }
                }
                EXPECT_EQ( m_generics.m_fixedVectorInt.size(), obj->m_fixedVectorInt.size() );
                for (size_t i = 0; i < obj->m_fixedVectorInt.size(); ++i)
                {
                    EXPECT_EQ( m_generics.m_fixedVectorInt[i], obj->m_fixedVectorInt[i] );
                }
                EXPECT_EQ( m_generics.m_listInt.size(), obj->m_listInt.size() );
                for (AZStd::list<int>::const_iterator it1 = obj->m_listInt.begin(), it2 = m_generics.m_listInt.begin(); it1 != obj->m_listInt.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( *it2, *it1 );
                }
                EXPECT_EQ( m_generics.m_forwardListInt.size(), obj->m_forwardListInt.size() );
                for (AZStd::forward_list<int>::const_iterator it1 = obj->m_forwardListInt.begin(), it2 = m_generics.m_forwardListInt.begin(); it1 != obj->m_forwardListInt.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( *it2, *it1 );
                }
                EXPECT_EQ( m_generics.m_setInt.size(), obj->m_setInt.size() );
                for (auto it1 = obj->m_setInt.begin(), it2 = m_generics.m_setInt.begin(); it1 != obj->m_setInt.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( *it2, *it1 );
                }
                EXPECT_EQ( m_generics.m_usetInt.size(), obj->m_usetInt.size() );
                for (auto it1 = obj->m_usetInt.begin(), it2 = m_generics.m_usetInt.begin(); it1 != obj->m_usetInt.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( *it2, *it1 );
                }
                EXPECT_EQ( m_generics.m_umultisetInt.size(), obj->m_umultisetInt.size() );
                for (auto it1 = obj->m_umultisetInt.begin(), it2 = m_generics.m_umultisetInt.begin(); it1 != obj->m_umultisetInt.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( *it2, *it1 );
                }
                EXPECT_EQ( m_generics.m_mapIntFloat.size(), obj->m_mapIntFloat.size() );
                for (auto it1 = obj->m_mapIntFloat.begin(), it2 = m_generics.m_mapIntFloat.begin(); it1 != obj->m_mapIntFloat.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( it2->first, it1->first );
                    EXPECT_EQ( it2->second, it1->second );
                }
                EXPECT_EQ( m_generics.m_umapIntFloat.size(), obj->m_umapIntFloat.size() );
                for (auto it1 = obj->m_umapIntFloat.begin(), it2 = m_generics.m_umapIntFloat.begin(); it1 != obj->m_umapIntFloat.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( it2->first, it1->first );
                    EXPECT_EQ( it2->second, it1->second );
                }
                EXPECT_EQ( m_generics.m_umultimapIntFloat.size(), obj->m_umultimapIntFloat.size() );
                for (auto it1 = obj->m_umultimapIntFloat.begin(), it2 = m_generics.m_umultimapIntFloat.begin(); it1 != obj->m_umultimapIntFloat.end(); ++it1, ++it2)
                {
                    EXPECT_EQ( it2->first, it1->first );
                    EXPECT_EQ( it2->second, it1->second );
                }
                EXPECT_EQ( 3, obj->m_umapPolymorphic.size() );
                EXPECT_EQ( SerializeTypeInfo<MyClassMix>::GetUuid(), obj->m_umapPolymorphic[1]->RTTI_GetType() );
                EXPECT_TRUE( *static_cast<MyClassMix*>(m_generics.m_umapPolymorphic[1]) == *static_cast<MyClassMix*>(obj->m_umapPolymorphic[1]) );
                EXPECT_EQ( SerializeTypeInfo<MyClassMix2>::GetUuid(), obj->m_umapPolymorphic[2]->RTTI_GetType() );
                EXPECT_TRUE( *static_cast<MyClassMix2*>(m_generics.m_umapPolymorphic[2]) == *static_cast<MyClassMix2*>(obj->m_umapPolymorphic[2]) );
                EXPECT_EQ( SerializeTypeInfo<MyClassMix3>::GetUuid(), obj->m_umapPolymorphic[3]->RTTI_GetType() );
                EXPECT_TRUE( *static_cast<MyClassMix3*>(m_generics.m_umapPolymorphic[3]) == *static_cast<MyClassMix3*>(obj->m_umapPolymorphic[3]) );
                EXPECT_EQ( m_generics.m_byteStream, obj->m_byteStream );
                EXPECT_EQ( m_generics.m_bitSet, obj->m_bitSet );
                EXPECT_EQ( m_generics.m_sharedPtr->m_data, obj->m_sharedPtr->m_data );
                EXPECT_EQ( m_generics.m_intrusivePtr->m_data, obj->m_intrusivePtr->m_data );
                EXPECT_EQ( m_generics.m_uniquePtr->m_data, obj->m_uniquePtr->m_data );
                delete obj;
            }


            void SaveObjects(ObjectStream* writer)
            {
                bool success = true;
                success = writer->WriteClass(&m_generics);
                EXPECT_TRUE(success);
            }

            void OnDone(ObjectStream::Handle handle, bool success, bool* done)
            {
                (void)handle;
                EXPECT_TRUE(success);
                *done = true;
            }

            void TestSave(IO::GenericStream* stream, ObjectStream::StreamType format)
            {
                ObjectStream* objStream = ObjectStream::Create(stream, *m_context, format);
                SaveObjects(objStream);
                bool done = objStream->Finalize();
                EXPECT_TRUE(done);
            }

            void TestLoad(IO::GenericStream* stream)
            {
                int cbCount = 0;
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&GenericsTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
                ObjectStream::LoadBlocking(stream, *m_context, readyCB);
                EXPECT_EQ( 1, cbCount );
            }

            void run()
            {
                m_context.reset(aznew SerializeContext());

                Generics::Reflect(*m_context);
                MyClassBase1::Reflect(*m_context);
                MyClassBase2::Reflect(*m_context);
                MyClassBase3::Reflect(*m_context);
                MyClassMix::Reflect(*m_context);
                MyClassMix2::Reflect(*m_context);
                MyClassMix3::Reflect(*m_context);
                SmartPtrClass::Reflect(*m_context);

                m_generics.Set();
                TestFileIOBase fileIO;
                SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

                // XML
                {
                    AZ_TracePrintf("GenericsTest", "\nWriting XML...\n");
                    IO::StreamerStream stream("serializegenericstest.xml", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_XML);
                }
                {
                    AZ_TracePrintf("GenericsTest", "Loading XML...\n");
                    IO::StreamerStream stream("serializegenericstest.xml", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // JSON
                {
                    AZ_TracePrintf("GenericsTest", "\nWriting JSON...\n");
                    IO::StreamerStream stream("serializegenericstest.json", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_JSON);
                }
                {
                    AZ_TracePrintf("GenericsTest", "Loading JSON...\n");
                    IO::StreamerStream stream("serializegenericstest.json", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                // Binary
                {
                    AZ_TracePrintf("GenericsTest", "Writing Binary...\n");
                    IO::StreamerStream stream("serializegenericstest.bin", IO::OpenMode::ModeWrite);
                    TestSave(&stream, ObjectStream::ST_BINARY);
                }
                {
                    AZ_TracePrintf("GenericsTest", "Loading Binary...\n");
                    IO::StreamerStream stream("serializegenericstest.bin", IO::OpenMode::ModeRead);
                    TestLoad(&stream);
                }

                m_generics.Unset();
                m_context.reset();
            }
        };

        GenericsTest test;
        test.run();
    }

    /*
    * Deprecation
    */

    namespace Deprecation
    {
        struct DeprecatedClass
        {
            AZ_CLASS_ALLOCATOR(DeprecatedClass, AZ::SystemAllocator, 0);
            AZ_TYPE_INFO(DeprecatedClass, "{893CA46E-6D1A-4D27-94F7-09E26DE5AE4B}")

                DeprecatedClass()
                : m_data(0) {}

            int m_data;
        };

        struct DeprecationTestClass
        {
            AZ_CLASS_ALLOCATOR(DeprecationTestClass, AZ::SystemAllocator, 0);
            AZ_TYPE_INFO(DeprecationTestClass, "{54E27F53-EF3F-4436-9378-E9AF56A9FA4C}")

                DeprecationTestClass()
                : m_deprecatedPtr(nullptr)
                , m_oldClassData(0)
                , m_newClassData(0.f)
                , m_missingMember(0)
                , m_data(0)
            {}

            ~DeprecationTestClass() { Clear(); }

            void Clear()
            {
                if (m_deprecatedPtr)
                {
                    delete m_deprecatedPtr;
                    m_deprecatedPtr = nullptr;
                }
            }

            DeprecatedClass     m_deprecated;
            DeprecatedClass*    m_deprecatedPtr;
            int                 m_oldClassData;
            float               m_newClassData;
            int                 m_missingMember;
            int                 m_data;
        };

        struct SimpleBaseClass
        {
            AZ_CLASS_ALLOCATOR(SimpleBaseClass, AZ::SystemAllocator, 0);
            AZ_RTTI(SimpleBaseClass, "{829F6E24-AAEF-4C97-9003-0BC22CB64786}")

                SimpleBaseClass()
                : m_data(0.f) {}
            virtual ~SimpleBaseClass() {}

            float m_data;
        };

        struct SimpleDerivedClass1 : public SimpleBaseClass
        {
            AZ_CLASS_ALLOCATOR(SimpleDerivedClass1, AZ::SystemAllocator, 0);
            AZ_RTTI(SimpleDerivedClass1, "{78632262-C303-49BC-ABAD-88B088098311}", SimpleBaseClass)

                SimpleDerivedClass1() {}
        };

        struct SimpleDerivedClass2 : public SimpleBaseClass
        {
            AZ_CLASS_ALLOCATOR(SimpleDerivedClass2, AZ::SystemAllocator, 0);
            AZ_RTTI(SimpleDerivedClass2, "{4932DF7C-0482-4846-AAE5-BED7D03F9E02}", SimpleBaseClass)

                SimpleDerivedClass2() {}
        };

        struct OwnerClass
        {
            AZ_CLASS_ALLOCATOR(OwnerClass, AZ::SystemAllocator, 0);
            AZ_TYPE_INFO(OwnerClass, "{3F305C77-4BE1-49E6-9C51-9F1284F18CCE}");

            OwnerClass() {}
            SimpleBaseClass* m_pointer = nullptr;
        };
    }

    TEST_F(Serialization, DeprecationRulesTest)
    {
        using namespace Deprecation;
        class DeprecationTest
        {
        public:
            DeprecatedClass m_deprecated;
            DeprecationTestClass m_deprecationTestClass;

            void WriteDeprecated(ObjectStream* writer)
            {
                bool success = writer->WriteClass(&m_deprecated);
                EXPECT_TRUE(success);
            }

            void WriteDeprecationTestClass(ObjectStream* writer)
            {
                bool success = writer->WriteClass(&m_deprecationTestClass);
                EXPECT_TRUE(success);
            }

            void CheckDeprecated(void* classPtr, const Uuid& classId)
            {
                (void)classPtr;
                (void)classId;
                // We should never hit here since our class was deprecated
                EXPECT_TRUE(false);
            }

            void CheckMemberDeprecation(void* classPtr, const Uuid& classId)
            {
                (void)classId;
                DeprecationTestClass* obj = reinterpret_cast<DeprecationTestClass*>(classPtr);
                EXPECT_EQ( 0, obj->m_deprecated.m_data );
                EXPECT_EQ( NULL, obj->m_deprecatedPtr );
                EXPECT_EQ( 0, obj->m_oldClassData );
                EXPECT_EQ( 0.f, obj->m_newClassData );
                EXPECT_EQ( 0, obj->m_missingMember );
                EXPECT_EQ( m_deprecationTestClass.m_data, obj->m_data );
                delete obj;
            }

            void run()
            {
                m_deprecated.m_data = 10;
                m_deprecationTestClass.m_deprecated.m_data = 10;
                m_deprecationTestClass.m_deprecatedPtr = aznew DeprecatedClass;
                m_deprecationTestClass.m_oldClassData = 10;
                m_deprecationTestClass.m_missingMember = 10;
                m_deprecationTestClass.m_data = 10;

                // Test new version without conversion.
                //  -Member types without reflection should be silently dropped.
                //  -Members whose reflection data don't match should be silently dropped.
                //  -Members whose names don't match should be silently dropped.
                //  -The converted class itself should still be accepted.
                AZ_TracePrintf("SerializeDeprecationTest", "\nTesting dropped/deprecated members:\n");
                {
                    // Write original data
                    AZStd::vector<char> xmlBuffer;
                    AZStd::vector<char> jsonBuffer;
                    AZStd::vector<char> binaryBuffer;
                    {
                        SerializeContext sc;
                        sc.Class<DeprecatedClass>()
                            ->Field("m_data", &DeprecatedClass::m_data);
                        sc.Class<DeprecationTestClass>()
                            ->Field("m_deprecated", &DeprecationTestClass::m_deprecated)
                            ->Field("m_deprecatedPtr", &DeprecationTestClass::m_deprecatedPtr)
                            ->Field("m_oldClassData", &DeprecationTestClass::m_oldClassData)
                            ->Field("m_missingMember", &DeprecationTestClass::m_missingMember)
                            ->Field("m_data", &DeprecationTestClass::m_data);

                        // XML
                        IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBuffer);
                        AZ_TracePrintf("SerializeDeprecationTest", "Writing XML\n");
                        ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, sc, ObjectStream::ST_XML);
                        WriteDeprecationTestClass(xmlObjStream);
                        bool success = xmlObjStream->Finalize();
                        EXPECT_TRUE(success);

                        // JSON
                        IO::ByteContainerStream<AZStd::vector<char> > jsonStream(&jsonBuffer);
                        AZ_TracePrintf("SerializeDeprecationTest", "Writing JSON\n");
                        ObjectStream* jsonObjStream = ObjectStream::Create(&jsonStream, sc, ObjectStream::ST_JSON);
                        WriteDeprecationTestClass(jsonObjStream);
                        success = jsonObjStream->Finalize();
                        EXPECT_TRUE(success);

                        // Binary
                        IO::ByteContainerStream<AZStd::vector<char> > binaryStream(&binaryBuffer);
                        AZ_TracePrintf("SerializeDeprecationTest", "Writing Binary\n");
                        ObjectStream* binaryObjStream = ObjectStream::Create(&binaryStream, sc, ObjectStream::ST_BINARY);
                        WriteDeprecationTestClass(binaryObjStream);
                        success = binaryObjStream->Finalize();
                        EXPECT_TRUE(success);
                    }

                    ObjectStream::ClassReadyCB readyCB(AZStd::bind(&DeprecationTest::CheckMemberDeprecation, this, AZStd::placeholders::_1, AZStd::placeholders::_2));
                    // Test deprecation with one member class not reflected at all
                    {
                        SerializeContext sc;
                        sc.Class<DeprecationTestClass>()
                            ->Version(2)
                            ->Field("m_deprecated", &DeprecationTestClass::m_deprecated)
                            ->Field("m_deprecatedPtr", &DeprecationTestClass::m_deprecatedPtr)
                            ->Field("m_oldClassData", &DeprecationTestClass::m_newClassData)
                            ->Field("m_data", &DeprecationTestClass::m_data);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with dropped class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStream(&xmlBuffer);
                        xmlStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&xmlStream, sc, readyCB);

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading JSON with dropped class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > jsonStream(&jsonBuffer);
                        jsonStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&jsonStream, sc, readyCB);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading Binary with dropped class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > binaryStream(&binaryBuffer);
                        binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&binaryStream, sc, readyCB);
                    }
                    // Test deprecation with one member class marked as deprecated
                    {
                        SerializeContext sc;
                        sc.ClassDeprecate("DeprecatedClass", "{893CA46E-6D1A-4D27-94F7-09E26DE5AE4B}");
                        sc.Class<DeprecationTestClass>()
                            ->Version(2)
                            ->Field("m_deprecated", &DeprecationTestClass::m_deprecated)
                            ->Field("m_deprecatedPtr", &DeprecationTestClass::m_deprecatedPtr)
                            ->Field("m_oldClassData", &DeprecationTestClass::m_newClassData)
                            ->Field("m_data", &DeprecationTestClass::m_data);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStream(&xmlBuffer);
                        xmlStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&xmlStream, sc, readyCB);

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading JSON with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > jsonStream(&jsonBuffer);
                        jsonStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&jsonStream, sc, readyCB);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading Binary with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > binaryStream(&binaryBuffer);
                        binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&binaryStream, sc, readyCB);
                    }

                    // Test deprecation with a converter to an entirely new type.
                    {
                        SerializeContext sc;

                        sc.Class<DeprecationTestClass>()
                            ->Version(2)
                            ->Field("m_deprecated", &DeprecationTestClass::m_deprecated)
                            ->Field("m_deprecatedPtr", &DeprecationTestClass::m_deprecatedPtr)
                            ->Field("m_oldClassData", &DeprecationTestClass::m_newClassData)
                            ->Field("m_data", &DeprecationTestClass::m_data);

                        AZ::SerializeContext::VersionConverter converter = [](AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement) -> bool
                        {
                            return classElement.Convert<DeprecationTestClass>(context);
                        };

                        sc.ClassDeprecate("DeprecatedClass", "{893CA46E-6D1A-4D27-94F7-09E26DE5AE4B}", converter);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStream(&xmlBuffer);
                        xmlStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&xmlStream, sc, readyCB);

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading JSON with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > jsonStream(&jsonBuffer);
                        jsonStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&jsonStream, sc, readyCB);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading Binary with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > binaryStream(&binaryBuffer);
                        binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&binaryStream, sc, readyCB);
                    }

                    // Test a converter that completely swaps uuid.
                    // This test should FAIL, because the uuid cannot be swapped in non-deprecation cases.
                    {
                        SerializeContext sc;

                        sc.Class<SimpleBaseClass>()
                            ->Version(1)
                            ->Field("m_data", &SimpleBaseClass::m_data);

                        AZ::SerializeContext::VersionConverter converter = [](AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement) -> bool
                        {
                            return classElement.Convert<SimpleBaseClass>(context);
                        };

                        sc.Class<DeprecationTestClass>()
                            ->Version(3, converter)
                            ->Field("m_oldClassData", &DeprecationTestClass::m_newClassData)
                            ->Field("m_data", &DeprecationTestClass::m_data);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStream(&xmlBuffer);
                        xmlStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);

                        // This should fail!
                        AZ_TEST_START_ASSERTTEST;
                        ObjectStream::LoadBlocking(&xmlStream, sc, readyCB);
                        AZ_TEST_STOP_ASSERTTEST(1);
                    }

                    // Test a deprecated class at the root level.
                    {
                        SerializeContext sc;

                        SimpleDerivedClass1 simpleDerivedClass1;
                        sc.Class<SimpleBaseClass>()
                            ->Version(1)
                            ->Field("m_data", &SimpleBaseClass::m_data);
                        sc.Class<SimpleDerivedClass1, SimpleBaseClass>()
                            ->Version(1);
                        sc.Class<SimpleDerivedClass2, SimpleBaseClass>()
                            ->Version(1);

                        AZStd::vector<char> xmlBufferRootTest;
                        AZStd::vector<char> jsonBufferRootTest;
                        AZStd::vector<char> binaryBufferRootTest;

                        {
                            // XML
                            IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBufferRootTest);
                            AZ_TracePrintf("SerializeDeprecationTest", "Writing XML\n");
                            ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, sc, ObjectStream::ST_XML);
                            xmlObjStream->WriteClass(&simpleDerivedClass1);
                            bool success = xmlObjStream->Finalize();
                            EXPECT_TRUE(success);

                            // JSON
                            IO::ByteContainerStream<AZStd::vector<char> > jsonStream(&jsonBufferRootTest);
                            AZ_TracePrintf("SerializeDeprecationTest", "Writing JSON\n");
                            ObjectStream* jsonObjStream = ObjectStream::Create(&jsonStream, sc, ObjectStream::ST_JSON);
                            jsonObjStream->WriteClass(&simpleDerivedClass1);
                            success = jsonObjStream->Finalize();
                            EXPECT_TRUE(success);

                            // Binary
                            IO::ByteContainerStream<AZStd::vector<char> > binaryStream(&binaryBufferRootTest);
                            AZ_TracePrintf("SerializeDeprecationTest", "Writing Binary\n");
                            ObjectStream* binaryObjStream = ObjectStream::Create(&binaryStream, sc, ObjectStream::ST_BINARY);
                            binaryObjStream->WriteClass(&simpleDerivedClass1);
                            success = binaryObjStream->Finalize();
                            EXPECT_TRUE(success);
                        }

                        sc.EnableRemoveReflection();
                        sc.Class<SimpleDerivedClass1>();
                        sc.DisableRemoveReflection();

                        AZ::SerializeContext::VersionConverter converter = [](AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement) -> bool
                        {
                            return classElement.Convert<SimpleDerivedClass2>(context);
                        };

                        sc.ClassDeprecate("SimpleDerivedClass1", "{78632262-C303-49BC-ABAD-88B088098311}", converter);

                        auto cb = [](void* classPtr, const Uuid& classId, SerializeContext* /*context*/) -> void
                        {
                            EXPECT_EQ( AzTypeInfo<SimpleDerivedClass2>::Uuid(), classId );
                            delete static_cast<SimpleDerivedClass2*>(classPtr);
                        };

                        ObjectStream::ClassReadyCB readyCBTest(cb);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStreamUuidTest(&xmlBufferRootTest);
                        xmlStreamUuidTest.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        EXPECT_TRUE(ObjectStream::LoadBlocking(&xmlStreamUuidTest, sc, readyCBTest));

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading JSON with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > jsonStream(&jsonBufferRootTest);
                        jsonStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&jsonStream, sc, readyCBTest);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading Binary with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > binaryStream(&binaryBufferRootTest);
                        binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&binaryStream, sc, readyCBTest);
                    }

                    // Test a converter that swaps uuid to a castable/compatible type in a deprecation converter.
                    {
                        SimpleDerivedClass1 simpleDerivedClass1;
                        OwnerClass ownerClass;
                        ownerClass.m_pointer = &simpleDerivedClass1;

                        SerializeContext sc;

                        sc.Class<SimpleBaseClass>()
                            ->Version(1)
                            ->Field("m_data", &SimpleBaseClass::m_data);
                        sc.Class<SimpleDerivedClass1, SimpleBaseClass>()
                            ->Version(1);
                        sc.Class<SimpleDerivedClass2, SimpleBaseClass>()
                            ->Version(1);
                        sc.Class<OwnerClass>()
                            ->Version(1)
                            ->Field("Pointer", &OwnerClass::m_pointer);

                        AZStd::vector<char> xmlBufferUuidTest;
                        AZStd::vector<char> jsonBufferUuidTest;
                        AZStd::vector<char> binaryBufferUuidTest;

                        {
                            // XML
                            IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBufferUuidTest);
                            AZ_TracePrintf("SerializeDeprecationTest", "Writing XML\n");
                            ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, sc, ObjectStream::ST_XML);
                            xmlObjStream->WriteClass(&ownerClass);
                            bool success = xmlObjStream->Finalize();
                            EXPECT_TRUE(success);

                            // JSON
                            IO::ByteContainerStream<AZStd::vector<char> > jsonStream(&jsonBufferUuidTest);
                            AZ_TracePrintf("SerializeDeprecationTest", "Writing JSON\n");
                            ObjectStream* jsonObjStream = ObjectStream::Create(&jsonStream, sc, ObjectStream::ST_JSON);
                            jsonObjStream->WriteClass(&ownerClass);
                            success = jsonObjStream->Finalize();
                            EXPECT_TRUE(success);

                            // Binary
                            IO::ByteContainerStream<AZStd::vector<char> > binaryStream(&binaryBufferUuidTest);
                            AZ_TracePrintf("SerializeDeprecationTest", "Writing Binary\n");
                            ObjectStream* binaryObjStream = ObjectStream::Create(&binaryStream, sc, ObjectStream::ST_BINARY);
                            binaryObjStream->WriteClass(&ownerClass);
                            success = binaryObjStream->Finalize();
                            EXPECT_TRUE(success);
                        }

                        sc.EnableRemoveReflection();
                        sc.Class<OwnerClass>();
                        sc.DisableRemoveReflection();

                        AZ::SerializeContext::VersionConverter converter = [](AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement) -> bool
                        {
                            const int idx = classElement.FindElement(AZ_CRC("Pointer", 0x320468a8));
                            classElement.GetSubElement(idx).Convert<SimpleDerivedClass2>(context);
                            return true;
                        };

                        sc.Class<OwnerClass>()
                            ->Version(2, converter)
                            ->Field("Pointer", &OwnerClass::m_pointer);

                        auto cb = [](void* classPtr, const Uuid& classId, SerializeContext* /*context*/) -> void
                        {
                            EXPECT_EQ( AzTypeInfo<OwnerClass>::Uuid(), classId );
                            EXPECT_TRUE(static_cast<OwnerClass*>(classPtr)->m_pointer);
                            EXPECT_EQ( AzTypeInfo<SimpleDerivedClass2>::Uuid(), static_cast<OwnerClass*>(classPtr)->m_pointer->RTTI_GetType() );
                            delete static_cast<OwnerClass*>(classPtr)->m_pointer;
                            delete static_cast<OwnerClass*>(classPtr);
                        };

                        ObjectStream::ClassReadyCB readyCBTest(cb);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStreamUuidTest(&xmlBufferUuidTest);
                        xmlStreamUuidTest.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        EXPECT_TRUE(ObjectStream::LoadBlocking(&xmlStreamUuidTest, sc, readyCBTest));

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading JSON with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > jsonStream(&jsonBufferUuidTest);
                        jsonStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&jsonStream, sc, readyCBTest);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading Binary with deprecated class\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > binaryStream(&binaryBufferUuidTest);
                        binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&binaryStream, sc, readyCBTest);
                    }
                }

                // Test root objects of deprecated classes.
                //  -Classes reflected as deprecated should be silently dropped.
                AZ_TracePrintf("SerializeDeprecationTest", "Testing deprecated root objects:\n");
                {
                    AZStd::vector<char> xmlBuffer;
                    AZStd::vector<char> jsonBuffer;
                    AZStd::vector<char> binaryBuffer;
                    // Write original data
                    {
                        SerializeContext sc;
                        sc.Class<DeprecatedClass>()
                            ->Field("m_data", &DeprecatedClass::m_data);

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Writing XML\n");
                        IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBuffer);
                        ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, sc, ObjectStream::ST_XML);
                        WriteDeprecated(xmlObjStream);
                        bool success = xmlObjStream->Finalize();
                        EXPECT_TRUE(success);

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Writing JSON\n");
                        IO::ByteContainerStream<AZStd::vector<char> > jsonStream(&jsonBuffer);
                        ObjectStream* jsonObjStream = ObjectStream::Create(&jsonStream, sc, ObjectStream::ST_JSON);
                        WriteDeprecated(jsonObjStream);
                        success = jsonObjStream->Finalize();
                        EXPECT_TRUE(success);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Writing Binary\n");
                        IO::ByteContainerStream<AZStd::vector<char> > binaryStream(&binaryBuffer);
                        ObjectStream* binaryObjStream = ObjectStream::Create(&binaryStream, sc, ObjectStream::ST_BINARY);
                        WriteDeprecated(binaryObjStream);
                        success = binaryObjStream->Finalize();
                        EXPECT_TRUE(success);
                    }
                    // Test deprecation
                    {
                        SerializeContext sc;
                        sc.ClassDeprecate("DeprecatedClass", "{893CA46E-6D1A-4D27-94F7-09E26DE5AE4B}");

                        ObjectStream::ClassReadyCB readyCB(AZStd::bind(&DeprecationTest::CheckDeprecated, this, AZStd::placeholders::_1, AZStd::placeholders::_2));

                        // XML
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading XML with deprecated root object\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > xmlStream(&xmlBuffer);
                        xmlStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&xmlStream, sc, readyCB);

                        // JSON
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading JSON with deprecated root object\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > jsonStream(&jsonBuffer);
                        jsonStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&jsonStream, sc, readyCB);

                        // Binary
                        AZ_TracePrintf("SerializeDeprecationTest", "Loading Binary with deprecated root object\n");
                        IO::ByteContainerStream<const AZStd::vector<char> > binaryStream(&binaryBuffer);
                        binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                        ObjectStream::LoadBlocking(&binaryStream, sc, readyCB);
                    }
                }

                m_deprecationTestClass.Clear();
            }
        };

        DeprecationTest test;
        test.run();
    }

    /*
    * Test complicated conversion
    */
    namespace Conversion
    {
        struct TestObj
        {
            AZ_TYPE_INFO(TestObj, "{6AE2EE4A-1DB8-41B7-B909-296A10CEF4EA}");
            TestObj() = default;
            Generics        m_dataOld;
            GenericsNew     m_dataNew;
        private:
            TestObj(const TestObj&) = default;
        };
    }

    TEST_F(Serialization, ConversionTest)
    {
        using namespace Conversion;
        class ConversionTest
        {
            TestObj m_testObj;

        public:
            void OnLoadedClassReady(void* classPtr, const Uuid& classId, int* callCount)
            {
                GenericsNew* obj = NULL;
                switch (*callCount)
                {
                case 0:
                    EXPECT_EQ( SerializeTypeInfo<TestObj>::GetUuid(), classId );
                    obj = &reinterpret_cast<TestObj*>(classPtr)->m_dataNew;
                    break;
                case 1:
                    EXPECT_EQ( SerializeTypeInfo<GenericsNew>::GetUuid(), classId );
                    obj = reinterpret_cast<GenericsNew*>(classPtr);
                    break;
                }
                EXPECT_TRUE(obj);
                EXPECT_EQ(m_testObj.m_dataNew.m_newInt, obj->m_newInt);
                EXPECT_EQ(m_testObj.m_dataNew.m_string, obj->m_string);
                EXPECT_EQ(m_testObj.m_dataNew.m_vectorInt2.size(), obj->m_vectorInt2.size());
                for (size_t i = 0; i < obj->m_vectorInt2.size(); ++i)
                {
                    EXPECT_EQ(m_testObj.m_dataNew.m_vectorInt2[i], obj->m_vectorInt2[i]);
                }
                EXPECT_EQ(m_testObj.m_dataNew.m_listIntList.size(), obj->m_listIntList.size());
                for (AZStd::list<AZStd::list<int> >::const_iterator iList1 = obj->m_listIntList.begin(), iList2 = m_testObj.m_dataNew.m_listIntList.begin(); iList1 != obj->m_listIntList.end(); ++iList1, ++iList2)
                {
                    const AZStd::list<int>& list1 = *iList1;
                    const AZStd::list<int>& list2 = *iList2;
                    EXPECT_EQ(list1.size(), list2.size());
                    for (AZStd::list<int>::const_iterator i1 = list1.begin(), i2 = list2.begin(); i1 != list1.end(); ++i1, ++i2)
                    {
                        EXPECT_EQ(*i1, *i2);
                    }
                }
                EXPECT_EQ(3, obj->m_umapPolymorphic.size());
                EXPECT_EQ(SerializeTypeInfo<MyClassMixNew>::GetUuid(), obj->m_umapPolymorphic[1]->RTTI_GetType());
                EXPECT_TRUE( *static_cast<MyClassMixNew*>(m_testObj.m_dataNew.m_umapPolymorphic[1]) == *static_cast<MyClassMixNew*>(obj->m_umapPolymorphic[1]) );
                EXPECT_EQ( SerializeTypeInfo<MyClassMix2>::GetUuid(), obj->m_umapPolymorphic[2]->RTTI_GetType() );
                EXPECT_TRUE( *static_cast<MyClassMix2*>(m_testObj.m_dataNew.m_umapPolymorphic[2]) == *static_cast<MyClassMix2*>(obj->m_umapPolymorphic[2]) );
                EXPECT_EQ( SerializeTypeInfo<MyClassMix3>::GetUuid(), obj->m_umapPolymorphic[3]->RTTI_GetType() );
                EXPECT_TRUE( *static_cast<MyClassMix3*>(m_testObj.m_dataNew.m_umapPolymorphic[3]) == *static_cast<MyClassMix3*>(obj->m_umapPolymorphic[3]) );

                if (*callCount == 0)
                {
                    delete reinterpret_cast<TestObj*>(classPtr);
                }
                else
                {
                    delete reinterpret_cast<GenericsNew*>(classPtr);
                }
                (*callCount)++;
            }

            void SaveObjects(ObjectStream* writer)
            {
                bool success = true;
                success = writer->WriteClass(&m_testObj);           // for testing non-root conversions
                EXPECT_TRUE(success);
                success = writer->WriteClass(&m_testObj.m_dataOld); // for testing root conversions
                EXPECT_TRUE(success);
            }

            void OnDone(ObjectStream::Handle handle, bool success, bool* done)
            {
                (void)handle;
                EXPECT_TRUE(success);
                *done = true;
            }

            void run()
            {
                m_testObj.m_dataOld.Set();
                m_testObj.m_dataNew.Set();
                TestFileIOBase fileIO;
                SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

                // write old version out
                {
                    SerializeContext context;
                    context.Class<TestObj>()->
                        Field("data", &TestObj::m_dataOld);
                    Generics::Reflect(context);
                    MyClassBase1::Reflect(context);
                    MyClassBase2::Reflect(context);
                    MyClassBase3::Reflect(context);
                    MyClassMix::Reflect(context);
                    MyClassMix2::Reflect(context);
                    MyClassMix3::Reflect(context);
                    SmartPtrClass::Reflect(context);


                    {
                        AZ_TracePrintf("SerializeConversionTest", "\nWriting XML...\n");
                        IO::StreamerStream stream("serializeconversiontest.xml", IO::OpenMode::ModeWrite);
                        ObjectStream* objStream = ObjectStream::Create(&stream, context, ObjectStream::ST_XML);
                        SaveObjects(objStream);
                        bool done = objStream->Finalize();
                        EXPECT_TRUE(done);
                    }
                    {
                        AZ_TracePrintf("SerializeConversionTest", "\nWriting JSON...\n");
                        IO::StreamerStream stream("serializeconversiontest.json", IO::OpenMode::ModeWrite);
                        ObjectStream* objStream = ObjectStream::Create(&stream, context, ObjectStream::ST_JSON);
                        SaveObjects(objStream);
                        bool done = objStream->Finalize();
                        EXPECT_TRUE(done);
                    }
                    {
                        AZ_TracePrintf("SerializeConversionTest", "Writing Binary...\n");
                        IO::StreamerStream stream("serializeconversiontest.bin", IO::OpenMode::ModeWrite);
                        ObjectStream* objStream = ObjectStream::Create(&stream, context, ObjectStream::ST_BINARY);
                        SaveObjects(objStream);
                        bool done = objStream->Finalize();
                        EXPECT_TRUE(done);
                    }
                }

                // load and convert
                {
                    SerializeContext context;
                    context.Class<TestObj>()->
                        Field("data", &TestObj::m_dataNew);
                    GenericsNew::Reflect(context);
                    MyClassBase1::Reflect(context);
                    MyClassBase2::Reflect(context);
                    MyClassBase3::Reflect(context);
                    MyClassMixNew::Reflect(context);
                    MyClassMix2::Reflect(context);
                    MyClassMix3::Reflect(context);
                    SmartPtrClass::Reflect(context);

                    {
                        AZ_TracePrintf("SerializeConversionTest", "Loading XML...\n");
                        IO::StreamerStream stream("serializeconversiontest.xml", IO::OpenMode::ModeRead);
                        int cbCount = 0;
                        ObjectStream::ClassReadyCB readyCB(AZStd::bind(&ConversionTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
                        ObjectStream::LoadBlocking(&stream, context, readyCB);
                        EXPECT_EQ( 2, cbCount );
                    }
                    {
                        AZ_TracePrintf("SerializeConversionTest", "Loading JSON...\n");
                        IO::StreamerStream stream("serializeconversiontest.json", IO::OpenMode::ModeRead);
                        int cbCount = 0;
                        ObjectStream::ClassReadyCB readyCB(AZStd::bind(&ConversionTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
                        ObjectStream::LoadBlocking(&stream, context, readyCB);
                        EXPECT_EQ( 2, cbCount );
                    }
                    {
                        AZ_TracePrintf("SerializeConversionTest", "Loading Binary...\n");
                        IO::StreamerStream stream("serializeconversiontest.bin", IO::OpenMode::ModeRead);
                        int cbCount = 0;
                        ObjectStream::ClassReadyCB readyCB(AZStd::bind(&ConversionTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
                        ObjectStream::LoadBlocking(&stream, context, readyCB);
                        EXPECT_EQ( 2, cbCount );
                    }
                }

                m_testObj.m_dataOld.Unset();
                m_testObj.m_dataNew.Unset();
            }
        };

        ConversionTest test;
        test.run();
    }

    /*
    * Data Overlay Test
    */
    namespace DataOverlay
    {
        struct DataOverlayTestStruct
        {
            AZ_TYPE_INFO(DataOverlayTestStruct, "{AD843B4D-0D08-4CE0-99F9-7E4E1EAD5984}")
                DataOverlayTestStruct()
                : m_int(0)
                , m_ptr(nullptr) {}

            int                     m_int;
            AZStd::vector<int>      m_intVector;
            DataOverlayTestStruct*  m_ptr;
        };
    }

    TEST_F(Serialization, DataOverlayTest)
    {
        using namespace DataOverlay;
        class DataOverlayTest
        {
            class DataOverlayProviderExample
                : public DataOverlayProviderBus::Handler
            {
            public:
                static DataOverlayProviderId    GetProviderId() { return AZ_CRC("DataOverlayProviderExample", 0x60dafdbd); }
                static u32                      GetIntToken() { return AZ_CRC("int_data", 0xd74868f3); }
                static u32                      GetVectorToken() { return AZ_CRC("vector_data", 0x0aca20c0); }
                static u32                      GetPointerToken() { return AZ_CRC("pointer_data", 0xa46a746e); }

                DataOverlayProviderExample()
                {
                    m_ptrData.m_int = 5;
                    m_ptrData.m_intVector.push_back(1);
                    m_ptrData.m_ptr = nullptr;

                    m_data.m_int = 3;
                    m_data.m_intVector.push_back(10);
                    m_data.m_intVector.push_back(20);
                    m_data.m_intVector.push_back(30);
                    m_data.m_ptr = &m_ptrData;
                }

                virtual void FillOverlayData(DataOverlayTarget* dest, const DataOverlayToken& dataToken)
                {
                    if (*reinterpret_cast<const u32*>(dataToken.m_dataUri.data()) == GetIntToken())
                    {
                        dest->SetData(m_data.m_int);
                    }
                    else if (*reinterpret_cast<const u32*>(dataToken.m_dataUri.data()) == GetVectorToken())
                    {
                        dest->SetData(m_data.m_intVector);
                    }
                    else if (*reinterpret_cast<const u32*>(dataToken.m_dataUri.data()) == GetPointerToken())
                    {
                        dest->SetData(*m_data.m_ptr);
                    }
                }

                DataOverlayTestStruct   m_data;
                DataOverlayTestStruct   m_ptrData;
            };

            class DataOverlayInstanceEnumeratorExample
                : public DataOverlayInstanceBus::Handler
            {
            public:
                enum InstanceType
                {
                    Type_Int,
                    Type_Vector,
                    Type_Pointer,
                };

                DataOverlayInstanceEnumeratorExample(InstanceType type)
                    : m_type(type) {}

                virtual DataOverlayInfo GetOverlayInfo()
                {
                    DataOverlayInfo info;
                    info.m_providerId = DataOverlayProviderExample::GetProviderId();
                    u32 token = m_type == Type_Int ? DataOverlayProviderExample::GetIntToken() : m_type == Type_Vector ? DataOverlayProviderExample::GetVectorToken() : DataOverlayProviderExample::GetPointerToken();
                    info.m_dataToken.m_dataUri.insert(info.m_dataToken.m_dataUri.end(), reinterpret_cast<u8*>(&token), reinterpret_cast<u8*>(&token) + sizeof(u32));
                    return info;
                }

                InstanceType m_type;
            };

            void CheckOverlay(const DataOverlayTestStruct* controlData, void* classPtr, const Uuid& uuid)
            {
                EXPECT_EQ( SerializeTypeInfo<DataOverlayTestStruct>::GetUuid(), uuid );
                DataOverlayTestStruct* newData = reinterpret_cast<DataOverlayTestStruct*>(classPtr);
                EXPECT_EQ( controlData->m_int, newData->m_int );
                EXPECT_EQ( controlData->m_intVector, newData->m_intVector );
                EXPECT_TRUE(newData->m_ptr != nullptr);
                EXPECT_TRUE(newData->m_ptr != controlData->m_ptr);
                EXPECT_EQ( controlData->m_ptr->m_int, newData->m_ptr->m_int );
                EXPECT_EQ( controlData->m_ptr->m_intVector, newData->m_ptr->m_intVector );
                EXPECT_EQ( controlData->m_ptr->m_ptr, newData->m_ptr->m_ptr );
                delete newData->m_ptr;
                delete newData;
            }

        public:
            void run()
            {
                SerializeContext serializeContext;

                // We must expose the class for serialization first.
                serializeContext.Class<DataOverlayTestStruct>()
                    ->Field("int", &DataOverlayTestStruct::m_int)
                    ->Field("intVector", &DataOverlayTestStruct::m_intVector)
                    ->Field("pointer", &DataOverlayTestStruct::m_ptr);

                DataOverlayTestStruct testData;
                testData.m_ptr = &testData;

                DataOverlayInstanceEnumeratorExample intOverlayEnumerator(DataOverlayInstanceEnumeratorExample::Type_Int);
                intOverlayEnumerator.BusConnect(DataOverlayInstanceId(&testData.m_int, SerializeTypeInfo<int>::GetUuid()));
                DataOverlayInstanceEnumeratorExample vectorOverlayEnumerator(DataOverlayInstanceEnumeratorExample::Type_Vector);
                vectorOverlayEnumerator.BusConnect(DataOverlayInstanceId(&testData.m_intVector, SerializeGenericTypeInfo<AZStd::vector<int> >::GetClassTypeId()));
                DataOverlayInstanceEnumeratorExample pointerOverlayEnumerator(DataOverlayInstanceEnumeratorExample::Type_Pointer);
                pointerOverlayEnumerator.BusConnect(DataOverlayInstanceId(&testData.m_ptr, SerializeTypeInfo<DataOverlayTestStruct>::GetUuid()));

                // XML
                AZStd::vector<char> xmlBuffer;
                IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBuffer);
                ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, serializeContext, ObjectStream::ST_XML);
                xmlObjStream->WriteClass(&testData);
                xmlObjStream->Finalize();

                AZ::IO::SystemFile tmpOut;
                tmpOut.Open("DataOverlayTest.xml", AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY);
                tmpOut.Write(xmlStream.GetData()->data(), xmlStream.GetLength());
                tmpOut.Close();

                DataOverlayProviderExample overlayProvider;
                overlayProvider.BusConnect(DataOverlayProviderExample::GetProviderId());
                xmlStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
                ObjectStream::ClassReadyCB readyCB(AZStd::bind(&DataOverlayTest::CheckOverlay, this, &overlayProvider.m_data, AZStd::placeholders::_1, AZStd::placeholders::_2));
                ObjectStream::LoadBlocking(&xmlStream, serializeContext, readyCB);
            }
        };

        DataOverlayTest test;
        test.run();
    }

    /*
    * DynamicSerializableFieldTest
    */
    TEST_F(Serialization, DynamicSerializableFieldTest)
    {
        SerializeContext serializeContext;

        // We must expose the class for serialization first.
        MyClassBase1::Reflect(serializeContext);
        MyClassBase2::Reflect(serializeContext);
        MyClassBase3::Reflect(serializeContext);
        MyClassMix::Reflect(serializeContext);

        MyClassMix obj;
        obj.Set(5); // Initialize with some value

        DynamicSerializableField testData;
        testData.m_data = &obj;
        testData.m_typeId = SerializeTypeInfo<MyClassMix>::GetUuid();

        // XML
        AZStd::vector<char> xmlBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBuffer);
        ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, serializeContext, ObjectStream::ST_XML);
        xmlObjStream->WriteClass(&testData);
        xmlObjStream->Finalize();

        AZ::IO::SystemFile tmpOut;
        tmpOut.Open("DynamicSerializableFieldTest.xml", AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY);
        tmpOut.Write(xmlStream.GetData()->data(), xmlStream.GetLength());
        tmpOut.Close();

        xmlStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        auto verifyLoad = [&testData](void* classPtr, const Uuid& uuid, SerializeContext* sc) -> void
        {
            EXPECT_EQ( SerializeTypeInfo<DynamicSerializableField>::GetUuid(), uuid );
            DynamicSerializableField* newData = reinterpret_cast<DynamicSerializableField*>(classPtr);
            EXPECT_EQ( SerializeTypeInfo<MyClassMix>::GetUuid(), newData->m_typeId );
            EXPECT_TRUE(newData->m_data != nullptr);
            EXPECT_TRUE( *reinterpret_cast<MyClassMix*>(testData.m_data) == *reinterpret_cast<MyClassMix*>(newData->m_data) );
            newData->DestroyData(sc);
            delete newData;
        };

        ObjectStream::ClassReadyCB readyCB(verifyLoad);
        ObjectStream::LoadBlocking(&xmlStream, serializeContext, readyCB);
    }

    /*
    * DynamicSerializableFieldTest
    */
    class SerializeDynamicSerializableFieldTest
        : public AllocatorsFixture
    {
        void VerifyLoad(const DynamicSerializableField* controlData, void* classPtr, const Uuid& uuid, SerializeContext* sc)
        {
            AZ_TEST_ASSERT(uuid == SerializeTypeInfo<DynamicSerializableField>::GetUuid());
            DynamicSerializableField* newData = reinterpret_cast<DynamicSerializableField*>(classPtr);
            AZ_TEST_ASSERT(newData->m_typeId == SerializeTypeInfo<MyClassMix>::GetUuid());
            AZ_TEST_ASSERT(newData->m_data != nullptr);
            AZ_TEST_ASSERT(*reinterpret_cast<MyClassMix*>(newData->m_data) == *reinterpret_cast<MyClassMix*>(controlData->m_data));
            newData->DestroyData(sc);
            delete newData;
        }

    public:
        void run()
        {
            SerializeContext serializeContext;

            // We must expose the class for serialization first.
            MyClassBase1::Reflect(serializeContext);
            MyClassBase2::Reflect(serializeContext);
            MyClassBase3::Reflect(serializeContext);
            MyClassMix::Reflect(serializeContext);

            MyClassMix obj;
            obj.Set(5); // Initialize with some value

            DynamicSerializableField testData;
            testData.m_data = &obj;
            testData.m_typeId = SerializeTypeInfo<MyClassMix>::GetUuid();

            // XML
            AZStd::vector<char> xmlBuffer;
            IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&xmlBuffer);
            ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, serializeContext, ObjectStream::ST_XML);
            xmlObjStream->WriteClass(&testData);
            xmlObjStream->Finalize();

            AZ::IO::SystemFile tmpOut;
            tmpOut.Open("DynamicSerializableFieldTest.xml", AZ::IO::SystemFile::SF_OPEN_CREATE | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY);
            tmpOut.Write(xmlStream.GetData()->data(), xmlStream.GetLength());
            tmpOut.Close();

            xmlStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            ObjectStream::ClassReadyCB readyCB(AZStd::bind(&SerializeDynamicSerializableFieldTest::VerifyLoad, this, &testData, AZStd::placeholders::_1, AZStd::placeholders::_2, AZStd::placeholders::_3));
            ObjectStream::LoadBlocking(&xmlStream, serializeContext, readyCB);
        }
    };

    TEST_F(SerializeDynamicSerializableFieldTest, NonSerializableTypeTest)
    {
        SerializeContext serializeContext;
        DynamicSerializableField testData;
        EXPECT_EQ(nullptr, testData.m_data);
        EXPECT_EQ(AZ::Uuid::CreateNull(), testData.m_typeId);

        // Write DynamicSerializableField to stream
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream<decltype(buffer)> stream(&buffer);
        {
            ObjectStream* binObjectStream = ObjectStream::Create(&stream, serializeContext, ObjectStream::ST_BINARY);
            binObjectStream->WriteClass(&testData);
            binObjectStream->Finalize();
        }

        // Load DynamicSerializableField from stream
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        {
            DynamicSerializableField loadData;
            loadData.m_typeId = AZ::Uuid::CreateRandom();

            // TypeId should be serialized in as a null Uuid and the m_data field should remain unchanged
            AZ::Utils::LoadObjectFromStreamInPlace(stream, loadData, &serializeContext);
            EXPECT_EQ(AZ::Uuid::CreateNull(), loadData.m_typeId);
        }
    }

    /*
    * CloneTest
    */
    namespace Clone
    {
        struct RefCounted
        {
            AZ_CLASS_ALLOCATOR(RefCounted, AZ::SystemAllocator, 0);
            AZ_TYPE_INFO(RefCounted, "{ca52979d-b926-461a-b1f5-66bbfdb80639}");

            RefCounted()
                : m_refCount(0)
                , m_data(0)
            {}

            RefCounted(int data)
                : m_refCount(0)
                , m_data(data)
            {}

            static void Reflect(SerializeContext& serializeContext)
            {
                serializeContext.Class<RefCounted>()
                    ->Field("Data", &RefCounted::m_data);
            }

            //////////////////////////////////////////////////////////////////////////
            // For intrusive pointers
            void add_ref() { m_refCount++; }
            void release()
            {
                --m_refCount;
                if (m_refCount == 0)
                {
                    delete this;
                }
            }
            int m_refCount;
            //////////////////////////////////////////////////////////////////////////
            int m_data;
        };

        struct Clonable
        {
            Clonable()
                : m_emptyInitText("Some init text!")
            {
            }

            AZ_TYPE_INFO(Clonable, "{3E463CC3-CC78-4F21-9BE8-0B0AA10E8E26}");

            static void Reflect(SerializeContext& serializeContext)
            {
                serializeContext.Class<Clonable>()
                    ->Field("m_int", &Clonable::m_int)
                    ->Field("m_emptyInitText", &Clonable::m_emptyInitText)
                    ->Field("m_map", &Clonable::m_map)
                    ->Field("m_fieldValues", &Clonable::m_fieldValues)
                    ->Field("m_smartArray", &Clonable::m_smartArray);
            }

            int m_int;
            AZStd::string m_emptyInitText;
            AZStd::unordered_map<int, int> m_map;
            AZStd::vector<DynamicSerializableField> m_fieldValues;
            AZStd::array<AZStd::intrusive_ptr<RefCounted>, 10> m_smartArray;
        };
    }
    TEST_F(Serialization, CloneTest)
    {
        using namespace Clone;
        
        // We must expose the class for serialization first.
        MyClassBase1::Reflect((*m_serializeContext));
        MyClassBase2::Reflect((*m_serializeContext));
        MyClassBase3::Reflect((*m_serializeContext));
        MyClassMix::Reflect((*m_serializeContext));
        RefCounted::Reflect((*m_serializeContext));
        Clonable::Reflect((*m_serializeContext));

        Clonable testObj;
        testObj.m_int = 100;
        testObj.m_emptyInitText = ""; // set to empty to make sure we write zero values
        testObj.m_map.insert(AZStd::make_pair(1, 2));
        testObj.m_smartArray[0] = aznew RefCounted(101);
        testObj.m_smartArray[1] = aznew RefCounted(201);
        testObj.m_smartArray[2] = aznew RefCounted(301);

        MyClassMix val1;
        val1.Set(5);    // Initialize with some value
        DynamicSerializableField valField1;
        valField1.m_data = &val1;
        valField1.m_typeId = SerializeTypeInfo<MyClassMix>::GetUuid();
        testObj.m_fieldValues.push_back(valField1);

        Clonable* cloneObj = m_serializeContext->CloneObject(&testObj);
        EXPECT_TRUE(cloneObj);
        EXPECT_EQ( testObj.m_int, cloneObj->m_int );
        EXPECT_EQ( testObj.m_emptyInitText, cloneObj->m_emptyInitText );
        EXPECT_EQ( testObj.m_map, cloneObj->m_map );
        EXPECT_EQ( testObj.m_fieldValues.size(), cloneObj->m_fieldValues.size() );
        EXPECT_TRUE(cloneObj->m_fieldValues[0].m_data);
        EXPECT_TRUE(cloneObj->m_fieldValues[0].m_data != testObj.m_fieldValues[0].m_data);
        EXPECT_TRUE( *reinterpret_cast<MyClassMix*>(testObj.m_fieldValues[0].m_data) == *reinterpret_cast<MyClassMix*>(cloneObj->m_fieldValues[0].m_data) );
        delete reinterpret_cast<MyClassMix*>(cloneObj->m_fieldValues[0].m_data);
        AZ_TEST_ASSERT(cloneObj->m_smartArray[0] && cloneObj->m_smartArray[0]->m_data == 101);
        AZ_TEST_ASSERT(cloneObj->m_smartArray[1] && cloneObj->m_smartArray[1]->m_data == 201);
        AZ_TEST_ASSERT(cloneObj->m_smartArray[2] && cloneObj->m_smartArray[2]->m_data == 301);
        delete cloneObj;
    }

    /*
    * Error Testing
    */
    namespace Error
    {
        struct UnregisteredClass
        {
            AZ_TYPE_INFO(UnregisteredClass, "{6558CEBC-D764-4E50-BAA0-025BF55FAD15}")
        };

        struct UnregisteredRttiClass
        {
            AZ_RTTI(UnregisteredRttiClass, "{F948E16B-975D-4F23-911E-2AA5758D8B21}");
            virtual ~UnregisteredRttiClass() {}
        };

        struct ChildOfUnregisteredClass
            : public UnregisteredClass
        {
            AZ_TYPE_INFO(ChildOfUnregisteredClass, "{C72CB2C9-7E9A-41EB-8219-5D13B6445AFC}")

                ChildOfUnregisteredClass() {}
            ChildOfUnregisteredClass(SerializeContext& sc)
            {
                sc.Class<ChildOfUnregisteredClass, UnregisteredClass>();
            }
        };
        struct ChildOfUnregisteredRttiClass
            : public UnregisteredRttiClass
        {
            AZ_RTTI(ChildOfUnregisteredRttiClass, "{E58F6984-4C0A-4D1B-B034-FDEF711AB711}", UnregisteredRttiClass);
            ChildOfUnregisteredRttiClass() {}
            ChildOfUnregisteredRttiClass(SerializeContext& sc)
            {
                sc.Class<ChildOfUnregisteredRttiClass, UnregisteredRttiClass>();
            }
        };
        struct UnserializableMembers
        {
            AZ_TYPE_INFO(UnserializableMembers, "{36F0C52A-5CAC-4060-982C-FC9A86D1393A}")

                UnserializableMembers() {}
            UnserializableMembers(SerializeContext& sc)
                : m_childOfUnregisteredRttiBase(sc)
                , m_childOfUnregisteredBase(&m_childOfUnregisteredRttiBase)
                , m_basePtrToGenericChild(&m_unserializableGeneric)
            {
                m_vectorUnregisteredClass.push_back();
                m_vectorUnregisteredRttiClass.push_back();
                m_vectorUnregisteredRttiBase.push_back(&m_unregisteredRttiMember);
                m_vectorGenericChildPtr.push_back(&m_unserializableGeneric);
                sc.Class<UnserializableMembers>()->
                    Field("unregisteredMember", &UnserializableMembers::m_unregisteredMember)->
                    Field("unregisteredRttiMember", &UnserializableMembers::m_unregisteredRttiMember)->
                    Field("childOfUnregisteredBase", &UnserializableMembers::m_childOfUnregisteredBase)->
                    Field("basePtrToGenericChild", &UnserializableMembers::m_basePtrToGenericChild)->
                    Field("vectorUnregisteredClass", &UnserializableMembers::m_vectorUnregisteredClass)->
                    Field("vectorUnregisteredRttiClass", &UnserializableMembers::m_vectorUnregisteredRttiClass)->
                    Field("vectorUnregisteredRttiBase", &UnserializableMembers::m_vectorUnregisteredRttiBase)->
                    Field("vectorGenericChildPtr", &UnserializableMembers::m_vectorGenericChildPtr);
            }

            ChildOfUnregisteredRttiClass            m_childOfUnregisteredRttiBase;
            GenericChild                            m_unserializableGeneric;

            UnregisteredClass                       m_unregisteredMember;
            UnregisteredRttiClass                   m_unregisteredRttiMember;
            UnregisteredRttiClass*                  m_childOfUnregisteredBase;
            GenericClass*                           m_basePtrToGenericChild;
            AZStd::vector<UnregisteredClass>        m_vectorUnregisteredClass;
            AZStd::vector<UnregisteredRttiClass>    m_vectorUnregisteredRttiClass;
            AZStd::vector<UnregisteredRttiClass*>   m_vectorUnregisteredRttiBase;
            AZStd::vector<GenericClass*>            m_vectorGenericChildPtr;
        };
    }

    TEST_F(Serialization, ErrorTest)
    {
        using namespace Error;
        class ErrorTest
        {
        public:
            void SaveObjects(ObjectStream* writer, SerializeContext* sc)
            {
                static int i = 0;
                bool success;

                // test saving root unregistered class
                if (i == 0)
                {
                    UnregisteredClass unregisteredClass;
                    AZ_TEST_START_ASSERTTEST;
                    success = writer->WriteClass(&unregisteredClass);
                    EXPECT_TRUE(!success);
                    AZ_TEST_STOP_ASSERTTEST(1);
                }

                // test saving root unregistered Rtti class
                else if (i == 1)
                {
                    UnregisteredRttiClass unregisteredRttiClass;
                    AZ_TEST_START_ASSERTTEST;
                    success = writer->WriteClass(&unregisteredRttiClass);
                    EXPECT_TRUE(!success);
                    AZ_TEST_STOP_ASSERTTEST(1);
                }

                // test saving root generic class
                else if (i == 2)
                {
                    GenericClass genericClass;
                    AZ_TEST_START_ASSERTTEST;
                    success = writer->WriteClass(&genericClass);
                    EXPECT_TRUE(!success);
                    AZ_TEST_STOP_ASSERTTEST(1);
                }

                // test saving as pointer to unregistered base class with no rtti
                else if (i == 3)
                {
                    ChildOfUnregisteredClass childOfUnregisteredClass(*sc);
                    AZ_TEST_START_ASSERTTEST;
                    success = writer->WriteClass(static_cast<UnregisteredClass*>(&childOfUnregisteredClass));
                    EXPECT_TRUE(!success);
                    AZ_TEST_STOP_ASSERTTEST(1);
                }

                // test saving unserializable members
                else if (i == 4)
                {
                    UnserializableMembers badMembers(*sc);
                    AZ_TEST_START_ASSERTTEST;
                    success = writer->WriteClass(&badMembers);
                    EXPECT_TRUE(!success);
                    AZ_TEST_STOP_ASSERTTEST(7); // 1 failure for each member
                }
                i++;
            }

            void run()
            {
                AZStd::vector<char> buffer;
                IO::ByteContainerStream<AZStd::vector<char> > stream(&buffer);

                // test saving root unregistered class
                {
                    SerializeContext sc;
                    ObjectStream* objStream = ObjectStream::Create(&stream, sc, ObjectStream::ST_XML);
                    SaveObjects(objStream, &sc);
                    objStream->Finalize();
                }
                // test saving root unregistered Rtti class
                {
                    SerializeContext sc;
                    ObjectStream* objStream = ObjectStream::Create(&stream, sc, ObjectStream::ST_XML);
                    SaveObjects(objStream, &sc);
                    objStream->Finalize();
                }
                // test saving root generic class
                {
                    SerializeContext sc;
                    ObjectStream* objStream = ObjectStream::Create(&stream, sc, ObjectStream::ST_XML);
                    SaveObjects(objStream, &sc);
                    objStream->Finalize();
                }
                // test saving as pointer to unregistered base class with no rtti
                {
                    SerializeContext sc;
                    ObjectStream* objStream = ObjectStream::Create(&stream, sc, ObjectStream::ST_XML);
                    SaveObjects(objStream, &sc);
                    objStream->Finalize();
                }
                // test saving unserializable members
                // errors covered:
                //  - unregistered type with no rtti
                //  - unregistered type with rtti
                //  - pointer to unregistered base with rtti
                //  - base pointer pointing to a generic child
                //  - vector of unregistered types
                //  - vector of unregistered types with rtti
                //  - vector of pointers to unregistered base with rtti
                //  - vector of base pointers pointing to generic child
                {
                    SerializeContext sc;
                    ObjectStream* objStream = ObjectStream::Create(&stream, sc, ObjectStream::ST_XML);
                    SaveObjects(objStream, &sc);
                    objStream->Finalize();
                }
            }
        };

        ErrorTest test;
        test.run();
    }

    namespace EditTest
    {
        struct MyEditStruct
        {
            AZ_TYPE_INFO(MyEditStruct, "{89CCD760-A556-4EDE-98C0-33FD9DD556B9}")

                MyEditStruct()
                : m_data(11)
                , m_specialData(3) {}

            int     Foo(int m) { return 5 * m; }
            bool    IsShowSpecialData() const { return true; }
            int     GetDataOption(int option) { return option * 2; }

            int m_data;
            int m_specialData;
        };

        int MyEditGlobalFunc(int m)
        {
            return 4 * m;
        }

        class MyEditStruct2
        {
        public:
            AZ_TYPE_INFO(MyEditStruct2, "{FFD27958-9856-4CE2-AE13-18878DE5ECE0}");

            MyEditStruct m_myEditStruct;
        };

        class MyEditStruct3
        {
        public:
            enum EditEnum
            {
                ENUM_Test1 = 1,
                ENUM_Test2 = 2,
                ENUM_Test3 = -1,
                ENUM_Test4 = INT_MAX,
            };

            enum class EditEnumClass : AZ::u8
            {
                EEC_1,
                EEC_2,
                EEC_255 = 255,
            };

        public:
            AZ_TYPE_INFO(MyEditStruct3, "{11F859C7-7A15-49C8-8A38-783A1EFC0E06}");

            EditEnum m_enum;
            EditEnum m_enum2;
            EditEnumClass m_enumClass;
        };
    }
} // namespace UnitTest
AZ_TYPE_INFO_SPECIALIZE(UnitTest::EditTest::MyEditStruct3::EditEnum, "{4AF433C2-055E-4E34-921A-A7D16AB548CA}");
AZ_TYPE_INFO_SPECIALIZE(UnitTest::EditTest::MyEditStruct3::EditEnumClass, "{4FEC2F0B-A599-4FCD-836B-89E066791793}");


namespace UnitTest
{
    TEST_F(Serialization, EditContextTest)
    {
        using namespace EditTest;

        class EditContextTest
        {
        public:
            bool BeginSerializationElement(SerializeContext* sc, void* instance, const SerializeContext::ClassData* classData, const SerializeContext::ClassElement* classElement)
            {
                (void)instance;
                (void)classData;
                (void)classElement;

                if (classElement)
                {
                    // if we are a pointer, then we may be pointing to a derived type.
                    if (classElement->m_flags & SerializeContext::ClassElement::FLG_POINTER)
                    {
                        // if dataAddress is a pointer in this case, cast it's value to a void* (or const void*) and dereference to get to the actual class.
                        instance = *(void**)(instance);
                        if (instance && classElement->m_azRtti)
                        {
                            AZ::Uuid actualClassId = classElement->m_azRtti->GetActualUuid(instance);
                            if (actualClassId != classElement->m_typeId)
                            {
                                // we are pointing to derived type, adjust class data, uuid and pointer.
                                classData = sc->FindClassData(actualClassId);
                                if (classData)
                                {
                                    instance = classElement->m_azRtti->Cast(instance, classData->m_azRtti->GetTypeId());
                                }
                            }
                        }
                    }
                }

                if (strcmp(classData->m_name, "MyEditStruct") == 0)
                {
                    EXPECT_TRUE(classData->m_editData != NULL);
                    EXPECT_EQ( 0, strcmp(classData->m_editData->m_name, "MyEditStruct") );
                    EXPECT_EQ( 0, strcmp(classData->m_editData->m_description, "My edit struct class used for ...") );
                    EXPECT_EQ( 2, classData->m_editData->m_elements.size() );
                    EXPECT_EQ( 0, strcmp(classData->m_editData->m_elements.front().m_description, "Special data group") );
                    EXPECT_EQ( 1, classData->m_editData->m_elements.front().m_attributes.size() );
                    EXPECT_TRUE(classData->m_editData->m_elements.front().m_attributes[0].first == AZ_CRC("Callback", 0x79f97426) );
                }
                else if (classElement && classElement->m_editData && strcmp(classElement->m_editData->m_description, "Type") == 0)
                {
                    EXPECT_EQ( 2, classElement->m_editData->m_attributes.size() );
                    // Number of options attribute
                    EXPECT_EQ(classElement->m_editData->m_attributes[0].first, AZ_CRC("NumOptions", 0x90274abc));
                    Edit::AttributeData<int>* intData = azrtti_cast<Edit::AttributeData<int>*>(classElement->m_editData->m_attributes[0].second);
                    EXPECT_TRUE(intData != NULL);
                    EXPECT_EQ( 3, intData->Get(instance) );
                    // Get options attribute
                    EXPECT_EQ( classElement->m_editData->m_attributes[1].first, AZ_CRC("Options", 0xd035fa87));
                    Edit::AttributeFunction<int(int)>* funcData = azrtti_cast<Edit::AttributeFunction<int(int)>*>(classElement->m_editData->m_attributes[1].second);
                    EXPECT_TRUE(funcData != NULL);
                    EXPECT_EQ( 20, funcData->Invoke(instance, 10) );
                }
                return true;
            }

            bool EndSerializationElement()
            {
                return true;
            }

            void run()
            {
                SerializeContext serializeContext;

                // We must expose the class for serialization first.
                serializeContext.Class<MyEditStruct>()->
                    Field("data", &MyEditStruct::m_data);

                serializeContext.Class<MyEditStruct2>()->
                    Field("m_myEditStruct", &MyEditStruct2::m_myEditStruct);

                serializeContext.Class<MyEditStruct3>()->
                    Field("m_enum", &MyEditStruct3::m_enum)->
                    Field("m_enum2", &MyEditStruct3::m_enum2)->
                    Field("m_enumClass", &MyEditStruct3::m_enumClass);

                // create edit context
                serializeContext.CreateEditContext();
                EditContext* editContext = serializeContext.GetEditContext();

                // reflect the class for editing
                editContext->Class<MyEditStruct>("MyEditStruct", "My edit struct class used for ...")->
                    ClassElement(AZ::Edit::ClassElements::Group, "Special data group")->
                    Attribute("Callback", &MyEditStruct::IsShowSpecialData)->
                    DataElement("ComboSelector", &MyEditStruct::m_data, "Name", "Type")->
                    Attribute("NumOptions", 3)->
                    Attribute("Options", &MyEditStruct::GetDataOption);

                // reflect class by using the element edit reflection as name/descriptor
                editContext->Class<MyEditStruct2>("MyEditStruct2", "My edit struct class 2 with redirected data element...")->
                    DataElement("ComboSelector", &MyEditStruct2::m_myEditStruct)->
                    Attribute("NumOptions", 3);

                // enumerate elements and verify the class reflection..
                MyEditStruct myObj;
                serializeContext.EnumerateObject(&myObj,
                    AZStd::bind(&EditContextTest::BeginSerializationElement, this, &serializeContext, AZStd::placeholders::_1, AZStd::placeholders::_2, AZStd::placeholders::_3),
                    AZStd::bind(&EditContextTest::EndSerializationElement, this),
                    SerializeContext::ENUM_ACCESS_FOR_READ);

                editContext->Enum<MyEditStruct3::EditEnum>("EditEnum", "The enum for testing the Enum<>() call")->
                    Value("Test1", MyEditStruct3::EditEnum::ENUM_Test1)->
                    Value("Test2", MyEditStruct3::EditEnum::ENUM_Test2)->
                    Value("Test3", MyEditStruct3::EditEnum::ENUM_Test3)->
                    Value("Test4", MyEditStruct3::EditEnum::ENUM_Test4);

                editContext->Enum<MyEditStruct3::EditEnumClass>("EditEnumClass", "The enum class for testing the Enum<>() call")->
                    Value("One", MyEditStruct3::EditEnumClass::EEC_1)->
                    Value("Two", MyEditStruct3::EditEnumClass::EEC_2)->
                    Value("TwoFiftyFive", MyEditStruct3::EditEnumClass::EEC_255);

                AZ_TEST_START_ASSERTTEST;
                editContext->Class<MyEditStruct3>("MyEditStruct3", "Used to test enum global reflection")->
                    DataElement("Enum", &MyEditStruct3::m_enum)-> // safe
                    DataElement("Enum2", &MyEditStruct3::m_enum2)-> // safe
                    EnumAttribute(MyEditStruct3::EditEnum::ENUM_Test1, "THIS SHOULD CAUSE AN ERROR")->
                    Attribute(AZ::Edit::Attributes::EnumValues, AZStd::vector<AZ::Edit::EnumConstant<MyEditStruct3::EditEnum>> {
                        AZ::Edit::EnumConstant<MyEditStruct3::EditEnum>(MyEditStruct3::EditEnum::ENUM_Test1, "EnumTest1 - ERROR"),
                        AZ::Edit::EnumConstant<MyEditStruct3::EditEnum>(MyEditStruct3::EditEnum::ENUM_Test2, "EnumTest2 - ERROR"),
                        AZ::Edit::EnumConstant<MyEditStruct3::EditEnum>(MyEditStruct3::EditEnum::ENUM_Test3, "EnumTest3 - ERROR"),
                        AZ::Edit::EnumConstant<MyEditStruct3::EditEnum>(MyEditStruct3::EditEnum::ENUM_Test4, "EnumTest4 - ERROR"),
                    })->
                    ElementAttribute(AZ::Edit::InternalAttributes::EnumValue, AZStd::make_pair(MyEditStruct3::EditEnum::ENUM_Test1, "THIS SHOULD ALSO CAUSE AN ERROR"));
                AZ_TEST_STOP_ASSERTTEST(0);
            }
        };

        EditContextTest test;
        test.run();
    }
    

    /**
    * Test cases when (usually with DLLs) we have to unload parts of the reflected context
    */
    TEST_F(Serialization, UnregisterTest)
    {
        using namespace EditTest;

        auto reflectClasses = [](SerializeContext* context)
        {
            context->Class<MyEditStruct>()->
                Field("data", &MyEditStruct::m_data);
        };

        SerializeContext serializeContext;

        // Register class
        reflectClasses(&serializeContext);

        // enumerate elements and verify the class reflection..
        MyEditStruct myObj;
        EXPECT_TRUE(serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid()) != nullptr);
        EXPECT_EQ( 0, strcmp(serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid())->m_name, "MyEditStruct") );

        // remove the class from the context
        serializeContext.EnableRemoveReflection();
        reflectClasses(&serializeContext);
        serializeContext.DisableRemoveReflection();
        EXPECT_EQ( nullptr, serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid()) );

        // Register class again
        reflectClasses(&serializeContext);
        EXPECT_EQ( nullptr, serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid())->m_editData ); // no edit data yet

                                                                                                                    // create edit context
        serializeContext.CreateEditContext();
        EditContext* editContext = serializeContext.GetEditContext();

        // reflect the class for editing
        editContext->Class<MyEditStruct>("MyEditStruct", "My edit struct class used for ...")->
            ClassElement(AZ::Edit::ClassElements::Group, "Special data group")->
            Attribute("Callback", &MyEditStruct::IsShowSpecialData)->
            DataElement("ComboSelector", &MyEditStruct::m_data, "Name", "Type")->
            Attribute("NumOptions", 3)->
            Attribute("Options", &MyEditStruct::GetDataOption);

        EXPECT_TRUE(serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid())->m_editData != nullptr);
        EXPECT_EQ( 0, strcmp(serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid())->m_editData->m_name, "MyEditStruct") );

        // remove the class from the context
        serializeContext.EnableRemoveReflection();
        reflectClasses(&serializeContext);
        serializeContext.DisableRemoveReflection();
        EXPECT_EQ( nullptr, serializeContext.FindClassData(AzTypeInfo<MyEditStruct>::Uuid()) );
    }

    namespace LargeData
    {
        class InnerPayload
        {
        public:

            AZ_CLASS_ALLOCATOR(InnerPayload, AZ::SystemAllocator, 0);
            AZ_RTTI(InnerPayload, "{3423157C-C6C5-4914-BB5C-B656439B8D3D}");

            AZStd::string m_textData;

            InnerPayload()
            {
                m_textData = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi sed pellentesque nibh. Mauris ac ipsum ante. Mauris dignissim vehicula dui, et mollis mauris tincidunt non. Aliquam sodales diam ante, in vestibulum nibh ultricies et. Pellentesque accumsan porta vulputate. Donec vel fringilla sem. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nam eu erat eu est mollis condimentum ut eget metus."
                    "Sed nec felis enim.Ut auctor arcu nec tristique volutpat.Nulla viverra vulputate nibh et fringilla.Curabitur sagittis eu libero ullamcorper porta.Ut ac nisi vitae massa luctus tristique.Donec scelerisque, odio at pharetra consectetur, nunc urna porta ligula, tincidunt auctor orci purus non nisi.Nulla at risus at lacus vestibulum varius vitae ac tellus.Etiam ut sem commodo justo tempor congue vel id odio.Duis erat sem, condimentum a neque id, bibendum consectetur ligula.In eget massa lectus.Interdum et malesuada fames ac ante ipsum primis in faucibus.Ut ornare lectus at sem condimentum gravida vel ut est."
                    "Curabitur nisl metus, euismod in enim eu, pulvinar ullamcorper lorem.Morbi et adipiscing nisi.Aliquam id dapibus sapien.Aliquam facilisis, lacus porta interdum mattis, erat metus tempus ligula, nec cursus augue tellus ut urna.Sed sagittis arcu vel magna consequat, eget eleifend quam tincidunt.Maecenas non ornare nisi, placerat ornare orci.Proin auctor in nunc eu ultrices.Vivamus interdum imperdiet sapien nec cursus."
                    "Etiam et iaculis tortor.Nam lacus risus, rutrum a mollis quis, accumsan quis risus.Mauris ac fringilla lectus.Cras posuere massa ultricies libero fermentum, in convallis metus porttitor.Duis hendrerit gravida neque at ultricies.Vestibulum semper congue gravida.Etiam vel mi quis risus ornare convallis nec et elit.Praesent a mollis erat, in eleifend libero.Fusce porttitor malesuada velit, nec pharetra justo rutrum sit amet.Ut vel egestas lacus, sit amet posuere nunc."
                    "Maecenas in eleifend risus.Integer volutpat sodales massa vitae consequat.Cras urna turpis, laoreet sed ante sit amet, dictum commodo sem.Vivamus porta, neque vel blandit dictum, enim metus molestie nisl, a consectetur libero odio eu magna.Maecenas nisi nibh, dignissim et nisi eget, adipiscing auctor ligula.Sed in nisl libero.Maecenas aliquam urna orci, ac ultrices massa sollicitudin vitae.Donec ullamcorper suscipit viverra.Praesent dolor ipsum, tincidunt eu quam sit amet, aliquam cursus orci.Praesent elementum est sit amet lectus imperdiet interdum.Pellentesque et sem et nulla tempus cursus.Sed enim dolor, viverra eu mauris id, ornare congue urna."
                    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi sed pellentesque nibh. Mauris ac ipsum ante. Mauris dignissim vehicula dui, et mollis mauris tincidunt non. Aliquam sodales diam ante, in vestibulum nibh ultricies et. Pellentesque accumsan porta vulputate. Donec vel fringilla sem. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nam eu erat eu est mollis condimentum ut eget metus."
                    "Sed nec felis enim.Ut auctor arcu nec tristique volutpat.Nulla viverra vulputate nibh et fringilla.Curabitur sagittis eu libero ullamcorper porta.Ut ac nisi vitae massa luctus tristique.Donec scelerisque, odio at pharetra consectetur, nunc urna porta ligula, tincidunt auctor orci purus non nisi.Nulla at risus at lacus vestibulum varius vitae ac tellus.Etiam ut sem commodo justo tempor congue vel id odio.Duis erat sem, condimentum a neque id, bibendum consectetur ligula.In eget massa lectus.Interdum et malesuada fames ac ante ipsum primis in faucibus.Ut ornare lectus at sem condimentum gravida vel ut est."
                    "Curabitur nisl metus, euismod in enim eu, pulvinar ullamcorper lorem.Morbi et adipiscing nisi.Aliquam id dapibus sapien.Aliquam facilisis, lacus porta interdum mattis, erat metus tempus ligula, nec cursus augue tellus ut urna.Sed sagittis arcu vel magna consequat, eget eleifend quam tincidunt.Maecenas non ornare nisi, placerat ornare orci.Proin auctor in nunc eu ultrices.Vivamus interdum imperdiet sapien nec cursus."
                    "Etiam et iaculis tortor.Nam lacus risus, rutrum a mollis quis, accumsan quis risus.Mauris ac fringilla lectus.Cras posuere massa ultricies libero fermentum, in convallis metus porttitor.Duis hendrerit gravida neque at ultricies.Vestibulum semper congue gravida.Etiam vel mi quis risus ornare convallis nec et elit.Praesent a mollis erat, in eleifend libero.Fusce porttitor malesuada velit, nec pharetra justo rutrum sit amet.Ut vel egestas lacus, sit amet posuere nunc."
                    "Maecenas in eleifend risus.Integer volutpat sodales massa vitae consequat.Cras urna turpis, laoreet sed ante sit amet, dictum commodo sem.Vivamus porta, neque vel blandit dictum, enim metus molestie nisl, a consectetur libero odio eu magna.Maecenas nisi nibh, dignissim et nisi eget, adipiscing auctor ligula.Sed in nisl libero.Maecenas aliquam urna orci, ac ultrices massa sollicitudin vitae.Donec ullamcorper suscipit viverra.Praesent dolor ipsum, tincidunt eu quam sit amet, aliquam cursus orci.Praesent elementum est sit amet lectus imperdiet interdum.Pellentesque et sem et nulla tempus cursus.Sed enim dolor, viverra eu mauris id, ornare congue urna."
                    ;
            }

            virtual ~InnerPayload()
            {}

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<InnerPayload>()->
                    Version(5, &InnerPayload::ConvertOldVersions)->
                    Field("m_textData", &InnerPayload::m_textData)
                    ;
            }

            static bool ConvertOldVersions(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
            {
                (void)context;
                (void)classElement;
                return false;
            }
        };

        class Payload
        {
        public:

            AZ_CLASS_ALLOCATOR(Payload, AZ::SystemAllocator, 0);
            AZ_RTTI(Payload, "{7A14FC65-44FB-4956-B5BC-4CFCBF36E1AE}");

            AZStd::string m_textData;
            AZStd::string m_newTextData;

            InnerPayload m_payload;

            SerializeContext m_context;

            Payload()
            {
                m_textData = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi sed pellentesque nibh. Mauris ac ipsum ante. Mauris dignissim vehicula dui, et mollis mauris tincidunt non. Aliquam sodales diam ante, in vestibulum nibh ultricies et. Pellentesque accumsan porta vulputate. Donec vel fringilla sem. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nam eu erat eu est mollis condimentum ut eget metus."
                    "Sed nec felis enim.Ut auctor arcu nec tristique volutpat.Nulla viverra vulputate nibh et fringilla.Curabitur sagittis eu libero ullamcorper porta.Ut ac nisi vitae massa luctus tristique.Donec scelerisque, odio at pharetra consectetur, nunc urna porta ligula, tincidunt auctor orci purus non nisi.Nulla at risus at lacus vestibulum varius vitae ac tellus.Etiam ut sem commodo justo tempor congue vel id odio.Duis erat sem, condimentum a neque id, bibendum consectetur ligula.In eget massa lectus.Interdum et malesuada fames ac ante ipsum primis in faucibus.Ut ornare lectus at sem condimentum gravida vel ut est."
                    "Curabitur nisl metus, euismod in enim eu, pulvinar ullamcorper lorem.Morbi et adipiscing nisi.Aliquam id dapibus sapien.Aliquam facilisis, lacus porta interdum mattis, erat metus tempus ligula, nec cursus augue tellus ut urna.Sed sagittis arcu vel magna consequat, eget eleifend quam tincidunt.Maecenas non ornare nisi, placerat ornare orci.Proin auctor in nunc eu ultrices.Vivamus interdum imperdiet sapien nec cursus."
                    "Etiam et iaculis tortor.Nam lacus risus, rutrum a mollis quis, accumsan quis risus.Mauris ac fringilla lectus.Cras posuere massa ultricies libero fermentum, in convallis metus porttitor.Duis hendrerit gravida neque at ultricies.Vestibulum semper congue gravida.Etiam vel mi quis risus ornare convallis nec et elit.Praesent a mollis erat, in eleifend libero.Fusce porttitor malesuada velit, nec pharetra justo rutrum sit amet.Ut vel egestas lacus, sit amet posuere nunc."
                    "Maecenas in eleifend risus.Integer volutpat sodales massa vitae consequat.Cras urna turpis, laoreet sed ante sit amet, dictum commodo sem.Vivamus porta, neque vel blandit dictum, enim metus molestie nisl, a consectetur libero odio eu magna.Maecenas nisi nibh, dignissim et nisi eget, adipiscing auctor ligula.Sed in nisl libero.Maecenas aliquam urna orci, ac ultrices massa sollicitudin vitae.Donec ullamcorper suscipit viverra.Praesent dolor ipsum, tincidunt eu quam sit amet, aliquam cursus orci.Praesent elementum est sit amet lectus imperdiet interdum.Pellentesque et sem et nulla tempus cursus.Sed enim dolor, viverra eu mauris id, ornare congue urna."
                    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi sed pellentesque nibh. Mauris ac ipsum ante. Mauris dignissim vehicula dui, et mollis mauris tincidunt non. Aliquam sodales diam ante, in vestibulum nibh ultricies et. Pellentesque accumsan porta vulputate. Donec vel fringilla sem. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nam eu erat eu est mollis condimentum ut eget metus."
                    "Sed nec felis enim.Ut auctor arcu nec tristique volutpat.Nulla viverra vulputate nibh et fringilla.Curabitur sagittis eu libero ullamcorper porta.Ut ac nisi vitae massa luctus tristique.Donec scelerisque, odio at pharetra consectetur, nunc urna porta ligula, tincidunt auctor orci purus non nisi.Nulla at risus at lacus vestibulum varius vitae ac tellus.Etiam ut sem commodo justo tempor congue vel id odio.Duis erat sem, condimentum a neque id, bibendum consectetur ligula.In eget massa lectus.Interdum et malesuada fames ac ante ipsum primis in faucibus.Ut ornare lectus at sem condimentum gravida vel ut est."
                    "Curabitur nisl metus, euismod in enim eu, pulvinar ullamcorper lorem.Morbi et adipiscing nisi.Aliquam id dapibus sapien.Aliquam facilisis, lacus porta interdum mattis, erat metus tempus ligula, nec cursus augue tellus ut urna.Sed sagittis arcu vel magna consequat, eget eleifend quam tincidunt.Maecenas non ornare nisi, placerat ornare orci.Proin auctor in nunc eu ultrices.Vivamus interdum imperdiet sapien nec cursus."
                    "Etiam et iaculis tortor.Nam lacus risus, rutrum a mollis quis, accumsan quis risus.Mauris ac fringilla lectus.Cras posuere massa ultricies libero fermentum, in convallis metus porttitor.Duis hendrerit gravida neque at ultricies.Vestibulum semper congue gravida.Etiam vel mi quis risus ornare convallis nec et elit.Praesent a mollis erat, in eleifend libero.Fusce porttitor malesuada velit, nec pharetra justo rutrum sit amet.Ut vel egestas lacus, sit amet posuere nunc."
                    "Maecenas in eleifend risus.Integer volutpat sodales massa vitae consequat.Cras urna turpis, laoreet sed ante sit amet, dictum commodo sem.Vivamus porta, neque vel blandit dictum, enim metus molestie nisl, a consectetur libero odio eu magna.Maecenas nisi nibh, dignissim et nisi eget, adipiscing auctor ligula.Sed in nisl libero.Maecenas aliquam urna orci, ac ultrices massa sollicitudin vitae.Donec ullamcorper suscipit viverra.Praesent dolor ipsum, tincidunt eu quam sit amet, aliquam cursus orci.Praesent elementum est sit amet lectus imperdiet interdum.Pellentesque et sem et nulla tempus cursus.Sed enim dolor, viverra eu mauris id, ornare congue urna."
                    ;
            }

            virtual ~Payload() {}

            static bool ConvertOldVersions(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
            {
                (void)classElement;
                (void)context;
                if (classElement.GetVersion() == 4)
                {
                    // convert from version 0
                    AZStd::string newData;
                    for (int i = 0; i < classElement.GetNumSubElements(); ++i)
                    {
                        AZ::SerializeContext::DataElementNode& elementNode = classElement.GetSubElement(i);

                        if (elementNode.GetName() == AZ_CRC("m_textData", 0xfc7870e5))
                        {
                            bool result = elementNode.GetData(newData);
                            EXPECT_TRUE(result);
                            classElement.RemoveElement(i);
                            break;
                        }
                    }

                    for (int i = 0; i < classElement.GetNumSubElements(); ++i)
                    {
                        AZ::SerializeContext::DataElementNode& elementNode = classElement.GetSubElement(i);
                        if (elementNode.GetName() == AZ_CRC("m_newTextData", 0x3feafc3d))
                        {
                            elementNode.SetData(context, newData);
                            break;
                        }
                    }
                    return true;
                }

                return false; // just discard unknown versions
            }

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<Payload>()->
                    Version(5, &Payload::ConvertOldVersions)->
                    Field("m_textData", &Payload::m_textData)->
                    Field("m_newTextData", &Payload::m_newTextData)->
                    Field("m_payload", &Payload::m_payload)
                    ;
            }

            void SaveObjects(ObjectStream* writer)
            {
                bool success = true;
                success = writer->WriteClass(this);
                EXPECT_TRUE(success);
            }

            void TestSave(IO::GenericStream* stream, ObjectStream::StreamType format)
            {
                ObjectStream* objStream = ObjectStream::Create(stream, m_context, format);
                SaveObjects(objStream);
                bool done = objStream->Finalize();
                EXPECT_TRUE(done);
            }
        };
    }

    

    TEST_F(Serialization, LargeDataTest)
    {
        using namespace LargeData;

        SerializeContext serializeContext;

        InnerPayload::Reflect(serializeContext);
        Payload::Reflect(serializeContext);

        TestFileIOBase fileIO;
        SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

        bool clone = true;
        if (clone)
        {
            Payload testObj;
            Payload* payload = serializeContext.CloneObject(&testObj);
            delete payload;
        }

        bool write = true;
        if (write)
        {
            Payload testObj;
            Payload* payload = serializeContext.CloneObject(&testObj);

            AZ_TracePrintf("LargeDataSerializationTest", "\nWriting as XML...\n");
            IO::StreamerStream stream("LargeDataSerializationTest.xml", IO::OpenMode::ModeWrite);
            ObjectStream* objStream = ObjectStream::Create(&stream, serializeContext, ObjectStream::ST_XML);
            objStream->WriteClass(payload);
            objStream->Finalize();

            delete payload;
        }

        bool writeJson = true;
        if (writeJson)
        {
            Payload testObj;
            Payload* payload = serializeContext.CloneObject(&testObj);

            AZ_TracePrintf("LargeDataSerializationTest", "\nWriting as JSON...\n");
            IO::StreamerStream stream("LargeDataSerializationTest.json", IO::OpenMode::ModeWrite);
            ObjectStream* objStream = ObjectStream::Create(&stream, serializeContext, ObjectStream::ST_JSON);
            objStream->WriteClass(payload);
            objStream->Finalize();

            delete payload;
        }


        bool writeBinary = true;
        if (writeBinary)
        {
            Payload testObj;
            Payload* payload = serializeContext.CloneObject(&testObj);

            AZ_TracePrintf("LargeDataSerializationTest", "\nWriting as Binary...\n");
            IO::StreamerStream stream("LargeDataSerializationTest.bin", IO::OpenMode::ModeWrite);
            ObjectStream* objStream = ObjectStream::Create(&stream, serializeContext, ObjectStream::ST_BINARY);
            objStream->WriteClass(payload);
            objStream->Finalize();

            delete payload;
        }

        auto classReady = [](void* classPtr, const Uuid& classId, SerializeContext*)
        {
            if (classId == SerializeTypeInfo<InnerPayload>::GetUuid())
            {
                InnerPayload* obj = reinterpret_cast<InnerPayload*>(classPtr);
                delete obj;
            }

            if (classId == SerializeTypeInfo<Payload>::GetUuid())
            {
                Payload* obj = reinterpret_cast<Payload*>(classPtr);
                delete obj;
            }
        };

        bool read = true;
        if (read)
        {
            bool done = false;
            auto onDone = [&done](ObjectStream::Handle, bool)
            {
                done = true;
            };
            ObjectStream::ClassReadyCB readyCB(classReady);
            ObjectStream::CompletionCB doneCB(onDone);
            {
                AZ_TracePrintf("LargeDataSerializationTest", "Loading as XML...\n");
                IO::StreamerStream stream2("LargeDataSerializationTest.xml", IO::OpenMode::ModeRead);
                ObjectStream::LoadBlocking(&stream2, serializeContext, readyCB);
            }
        }

        bool readJson = true;
        if (readJson)
        {
            bool done = false;

            auto onDone = [&done](ObjectStream::Handle, bool)
            {
                done = true;
            };
            ObjectStream::ClassReadyCB readyCB(classReady);
            ObjectStream::CompletionCB doneCB(onDone);
            {
                AZ_TracePrintf("LargeDataSerializationTest", "Loading as JSON...\n");
                IO::StreamerStream stream2("LargeDataSerializationTest.json", IO::OpenMode::ModeRead);
                ObjectStream::LoadBlocking(&stream2, serializeContext, readyCB);
            }
        }

        bool readBinary = true;
        if (readBinary)
        {
            bool done = false;
            auto onDone = [&done](ObjectStream::Handle, bool)
            {
                done = true;
            };
            ObjectStream::ClassReadyCB readyCB(classReady);
            ObjectStream::CompletionCB doneCB(onDone);
            {
                AZ_TracePrintf("LargeDataSerializationTest", "Loading as Binary...\n");
                IO::StreamerStream stream2("LargeDataSerializationTest.bin", IO::OpenMode::ModeRead);
                ObjectStream::LoadBlocking(&stream2, serializeContext, readyCB);
            }
        }
    }

    /*
    * Test serialization using FileUtil.
    * FileUtil interacts with the serialization context through the ComponentApplicationBus.
    */
    class SerializationFileUtil
        : public Serialization
    {
    public:
        void SetUp() override
        {
            Serialization::SetUp();
			m_prevFileIO = AZ::IO::FileIOBase::GetInstance();
            AZ::IO::FileIOBase::SetInstance(&m_fileIO);
            
            BaseRtti::Reflect(*m_serializeContext);
        }

        void TearDown() override
        {
			AZ::IO::FileIOBase::SetInstance(m_prevFileIO);
            Serialization::TearDown();
        }

        void TestFileUtilsStream(AZ::DataStream::StreamType streamType)
        {
            BaseRtti toSerialize;
            toSerialize.m_data = false;

            // Test Stream Write
            AZStd::vector<char> charBuffer;
            IO::ByteContainerStream<AZStd::vector<char> > charStream(&charBuffer);
            bool success = AZ::Utils::SaveObjectToStream(charStream, streamType, &toSerialize);
            EXPECT_TRUE(success);

            // Test Stream Read
            // Set the stream to the beginning so what was written can be read.
            charStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
            BaseRtti* deserialized = AZ::Utils::LoadObjectFromStream<BaseRtti>(charStream);
            EXPECT_TRUE(deserialized);
            EXPECT_EQ( toSerialize.m_data, deserialized->m_data );
            delete deserialized;
            deserialized = nullptr;

            // Test LoadObjectFromBuffer
            // First, save the object to a u8 buffer.
            AZStd::vector<u8> u8Buffer;
            IO::ByteContainerStream<AZStd::vector<u8> > u8Stream(&u8Buffer);
            success = AZ::Utils::SaveObjectToStream(u8Stream, streamType, &toSerialize);
            EXPECT_TRUE(success);
            u8Stream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
            deserialized = AZ::Utils::LoadObjectFromBuffer<BaseRtti>(&u8Buffer[0], u8Buffer.size());
            EXPECT_TRUE(deserialized);
            EXPECT_EQ( toSerialize.m_data, deserialized->m_data );
            delete deserialized;
            deserialized = nullptr;

            // Write to stream twice, read once.
            // Note that subsequent calls to write to stream will be ignored.
            // Note that many asserts here are commented out because the stream functionality was giving
            // unexpected results. There are rally stories on the backlog backlog (I e-mailed someone to put them on the backlog)
            // related to this.
            AZStd::vector<char> charBufferWriteTwice;
            IO::ByteContainerStream<AZStd::vector<char> > charStreamWriteTwice(&charBufferWriteTwice);
            success = AZ::Utils::SaveObjectToStream(charStreamWriteTwice, streamType, &toSerialize);
            EXPECT_TRUE(success);
            BaseRtti secondSerializedObject;
            secondSerializedObject.m_data = true;
            success = AZ::Utils::SaveObjectToStream(charStreamWriteTwice, streamType, &secondSerializedObject);
            // SaveObjectToStream currently returns success after attempting to save a second object.
            // This does not match up with the later behavior of loading from this stream.
            // Currently, saving twice returns a success on each save, and loading once returns the first object.
            // What should happen, is either the attempt to save onto the stream again should return false,
            // or the read should return the second object first.
            //EXPECT_TRUE(success);
            charStreamWriteTwice.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            deserialized = AZ::Utils::LoadObjectFromStream<BaseRtti>(charStreamWriteTwice);
            EXPECT_TRUE(deserialized);
            // Read the above text. This is here for whoever addresses these backlog items.
            //EXPECT_EQ( toSerialize.m_data, deserialized->m_data );
            //EXPECT_EQ( secondSerializedObject.m_data, deserialized->m_data );
            delete deserialized;
            deserialized = nullptr;
        }

        void TestFileUtilsFile(AZ::DataStream::StreamType streamType)
        {
            BaseRtti toSerialize;
            toSerialize.m_data = false;

            // Test save once, read once.
            AZStd::string filePath = GetTestFolderPath() + "FileUtilsTest";
            bool success = AZ::Utils::SaveObjectToFile(filePath, streamType, &toSerialize);
            EXPECT_TRUE(success);

            BaseRtti* deserialized = AZ::Utils::LoadObjectFromFile<BaseRtti>(filePath);
            EXPECT_TRUE(deserialized);
            EXPECT_EQ( toSerialize.m_data, deserialized->m_data );
            delete deserialized;
            deserialized = nullptr;

            // Test save twice, read once.
            // This is valid with files because saving a file again will overwrite it. Note that streams function differently.
            success = AZ::Utils::SaveObjectToFile(filePath, streamType, &toSerialize);
            EXPECT_TRUE(success);
            success = AZ::Utils::SaveObjectToFile(filePath, streamType, &toSerialize);
            EXPECT_TRUE(success);

            deserialized = AZ::Utils::LoadObjectFromFile<BaseRtti>(filePath);
            EXPECT_TRUE(deserialized);
            EXPECT_EQ( toSerialize.m_data, deserialized->m_data );
            delete deserialized;
            deserialized = nullptr;

            // Test reading from an invalid file. The system should return 'nullptr' when given a bad file path.
            AZ::IO::SystemFile::Delete(filePath.c_str());
            deserialized = AZ::Utils::LoadObjectFromFile<BaseRtti>(filePath);
            EXPECT_EQ( nullptr, deserialized );
        }

		TestFileIOBase m_fileIO;
		AZ::IO::FileIOBase* m_prevFileIO;
    };

    TEST_F(SerializationFileUtil, TestFileUtilsStream_XML)
    {
        TestFileUtilsStream(ObjectStream::ST_XML);
    }

    TEST_F(SerializationFileUtil, TestFileUtilsStream_Binary)
    {
        TestFileUtilsStream(ObjectStream::ST_BINARY);
    }

    TEST_F(SerializationFileUtil, DISABLED_TestFileUtilsFile_XML)
    {
        TestFileUtilsFile(ObjectStream::ST_XML);
    }

    TEST_F(SerializationFileUtil, DISABLED_TestFileUtilsFile_Binary)
    {
        TestFileUtilsFile(ObjectStream::ST_BINARY);
    }

    /**
    * Tests generating and applying patching to serialized structures.
    * \note There a lots special... \TODO add notes depending on the final solution
    */
    namespace Patching
    {
        // Object that we will store in container and patch in the complex case
        class ContainedObjectPersistentId
        {
        public:
            AZ_TYPE_INFO(ContainedObjectPersistentId, "{D0C4D19C-7EFF-4F93-A5F0-95F33FC855AA}")

                ContainedObjectPersistentId()
                : m_data(0)
                , m_persistentId(0)
            {}

            u64 GetPersistentId() const { return m_persistentId; }
            void SetPersistentId(u64 pesistentId) { m_persistentId = pesistentId; }

            int m_data;
            u64 m_persistentId; ///< Returns the persistent object ID

            static u64 GetPersistentIdWrapper(const void* instance)
            {
                return reinterpret_cast<const ContainedObjectPersistentId*>(instance)->GetPersistentId();
            }

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<ContainedObjectPersistentId>()->
                    PersistentId(&ContainedObjectPersistentId::GetPersistentIdWrapper)->
                    Field("m_data", &ContainedObjectPersistentId::m_data)->
                    Field("m_persistentId", &ContainedObjectPersistentId::m_persistentId);
            }
        };

        class ContainedObjectDerivedPersistentId
            : public ContainedObjectPersistentId
        {
        public:
            AZ_TYPE_INFO(ContainedObjectDerivedPersistentId, "{1c3ba36a-ceee-4118-89e7-807930bf2bec}");

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<ContainedObjectDerivedPersistentId, ContainedObjectPersistentId>();
            }
        };

        class ContainedObjectNoPersistentId
        {
        public:
            AZ_CLASS_ALLOCATOR(ContainedObjectNoPersistentId, SystemAllocator, 0);
            AZ_TYPE_INFO(ContainedObjectNoPersistentId, "{A9980498-6E7A-42C0-BF9F-DFA48142DDAB}");

            ContainedObjectNoPersistentId()
                : m_data(0)
            {}

            ContainedObjectNoPersistentId(int data)
                : m_data(data)
            {}

            int m_data;

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<ContainedObjectNoPersistentId>()->
                    Field("m_data", &ContainedObjectNoPersistentId::m_data);
            }
        };

        class CommonPatch
        {
        public:
            AZ_RTTI(CommonPatch, "{81FE64FA-23DB-40B5-BD1B-9DC145CB86EA}")

                virtual ~CommonPatch() {}

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<CommonPatch>()->
                    SerializerForEmptyClass();
            }
        };

        class ObjectToPatch
            : public CommonPatch
        {
        public:
            AZ_RTTI(ObjectToPatch, "{47E5CF10-3FA1-4064-BE7A-70E3143B4025}", CommonPatch)

            int m_intValue = 0;
            AZStd::vector<ContainedObjectPersistentId> m_objectArray;
            AZStd::vector<ContainedObjectDerivedPersistentId> m_derivedObjectArray;
            AZStd::unordered_map<u32, ContainedObjectNoPersistentId*> m_objectMap;
            AZStd::vector<ContainedObjectNoPersistentId> m_objectArrayNoPersistentId;
            AZ::DynamicSerializableField m_dynamicField;

            ~ObjectToPatch() override
            {
                m_dynamicField.DestroyData();

                for (auto mapElement : m_objectMap)
                {
                    delete mapElement.second;
                }
            }

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<ObjectToPatch, CommonPatch>()->
                    Field("m_dynamicField", &ObjectToPatch::m_dynamicField)->
                    Field("m_intValue", &ObjectToPatch::m_intValue)->
                    Field("m_objectArray", &ObjectToPatch::m_objectArray)->
                    Field("m_derivedObjectArray", &ObjectToPatch::m_derivedObjectArray)->
                    Field("m_objectMap", &ObjectToPatch::m_objectMap)->
                    Field("m_objectArrayNoPersistentId", &ObjectToPatch::m_objectArrayNoPersistentId);
            }
        };

        class DifferentObjectToPatch
            : public CommonPatch
        {
        public:
            AZ_RTTI(DifferentObjectToPatch, "{2E107ABB-E77A-4188-AC32-4CA8EB3C5BD1}", CommonPatch)

                float m_data;

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<DifferentObjectToPatch, CommonPatch>()->
                    Field("m_data", &DifferentObjectToPatch::m_data);
            }
        };

        class ObjectsWithGenerics
        {
        public:
            AZ_CLASS_ALLOCATOR(ObjectsWithGenerics, SystemAllocator, 0);
            AZ_TYPE_INFO(ObjectsWithGenerics, "{DE1EE15F-3458-40AE-A206-C6C957E2432B}");

            static void Reflect(AZ::SerializeContext& sc)
            {
                sc.Class<ObjectsWithGenerics>()->
                    Field("m_string", &ObjectsWithGenerics::m_string);
            }

            AZStd::string m_string;
        };
    }

    class PatchingTest
        : public Serialization
    {
    protected:
        void SetUp() override
        {
            Serialization::SetUp();

            m_serializeContext = AZStd::make_unique<SerializeContext>();

        using namespace Patching;
            CommonPatch::Reflect(*m_serializeContext);
            ContainedObjectPersistentId::Reflect(*m_serializeContext);
            ContainedObjectDerivedPersistentId::Reflect(*m_serializeContext);
            ContainedObjectNoPersistentId::Reflect(*m_serializeContext);
            ObjectToPatch::Reflect(*m_serializeContext);
            DifferentObjectToPatch::Reflect(*m_serializeContext);
            ObjectsWithGenerics::Reflect(*m_serializeContext);
        }
        
        void TearDown() override
        {
            m_serializeContext.reset();

            Serialization::TearDown();
        }

        AZStd::unique_ptr<SerializeContext> m_serializeContext;
    };

    TEST_F(PatchingTest, UberTest)
    {
        using namespace Patching;

        ObjectToPatch sourceObj;
        sourceObj.m_intValue = 101;
        sourceObj.m_objectArray.push_back();
        sourceObj.m_objectArray.push_back();
        sourceObj.m_objectArray.push_back();
        sourceObj.m_dynamicField.Set(aznew ContainedObjectNoPersistentId(40));
        {
            // derived
            sourceObj.m_derivedObjectArray.push_back();
            sourceObj.m_derivedObjectArray.push_back();
            sourceObj.m_derivedObjectArray.push_back();
        }

        // test generic containers with persistent ID
        sourceObj.m_objectArray[0].m_persistentId = 1;
        sourceObj.m_objectArray[0].m_data = 201;
        sourceObj.m_objectArray[1].m_persistentId = 2;
        sourceObj.m_objectArray[1].m_data = 202;
        sourceObj.m_objectArray[2].m_persistentId = 3;
        sourceObj.m_objectArray[2].m_data = 203;
        {
            // derived
            sourceObj.m_derivedObjectArray[0].m_persistentId = 1;
            sourceObj.m_derivedObjectArray[0].m_data = 2010;
            sourceObj.m_derivedObjectArray[1].m_persistentId = 2;
            sourceObj.m_derivedObjectArray[1].m_data = 2020;
            sourceObj.m_derivedObjectArray[2].m_persistentId = 3;
            sourceObj.m_derivedObjectArray[2].m_data = 2030;
        }

        // test generic container without persistent ID (we will use index)
        sourceObj.m_objectMap.insert(AZStd::make_pair(1, aznew ContainedObjectNoPersistentId(401)));
        sourceObj.m_objectMap.insert(AZStd::make_pair(2, aznew ContainedObjectNoPersistentId(402)));
        sourceObj.m_objectMap.insert(AZStd::make_pair(3, aznew ContainedObjectNoPersistentId(403)));
        sourceObj.m_objectMap.insert(AZStd::make_pair(4, aznew ContainedObjectNoPersistentId(404)));

        ObjectToPatch targetObj;
        targetObj.m_intValue = 121;
        targetObj.m_objectArray.push_back();
        targetObj.m_objectArray.push_back();
        targetObj.m_objectArray.push_back();
        targetObj.m_objectArray[0].m_persistentId = 1;
        targetObj.m_objectArray[0].m_data = 301;
        targetObj.m_dynamicField.Set(aznew ContainedObjectNoPersistentId(50));
        {
            // derived
            targetObj.m_derivedObjectArray.push_back();
            targetObj.m_derivedObjectArray.push_back();
            targetObj.m_derivedObjectArray.push_back();
            targetObj.m_derivedObjectArray[0].m_persistentId = 1;
            targetObj.m_derivedObjectArray[0].m_data = 3010;
        }
        // remove element 2
        targetObj.m_objectArray[1].m_persistentId = 3;
        targetObj.m_objectArray[1].m_data = 303;
        {
            // derived
            targetObj.m_derivedObjectArray[1].m_persistentId = 3;
            targetObj.m_derivedObjectArray[1].m_data = 3030;
        }
        // add new element
        targetObj.m_objectArray[2].m_persistentId = 4;
        targetObj.m_objectArray[2].m_data = 304;
        {
            // derived
            targetObj.m_derivedObjectArray[2].m_persistentId = 4;
            targetObj.m_derivedObjectArray[2].m_data = 3040;
        }
        // rearrange object map, add and remove elements, keep in mind without persistent id it's just index based
        targetObj.m_objectMap.insert(AZStd::make_pair(1, aznew ContainedObjectNoPersistentId(501)));
        targetObj.m_objectMap.insert(AZStd::make_pair(5, aznew ContainedObjectNoPersistentId(405)));

        // insert lots of objects without persistent id
        targetObj.m_objectArrayNoPersistentId.resize(999);
        for (int i = 0; i < targetObj.m_objectArrayNoPersistentId.size(); ++i)
        {
            targetObj.m_objectArrayNoPersistentId[i].m_data = i;
        }

        DataPatch patch;
        patch.Create(&sourceObj, &targetObj, DataPatch::FlagsMap(), m_serializeContext.get());
        ObjectToPatch* targetGenerated = patch.Apply(&sourceObj, m_serializeContext.get());

        // Compare the generated and original target object
        EXPECT_EQ( targetGenerated->m_intValue, targetObj.m_intValue );
        EXPECT_EQ( targetGenerated->m_objectArray.size(), targetObj.m_objectArray.size() );
        EXPECT_EQ( targetGenerated->m_objectArray[0].m_data, targetObj.m_objectArray[0].m_data );
        EXPECT_EQ( targetGenerated->m_objectArray[0].m_persistentId, targetObj.m_objectArray[0].m_persistentId );
        EXPECT_EQ( targetGenerated->m_objectArray[1].m_data, targetObj.m_objectArray[1].m_data );
        EXPECT_EQ( targetGenerated->m_objectArray[1].m_persistentId, targetObj.m_objectArray[1].m_persistentId );
        EXPECT_EQ( targetGenerated->m_objectArray[2].m_data, targetObj.m_objectArray[2].m_data );
        EXPECT_EQ( targetGenerated->m_objectArray[2].m_persistentId, targetObj.m_objectArray[2].m_persistentId );
        EXPECT_EQ( 50, targetGenerated->m_dynamicField.Get<ContainedObjectNoPersistentId>()->m_data );
        {
            // derived
            EXPECT_EQ( targetGenerated->m_derivedObjectArray.size(), targetObj.m_derivedObjectArray.size() );
            EXPECT_EQ( targetGenerated->m_derivedObjectArray[0].m_data, targetObj.m_derivedObjectArray[0].m_data );
            EXPECT_EQ( targetGenerated->m_derivedObjectArray[0].m_persistentId, targetObj.m_derivedObjectArray[0].m_persistentId );
            EXPECT_EQ( targetGenerated->m_derivedObjectArray[1].m_data, targetObj.m_derivedObjectArray[1].m_data );
            EXPECT_EQ( targetGenerated->m_derivedObjectArray[1].m_persistentId, targetObj.m_derivedObjectArray[1].m_persistentId );
            EXPECT_EQ( targetGenerated->m_derivedObjectArray[2].m_data, targetObj.m_derivedObjectArray[2].m_data );
            EXPECT_EQ( targetGenerated->m_derivedObjectArray[2].m_persistentId, targetObj.m_derivedObjectArray[2].m_persistentId );
        }
        // test generic containers without persistent ID (by index)
        EXPECT_EQ( targetGenerated->m_objectMap.size(), targetObj.m_objectMap.size() );
        EXPECT_EQ( targetGenerated->m_objectMap[1]->m_data, targetObj.m_objectMap[1]->m_data );
        EXPECT_EQ( targetGenerated->m_objectMap[5]->m_data, targetObj.m_objectMap[5]->m_data );

        // test that the relative order of elements without persistent ID is preserved
        EXPECT_EQ( targetGenerated->m_objectArrayNoPersistentId.size(), targetObj.m_objectArrayNoPersistentId.size() );
        for (int i = 0; i < targetObj.m_objectArrayNoPersistentId.size(); ++i)
        {
            EXPECT_EQ( targetGenerated->m_objectArrayNoPersistentId[i].m_data, targetObj.m_objectArrayNoPersistentId[i].m_data );
        }

        // test root element replacement
        ObjectToPatch obj1;
        DifferentObjectToPatch obj2;
        obj1.m_intValue = 99;
        obj2.m_data = 3.33f;

        DataPatch patch1;
        patch1.Create(static_cast<CommonPatch*>(&obj1), static_cast<CommonPatch*>(&obj2), DataPatch::FlagsMap(), m_serializeContext.get()); // cast to base classes
        DifferentObjectToPatch* obj2Generated = patch1.Apply<DifferentObjectToPatch>(&obj1, m_serializeContext.get());
        EXPECT_EQ( obj2.m_data, obj2Generated->m_data );

        // \note do we need to add support for base class patching and recover for root elements with proper casting

        // Combining patches
        targetObj.m_intValue = 301;
        targetObj.m_objectArray[0].m_data = 401;
        targetObj.m_objectArray[1].m_data = 402;
        targetObj.m_objectArray.pop_back(); // remove an element
        targetObj.m_objectMap.find(5)->second->m_data = 505;
        targetObj.m_objectMap.insert(AZStd::make_pair(6, aznew ContainedObjectNoPersistentId(406)));

        DataPatch patch2;
        patch2.Create(&sourceObj, &targetObj, DataPatch::FlagsMap(), m_serializeContext.get());
        patch.Apply(patch2);
        ObjectToPatch* targetGenerated2 = patch.Apply(&sourceObj, m_serializeContext.get());

        // Compare the generated and original target object
        EXPECT_EQ( targetGenerated2->m_intValue, targetObj.m_intValue );
        EXPECT_EQ( targetGenerated2->m_objectArray.size(), targetObj.m_objectArray.size() + 1 );
        EXPECT_EQ( targetGenerated2->m_objectArray[0].m_data, targetObj.m_objectArray[0].m_data );
        EXPECT_EQ( targetGenerated2->m_objectArray[0].m_persistentId, targetObj.m_objectArray[0].m_persistentId );
        EXPECT_EQ( targetGenerated2->m_objectArray[1].m_data, targetObj.m_objectArray[1].m_data );
        EXPECT_EQ( targetGenerated2->m_objectArray[1].m_persistentId, targetObj.m_objectArray[1].m_persistentId );
        EXPECT_EQ( 50, targetGenerated2->m_dynamicField.Get<ContainedObjectNoPersistentId>()->m_data );
        EXPECT_EQ( 304, targetGenerated2->m_objectArray[2].m_data );  // merged from the based patch
        EXPECT_EQ( 4, targetGenerated2->m_objectArray[2].m_persistentId );
        // test generic containers without persistent ID (by index)
        EXPECT_EQ( targetGenerated2->m_objectMap.size(), targetObj.m_objectMap.size() );
        EXPECT_EQ( targetGenerated2->m_objectMap[1]->m_data, targetObj.m_objectMap[1]->m_data );
        EXPECT_EQ( targetGenerated2->m_objectMap[5]->m_data, targetObj.m_objectMap[5]->m_data );
        EXPECT_EQ( targetGenerated2->m_objectMap[6]->m_data, targetObj.m_objectMap[6]->m_data );

        targetGenerated->m_dynamicField.DestroyData(m_serializeContext.get());
        targetGenerated2->m_dynamicField.DestroyData(m_serializeContext.get());
        targetObj.m_dynamicField.DestroyData(m_serializeContext.get());
        sourceObj.m_dynamicField.DestroyData(m_serializeContext.get());

        delete targetGenerated;
        delete targetGenerated2;
        delete obj2Generated;

        // test generics
        ObjectsWithGenerics sourceGeneric;
        sourceGeneric.m_string = "Hello";

        ObjectsWithGenerics targetGeneric;
        targetGeneric.m_string = "Ola";

        DataPatch genericPatch;
        genericPatch.Create(&sourceGeneric, &targetGeneric, DataPatch::FlagsMap(), m_serializeContext.get());

        ObjectsWithGenerics* targerGenericGenerated = genericPatch.Apply(&sourceGeneric, m_serializeContext.get());
        EXPECT_EQ( targetGeneric.m_string, targerGenericGenerated->m_string );
        delete targerGenericGenerated;
    }

    /*
    *
    */
    class SerializeDescendentDataElementTest
        : public AllocatorsFixture
    {
    public:
        struct DataElementTestClass
        {
            AZ_CLASS_ALLOCATOR(DataElementTestClass, AZ::SystemAllocator, 0);
            AZ_TYPE_INFO(DataElementTestClass, "{F515B922-BBB9-4216-A2C9-FD665AA30046}");

            DataElementTestClass() {}

            AZStd::unique_ptr<AZ::Entity> m_data;
            AZStd::vector<AZ::Vector2> m_positions;

        private:
            DataElementTestClass(const DataElementTestClass&) = delete;
        };

        static bool VersionConverter(AZ::SerializeContext& sc, AZ::SerializeContext::DataElementNode& classElement)
        {
            if (classElement.GetVersion() == 0)
            {
                auto entityIdElements = AZ::Utils::FindDescendantElements(sc, classElement, AZStd::vector<AZ::Crc32>({ AZ_CRC("m_data"), AZ_CRC("element"), AZ_CRC("Id"), AZ_CRC("id") }));
                EXPECT_EQ(1, entityIdElements.size());
                AZ::u64 id1;
                EXPECT_TRUE(entityIdElements.front()->GetData(id1));
                EXPECT_EQ(47, id1);

                auto vector2Elements = AZ::Utils::FindDescendantElements(sc, classElement, AZStd::vector<AZ::Crc32>({ AZ_CRC("m_positions"), AZ_CRC("element") }));
                EXPECT_EQ(2, vector2Elements.size());
                AZ::Vector2 position;
                EXPECT_TRUE(vector2Elements[0]->GetData(position));
                EXPECT_FLOAT_EQ(1.0f, position.GetX());
                EXPECT_FLOAT_EQ(2.0f, position.GetY());

                EXPECT_TRUE(vector2Elements[1]->GetData(position));
                EXPECT_FLOAT_EQ(2.0f, position.GetX());
                EXPECT_FLOAT_EQ(4.0f, position.GetY());
            }

            return true;
        }

        DataElementTestClass m_dataElementClass;

        void run()
        {
            m_dataElementClass.m_data = AZStd::make_unique<AZ::Entity>("DataElement");
            m_dataElementClass.m_data->SetId(AZ::EntityId(47));
            m_dataElementClass.m_positions.emplace_back(1.0f, 2.0f);
            m_dataElementClass.m_positions.emplace_back(2.0f, 4.0f);

            // Write original data
            AZStd::vector<AZ::u8> binaryBuffer;
            {
                AZ::SerializeContext sc;
                AZ::Entity::Reflect(&sc);
                sc.Class<DataElementTestClass>()
                    ->Version(0)
                    ->Field("m_data", &DataElementTestClass::m_data)
                    ->Field("m_positions", &DataElementTestClass::m_positions);

                // Binary
                AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8> > binaryStream(&binaryBuffer);
                AZ::ObjectStream* binaryObjStream = ObjectStream::Create(&binaryStream, sc, ObjectStream::ST_BINARY);
                binaryObjStream->WriteClass(&m_dataElementClass);
                EXPECT_TRUE(binaryObjStream->Finalize());
            }

            // Test find descendant version converter
            {
                AZ::SerializeContext sc;
                AZ::Entity::Reflect(&sc);
                sc.Class<DataElementTestClass>()
                    ->Version(1, &VersionConverter)
                    ->Field("m_data", &DataElementTestClass::m_data)
                    ->Field("m_positions", &DataElementTestClass::m_positions);

                // Binary
                IO::ByteContainerStream<const AZStd::vector<AZ::u8> > binaryStream(&binaryBuffer);
                binaryStream.Seek(0, IO::GenericStream::ST_SEEK_BEGIN);
                ObjectStream::LoadBlocking(&binaryStream, sc, {});
            }
        }
    };

    TEST_F(SerializeDescendentDataElementTest, FindTest)
    {
        run();
    }

    class SerializableAnyFieldTest
        : public AllocatorsFixture
    {
    public:
        struct AnyMemberClass
        {
            AZ_TYPE_INFO(AnyMemberClass, "{67F73D37-5F9E-42FE-AFC9-9867924D87DD}");
            AZ_CLASS_ALLOCATOR(AnyMemberClass, AZ::SystemAllocator, 0);
            static void Reflect(ReflectContext* context)
            {
                if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
                {
                    serializeContext->Class<AnyMemberClass>()
                        ->Field("Any", &AnyMemberClass::m_any)
                        ;
                }
            }
            AZStd::any m_any;
        };

        // We must expose the class for serialization first.
        void SetUp() override
        {
            AllocatorsFixture::SetUp();

            AZ::AllocatorInstance<AZ::PoolAllocator>::Create();
            AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Create();

            m_serializeContext = AZStd::make_unique<SerializeContext>();
            AnyMemberClass::Reflect(m_serializeContext.get());
            MyClassBase1::Reflect(*m_serializeContext);
            MyClassBase2::Reflect(*m_serializeContext);
            MyClassBase3::Reflect(*m_serializeContext);
            MyClassMix::Reflect(*m_serializeContext);
            ReflectedString::Reflect(m_serializeContext.get());
        }

        void TearDown() override
        {

            m_serializeContext->EnableRemoveReflection();
            AnyMemberClass::Reflect(m_serializeContext.get());
            MyClassBase1::Reflect(*m_serializeContext);
            MyClassBase2::Reflect(*m_serializeContext);
            MyClassBase3::Reflect(*m_serializeContext);
            MyClassMix::Reflect(*m_serializeContext);
            ReflectedString::Reflect(m_serializeContext.get());
            m_serializeContext->DisableRemoveReflection();

            m_serializeContext.reset();

            AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Destroy();
            AZ::AllocatorInstance<AZ::PoolAllocator>::Destroy();

            AllocatorsFixture::TearDown();
        }
        struct ReflectedString
        {
            AZ_TYPE_INFO(ReflectedString, "{5DE01DEA-119F-43E9-B87C-BF980EBAD896}");
            AZ_CLASS_ALLOCATOR(ReflectedString, AZ::SystemAllocator, 0);
            static void Reflect(ReflectContext* context)
            {
                if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
                {
                    //String class must reflected in at least one field
                    serializeContext->Class<ReflectedString>()
                        ->Field("String", &ReflectedString::m_name)
                        ;
                }
            }
            AZStd::string m_name;
        };

    protected:
        struct NonReflectedClass
        {
            AZ_TYPE_INFO(NonReflectedClass, "{13B8CFB0-601A-4C03-BC19-4EDC71156254}");
            AZ_CLASS_ALLOCATOR(NonReflectedClass, AZ::SystemAllocator, 0);
            AZ::u64 m_num;
            AZStd::string m_name;
        };

        


        AZStd::unique_ptr<SerializeContext> m_serializeContext;
    };

    TEST_F(SerializableAnyFieldTest, EmptyAnyTest)
    {
        AZStd::any emptyAny;

        // BINARY
        AZStd::vector<char> byteBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > byteStream(&byteBuffer);
        ObjectStream* byteObjStream = ObjectStream::Create(&byteStream, *m_serializeContext, ObjectStream::ST_BINARY);
        byteObjStream->WriteClass(&emptyAny);
        byteObjStream->Finalize();

        byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        AZStd::any readAnyData;
        AZ::Utils::LoadObjectFromStreamInPlace(byteStream, readAnyData, m_serializeContext.get());
        EXPECT_TRUE(readAnyData.empty());

        // JSON
        byteBuffer.clear();
        IO::ByteContainerStream<AZStd::vector<char> > jsonStream(&byteBuffer);
        ObjectStream* jsonObjStream = ObjectStream::Create(&jsonStream, *m_serializeContext, ObjectStream::ST_JSON);
        jsonObjStream->WriteClass(&emptyAny);
        jsonObjStream->Finalize();

        jsonStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::any readAnyDataJson;
        AZ::Utils::LoadObjectFromStreamInPlace(jsonStream, readAnyDataJson, m_serializeContext.get());
        EXPECT_TRUE(readAnyDataJson.empty());

        // JSON
        byteBuffer.clear();
        IO::ByteContainerStream<AZStd::vector<char> > xmlStream(&byteBuffer);
        ObjectStream* xmlObjStream = ObjectStream::Create(&xmlStream, *m_serializeContext, ObjectStream::ST_XML);
        xmlObjStream->WriteClass(&emptyAny);
        xmlObjStream->Finalize();

        xmlStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        AZStd::any readAnyDataXml;
        AZ::Utils::LoadObjectFromStreamInPlace(xmlStream, readAnyDataXml, m_serializeContext.get());
        EXPECT_TRUE(readAnyDataXml.empty());
    }

    TEST_F(SerializableAnyFieldTest, ReflectedFieldTest)
    {
        MyClassMix obj;
        obj.Set(5); // Initialize with some value

        AZStd::any testData(obj);

        // BINARY
        AZStd::vector<char> byteBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > byteStream(&byteBuffer);
        ObjectStream* byteObjStream = ObjectStream::Create(&byteStream, *m_serializeContext, ObjectStream::ST_XML);
        byteObjStream->WriteClass(&testData);
        byteObjStream->Finalize();

        byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        AZStd::any readAnyData;
        AZ::Utils::LoadObjectFromStreamInPlace(byteStream, readAnyData, m_serializeContext.get());
        EXPECT_EQ(SerializeTypeInfo<MyClassMix>::GetUuid(), readAnyData.type());
        EXPECT_NE(nullptr, AZStd::any_cast<void>(&readAnyData));
        const MyClassMix& anyMixRef = *AZStd::any_cast<MyClassMix>(&testData);
        const MyClassMix& readAnyMixRef = *AZStd::any_cast<MyClassMix>(&readAnyData);
        EXPECT_EQ(anyMixRef.m_dataMix, readAnyMixRef.m_dataMix);
    }

    TEST_F(SerializableAnyFieldTest, NonReflectedFieldTest)
    {
        NonReflectedClass notReflected;
        notReflected.m_num = 17;
        notReflected.m_name = "Test";

        AZStd::any testData(notReflected);

        // BINARY
        AZStd::vector<char> byteBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > byteStream(&byteBuffer);
        ObjectStream* byteObjStream = ObjectStream::Create(&byteStream, *m_serializeContext, ObjectStream::ST_BINARY);
        AZ_TEST_START_ASSERTTEST;
        byteObjStream->WriteClass(&testData);
        AZ_TEST_STOP_ASSERTTEST(1);
        byteObjStream->Finalize();

        byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        AZStd::any readAnyData;
        AZ::Utils::LoadObjectFromStreamInPlace(byteStream, readAnyData, m_serializeContext.get());
        EXPECT_EQ(AZ::Uuid::CreateNull(), readAnyData.type());
        EXPECT_TRUE(readAnyData.empty());
    }

    TEST_F(SerializableAnyFieldTest, EnumerateFieldTest)
    {
        MyClassMix obj;
        obj.m_dataMix = 5.;
        m_serializeContext->EnumerateObject(&obj,
            [this](void* classPtr, const SerializeContext::ClassData* classData, const SerializeContext::ClassElement*)
        {
            if (classData->m_typeId == azrtti_typeid<MyClassMix>())
            {
                auto mixinClassPtr = reinterpret_cast<MyClassMix*>(classPtr);
                EXPECT_NE(nullptr, mixinClassPtr);
                EXPECT_DOUBLE_EQ(5.0, mixinClassPtr->m_dataMix);
            }
            return true;
        },
            [this]() -> bool
        {
            return true;
        },
            SerializeContext::ENUM_ACCESS_FOR_READ);
    }

    TEST_F(SerializableAnyFieldTest, MemberFieldTest)
    {
        MyClassMix mixedClass;
        mixedClass.m_enum = MyClassBase3::Option3;
        AnyMemberClass anyWrapper;
        anyWrapper.m_any = AZStd::any(mixedClass);

        // BINARY
        AZStd::vector<char> byteBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > byteStream(&byteBuffer);
        ObjectStream* byteObjStream = ObjectStream::Create(&byteStream, *m_serializeContext, ObjectStream::ST_BINARY);
        byteObjStream->WriteClass(&anyWrapper);
        byteObjStream->Finalize();

        byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        AnyMemberClass readAnyWrapper;
        AZ::Utils::LoadObjectFromStreamInPlace(byteStream, readAnyWrapper, m_serializeContext.get());
        EXPECT_EQ(SerializeTypeInfo<MyClassMix>::GetUuid(), readAnyWrapper.m_any.type());
        EXPECT_NE(nullptr, AZStd::any_cast<void>(&readAnyWrapper.m_any));
        auto* readMixedClass = AZStd::any_cast<MyClassMix>(&readAnyWrapper.m_any);
        EXPECT_NE(nullptr, readMixedClass);
        EXPECT_EQ(MyClassBase3::Option3, readMixedClass->m_enum);
        MyClassMix& anyMixRef = *AZStd::any_cast<MyClassMix>(&anyWrapper.m_any);
        EXPECT_EQ(anyMixRef, *readMixedClass);
    }

    TEST_F(SerializableAnyFieldTest, AZStdStringFieldTest)
    {
        AZStd::string test("Canvas");
        AZStd::any anyString(test);

        // BINARY
        AZStd::vector<char> byteBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > byteStream(&byteBuffer);
        ObjectStream* byteObjStream = ObjectStream::Create(&byteStream, *m_serializeContext, ObjectStream::ST_BINARY);
        byteObjStream->WriteClass(&anyString);
        byteObjStream->Finalize();

        byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        AZStd::any readAnyString;
        AZ::Utils::LoadObjectFromStreamInPlace(byteStream, readAnyString, m_serializeContext.get());
        EXPECT_EQ(azrtti_typeid<AZStd::string>(), readAnyString.type());
        auto* serializedString = AZStd::any_cast<AZStd::string>(&readAnyString);
        EXPECT_NE(nullptr, serializedString);
        EXPECT_EQ(test, *serializedString);
    }

    TEST_F(SerializableAnyFieldTest, ReflectedPointerFieldTest)
    {
        MyClassMix obj;
        obj.Set(26); // Initialize with some value

        AZStd::any testData(&obj);

        // BINARY
        AZStd::vector<char> byteBuffer;
        IO::ByteContainerStream<AZStd::vector<char> > byteStream(&byteBuffer);
        ObjectStream* byteObjStream = ObjectStream::Create(&byteStream, *m_serializeContext, ObjectStream::ST_BINARY);
        byteObjStream->WriteClass(&testData);
        byteObjStream->Finalize();

        byteStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

        AZStd::any readAnyData;
        AZ::Utils::LoadObjectFromStreamInPlace(byteStream, readAnyData, m_serializeContext.get());
        EXPECT_EQ(SerializeTypeInfo<MyClassMix>::GetUuid(), readAnyData.type());
        EXPECT_NE(nullptr, AZStd::any_cast<void>(&readAnyData));
        const MyClassMix* anyMixRef = AZStd::any_cast<MyClassMix*>(testData);
        const MyClassMix& readAnyMixRef = *AZStd::any_cast<MyClassMix>(&readAnyData);
        EXPECT_EQ(anyMixRef->m_dataMix, readAnyMixRef.m_dataMix);
    }

    TEST_F(PatchingTest, CompareIdentical_DataPatchIsEmpty)
    {
        using namespace Patching;
        ObjectToPatch sourceObj;
        ObjectToPatch targetObj;

        // Patch without overrides should be empty
        DataPatch patch;
        patch.Create(&sourceObj, &targetObj, DataPatch::FlagsMap(), m_serializeContext.get());
        EXPECT_FALSE(patch.IsData());
    }

    TEST_F(PatchingTest, CompareIdenticalWithForceOverride_DataPatchHasData)
    {
        using namespace Patching;
        ObjectToPatch sourceObj;
        ObjectToPatch targetObj;

        DataPatch::AddressType forceOverrideAddress;
        forceOverrideAddress.emplace_back(AZ_CRC("m_intValue"));

        DataPatch::FlagsMap flagsMap;
        flagsMap.emplace(forceOverrideAddress, DataPatch::Flag::ForceOverride);

        DataPatch patch;
        patch.Create(&sourceObj, &targetObj, flagsMap, m_serializeContext.get());
        EXPECT_TRUE(patch.IsData());
    }

    TEST_F(PatchingTest, ChangeSourceAfterForceOverride_TargetDataUnchanged)
    {
        using namespace Patching;
        ObjectToPatch sourceObj;
        ObjectToPatch targetObj;

        DataPatch::AddressType forceOverrideAddress;
        forceOverrideAddress.emplace_back(AZ_CRC("m_intValue"));

        DataPatch::FlagsMap flagsMap;
        flagsMap.emplace(forceOverrideAddress, DataPatch::Flag::ForceOverride);

        DataPatch patch;
        patch.Create(&sourceObj, &targetObj, flagsMap, m_serializeContext.get());

        // change source after patch is created
        sourceObj.m_intValue = 5;

        AZStd::unique_ptr<ObjectToPatch> targetObj2(patch.Apply(&sourceObj, m_serializeContext.get()));
        EXPECT_EQ(targetObj.m_intValue, targetObj2->m_intValue);
    }
    //class AssetSerializationTest : public SerializeTest
    //{
    //public:
    //
    //    void OnLoadedClassReady(void* classPtr, const Uuid& classId, int* callCount)
    //    {
    //        (void)callCount;

    //        if (classId == SerializeTypeInfo<Asset>::GetUuid())
    //        {
    //            Asset* obj = reinterpret_cast<Asset*>(classPtr);
    //            //EXPECT_TRUE();
    //            delete obj;
    //        }
    //    }

    //    void OnDone(ObjectStream::Handle handle, bool success, bool* done)
    //    {
    //        (void)handle;
    //        (void)success;
    //        *done = true;
    //    }

    //    void run()
    //    {
    //        SerializeContext serializeContext;
    //
    //        Payload::Reflect(serializeContext);
    //        TestFileIOBase fileIO;
    //        SetRestoreFileIOBaseRAII restoreFileIOScope(fileIO);

    //        bool clone = true;
    //        if (clone)
    //        {
    //            Payload testObj;
    //            Payload* payload = serializeContext.CloneObject(&testObj);
    //            delete payload;
    //        }

    //        bool write = true;
    //        if (write)
    //        {
    //            Payload testObj;
    //            Payload* payload = serializeContext.CloneObject(&testObj);

    //            AZ_TracePrintf("LargeDataSerializationTest", "\nWriting as XML...\n");
    //            IO::StreamerStream stream("LargeDataSerializationTest.xml", IO::OpenMode::ModeWrite);
    //            ObjectStream* objStream = ObjectStream::Create(&stream, serializeContext, ObjectStream::ST_XML);
    //            objStream->WriteClass(payload);
    //            objStream->Finalize();

    //            delete payload;
    //        }

    //        bool writeBinary = true;
    //        if (writeBinary)
    //        {
    //            Payload testObj;
    //            Payload* payload = serializeContext.CloneObject(&testObj);

    //            AZ_TracePrintf("LargeDataSerializationTest", "\nWriting as Binary...\n");
    //            IO::StreamerStream stream("LargeDataSerializationTest.bin", IO::OpenMode::ModeWrite);
    //            ObjectStream* objStream = ObjectStream::Create(&stream, serializeContext, ObjectStream::ST_BINARY);
    //            objStream->WriteClass(payload);
    //            objStream->Finalize();

    //            delete payload;
    //        }

    //        bool read = true;
    //        if (read)
    //        {
    //            int cbCount = 0;
    //            bool done = false;
    //            ObjectStream::ClassReadyCB readyCB(AZStd::bind(&LargeDataSerializationTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
    //            ObjectStream::CompletionCB doneCB(AZStd::bind(&LargeDataSerializationTest::OnDone, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &done));
    //            {
    //                AZ_TracePrintf("LargeDataSerializationTest", "Loading as XML...\n");
    //                IO::StreamerStream stream2("LargeDataSerializationTest.xml", IO::OpenMode::ModeRead);
    //                ObjectStream::LoadBlocking(&stream2, serializeContext, readyCB);
    //            }
    //        }

    //        bool readBinary = true;
    //        if (readBinary)
    //        {
    //            int cbCount = 0;
    //            bool done = false;
    //            ObjectStream::ClassReadyCB readyCB(AZStd::bind(&LargeDataSerializationTest::OnLoadedClassReady, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &cbCount));
    //            ObjectStream::CompletionCB doneCB(AZStd::bind(&LargeDataSerializationTest::OnDone, this, AZStd::placeholders::_1, AZStd::placeholders::_2, &done));
    //            {
    //                AZ_TracePrintf("LargeDataSerializationTest", "Loading as Binary...\n");
    //                IO::StreamerStream stream2("LargeDataSerializationTest.bin", IO::OpenMode::ModeRead);
    //                ObjectStream::LoadBlocking(&stream2, serializeContext, readyCB);
    //            }
    //        }
    //    }
    //};

    //TEST_F(AssetSerializationTest, Test)
    //{
    //    run();
    //}
}

