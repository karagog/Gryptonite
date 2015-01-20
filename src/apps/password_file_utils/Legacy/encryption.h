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

#ifndef _GCRYPT_H
#define	_GCRYPT_H

#include <QString>
#include <string>


class LegacyRoutines
{
public:

    // These are the old encryption functions
    static QString EncryptString_v1(const char *instr, const char *passPhrase);
    static QString DecryptString_v1(const char *instr, const char *passPhrase);

    static std::string EncryptString_v2(const std::string &instr, const std::string &passPhrase);
    static std::string DecryptString_v2(const std::string &instr, const std::string &passPhrase);

};


#endif	/* _GCRYPT_H */

