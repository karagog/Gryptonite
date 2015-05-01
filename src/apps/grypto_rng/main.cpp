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
#include "about.h"
#include "server.h"
#include <gutil/application.h>
#include <gutil/updatenotifier.h>
#include <gutil/qt_settings.h>
#include <gutil/globallogger.h>
#include <gutil/filelogger.h>
#include <gutil/messageboxlogger.h>
#include <gutil/grouplogger.h>
#include <gutil/smartpointer.h>
#include <gutil/commandlineargs.h>
#include <gutil/consoleiodevice.h>
#include <gutil/consolelogger.h>
#include <grypto/common.h>
#include <grypto/notifyupdatedialog.h>
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL;

#define APPLICATION_LOG "grypto_rng.log"

class Application : public GUtil::Qt::Application
{
    Q_OBJECT
    Server server;
    GUtil::Qt::UpdateNotifier updater;
    GUtil::SmartPointer<GUtil::Qt::Settings> settings;
    GUtil::SmartPointer<MainWindow> main_window;
public:
    Application(int &argc, char **argv)
        :GUtil::Qt::Application(argc, argv, GRYPTO_APP_NAME, GRYPTO_VERSION_STRING),
          updater(GUtil::Version(GRYPTO_VERSION_STRING))
    {
        SetTrapExceptions(true);
        qRegisterMetaType<std::shared_ptr<std::exception>>("std::shared_ptr<std::exception>");

        connect(&updater, SIGNAL(UpdateInfoReceived(QString,QUrl)),
                this, SLOT(_update_info_received(QString,QUrl)));

        SmartPointer<GroupLogger> gl( new GroupLogger );
        CommandLineArgs args(argc, argv);
        
        // Log global messages to a group logger, which writes to all loggers in the group
        SetGlobalLogger(gl.Data());
        gl->AddLogger(new GUtil::FileLogger(QString("%1/" APPLICATION_LOG)
                                            .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).toUtf8()));
        
        if(NULL == args.FindArgument("-headless", false)){
            gl->AddLogger(new GUtil::Qt::MessageBoxLogger(main_window.Data()));
            settings = new GUtil::Qt::Settings("rng");
            main_window = new MainWindow(settings.Data());
            main_window->show();
        }
        else{
            gl->AddLogger(new ConsoleLogger(Console::StandardError));
        }
        gl.Relinquish();
    }

    virtual void CheckForUpdates(bool){
        updater.CheckForUpdates(QUrl(GRYPTO_LATEST_VERSION_URL));
    }

protected:
    virtual void show_about(QWidget *parent){
        (new ::About(parent == 0 ? main_window.Data() : parent))->ShowAbout();
    }
    virtual void about_to_quit(){
        if(main_window)
            main_window->AboutToQuit();
        
        GUtil::Qt::Application::about_to_quit();

        // The messagebox logger is a child of the main window, so we have to clean up the loggers first
        //  to avoid a double-deletion
        SetGlobalLogger(NULL);
        main_window.Clear();
    }

private slots:
    void _update_info_received(const QString &latest_version_string, const QUrl &download_url){
        GASSERT(main_window);
        GUtil::Version latest_version(latest_version_string.toUtf8().constData());
        if(updater.GetCurrentVersion() < latest_version){
            Grypt::NotifyUpdateDialog::Show(main_window.Data(), latest_version_string, download_url);
        }
        else{
            QMessageBox::information(main_window.Data(), tr("Up to date"), tr("Your software is up to date"));
        }
    }
};

#include "main.moc"


int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(grypto_ui);
    return Application(argc, argv).exec();
}
