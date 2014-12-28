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

#ifndef GETPASSWORDDIALOG_H
#define GETPASSWORDDIALOG_H

#include "gutil_qt_settings.h"
#include <QDialog>

namespace Ui {
class GetPasswordDialog;
}

namespace Grypt
{


class GetPasswordDialog :
        public QDialog
{
    Q_OBJECT
    QByteArray password;
    QByteArray keyfile;
    Ui::GetPasswordDialog *ui;
    GUtil::Qt::Settings *m_settings;
public:
    explicit GetPasswordDialog(
            GUtil::Qt::Settings *,
            const QString &filename = QString(),
            QWidget *parent = 0);
    ~GetPasswordDialog();

    QByteArray const &Password() const{ return password; }
    QByteArray const &KeyFile() const{ return keyfile; }

    virtual void accept();


private slots:

    void _select_keyfile();
    void _combobox_indexchanged(int);

};


}

#endif // GETPASSWORDDIALOG_H
