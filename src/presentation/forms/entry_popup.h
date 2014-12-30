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

#ifndef GRYPTO_ENTRY_VIEW
#define GRYPTO_ENTRY_VIEW

#include "grypto_entry.h"
#include <QSortFilterProxyModel>
#include <QAction>

class QCloseEvent;
class QUuid;

namespace Ui{
class EntryPopup;
}

namespace Grypt
{


class EntryPopup :
        public QWidget
{
    Q_OBJECT
public:

    EntryPopup(const Entry &, QWidget *parent = 0);
    ~EntryPopup();


private slots:

    void copy_to_clipboard();
    void _show_in_main_window();


private:

    Ui::EntryPopup *ui;
    Entry m_entry;

};


}

#endif // GRYPTO_ENTRY_VIEW
