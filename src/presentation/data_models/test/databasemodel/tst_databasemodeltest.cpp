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

#include <grypto_databasemodel.h>
#include <QString>
#include <QtTest>
#include <QCoreApplication>

class DatabasemodelTest : public QObject
{
    Q_OBJECT

public:
    DatabasemodelTest();

private Q_SLOTS:
    void test_new_entry();
    void test_update_entry();
    void test_delete_entry();
    void test_move_entries_basic();
    void test_move_entries_down_same_parent();
    void test_move_entries_up_same_parent();
};

DatabasemodelTest::DatabasemodelTest()
{
}

void DatabasemodelTest::test_new_entry()
{
    QVERIFY2(false, "Failure");
}

void DatabasemodelTest::test_update_entry()
{
    QVERIFY2(false, "Failure");
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
