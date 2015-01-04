/*Copyright 2014-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include <grypto_passworddatabase.h>
#include <grypto_xmlconverter.h>
#include <grypto_entry.h>
#include <gutil/cryptopp_rng.h>
#include <gutil/databaseutils.h>
#include <QString>
#include <QtTest>
using namespace std;
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GRYPTO;

#define TEST_FILEPATH  "testdb.sqlite"
#define TEST_PASSWORD "password...shhh"

static GUtil::CryptoPP::RNG __cryptopp_rng;
static GUtil::RNG_Initializer __rng_init(&__cryptopp_rng);

Grypt::Credentials creds;

class DatabaseTest : public QObject
{
    Q_OBJECT
    PasswordDatabase *db;

public:
    DatabaseTest();

private Q_SLOTS:
    void initTestCase();
    void test_create();
    void test_entry();
    void test_entry_insert();
    void test_entry_delete();
    void test_entry_update();
    void cleanupTestCase();
};

DatabaseTest::DatabaseTest()
    :db(0)
{
    creds.Password = TEST_PASSWORD;
}

void DatabaseTest::initTestCase()
{
    if(QFile::exists(TEST_FILEPATH))
        QVERIFY(QFile::remove(TEST_FILEPATH));

    bool no_exception = true;
    try
    {
        db = new PasswordDatabase(TEST_FILEPATH, creds);
    }
    catch(...)
    {
        no_exception = false;
    }
    QVERIFY(no_exception);
}

void DatabaseTest::test_create()
{
    // Try opening the database with the wrong key
    bool exception_hit = false;
    Credentials bad_creds;
    bad_creds.Password = "wrong password";
    try
    {
        PasswordDatabase newdb(TEST_FILEPATH, bad_creds);
    }
    catch(const AuthenticationException<> &ex)
    {
        exception_hit = true;
    }
    QVERIFY(exception_hit);

    // Try opening the database with the right key (No exception)
    PasswordDatabase newdb(TEST_FILEPATH, creds);
}

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

void DatabaseTest::test_entry()
{
    Entry e, e2;
    SecretValue v;

    e.SetName("first entry");
    e.SetDescription("first description");
    e.SetModifyDate(QDateTime::currentDateTime());

    v.SetName("one");
    v.SetValue("Hello World!");
    v.SetNotes("note to self");
    e.Values().append(v);

    QByteArray entry_xml = XmlConverter::ToXmlString(e);
    e2 = XmlConverter::FromXmlString<Entry>(entry_xml);
    QVERIFY(__compare_entries(e, e2));

    db->AddEntry(e);

    // After insertion, the id should be updated to the new value
    QVERIFY(e.GetId() != e2.GetId());

    // Try reading it back in, it should be the same as the original
    Entry e3 = db->FindEntry(e.GetId());
    QVERIFY(__compare_entries(e, e3));
}

void DatabaseTest::test_entry_insert()
{
    Entry e;
    e.SetName("new entry");
    e.SetDescription("testing insertion");
    e.SetModifyDate(QDateTime::currentDateTime());
    e.SetRow(1);
    db->AddEntry(e);

    e.SetRow(0);
    db->AddEntry(e);

    vector<Entry> el = db->FindEntriesByParentId(EntryId::Null());
    QVERIFY(el.size() == 3);
    QVERIFY(el[0].GetName() == "new entry");
    QVERIFY(el[1].GetName() == "first entry");
    QVERIFY(el[2].GetName() == "new entry");
    QVERIFY(el[0].GetRow() == 0);
    QVERIFY(el[1].GetRow() == 1);
    QVERIFY(el[2].GetRow() == 2);
    QVERIFY(__compare_entries(el[0], e));
}

void DatabaseTest::test_entry_delete()
{
    Entry e;
    e.SetRow(0);
    db->AddEntry(e);
    db->DeleteEntry(e.GetId());

    vector<Entry> el = db->FindEntriesByParentId(EntryId::Null());
    QVERIFY(el.size() == 3);
    QVERIFY(el[0].GetName() == "new entry");
    QVERIFY(el[1].GetName() == "first entry");
    QVERIFY(el[2].GetName() == "new entry");
    QVERIFY(el[0].GetRow() == 0);
    QVERIFY(el[1].GetRow() == 1);
    QVERIFY(el[2].GetRow() == 2);
}

void DatabaseTest::test_entry_update()
{
    Entry e;
    db->AddEntry(e);

    e.SetName("updated entry");
    e.SetDescription("totally new description");
    e.SetFavoriteIndex(0);
    db->UpdateEntry(e);

    Entry e2 = db->FindEntry(e.GetId());
    QVERIFY(__compare_entries(e, e2));
}

void DatabaseTest::cleanupTestCase()
{
    delete db;

    // Make sure we can remove the file after destroying the class
    QVERIFY(QFile::remove(TEST_FILEPATH));
}


QTEST_APPLESS_MAIN(DatabaseTest)

#include "tst_databasetest.moc"
