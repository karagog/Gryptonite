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

#include "about.h"
#include <grypto_common.h>


About::About(QWidget *p)
    :GUtil::Qt::About(p, true, true)
{
    _dialog.resize(600, 325);

    SetWindowTitle(QString(tr("About %1")).arg(GRYPTO_APP_NAME));
    _header.setText(QString(tr("%1 - v%2")).arg(GRYPTO_APP_NAME).arg(GRYPTO_VERSION_STRING));
    _text.setText(tr(
                      "Gryptonite is an application that stores your most secret and"
                      " personal information in a securely encrypted database. The database"
                      " is unlocked by a single master password (and/or keyfile)."

                      "\n\nIt was developed by George Karagoulis using"
                      " the Qt framework and Crypto++ v5.6.2 encryption libraries. It was created"
                      " as a personal hobby and labor of love, and therefore comes with no"
                      " warrantee."

                      "\n\nIt is open-source software built on top of open-source software, and"
                      " is intended to be free and open forever. That said...you could donate to show"
                      " your appreciation for this developer's hard work ;)"
                      ));
}
