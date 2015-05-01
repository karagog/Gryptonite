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

#include <grypto/aboutbase.h>
#include <grypto/common.h>

#define GRYPTO_RNG_APP_NAME "Grypto-RNG"

class About : public Grypt::AboutBase
{
    Q_OBJECT
public:
    About(QWidget *p = 0)
        :Grypt::AboutBase(GRYPTO_RNG_APP_NAME, GRYPTO_VERSION_STRING, p)
    {
        _buildinfo.setText(QString("Built on %1").arg(__DATE__ " - " __TIME__));

        QString txt = _text.toPlainText();
        txt.prepend(GRYPTO_RNG_APP_NAME
                    " is a high quality cryptographic pseudo-random number generator."
                    " It can do simple functions like flipping a coin or rolling dice,"
                    " or more complex functions like generating statistical distributions."
                    " While not a \"true\" random number generator, it performs indistinguishibly"
                    " well on objective randomness tests, so for all intents and purposes"
                    " it should be just as good.\n\n");
        _text.setText(txt);
    }
};
