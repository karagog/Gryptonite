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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <gutil/qt_settings.h>
#include <gutil/application.h>
#include <gutil/updatenotifier.h>

namespace CryptoPP{
class Exception;
}

class MainWindow;


class Application :
        public GUtil::Qt::Application
{
    Q_OBJECT
    GUtil::Qt::Settings settings;
    GUtil::Qt::UpdateNotifier updater;
    bool m_silent;
    MainWindow *main_window;
public:

    Application(int &argc, char **argv);


public slots:

    /** Checks the internet to see if updates are available */
    virtual void CheckForUpdates(bool);


protected:

    virtual void about_to_quit();
    virtual void handle_exception(std::exception &);
    virtual void show_about(QWidget *);


private slots:

    void _update_info_received(const QString &, const QUrl &);
    void _disable_auto_update();

};

/** Gives you global access to the application instance */
#define GRYPTO_APP static_cast<Application *>(qApp)

#endif // APPLICATION_H
