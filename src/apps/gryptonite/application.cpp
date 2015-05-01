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

#include "application.h"
#include "mainwindow.h"
#include "about.h"
#include "settings.h"
#include <grypto/notifyupdatedialog.h>
#include <gutil/globallogger.h>
#include <gutil/grouplogger.h>
#include <gutil/filelogger.h>
#include <gutil/messageboxlogger.h>
#include <gutil/cryptopp_rng.h>
#include <gutil/commandlineargs.h>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL;
using namespace std;

#define APPLICATION_LOG     GRYPTO_APP_NAME ".log"

// The global RNG should be a good one (from Crypto++)
static GUtil::CryptoPP::RNG __cryptopp_rng;
static GUtil::RNG_Initializer __rng_init(&__cryptopp_rng);

static void __init_default_settings(GUtil::Qt::Settings &settings)
{
    bool anything_changed = false;
    auto init_setting = [&](const char *key, const QVariant &value){
        if(!settings.Contains(key)){
            settings.SetValue(key, value);
            anything_changed = true;
        }
    };

    init_setting(GRYPTONITE_SETTING_AUTOLAUNCH_URLS, true);
    init_setting(GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE, true);
    init_setting(GRYPTONITE_SETTING_RECENT_FILES_LENGTH, 10);
    init_setting(GRYPTONITE_SETTING_CLOSE_MINIMIZES_TO_TRAY, true);
    init_setting(GRYPTONITE_SETTING_TIME_FORMAT_24HR, true);
    init_setting(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT, 15);
    init_setting(GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT, 30);
    init_setting(GRYPTONITE_SETTING_CHECK_FOR_UPDATES, true);

    if(anything_changed)
        settings.CommitChanges();
}

Application::Application(int &argc, char **argv)
    :GUtil::Qt::Application(argc, argv, GRYPTO_APP_NAME, GRYPTO_VERSION_STRING),
      settings("main"),
      updater(GUtil::Version(GRYPTO_VERSION_STRING), this),
      main_window(NULL)
{
    GUtil::Qt::MessageBoxLogger *mbl = new GUtil::Qt::MessageBoxLogger;

    // Log global messages to a group logger, which writes to all loggers in the group
    SetGlobalLogger(new GroupLogger{
                        mbl,
                        new FileLogger(QString("%1/" APPLICATION_LOG)
                            .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).toUtf8()),
                    });

    CommandLineArgs args(argc, argv);
    QString open_file;
    if(args.Length() > 1){
        open_file = QString::fromUtf8(args[1]);
    }

    // Don't let exceptions crash us, they will be logged to the global logger
    SetTrapExceptions(true);

    // Initialize resources in case we have a static build
    Q_INIT_RESOURCE(grypto_ui);

    // Register the metatypes we are going to use
    qRegisterMetaType<shared_ptr<exception>>("std::shared_ptr<exception>");
    qRegisterMetaType<Grypt::EntryId>("Grypt::EntryId");
    qRegisterMetaTypeStreamOperators<Grypt::IdType>("Grypt::IdType");

    try{
        __init_default_settings(settings);

        main_window = new MainWindow(&settings, open_file);

        // This only gets set if no exception is hit. Otherwise we DO want the application
        //  to close after it has shown the error message
        setQuitOnLastWindowClosed(false);
    }
    catch(exception &ex){
        handle_exception(ex);
    }

    mbl->SetParentWidget(main_window);
    connect(&updater, SIGNAL(UpdateInfoReceived(QString,QUrl)),
            this, SLOT(_update_info_received(QString,QUrl)));

    if(settings.Value(GRYPTONITE_SETTING_CHECK_FOR_UPDATES).toBool())
    {
        // This works asynchronously; no hanging here!
        CheckForUpdates(true);
    }
}

void Application::about_to_quit()
{
    if(main_window){
        main_window->AboutToQuit();
        main_window->deleteLater();
    }
    SetGlobalLogger(NULL);
}

void Application::handle_exception(std::exception &ex)
{
    if(0 != dynamic_cast<GUtil::CancelledOperationException<> *>(&ex)){
        QMessageBox::information(main_window, "Cancelled", "The operation has been cancelled");
    }
    else{
        if(0 != dynamic_cast<GUtil::ReadOnlyException<> *>(&ex)){
            // Put the application into readonly mode
            main_window->DropToReadOnly();
        }
        GUtil::Qt::Application::handle_exception(ex);
    }
}

void Application::show_about(QWidget *)
{
    (new ::About(main_window))->ShowAbout();
}

void Application::CheckForUpdates(bool silent)
{
    m_silent = silent;
    updater.CheckForUpdates(QUrl(GRYPTO_LATEST_VERSION_URL));
}

void Application::_update_info_received(const QString &latest_version_string,
                                        const QUrl &download_url)
{
    Version latest_version(latest_version_string.toUtf8().constData());
    if(updater.GetCurrentVersion() < latest_version){
        Grypt::NotifyUpdateDialog::Show(main_window, latest_version_string, download_url);
    }
    else if(!m_silent){
        QMessageBox::information(main_window,
                                 tr("Up to date"),
                                 tr("Your software is up to date"));
    }
}

void Application::_disable_auto_update()
{
    settings.SetValue(GRYPTONITE_SETTING_CHECK_FOR_UPDATES, false);
    settings.CommitChanges();
}
