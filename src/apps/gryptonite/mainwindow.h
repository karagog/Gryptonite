/*Copyright 2014-2015 George Karagoulis

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

#include <grypto/clipboardaccess.h>
#include <grypto/databasemodel.h>
#include <grypto/lockout.h>
#include <gutil/smartpointer.h>
#include <gutil/progressbarcontrol.h>
#include <QMainWindow>
#include <QUndoStack>
#include <QSystemTrayIcon>
#include <QPointer>
#include <QAction>
#include <QProcess>
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
    friend class modal_dialog_helper_t;
public:

    /** Creates a main window, optionally opening the given file. */
    explicit MainWindow(GUtil::Qt::Settings *,
                        const QString &open_file = QString::null,
                        QWidget *parent = 0);
    ~MainWindow();

    bool IsFileOpen() const;

    bool IsLocked() const{ return m_isLocked; }

    bool IsReadOnly() const{ return m_readonly; }

    /** For proper behavior this must be called before any application cleanup code. */
    void AboutToQuit();

public slots:

    /** Puts the interface irrevocably into read only mode. This is used in case of
     *  emergencies, like if the data access layer is failing to write the database.
    */
    void DropToReadOnly();

    /** If the interface was in read-only mode, this puts it back in editable mode. */
    void RecoverFromReadOnly();

    /** This slot is called when a database import operation is finished. */
    void DatabaseImportFinished();

    /** Locks the interface. (true = lock)*/
    void Lock();

    /** Prompts the user for a password and if it's correct, unlocks the interface. */
    void RequestUnlock();

    /** Activates the main window and selects the entry given by id. */
    void ShowEntryById(const Grypt::EntryId &);

    /** Hides the main window and "pops out" the entry into a smaller dialogue window. */
    void PopOutCurrentEntry();


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
    void _export_to_xml();
    void _import_from_xml();

    void _new_entry();
    void _new_child_entry();
    void _edit_entry();
    void _delete_entry();
    void _add_remove_favorite();
    void _search();
    void _action_lock_unlock_interface();
    void _grypto_transforms();
    void _grypto_rng();

    void _undo();
    void _redo();
    void _expand_all();
    void _collapse_all();

    void _update_trayIcon_menu();
    void _favorite_action_clicked(QAction *);

    void _nav_index_changed(int);
    void _update_undo_text();
    void _update_recent_files(const QString & = QString::null);
    void _create_recent_files_menu(const QStringList &paths);

    void _filter_updated(const Grypt::FilterInfo_t &);

    void _treeview_doubleclicked(const QModelIndex &);
    void _treeview_currentindex_changed(const QModelIndex &);

    void _entry_row_activated(int);
    void _progress_updated(int, bool cancellable, const QString & = QString::null);

    void _organize_favorites();
    void _edit_preferences();
    void _tray_icon_activated(QSystemTrayIcon::ActivationReason);

    void _hide();
    void _show();

    void _reset_lockout_timer();

    void _entries_added_or_removed(const QModelIndex &);
    void _entries_moved(const QModelIndex &, int, int, const QModelIndex &);
    void _entries_updated(const QModelIndex &, const QModelIndex &);


private:
    Ui::MainWindow *ui;
    QSystemTrayIcon m_trayIcon;
    QToolButton *btn_navBack;
    QToolButton *btn_navForward;
    QLabel *m_fileLabel;
    GUtil::Qt::Settings *m_settings;
    GUtil::SmartPointer<QActionGroup> m_recentFilesGroup;
    QAction m_add_remove_favorites;
    QAction m_action_new_child;
    QPushButton m_btn_pop_out;
    QUndoStack m_navStack;
    Grypt::ClipboardAccess m_clipboard;
    Grypt::Lockout m_lockoutTimer;
    bool m_isLocked;
    bool m_importing;
    QByteArray m_lockedState;
    QByteArray m_savedState;
    QString m_keyfileLocation;
    QProcess m_grypto_transforms;
    QProcess m_grypto_rng;
    bool m_grypto_transforms_visible;
    QPointer<QWidget> m_entryView;

    bool m_minimize_msg_shown;
    bool m_canHide;
    bool m_readonly;

    GUtil::Qt::ProgressBarControl m_progressBar;
#ifdef Q_OS_WIN
    QWinTaskbarButton m_taskbarButton;
#endif // Q_OS_WIN

    void _new_open_database(const QString &);
    void _install_new_database_model(Grypt::DatabaseModel *dbm);
    void _update_ui_file_opened(bool);
    void _update_available_actions();
    void _lock_unlock_interface(bool);
    void _update_time_format();
    Grypt::FilteredDatabaseModel *_get_proxy_model() const;
    Grypt::DatabaseModel *_get_database_model() const;
    bool _verify_credentials();
    QString _get_keyfile_location() const;

    void _select_entry(const Grypt::EntryId &);
    Grypt::Entry const *_get_currently_selected_entry() const;
    void _edit_entry(const Grypt::Entry &);
    bool _handle_key_pressed(QKeyEvent *);
    void _prepare_ui_for_import();

};

#endif // MAINWINDOW_H
