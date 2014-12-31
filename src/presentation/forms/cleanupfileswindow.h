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

#ifndef CLEANUPFILESWINDOW_H
#define CLEANUPFILESWINDOW_H

#include <gutil/qt_settings.h>
#include <QWidget>

namespace Ui {
class CleanupFilesWindow;
}
class QListWidgetItem;

namespace Grypt{
class DatabaseModel;


class CleanupFilesWindow : public QWidget
{
    Q_OBJECT
    Ui::CleanupFilesWindow *ui;
    GUtil::Qt::Settings *m_settings;
    DatabaseModel *m_dbModel;
public:
    explicit CleanupFilesWindow(DatabaseModel *,
                                GUtil::Qt::Settings *,
                                QWidget *parent = 0);
    ~CleanupFilesWindow();


protected:

    virtual void closeEvent(QCloseEvent *);
    virtual bool eventFilter(QObject *, QEvent *);


private slots:

    void _remove_all();
    void _export_item(QListWidgetItem * = 0);

};


}

#endif // CLEANUPFILESWINDOW_H
