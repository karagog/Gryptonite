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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "lockout.h"
#include "grypto_clipboardaccess.h"
#include "grypto_databasemodel.h"
#include <gutil/smartpointer.h>
#include <gutil/progressbarcontrol.h>
#include <QMainWindow>
#include <QUndoStack>
#include <QSystemTrayIcon>
#include <QPointer>
#include <QAction>
#ifdef Q_OS_WIN
#include <QWinTaskbarButton>
#endif // Q_OS_WIN

#define MAINWINDOW_GEOMETRY_SETTING "MAINWIN_GEOMETRY"
#define MAINWINDOW_STATE_SETTING    "MAINWIN_STATE"
#define MAINWINDOW_SEARCH_SETTING   "MAINWIN_SEARCH_PARAMS"

namespace Ui {
class MainWindow;
}

namespace Grypt{
class FilteredDatabaseModel;
class FilterInfo_t;
class EntryModel;
class Entry;
class EntryView;
}

class QLabel;

namespace GUtil{ namespace Qt{
    class Settings;
}}

class QModelIndex;
class QToolButton;
class QActionGroup;


class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    explicit MainWindow(GUtil::Qt::Settings *, QWidget *parent = 0);
    ~MainWindow();

    bool IsFileOpen() const;

    bool IsLocked() const{ return m_isLocked; }

    /** For proper behavior this must be called before any application cleanup code. */
    void AboutToQuit();

signals:

    void NotifyUserInteraction();


public slots:

    /** Locks the interface. (true = lock)*/
    void Lock();

    /** Prompts the user for a password and if it's correct, unlocks the interface. */
    void RequestUnlock();

    /** Activates the main window and selects the entry given by id. */
    void ShowEntryById(const Grypt::EntryId &);


protected:

    virtual bool event(QEvent *);
    virtual void closeEvent(QCloseEvent *);
    virtual void keyPressEvent(QKeyEvent *);
    virtual bool eventFilter(QObject *, QEvent *);


private slots:

    void _new_open_database();
    void _open_recent_database(QAction *);
    void _close_database(bool delete_model = true);
    void _save_as();
    void _export_to_portable_safe();
    void _import_from_portable_safe();

    void _new_entry();
    void _edit_entry();
    void _delete_entry();
    void _add_remove_favorite();
    void _search();
    void _action_lock_unlock_interface();
    void _cryptographic_transformations();

    void _undo();
    void _redo();

    void _update_trayIcon_menu();
    void _favorite_action_clicked(QAction *);

    void _nav_index_changed(int);
    void _update_undo_text();
    void _update_recent_files(const QString & = QString::null);

    void _filter_updated(const Grypt::FilterInfo_t &);

    void _treeview_doubleclicked(const QModelIndex &);
    void _treeview_currentindex_changed(const QModelIndex &);

    void _entry_row_activated(int);
    void _progress_updated(int, const QString & = QString::null);

    void _organize_favorites();
    void _edit_preferences();
    void _tray_icon_activated(QSystemTrayIcon::ActivationReason);

    void _hide();
    void _show();


private:
    Ui::MainWindow *ui;
    QSystemTrayIcon m_trayIcon;
    QToolButton *btn_navBack;
    QToolButton *btn_navForward;
    QLabel *m_fileLabel;
    GUtil::Qt::Settings *m_settings;
    GUtil::SmartPointer<QActionGroup> m_recentFilesGroup;
    QAction m_add_remove_favorites;
    QUndoStack m_navStack;
    Grypt::ClipboardAccess m_clipboard;
    Lockout m_lockoutTimer;
    bool m_isLocked;
    QByteArray m_savedState;
    GUtil::SmartPointer<QWidget> m_encryptDecryptWindow;
    QPointer<QWidget> m_entryView;

    bool m_minimize_msg_shown;
    bool m_requesting_unlock;

    GUtil::Qt::ProgressBarControl m_progressBar;
#ifdef Q_OS_WIN
    QWinTaskbarButton m_taskbarButton;
#endif // Q_OS_WIN

    void _new_open_database(const QString &);
    void _install_new_database_model(Grypt::DatabaseModel *dbm);
    void _update_ui_file_opened(bool);
    void _lock_unlock_interface(bool);
    Grypt::FilteredDatabaseModel *_get_proxy_model() const;
    Grypt::DatabaseModel *_get_database_model() const;

    void _select_entry(const Grypt::EntryId &);
    Grypt::Entry const *_get_currently_selected_entry() const;
    void _edit_entry(const Grypt::Entry &);
    bool _handle_key_pressed(QKeyEvent *);
    void _reset_lockout_timer(QEvent *);

};

#endif // MAINWINDOW_H
