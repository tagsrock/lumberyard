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

#include "SQLiteConnection.h"

// note that this includes the 3rd Party sqlite implementation.
// if we need to add compile switches, we would add them here.
#include <AzCore/std/parallel/lock.h>
#include <AzCore/Math/Uuid.h>
#include <SQLite/sqlite3.h>

namespace AzToolsFramework
{
    namespace SQLite
    {
        /**  A statement prototype represents a registered statement ("SELECT * FROM assets WHERE assets.name = :name")
        * To actually execute it, you'd call GetStatement on the manager which will create for you a Statement from a prototype
        */
        class StatementPrototype
        {
        public:
            AZ_CLASS_ALLOCATOR(StatementPrototype, AZ::SystemAllocator, 0)
            StatementPrototype();
            StatementPrototype(const AZStd::string& stmt);
            ~StatementPrototype();

            void SetSqlText(const AZStd::string& txt) { m_sqlText = txt; }
            AZStd::string GetSqlText() { return m_sqlText; }

            Statement* Prepare(sqlite3* db); // get a copy of a statement ready to execute
            void RetireStatement(Statement* finishedWithStatement); // finished with a statement, return it to pool
        private:
            AZStd::string m_sqlText;
            AZStd::vector<Statement*> m_cachedPreparedStatements;
            AZStd::recursive_mutex m_mutex;
            sqlite3* m_db;


            StatementPrototype(const StatementPrototype& other); // forbid ordinary copy construct
            StatementPrototype& operator=(const StatementPrototype& other); // forbid operator=
        };


        Connection::Connection(void)
            : m_db(NULL)
        {
        }

        Connection::~Connection(void)
        {
            Close();
        }

        bool Connection::IsOpen() const
        {
            return (m_db != NULL);
        }

        bool Connection::Open(const AZStd::string& filename, bool readOnly)
        {
            AZ_Assert(m_db == NULL, "You have to close the database prior to opening a new one.");

            int res = 0;
            if (readOnly)
            {
                res = sqlite3_open_v2(filename.c_str(), &m_db, SQLITE_OPEN_READONLY, nullptr);
            }
            else
            {
                res = sqlite3_open(filename.c_str(), &m_db);
            }

            if (res != SQLITE_OK)
            {
                return false;
            }

            //AZ_Assert(m_db, "Unable to open sql database at %s", filename.c_str());
            //AZ_Assert(res == SQLITE_OK, "Database returned an errror when trying to open %s", filename.c_str());
            sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
            //WAL journal mode enabled for better concurrency with external asset browser.
            //Reads do not block writes
            sqlite3_exec(m_db, "PRAGMA journal_mode = wal;", NULL, NULL, NULL);  // we'll journal using WAL strategy
            sqlite3_exec(m_db, "PRAGMA cache_size = 160000;", NULL, NULL, NULL);

            // turn sync off - you will lose data on power loss but all the data can be rebuilt from cache anyway.
            // you still don't lose data if the application crashes, only if you literally lose power while the disk is writing.
            // and because you're in WAL mode, you only lose the current transaction anyway.
            sqlite3_exec(m_db, "PRAGMA synchronous = 0;", NULL, NULL, NULL);
            return      (res == SQLITE_OK);
        }

        void Connection::Close()
        {
            if (m_db)
            {
                FinalizeAll();
                sqlite3_close(m_db);
                m_db = NULL;
            }
        }

        void Connection::FinalizeAll()
        {
            for (auto it : m_statementPrototypes)
            {
                delete it.second;
            }
            m_statementPrototypes.clear();
        }

        void Connection::AddStatement(const AZStd::string& shortName, const AZStd::string& sqlText)
        {
            AZ_Assert(m_statementPrototypes.find(shortName) == m_statementPrototypes.end(), "You may not register the same prototype twice");

            m_statementPrototypes[shortName] = aznew StatementPrototype(sqlText);//, AZStd::move(StatementPrototype(sqlText))));
        }

        void Connection::RemoveStatement(const char* name)
        {
            auto it = m_statementPrototypes.find(name);

            AZ_Assert(it != m_statementPrototypes.end(), "Asked to remove a statement: %s : which does not currently exist\n", name);
            if (it == m_statementPrototypes.end())
            {
                return;
            }

            delete it->second;
            m_statementPrototypes.erase(it);
        }

        void Connection::AddStatement(const char* shortName, const char* sqlText)
        {
            AddStatement(AZStd::string(shortName), AZStd::string(sqlText));
        }

        Statement* Connection::GetStatement(const AZStd::string& stmtName)
        {
            auto item = m_statementPrototypes.find(stmtName);
            if (item == m_statementPrototypes.end())
            {
                AZ_Assert(0, "Invalid statement requested from the sql connection '%s'", stmtName.c_str());
                return nullptr;
            }

            return item->second->Prepare(m_db);
        }



        Statement* StatementPrototype::Prepare(sqlite3* db)
        {
            {
                AZStd::lock_guard<AZStd::recursive_mutex> myLocker(m_mutex);
                if (!m_cachedPreparedStatements.empty())
                {
                    Statement* prePrepared = m_cachedPreparedStatements.back();
                    m_cachedPreparedStatements.pop_back();
                    return prePrepared;
                }
            }

            // if we get here, we have no such prepared statement
            Statement* newStatement = aznew Statement(this);
            newStatement->PrepareFirstTime(db);
            return newStatement;
        }

        void StatementPrototype::RetireStatement(Statement* finishedWithStatement)
        {
            AZ_Assert(finishedWithStatement, "null statement");
            if (!finishedWithStatement)
            {
                return;
            }

            AZ_Assert(finishedWithStatement->GetParentPrototype() == this, "Invalid call to retire a statement to wrong parent.");
            if (finishedWithStatement->GetParentPrototype() != this)
            {
                return;
            }

            finishedWithStatement->Reset();
            {
                AZStd::lock_guard<AZStd::recursive_mutex> myLock(m_mutex);
                m_cachedPreparedStatements.push_back(finishedWithStatement);
            }
        }

        void Connection::BeginTransaction()
        {
            AZ_Assert(m_db, "BeginTransaction:  Database is not open!");
            sqlite3_exec(m_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
        }

        void Connection::CommitTransaction()
        {
            AZ_Assert(m_db, "CommitTransaction:  Database is not open!");
            sqlite3_exec(m_db, "COMMIT TRANSACTION;", NULL, NULL, NULL);
        }

        void Connection::RollbackTransaction()
        {
            AZ_Assert(m_db, "RollbackTransaction:  Database is not open!");
            sqlite3_exec(m_db, "ROLLBACK;", NULL, NULL, NULL);
        }

        void Connection::Vacuum()
        {
            AZ_Assert(m_db, "Vacuum:  Database is not open!");
            sqlite3_exec(m_db, "VACUUM;", NULL, NULL, NULL);
        }

        int Connection::GetLastRowID()
        {
            AZ_Assert(m_db, "GetLastRowID:  Database is not open!");
            return (int)sqlite3_last_insert_rowid(m_db);
        }

        int Connection::GetNumAffectedRows()
        {
            return sqlite3_changes(m_db);
        }

        bool Connection::ExecuteOneOffStatement(const char* name)
        {
            AZ_Assert(IsOpen(), "Invalid operation - Database is not open.");
            AZ_Assert(name, "Invalid input - name is not valid");

            if ((!IsOpen()) || (!name))
            {
                return false;
            }

            Statement* pState = GetStatement(name);
            if (!pState)
            {
                return false;
            }

            int res = pState->Step();
            pState->Finalize();

            if (res == Statement::SqlError)
            {
                return false;
            }

            return true;
        }

        bool Connection::DoesTableExist(const char* name)
        {
            AZ_Assert(IsOpen(), "Invalid operation - Database is not open.");
            AZ_Assert(name, "Invalid input - name is not valid");
            if (!IsOpen())
            {
                return false;
            }
            if (!name)
            {
                return false;
            }

            StatementPrototype stmt("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=:1;");
            Statement* execute = stmt.Prepare(m_db); // execute now belongs to stmt and will die when stmt leaves scope.

            execute->BindValueText(1, name);
            int res = execute->Step();
            if (res == Statement::SqlError)
            {
                execute->Finalize();
                return false;
            }

            bool hasData = execute->GetColumnInt(0) != 0;
            execute->Finalize();

            return hasData;
        }

        /////////////////////////////////////////////////

        void Statement::Finalize()
        {
            if (m_parentPrototype)
            {
                m_parentPrototype->RetireStatement(this);
            }
        }


        Statement::Statement(StatementPrototype* parent)
            : m_statement(NULL)
            , m_parentPrototype(parent)
        {
        }

        Statement::~Statement()
        {
            if (m_statement)
            {
                sqlite3_finalize(m_statement);
            }
            m_statement = nullptr;
        }

        StatementPrototype::StatementPrototype(const AZStd::string& sql)
            : m_sqlText(sql)
        {
        }

        StatementPrototype::~StatementPrototype()
        {
            for (auto element : m_cachedPreparedStatements)
            {
                delete element;
            }
        }

        bool Statement::PrepareFirstTime(sqlite3* db)
        {
            AZ_Assert(db, "Vacuum:  Database is null!");
            int res = sqlite3_prepare_v2(db, m_parentPrototype->GetSqlText().c_str(), (int)m_parentPrototype->GetSqlText().length(), &m_statement, NULL);
            AZ_Assert(res == SQLITE_OK, "Statement::Prepare: failed! %s ( prototype is '%s')", sqlite3_errmsg(db), m_parentPrototype->GetSqlText().c_str());
            return (res == SQLITE_OK);
        }

        bool Statement::Prepared() const
        {
            return (m_statement != NULL);
        }


        Statement::SqlStatus Statement::Step()
        {
            AZ_Assert(m_statement, "Statement::Step: Statement not ready!");
            int res = SQLITE_BUSY;
            while (res == SQLITE_BUSY)
            {
                res = sqlite3_step(m_statement);
            }

            AZ_Assert(res != SQLITE_ERROR, "Statement::Step: SQLITE_ERROR!");

            if (res == SQLITE_ROW)
            {
                return SqlOK;
            }
            else if (res == SQLITE_DONE)
            {
                return SqlDone;
            }
            return SqlError;
        }

        int Statement::FindColumn(const char* name)
        {
            AZ_Assert(m_statement, "Statement::FindColumn: Statement not ready!");
            if (!m_cachedColumnNames.empty())
            {
                auto it = m_cachedColumnNames.find(name);
                if (it == m_cachedColumnNames.end())
                {
                    return -1;
                }

                return it->second;
            }

            // build the cache:
            int columnCount = sqlite3_column_count(m_statement);
            if (columnCount == 0)
            {
                return -1;
            }

            for (int idx = 0; idx < columnCount; ++idx)
            {
                m_cachedColumnNames[sqlite3_column_name(m_statement, idx)] = idx;
            }

            return FindColumn(name);
        }


        AZStd::string   Statement::GetColumnText(int col)
        {
            AZ_Assert(m_statement, "Statement::GetColumnText: Statement not ready!");
            const unsigned char* str = sqlite3_column_text(m_statement, col);
            if (str)
            {
                return reinterpret_cast<const char*>(str);
            }
            return "";
        }

        int Statement::GetColumnInt(int col)
        {
            AZ_Assert(m_statement, "Statement::GetColumnInt: Statement not ready!");
            return sqlite3_column_int(m_statement, col);
        }

        AZ::s64     Statement::GetColumnInt64(int col)
        {
            AZ_Assert(m_statement, "Statement::GetColumnInt64: Statement not ready!");
            return sqlite3_column_int64(m_statement, col);
        }


        double  Statement::GetColumnDouble(int col)
        {
            AZ_Assert(m_statement, "Statement::GetColumnDouble: Statement not ready!");
            return sqlite3_column_double(m_statement, col);
        }

        int Statement::GetColumnBlobBytes(int col)
        {
            AZ_Assert(m_statement, "Statement::GetColumnBlobBytes: Statement not ready!");
            return sqlite3_column_bytes(m_statement, col);
        }

        const void* Statement::GetColumnBlob(int col)
        {
            AZ_Assert(m_statement, "Statement::GetColumnBlob: Statement not ready!");
            return sqlite3_column_blob(m_statement, col);
        }

        AZ::Uuid Statement::GetColumnUuid(int col)
        {
            const void* blobAddr = GetColumnBlob(col);
            int blobBytes = GetColumnBlobBytes(col);
            AZ::Uuid newUuid;
            AZ_Assert(blobBytes == sizeof(newUuid.data), "Database does not contain a UUID");

            memcpy(newUuid.data, blobAddr, blobBytes);
            return newUuid;
        }

        bool Statement::BindValueBlob(int idx, void* data, int size)
        {
            AZ_Assert(m_statement, "Statement::GetColumnBlob: Statement not ready!");
            int res = sqlite3_bind_blob(m_statement, idx, data, size, nullptr);
            AZ_Assert(res == SQLITE_OK, "Statement::BindValueBlob: failed to bind!");
            return (res == SQLITE_OK);
        }


        bool Statement::BindValueUuid(int idx, const AZ::Uuid& data)
        {
            AZ_Assert(m_statement, "Statement::BindValueUuid: Statement not ready!");
            int res = sqlite3_bind_blob(m_statement, idx, data.data, sizeof(data.data), nullptr);
            AZ_Assert(res == SQLITE_OK, "Statement::BindValueUuid: failed to bind!");
            return (res == SQLITE_OK);
        }


        bool Statement::BindValueDouble(int idx, double data)
        {
            int res = sqlite3_bind_double(m_statement, idx, data);
            AZ_Assert(res == SQLITE_OK, "Statement::BindValueDouble: failed to bind!");
            return (res == SQLITE_OK);
        }

        bool Statement::BindValueInt(int idx, int data)
        {
            int res = sqlite3_bind_int(m_statement, idx, data);
            AZ_Assert(res == SQLITE_OK, "Statement::BindValueInt: failed to bind!");
            return     (res == SQLITE_OK);
        }

        bool Statement::BindValueInt64(int idx, AZ::s64 data)
        {
            int res = sqlite3_bind_int64(m_statement, idx, data);
            AZ_Assert(res == SQLITE_OK, "Statement::BindValueInt64: failed to bind!");
            return     (res == SQLITE_OK);
        }

        bool Statement::BindValueText(int idx, const char* data)
        {
            int res = sqlite3_bind_text(m_statement, idx, data, static_cast<int>(strlen(data)), NULL);
            AZ_Assert(res == SQLITE_OK, "Statement::BindValueText: failed to bind!");
            return     (res == SQLITE_OK);
        }

        bool Statement::Reset()
        {
            AZ_Assert(m_statement, "Statement::Reset: Statement not ready!");
            sqlite3_reset(m_statement);
            int res = sqlite3_clear_bindings(m_statement);
            AZ_Assert(res == SQLITE_OK, "Statement::sqlite3_clear_bindings: failed!");
            return    (res == SQLITE_OK);
        }

        int Statement::GetNamedParamIdx(const char* name)
        {
            AZ_Assert(m_statement, "Statement::GetNamedParamIdx: Statement not ready!");
            int returnVal = sqlite3_bind_parameter_index(m_statement, name);
            AZ_Assert(returnVal, "Parameter %s not found in statement %s!", name, m_parentPrototype->GetSqlText().c_str());
            return returnVal;
        }

        const StatementPrototype* Statement::GetParentPrototype() const
        {
            return m_parentPrototype;
        }

        StatementAutoFinalizer::StatementAutoFinalizer(Connection& connect, const char* statementName)
        {
            m_statement = connect.GetStatement(statementName);
        }

        StatementAutoFinalizer::~StatementAutoFinalizer()
        {
            m_statement->Finalize();
            m_statement = nullptr;
        }

        Statement* StatementAutoFinalizer::Get() const
        {
            return m_statement;
        }

        ScopedTransaction::ScopedTransaction(Connection* connect)
        {
            m_connection = connect;
            m_connection->BeginTransaction();
        }

        ScopedTransaction::~ScopedTransaction()
        {
            if (m_connection)
            {
                m_connection->RollbackTransaction();
                m_connection = nullptr;
            }
        }

        void ScopedTransaction::Commit()
        {
            if (m_connection)
            {
                m_connection->CommitTransaction();
                m_connection = nullptr;
            }
        }
    } // namespace SQLite
} // namespace AZFramework