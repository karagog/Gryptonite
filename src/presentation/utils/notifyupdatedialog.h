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

#ifndef NOTIFYUPDATEDIALOG_H
#define NOTIFYUPDATEDIALOG_H

#include <grypto/common.h>
#include <QMessageBox>
#include <QUrl>

NAMESPACE_GRYPTO;


class NotifyUpdateDialog
{
public:
    static void Show(QWidget *parent,
                     const QString &latest_version_string,
                     const QUrl &download_url)
    {
        QMessageBox mb(parent);
        mb.setIcon(QMessageBox::Information);
        mb.addButton(QMessageBox::Ok);
        mb.setTextFormat(::Qt::RichText);
        mb.setWindowTitle(QObject::tr("Update Available!"));
        mb.setText(QString(QObject::tr("There is a new version of %1 available: %2"
                                       "<br/>"
                                       "<a href='%3'>Go get it!</a>"))
                   .arg(GRYPTO_APP_NAME)
                   .arg(latest_version_string)
                   .arg(download_url.toString()));
        mb.exec();
    }
};


END_NAMESPACE_GRYPTO;

#endif // NOTIFYUPDATEDIALOG_H

