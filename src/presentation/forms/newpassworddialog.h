/*Copyright 2014-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef GRYPTO_NEWPASSWORD_H
#define GRYPTO_NEWPASSWORD_H

#include <gutil/qt_settings.h>
#include <grypto/common.h>
#include <QDialog>

namespace Ui{
    class NewPassword;
}

namespace Grypt{


class NewPasswordDialog :
        public QDialog
{
    Q_OBJECT
    Ui::NewPassword *ui;
    GUtil::Qt::Settings *m_settings;
    Credentials m_creds;
public:

    NewPasswordDialog(GUtil::Qt::Settings *, const QString &title, QWidget *par = 0);
    ~NewPasswordDialog();

    Credentials const &GetCredentials() const{ return m_creds; }

    QString GetKeyfileLocation() const;

    virtual void accept();


protected:

    virtual bool eventFilter(QObject *, QEvent *);


private slots:

    void _select_keyfile();
    void _generate_keyfile();
    void _combobox_indexchanged(int);

};


}


#endif // GRYPTO_NEWPASSWORD_H
