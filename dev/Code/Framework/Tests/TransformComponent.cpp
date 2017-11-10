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

#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Serialization/Utils.h>

#include <AzFramework/Application/Application.h>
#include <AzFramework/Components/TransformComponent.h>

#include <AzFramework/Math/MathUtils.h>

using namespace AZ;
using namespace AzFramework;

namespace UnitTest
{
    // Fixture base class for AzFramework::TransformComponent tests.
    class TransformComponentApplication
        : public AllocatorsFixture
    {
    public:
        TransformComponentApplication()
            : AllocatorsFixture(15, false)
        {
        }

    protected:
        void SetUp() override
        {
            ComponentApplication::Descriptor desc;
            desc.m_useExistingAllocator = true;

            m_app.Start(desc);
        }

        void TearDown() override
        {
            m_app.Stop();
        }

        AzFramework::Application m_app;
    };

    
    // Runs a series of tests on TransformComponent.
    class TransformComponentUberTest
        : public TransformComponentApplication
        , public TransformNotificationBus::Handler
    {
    public:

        //////////////////////////////////////////////////////////////////////////////
        // TransformNotificationBus
        virtual void OnTransformChanged(const Transform& local, const Transform& world)
        {
            AZ_TEST_ASSERT(m_checkWorldTM == world);
            AZ_TEST_ASSERT(m_checkLocalTM == local);
        }

        /// Called when the parent of an entity has changed. When the old/new parent are invalid the !EntityId.IsValid
        virtual void OnParentChanged(EntityId oldParent, EntityId newParent)
        {
            AZ_TEST_ASSERT(m_checkOldParentId == oldParent);
            AZ_TEST_ASSERT(m_checkNewParentId == newParent);
        }

        //////////////////////////////////////////////////////////////////////////////

        void run()
        {
            m_checkWorldTM = Transform::CreateIdentity();
            m_checkLocalTM = Transform::CreateIdentity();

            // Create test entity
            Entity childEntity, parentEntity;
            TransformComponent* childTransformComponent = childEntity.CreateComponent<TransformComponent>();
            TransformComponent* parentTranformComponent = parentEntity.CreateComponent<TransformComponent>();
            (void)parentTranformComponent;

            TransformNotificationBus::Handler::BusConnect(childEntity.GetId());

            childEntity.Init();
            parentEntity.Init();

            // We bind transform interface only when entity is activated
            AZ_TEST_ASSERT(childEntity.GetTransform() == nullptr);
            childEntity.Activate();
            TransformInterface* childTransform = childEntity.GetTransform();

            parentEntity.Activate();
            TransformInterface* parentTransform = parentEntity.GetTransform();
            parentTransform->SetWorldTM(Transform::CreateTranslation(Vector3(1.0f, 0.0f, 0.0f)));

            // Check validity of transform interface and initial transforms
            AZ_TEST_ASSERT(childTransform == static_cast<TransformInterface*>(childTransformComponent));
            AZ_TEST_ASSERT(childTransform->GetWorldTM() == m_checkWorldTM);
            AZ_TEST_ASSERT(childTransform->GetLocalTM() == m_checkLocalTM);
            AZ_TEST_ASSERT(childTransform->GetParentId() == m_checkNewParentId);

            // Modify the local (and world) matrix
            m_checkLocalTM = Transform::CreateTranslation(Vector3(5.0f, 0.0f, 0.0f));
            m_checkWorldTM = m_checkLocalTM;
            childTransform->SetWorldTM(m_checkWorldTM);

            // Parent the child object
            m_checkNewParentId = parentEntity.GetId();
            m_checkLocalTM *= parentTransform->GetWorldTM().GetInverseFull(); // the set parent will move the child object into parent space
            childTransform->SetParent(m_checkNewParentId);

            // Deactivate the parent (this essentially removes the parent)
            m_checkNewParentId.SetInvalid();
            m_checkOldParentId = parentEntity.GetId();
            m_checkLocalTM = m_checkWorldTM; // we will remove the parent
            parentEntity.Deactivate();

            // now we should we without a parent
            childEntity.Deactivate();
        }

        Transform m_checkWorldTM;
        Transform m_checkLocalTM;
        EntityId m_checkOldParentId;
        EntityId m_checkNewParentId;
    };

    TEST_F(TransformComponentUberTest, Test)
    {
        run();
    }

    class TransformComponentChildNotificationTest
        : public TransformComponentApplication
        , public TransformNotificationBus::Handler
    {
    public:
        void OnChildAdded(EntityId child) override
        {
            AZ_TEST_ASSERT(child == m_checkChildId);
            m_onChildAddedCount++;
        }

        void OnChildRemoved(EntityId child) override
        {
            AZ_TEST_ASSERT(child == m_checkChildId);
            m_onChildRemovedCount++;
        }

        void run()
        {
            // Create ID for parent and begin listening for child add/remove notifications
            AZ::EntityId parentId = Entity::MakeId();
            TransformNotificationBus::Handler::BusConnect(parentId);

            Entity childEntity;
            TransformComponentConfiguration transformConfig;
            transformConfig.m_isStatic = false;
            childEntity.CreateComponent<TransformComponent>(transformConfig);

            m_checkChildId = childEntity.GetId();

            childEntity.Init();
            childEntity.Activate();
            TransformInterface* childTransform = childEntity.GetTransform();

            // Expected number of notifications to OnChildAdded and OnChildRemoved
            int checkAddCount = 0;
            int checkRemoveCount = 0;

            // Changing to target parentId should notify add
            AZ_TEST_ASSERT(m_onChildAddedCount == checkAddCount);
            childTransform->SetParent(parentId);
            checkAddCount++;
            AZ_TEST_ASSERT(m_onChildAddedCount == checkAddCount);

            // Deactivating child should notify removal
            AZ_TEST_ASSERT(m_onChildRemovedCount == checkRemoveCount);
            childEntity.Deactivate();
            checkRemoveCount++;
            AZ_TEST_ASSERT(m_onChildRemovedCount == checkRemoveCount);

            // Activating child (while parentId is set) should notify add
            AZ_TEST_ASSERT(m_onChildAddedCount == checkAddCount);
            childEntity.Activate();
            checkAddCount++;
            AZ_TEST_ASSERT(m_onChildAddedCount == checkAddCount);

            // Setting parent invalid should notify removal
            AZ_TEST_ASSERT(m_onChildRemovedCount == checkRemoveCount);
            childTransform->SetParent(EntityId());
            checkRemoveCount++;
            AZ_TEST_ASSERT(m_onChildRemovedCount == checkRemoveCount);

            childEntity.Deactivate();
        }

        EntityId m_checkChildId;
        int m_onChildAddedCount = 0;
        int m_onChildRemovedCount = 0;
    };

    TEST_F(TransformComponentChildNotificationTest, Test)
    {
        run();
    }

    class LookAtTransformTest
        : public ::testing::Test
    {
    public:
        void run()
        {
            // CreateLookAt
            AZ::Vector3 lookAtEye(1.0f, 2.0f, 3.0f);
            AZ::Vector3 lookAtTarget(10.0f, 5.0f, -5.0f);
            AZ::Transform t1 = AzFramework::CreateLookAt(lookAtEye, lookAtTarget);
            AZ_TEST_ASSERT(t1.GetBasisY().IsClose((lookAtTarget - lookAtEye).GetNormalized()));
            AZ_TEST_ASSERT(t1.GetTranslation() == lookAtEye);
            AZ_TEST_ASSERT(t1.IsOrthogonal());

            AZ_TEST_START_ASSERTTEST;
            t1 = AzFramework::CreateLookAt(lookAtEye, lookAtEye); //degenerate direction
            AZ_TEST_STOP_ASSERTTEST(1);
            AZ_TEST_ASSERT(t1.IsOrthogonal());
            AZ_TEST_ASSERT(t1 == AZ::Transform::CreateIdentity());

            t1 = AzFramework::CreateLookAt(lookAtEye, lookAtEye + AZ::Vector3::CreateAxisZ()); //degenerate with up direction
            AZ_TEST_ASSERT(t1.GetBasisY().IsClose(AZ::Vector3::CreateAxisZ()));
            AZ_TEST_ASSERT(t1.GetTranslation() == lookAtEye);
            AZ_TEST_ASSERT(t1.IsOrthogonal());
        }
    };

    TEST_F(LookAtTransformTest, Test)
    {
        run();
    }

    // Test TransformComponent's methods of modifying/retrieving underlying translation, rotation and scale transform component.
    class TransformComponentTransformMatrixSetGet
        : public TransformComponentApplication
    {
    protected:
        void SetUp() override
        {
            TransformComponentApplication::SetUp();

            m_parentEntity = aznew Entity("Parent");
            m_parentId = m_parentEntity->GetId();
            m_parentEntity->Init();
            m_parentEntity->CreateComponent<TransformComponent>();

            m_childEntity = aznew Entity("Child");
            m_childId = m_childEntity->GetId();
            m_childEntity->Init();
            m_childEntity->CreateComponent<TransformComponent>();

            m_parentEntity->Activate();
            m_childEntity->Activate();

            TransformBus::Event(m_childId, &TransformBus::Events::SetParent, m_parentId);
        }

        void TearDown() override
        {
            m_childEntity->Deactivate();
            m_parentEntity->Deactivate();
            delete m_childEntity;
            delete m_parentEntity;

            TransformComponentApplication::TearDown();
        }

        Entity* m_parentEntity = nullptr;
        EntityId m_parentId = EntityId();
        Entity* m_childEntity = nullptr;
        EntityId m_childId = EntityId();
    };

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalX_SimpleValues_Set)
    {
        float tx = 123.123f;
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalX, tx);
        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        EXPECT_TRUE(tx == tm.GetElement(0, 3));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetLocalX_SimpleValues_Set)
    {
        Transform tm;
        tm.SetElement(0, 3, 432.456f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, tm);
        float tx = 0;
        TransformBus::EventResult(tx, m_childId, &TransformBus::Events::GetLocalX);
        EXPECT_TRUE(tx == tm.GetElement(0, 3));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalY_SimpleValues_Set)
    {
        float ty = 435.676f;
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalY, ty);
        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        EXPECT_TRUE(ty == tm.GetElement(1, 3));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetLocalY_SimpleValues_Set)
    {
        Transform tm;
        tm.SetElement(1, 3, 154.754f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, tm);
        float ty = 0;
        TransformBus::EventResult(ty, m_childId, &TransformBus::Events::GetLocalY);
        EXPECT_TRUE(ty == tm.GetElement(1, 3));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalZ_SimpleValues_Set)
    {
        float tz = 987.456f;
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalZ, tz);
        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        EXPECT_TRUE(tz == tm.GetElement(2, 3));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetLocalZ_SimpleValues_Set)
    {
        Transform tm;
        tm.SetElement(2, 3, 453.894f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, tm);
        float tz = 0;
        TransformBus::EventResult(tz, m_childId, &TransformBus::Events::GetLocalZ);
        EXPECT_TRUE(tz == tm.GetElement(2, 3));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalRotation_SimpleValues_Set)
    {
        // add some scale first
        float sx = 1.03f, sy = 0.67f, sz = 1.23f;
        Transform tm = Transform::CreateScale(Vector3(sx, sy, sz));
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, tm);

        float rx = 42.435f;
        float ry = 19.454f;
        float rz = 98.356f;
        Vector3 angles(rx, ry, rz);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalRotation, angles);

        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Matrix3x3 rotateZ = Matrix3x3::CreateRotationZ(rz);
        Matrix3x3 rotateY = Matrix3x3::CreateRotationY(ry);
        Matrix3x3 rotateX = Matrix3x3::CreateRotationX(rx);
        Matrix3x3 finalRotate = rotateX * rotateY * rotateZ;

        Vector3 basisX = tm.GetBasisX();
        Vector3 expectedBasisX = finalRotate.GetBasisX() * sx;
        EXPECT_TRUE(basisX.IsClose(expectedBasisX));
        Vector3 basisY = tm.GetBasisY();
        Vector3 expectedBasisY = finalRotate.GetBasisY() * sy;
        EXPECT_TRUE(basisY.IsClose(expectedBasisY));
        Vector3 basisZ = tm.GetBasisZ();
        Vector3 expectedBasisZ = finalRotate.GetBasisZ() * sz;
        EXPECT_TRUE(basisZ.IsClose(expectedBasisZ));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetLocalRotation_SimpleValues_Return)
    {
        float rx = 0.66f;
        float ry = 1.23f;
        float rz = 0.23f;
        Matrix3x3 rotateZ = Matrix3x3::CreateRotationZ(rz);
        Matrix3x3 rotateY = Matrix3x3::CreateRotationY(ry);
        Matrix3x3 rotateX = Matrix3x3::CreateRotationX(rx);
        Matrix3x3 finalRotate = rotateX * rotateY * rotateZ;
        Transform tm = Transform::CreateFromMatrix3x3(finalRotate);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, tm);

        Vector3 angles;
        TransformBus::EventResult(angles, m_childId, &TransformBus::Events::GetLocalRotation);

        EXPECT_TRUE(angles.IsClose(Vector3(rx, ry, rz)));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalRotationQuaternion_SimpleValues_Set)
    {
        float rx = 42.435f;
        float ry = 19.454f;
        float rz = 98.356f;
        Quaternion quatX = Quaternion::CreateRotationX(rx);
        Quaternion quatY = Quaternion::CreateRotationY(ry);
        Quaternion quatZ = Quaternion::CreateRotationZ(rz);
        Quaternion finalQuat = quatX * quatY * quatZ;
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalRotationQuaternion, finalQuat);

        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Matrix3x3 rotateZ = Matrix3x3::CreateRotationZ(rz);
        Matrix3x3 rotateY = Matrix3x3::CreateRotationY(ry);
        Matrix3x3 rotateX = Matrix3x3::CreateRotationX(rx);
        Matrix3x3 finalRotate = rotateX * rotateY * rotateZ;

        Vector3 basisX = tm.GetBasisX();
        Vector3 expectedBasisX = finalRotate.GetBasisX();
        EXPECT_TRUE(basisX.IsClose(expectedBasisX));
        Vector3 basisY = tm.GetBasisY();
        Vector3 expectedBasisY = finalRotate.GetBasisY();
        EXPECT_TRUE(basisY.IsClose(expectedBasisY));
        Vector3 basisZ = tm.GetBasisZ();
        Vector3 expectedBasisZ = finalRotate.GetBasisZ();
        EXPECT_TRUE(basisZ.IsClose(expectedBasisZ));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetLocalRotationQuaternion_SimpleValues_Return)
    {
        float rx = 0.66f;
        float ry = 1.23f;
        float rz = 0.23f;
        Matrix3x3 rotateZ = Matrix3x3::CreateRotationZ(rz);
        Matrix3x3 rotateY = Matrix3x3::CreateRotationY(ry);
        Matrix3x3 rotateX = Matrix3x3::CreateRotationX(rx);
        Matrix3x3 finalRotate = rotateX * rotateY * rotateZ;
        Transform tm = Transform::CreateFromMatrix3x3(finalRotate);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, tm);

        Quaternion quatX = Quaternion::CreateRotationX(rx);
        Quaternion quatY = Quaternion::CreateRotationY(ry);
        Quaternion quatZ = Quaternion::CreateRotationZ(rz);
        Quaternion expectedQuat = quatX * quatY * quatZ;

        Quaternion resultQuat;
        TransformBus::EventResult(resultQuat, m_childId, &TransformBus::Events::GetLocalRotationQuaternion);

        EXPECT_TRUE(resultQuat.IsClose(expectedQuat));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, RotateAroundLocalX_SimpleValues_Set)
    {
        float rx = 1.43f;
        TransformBus::Event(m_childId, &TransformBus::Events::RotateAroundLocalX, rx);
        Vector3 localRotation;
        TransformBus::EventResult(localRotation, m_childId, &TransformBus::Events::GetLocalRotation);
        EXPECT_TRUE(localRotation.IsClose(Vector3(rx, 0.0f, 0.0f)));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, RotateAroundLocalX_RepeatCallingThisFunctionDoesNotSkewScale)
    {
        // test numeric stability
        float rx = 1.43f;
        for (int i = 0; i < 100; ++i)
        {
            TransformBus::Event(m_childId, &TransformBus::Events::RotateAroundLocalX, rx);
        }
        Vector3 localScale;
        TransformBus::EventResult(localScale, m_childId, &TransformBus::Events::GetLocalScale);
        EXPECT_TRUE(localScale.IsClose(Vector3(1.0f, 1.0f, 1.0f)));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, RotateAroundLocalY_SimpleValue_Set)
    {
        float ry = 1.43f;
        TransformBus::Event(m_childId, &TransformBus::Events::RotateAroundLocalY, ry);
        Vector3 localRotation;
        TransformBus::EventResult(localRotation, m_childId, &TransformBus::Events::GetLocalRotation);
        EXPECT_TRUE(localRotation.IsClose(Vector3(0.0f, ry, 0.0f)));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, RotateAroundLocalY_RepeatCallingThisFunctionDoesNotSkewScale)
    {
        // test numeric stability
        float ry = 1.43f;
        for (int i = 0; i < 100; ++i)
        {
            TransformBus::Event(m_childId, &TransformBus::Events::RotateAroundLocalY, ry);
        }
        Vector3 localScale;
        TransformBus::EventResult(localScale, m_childId, &TransformBus::Events::GetLocalScale);
        EXPECT_TRUE(localScale.IsClose(Vector3(1.0f, 1.0f, 1.0f)));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, RotateAroundLocalZ_SimpleValues_Set)
    {
        float rz = 1.43f;
        TransformBus::Event(m_childId, &TransformBus::Events::RotateAroundLocalZ, rz);
        Vector3 localRotation;
        TransformBus::EventResult(localRotation, m_childId, &TransformBus::Events::GetLocalRotation);
        EXPECT_TRUE(localRotation.IsClose(Vector3(0.0f, 0.0f, rz)));   
    }

    TEST_F(TransformComponentTransformMatrixSetGet, RotateAroundLocalZ_RepeatCallingThisFunctionDoesNotSkewScale)
    {
        // test numeric stability
        float rz = 1.43f;
        for (int i = 0; i < 100; ++i)
        {
            TransformBus::Event(m_childId, &TransformBus::Events::RotateAroundLocalZ, rz);
        }
        Vector3 localScale;
        TransformBus::EventResult(localScale, m_childId, &TransformBus::Events::GetLocalScale);
        EXPECT_TRUE(localScale.IsClose(Vector3(1.0f, 1.0f, 1.0f)));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalScale_SimpleValues_Set)
    {
        float sx = 42.564f;
        float sy = 12.460f;
        float sz = 28.692f;
        Vector3 expectedScales(sx, sy, sz);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalScale, expectedScales);

        Transform tm ;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 scales = tm.RetrieveScaleExact();
        EXPECT_TRUE(scales.IsClose(expectedScales));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalScaleX_SimpleValues_Set)
    {
        float sx = 64.336f;
        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 expectedScales = tm.RetrieveScaleExact();
        expectedScales.SetX(sx);

        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalScaleX, sx);

        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 scales = tm.RetrieveScaleExact();
        EXPECT_TRUE(scales.IsClose(expectedScales));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalScaleY_SimpleValues_Set)
    {
        float sy = 23.754f;
        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 expectedScales = tm.RetrieveScaleExact();
        expectedScales.SetY(sy);

        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalScaleY, sy);

        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 scales = tm.RetrieveScaleExact();
        EXPECT_TRUE(scales.IsClose(expectedScales));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, SetLocalScaleZ_SimpleValues_Set)
    {
        float sz = 65.140f;
        Transform tm;
        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 expectedScales = tm.RetrieveScaleExact();
        expectedScales.SetZ(sz);

        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalScaleZ, sz);

        TransformBus::EventResult(tm, m_childId, &TransformBus::Events::GetLocalTM);
        Vector3 scales = tm.RetrieveScaleExact();
        EXPECT_TRUE(scales.IsClose(expectedScales));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetLocalScale_SimpleValues_Return)
    {
        float sx = 43.463f;
        float sy = 346.22f;
        float sz = 863.32f;
        Vector3 expectedScales(sx, sy, sz);
        Transform scaleTM = Transform::CreateScale(expectedScales);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, scaleTM);

        Vector3 scales;
        TransformBus::EventResult(scales, m_childId, &TransformBus::Events::GetLocalScale);
        EXPECT_TRUE(scales.IsClose(expectedScales));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetWorldScale_ChildHasNoScale_ReturnScaleSameAsParent)
    {
        float sx = 43.463f;
        float sy = 346.22f;
        float sz = 863.32f;
        Vector3 expectedScales(sx, sy, sz);
        Transform scaleTM = Transform::CreateScale(expectedScales);
        TransformBus::Event(m_parentId, &TransformBus::Events::SetLocalTM, scaleTM);

        Vector3 scales;
        TransformBus::EventResult(scales, m_childId, &TransformBus::Events::GetWorldScale);
        EXPECT_TRUE(scales.IsClose(expectedScales));
    }

    TEST_F(TransformComponentTransformMatrixSetGet, GetWorldScale_ChildHasScale_ReturnCompoundScale)
    {
        float sx = 4.463f;
        float sy = 3.22f;
        float sz = 8.32f;
        Vector3 parentScales(sx, sy, sz);
        Transform parentScaleTM = Transform::CreateScale(parentScales);
        TransformBus::Event(m_parentId, &TransformBus::Events::SetLocalTM, parentScaleTM);

        float csx = 1.64f;
        float csy = 9.35f;
        float csz = 1.57f;
        Vector3 childScales(csx, csy, csz);
        Transform childScaleTM = Transform::CreateScale(childScales);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTM, childScaleTM);

        Vector3 scales;
        TransformBus::EventResult(scales, m_childId, &TransformBus::Events::GetWorldScale);
        EXPECT_TRUE(scales.IsClose(parentScales * childScales));
    }

    class TransformComponentHierarchy
        : public TransformComponentApplication
    {
    protected:
        void SetUp() override
        {
            TransformComponentApplication::SetUp();

            m_parentEntity = aznew Entity("Parent");
            m_parentId = m_parentEntity->GetId();
            m_parentEntity->Init();
            m_parentEntity->CreateComponent<TransformComponent>();

            m_childEntity = aznew Entity("Child");
            m_childId = m_childEntity->GetId();
            m_childEntity->Init();
            m_childEntity->CreateComponent<TransformComponent>();

            m_parentEntity->Activate();
            m_childEntity->Activate();
        }

        void TearDown() override
        {
            m_childEntity->Deactivate();
            m_parentEntity->Deactivate();
            delete m_childEntity;
            delete m_parentEntity;

            TransformComponentApplication::TearDown();
        }

        Entity* m_parentEntity = nullptr;
        EntityId m_parentId = EntityId();
        Entity* m_childEntity = nullptr;
        EntityId m_childId = EntityId();
    };

    TEST_F(TransformComponentHierarchy, SetParent_NormalValue_SetKeepWorldTransform)
    {
        AZ::Vector3 childLocalPos(20.45f, 46.14f, 93.65f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTranslation, childLocalPos);
        AZ::Vector3 expectedChildWorldPos = childLocalPos;

        AZ::Vector3 parentLocalPos(65.24f, 10.65, 37.87f);
        TransformBus::Event(m_parentId, &TransformBus::Events::SetLocalTranslation, parentLocalPos);

        TransformBus::Event(m_childId, &TransformBus::Events::SetParent, m_parentId);

        AZ::Vector3 childWorldPos;
        TransformBus::EventResult(childWorldPos, m_childId, &TransformBus::Events::GetWorldTranslation);
        EXPECT_TRUE(childWorldPos == expectedChildWorldPos);
    }

    TEST_F(TransformComponentHierarchy, SetParentRelative_NormalValue_SetKeepLocalTransform)
    {
        AZ::Vector3 expectedChildLocalPos(22.45f, 42.14f, 97.45f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTranslation, expectedChildLocalPos);
        AZ::Vector3 parentLocalPos(15.64f, 12.65, 29.87f);
        TransformBus::Event(m_parentId, &TransformBus::Events::SetLocalTranslation, parentLocalPos);

        TransformBus::Event(m_childId, &TransformBus::Events::SetParentRelative, m_parentId);

        AZ::Vector3 childLocalPos;
        TransformBus::EventResult(childLocalPos, m_childId, &TransformBus::Events::GetLocalTranslation);
        EXPECT_TRUE(childLocalPos == expectedChildLocalPos);
    }

    TEST_F(TransformComponentHierarchy, SetParent_Null_SetKeepWorldTransform)
    {
        AZ::Vector3 childLocalPos(28.45f, 56.14f, 43.65f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTranslation, childLocalPos);
        AZ::Vector3 parentLocalPos(85.24f, 12.65, 33.87f);
        TransformBus::Event(m_parentId, &TransformBus::Events::SetLocalTranslation, parentLocalPos);

        TransformBus::Event(m_childId, &TransformBus::Events::SetParentRelative, m_parentId);

        AZ::Vector3 expectedChildWorldPos;
        TransformBus::EventResult(expectedChildWorldPos, m_childId, &TransformBus::Events::GetWorldTranslation);

        TransformBus::Event(m_childId, &TransformBus::Events::SetParent, AZ::EntityId());

        AZ::Vector3 childWorldPos;
        TransformBus::EventResult(childWorldPos, m_childId, &TransformBus::Events::GetWorldTranslation);
        EXPECT_TRUE(childWorldPos == expectedChildWorldPos);

        // child entity doesn't have a parent now, its world position should equal its local one
        AZ::Vector3 actualChildLocalPos;
        TransformBus::EventResult(actualChildLocalPos, m_childId, &TransformBus::Events::GetLocalTranslation);
        EXPECT_TRUE(actualChildLocalPos == expectedChildWorldPos);
    }

    TEST_F(TransformComponentHierarchy, SetParentRelative_Null_SetKeepLocalTransform)
    {
        AZ::Vector3 childLocalPos(28.45f, 49.14f, 94.65f);
        TransformBus::Event(m_childId, &TransformBus::Events::SetLocalTranslation, childLocalPos);
        AZ::Vector3 parentLocalPos(66.24f, 19.65, 32.87f);
        TransformBus::Event(m_parentId, &TransformBus::Events::SetLocalTranslation, parentLocalPos);

        TransformBus::Event(m_childId, &TransformBus::Events::SetParent, m_parentId);

        AZ::Vector3 expectedChildLocalPos;
        TransformBus::EventResult(expectedChildLocalPos, m_childId, &TransformBus::Events::GetLocalTranslation);

        TransformBus::Event(m_childId, &TransformBus::Events::SetParentRelative, AZ::EntityId());

        childLocalPos;
        TransformBus::EventResult(childLocalPos, m_childId, &TransformBus::Events::GetLocalTranslation);
        EXPECT_TRUE(childLocalPos == expectedChildLocalPos);

        // child entity doesn't have a parent now, its world position should equal its local one
        AZ::Vector3 actualChildWorldPos;
        TransformBus::EventResult(actualChildWorldPos, m_childId, &TransformBus::Events::GetWorldTranslation);
        EXPECT_TRUE(actualChildWorldPos == expectedChildLocalPos);
    }

    // Fixture provides TransformComponent that is static (or not static) on an entity that has been activated.
    template<bool IsStatic>
    class StaticOrMovableTransformComponent
        : public TransformComponentApplication
    {
    protected:
        void SetUp() override
        {
            TransformComponentApplication::SetUp();

            m_entity = aznew Entity(IsStatic ? "Static Entity" : "Movable Entity");

            TransformComponentConfiguration transformConfig;
            transformConfig.m_isStatic = IsStatic;
            m_transformInterface = m_entity->CreateComponent<TransformComponent>(transformConfig);

            m_entity->Init();
            m_entity->Activate();
        }

        Entity* m_entity;
        TransformInterface* m_transformInterface = nullptr;
    };

    class MovableTransformComponent : public StaticOrMovableTransformComponent<false> {};
    class StaticTransformComponent : public StaticOrMovableTransformComponent<true> {};

    TEST_F(StaticTransformComponent, SanityCheck)
    {
        ASSERT_NE(m_entity, nullptr);
        ASSERT_NE(m_transformInterface, nullptr);
        EXPECT_EQ(m_entity->GetState(), Entity::ES_ACTIVE);
    }

    TEST_F(MovableTransformComponent, IsStaticTransform_False)
    {
        EXPECT_FALSE(m_transformInterface->IsStaticTransform());
    }

    TEST_F(StaticTransformComponent, IsStaticTransform_True)
    {
        EXPECT_TRUE(m_transformInterface->IsStaticTransform());
    }

    TEST_F(MovableTransformComponent, SetWorldTM_MovesEntity)
    {
        Transform previousTM = m_transformInterface->GetWorldTM();
        Transform nextTM = Transform::CreateTranslation(Vector3(1.f, 2.f, 3.f));
        m_transformInterface->SetWorldTM(nextTM);
        EXPECT_TRUE(m_transformInterface->GetWorldTM().IsClose(nextTM));
    }

    TEST_F(StaticTransformComponent, SetWorldTM_DoesNothing)
    {
        Transform previousTM = m_transformInterface->GetWorldTM();
        Transform nextTM = Transform::CreateTranslation(Vector3(1.f, 2.f, 3.f));
        m_transformInterface->SetWorldTM(nextTM);
        EXPECT_TRUE(m_transformInterface->GetWorldTM().IsClose(previousTM));
    }

    TEST_F(MovableTransformComponent, SetLocalTM_MovesEntity)
    {
        Transform previousTM = m_transformInterface->GetLocalTM();
        Transform nextTM = Transform::CreateTranslation(Vector3(1.f, 2.f, 3.f));
        m_transformInterface->SetLocalTM(nextTM);
        EXPECT_TRUE(m_transformInterface->GetLocalTM().IsClose(nextTM));
    }

    TEST_F(StaticTransformComponent, SetLocalTM_DoesNothing)
    {
        Transform previousTM = m_transformInterface->GetLocalTM();
        Transform nextTM = Transform::CreateTranslation(Vector3(1.f, 2.f, 3.f));
        m_transformInterface->SetLocalTM(nextTM);
        EXPECT_TRUE(m_transformInterface->GetLocalTM().IsClose(previousTM));
    }

    TEST_F(StaticTransformComponent, SetLocalTmOnDeactivatedEntity_MovesEntity)
    {
        // when static transform component is deactivated, it should allow movement
        Transform previousTM = m_transformInterface->GetLocalTM();
        m_entity->Deactivate();
        Transform nextTM = Transform::CreateTranslation(Vector3(1.f, 2.f, 3.f));
        m_transformInterface->SetLocalTM(nextTM);
        EXPECT_TRUE(m_transformInterface->GetLocalTM().IsClose(nextTM));
    }

    // Sets up a parent/child relationship between two static transform components
    class ParentedStaticTransformComponent
        : public TransformComponentApplication
    {
    protected:
        void SetUp() override
        {
            TransformComponentApplication::SetUp();

            m_parentEntity = aznew Entity("Parent");
            m_parentEntity->Init();

            TransformComponentConfiguration parentConfig;
            parentConfig.m_isStatic = true;
            parentConfig.m_transform.SetPosition(5.f, 5.f, 5.f);
            m_parentEntity->CreateComponent<TransformComponent>(parentConfig);

            m_childEntity = aznew Entity("Child");
            m_childEntity->Init();

            TransformComponentConfiguration childConfig;
            childConfig.m_isStatic = true;
            childConfig.m_transform.SetPosition(5.f, 5.f, 5.f);
            childConfig.m_parentId = m_parentEntity->GetId();
            childConfig.m_parentActivationTransformMode = TransformComponentConfiguration::ParentActivationTransformMode::MaintainOriginalRelativeTransform;
            m_childEntity->CreateComponent<TransformComponent>(childConfig);
        }

        Entity* m_parentEntity = nullptr;
        Entity* m_childEntity = nullptr;
    };

    // we do expect a static entity to move if its parent is activated after itself
    TEST_F(ParentedStaticTransformComponent, ParentActivatesLast_OffsetObeyed)
    {
        m_childEntity->Activate();

        Transform previousWorldTM;
        TransformBus::EventResult(previousWorldTM, m_childEntity->GetId(), &TransformBus::Events::GetWorldTM);

        m_parentEntity->Activate();

        Transform nextWorldTM;
        TransformBus::EventResult(nextWorldTM, m_childEntity->GetId(), &TransformBus::Events::GetWorldTM);

        EXPECT_FALSE(previousWorldTM.IsClose(nextWorldTM));
    }

    // Fixture that loads a TransformComponent from a buffer.
    // Useful for testing version converters.
    class TransformComponentVersionConverter
        : public TransformComponentApplication
    {
    public:
        void SetUp() override
        {
            TransformComponentApplication::SetUp();

            m_transformComponent.reset(AZ::Utils::LoadObjectFromBuffer<TransformComponent>(m_objectStreamBuffer, strlen(m_objectStreamBuffer) + 1));
            m_transformInterface = m_transformComponent.get();
        }

        void TearDown() override
        {
            m_transformComponent.reset();

            TransformComponentApplication::TearDown();
        }

        AZStd::unique_ptr<TransformComponent> m_transformComponent;
        TransformInterface* m_transformInterface = nullptr;
        const char* m_objectStreamBuffer = nullptr;
    };

    class TransformComponentConvertFromV2
        : public TransformComponentVersionConverter
    {
    public:
        TransformComponentConvertFromV2()
        {
            m_objectStreamBuffer = 
R"DELIMITER(<ObjectStream version="1">
    <Class name="TransformComponent" field="element" version="2" type="{22B10178-39B6-4C12-BB37-77DB45FDD3B6}">
	    <Class name="AZ::Component" field="BaseClass1" type="{EDFCB2CF-F75D-43BE-B26B-F35821B29247}">
		    <Class name="AZ::u64" field="Id" value="18023671824091307142" type="{D6597933-47CD-4FC8-B911-63F3E2B0993A}"/>
	    </Class>
	    <Class name="NetBindable" field="BaseClass2" type="{80206665-D429-4703-B42E-94434F82F381}">
		    <Class name="bool" field="m_isSyncEnabled" value="true" type="{A0CA880C-AFE4-43CB-926C-59AC48496112}"/>
	    </Class>
	    <Class name="EntityId" field="Parent" version="1" type="{6383F1D3-BB27-4E6B-A49A-6409B2059EAA}">
		    <Class name="AZ::u64" field="id" value="4294967295" type="{D6597933-47CD-4FC8-B911-63F3E2B0993A}"/>
	    </Class>
	    <Class name="Transform" field="Transform" value="1.0000000 0.0000000 0.0000000 0.0000000 1.0000000 0.0000000 0.0000000 0.0000000 1.0000000 0.0000000 0.0000000 0.0000000" type="{5D9958E9-9F1E-4985-B532-FFFDE75FEDFD}"/>
	    <Class name="Transform" field="LocalTransform" value="1.0000000 0.0000000 0.0000000 0.0000000 1.0000000 0.0000000 0.0000000 0.0000000 1.0000000 0.0000000 0.0000000 0.0000000" type="{5D9958E9-9F1E-4985-B532-FFFDE75FEDFD}"/>
	    <Class name="unsigned int" field="ParentActivationTransformMode" value="0" type="{43DA906B-7DEF-4CA8-9790-854106D3F983}"/>
    </Class>
</ObjectStream>)DELIMITER";
        }
    };

    TEST_F(TransformComponentConvertFromV2, IsStatic_False)
    {
        EXPECT_FALSE(m_transformInterface->IsStaticTransform());
    }

} // namespace UnitTest
