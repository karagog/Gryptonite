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

#include "legacyplugin.h"
#include "../../legacy/legacyutils.h"
#include <grypto/entry.h>
#include <grypto/newpassworddialog.h>
#include <grypto/getpassworddialog.h>
#include <QMessageBox>
#include <QFileDialog>
using namespace std;
USING_NAMESPACE_GRYPTO1(Legacy);

NAMESPACE_GRYPTO;


LegacyPlugin::LegacyPlugin(QObject *parent)
    :QObject(parent)
{
}

QString LegacyPlugin::UpgradeDatabase(const QString &path,
                                      Grypt::Credentials &new_creds,
                                      GUtil::Qt::Settings *settings,
                                      function<void(int, const QString &)> progress_cb,
                                      QWidget *parent)
{
    QString ret;
    LegacyUtils::FileVersionEnum version =
            LegacyUtils::GetFileVersion(path.toUtf8().constData());
    if(LegacyUtils::CurrentVersion != version)
    {
        // Ask the user if they want to upgrade
        if(QMessageBox::No == QMessageBox::warning(parent, QObject::tr("Old file detected"),
                             QString("The file you selected is either unrecognized, or in an older format."
                                     " Would you like to attempt to update it?  (The original file will be preserved)"),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No)){
            return QString::null;
        }

        // Get the path of the new file
        ret = QFileDialog::getSaveFileName(parent,
                                           QObject::tr("Select updated file path"),
                                           QString(),
                                           "Grypto DB (*.gdb);;All Files (*)");
        if(ret.isEmpty())
            return QString::null;
        else if(QFileInfo(ret).suffix().isEmpty())
            ret.append(".gdb");

        Credentials creds;
        {
            GetPasswordDialog dlg(settings, QFileInfo(path).fileName(), parent);
            if(QDialog::Rejected == dlg.exec())
                return QString::null;
            creds = dlg.GetCredentials();
        }
        {
            NewPasswordDialog dlg(settings, QFileInfo(ret).fileName(), parent);
            if(QDialog::Rejected == dlg.exec())
                return QString::null;
            new_creds = dlg.GetCredentials();
        }

        progress_cb(0, QString::null);

        LegacyUtils::UpdateFileToCurrentVersion(path.toUtf8(), version,
                                                ret.toUtf8(),
                                                creds, new_creds,
                                                progress_cb
        );

        creds = new_creds;
    }
    return ret;
}


END_NAMESPACE_GRYPTO;
