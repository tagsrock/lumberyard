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
#ifdef UNIT_TEST
#include "AssetProcessingStateDataUnitTests.h"
#include <AzCore/std/string/string.h>
#include <QTemporaryDir>
#include <QStringList>
#include <QString>
#include <QDir>
#include <QDebug>
#include <QSet>
#include <AzToolsFramework/API/AssetDatabaseBus.h>
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/utilities/assetUtils.h"

namespace AssetProcessingStateDataUnitTestInternal
{
    // a utility class to redirect the location the database is stored to a different location so that we don't
    // touch real data during unit tests.
    class FakeDatabaseLocationListener
        : protected AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler
    {
    public:
        FakeDatabaseLocationListener(const char* desiredLocation, const char* assetPath)
            : m_location(desiredLocation)
            , m_assetPath(assetPath)
        {
            AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler::BusConnect();
        }
        ~FakeDatabaseLocationListener()
        {
            AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler::BusDisconnect();
        }
    protected:
        // IMPLEMENTATION OF -------------- AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Listener
        bool GetAssetDatabaseLocation(AZStd::string& location) override
        {
            location = m_location;
            return true;
        }

        // ------------------------------------------------------------

    private:
        AZStd::string m_location;
        AZStd::string m_assetPath;
    };
}

// perform some operations on the state data given.  (Does not perform save and load tests)
void AssetProcessingStateDataUnitTest::DataTest(AssetProcessor::AssetDatabaseConnection* stateData)
{
    using namespace AzToolsFramework::AssetDatabase;

    ScanFolderDatabaseEntry scanFolder;
    SourceDatabaseEntry source;
    JobDatabaseEntry job;
    ProductDatabaseEntry product;

    ScanFolderDatabaseEntryContainer scanFolders;
    SourceDatabaseEntryContainer sources;
    JobDatabaseEntryContainer jobs;
    ProductDatabaseEntryContainer products;

    QString outName;
    QString outPlat;
    QString outJobDescription;

    AZ::Uuid validSourceGuid1 = AZ::Uuid::CreateRandom();
    AZ::Uuid validSourceGuid2 = AZ::Uuid::CreateRandom();
    AZ::Uuid validSourceGuid3 = AZ::Uuid::CreateRandom();
    AZ::Uuid validSourceGuid4 = AZ::Uuid::CreateRandom();
    AZ::Uuid validSourceGuid5 = AZ::Uuid::CreateRandom();
    AZ::Uuid validSourceGuid6 = AZ::Uuid::CreateRandom();

    AZ::u32 validFingerprint1 = 1;
    AZ::u32 validFingerprint2 = 2;
    AZ::u32 validFingerprint3 = 3;
    AZ::u32 validFingerprint4 = 4;
    AZ::u32 validFingerprint5 = 5;
    AZ::u32 validFingerprint6 = 6;

    AZ::Uuid validBuilderGuid1 = AZ::Uuid::CreateRandom();
    AZ::Uuid validBuilderGuid2 = AZ::Uuid::CreateRandom();
    AZ::Uuid validBuilderGuid3 = AZ::Uuid::CreateRandom();
    AZ::Uuid validBuilderGuid4 = AZ::Uuid::CreateRandom();
    AZ::Uuid validBuilderGuid5 = AZ::Uuid::CreateRandom();
    AZ::Uuid validBuilderGuid6 = AZ::Uuid::CreateRandom();

    AZ::Data::AssetType validAssetType1 = AZ::Data::AssetType::CreateRandom();
    AZ::Data::AssetType validAssetType2 = AZ::Data::AssetType::CreateRandom();
    AZ::Data::AssetType validAssetType3 = AZ::Data::AssetType::CreateRandom();
    AZ::Data::AssetType validAssetType4 = AZ::Data::AssetType::CreateRandom();
    AZ::Data::AssetType validAssetType5 = AZ::Data::AssetType::CreateRandom();
    AZ::Data::AssetType validAssetType6 = AZ::Data::AssetType::CreateRandom();

    AzToolsFramework::AssetSystem::JobStatus statusAny = AzToolsFramework::AssetSystem::JobStatus::Any;
    AzToolsFramework::AssetSystem::JobStatus statusQueued = AzToolsFramework::AssetSystem::JobStatus::Queued;
    AzToolsFramework::AssetSystem::JobStatus statusCompleted = AzToolsFramework::AssetSystem::JobStatus::Completed;

    //////////////////////////////////////////////////////////////////////////
    //ScanFolder
    //the database all starts with a scan folder since all sources are have one
    auto ScanFoldersContainScanFolderID = [](const ScanFolderDatabaseEntryContainer& scanFolders, AZ::s64 scanFolderID) -> bool
        {
            for (const auto& scanFolder : scanFolders)
            {
                if (scanFolder.m_scanFolderID == scanFolderID)
                {
                    return true;
                }
            }
            return false;
        };

    auto ScanFoldersContainScanPath = [](const ScanFolderDatabaseEntryContainer& scanFolders, const char* scanPath) -> bool
        {
            for (const auto& scanFolder : scanFolders)
            {
                if (scanFolder.m_scanFolder == scanPath)
                {
                    return true;
                }
            }
            return false;
        };
    
    auto ScanFoldersContainPortableKey = [](const ScanFolderDatabaseEntryContainer& scanFolders, const char* portableKey) -> bool
    {
        for (const auto& scanFolder : scanFolders)
        {
            if (scanFolder.m_portableKey == portableKey)
            {
                return true;
            }
        }
        return false;
    };

    //there are no scan folders yet so trying to find one should fail
    UNIT_TEST_EXPECT_FALSE(stateData->GetScanFolders(scanFolders));
    UNIT_TEST_EXPECT_FALSE(stateData->GetScanFolderByScanFolderID(0, scanFolder));
    UNIT_TEST_EXPECT_FALSE(stateData->GetScanFolderBySourceID(0, scanFolder));
    UNIT_TEST_EXPECT_FALSE(stateData->GetScanFolderByProductID(0, scanFolder));
    UNIT_TEST_EXPECT_FALSE(stateData->GetScanFolderByPortableKey("sadfsadfsadfsadfs", scanFolder));
    scanFolders.clear();

    //add a scanfolder
    scanFolder = ScanFolderDatabaseEntry("c:/lumberyard/dev", "dev", "rootportkey", "");
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(scanFolder));
    if (scanFolder.m_scanFolderID == -1)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //add the same folder again, should not add another because it already exists, so we should get the same id
    // not only that, but the path should update.
    ScanFolderDatabaseEntry dupeScanFolder = ScanFolderDatabaseEntry("c:/lumberyard/dev2", "dev", "rootportkey", "");
    dupeScanFolder.m_scanFolderID = -1;
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(dupeScanFolder));
    if (!(dupeScanFolder == scanFolder))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    UNIT_TEST_EXPECT_TRUE(dupeScanFolder.m_portableKey == scanFolder.m_portableKey);
    UNIT_TEST_EXPECT_TRUE(dupeScanFolder.m_scanFolderID == scanFolder.m_scanFolderID);

    //get all scan folders, there should only the one we added
    scanFolders.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetScanFolders(scanFolders));
    UNIT_TEST_EXPECT_TRUE(scanFolders.size() == 1);
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanPath(scanFolders, "c:/lumberyard/dev2"));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanFolderID(scanFolders, scanFolder.m_scanFolderID));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainPortableKey(scanFolders, scanFolder.m_portableKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainPortableKey(scanFolders, "rootportkey"));

    //retrieve the one we just made by id
    ScanFolderDatabaseEntry retrieveScanfolderById;
    UNIT_TEST_EXPECT_TRUE(stateData->GetScanFolderByScanFolderID(scanFolder.m_scanFolderID, retrieveScanfolderById));
    if (retrieveScanfolderById.m_scanFolderID == -1 ||
        retrieveScanfolderById.m_scanFolderID != scanFolder.m_scanFolderID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //retrieve the one we just made by portable key
    ScanFolderDatabaseEntry retrieveScanfolderByScanPath;
    UNIT_TEST_EXPECT_TRUE(stateData->GetScanFolderByPortableKey("rootportkey", retrieveScanfolderByScanPath));
    if (retrieveScanfolderByScanPath.m_scanFolderID == -1 ||
        retrieveScanfolderByScanPath.m_scanFolderID != scanFolder.m_scanFolderID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //add another folder
    ScanFolderDatabaseEntry gameScanFolderEntry("c:/lumberyard/game", "game", "gameportkey", "");
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(gameScanFolderEntry));
    if (gameScanFolderEntry.m_scanFolderID == -1 ||
        gameScanFolderEntry.m_scanFolderID == scanFolder.m_scanFolderID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //get all scan folders, there should only the two we added
    scanFolders.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetScanFolders(scanFolders));
    UNIT_TEST_EXPECT_TRUE(scanFolders.size() == 2);
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanPath(scanFolders, "c:/lumberyard/dev2"));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanPath(scanFolders, "c:/lumberyard/game"));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanFolderID(scanFolders, scanFolder.m_scanFolderID));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanFolderID(scanFolders, gameScanFolderEntry.m_scanFolderID));

    //remove the game scan folder
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveScanFolder(848475));//should return true even if it doesn't exist, false only means SQL failed
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveScanFolder(gameScanFolderEntry.m_scanFolderID));

    //get all scan folders again, there should now only the first we added
    scanFolders.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetScanFolders(scanFolders));
    UNIT_TEST_EXPECT_TRUE(scanFolders.size() == 1);
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanPath(scanFolders, "c:/lumberyard/dev2"));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanFolderID(scanFolders, scanFolder.m_scanFolderID));

    //add another folder again
    gameScanFolderEntry = ScanFolderDatabaseEntry("c:/lumberyard/game", "game", "gameportkey2", "");
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(gameScanFolderEntry));
    if (gameScanFolderEntry.m_scanFolderID == -1 ||
        gameScanFolderEntry.m_scanFolderID == scanFolder.m_scanFolderID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //get all scan folders, there should only the two we added
    scanFolders.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetScanFolders(scanFolders));
    UNIT_TEST_EXPECT_TRUE(scanFolders.size() == 2);
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanPath(scanFolders, "c:/lumberyard/dev2"));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanPath(scanFolders, "c:/lumberyard/game"));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanFolderID(scanFolders, scanFolder.m_scanFolderID));
    UNIT_TEST_EXPECT_TRUE(ScanFoldersContainScanFolderID(scanFolders, gameScanFolderEntry.m_scanFolderID));

    //remove scan folder by using a container
    ScanFolderDatabaseEntryContainer tempScanFolderDatabaseEntryContainer; // note that on clang, its illegal to call a non-const function with a temp variable container as the param
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveScanFolders(tempScanFolderDatabaseEntryContainer)); // call with empty
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveScanFolders(scanFolders));
    scanFolders.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetScanFolders(scanFolders));

    ///////////////////////////////////////////////////////////
    //setup for sources tests
    //for the rest of the test lets add the original scan folder
    scanFolder = ScanFolderDatabaseEntry("c:/lumberyard/dev", "dev", "devkey2", "");
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(scanFolder));
    ///////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    //Sources
    auto SourcesContainSourceID = [](const SourceDatabaseEntryContainer& sources, AZ::s64 sourceID) -> bool
        {
            for (const auto& source : sources)
            {
                if (source.m_sourceID == sourceID)
                {
                    return true;
                }
            }
            return false;
        };

    auto SourcesContainSourceName = [](const SourceDatabaseEntryContainer& sources, const char* sourceName) -> bool
        {
            for (const auto& source : sources)
            {
                if (source.m_sourceName == sourceName)
                {
                    return true;
                }
            }
            return false;
        };

    auto SourcesContainSourceGuid = [](const SourceDatabaseEntryContainer& sources, AZ::Uuid sourceGuid) -> bool
        {
            for (const auto& source : sources)
            {
                if (source.m_sourceGuid == sourceGuid)
                {
                    return true;
                }
            }
            return false;
        };

    //there are no sources yet so trying to find one should fail
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourceBySourceID(3443, source));
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourceBySourceGuid(AZ::Uuid::Create(), source));
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("source", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Raw, sources));
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("source", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::StartsWith, sources));
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("source", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::EndsWith, sources));
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("source", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Matches, sources));

    //trying to add a source without a valid scan folder pk should fail
    source = SourceDatabaseEntry(234234, "SomeSource1.tif", validSourceGuid1);
    {
        UnitTestUtils::AssertAbsorber absorb;
        UNIT_TEST_EXPECT_FALSE(stateData->SetSource(source));
        UNIT_TEST_EXPECT_TRUE(absorb.m_numWarningsAbsorbed > 0);
    }

    //setting a valid scan folder pk should allow it to be added
    source = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource1.tif", validSourceGuid1);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source));
    if (source.m_sourceID == -1)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - source failed to add");
        return;
    }

    //get all sources, there should only the one we added
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));

    //add the same source again, should not add another because it already exists, so we should get the same id
    SourceDatabaseEntry dupeSource(source);
    dupeSource.m_sourceID = -1;
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(dupeSource));
    if (dupeSource.m_sourceID != source.m_sourceID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //get all sources, there should still only the one we added
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));

    //add the same source again, but change the scan folder.  This should NOT add a new source - even if we don't know what the sourceID is:
    ScanFolderDatabaseEntry scanfolder2 = ScanFolderDatabaseEntry("c:/lumberyard/dev2", "dev2", "devkey3", "");
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(scanfolder2));

    SourceDatabaseEntry dupeSource2(source);
    dupeSource2.m_scanFolderPK = scanfolder2.m_scanFolderID;
    dupeSource2.m_sourceID = -1;
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(dupeSource2));
    if (dupeSource2.m_sourceID != source.m_sourceID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //get all sources, there should still only the one we added
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));

    //add the same source again, but change the scan folder back  This should NOT add a new source - this time we do know what the source ID is!
    SourceDatabaseEntry dupeSource3(source);
    dupeSource3.m_scanFolderPK = scanFolder.m_scanFolderID; // changing it back here.
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(dupeSource3));
    if (dupeSource3.m_sourceID != source.m_sourceID)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - scan folder failed to add");
        return;
    }

    //get all sources, there should still only the one we added
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));

    // remove the extra scan folder, make sure it doesn't drop the source since it should now be bound to the original scan folder agian
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveScanFolder(scanfolder2.m_scanFolderID));
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));

    //try retrieving this source by id
    SourceDatabaseEntry retrieveSourceBySourceID;
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourceBySourceID(source.m_sourceID, retrieveSourceBySourceID));
    if (retrieveSourceBySourceID.m_sourceID == -1 ||
        retrieveSourceBySourceID.m_sourceID != source.m_sourceID ||
        retrieveSourceBySourceID.m_scanFolderPK != source.m_scanFolderPK ||
        retrieveSourceBySourceID.m_sourceGuid != source.m_sourceGuid ||
        retrieveSourceBySourceID.m_sourceName != source.m_sourceName)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetSourceBySourceID failed");
        return;
    }

    //try retrieving this source by guid
    SourceDatabaseEntry retrieveSourceBySourceGuid;
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourceBySourceGuid(source.m_sourceGuid, retrieveSourceBySourceGuid));
    if (retrieveSourceBySourceGuid.m_sourceID == -1 ||
        retrieveSourceBySourceGuid.m_sourceID != source.m_sourceID ||
        retrieveSourceBySourceGuid.m_scanFolderPK != source.m_scanFolderPK ||
        retrieveSourceBySourceGuid.m_sourceGuid != source.m_sourceGuid ||
        retrieveSourceBySourceGuid.m_sourceName != source.m_sourceName)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetSourceBySourceID failed");
        return;
    }

    //try retrieving this source by source name
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("Source1.tif", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Raw, sources));
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("_SomeSource1_", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Raw, sources));
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourcesLikeSourceName("SomeSource1%", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Raw, sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourcesLikeSourceName("%SomeSource1%", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Raw, sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("Source1", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::StartsWith, sources));
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourcesLikeSourceName("Some", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::StartsWith, sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("SomeSource", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::EndsWith, sources));
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourcesLikeSourceName(".tif", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::EndsWith, sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSourcesLikeSourceName("blah", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Matches, sources));
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSourcesLikeSourceName("meSour", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Matches, sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 1);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource1.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source.m_sourceGuid));


    //remove a source
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSource(432234));//should return true even if it doesn't exist, false only if SQL failed
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSource(source.m_sourceID));

    //get all sources, there shouldn't be any
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSources(sources));

    //Add two sources then delete the via container
    SourceDatabaseEntry source2(scanFolder.m_scanFolderID, "SomeSource2.tif", validSourceGuid2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source2));
    SourceDatabaseEntry source3(scanFolder.m_scanFolderID, "SomeSource3.tif", validSourceGuid3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source3));

    //get all sources, there should only the two we added
    sources.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetSources(sources));
    UNIT_TEST_EXPECT_TRUE(sources.size() == 2);
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource2.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceName(sources, "SomeSource3.tif"));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source2.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source2.m_sourceGuid));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceID(sources, source3.m_sourceID));
    UNIT_TEST_EXPECT_TRUE(SourcesContainSourceGuid(sources, source3.m_sourceGuid));

    //Remove source via container
    SourceDatabaseEntryContainer tempSourceDatabaseEntryContainer;
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSources(tempSourceDatabaseEntryContainer)); // try it with an empty one.
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSources(sources));

    //get all sources, there should none
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSources(sources));

    //Add two sources then delete the via removing by scan folder id
    source2 = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource2.tif", validSourceGuid2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source2));
    source3 = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource3.tif", validSourceGuid3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source3));

    //remove all sources for a scan folder
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->RemoveSourcesByScanFolderID(3245532));
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSourcesByScanFolderID(scanFolder.m_scanFolderID));

    //get all sources, there should none
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSources(sources));

    //Add two sources then delete the via removing the scan folder
    source2 = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource2.tif", validSourceGuid2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source2));
    source3 = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource3.tif", validSourceGuid3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source3));

    //remove the scan folder for these sources, the sources should cascade delete
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveScanFolder(scanFolder.m_scanFolderID));

    //get all sources, there should none
    sources.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetSources(sources));

    ////////////////////////////////////////////////////////////////
    //Setup for jobs tests by having a scan folder and some sources
    //Add a scan folder
    scanFolder = ScanFolderDatabaseEntry("c:/lumberyard/dev", "dev", "devkey3", "");
    UNIT_TEST_EXPECT_TRUE(stateData->SetScanFolder(scanFolder));

    //Add some sources
    source = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource1.tif", validSourceGuid1);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source));
    source2 = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource2.tif", validSourceGuid2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source2));
    source3 = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource3.tif", validSourceGuid3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source3));
    /////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    //Jobs
    auto JobsContainJobID = [](const JobDatabaseEntryContainer& jobs, AZ::s64 jobId) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_jobID == jobId)
                {
                    return true;
                }
            }
            return false;
        };

    auto JobsContainJobKey = [](const JobDatabaseEntryContainer& jobs, const char* jobKey) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_jobKey == jobKey)
                {
                    return true;
                }
            }
            return false;
        };

    auto JobsContainFingerprint = [](const JobDatabaseEntryContainer& jobs, AZ::u32 fingerprint) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_fingerprint == fingerprint)
                {
                    return true;
                }
            }
            return false;
        };

    auto JobsContainPlatform = [](const JobDatabaseEntryContainer& jobs, const char* platform) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_platform == platform)
                {
                    return true;
                }
            }
            return false;
        };

    auto JobsContainBuilderGuid = [](const JobDatabaseEntryContainer& jobs, AZ::Uuid builderGuid) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_builderGuid == builderGuid)
                {
                    return true;
                }
            }
            return false;
        };

    auto JobsContainStatus = [](const JobDatabaseEntryContainer& jobs, AzToolsFramework::AssetSystem::JobStatus status) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_status == status)
                {
                    return true;
                }
            }
            return false;
        };


    auto JobsContainRunKey = [](const JobDatabaseEntryContainer& jobs, AZ::s64 runKey) -> bool
        {
            for (const auto& job : jobs)
            {
                if (job.m_jobRunKey == runKey)
                {
                    return true;
                }
            }
            return false;
        };


    //there are no jobs yet so trying to find one should fail
    jobs.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobs(jobs));
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobByJobID(3443, job));
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobsBySourceID(3234, jobs));
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobsBySourceName("none", jobs));

    //trying to add a job without a valid source pk should fail:
    {
        UnitTestUtils::AssertAbsorber absorber;
        job = JobDatabaseEntry(234234, "jobkey", validFingerprint1, "pc", validBuilderGuid1, statusQueued, 1);
        UNIT_TEST_EXPECT_FALSE(stateData->SetJob(job));
        UNIT_TEST_EXPECT_TRUE(absorber.m_numWarningsAbsorbed > 0);
    }

    //trying to add a job with a valid source pk but an invalid job id  should fail:
    {
        UnitTestUtils::AssertAbsorber absorb;
        job = JobDatabaseEntry(source.m_sourceID, "jobkey", validFingerprint1, "pc", validBuilderGuid1, statusQueued, 0);
        UNIT_TEST_EXPECT_FALSE(stateData->SetJob(job));
        UNIT_TEST_EXPECT_TRUE(absorb.m_numErrorsAbsorbed > 0);
    }

    //setting a valid scan folder pk should allow it to be added AND should tell you what the job ID will be.
    // the run key should be untouched.
    job = JobDatabaseEntry(source.m_sourceID, "jobKey1", validFingerprint1, "pc", validBuilderGuid1, statusQueued, 1);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job));
    UNIT_TEST_EXPECT_TRUE(job.m_jobID != -1);
    UNIT_TEST_EXPECT_TRUE(job.m_jobRunKey == 1);

    //get all jobs, there should only the one we added
    jobs.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetJobs(jobs));
    UNIT_TEST_EXPECT_TRUE(jobs.size() == 1);
    UNIT_TEST_EXPECT_TRUE(JobsContainJobID(jobs, job.m_jobID));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobKey(jobs, job.m_jobKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainFingerprint(jobs, job.m_fingerprint));
    UNIT_TEST_EXPECT_TRUE(JobsContainPlatform(jobs, job.m_platform.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainBuilderGuid(jobs, job.m_builderGuid));
    UNIT_TEST_EXPECT_TRUE(JobsContainStatus(jobs, job.m_status));
    UNIT_TEST_EXPECT_TRUE(JobsContainRunKey(jobs, job.m_jobRunKey));

    //add the same job again, should not add another because it already exists, so we should get the same id
    JobDatabaseEntry dupeJob(job);
    dupeJob.m_jobID = -1;
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(dupeJob));
    if (!(dupeJob == job))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - SetJob failed to add");
        return;
    }

    //get all jobs, there should still only the one we added
    jobs.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetJobs(jobs));
    UNIT_TEST_EXPECT_TRUE(jobs.size() == 1);
    UNIT_TEST_EXPECT_TRUE(JobsContainJobID(jobs, job.m_jobID));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobKey(jobs, job.m_jobKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainFingerprint(jobs, job.m_fingerprint));
    UNIT_TEST_EXPECT_TRUE(JobsContainPlatform(jobs, job.m_platform.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainBuilderGuid(jobs, job.m_builderGuid));
    UNIT_TEST_EXPECT_TRUE(JobsContainStatus(jobs, job.m_status));

    //try retrieving this source by id
    UNIT_TEST_EXPECT_TRUE(stateData->GetJobByJobID(job.m_jobID, job));
    if (job.m_jobID == -1 ||
        job.m_jobID != job.m_jobID ||
        job.m_sourcePK != job.m_sourcePK ||
        job.m_jobKey != job.m_jobKey ||
        job.m_fingerprint != job.m_fingerprint ||
        job.m_platform != job.m_platform)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetJobByJobID failed");
        return;
    }

    //try retrieving jobs by source id
    jobs.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetJobsBySourceID(source.m_sourceID, jobs));
    UNIT_TEST_EXPECT_TRUE(jobs.size() == 1);
    UNIT_TEST_EXPECT_TRUE(JobsContainJobID(jobs, job.m_jobID));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobKey(jobs, job.m_jobKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainFingerprint(jobs, job.m_fingerprint));
    UNIT_TEST_EXPECT_TRUE(JobsContainPlatform(jobs, job.m_platform.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainBuilderGuid(jobs, job.m_builderGuid));
    UNIT_TEST_EXPECT_TRUE(JobsContainStatus(jobs, job.m_status));

    //try retrieving jobs by source name
    jobs.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetJobsBySourceName(source.m_sourceName.c_str(), jobs));
    UNIT_TEST_EXPECT_TRUE(jobs.size() == 1);
    UNIT_TEST_EXPECT_TRUE(JobsContainJobID(jobs, job.m_jobID));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobKey(jobs, job.m_jobKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainFingerprint(jobs, job.m_fingerprint));
    UNIT_TEST_EXPECT_TRUE(JobsContainPlatform(jobs, job.m_platform.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainBuilderGuid(jobs, job.m_builderGuid));
    UNIT_TEST_EXPECT_TRUE(JobsContainStatus(jobs, job.m_status));

    //remove a job
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveJob(432234));
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveJob(job.m_jobID));

    //get all jobs, there shouldn't be any
    jobs.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobs(jobs));

    //Add two jobs then delete the via container
    JobDatabaseEntry job2(source2.m_sourceID, "jobkey2", validFingerprint2, "pc", validBuilderGuid2, statusQueued, 2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job2));
    JobDatabaseEntry job3(source3.m_sourceID, "jobkey3", validFingerprint3, "pc", validBuilderGuid3, statusQueued, 3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job3));

    //get all jobs, there should be 3
    jobs.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetJobs(jobs));
    UNIT_TEST_EXPECT_TRUE(jobs.size() == 2);
    UNIT_TEST_EXPECT_TRUE(JobsContainJobID(jobs, job2.m_jobID));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobKey(jobs, job2.m_jobKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainFingerprint(jobs, job2.m_fingerprint));
    UNIT_TEST_EXPECT_TRUE(JobsContainPlatform(jobs, job2.m_platform.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainBuilderGuid(jobs, job2.m_builderGuid));
    UNIT_TEST_EXPECT_TRUE(JobsContainStatus(jobs, job2.m_status));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobID(jobs, job3.m_jobID));
    UNIT_TEST_EXPECT_TRUE(JobsContainJobKey(jobs, job3.m_jobKey.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainFingerprint(jobs, job3.m_fingerprint));
    UNIT_TEST_EXPECT_TRUE(JobsContainPlatform(jobs, job3.m_platform.c_str()));
    UNIT_TEST_EXPECT_TRUE(JobsContainBuilderGuid(jobs, job3.m_builderGuid));
    UNIT_TEST_EXPECT_TRUE(JobsContainStatus(jobs, job3.m_status));

    //Remove job via container
    JobDatabaseEntryContainer tempJobDatabaseEntryContainer;
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveJobs(tempJobDatabaseEntryContainer)); // make sure it works on an empty container.
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveJobs(jobs));

    //get all jobs, there should none
    jobs.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobs(jobs));

    //Add two jobs then delete the via removing by source
    job2 = JobDatabaseEntry(source.m_sourceID, "jobkey2", validFingerprint2, "pc", validBuilderGuid2, statusQueued, 4);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job2));
    job3 = JobDatabaseEntry(source.m_sourceID, "jobkey3", validFingerprint3, "pc", validBuilderGuid3, statusQueued, 5);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job3));

    //remove the scan folder for these jobs, the jobs should cascade delete
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSource(source.m_sourceID));

    //get all jobs, there should none
    jobs.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetJobs(jobs));

    ////////////////////////////////////////////////////////////////
    //Setup for product tests by having a some sources and jobs
    source = SourceDatabaseEntry(scanFolder.m_scanFolderID, "SomeSource1.tif", validSourceGuid1);
    UNIT_TEST_EXPECT_TRUE(stateData->SetSource(source));

    //Add jobs
    job = JobDatabaseEntry(source.m_sourceID, "jobkey1", validFingerprint1, "pc", validBuilderGuid1, statusCompleted, 6);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job));
    job2 = JobDatabaseEntry(source.m_sourceID, "jobkey2", validFingerprint2, "pc", validBuilderGuid2, statusCompleted, 7);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job2));
    job3 = JobDatabaseEntry(source.m_sourceID, "jobkey3", validFingerprint3, "pc", validBuilderGuid3, statusCompleted, 8);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job3));
    /////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    //products
    auto ProductsContainProductID = [](const ProductDatabaseEntryContainer& products, AZ::s64 productId) -> bool
        {
            for (const auto& product : products)
            {
                if (product.m_productID == productId)
                {
                    return true;
                }
            }
            return false;
        };

    auto ProductsContainProductSubID = [](const ProductDatabaseEntryContainer& products, AZ::u32 subid) -> bool
        {
            for (const auto& product : products)
            {
                if (product.m_subID == subid)
                {
                    return true;
                }
            }
            return false;
        };

    auto ProductsContainProductName = [](const ProductDatabaseEntryContainer& products, const char* productName) -> bool
        {
            for (const auto& product : products)
            {
                if (product.m_productName == productName)
                {
                    return true;
                }
            }
            return false;
        };

    auto ProductsContainAssetType = [](const ProductDatabaseEntryContainer& products, AZ::Data::AssetType assetType) -> bool
        {
            for (const auto& product : products)
            {
                if (product.m_assetType == assetType)
                {
                    return true;
                }
            }
            return false;
        };

    //there are no products yet so trying to find one should fail
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductByProductID(3443, product));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsLikeProductName("none", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Raw, products));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsLikeProductName("none", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::StartsWith, products));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsLikeProductName("none", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::EndsWith, products));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsLikeProductName("none", AzToolsFramework::AssetDatabase::AssetDatabaseConnection::Matches, products));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsBySourceID(25654, products));
    UNIT_TEST_EXPECT_FALSE(stateData->GetProductsBySourceName("none", products));

    //trying to add a product without a valid job pk should fail
    product = ProductDatabaseEntry(234234, 1, "SomeProduct1.dds", validAssetType1);
    {
        UnitTestUtils::AssertAbsorber absorber;
        UNIT_TEST_EXPECT_FALSE(stateData->SetProduct(product));
        UNIT_TEST_EXPECT_TRUE(absorber.m_numWarningsAbsorbed > 0);
    }

    //setting a valid scan folder pk should allow it to be added
    product = ProductDatabaseEntry(job.m_jobID, 1, "SomeProduct1.dds", validAssetType1);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product));
    if (product.m_productID == -1)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - SetProduct failed to add");
        return;
    }

    //get all products, there should only the one we added
    products.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProducts(products));
    UNIT_TEST_EXPECT_TRUE(products.size() == 1);
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductID(products, product.m_productID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductSubID(products, product.m_subID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductName(products, product.m_productName.c_str()));
    UNIT_TEST_EXPECT_TRUE(ProductsContainAssetType(products, product.m_assetType));

    //add the same product again, should not add another because it already exists, so we should get the same id
    ProductDatabaseEntry dupeProduct(product);
    dupeProduct.m_productID = -1;
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(dupeProduct));
    if (!(dupeProduct == product))
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - SetProduct failed to add");
        return;
    }

    //get all products, there should still only the one we added
    products.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProducts(products));
    UNIT_TEST_EXPECT_TRUE(products.size() == 1);
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductID(products, product.m_productID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductSubID(products, product.m_subID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductName(products, product.m_productName.c_str()));
    UNIT_TEST_EXPECT_TRUE(ProductsContainAssetType(products, product.m_assetType));

    //try retrieving this source by id
    ProductDatabaseEntry retrievedProduct;
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductByProductID(product.m_productID, retrievedProduct));
    if (retrievedProduct.m_productID == -1 ||
        retrievedProduct.m_productID != product.m_productID ||
        retrievedProduct.m_jobPK != product.m_jobPK ||
        retrievedProduct.m_subID != product.m_subID ||
        retrievedProduct.m_productName != product.m_productName ||
        retrievedProduct.m_assetType != product.m_assetType)
    {
        Q_EMIT UnitTestFailed("AssetProcessingStateDataTest Failed - GetProductByProductID failed");
        return;
    }

    //try retrieving products by source id
    products.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsBySourceID(source.m_sourceID, products));
    UNIT_TEST_EXPECT_TRUE(products.size() == 1);
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductID(products, product.m_productID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductSubID(products, product.m_subID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductName(products, product.m_productName.c_str()));
    UNIT_TEST_EXPECT_TRUE(ProductsContainAssetType(products, product.m_assetType));

    //try retrieving products by source name
    products.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProductsBySourceName(source.m_sourceName.c_str(), products));
    UNIT_TEST_EXPECT_TRUE(products.size() == 1);
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductID(products, product.m_productID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductSubID(products, product.m_subID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductName(products, product.m_productName.c_str()));
    UNIT_TEST_EXPECT_TRUE(ProductsContainAssetType(products, product.m_assetType));

    //remove a product
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProduct(432234));
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProduct(product.m_productID));

    //get all products, there shouldn't be any
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));

    //Add two products then delete the via container
    ProductDatabaseEntry product2(job.m_jobID, 2, "SomeProduct2.dds", validAssetType2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product2));
    ProductDatabaseEntry product3(job.m_jobID, 3, "SomeProduct3.dds", validAssetType3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product3));

    //get all products, there should be 3
    products.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->GetProducts(products));
    UNIT_TEST_EXPECT_TRUE(products.size() == 2);
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductID(products, product2.m_productID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductSubID(products, product2.m_subID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductName(products, product2.m_productName.c_str()));
    UNIT_TEST_EXPECT_TRUE(ProductsContainAssetType(products, product2.m_assetType));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductID(products, product3.m_productID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductSubID(products, product3.m_subID));
    UNIT_TEST_EXPECT_TRUE(ProductsContainProductName(products, product3.m_productName.c_str()));
    UNIT_TEST_EXPECT_TRUE(ProductsContainAssetType(products, product3.m_assetType));

    //Remove product via container
    ProductDatabaseEntryContainer tempProductDatabaseEntryContainer;
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProducts(tempProductDatabaseEntryContainer)); // test in the empty case
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProducts(products));

    //get all products, there should none
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));

    //Add two products then delete the via removing by source id
    product2 = ProductDatabaseEntry(job.m_jobID, 2, "SomeProduct2.dds", validAssetType2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product2));
    product3 = ProductDatabaseEntry(job.m_jobID, 3, "SomeProduct3.dds", validAssetType3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product3));

    //remove all products for a job id
    products.clear();
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProductsByJobID(3245532));
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProductsByJobID(job.m_jobID));

    //get all products, there should none
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));

    //Add two products then delete the via removing by source
    product2 = ProductDatabaseEntry(job.m_jobID, 2, "SomeProduct2.dds", validAssetType2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product2));
    product3 = ProductDatabaseEntry(job.m_jobID, 3, "SomeProduct3.dds", validAssetType3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product3));

    //remove all product for source id
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProductsBySourceID(3245532));
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveProductsBySourceID(source.m_sourceID));

    //get all products, there should none
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));

    //Add two products then delete the via removing the job
    product2 = ProductDatabaseEntry(job.m_jobID, 2, "SomeProduct2.dds", validAssetType2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product2));
    product3 = ProductDatabaseEntry(job.m_jobID, 3, "SomeProduct3.dds", validAssetType3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product3));

    //the products should cascade delete
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveJob(job.m_jobID));

    //get all products, there should none
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));

    //Add jobs
    job = JobDatabaseEntry(source.m_sourceID, "jobkey1", validFingerprint1, "pc", validBuilderGuid1, statusCompleted, 9);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job));
    job2 = JobDatabaseEntry(source.m_sourceID, "jobkey2", validFingerprint2, "pc", validBuilderGuid2, statusCompleted, 10);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job2));
    job3 = JobDatabaseEntry(source.m_sourceID, "jobkey3", validFingerprint3, "pc", validBuilderGuid3, statusCompleted, 11);
    UNIT_TEST_EXPECT_TRUE(stateData->SetJob(job3));

    //Add two products then delete the via removing the job
    product2 = ProductDatabaseEntry(job.m_jobID, 2, "SomeProduct2.dds", validAssetType2);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product2));
    product3 = ProductDatabaseEntry(job.m_jobID, 3, "SomeProduct3.dds", validAssetType3);
    UNIT_TEST_EXPECT_TRUE(stateData->SetProduct(product3));

    //the products should cascade delete
    UNIT_TEST_EXPECT_TRUE(stateData->RemoveSource(source.m_sourceID));

    //get all products, there should none
    products.clear();
    UNIT_TEST_EXPECT_FALSE(stateData->GetProducts(products));
}

void AssetProcessingStateDataUnitTest::ExistenceTest(AssetProcessor::AssetDatabaseConnection* stateData)
{
    UNIT_TEST_EXPECT_FALSE(stateData->DataExists());
    stateData->ClearData(); // this is expected to initialize a database.
    UNIT_TEST_EXPECT_TRUE(stateData->DataExists());
}

void AssetProcessingStateDataUnitTest::AssetProcessingStateDataTest()
{
    using namespace AssetProcessingStateDataUnitTestInternal;
    using namespace AzToolsFramework::AssetDatabase;

    ProductDatabaseEntryContainer products;
    QTemporaryDir tempDir;
    QDir dirPath(tempDir.path());

    bool testsFailed = false;
    connect(this, &UnitTestRun::UnitTestFailed, this, [&testsFailed]()
        {
            testsFailed = true;
        }, Qt::DirectConnection);

    // now test the SQLite version of the database on its own.
    {
        FakeDatabaseLocationListener listener(dirPath.filePath("statedatabase.sqlite").toUtf8().constData(), "displayString");
        AssetProcessor::AssetDatabaseConnection connection;

        ExistenceTest(&connection);
        if (testsFailed)
        {
            return;
        }

        DataTest(&connection);
        if (testsFailed)
        {
            return;
        }
    }

    Q_EMIT UnitTestPassed();
}

void AssetProcessingStateDataUnitTest::StartTest()
{
    AssetProcessingStateDataTest();
}

REGISTER_UNIT_TEST(AssetProcessingStateDataUnitTest)

#include <native/unittests/AssetProcessingStateDataUnitTests.moc>

#endif //UNIT_TEST
