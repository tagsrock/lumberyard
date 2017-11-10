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


#include "AssetDatabase.h"
#include <QMetaType>
#include <QString>
#include <QSet>
#include <AzCore/Math/Uuid.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/IO/SystemFile.h>
#include <AzToolsFramework/SQLite/SQLiteConnection.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/std/algorithm.h>
#include "native/assetprocessor.h"
#include <AzFramework/StringFunc/StringFunc.h>
#include <QFileInfo>

namespace AssetProcessor
{
    using namespace AzToolsFramework;
    using namespace AzToolsFramework::AssetSystem;
    using namespace AzToolsFramework::AssetDatabase;
    using namespace AzToolsFramework::SQLite;
    
    //tack on the namespace to avoid statement name collisions
    namespace
    {
        static const char* LOG_NAME = "AssetProcessor";

        //////////////////////////////////////////////////////////////////////////
        //tables
        static const char* CREATE_DATABASE_INFOTABLE = "AssetProcessor::CreateDatabaseInfoTable";
        static const char* CREATE_DATABASE_INFOTABLE_STATEMENT =
            "CREATE TABLE IF NOT EXISTS dbinfo( "
            "    rowID   INTEGER PRIMARY KEY, "
            "    version INTEGER NOT NULL);";

        static const char* CREATE_SCANFOLDERS_TABLE = "AssetProcessor::CreateScanFoldersTable";
        static const char* CREATE_SCANFOLDERS_TABLE_STATEMENT =
            "CREATE TABLE IF NOT EXISTS ScanFolders( "
            "   ScanFolderID    INTEGER PRIMARY KEY AUTOINCREMENT, "
            "   ScanFolder      TEXT NOT NULL collate nocase, "
            "   DisplayName     TEXT NOT NULL collate nocase, "
            "   PortableKey     TEXT NOT NULL collate nocase, "
            "   OutputPrefix    TEXT NOT NULL collate nocase, "
            "   IsRoot          INTEGER NOT NULL);";

        static const char* CREATE_SOURCES_TABLE = "AssetProcessor::CreateSourceTable";
        static const char* CREATE_SOURCES_TABLE_STATEMENT =
            "CREATE TABLE IF NOT EXISTS Sources("
            "    SourceID         INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    ScanFolderPK     INTEGER NOT NULL, "
            "    SourceName       TEXT NOT NULL collate nocase, "
            "    SourceGuid       BLOB NOT NULL, "
            "    FOREIGN KEY (ScanFolderPK) REFERENCES "
            "       ScanFolders(ScanFolderID) ON DELETE CASCADE);";

        static const char* CREATE_JOBS_TABLE = "AssetProcessor::CreateJobsTable";
        static const char* CREATE_JOBS_TABLE_STATEMENT =
            "CREATE TABLE IF NOT EXISTS Jobs("
            "    JobID            INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    SourcePK         INTEGER NOT NULL, "
            "    JobKey           TEXT NOT NULL collate nocase, "
            "    Fingerprint      INTEGER NOT NULL, "
            "    Platform         TEXT NOT NULL collate nocase, "
            "    BuilderGuid      BLOB NOT NULL, "
            "    Status           INTEGER NOT NULL, "
            "    JobRunKey        INTEGER NOT NULL, "
            "    FirstFailLogTime INTEGER NOT NULL, "
            "    FirstFailLogFile TEXT collate nocase, "
            "    LastFailLogTime  INTEGER NOT NULL, "
            "    LastFailLogFile  TEXT collate nocase, "
            "    LastLogTime      INTEGER NOT NULL, "
            "    LastLogFile      TEXT collate nocase, "
            "    FOREIGN KEY (SourcePK) REFERENCES "
            "       Sources(SourceID) ON DELETE CASCADE);";

        static const char* CREATEINDEX_JOBS_JOBRUNKEY = "AssetProcesser::CreateIndexJobsJobRunKey";
        static const char* CREATEINDEX_JOBS_JOBRUNKEY_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Jobs_JobRunKey ON Jobs (JobRunKey);";

        static const char* CREATEINDEX_JOBS_JOBKEY = "AssetProcesser::CreateIndexJobsJobKey";
        static const char* CREATEINDEX_JOBS_JOBKEY_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Jobs_JobKey ON Jobs (JobKey);";

        static const char* CREATE_PRODUCT_TABLE = "AssetProcessor::CreateProductTable";
        static const char* CREATE_PRODUCT_TABLE_STATEMENT =
            "CREATE TABLE IF NOT EXISTS Products( "
            "    ProductID      INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    JobPK          INTEGER NOT NULL, "
            "    ProductName    TEXT NOT NULL collate nocase, "
            "    SubID          INTEGER NOT NULL, "
            "    AssetType      BLOB NOT NULL, "
            "    LegacyGuid     BLOB NOT NULL, "
            "    FOREIGN KEY (JobPK) REFERENCES "
            "       Jobs(JobID) ON DELETE CASCADE);";

        static const char* CREATE_SOURCE_DEPENDENCY_TABLE = "AssetProcessor::CreateSourceDependencyTable";
        static const char* CREATE_SOURCE_DEPENDENCY_TABLE_STATEMENT =
            "CREATE TABLE IF NOT EXISTS SourceDependency("
            "    SourceDependencyID            INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    BuilderGuid                   BLOB NOT NULL, "
            "    Source                        TEXT NOT NULL collate nocase, "
            "    DependsOnSource               TEXT NOT NULL collate nocase); ";

        //////////////////////////////////////////////////////////////////////////
        //indices
        static const char* CREATEINDEX_DEPENDSONSOURCE_SOURCEDEPENDENCY = "AssetProcesser::CreateIndexDependsOnSource_SourceDependency";
        static const char* CREATEINDEX_DEPENDSONSOURCE_SOURCEDEPENDENCY_STATEMENT =
            "CREATE INDEX IF NOT EXISTS DependsOnSource_SourceDependency ON SourceDependency (DependsOnSource);";
        static const char* CREATEINDEX_BUILDERGUID_SOURCE_SOURCEDEPENDENCY = "AssetProcesser::CreateIndexBuilderGuid_Source_SourceDependency";
        static const char* CREATEINDEX_BUILDERGUID_SOURCE_SOURCEDEPENDENCY_STATEMENT =
            "CREATE INDEX IF NOT EXISTS BuilderGuid_Source_SourceDependency ON SourceDependency (BuilderGuid, Source);";
        static const char* CREATEINDEX_SCANFOLDERS_SOURCES = "AssetProcesser::CreateIndexScanFoldersSources";
        static const char* CREATEINDEX_SCANFOLDERS_SOURCES_STATEMENT =
            "CREATE INDEX IF NOT EXISTS ScanFolders_Sources ON Sources (ScanFolderPK);";
        static const char* DROPINDEX_SCANFOLDERS_SOURCES_STATEMENT =
            "DROP INDEX IF EXISTS ScanFolders_Sources_idx;";

        static const char* CREATEINDEX_SCANFOLDERS_SOURCES_SCANFOLDER = "AssetProcesser::CreateIndexScanFoldersSourcesScanFolder";
        static const char* CREATEINDEX_SCANFOLDERS_SOURCES_SCANFOLDER_STATEMENT =
            "CREATE INDEX IF NOT EXISTS IdxSources_SourceAndScanFolder ON Sources (ScanFolderPK, SourceName);";

        static const char* CREATEINDEX_SOURCES_JOBS = "AssetProcesser::CreateIndexSourcesJobs";
        static const char* CREATEINDEX_SOURCES_JOBS_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Sources_Jobs ON Jobs (SourcePK);";
        static const char* DROPINDEX_SOURCES_JOBS_STATEMENT =
            "DROP INDEX IF EXISTS Sources_Jobs_idx;";

        static const char* CREATEINDEX_JOBS_PRODUCTS = "AssetProcesser::CreateIndexJobsProducts";
        static const char* CREATEINDEX_JOBS_PRODUCTS_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Jobs_Products ON Products (JobPK);";
        static const char* DROPINDEX_JOBS_PRODUCTS_STATEMENT =
            "DROP INDEX IF EXISTS Jobs_Products_idx;";

        static const char* CREATEINDEX_SOURCE_NAME = "AssetProcessor::CreateIndexSourceName";
        static const char* CREATEINDEX_SOURCE_NAME_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Sources_SourceName ON Sources (SourceName);";
        static const char* DROPINDEX_SOURCE_NAME_STATEMENT =
            "DROP INDEX IF EXISTS Sources_SourceName_idx;";

        static const char* CREATEINDEX_SOURCE_GUID = "AssetProcessor::CreateIndexSourceGuid";
        static const char* CREATEINDEX_SOURCE_GUID_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Sources_SourceGuid ON Sources (SourceGuid);";

        static const char* CREATEINDEX_PRODUCT_NAME = "AssetProcessor::CreateIndexProductName";
        static const char* CREATEINDEX_PRODUCT_NAME_STATEMENT =
            "CREATE INDEX IF NOT EXISTS Products_ProductName ON Products (ProductName);";
        static const char* DROPINDEX_PRODUCT_NAME_STATEMENT =
            "DROP INDEX IF EXISTS Products_ProductName_idx;";

        //////////////////////////////////////////////////////////////////////////
        //insert/set/update/delete
        static const char* SET_DATABASE_VERSION = "AssetProcessor::SetDatabaseVersion";
        static const char* SET_DATABASE_VERSION_STATEMENT =
            "INSERT OR REPLACE INTO dbinfo(rowID, version) "
            "VALUES (1, :ver);";

        static const char* INSERT_SCANFOLDER = "AssetProcessor::InsertScanFolder";
        static const char* INSERT_SCANFOLDER_STATEMENT =
            "INSERT INTO ScanFolders (ScanFolder, DisplayName, PortableKey, OutputPrefix, IsRoot) "
            "VALUES (:scanfolder, :displayname, :portablekey, :outputprefix, :isroot);";

        static const char* UPDATE_SCANFOLDER = "AssetProcessor::UpdateScanFolder";
        static const char* UPDATE_SCANFOLDER_STATEMENT =
            "UPDATE ScanFolders SET "
                "ScanFolder =   :scanfolder, "
                "DisplayName =  :displayname, "
                "PortableKey =  :portablekey, "
                "OutputPrefix = :outputprefix, "
                "IsRoot = :isroot "
            "WHERE "
                "ScanFolderID = :scanfolderid;";

        static const char* DELETE_SCANFOLDER = "AssetProcessor::RemoveScanFolder";
        static const char* DELETE_SCANFOLDER_STATEMENT =
            "DELETE FROM ScanFolders WHERE "
            "(ScanFolderID = :scanfolderid);";

        static const char* INSERT_SOURCE = "AssetProcessor::InsertSource";
        static const char* INSERT_SOURCE_STATEMENT =
            "INSERT INTO Sources (ScanFolderPK, SourceName, SourceGuid) "
            "VALUES (:scanfolderid, :sourcename, :sourceguid);";

        static const char* UPDATE_SOURCE = "AssetProcessor::UpdateSource";
        static const char* UPDATE_SOURCE_STATEMENT =
            "UPDATE Sources SET "
            "ScanFolderPK = :scanfolderpk, "
            "SourceName = :sourcename, "
            "SourceGuid = :sourceguid WHERE "
            "SourceID = :sourceid;";

        static const char* DELETE_SOURCE = "AssetProcessor::DeleteSource";
        static const char* DELETE_SOURCE_STATEMENT =
            "DELETE FROM Sources WHERE "
            "SourceID = :sourceid;";

        static const char* DELETE_SOURCE_BY_SCANFOLDERID = "AssetProcessor::DeleteSourceByScanFolderID";
        static const char* DELETE_SOURCE_BY_SCANFOLDERID_STATEMENT =
            "DELETE FROM Sources WHERE "
            "ScanFolderPK = :scanfolderid;";

        static const char* GET_HIGHEST_JOBRUNKEY = "AssetProcessor::GetHighestJobRunKey";
        static const char* GET_HIGHEST_JOBRUNKEY_STATEMENT =
            "SELECT JobRunKey FROM Jobs ORDER BY JobRunKey DESC LIMIT 1";

        static const char* INSERT_JOB = "AssetProcessor::InsertJob";
        static const char* INSERT_JOB_STATEMENT =
            "INSERT INTO Jobs (SourcePK, JobKey, Fingerprint, Platform, BuilderGuid, Status, JobRunKey, FirstFailLogTime, FirstFailLogFile, LastFailLogTime, LastFailLogFile, LastLogTime, LastLogFile) "
            "VALUES (:sourceid, :jobkey, :fingerprint, :platform, :builderguid, :status, :jobrunkey, :firstfaillogtime, :firstfaillogfile, :lastfaillogtime, :lastfaillogfile, :lastlogtime, :lastlogfile);";

        static const char* UPDATE_JOB = "AssetProcessor::UpdateJob";
        static const char* UPDATE_JOB_STATEMENT =
            "UPDATE Jobs SET "
            "SourcePK = :sourceid, "
            "JobKey = :jobkey, "
            "Fingerprint = :fingerprint, "
            "Platform = :platform, "
            "BuilderGuid = :builderguid, "
            "Status = :status, "
            "JobRunKey = :jobrunkey, "
            "FirstFailLogTime = :firstfaillogtime, "
            "FirstFailLogFile = :firstfaillogfile, "
            "LastFailLogTime = :lastfaillogtime, "
            "LastFailLogFile = :lastfaillogfile, "
            "LastLogTime = :lastlogtime, "
            "LastLogFile = :lastlogfile WHERE "
            "JobID = :jobid;";

        static const char* DELETE_JOB = "AssetProcessor::DeleteJob";
        static const char* DELETE_JOB_STATEMENT =
            "DELETE FROM Jobs WHERE "
            "JobID = :jobid;";

        static const char* INSERT_PRODUCT = "AssetProcessor::InsertProduct";
        static const char* INSERT_PRODUCT_STATEMENT =
            "INSERT INTO Products (JobPK, SubID, ProductName, AssetType, LegacyGuid) "
            "VALUES (:jobid, :subid, :productname, :assettype, :legacyguid);";

        static const char* UPDATE_PRODUCT = "AssetProcessor::UpdateProduct";
        static const char* UPDATE_PRODUCT_STATEMENT =
            "UPDATE Products SET "
            "JobPK = :jobid, "
            "SubID = :subid, "
            "ProductName = :productname, "
            "AssetType = :assetttype, "
            "LegacyGuid = :legacyguid WHERE "
            "ProductID = :productid;";

        static const char* DELETE_PRODUCT = "AssetProcessor::DeleteProduct";
        static const char* DELETE_PRODUCT_STATEMENT =
            "DELETE FROM Products WHERE "
            "ProductID = :productid;";

        static const char* DELETE_PRODUCTS_BY_JOBID = "AssetProcessor::DeleteAllProductsByJobID";
        static const char* DELETE_PRODUCTS_BY_JOBID_STATEMENT =
            "DELETE FROM Products WHERE "
            "JobPK = :jobid;";

        static const char* DELETE_PRODUCTS_BY_SOURCEID = "AssetProcessor::DeleteAllProductsBySourceID";
        static const char* DELETE_PRODUCTS_BY_SOURCEID_STATEMENT =
            "DELETE FROM Products "
            "WHERE EXISTS "
            "(SELECT * FROM Jobs WHERE "
            "Products.JobPK = Jobs.JobID AND "
            "Jobs.SourcePK = :sourceid);";
        
        static const char* DELETE_PRODUCTS_BY_SOURCEID_PLATFORM = "AssetProcessor::DeleteProductsBySourceIDPlatform";
        static const char* DELETE_PRODUCTS_BY_SOURCEID_PLATFORM_STATEMENT =
            "DELETE FROM Products "
            "WHERE EXISTS "
            "(SELECT * FROM Jobs WHERE "
            "Products.JobPK = Jobs.JobsID AND "
            "Jobs.SourcePK = :sourceid AND "
            "Jobs.Platform = :platform);";

        static const char* INSERT_SOURCE_DEPENDENCY = "AssetProcessor::InsertSourceDependency";
        static const char* INSERT_SOURCE_DEPENDENCY_STATEMENT =
            "INSERT INTO SourceDependency (BuilderGuid, Source, DependsOnSource) "
            "VALUES (:builderGuid, :source, :dependsOnSource);";

        static const char* UPDATE_SOURCE_DEPENDENCY = "AssetProcessor::UpdateSourceDependency";
        static const char* UPDATE_SOURCE_DEPENDENCY_STATEMENT =
            "UPDATE SourceDependency SET "
            "DependsOnSource = :dependsOnSource, WHERE "
            "BuilderGuid = :builderGuid AND "
            "Source = :source;";

        static const char* DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID = "AssetProcessor::DeleteSourceDependencBySourceDependencyId";
        static const char* DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID_STATEMENT =
            "DELETE FROM SourceDependency WHERE "
            "SourceDependencyID = :sourceDependencyId;";
    }

    AssetDatabaseConnection::AssetDatabaseConnection()
    {
        qRegisterMetaType<ScanFolderDatabaseEntry>( "ScanFolderEntry" );
        qRegisterMetaType<SourceDatabaseEntry>( "SourceEntry" );
        qRegisterMetaType<JobDatabaseEntry>( "JobDatabaseEntry" );
        qRegisterMetaType<ProductDatabaseEntry>( "ProductEntry" );
        qRegisterMetaType<CombinedDatabaseEntry>( "CombinedEntry" );
        qRegisterMetaType<SourceDatabaseEntryContainer>( "SourceEntryContainer" );
        qRegisterMetaType<JobDatabaseEntryContainer>( "JobDatabaseEntryContainer" );
        qRegisterMetaType<ProductDatabaseEntryContainer>( "ProductEntryContainer" );
        qRegisterMetaType<CombinedDatabaseEntryContainer>( "CombinedEntryContainer" );
    }

    AssetDatabaseConnection::~AssetDatabaseConnection()
    {
        CloseDatabase();
    }

    bool AssetDatabaseConnection::DataExists()
    {
        AZStd::string dbFilePath = GetAssetDatabaseFilePath();
        return AZ::IO::SystemFile::Exists(dbFilePath.c_str());
    }

    void AssetDatabaseConnection::LoadData()
    {
        if((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
        {
            OpenDatabase();
        }
    }

    void AssetDatabaseConnection::ClearData()
    {
        if((m_databaseConnection) && (m_databaseConnection->IsOpen()))
        {
            CloseDatabase();
        }
        AZStd::string dbFilePath = GetAssetDatabaseFilePath();
        AZ::IO::SystemFile::Delete(dbFilePath.c_str());
        OpenDatabase();
    }


    bool AssetDatabaseConnection::PostOpenDatabase()
    {
        DatabaseVersion foundVersion = QueryDatabaseVersion();
        bool dropAllTables = true;

        if (foundVersion == DatabaseVersion::AddedOutputPrefixToScanFolders)
        {
            // execute statements to upgrade the database
            if (m_databaseConnection->ExecuteOneOffStatement(CREATEINDEX_JOBS_JOBKEY))
            {
                foundVersion = DatabaseVersion::AddedJobKeyIndex;
            }
        }

        // over here, check the version number, and perform upgrading if you need to
        if (foundVersion == DatabaseVersion::AddedJobKeyIndex)
        {
            if (
                (m_databaseConnection->ExecuteOneOffStatement(CREATEINDEX_SOURCE_GUID))&&
                (m_databaseConnection->ExecuteOneOffStatement(CREATEINDEX_SCANFOLDERS_SOURCES_SCANFOLDER))
                )
            {
                foundVersion = DatabaseVersion::AddedSourceGuidIndex;
            }
        }

        if (foundVersion == DatabaseVersion::AddedSourceGuidIndex)
        {
            if (
                (m_databaseConnection->ExecuteOneOffStatement(CREATE_SOURCE_DEPENDENCY_TABLE)) &&
                (m_databaseConnection->ExecuteOneOffStatement(CREATEINDEX_DEPENDSONSOURCE_SOURCEDEPENDENCY)) &&
                (m_databaseConnection->ExecuteOneOffStatement(CREATEINDEX_BUILDERGUID_SOURCE_SOURCEDEPENDENCY))
                )
            {
                foundVersion = DatabaseVersion::AddedSourceDependencyTable;
            }
        }
        
        if(foundVersion == CurrentDatabaseVersion())
        {
            dropAllTables = false;
        }
        else
        {
            dropAllTables = true;
        }

        // example, if you know how to get from version 1 to version 2, and we're on version 1 and should be on version 2,
        // we can either drop all tables and recreate them, or we can write statements which upgrade the database.
        // if you know how to upgrade, write your modify statements here, then set dropAllTables to false.
        // otherwise it will re-create from scratch.

        if(dropAllTables)
        {
            // drop all tables by destroying the entire database.
            m_databaseConnection->Close();
            AZStd::string dbFilePath = GetAssetDatabaseFilePath();
            AZ::IO::SystemFile::Delete(dbFilePath.c_str());
            if(!m_databaseConnection->Open(dbFilePath, IsReadOnly()))
            {
                delete m_databaseConnection;
                m_databaseConnection = nullptr;
                AZ_Error(LOG_NAME, false, "Unable to open the asset database at %s\n", dbFilePath.c_str());
                return false;
            }

            CreateStatements();
            ExecuteCreateStatements();
        }

        // now that the database matches the schema, update it:
        SetDatabaseVersion(CurrentDatabaseVersion());

        return AzToolsFramework::AssetDatabase::AssetDatabaseConnection::PostOpenDatabase();
    }


    void AssetDatabaseConnection::ExecuteCreateStatements()
    {
        AZ_Assert(m_databaseConnection, "No connection!");
        for(const auto& element : m_createStatements)
        {
            m_databaseConnection->ExecuteOneOffStatement(element.c_str());
        }
    }

    void AssetDatabaseConnection::SetDatabaseVersion(DatabaseVersion ver)
    {
        AZ_Error(LOG_NAME, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist");
        AZ_Error(LOG_NAME, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open");
        AZ_Error(LOG_NAME, m_databaseConnection->DoesTableExist("dbinfo"), "Fatal: dbinfo table does not exist");

        StatementAutoFinalizer autoFinal(*m_databaseConnection, SET_DATABASE_VERSION);
        Statement* statement = autoFinal.Get();
        AZ_Error(LOG_NAME, statement, "Statement not found: %s", SET_DATABASE_VERSION);

        statement->BindValueInt(statement->GetNamedParamIdx(":ver"), static_cast<int>(ver));
        Statement::SqlStatus result = statement->Step();
        AZ_Warning(LOG_NAME, result != SQLite::Statement::SqlOK, "Failed to execute SetDatabaseVersion.");
    }

    void AssetDatabaseConnection::CreateStatements()
    {
        AZ_Assert(m_databaseConnection, "No connection!");
        AZ_Assert(m_databaseConnection->IsOpen(), "Connection is not open");

        AzToolsFramework::AssetDatabase::AssetDatabaseConnection::CreateStatements();


        // ---------------------------------------------------------------------------------------------
        //                  Housekeeping
        // ---------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement("VACUUM", "VACUUM");
        m_databaseConnection->AddStatement("ANALYZE", "ANALYZE");

        // ---------------------------------------------------------------------------------------------
        //                  Database Info table
        // ----------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATE_DATABASE_INFOTABLE, CREATE_DATABASE_INFOTABLE_STATEMENT);
        m_createStatements.push_back(CREATE_DATABASE_INFOTABLE);

        m_databaseConnection->AddStatement(SET_DATABASE_VERSION, SET_DATABASE_VERSION_STATEMENT);

        // ----------------------------------------------------------------------------------------------
        //                  ScanFolders table
        // ----------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATE_SCANFOLDERS_TABLE, CREATE_SCANFOLDERS_TABLE_STATEMENT);
        m_createStatements.push_back(CREATE_SCANFOLDERS_TABLE);
        
        m_databaseConnection->AddStatement(INSERT_SCANFOLDER, INSERT_SCANFOLDER_STATEMENT);
        m_databaseConnection->AddStatement(UPDATE_SCANFOLDER, UPDATE_SCANFOLDER_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_SCANFOLDER, DELETE_SCANFOLDER_STATEMENT);

        // ---------------------------------------------------------------------------------------------
        //                  Source table
        // ---------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATE_SOURCES_TABLE, CREATE_SOURCES_TABLE_STATEMENT);
        m_createStatements.push_back(CREATE_SOURCES_TABLE);

        m_databaseConnection->AddStatement(INSERT_SOURCE, INSERT_SOURCE_STATEMENT);
        m_databaseConnection->AddStatement(UPDATE_SOURCE, UPDATE_SOURCE_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_SOURCE, DELETE_SOURCE_STATEMENT);

        // ---------------------------------------------------------------------------------------------
        //                  Jobs table
        // ---------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATE_JOBS_TABLE, CREATE_JOBS_TABLE_STATEMENT);
        m_createStatements.push_back(CREATE_JOBS_TABLE);
        
        m_databaseConnection->AddStatement(GET_HIGHEST_JOBRUNKEY, GET_HIGHEST_JOBRUNKEY_STATEMENT);
        m_databaseConnection->AddStatement(INSERT_JOB, INSERT_JOB_STATEMENT);
        m_databaseConnection->AddStatement(UPDATE_JOB, UPDATE_JOB_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_JOB, DELETE_JOB_STATEMENT);
        // ---------------------------------------------------------------------------------------------
        //                   Products table
        // ---------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATE_PRODUCT_TABLE, CREATE_PRODUCT_TABLE_STATEMENT);
        m_createStatements.push_back(CREATE_PRODUCT_TABLE);

        m_databaseConnection->AddStatement(INSERT_PRODUCT, INSERT_PRODUCT_STATEMENT);
        m_databaseConnection->AddStatement(UPDATE_PRODUCT, UPDATE_PRODUCT_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_PRODUCT, DELETE_PRODUCT_STATEMENT);

        m_databaseConnection->AddStatement(DELETE_PRODUCTS_BY_JOBID, DELETE_PRODUCTS_BY_JOBID_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_PRODUCTS_BY_SOURCEID, DELETE_PRODUCTS_BY_SOURCEID_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_PRODUCTS_BY_SOURCEID_PLATFORM, DELETE_PRODUCTS_BY_SOURCEID_PLATFORM_STATEMENT);

        // ---------------------------------------------------------------------------------------------
        //                   Source Dependency table
        // ---------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATE_SOURCE_DEPENDENCY_TABLE, CREATE_SOURCE_DEPENDENCY_TABLE_STATEMENT);
        m_createStatements.push_back(CREATE_SOURCE_DEPENDENCY_TABLE);

        m_databaseConnection->AddStatement(INSERT_SOURCE_DEPENDENCY, INSERT_SOURCE_DEPENDENCY_STATEMENT);
        m_databaseConnection->AddStatement(UPDATE_SOURCE_DEPENDENCY, UPDATE_SOURCE_DEPENDENCY_STATEMENT);
        m_databaseConnection->AddStatement(DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID, DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID_STATEMENT);

        // ---------------------------------------------------------------------------------------------
        //                   Indices
        // ---------------------------------------------------------------------------------------------
        m_databaseConnection->AddStatement(CREATEINDEX_DEPENDSONSOURCE_SOURCEDEPENDENCY, CREATEINDEX_DEPENDSONSOURCE_SOURCEDEPENDENCY_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_DEPENDSONSOURCE_SOURCEDEPENDENCY);

        m_databaseConnection->AddStatement(CREATEINDEX_BUILDERGUID_SOURCE_SOURCEDEPENDENCY, CREATEINDEX_BUILDERGUID_SOURCE_SOURCEDEPENDENCY_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_BUILDERGUID_SOURCE_SOURCEDEPENDENCY);

        m_databaseConnection->AddStatement(CREATEINDEX_SCANFOLDERS_SOURCES_SCANFOLDER, CREATEINDEX_SCANFOLDERS_SOURCES_SCANFOLDER_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_SCANFOLDERS_SOURCES_SCANFOLDER);

        m_databaseConnection->AddStatement(CREATEINDEX_SOURCES_JOBS, CREATEINDEX_SOURCES_JOBS_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_SOURCES_JOBS);

        m_databaseConnection->AddStatement(CREATEINDEX_JOBS_PRODUCTS, CREATEINDEX_JOBS_PRODUCTS_STATEMENT);
        
        m_createStatements.push_back(CREATEINDEX_JOBS_PRODUCTS);

        m_databaseConnection->AddStatement(CREATEINDEX_JOBS_JOBRUNKEY, CREATEINDEX_JOBS_JOBRUNKEY_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_JOBS_JOBRUNKEY);

        m_databaseConnection->AddStatement(CREATEINDEX_JOBS_JOBKEY, CREATEINDEX_JOBS_JOBKEY_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_JOBS_JOBKEY);

        m_databaseConnection->AddStatement(CREATEINDEX_SOURCE_NAME, CREATEINDEX_SOURCE_NAME_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_SOURCE_NAME);

        m_databaseConnection->AddStatement(CREATEINDEX_SOURCE_GUID, CREATEINDEX_SOURCE_GUID_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_SOURCE_GUID);
        
        m_databaseConnection->AddStatement(CREATEINDEX_PRODUCT_NAME, CREATEINDEX_PRODUCT_NAME_STATEMENT);
        m_createStatements.push_back(CREATEINDEX_PRODUCT_NAME);
    }

    void AssetDatabaseConnection::VacuumAndAnalyze()
    {
        if(m_databaseConnection)
        {
            m_databaseConnection->ExecuteOneOffStatement("VACUUM");
            m_databaseConnection->ExecuteOneOffStatement("ANALYZE");
        }
    }

    bool AssetDatabaseConnection::GetScanFolderByScanFolderID(AZ::s64 scanfolderID, ScanFolderDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryScanFolderByScanFolderID( scanfolderID, 
            [&](ScanFolderDatabaseEntry& scanFolderEntry)
            {
                entry = scanFolderEntry;
                found = true;
                return false;//only one
            });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetScanFolderBySourceID(AZ::s64 sourceID, ScanFolderDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryScanFolderBySourceID( sourceID, 
            [&](ScanFolderDatabaseEntry& scanFolderEntry)
        {
            entry = scanFolderEntry;
            found = true;
            return false;//only one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetScanFolderByJobID(AZ::s64 jobID, ScanFolderDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryScanFolderByJobID( jobID, 
            [&](ScanFolderDatabaseEntry& scanFolderEntry)
        {
            entry = scanFolderEntry;
            found = true;
            return false;//only one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetScanFolderByProductID(AZ::s64 productID, ScanFolderDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryScanFolderByProductID( productID, 
            [&](ScanFolderDatabaseEntry& scanFolderEntry)
        {
            entry = scanFolderEntry;
            found = true;
            return false;//only one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetScanFolderByPortableKey(QString portableKey, ScanFolderDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryScanFolderByPortableKey(portableKey.toUtf8().constData(),
            [&](ScanFolderDatabaseEntry& scanFolder)
            {
                entry = AZStd::move(scanFolder);
                found = true;
                return false;//only one
            });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetScanFolders(ScanFolderDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QueryScanFoldersTable(
            [&](ScanFolderDatabaseEntry& scanFolder)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(scanFolder);
                return true;//all
            });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::SetScanFolder(ScanFolderDatabaseEntry& entry)
    {
        if(!ValidateDatabaseTable(INSERT_SCANFOLDER, "ScanFolders"))
        {
            AZ_Error(LOG_NAME, false, "Could not find ScanFolder table");
            return false;
        }

        ScanFolderDatabaseEntry existingEntry;

        if(entry.m_scanFolderID == -1)
        {
            //they didn't supply an id, add to database!

            //make sure the scan path is not already in the database
            if(GetScanFolderByPortableKey(entry.m_portableKey.c_str(), existingEntry))
            {
                //its in the database already, update the input entry id and try again:
                entry.m_scanFolderID = existingEntry.m_scanFolderID;
                return SetScanFolder(entry);
            }

            //its not in the database, add it
            // it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, INSERT_SCANFOLDER);
            Statement* statement = autoFinal.Get();
            AZ_Error(LOG_NAME, statement, "Could not get statement: %s", INSERT_SCANFOLDER);

            int scanFolderIdx = statement->GetNamedParamIdx(":scanfolder");
            if(!scanFolderIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for scanFolder in %s", INSERT_SCANFOLDER);
                return false;
            }
            statement->BindValueText(scanFolderIdx, entry.m_scanFolder.c_str());

            int displayNameIdx = statement->GetNamedParamIdx(":displayname");
            if (!displayNameIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for displayname in %s", INSERT_SCANFOLDER);
                return false;
            }
            statement->BindValueText(displayNameIdx, entry.m_displayName.c_str());

            int portableKeyIdx = statement->GetNamedParamIdx(":portablekey");
            if (!portableKeyIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for portablekey in %s", INSERT_SCANFOLDER);
                return false;
            }
            statement->BindValueText(portableKeyIdx, entry.m_portableKey.c_str());

            int outputPrefixIdx = statement->GetNamedParamIdx(":outputprefix");
            if (!outputPrefixIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for outputPrefix %i", outputPrefixIdx);
                return false;
            }
            statement->BindValueText(outputPrefixIdx, entry.m_outputPrefix.c_str());

            int isRootIdx = statement->GetNamedParamIdx(":isroot");
            if (!isRootIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for isRootIdx %i", isRootIdx);
                return false;
            }
            statement->BindValueInt(isRootIdx, entry.m_isRoot);

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to write the new scan folder into the database.");
                return false;
            }

            if(GetScanFolderByPortableKey(entry.m_portableKey.c_str(), existingEntry))
            {
                //its in the database already, update the input entry
                entry.m_scanFolderID = existingEntry.m_scanFolderID;
                return true;
            }

            AZ_Error(LOG_NAME, false, "Failed to read the new scan folder into the database.");
            return false;
        }
        else
        {
            //they supplied an id, see if it exists in the database
            if(!GetScanFolderByScanFolderID(entry.m_scanFolderID, existingEntry))
            {
                AZ_WarningOnce(LOG_NAME, false, "Failed to write the new scan folder into the database.");
                return false;
            }

            StatementAutoFinalizer autoFinal(*m_databaseConnection, UPDATE_SCANFOLDER);
            Statement* statement = autoFinal.Get();
            AZ_Error(LOG_NAME, statement, "Could not get statement: %s", UPDATE_SCANFOLDER);

            int scanFolderIDIdx = statement->GetNamedParamIdx(":scanfolderid");
            if(!scanFolderIDIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for scanfolderid %i", scanFolderIDIdx);
                return false;
            }
            statement->BindValueInt64(scanFolderIDIdx, entry.m_scanFolderID);

            int scanFolderIdx = statement->GetNamedParamIdx(":scanfolder");
            if(!scanFolderIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for scanFolder %i", scanFolderIdx);
                return false;
            }
            statement->BindValueText(scanFolderIdx, entry.m_scanFolder.c_str());

            int displayNameIdx = statement->GetNamedParamIdx(":displayname");
            if (!displayNameIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for displayname %i", displayNameIdx);
                return false;
            }
            statement->BindValueText(displayNameIdx, entry.m_displayName.c_str());

            int portableKeyIdx = statement->GetNamedParamIdx(":portablekey");
            if (!portableKeyIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for portablekey %i", portableKeyIdx);
                return false;
            }
            statement->BindValueText(portableKeyIdx, entry.m_portableKey.c_str());

            int outputPrefixIdx = statement->GetNamedParamIdx(":outputprefix");
            if (!outputPrefixIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for outputprefix %i", outputPrefixIdx);
                return false;
            }
            statement->BindValueText(outputPrefixIdx, entry.m_outputPrefix.c_str());

            int isRootIdx = statement->GetNamedParamIdx(":isroot");
            if (!isRootIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for isroot %i", isRootIdx);
                return false;
            }
            statement->BindValueInt(isRootIdx, entry.m_isRoot);

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to write the new scan folder into the database.");
                return false;
            }

            return true;
        }
    }

    bool AssetDatabaseConnection::RemoveScanFolder(AZ::s64 scanFolderID)
    {
        if(!ValidateDatabaseTable(DELETE_SCANFOLDER, "ScanFolders"))
        {
            return false;
        }

        ScopedTransaction transaction(m_databaseConnection);

        StatementAutoFinalizer autoFinal(*m_databaseConnection, DELETE_SCANFOLDER);
        Statement* statement = autoFinal.Get();
        AZ_Error(LOG_NAME, statement, "Could not get statement: %s", DELETE_SCANFOLDER);

        int scanFolderIDIdx = statement->GetNamedParamIdx(":scanfolderid");
        if(!scanFolderIDIdx)
        {
            AZ_Error(LOG_NAME, false, "Could not find the Idx for scanFolderID %i", scanFolderIDIdx);
            return false;
        }
        statement->BindValueInt64(scanFolderIDIdx, scanFolderID);

        if(statement->Step() == Statement::SqlError)
        {
            AZ_Warning(LOG_NAME, false, "Failed to remove the scan folder from the database.");
            return false;
        }

        transaction.Commit();

        return true;
    }

    bool AssetDatabaseConnection::RemoveScanFolders(ScanFolderDatabaseEntryContainer& container)
    {
        bool succeeded = true;
        for(auto& entry : container)
        {
            succeeded &= RemoveScanFolder(entry.m_scanFolderID);
            if(succeeded)
            {
                entry.m_scanFolderID = -1;//set it to default -1 as this is no longer exists
            }
        }
        return succeeded;
    }

    bool AssetDatabaseConnection::GetSourceBySourceID(AZ::s64 sourceID, SourceDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QuerySourceBySourceID( sourceID,
            [&](SourceDatabaseEntry& source)
            {
                found = true;
                entry = AZStd::move(source);
                return false;//one
            });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourceBySourceGuid(AZ::Uuid sourceGuid, SourceDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QuerySourceBySourceGuid(sourceGuid,
            [&](SourceDatabaseEntry& source)
        {
            found = true;
            entry = AZStd::move(source);
            return false;//one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSources(SourceDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QuerySourcesTable(
            [&](SourceDatabaseEntry& source)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(source);
                return true;//all
            });
        return  found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourcesBySourceName(QString exactSourceName, SourceDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QuerySourceBySourceName(exactSourceName.toUtf8().constData(),
            [&](SourceDatabaseEntry& source)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(source);
            return true;//all
        });
        return  found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourcesBySourceNameScanFolderId(QString exactSourceName, AZ::s64 scanFolderID, SourceDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QuerySourceBySourceNameScanFolderID(exactSourceName.toUtf8().constData(),
            scanFolderID,
            [&](SourceDatabaseEntry& source)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(source);
            return true;//all
        });
        return  found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourcesLikeSourceName(QString likeSourceName, LikeType likeType, SourceDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QuerySourceLikeSourceName(likeSourceName.toUtf8().constData(), likeType,
            [&](SourceDatabaseEntry& source)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(source);
                return true;//all
            });
        return  found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourceByJobID(AZ::s64 jobID, SourceDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QuerySourceByJobID( jobID,
            [&](SourceDatabaseEntry& source)
        {
            found = true;
            entry = AZStd::move(source);
            return false;//one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourceByProductID(AZ::s64 productID, SourceDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QuerySourceByProductID( productID,
            [&](SourceDatabaseEntry& source)
        {
            found = true;
            entry = AZStd::move(source);
            return false;//one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourcesByProductName(QString exactProductName, SourceDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QueryCombinedByProductName(exactProductName.toUtf8().constData(), 
            [&](CombinedDatabaseEntry& combined)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(combined);
            return true; // return true to continue collecting all
        });
        return  found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourcesLikeProductName(QString likeProductName, LikeType likeType, SourceDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QueryCombinedLikeProductName(likeProductName.toUtf8().constData(), likeType,
            [&](CombinedDatabaseEntry& combined)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(combined);
            return true;//all
        });
        return  found && succeeded;
    }

    bool AssetDatabaseConnection::SetSource(SourceDatabaseEntry& entry)
    {
        if(!ValidateDatabaseTable(INSERT_SOURCE, "Sources"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Sources table");
            return false;
        }
        
        if(entry.m_sourceID == -1)
        {
            //they didn't supply an id, add to database
            
            //first make sure its not already in the database
            SourceDatabaseEntry existingEntry;
            if(GetSourceBySourceGuid(entry.m_sourceGuid, existingEntry))
            {
                // this source guid already exists.  note that the UUID is final, there is only ever one UUID for a source
                // if folders override each other, the UUID stays the same but the scanfolder field changes but its still considered the same source file.
                entry.m_sourceID = existingEntry.m_sourceID;
                return SetSource(entry); // now update the existing field
            }

            // it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, INSERT_SOURCE);
            Statement* statement = autoFinal.Get();
            if(!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", INSERT_SOURCE);
                return false;
            }

            int scanFolderIDIdx = statement->GetNamedParamIdx(":scanfolderid");
            if(!scanFolderIDIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find scanfolderpk in statement %s", INSERT_SOURCE);
                return false;
            }
            statement->BindValueInt64(scanFolderIDIdx, entry.m_scanFolderPK);

            int sourceNameIdx = statement->GetNamedParamIdx(":sourcename");
            if(!sourceNameIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourcename in statement %s", INSERT_SOURCE);
                return false;
            }
            statement->BindValueText(sourceNameIdx, entry.m_sourceName.c_str());

            int sourceGuidIdx = statement->GetNamedParamIdx(":sourceguid");
            if(!sourceGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourceguid in statement %s", INSERT_SOURCE);
                return false;
            }
            statement->BindValueUuid(sourceGuidIdx, entry.m_sourceGuid);

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to write the new source into the database.");
                return false;
            }

            //now that its in the database get the id
            if (GetSourceBySourceGuid(entry.m_sourceGuid, existingEntry))
            {
                entry.m_sourceID = existingEntry.m_sourceID;
                return true;
            }

            AZ_Error(LOG_NAME, false, "Failed to read the new source into the database.");
            return false;
        }
        else
        {
            //they supplied an id, see if it exists in the database
            SourceDatabaseEntry existingEntry;
            if(!GetSourceBySourceID(entry.m_sourceID, existingEntry))
            {
                //they supplied an id but is not in the database!
                AZ_Error(LOG_NAME, false, "Failed to write the source into the database.");
                return false;
            }

            // don't bother updating the database if all fields are equal.
            // note that we already looked it up by source ID
            if ((existingEntry.m_scanFolderPK == entry.m_scanFolderPK) &&
                (existingEntry.m_sourceGuid == entry.m_sourceGuid) &&
                (existingEntry.m_sourceName == entry.m_sourceName))
            {
                return true;
            }
                
            StatementAutoFinalizer autoFinal(*m_databaseConnection, UPDATE_SOURCE);
            Statement* statement = autoFinal.Get();
            if(!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", UPDATE_SOURCE);
                return false;
            }

            int sourceIdx = statement->GetNamedParamIdx(":sourceid");
            if(!sourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourceid in statement %s", INSERT_SOURCE);
                return false;
            }
            statement->BindValueInt64(sourceIdx, entry.m_sourceID);

            int scanFolderIdx = statement->GetNamedParamIdx(":scanfolderpk");
            if(!scanFolderIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find scanfolderpk in statement %s", INSERT_SOURCE);
                return false;
            }
            statement->BindValueInt64(scanFolderIdx, entry.m_scanFolderPK);

            int sourceNameIdx = statement->GetNamedParamIdx(":sourcename");
            if(!sourceNameIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourcename in statement %s", UPDATE_SOURCE);
                return false;
            }
            statement->BindValueText(sourceNameIdx, entry.m_sourceName.c_str());

            int sourceGuidIdx = statement->GetNamedParamIdx(":sourceguid");
            if(!sourceGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourceguid in statement %s", UPDATE_SOURCE);
                return false;
            }
            statement->BindValueUuid(sourceGuidIdx, entry.m_sourceGuid);

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to execute %s to update fingerprints (key %i)", UPDATE_SOURCE, entry.m_sourceID);
                return false;
            }

            return true;
        }
    }

    // this must actually delete the source
    bool AssetDatabaseConnection::RemoveSource(AZ::s64 sourceID)
    {
        if(!ValidateDatabaseTable(DELETE_SOURCE, "Sources"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Sources table");
            return false;
        }

        ScopedTransaction transaction(m_databaseConnection);

        StatementAutoFinalizer autoFinal(*m_databaseConnection, DELETE_SOURCE);
        Statement* statement = autoFinal.Get();
        if(!statement)
        {
            AZ_Error(LOG_NAME, statement, "Could not get statement: %s", DELETE_SOURCE);
            return false;
        }

        int sourceIDIdx = statement->GetNamedParamIdx(":sourceid");
        if(!sourceIDIdx)
        {
            AZ_Error(LOG_NAME, false, "could not find sourceid in statement %s", DELETE_SOURCE);
            return false;
        }

        statement->BindValueInt64(sourceIDIdx, sourceID);

        if(statement->Step() == Statement::SqlError)
        {
            AZ_Warning(LOG_NAME, false, "Failed to RemoveSource form the database");
            return false;
        }

        transaction.Commit();
        
        return true;
    }

    bool AssetDatabaseConnection::RemoveSources(SourceDatabaseEntryContainer& container)
    {
        bool succeeded = true;
        for(auto& entry : container)
        {
            succeeded &= RemoveSource(entry.m_sourceID);
            if(succeeded)
            {
                entry.m_sourceID = -1;//set it to -1 as it no longer exists
            }
        }
        return succeeded;
    }

    bool AssetDatabaseConnection::RemoveSourcesByScanFolderID(AZ::s64 scanFolderID)
    {
        bool found = false;
        bool succeeded = QuerySourceByScanFolderID( scanFolderID,
            [&](SourceDatabaseEntry& source)
        {
            found = true;
            succeeded &= RemoveSource(source.m_sourceID);
            return true;//all
        });
        return found && succeeded;
    }

    AZ::s64 AssetDatabaseConnection::GetHighestJobRunKey()
    {
        if (!m_databaseConnection)
        {
            return 0;
        }
        StatementAutoFinalizer autoFinal(*m_databaseConnection, GET_HIGHEST_JOBRUNKEY);
        Statement* statement = autoFinal.Get();
        if (!statement)
        {
            AZ_Error(LOG_NAME, statement, "Could not get statement: %s\n", GET_HIGHEST_JOBRUNKEY);
            return 0;
        }

        if (statement->Step() == Statement::SqlError)
        {
            // this is okay, since the table may be empty.
            return 0;
        }

        return statement->GetColumnInt64(0);
    }

    bool AssetDatabaseConnection::GetJobs(JobDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryJobsTable(
            [&](JobDatabaseEntry& job)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(job);
            return true;//all
        },  builderGuid,
            jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
            platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
            status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobByJobID(AZ::s64 jobID, JobDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryJobByJobID( jobID,
            [&](JobDatabaseEntry& job)
        {
            found = true;
            entry = AZStd::move(job);
            return false;//one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobByProductID(AZ::s64 productID, JobDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryJobByProductID(productID,
            [&](JobDatabaseEntry& job)
        {
            found = true;
            entry = AZStd::move(job);
            return true;//all
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobsBySourceID(AZ::s64 sourceID, JobDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryJobBySourceID(sourceID,
            [&](JobDatabaseEntry& job)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(job);
            return true;//all
        },  builderGuid,
            jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
            platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
            status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobsBySourceName(QString exactSourceName, JobDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QuerySourceBySourceName(exactSourceName.toUtf8().constData(),
            [&](SourceDatabaseEntry& source)
        {
            succeeded = QueryJobBySourceID(source.m_sourceID,
                [&](JobDatabaseEntry& job)
                {
                    found = true;
                    container.push_back();
                    container.back() = AZStd::move(job);
                    return true;//all
                },  builderGuid,
                    jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
                    platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
                    status);
            return true;//all
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobsLikeSourceName(QString likeSourceName, LikeType likeType, JobDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QuerySourceLikeSourceName(likeSourceName.toUtf8().constData(), likeType,
            [&](SourceDatabaseEntry& source)
        {
            succeeded = QueryJobBySourceID(source.m_sourceID,
                [&](JobDatabaseEntry& job)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(job);
                return true;//all
            },  builderGuid,
                jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
                platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
                status);
            return true;//all
        });
        return found && succeeded;
    }
    
    bool AssetDatabaseConnection::GetJobsByProductName(QString exactProductName, JobDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductByProductName(exactProductName.toUtf8().constData(),
            [&](ProductDatabaseEntry& product)
        {
            succeeded = QueryJobByProductID(product.m_productID,
                [&](JobDatabaseEntry& job)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(job);
                return true;//all
            });
            return true;//all
        },  builderGuid,
            jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
            platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
            status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobsLikeProductName(QString likeProductName, LikeType likeType, JobDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductLikeProductName(likeProductName.toUtf8().constData(), likeType,
            [&](ProductDatabaseEntry& product)
        {
            succeeded = QueryJobByProductID(product.m_productID,
                [&](JobDatabaseEntry& job)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(job);
                return true;//all
            });
            return true;//all
        },  builderGuid,
            jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
            platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
            status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::SetJob(JobDatabaseEntry& entry)
    {
        if(!ValidateDatabaseTable("SetJob", "Jobs"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Jobs table");
            return false;
        }

        if (entry.m_jobRunKey <= 0)
        {
            AZ_Error(LOG_NAME, false, "You must specify a valid Job Run Key for a job to make it into the database.\n");
            return false;
        }

        if(entry.m_jobID == -1)
        {
            //they didn't supply an id, add to database

            //make sure its not already in the database
            JobDatabaseEntryContainer existingJobs;
            if(GetJobsBySourceID(entry.m_sourcePK, existingJobs, entry.m_builderGuid, entry.m_jobKey.c_str(), entry.m_platform.c_str()))
            {
                //see if this job is already here
                for(const auto& existingjob : existingJobs)
                {
                    if(existingjob == entry)
                    {
                        //this job already exists
                        entry.m_jobID = existingjob.m_jobID;
                        return true;
                    }
                }
            }

            // it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, INSERT_JOB);
            Statement* statement = autoFinal.Get();
            if(!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", INSERT_JOB);
                return false;
            }

            int sourceIdx = statement->GetNamedParamIdx(":sourceid");
            if(!sourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourceid in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt64(sourceIdx, entry.m_sourcePK);

            int jobKeyIdx = statement->GetNamedParamIdx(":jobkey");
            if(!jobKeyIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find jobkey in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueText(jobKeyIdx, entry.m_jobKey.c_str());

            int fingerprintIdx = statement->GetNamedParamIdx(":fingerprint");
            if(!fingerprintIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find fingerprint in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt(fingerprintIdx, entry.m_fingerprint);

            int platformIdx = statement->GetNamedParamIdx(":platform");
            if(!platformIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find platform in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueText(platformIdx, entry.m_platform.c_str());

            int builderguidIdx = statement->GetNamedParamIdx(":builderguid");
            if(!builderguidIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find builderguid in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueUuid(builderguidIdx, entry.m_builderGuid);

            int statusIdx = statement->GetNamedParamIdx(":status");
            if(!statusIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find status in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt(statusIdx, static_cast<int>(entry.m_status));

            int jobrunkeyIdx = statement->GetNamedParamIdx(":jobrunkey");
            if(!jobrunkeyIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find jobrunkey in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt64(jobrunkeyIdx, entry.m_jobRunKey);

            int firstfaillogtimeIdx = statement->GetNamedParamIdx(":firstfaillogtime");
            if(!firstfaillogtimeIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find firstfaillogtime in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt64(firstfaillogtimeIdx, entry.m_firstFailLogTime);

            int firstfaillogfileIdx = statement->GetNamedParamIdx(":firstfaillogfile");
            if(!firstfaillogfileIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find firstfaillogfile in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueText(firstfaillogfileIdx, entry.m_firstFailLogFile.c_str());

            int lastfaillogtimeIdx = statement->GetNamedParamIdx(":lastfaillogtime");
            if(!lastfaillogtimeIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastfaillogtime in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt64(lastfaillogtimeIdx, entry.m_lastFailLogTime);

            int lastfaillogfileIdx = statement->GetNamedParamIdx(":lastfaillogfile");
            if(!lastfaillogfileIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastfaillogfile in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueText(lastfaillogfileIdx, entry.m_lastFailLogFile.c_str());

            int lastlogtimeIdx = statement->GetNamedParamIdx(":lastlogtime");
            if(!lastlogtimeIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastlogtime in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueInt64(lastlogtimeIdx, entry.m_lastLogTime);

            int lastlogfileIdx = statement->GetNamedParamIdx(":lastlogfile");
            if(!lastlogfileIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastlogfile in statement %s", INSERT_JOB);
                return false;
            }
            statement->BindValueText(lastlogfileIdx, entry.m_lastLogFile.c_str());

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to write the new job into the database.");
                return false;
            }

            //make sure its now in the database
            existingJobs.clear();
            if(GetJobsBySourceID(entry.m_sourcePK, existingJobs, entry.m_builderGuid, entry.m_jobKey.c_str(), entry.m_platform.c_str()))
            {
                //see if this job is already here
                for(const auto& existingjob : existingJobs)
                {
                    if(existingjob == entry)
                    {
                        //this job already exists
                        entry.m_jobID = existingjob.m_jobID;
                        return true;
                    }
                }
            }

            AZ_Warning(LOG_NAME, false, "Failed to read the new job from the database.");
            return false;
        }
        else
        {
            //they supplied an id, see if it exists in the database
            JobDatabaseEntry existingEntry;
            if(!GetJobByJobID(entry.m_jobID, existingEntry))
            {
                AZ_Error(LOG_NAME, false, "Failed to find the job in the database.");
                return false;
            }

            //its in the database already, if its not the same update the database
            if(existingEntry == entry)
            {
                return true;
            }

            // it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, UPDATE_JOB);
            Statement* statement = autoFinal.Get();
            if(!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", UPDATE_JOB);
                return false;
            }
            
            int jobIDIdx = statement->GetNamedParamIdx(":jobid");
            if(!jobIDIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find jobid in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt64(jobIDIdx, entry.m_jobID);

            int sourceIdx = statement->GetNamedParamIdx(":sourceid");
            if(!sourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find sourceid in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt64(sourceIdx, entry.m_sourcePK);

            int jobKeyIdx = statement->GetNamedParamIdx(":jobkey");
            if(!jobKeyIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find jobkey in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueText(jobKeyIdx, entry.m_jobKey.c_str());

            int fingerprintIdx = statement->GetNamedParamIdx(":fingerprint");
            if(!fingerprintIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find fingerprint in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt(fingerprintIdx, entry.m_fingerprint);

            int platformIdx = statement->GetNamedParamIdx(":platform");
            if(!platformIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find platform in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueText(platformIdx, entry.m_platform.c_str());

            int builderguidIdx = statement->GetNamedParamIdx(":builderguid");
            if(!builderguidIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find builderguid in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueUuid(builderguidIdx, entry.m_builderGuid);

            int statusIdx = statement->GetNamedParamIdx(":status");
            if(!statusIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find status in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt(statusIdx, static_cast<int>(entry.m_status));

            int jobrunkeyIdx = statement->GetNamedParamIdx(":jobrunkey");
            if(!jobrunkeyIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find jobrunkey in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt64(jobrunkeyIdx, entry.m_jobRunKey);

            int firstfaillogtimeIdx = statement->GetNamedParamIdx(":firstfaillogtime");
            if(!firstfaillogtimeIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find firstfaillogtime in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt64(firstfaillogtimeIdx, entry.m_firstFailLogTime);

            int firstfaillogfileIdx = statement->GetNamedParamIdx(":firstfaillogfile");
            if(!firstfaillogfileIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find firstfaillogfile in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueText(firstfaillogfileIdx, entry.m_firstFailLogFile.c_str());

            int lastfaillogtimeIdx = statement->GetNamedParamIdx(":lastfaillogtime");
            if(!lastfaillogtimeIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastfaillogtime in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt64(lastfaillogtimeIdx, entry.m_lastFailLogTime);

            int lastfaillogfileIdx = statement->GetNamedParamIdx(":lastfaillogfile");
            if(!lastfaillogfileIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastfaillogfile in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueText(lastfaillogfileIdx, entry.m_lastFailLogFile.c_str());

            int lastlogtimeIdx = statement->GetNamedParamIdx(":lastlogtime");
            if(!lastlogtimeIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastlogtime in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueInt64(lastlogtimeIdx, entry.m_lastLogTime);

            int lastlogfileIdx = statement->GetNamedParamIdx(":lastlogfile");
            if(!lastlogfileIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find lastlogfile in statement %s", UPDATE_JOB);
                return false;
            }
            statement->BindValueText(lastlogfileIdx, entry.m_lastLogFile.c_str());

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to execute %s to update job (key %i)", UPDATE_JOB, entry.m_jobID);
                return false;
            }

            return true;
        }
    }

    // this must actually delete the job
    bool AssetDatabaseConnection::RemoveJob(AZ::s64 jobID)
    {
        if(!ValidateDatabaseTable(DELETE_JOB, "Jobs"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Jobs table");
            return false;
        }

        ScopedTransaction transaction(m_databaseConnection);

        StatementAutoFinalizer autoFinal(*m_databaseConnection, DELETE_JOB);
        Statement* statement = autoFinal.Get();
        if(!statement)
        {
            AZ_Error(LOG_NAME, statement, "Could not get statement: %s", DELETE_JOB);
            return false;
        }

        int jobIDIdx = statement->GetNamedParamIdx(":jobid");
        if(!jobIDIdx)
        {
            AZ_Error(LOG_NAME, false, "could not find jobid in statement %s", DELETE_JOB);
            return false;
        }
        statement->BindValueInt64(jobIDIdx, jobID);

        if(statement->Step() == Statement::SqlError)
        {
            AZ_Warning(LOG_NAME, false, "Failed to RemoveSource form the database");
            return false;
        }

        transaction.Commit();

        return true;
    }

    bool AssetDatabaseConnection::RemoveJobs(JobDatabaseEntryContainer& container)
    {
        bool succeeded = true;
        for(auto& entry : container)
        {
            succeeded &= RemoveJob(entry.m_jobID);
            if(succeeded)
            {
                entry.m_jobID = -1; //set it to -1 as the id is no longer valid
            }
        }

        return succeeded;
    }

    bool AssetDatabaseConnection::RemoveJobByProductID(AZ::s64 productID)
    {
        JobDatabaseEntry job;
        bool succeeded = GetJobByProductID(productID, job);
        if(succeeded)
        {
            succeeded &= RemoveJob(job.m_jobID);
        }
        return succeeded;
    }

    bool AssetDatabaseConnection::GetProductByProductID(AZ::s64 productID, ProductDatabaseEntry& entry)
    {
        bool found = false;
        bool succeeded = QueryProductByProductID(productID,
            [&](ProductDatabaseEntry& product)
            {
                found = true;
                entry = AZStd::move(product);
                return false;//only one
            });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetProducts(ProductDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductsTable(
                [&](ProductDatabaseEntry& product)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(product);
                return true;//all
            }, builderGuid,
               jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
               platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
               status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetProductsByProductName(QString exactProductName, ProductDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductByProductName(exactProductName.toUtf8().constData(),
            [&](ProductDatabaseEntry& product)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(product);
            return true;//all
        }, builderGuid,
           jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
           platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
           status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetProductsLikeProductName(QString likeProductName, LikeType likeType, ProductDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductLikeProductName(likeProductName.toUtf8().constData(), likeType,
                [&](ProductDatabaseEntry& product)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(product);
                return true;//all
            }, builderGuid,
               jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
               platform.isEmpty() ? nullptr : platform.toUtf8().constData());
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetProductsBySourceName(QString exactSourceName, ProductDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductBySourceName(exactSourceName.toUtf8().constData(),
                [&](ProductDatabaseEntry& product)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(product);
                return true;//all
            }, builderGuid,
               jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
               platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
               status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetProductsLikeSourceName(QString likeSourceName, LikeType likeType, ProductDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryProductLikeSourceName(likeSourceName.toUtf8().constData(), likeType,
            [&](ProductDatabaseEntry& product)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(product);
            return true;//all
        }, builderGuid,
            jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
            platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
            status);
        return found && succeeded;
    }
    
    bool AssetDatabaseConnection::GetProductsBySourceID(AZ::s64 sourceID, ProductDatabaseEntryContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryCombinedBySourceID(sourceID,
            [&](CombinedDatabaseEntry& combined)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(combined);
                return true;//all
            }, builderGuid,
               jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
               platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
               status);
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetProductsByJobID(AZ::s64 jobID, ProductDatabaseEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QueryCombinedByJobID(jobID,
            [&](CombinedDatabaseEntry& combined)
            {
                found = true;
                container.push_back();
                container.back() = AZStd::move(combined);
                return true;//all
            });
        return found && succeeded;
    }

    //! For a given source, set the list of products for that source.
    //! Removes any data that's present and overwrites it with the new list
    //! Note that an empty list is in fact acceptable data, it means the source emitted no products
    bool AssetDatabaseConnection::SetProduct(ProductDatabaseEntry& entry)
    {
        if(!ValidateDatabaseTable(INSERT_PRODUCT, "Products"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Products table");
            return false;
        }

        if(entry.m_productID == -1)
        {
            //they didn't set an id, add to database

            //make sure its not already in the database
            ProductDatabaseEntryContainer existingProducts;
            if(GetProductsByJobID(entry.m_jobPK, existingProducts))
            {
                for(const auto& existingProduct : existingProducts)
                {
                    if(existingProduct == entry)
                    {
                        //this product already exists
                        entry.m_productID = existingProduct.m_productID;
                        return true;
                    }
                }
            }

            // scope created for the statement.
            StatementAutoFinalizer autoFinalizer(*m_databaseConnection, INSERT_PRODUCT);
            Statement* statement = autoFinalizer.Get();
            AZ_Assert(statement, "Statement not found: %s", INSERT_PRODUCT);

            int jobIdx = statement->GetNamedParamIdx(":jobid");
            if(!jobIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :jobid for %s ", INSERT_PRODUCT);
                return false;
            }
            statement->BindValueInt64(jobIdx, entry.m_jobPK);

            int subIdIdx = statement->GetNamedParamIdx(":subid");
            if(!subIdIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :subid for %s ", INSERT_PRODUCT);
                return false;
            }
            statement->BindValueInt(subIdIdx, entry.m_subID);
            
            int productNameIdx = statement->GetNamedParamIdx(":productname");
            if(!productNameIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :productname for %s ", INSERT_PRODUCT);
                return false;
            }
            statement->BindValueText(productNameIdx, entry.m_productName.c_str());

            int assetTypeIdx = statement->GetNamedParamIdx(":assettype");
            if(!assetTypeIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :assettype for %s ", INSERT_PRODUCT);
                return false;
            }
            statement->BindValueUuid(assetTypeIdx, entry.m_assetType);

            int legacyGuidIdx = statement->GetNamedParamIdx(":legacyguid");
            if(!legacyGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :legacyguid for %s ", INSERT_PRODUCT);
                return false;
            }
            statement->BindValueUuid(legacyGuidIdx, entry.m_legacyGuid);

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to execute the INSERT_PRODUCT statement");
                return false;
            }

            //now read it from the database
            existingProducts.clear();
            if(GetProductsByJobID(entry.m_jobPK, existingProducts))
            {
                for(const auto& existingProduct : existingProducts)
                {
                    if(existingProduct == entry)
                    {
                        //this product already exists
                        entry.m_productID = existingProduct.m_productID;
                        return true;
                    }
                }
            }

            return false;
        }
        else
        {
            //they supplied an id, see if it exists in the database
            ProductDatabaseEntry existingEntry;
            if(!GetProductByProductID(entry.m_productID, existingEntry))
            {
                AZ_Error(LOG_NAME, false, "Failed to write the product into the database.");
                return false;
            }

            //if the product is now different update it
            if(existingEntry == entry)
            {
                return true;
            }

            //its in the database already, update the database
            // it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, UPDATE_PRODUCT);
            Statement* statement = autoFinal.Get();
            if(!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", UPDATE_PRODUCT);
                return false;
            }

            int productIdx = statement->GetNamedParamIdx(":productid");
            if(!productIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :productid for %s ", UPDATE_PRODUCT);
                return false;
            }
            statement->BindValueInt64(productIdx, entry.m_productID);

            int jobIdx = statement->GetNamedParamIdx(":jobid");
            if(!jobIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :jobid for %s ", UPDATE_PRODUCT);
                return false;
            }
            statement->BindValueInt64(jobIdx, entry.m_jobPK);

            int subIdIdx = statement->GetNamedParamIdx(":subid");
            if(!subIdIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :subid for %s ", UPDATE_PRODUCT);
                return false;
            }
            statement->BindValueInt(subIdIdx, entry.m_subID);

            int productNameIdx = statement->GetNamedParamIdx(":productname");
            if(!productNameIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :productname for %s ", UPDATE_PRODUCT);
                return false;
            }
            statement->BindValueText(productNameIdx, entry.m_productName.c_str());

            int assetTypeIdx = statement->GetNamedParamIdx(":assetType");
            if(!assetTypeIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :assetType for %s ", UPDATE_PRODUCT);
                return false;
            }
            statement->BindValueUuid(assetTypeIdx, entry.m_assetType);

            int legacyGuidIdx = statement->GetNamedParamIdx(":legacyguid");
            if(!legacyGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :legacyguid for %s ", UPDATE_PRODUCT);
                return false;
            }
            statement->BindValueUuid(legacyGuidIdx, entry.m_legacyGuid);

            if(statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to execute %s to update (key %i)", UPDATE_PRODUCT, entry.m_productID);
                return false;
            }

            return true;
        }
    }

    bool AssetDatabaseConnection::SetProducts(ProductDatabaseEntryContainer& container)
    {
        bool succeeded = false;
        for(auto& entry : container)
        {
            succeeded &= SetProduct(entry);
        }
        return succeeded;
    }

    //! Clear the products for a given source.  This removes the entry entirely, not just sets it to empty.
    bool AssetDatabaseConnection::RemoveProduct(AZ::s64 productID)
    {
        if(!ValidateDatabaseTable("RemoveProduct", "Products"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Products table");
            return false;
        }

        ScopedTransaction transaction(m_databaseConnection);

        // scope created for the statement.
        StatementAutoFinalizer autoFinalizer(*m_databaseConnection, DELETE_PRODUCT);
        Statement* statement = autoFinalizer.Get();
        AZ_Assert(statement, "Statement not found: %s", DELETE_PRODUCT);

        int productIDIdx = statement->GetNamedParamIdx(":productid");
        if(!productIDIdx)
        {
            AZ_Error(LOG_NAME, false, "Could not find the Idx for :productid for %s ", DELETE_PRODUCT);
            return false;
        }
        statement->BindValueInt64(productIDIdx, productID);

        if(statement->Step() == Statement::SqlError)
        {
            AZ_Warning(LOG_NAME, false, "Failed to execute the DELETE_PRODUCT statement on productID %i", productID);
            return false;
        }

        transaction.Commit();

        return true;
    }

    bool AssetDatabaseConnection::RemoveProducts(ProductDatabaseEntryContainer& container)
    {
        bool succeeded = true;
        for(auto& entry : container)
        {
            succeeded &= RemoveProduct(entry.m_productID);
            if(succeeded)
            {
                entry.m_productID = -1;
            }
        }
        return succeeded;
    }

    bool AssetDatabaseConnection::RemoveProductsByJobID(AZ::s64 jobID)
    {
        if(!ValidateDatabaseTable(DELETE_PRODUCTS_BY_JOBID, "Products"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Jobs or Products table");
            return false;
        }

        ScopedTransaction transaction(m_databaseConnection);

        // scope created for the statement.
        StatementAutoFinalizer autoFinalizer(*m_databaseConnection, DELETE_PRODUCTS_BY_JOBID);
        Statement* statement = autoFinalizer.Get();
        AZ_Assert(statement, "Statement not found: %s", DELETE_PRODUCTS_BY_JOBID);

        int jobIdx = statement->GetNamedParamIdx(":jobid");
        if(!jobIdx)
        {
            AZ_Error(LOG_NAME, false, "Could not find the Idx for :jobid for %s",
                DELETE_PRODUCTS_BY_JOBID);
            return false;
        }
        statement->BindValueInt64(jobIdx, jobID);

        if(statement->Step() == Statement::SqlError)
        {
            AZ_Warning(LOG_NAME, false, "Failed to execute the %s statement on jobID %i", jobID);
            return false;
        }

        transaction.Commit();

        return true;
    }

    bool AssetDatabaseConnection::RemoveProductsBySourceID(AZ::s64 sourceID, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        if(!builderGuid.IsNull() || jobKey != nullptr)
        {
            //we have to do custom query the delete
            ProductDatabaseEntryContainer products;
            bool succeeded = GetProductsBySourceID(sourceID, products, builderGuid, jobKey, platform, status);
            if(succeeded)
            {
                succeeded &= RemoveProducts(products);
            }
            return succeeded;
        }

        if( !ValidateDatabaseTable("RemoveProductsBySourceID", "Jobs") ||
            !ValidateDatabaseTable("RemoveProductsBySourceID", "Products"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Jobs or Products table");
            return false;
        }

        const char* name = DELETE_PRODUCTS_BY_SOURCEID;
        if(!platform.isEmpty())
        {
            name = DELETE_PRODUCTS_BY_SOURCEID_PLATFORM;
        }

        ScopedTransaction transaction(m_databaseConnection);

        // scope created for the statement.
        StatementAutoFinalizer autoFinalizer(*m_databaseConnection, name);
        Statement* statement = autoFinalizer.Get();
        AZ_Assert(statement, "Statement not found: %s", name);

        int sourceIdx = statement->GetNamedParamIdx(":sourceid");
        if(!sourceIdx)
        {
            AZ_Error(LOG_NAME, false, "Could not find the Idx for :sourceid for %s",
                name);
            return false;
        }
        statement->BindValueInt64(sourceIdx, sourceID);

        AZStd::string platformStr;
        if(name == DELETE_PRODUCTS_BY_SOURCEID_PLATFORM)
        {
            int platformIdx = statement->GetNamedParamIdx(":platform");
            if(!platformIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find the Idx for :platform for %s ", name);
                return false;
            }
            platformStr = platform.toUtf8().constData();
            statement->BindValueText(platformIdx, platformStr.c_str());
        }

        if(statement->Step() == Statement::SqlError)
        {
            AZ_Warning(LOG_NAME, false, "Failed to execute the %s statement on sourceID %i", name, sourceID);
            return false;
        }

        transaction.Commit();

        return true;
    }

    bool AssetDatabaseConnection::GetJobInfoByJobID(AZ::s64 jobID, JobInfo& entry)
    {
        bool found = false;
        bool succeeded = QueryJobInfoByJobID(jobID,
            [&](JobInfo& jobInfo)
        {
            found = true;
            entry = AZStd::move(jobInfo);
            return true;//all
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobInfoByJobKey(AZStd::string jobKey, JobInfoContainer& container)
    {
        bool found = false;
        bool succeeded = QueryJobInfoByJobKey(jobKey,
            [&](JobInfo& jobInfo)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(jobInfo);
            return true;//all
        });
        return found && succeeded;

    }

    bool AssetDatabaseConnection::GetJobInfoByJobRunKey(AZ::u64 jobRunKey, JobInfoContainer& container)
    {
        bool found = false;
        bool succeeded = QueryJobInfoByJobRunKey(jobRunKey,
            [&](JobInfo& jobInfo)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(jobInfo);
            return true;//all
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetJobInfoBySourceName(QString exactSourceName, JobInfoContainer& container, AZ::Uuid builderGuid, QString jobKey, QString platform, JobStatus status)
    {
        bool found = false;
        bool succeeded = QueryJobInfoBySourceName(exactSourceName.toUtf8().constData(),
            [&](JobInfo& jobInfo)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(jobInfo);
            return true;//all
        }, builderGuid,
            jobKey.isEmpty() ? nullptr : jobKey.toUtf8().constData(),
            platform.isEmpty() ? nullptr : platform.toUtf8().constData(),
            status);
        return found && succeeded;
    }
   
    bool AssetDatabaseConnection::SetSourceFileDependencies(SourceFileDependencyEntryContainer& container)
    {
        bool succeeded = true;
        for (auto& entry : container)
        {
            succeeded = succeeded && SetSourceFileDependency(entry);
        }
        return succeeded;
    }

    bool AssetDatabaseConnection::SetSourceFileDependency(SourceFileDependencyEntry& entry)
    {
        if (!ValidateDatabaseTable(INSERT_SOURCE_DEPENDENCY, "SourceDependency"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Source Dependency table");
            return false;
        }

        if (entry.m_sourceDependencyID == -1)
        {
            //they didn't supply an id, add to database

            //first make sure its not already in the database
            SourceFileDependencyEntry existingEntry;
            if (GetSourceFileDependency(entry, existingEntry))
            {
                // We already have this entry in the database
                return true;
            }

            // it is a single statement, do not wrap it in a transaction, this wastes a lot of time.
            StatementAutoFinalizer autoFinal(*m_databaseConnection, INSERT_SOURCE_DEPENDENCY);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", INSERT_SOURCE_DEPENDENCY);
                return false;
            }

            int builderGuidIdx = statement->GetNamedParamIdx(":builderGuid");
            if (!builderGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find builderGuid in statement %s", INSERT_SOURCE_DEPENDENCY);
                return false;
            }
            statement->BindValueUuid(builderGuidIdx, entry.m_builderGuid);

            int sourceIdx = statement->GetNamedParamIdx(":source");
            if (!sourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find source in statement %s", INSERT_SOURCE_DEPENDENCY);
                return false;
            }
            statement->BindValueText(sourceIdx, entry.m_source.c_str());

            int dependsOnSourceIdx = statement->GetNamedParamIdx(":dependsOnSource");
            if (!dependsOnSourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find dependsOnSource in statement %s", INSERT_SOURCE_DEPENDENCY);
                return false;
            }

            statement->BindValueText(dependsOnSourceIdx, entry.m_dependsOnSource.c_str());

            if (statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to write the new source dependency into the database.");
                return false;
            }

            //now that its in the database get the id
            if (GetSourceFileDependency(entry, existingEntry))
            {
                entry.m_sourceDependencyID = existingEntry.m_sourceDependencyID;
                return true;
            }

            AZ_Error(LOG_NAME, false, "Failed to read the new source dependency into the database.");
            return false;
        }
        else
        {
            //they supplied an id, see if it exists in the database
            SourceFileDependencyEntry existingEntry;
            if (!GetSourceFileDependencyBySourceDependencyId(entry.m_sourceDependencyID, existingEntry))
            {
                //they supplied an id but is not in the database!
                AZ_Error(LOG_NAME, false, "Failed to write the source dependency into the database.");
                return false;
            }

            // don't bother updating the database if all fields are equal.
            if ((existingEntry.m_builderGuid == entry.m_builderGuid) &&
                (existingEntry.m_source == entry.m_source) &&
                (existingEntry.m_dependsOnSource == entry.m_dependsOnSource))
            {
                return true;
            }

            StatementAutoFinalizer autoFinal(*m_databaseConnection, UPDATE_SOURCE_DEPENDENCY);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", UPDATE_SOURCE_DEPENDENCY);
                return false;
            }

            int builderGuidIdx = statement->GetNamedParamIdx(":builderGuid");
            if (!builderGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find builderGuid in statement %s", UPDATE_SOURCE_DEPENDENCY);
                return false;
            }
            statement->BindValueUuid(builderGuidIdx, entry.m_builderGuid);

            int sourceIdx = statement->GetNamedParamIdx(":source");
            if (!sourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find source in statement %s", UPDATE_SOURCE_DEPENDENCY);
                return false;
            }
            statement->BindValueText(sourceIdx, entry.m_source.c_str());

            int dependsOnSourceIdx = statement->GetNamedParamIdx(":dependsOnSource");
            if (!dependsOnSourceIdx)
            {
                AZ_Error(LOG_NAME, false, "could not find dependsOnSource in statement %s", UPDATE_SOURCE_DEPENDENCY);
                return false;
            }
            statement->BindValueText(dependsOnSourceIdx, entry.m_dependsOnSource.c_str());

            if (statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to execute %s to update source dependency (key %i)", UPDATE_SOURCE_DEPENDENCY, entry.m_sourceDependencyID);
                return false;
            }

            return true;
        }
    }

    bool AssetDatabaseConnection::RemoveSourceFileDependencies(SourceFileDependencyEntryContainer& container)
    {
        bool succeeded = true;
        for (auto& entry : container)
        {
            succeeded = succeeded && RemoveSourceFileDependency(entry);
            if (succeeded)
            {
                entry.m_sourceDependencyID = -1;//set it to -1 as it no longer exists
            }
        }
        return succeeded;
    }

    bool AssetDatabaseConnection::RemoveSourceFileDependency(const SourceFileDependencyEntry& entry)
    {
        if (!ValidateDatabaseTable(DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID, "SourceDependency"))
        {
            AZ_Error(LOG_NAME, false, "Could not find Source Dependency table");
            return false;
        }

        if (entry.m_sourceDependencyID == -1)
        {
            //they didn't supply an id, check to make sure that an entry exists in the database and than delete it
            SourceFileDependencyEntry existingEntry;
            if (GetSourceFileDependency(entry, existingEntry))
            {
                return RemoveSourceFileDependency(existingEntry);
            }

            return true; // no such entry exists in the database
        }
        else
        {
            //they supplied an id, see if it exists in the database
            SourceFileDependencyEntry existingEntry;
            if (!GetSourceFileDependencyBySourceDependencyId(entry.m_sourceDependencyID, existingEntry))
            {
                //they supplied an id but is not found in the database!
                AZ_Warning(LOG_NAME, false, "Could not find SourceDependencyid in the database.");
                return true;
            }

            ScopedTransaction transaction(m_databaseConnection);

            StatementAutoFinalizer autoFinal(*m_databaseConnection, DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, statement, "Could not get statement: %s", DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID);
                return false;
            }

            int sourceDependencyID = statement->GetNamedParamIdx(":sourceDependencyId");
            if (!sourceDependencyID)
            {
                AZ_Error(LOG_NAME, false, "could not find sourceid in statement %s", DELETE_SOURCE_DEPENDENCY_SOURCEDEPENDENCYID);
                return false;
            }

            statement->BindValueInt64(sourceDependencyID, existingEntry.m_sourceDependencyID);

            if (statement->Step() == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Failed to RemoveSourceDependency form the database");
                return false;
            }

            transaction.Commit();

            return true;
        }
    }

    bool AssetDatabaseConnection::GetSourceFileDependency(const SourceFileDependencyEntry& inputEntry, SourceFileDependencyEntry& databaseEntry)
    {
        bool found = false;
        bool succeeded = QuerySourceDependency(inputEntry.m_builderGuid, inputEntry.m_source.c_str(), inputEntry.m_dependsOnSource.c_str(),
            [&](SourceFileDependencyEntry& entry)
        {
            found = true;
            databaseEntry = AZStd::move(entry);
            return false; //one
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourceFileDependenciesByBuilderGUIDAndSource(const AZ::Uuid& builderGuid, const char* source, SourceFileDependencyEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QuerySourceDependencyByBuilderGUIDAndSource(builderGuid, source,
            [&](SourceFileDependencyEntry& entry)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(entry);
            return true;//all
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourceFileDependenciesByDependsOnSource(QString dependsOnSource, SourceFileDependencyEntryContainer& container)
    {
        bool found = false;
        bool succeeded = QuerySourceDependencyByDependsOnSource(dependsOnSource.toUtf8().constData(),
            [&](SourceFileDependencyEntry& entry)
        {
            found = true;
            container.push_back();
            container.back() = AZStd::move(entry);
            return true;//all
        });
        return found && succeeded;
    }

    bool AssetDatabaseConnection::GetSourceFileDependencyBySourceDependencyId(AZ::s64 sourceDependencyId, SourceFileDependencyEntry& sourceDependencyEntry)
    {
        bool found = false;
        bool succeeded = QuerySourceDependencyBySourceDependencyId(sourceDependencyId,
            [&](SourceFileDependencyEntry& entry)
        {
            found = true;
            sourceDependencyEntry = AZStd::move(entry);
            return false; //one
        });
        return found && succeeded;
    }


}//namespace AssetProcessor

