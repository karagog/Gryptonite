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

#ifndef GRYPTO_GLOBALS_H
#define GRYPTO_GLOBALS_H

#include <gutil/id.h>
#include <gutil/exception.h>
#include <gutil/cryptopp_cryptor.h>
#include <QMetaType>


#define GRYPTO_APP_NAME "Gryptonite"

#define GRYPTO_VERSION_STRING "3.0"


typedef unsigned char byte;

namespace Grypt{
    typedef ::GUtil::CryptoPP::Cryptor::Credentials Credentials;
}




#define NAMESPACE_GRYPTO namespace Grypt {
#define NAMESPACE_GRYPTO1( ns ) namespace Grypt { namespace ns {

#define END_NAMESPACE_GRYPTO }
#define END_NAMESPACE_GRYPTO1 }}

#define USING_NAMESPACE_GRYPTO using namespace Grypt
#define USING_NAMESPACE_GRYPTO1( ns ) using namespace Grypt :: ns


NAMESPACE_GRYPTO;
typedef GUtil::Id<16> IdType;
typedef IdType EntryId;
typedef IdType FileId;
END_NAMESPACE_GRYPTO;

// So we can store it in a QVariant
Q_DECLARE_METATYPE(Grypt::IdType)

GUTIL_DEFINE_ID_QHASH( Grypt::IdType::Size );


#endif // GRYPTO_MACROS_H
