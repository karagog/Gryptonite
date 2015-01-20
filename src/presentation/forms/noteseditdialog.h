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

#ifndef NOTESEDITDIALOG_H
#define NOTESEDITDIALOG_H

#include <QDialog>

namespace Ui {
class NotesEditDialog;
}

namespace Grypt
{


class NotesEditDialog :
        public QDialog
{
    Q_OBJECT
    QString notes;
public:
    explicit NotesEditDialog(const QString &starting_text, QWidget *parent = 0);
    ~NotesEditDialog();

    QString const &Notes() const{ return notes; }


public slots:

    virtual void accept();


private:
    Ui::NotesEditDialog *ui;
};


}

#endif // NOTESEDITDIALOG_H
