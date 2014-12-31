/*Copyright 2010 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include <string>
#include <QString>
#include "encryption.h"
#include <cryptopp/default.h>
#include <cryptopp/hex.h>
#include <cryptopp/cryptlib.h>
using namespace std;

namespace Grypt{ namespace Legacy{


namespace V1{

QString Encryption::EncryptString(const char *instr, const char *passPhrase)
{
    std::string outstr;

    CryptoPP::DefaultEncryptorWithMAC encryptor(passPhrase, new CryptoPP::HexEncoder(new CryptoPP::StringSink(outstr)));
    encryptor.Put((byte *)instr, strlen(instr));
    encryptor.MessageEnd();

    return QString::fromStdString(outstr);
}

QString Encryption::DecryptString(const char *instr, const char *passPhrase)
{
    std::string outstr;

    CryptoPP::HexDecoder decryptor(new CryptoPP::DefaultDecryptorWithMAC(passPhrase, new CryptoPP::StringSink(outstr)));
    decryptor.Put((byte *)instr, strlen(instr));
    decryptor.MessageEnd();

    return QString::fromStdString(outstr);
}

}


namespace V2{

string Encryption::EncryptString(const string &instr, const string &passPhrase)
{
    string outstr;

    CryptoPP::DefaultEncryptorWithMAC encryptor(passPhrase.c_str(),
                                                new CryptoPP::StringSink(outstr));

    try
    {
        encryptor.Put((byte *)instr.c_str(), instr.length());
        encryptor.MessageEnd();
    }
    catch(CryptoPP::Exception ex)
    {
        return "";
    }

    return outstr;
}

string Encryption::DecryptString(const string &instr, const string &passPhrase)
{
    string outstr;

    CryptoPP::DefaultDecryptorWithMAC decryptor(passPhrase.c_str(),
                                                new CryptoPP::StringSink(outstr));

    try
    {
        decryptor.Put((byte *)instr.c_str(), instr.length());
        decryptor.MessageEnd();
    }
    catch(CryptoPP::Exception ex)
    {
        return "";
    }

    return outstr;
}

}


namespace V3{

string Encryption::EncryptString(const string &instr, const string &passPhrase)
{
    return V2::Encryption::EncryptString(instr, passPhrase);
}

string Encryption::DecryptString(const string &instr, const string &passPhrase)
{
    return V2::Encryption::DecryptString(instr, passPhrase);
}

}


}}
