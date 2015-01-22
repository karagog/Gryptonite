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
#include <grypto_common.h>
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
    if(!settings.Contains(GRYPTONITE_SETTING_AUTOLAUNCH_URLS)){
        settings.SetValue(GRYPTONITE_SETTING_AUTOLAUNCH_URLS, true);
        settings.SetValue(GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE, true);
        settings.SetValue(GRYPTONITE_SETTING_CLOSE_MINIMIZES_TO_TRAY, true);
        settings.SetValue(GRYPTONITE_SETTING_TIME_FORMAT_24HR, true);
        settings.SetValue(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT, 15);
        settings.SetValue(GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT, 30);
        settings.CommitChanges();
    }
}

Application::Application(int &argc, char **argv)
    :GUtil::Qt::Application(argc, argv, GRYPTO_APP_NAME, GRYPTO_VERSION_STRING),
      settings(GRYPTO_SETTINGS_IDENTIFIER),
      main_window(NULL)
{
    // Log global messages to a group logger, which writes to all loggers in the group
    SetGlobalLogger(new GroupLogger{

                        // Comment this line for release (silently show errors only in log)
                        new GUtil::Qt::MessageBoxLogger,

                        new FileLogger(QString("%1/" APPLICATION_LOG)
                            .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).toUtf8()),
                    });

    CommandLineArgs args(argc, argv);
    String open_file;
    if(args.Length() > 1){
        open_file = args[1];
    }

    // Don't let exceptions crash us, they will be logged to the global logger
    SetTrapExceptions(true);

    // Initialize resources in case we have a static build
    Q_INIT_RESOURCE(grypto_ui);

    // Register the metatypes we are going to use
    qRegisterMetaType<shared_ptr<Exception<>>>("std::shared_ptr<GUtil::Exception<>>");
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
        GUtil::Qt::Application::handle_exception(ex);
    }
}

void Application::show_about(QWidget *)
{
    (new ::About(main_window))->ShowAbout();
}
