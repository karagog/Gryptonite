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

#include "mainwindow.h"
#include <grypto_notifyupdatedialog.h>
#include <gutil/updatenotifier.h>
#include <gutil/application.h>
#include <gutil/qt_settings.h>
#include <gutil/globallogger.h>
#include <gutil/filelogger.h>
#include <QStandardPaths>
#include <QUrl>
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GRYPTO;

#define APPLICATION_LOG "grypto_transforms.log"

class Application : public GUtil::Qt::Application
{
    Q_OBJECT
    GUtil::Qt::UpdateNotifier updater;
    GUtil::Qt::Settings settings;
    MainWindow main_window;
public:
    Application(int &argc, char **argv)
        :GUtil::Qt::Application(argc, argv, GRYPTO_APP_NAME, GRYPTO_VERSION_STRING),
          updater(GUtil::Version(GRYPTO_VERSION_STRING)),
          settings("transforms"),
          main_window(&settings)
    {
        connect(&updater, SIGNAL(UpdateInfoReceived(QString,QUrl)),
                this, SLOT(_update_info_received(QString,QUrl)));

        main_window.show();
    }

    virtual void CheckForUpdates(bool){
        updater.CheckForUpdates(QUrl(GRYPTO_LATEST_VERSION_URL));
    }
private slots:
    void _update_info_received(const QString &latest_version_string, const QUrl &download_url){
        GUtil::Version latest_version(latest_version_string.toUtf8().constData());
        if(updater.GetCurrentVersion() < latest_version){
            NotifyUpdateDialog::Show(&main_window, latest_version_string, download_url);
        }
        else{
            QMessageBox::information(&main_window, tr("Up to date"), tr("Your software is up to date"));
        }
    }
};

#include "main.moc"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(grypto_ui);

    int ret = 1;
    try
    {
        Application a(argc, argv);

        // Set up a global file logger, so the application can log errors somewhere
        SetGlobalLogger(new FileLogger(
                            String::Format("%s/" APPLICATION_LOG,
                                           QStandardPaths::writableLocation(QStandardPaths::DataLocation).toUtf8().constData())));

        a.setQuitOnLastWindowClosed(false);

        // Don't allow exceptions to crash us. You can read the log to find exception details.
        a.SetTrapExceptions(true);

        // Execute the application event loop
        ret = a.exec();
    }
    catch(const std::exception &ex)
    {
        GlobalLogger().LogException(ex);
        ret = 2;
    }

    // Clean up and return
    SetGlobalLogger(NULL);
    return ret;
}
