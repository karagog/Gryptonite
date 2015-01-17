/*Copyright 2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include <grypto_entry.h>
#include <grypto_databasemodel.h>
#include <gutil/cryptopp_rng.h>
#include <QString>
#include <QtTest>
#include <QCoreApplication>
using namespace Grypt;

#define DATABASE_PATH "testdb.sqlite"

// You need the cryptopp rng because the cstdrng fails in Windows when called on a background thread
static GUtil::CryptoPP::RNG __cryptopp_rng;
static GUtil::RNG_Initializer __rng_init(&__cryptopp_rng);

bool __compare_entries(const Entry &lhs, const Entry &rhs)
{
    bool ret = lhs.GetName() == rhs.GetName() &&
            lhs.GetDescription() == rhs.GetDescription() &&
            lhs.GetFavoriteIndex() == rhs.GetFavoriteIndex() &&
            lhs.GetModifyDate().toTime_t() == rhs.GetModifyDate().toTime_t() &&
            lhs.GetId() == rhs.GetId() &&
            lhs.GetParentId() == rhs.GetParentId() &&
            lhs.GetRow() == rhs.GetRow() &&
            lhs.GetFileId() == rhs.GetFileId() &&
            lhs.GetFileName() == rhs.GetFileName() &&
            lhs.GetFilePath() == rhs.GetFilePath() &&
            lhs.Values().count() == rhs.Values().count();

    for(int i = 0; ret && i < lhs.Values().count(); ++i)
    {
        ret = lhs.Values()[i].GetName() == rhs.Values()[i].GetName() &&
                lhs.Values()[i].GetValue() == rhs.Values()[i].GetValue() &&
                lhs.Values()[i].GetNotes() == rhs.Values()[i].GetNotes() &&
                lhs.Values()[i].GetIsHidden() == rhs.Values()[i].GetIsHidden();
    }
    return ret;
}

class DatabasemodelTest : public QObject
{
    Q_OBJECT
    Credentials m_creds;

public:
    DatabasemodelTest();
    ~DatabasemodelTest();

private Q_SLOTS:
    void test_new_entry();
    void test_new_entry_with_file();
    void test_new_entry_with_siblings();
    void test_update_entry();
    void test_update_entry_with_file();
    void test_delete_entry();
    void test_delete_entry_with_siblings();
    void test_delete_entry_with_file();
    void test_move_entries_basic();
    void test_move_entries_down_same_parent();
    void test_move_entries_up_same_parent();

private:
    void _cleanup_database(){
        QFile::remove(DATABASE_PATH);
    }
};

DatabasemodelTest::DatabasemodelTest()
{
    qRegisterMetaType<std::shared_ptr<GUtil::Exception<>>>("std::shared_ptr<GUtil::Exception<>>");

    m_creds.Password = "password";
    _cleanup_database();

    // This creates and initializes an empty database
    DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
}

DatabasemodelTest::~DatabasemodelTest()
{
    //_cleanup_database();
}

void DatabasemodelTest::test_new_entry()
{
    // Test basic insertion of an entry
    Entry e;
    QDateTime modify_date(QDate(2015, 1, 3), QTime(20, 0, 0));
    e.SetName("entry1");
    e.SetDescription("description");
    e.SetFavoriteIndex(0);
    e.SetModifyDate(modify_date);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e);

        // Check that the new data is immediately accessible, because
        //  it may be added on a background thread.
        Entry e2 = dbm.FindEntryById(e.GetId());
        QVERIFY(__compare_entries(e, e2));
    }

    // Check that the data persists after the original object dies
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        Entry e2 = dbm.FindEntryById(e.GetId());
        QVERIFY(__compare_entries(e, e2));

        QVERIFY(e2.GetRow() == 0);
        QVERIFY(e2.GetName() == "entry1");
        QVERIFY(e2.GetDescription() == "description");
        QVERIFY(e2.GetFavoriteIndex() == 0);
        QVERIFY(e2.GetModifyDate() == modify_date);
    }


    // Test that insertion before the row affects the sibling row
    Entry e2;
    e2.SetName("entry2");
    e2.SetDescription("description2");
    e2.SetFavoriteIndex(1);
    e2.SetModifyDate(QDateTime::currentDateTime());
    e2.SetRow(0);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e2);

        Entry e3 = dbm.FindEntryById(e2.GetId());
        Entry e4 = dbm.FindEntryById(e.GetId());
        QVERIFY(e3.GetRow() == 0);
        QVERIFY(e4.GetRow() == 1);
    }

    // Make sure the changes persist
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        Entry e3 = dbm.FindEntryById(e.GetId());
        QVERIFY(e3.GetRow() == 1);
    }


    // Add an entry, then undo it
    Entry e3(e);
    e3.SetRow(1);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e3);
        QVERIFY(dbm.FindIndexById(e3.GetId()).isValid());

        e3 = dbm.FindEntryById(e3.GetId());
        Entry e4 = dbm.FindEntryById(e2.GetId());
        Entry e5 = dbm.FindEntryById(e.GetId());
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e3.GetRow() == 1);
        QVERIFY(e5.GetRow() == 2);

        dbm.Undo();
        e4 = dbm.FindEntryById(e2.GetId());
        e5 = dbm.FindEntryById(e.GetId());
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e5.GetRow() == 1);
        QVERIFY(!dbm.FindIndexById(e3.GetId()).isValid());

        dbm.Redo();
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e2.GetId());
        e5 = dbm.FindEntryById(e.GetId());
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e3.GetRow() == 1);
        QVERIFY(e5.GetRow() == 2);

        dbm.Undo();
    }

    // Make sure the changes persist
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        Entry e4 = dbm.FindEntryById(e2.GetId());
        Entry e5 = dbm.FindEntryById(e.GetId());
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e5.GetRow() == 1);
        QVERIFY(!dbm.FindIndexById(e3.GetId()).isValid());
    }
}

void DatabasemodelTest::test_new_entry_with_file()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_new_entry_with_siblings()
{
    _cleanup_database();

    Entry  e0, e1, e2, e3, e4, e5;
    {
        // Populate the siblings for the test
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e0);
        dbm.AddEntry(e1);
        dbm.AddEntry(e2);
        dbm.AddEntry(e3);
        dbm.AddEntry(e4);
        dbm.AddEntry(e5);

        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e2.GetRow() == 2);
        QVERIFY(e3.GetRow() == 3);
        QVERIFY(e4.GetRow() == 4);
        QVERIFY(e5.GetRow() == 5);
    }

    // Add an entry somewhere in the middle of the list of siblings
    Entry e_new;
    e_new.SetRow(2);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e_new);

        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        e_new = dbm.FindEntryById(e_new.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e_new.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e_new.GetRow() == 2);
        QVERIFY(e2.GetRow() == 3);
        QVERIFY(e3.GetRow() == 4);
        QVERIFY(e4.GetRow() == 5);
        QVERIFY(e5.GetRow() == 6);

        // Check that undoing works
        dbm.Undo();
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        bool exception_hit = false;
        try{
            // We should not be able to find the new entry
            dbm.FindEntryById(e_new.GetId());
        }catch(...){
            exception_hit = true;
        }
        QVERIFY(exception_hit);
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e2.GetRow() == 2);
        QVERIFY(e3.GetRow() == 3);
        QVERIFY(e4.GetRow() == 4);
        QVERIFY(e5.GetRow() == 5);

        dbm.Redo();
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        e_new = dbm.FindEntryById(e_new.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e_new.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e_new.GetRow() == 2);
        QVERIFY(e2.GetRow() == 3);
        QVERIFY(e3.GetRow() == 4);
        QVERIFY(e4.GetRow() == 5);
        QVERIFY(e5.GetRow() == 6);
    }

    // Check that the changes were persistent
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        e_new = dbm.FindEntryById(e_new.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e_new.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e_new.GetRow() == 2);
        QVERIFY(e2.GetRow() == 3);
        QVERIFY(e3.GetRow() == 4);
        QVERIFY(e4.GetRow() == 5);
        QVERIFY(e5.GetRow() == 6);
    }
}

void DatabasemodelTest::test_update_entry()
{
    _cleanup_database();

    Entry e;
    QDateTime modify_date(QDate(2015, 1, 3), QTime(20, 0, 0));
    e.SetName("entry1");
    e.SetDescription("description");
    e.SetFavoriteIndex(0);
    e.SetModifyDate(modify_date);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e);

        e = dbm.FindEntryById(e.GetId());
        QVERIFY(e.GetDescription() == "description");

        Entry e2(e);
        e2.SetDescription("new description");
        dbm.UpdateEntry(e2);
        QVERIFY(e2.GetId() == e.GetId());

        Entry e3 = dbm.FindEntryById(e2.GetId());
        QVERIFY(__compare_entries(e2, e3));
    }

    // Make sure the changes persist
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        Entry e2 = dbm.FindEntryById(e.GetId());
        QVERIFY(e2.GetDescription() == "new description");
    }


    // Test that undoing works
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        e.SetDescription("completely different");
        dbm.UpdateEntry(e);
        e = dbm.FindEntryById(e.GetId());
        QVERIFY(e.GetDescription() == "completely different");

        dbm.Undo();
        e = dbm.FindEntryById(e.GetId());
        QVERIFY(e.GetDescription() == "new description");

        dbm.Redo();
        e = dbm.FindEntryById(e.GetId());
        QVERIFY(e.GetDescription() == "completely different");
    }
}

void DatabasemodelTest::test_update_entry_with_file()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_delete_entry()
{
    Entry e;
    QDateTime modify_date(QDate(2015, 1, 3), QTime(20, 0, 0));
    e.SetName("entry1");
    e.SetDescription("description");
    e.SetFavoriteIndex(0);
    e.SetModifyDate(modify_date);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e);

        dbm.RemoveEntry(e);
        QVERIFY(!dbm.FindIndexById(e.GetId()).isValid());

        // Check that undoing brings the entry back
        dbm.Undo();
        QVERIFY(dbm.FindIndexById(e.GetId()).isValid());
        Entry e2 = dbm.FindEntryById(e.GetId());
        QVERIFY(__compare_entries(e, e2));

        // Redoing should delete it again
        dbm.Redo();
        QVERIFY(!dbm.FindIndexById(e.GetId()).isValid());
    }

    // Make sure the changes persist
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        QVERIFY(!dbm.FindIndexById(e.GetId()).isValid());
    }


    // Test that deleting a parent does not delete its children,
    //  at least until you tell it to delete the orphans
    Entry e2(e), e3(e);
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e);

        e2.SetParentId(e.GetId());
        dbm.AddEntry(e2);

        e3.SetParentId(e2.GetId());
        dbm.AddEntry(e3);

        e = dbm.FindEntryById(e.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        QVERIFY(e.GetParentId().IsNull());
        QVERIFY(e2.GetParentId() == e.GetId());
        QVERIFY(e3.GetParentId() == e2.GetId());

        dbm.RemoveEntry(e);
        QVERIFY(!dbm.FindIndexById(e.GetId()).isValid());

        // the orphans are still there
        QVERIFY(dbm.FindIndexById(e2.GetId()).isValid());
        QVERIFY(dbm.FindIndexById(e3.GetId()).isValid());

        // If I undo delete, the model should show the children again
        dbm.Undo();
        QModelIndex ind = dbm.FindIndexById(e.GetId());
        QVERIFY(ind.isValid());
        QVERIFY(dbm.rowCount(ind) == 1);
        QVERIFY(dbm.rowCount(dbm.index(0, 0, ind)) == 1);
        QVERIFY(dbm.FindIndexById(e2.GetId()).isValid());
        QVERIFY(dbm.FindIndexById(e3.GetId()).isValid());

        dbm.Redo();
        QVERIFY(!dbm.FindIndexById(e.GetId()).isValid());
        QVERIFY(dbm.FindIndexById(e2.GetId()).isValid());
        QVERIFY(dbm.FindIndexById(e3.GetId()).isValid());
    }

    // We deleted the parents, so the orphans will not be added to the
    //  index when we create a new database model
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        QVERIFY(!dbm.FindIndexById(e2.GetId()).isValid());
        QVERIFY(!dbm.FindIndexById(e3.GetId()).isValid());

        // You can manually delete the orphans, but they don't affect us at the model level
        dbm.DeleteOrphans();
    }
}

void DatabasemodelTest::test_delete_entry_with_file()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_delete_entry_with_siblings()
{
    _cleanup_database();

    Entry  e0, e1, e2, e3, e4, e5;
    {
        // Populate the siblings for the test
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e0);
        dbm.AddEntry(e1);
        dbm.AddEntry(e2);
        dbm.AddEntry(e3);
        dbm.AddEntry(e4);
        dbm.AddEntry(e5);

        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e2.GetRow() == 2);
        QVERIFY(e3.GetRow() == 3);
        QVERIFY(e4.GetRow() == 4);
        QVERIFY(e5.GetRow() == 5);
    }

    // Delete an entry somewhere in the middle of the list of siblings
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.RemoveEntry(e2);

        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        bool exception_hit = false;
        try{
            // We should not be able to find the deleted entry
            dbm.FindEntryById(e2.GetId());
        }catch(...){
            exception_hit = true;
        }
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(exception_hit);
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);

        // Check that undoing works
        dbm.Undo();
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e2.GetRow() == 2);
        QVERIFY(e3.GetRow() == 3);
        QVERIFY(e4.GetRow() == 4);
        QVERIFY(e5.GetRow() == 5);

        dbm.Redo();
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        exception_hit = false;
        try{
            dbm.FindEntryById(e2.GetId());
        }catch(...){
            exception_hit = true;
        }
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(exception_hit);
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);
    }

    // Check that the changes were persistent
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        bool exception_hit = false;
        try{
            dbm.FindEntryById(e2.GetId());
        }catch(...){
            exception_hit = true;
        }
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(exception_hit);
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);
    }
}

void DatabasemodelTest::test_move_entries_basic()
{
    _cleanup_database();

    Entry   e0,       e1;
    Entry e2, e3,   e4, e5;
    {
        // Populate the initial hierarchy for the test
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e0);
        dbm.AddEntry(e1);

        e2.SetParentId(e0.GetId());
        e3.SetParentId(e0.GetId());

        e4.SetParentId(e1.GetId());
        e5.SetParentId(e1.GetId());

        dbm.AddEntry(e2);
        dbm.AddEntry(e3);
        dbm.AddEntry(e4);
        dbm.AddEntry(e5);

        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == e0.GetId());
        QVERIFY(e3.GetParentId() == e0.GetId());
        QVERIFY(e4.GetParentId() == e1.GetId());
        QVERIFY(e5.GetParentId() == e1.GetId());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e2.GetRow() == 0);
        QVERIFY(e3.GetRow() == 1);
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e5.GetRow() == 1);
    }


    // Now move an entry from the middle of one parent to the middle of another parent
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.FetchAllEntries();
        dbm.MoveEntries(dbm.FindIndexById(e0.GetId()), 0, 0,
                        dbm.FindIndexById(e1.GetId()), 1);

        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == e1.GetId());
        QVERIFY(e3.GetParentId() == e0.GetId());
        QVERIFY(e4.GetParentId() == e1.GetId());
        QVERIFY(e5.GetParentId() == e1.GetId());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e3.GetRow() == 0);
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e5.GetRow() == 2);


        // Try undoing the move
        dbm.Undo();
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == e0.GetId());
        QVERIFY(e3.GetParentId() == e0.GetId());
        QVERIFY(e4.GetParentId() == e1.GetId());
        QVERIFY(e5.GetParentId() == e1.GetId());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e2.GetRow() == 0);
        QVERIFY(e3.GetRow() == 1);
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e5.GetRow() == 1);

        dbm.Redo();
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == e1.GetId());
        QVERIFY(e3.GetParentId() == e0.GetId());
        QVERIFY(e4.GetParentId() == e1.GetId());
        QVERIFY(e5.GetParentId() == e1.GetId());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e3.GetRow() == 0);
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e5.GetRow() == 2);
    }

    // Make sure changes are persistent
    {
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        e0 = dbm.FindEntryById(e0.GetId());
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e0.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == e1.GetId());
        QVERIFY(e3.GetParentId() == e0.GetId());
        QVERIFY(e4.GetParentId() == e1.GetId());
        QVERIFY(e5.GetParentId() == e1.GetId());
        QVERIFY(e0.GetRow() == 0);
        QVERIFY(e1.GetRow() == 1);
        QVERIFY(e3.GetRow() == 0);
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e5.GetRow() == 2);
    }
}

void DatabasemodelTest::test_move_entries_down_same_parent()
{
    _cleanup_database();

    Entry e1, e2, e3, e4, e5;
    {
        // Populate the initial hierarchy for the test
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e1);
        dbm.AddEntry(e2);
        dbm.AddEntry(e3);
        dbm.AddEntry(e4);
        dbm.AddEntry(e5);

        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);


        // Now move some entries down with respect to its siblings
        dbm.MoveEntries(QModelIndex(), 1, 2,
                        QModelIndex(), 4);
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 2);
        QVERIFY(e3.GetRow() == 3);
        QVERIFY(e4.GetRow() == 1);
        QVERIFY(e5.GetRow() == 4);

        // Check that undoing works
        dbm.Undo();
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);

        dbm.Redo();
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 2);
        QVERIFY(e3.GetRow() == 3);
        QVERIFY(e4.GetRow() == 1);
        QVERIFY(e5.GetRow() == 4);
    }
}

void DatabasemodelTest::test_move_entries_up_same_parent()
{
    _cleanup_database();

    Entry e1, e2, e3, e4, e5;
    {
        // Populate the initial hierarchy for the test
        DatabaseModel dbm(DATABASE_PATH); dbm.Open(m_creds);
        dbm.AddEntry(e1);
        dbm.AddEntry(e2);
        dbm.AddEntry(e3);
        dbm.AddEntry(e4);
        dbm.AddEntry(e5);

        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);


        // Now move some entries up with respect to its siblings
        dbm.MoveEntries(QModelIndex(), 2, 3,
                        QModelIndex(), 1);
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 3);
        QVERIFY(e3.GetRow() == 1);
        QVERIFY(e4.GetRow() == 2);
        QVERIFY(e5.GetRow() == 4);

        // Check that undoing works
        dbm.Undo();
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 1);
        QVERIFY(e3.GetRow() == 2);
        QVERIFY(e4.GetRow() == 3);
        QVERIFY(e5.GetRow() == 4);

        dbm.Redo();
        e1 = dbm.FindEntryById(e1.GetId());
        e2 = dbm.FindEntryById(e2.GetId());
        e3 = dbm.FindEntryById(e3.GetId());
        e4 = dbm.FindEntryById(e4.GetId());
        e5 = dbm.FindEntryById(e5.GetId());
        QVERIFY(e1.GetParentId() == EntryId::Null());
        QVERIFY(e2.GetParentId() == EntryId::Null());
        QVERIFY(e3.GetParentId() == EntryId::Null());
        QVERIFY(e4.GetParentId() == EntryId::Null());
        QVERIFY(e5.GetParentId() == EntryId::Null());
        QVERIFY(e1.GetRow() == 0);
        QVERIFY(e2.GetRow() == 3);
        QVERIFY(e3.GetRow() == 1);
        QVERIFY(e4.GetRow() == 2);
        QVERIFY(e5.GetRow() == 4);
    }
}


QTEST_MAIN(DatabasemodelTest)

#include "tst_databasemodeltest.moc"
