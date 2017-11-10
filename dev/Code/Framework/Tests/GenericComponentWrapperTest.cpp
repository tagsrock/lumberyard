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
#include <AzTest/AzTest.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Serialization/Utils.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/Application/ToolsApplication.h>

// Test that editor-components wrapped within a GenericComponentWrapper
// are moved out of the wrapper when a slice is loaded.
const char kWrappedEditorComponent[] =
R"DELIMITER(<ObjectStream version="1">
    <Class name="SliceComponent" field="element" version="1" type="{AFD304E4-1773-47C8-855A-8B622398934F}">
        <Class name="AZ::Component" field="BaseClass1" type="{EDFCB2CF-F75D-43BE-B26B-F35821B29247}">
            <Class name="AZ::u64" field="Id" value="7737200995084371546" type="{D6597933-47CD-4FC8-B911-63F3E2B0993A}"/>
        </Class>
        <Class name="AZStd::vector" field="Entities" type="{2BADE35A-6F1B-4698-B2BC-3373D010020C}">
            <Class name="AZ::Entity" field="element" version="2" type="{75651658-8663-478D-9090-2432DFCAFA44}">
                <Class name="EntityId" field="Id" version="1" type="{6383F1D3-BB27-4E6B-A49A-6409B2059EAA}">
                    <Class name="AZ::u64" field="id" value="16119032733109672753" type="{D6597933-47CD-4FC8-B911-63F3E2B0993A}"/>
                </Class>
                <Class name="AZStd::string" field="Name" value="RigidPhysicsMesh" type="{EF8FF807-DDEE-4EB0-B678-4CA3A2C490A4}"/>
                <Class name="bool" field="IsDependencyReady" value="true" type="{A0CA880C-AFE4-43CB-926C-59AC48496112}"/>
                <Class name="AZStd::vector" field="Components" type="{2BADE35A-6F1B-4698-B2BC-3373D010020C}">
                    <Class name="GenericComponentWrapper" field="element" type="{68D358CA-89B9-4730-8BA6-E181DEA28FDE}">
                        <Class name="EditorComponentBase" field="BaseClass1" version="1" type="{D5346BD4-7F20-444E-B370-327ACD03D4A0}">
                            <Class name="AZ::Component" field="BaseClass1" type="{EDFCB2CF-F75D-43BE-B26B-F35821B29247}">
                                <Class name="AZ::u64" field="Id" value="11874523501682509824" type="{D6597933-47CD-4FC8-B911-63F3E2B0993A}"/>
                            </Class>
                        </Class>
                        <Class name="SelectionComponent" field="m_template" type="{73B724FC-43D1-4C75-ACF5-79AA8A3BF89D}">
                            <Class name="AZ::Component" field="BaseClass1" type="{EDFCB2CF-F75D-43BE-B26B-F35821B29247}">
                                <Class name="AZ::u64" field="Id" value="0" type="{D6597933-47CD-4FC8-B911-63F3E2B0993A}"/>
                            </Class>
                        </Class>
                    </Class>
                </Class>
            </Class>
        </Class>
        <Class name="AZStd::list" field="Prefabs" type="{B845AD64-B5A0-4CCD-A86B-3477A36779BE}"/>
        <Class name="bool" field="IsDynamic" value="false" type="{A0CA880C-AFE4-43CB-926C-59AC48496112}"/>
    </Class>
</ObjectStream>)DELIMITER";

class WrappedEditorComponentTest
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_app.Start(AZ::ComponentApplication::Descriptor());

        m_slice.reset(AZ::Utils::LoadObjectFromBuffer<AZ::SliceComponent>(kWrappedEditorComponent, strlen(kWrappedEditorComponent) + 1));
        if (m_slice)
        {
            if (m_slice->GetNewEntities().size() > 0)
            {
                m_entityFromSlice = m_slice->GetNewEntities()[0];
                if (m_entityFromSlice)
                {
                    if (m_entityFromSlice->GetComponents().size() > 0)
                    {
                        m_componentFromSlice = m_entityFromSlice->GetComponents()[0];
                    }
                }
            }
        }
    }

    void TearDown() override
    {
        m_slice.reset();

        m_app.Stop();
    }

    AzToolsFramework::ToolsApplication m_app;
    AZStd::unique_ptr<AZ::SliceComponent> m_slice;
    AZ::Entity* m_entityFromSlice = nullptr;
    AZ::Component* m_componentFromSlice = nullptr;
};

TEST_F(WrappedEditorComponentTest, Slice_Loaded)
{
    EXPECT_NE(m_slice.get(), nullptr);
}

TEST_F(WrappedEditorComponentTest, EntityFromSlice_Exists)
{
    EXPECT_NE(m_entityFromSlice, nullptr);
}

TEST_F(WrappedEditorComponentTest, ComponentFromSlice_Exists)
{
    EXPECT_NE(m_componentFromSlice, nullptr);
}

TEST_F(WrappedEditorComponentTest, Component_IsNotGenericComponentWrapper)
{
    EXPECT_EQ(azrtti_cast<AzToolsFramework::Components::GenericComponentWrapper*>(m_componentFromSlice), nullptr);
}

// The swapped component should have adopted the GenericComponentWrapper's ComponentId.
TEST_F(WrappedEditorComponentTest, ComponentId_MatchesWrapperId)
{
    EXPECT_EQ(m_componentFromSlice->GetId(), 11874523501682509824);
}