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
    void test_update_entry();
    void test_delete_entry();
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
    m_creds.Password = "password";
    _cleanup_database();

    // This creates and initializes an empty database
    DatabaseModel dbm(DATABASE_PATH, m_creds);
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
        DatabaseModel dbm(DATABASE_PATH, m_creds);
        dbm.AddEntry(e);

        // Check that the new data is immediately accessible, because
        //  it may be added on a background thread.
        Entry e2 = dbm.FindEntryById(e.GetId());
        QVERIFY(__compare_entries(e, e2));
    }

    // Check that the data persists after the original object dies
    {
        DatabaseModel dbm(DATABASE_PATH, m_creds);
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
        DatabaseModel dbm(DATABASE_PATH, m_creds);
        dbm.AddEntry(e2);

        Entry e3 = dbm.FindEntryById(e2.GetId());
        Entry e4 = dbm.FindEntryById(e.GetId());
        QVERIFY(e3.GetRow() == 0);
        QVERIFY(e4.GetRow() == 1);
    }

    // Make sure the changes persist
    {
        DatabaseModel dbm(DATABASE_PATH, m_creds);
        Entry e3 = dbm.FindEntryById(e.GetId());
        QVERIFY(e3.GetRow() == 1);
    }


    // Add an entry, then undo it
    Entry e3(e);
    e3.SetRow(1);
    {
        DatabaseModel dbm(DATABASE_PATH, m_creds);
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
        DatabaseModel dbm(DATABASE_PATH, m_creds);
        Entry e4 = dbm.FindEntryById(e2.GetId());
        Entry e5 = dbm.FindEntryById(e.GetId());
        QVERIFY(e4.GetRow() == 0);
        QVERIFY(e5.GetRow() == 1);
        QVERIFY(!dbm.FindIndexById(e3.GetId()).isValid());
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
        DatabaseModel dbm(DATABASE_PATH, m_creds);
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
        DatabaseModel dbm(DATABASE_PATH, m_creds);
        Entry e2 = dbm.FindEntryById(e.GetId());
        QVERIFY(e2.GetDescription() == "new description");
    }


    // Test that undoing works
    {
        DatabaseModel dbm(DATABASE_PATH, m_creds);
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

void DatabasemodelTest::test_delete_entry()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_move_entries_basic()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_move_entries_down_same_parent()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_move_entries_up_same_parent()
{
    QVERIFY2(false, "Failure");
}


QTEST_MAIN(DatabasemodelTest)

#include "tst_databasemodeltest.moc"
