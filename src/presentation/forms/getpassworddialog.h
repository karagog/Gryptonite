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

#include <gutil/qt_settings.h>
#include <grypto_common.h>
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
    Ui::GetPasswordDialog *ui;
    GUtil::Qt::Settings *m_settings;
    Credentials m_creds;
public:
    explicit GetPasswordDialog(
            GUtil::Qt::Settings *,
            const QString &filename = QString(),
            QWidget *parent = 0);
    ~GetPasswordDialog();

    Credentials const &GetCredentials() const{ return m_creds; }

    virtual void accept();


private slots:

    void _select_keyfile();
    void _combobox_indexchanged(int);

};


}

#endif // GETPASSWORDDIALOG_H
