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
#include "encryption.h"
#include <cryptopp/default.h>
#include <cryptopp/hex.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/gzip.h>
using namespace std;

namespace Grypt{ namespace Legacy{


namespace V1{

string Encryption::DecryptString(const string &instr, const char *passPhrase)
{
    string outstr;

    CryptoPP::HexDecoder decryptor(new CryptoPP::DefaultDecryptorWithMAC(passPhrase, new CryptoPP::StringSink(outstr)));
    decryptor.Put((byte *)instr.data(), instr.length());
    decryptor.MessageEnd();

    return outstr;
}

}


namespace V2{

string Encryption::DecryptString(const string &instr, const string &passPhrase)
{
    string outstr;

    CryptoPP::DefaultDecryptorWithMAC decryptor(passPhrase.c_str(),
                                                new CryptoPP::StringSink(outstr));
    decryptor.Put((byte *)instr.c_str(), instr.length());
    decryptor.MessageEnd();

    return outstr;
}

}


namespace V3{

string Encryption::DecryptString(const string &instr, const string &passPhrase)
{
    string data = V2::Encryption::DecryptString(instr, passPhrase);
    string decompressed;
    if(data[0] == '1')
    {
        data = data.substr(1);
        CryptoPP::StringSource(data, true, new CryptoPP::Gunzip(new CryptoPP::StringSink(decompressed)));
    }
    else{
        decompressed = data;
    }
    return decompressed;
}

}


}}
