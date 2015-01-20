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

#ifndef _ENTRY_EDITOR_H
#define	_ENTRY_EDITOR_H

#include "grypto_entry.h"
#include <QDialog>

namespace Ui{
class EntryEdit;
}

namespace Grypt{
class DatabaseModel;


class EntryEdit :
        public QDialog
{
    Q_OBJECT
public:

    explicit EntryEdit(DatabaseModel *, QWidget *parent = 0);
    explicit EntryEdit(const Entry &, DatabaseModel *, QWidget *parent = 0);
    ~EntryEdit();

    Entry &GetEntry() { return m_entry; }

public slots:

    void accept();


private slots:

    void _add_secret();
    void _del_secret();
    void _toggle_secret();
    void _generate_value();
    void _edit_notes();
    void _move_up();
    void _move_down();
    void _select_file();


private:

    Ui::EntryEdit *ui;
    DatabaseModel *m_dbModel;
    Entry m_entry;

    void _init(DatabaseModel *);

};


}

#endif	/* _ENTRY_EDITOR_H */
