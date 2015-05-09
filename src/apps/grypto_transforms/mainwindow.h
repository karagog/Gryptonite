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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gutil/qt_settings.h>
#include <gutil/consoleiodevice.h>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Ui::MainWindow *ui;
    GUtil::Qt::Settings *m_settings;

    // This guy reads stdin for us on a background thread, so we
    //  can respond asynchronously to certain commands
    GUtil::Qt::ConsoleIODevice m_console;

public:
    explicit MainWindow(GUtil::Qt::Settings *, QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual void closeEvent(QCloseEvent *);

private slots:
    void _new_msg_stdin();
    void _about_to_quit();
};

#endif // MAINWINDOW_H
