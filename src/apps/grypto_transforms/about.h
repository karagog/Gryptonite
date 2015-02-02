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

#include <grypto_aboutbase.h>
#include <grypto_common.h>

#define GRYPTO_TRANSFORMS_APP_NAME "Grypto-Transforms"

class About : public Grypt::AboutBase
{
    Q_OBJECT
public:
    About(QWidget *p = 0)
        :Grypt::AboutBase(GRYPTO_TRANSFORMS_APP_NAME, GRYPTO_VERSION_STRING, p)
    {
        _buildinfo.setText(QString("Built on %1").arg(__DATE__ " - " __TIME__));

        QString txt = _text.toPlainText();
        txt.prepend(GRYPTO_TRANSFORMS_APP_NAME
                    " is a type of data transformation calculator. You can use it"
                    " to compute hashes, encrypt/decrypt data and convert between"
                    " UTF-8, base-64 and hex representations. The encryption is"
                    " very strong and provides both confidentiality and authenticity."
                    " It is implemented using AES with CCM.\n\n");
        _text.setText(txt);
    }
};
