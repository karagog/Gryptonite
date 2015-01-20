/*Copyright 2014 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "grypto_cryptor.h"
#include <QString>
#include <QtTest>
USING_NAMESPACE_GRYPTO;
using namespace std;


class CryptorTest : public QObject
{
    Q_OBJECT
    Cryptor crypt;

public:
    CryptorTest();

private Q_SLOTS:
    void test_encryption();
};

CryptorTest::CryptorTest()
{
    crypt.SetPassword("password");
}

// Returns true if equal zero
bool __compare_iv_with_zero(byte *iv)
{
    bool ret = true;
    byte *cur_l = iv;
    while(ret && cur_l < (iv + Cryptor::BlockSize))
        ret = *(cur_l++) == 0;
    return ret;
}

void CryptorTest::test_encryption()
{
    byte iv[Cryptor::BlockSize];

    // Initialize to 0
    memset(iv, 0, Cryptor::BlockSize);
    QVERIFY(__compare_iv_with_zero(iv));

    // Test encrypting a small string
    string r;
    string s = "Hello world!!!";
    string c = crypt.EncryptData(s, iv);

    QVERIFY(s != c);
    QVERIFY(!__compare_iv_with_zero(iv));

    r = crypt.DecryptData(c, iv);
    QVERIFY(s == r);

    // Test encrypting a long string
    s = "This is a really long string, to see if it makes a difference between a long string and a short string.";
    memset(iv, 0, Cryptor::BlockSize);
    c = crypt.EncryptData(s, iv);

    QVERIFY(s != c);
    QVERIFY(!__compare_iv_with_zero(iv));

    r = crypt.DecryptData(c, iv);
    QVERIFY(s == r);

    // Try decrypting with the wrong iv (change one byte of iv)
    iv[3] = iv[3] + 1;
    r = crypt.DecryptData(c, iv);
    QVERIFY(s != r);
}

QTEST_APPLESS_MAIN(CryptorTest)

#include "tst_cryptortest.moc"
