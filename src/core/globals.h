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
#include <QMetaType>


#define GRYPTO_APP_NAME "Gryptonite"

#define GRYPTO_VERSION_STRING "1.0"


// Default attribute labels
// Basic labels
#define S_LABEL         "Label"
#define S_DESCRIPTION   "Description"
#define S_VALUE         "Value"
#define S_PASSWORD      "Password"
#define S_NOTES         "Notes"
#define S_BINARY        "Bin"
#define S_BROKEN_ID     "Broke"

// For a website
#define S_URL           "URL"
#define S_USERNAME      "Username"

// For a credit card
#define S_CCNUMBER      "Card Number"
#define S_EXP_DATE      "Expiration"
#define S_SECURITYCODE  "Security Code"

// These are the date formats used throughout
#define TFH_DATE_FORMAT "M/dd/yy h:mm"
#define AP_DATE_FORMAT "M/dd/yy h:mm AP"



typedef unsigned char byte;




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
