/*Copyright 2010-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef GRYPT_LEGACY_ENCRYPTION_H
#define	GRYPT_LEGACY_ENCRYPTION_H
#include <QString>
#include <string>

/** \file
 *  This file keeps track of the old encryption methods so we can upgrade old
 *  file formats.
*/

namespace Grypt{ namespace Legacy{


namespace V1{

class Encryption
{
public:
    static QString EncryptString(const char *instr, const char *passPhrase);
    static QString DecryptString(const char *instr, const char *passPhrase);
};

}


namespace V2{

class Encryption
{
public:
    static std::string EncryptString(const std::string &instr, const std::string &passPhrase);
    static std::string DecryptString(const std::string &instr, const std::string &passPhrase);
};

}


namespace V3{

class Encryption
{
public:
    static std::string EncryptString(const std::string &instr, const std::string &passPhrase);
    static std::string DecryptString(const std::string &instr, const std::string &passPhrase);
};

}


}}

#endif	/* GRYPT_LEGACY_ENCRYPTION_H */

