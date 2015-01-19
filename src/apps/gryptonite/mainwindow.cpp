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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "preferences_edit.h"
#include "settings.h"
#include "legacymanager.h"
#include "grypto_common.h"
#include "grypto_newpassworddialog.h"
#include "grypto_getpassworddialog.h"
#include "grypto_databasemodel.h"
#include "grypto_filtereddatabasemodel.h"
#include "grypto_entry_edit.h"
#include "entry_popup.h"
#include "grypto_cryptotransformswindow.h"
#include <grypto_organizefavoritesdialog.h>
#include <gutil/qt_settings.h>
#include <gutil/widget.h>
#include <gutil/application.h>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QActionGroup>
#include <QToolButton>
#include <QLabel>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QWhatsThis>
#ifdef Q_OS_WIN
#include <QWinTaskbarProgress>
#endif // Q_OS_WIN
USING_NAMESPACE_GUTIL1(Qt);
USING_NAMESPACE_GUTIL1(CryptoPP);
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GRYPTO;
using namespace std;

#define STATUSBAR_MSG_TIMEOUT 5000
#define RECENT_FILE_LIMIT 5

#define SETTING_RECENT_FILES "recent_files"

static void __show_access_denied(QWidget *w, const QString &msg)
{
    QMessageBox::critical(w, QObject::tr("Access Denied"), msg);
}


MainWindow::MainWindow(GUtil::Qt::Settings *s, QWidget *parent)
    :QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_trayIcon(QIcon(":/grypto/icons/main.png")),
      m_fileLabel(new QLabel(this)),
      m_settings(s),
      m_add_remove_favorites(this),
      m_isLocked(true),
      m_minimize_msg_shown(false),
      m_requesting_unlock(false),
      m_progressBar(true)
#ifdef Q_OS_WIN
      ,m_taskbarButton(this)
#endif
{
    ui->setupUi(this);
    setWindowTitle(GRYPTO_APP_NAME);
    ui->action_About->setText(tr("&About " GRYPTO_APP_NAME));

    ui->treeView->setModel(new FilteredDatabaseModel(this));
    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    _update_trayIcon_menu();
    m_trayIcon.show();

    ui->statusbar->addPermanentWidget(&m_progressBar, 1);
    ui->statusbar->addPermanentWidget(m_fileLabel);

    // Set up the toolbar
    btn_navBack = new QToolButton(this);
    btn_navBack->setIcon(QIcon(":/grypto/icons/leftarrow.png"));
    btn_navBack->setToolTip(tr("Navigate Backwards"));
    btn_navBack->setWhatsThis(tr("Navigate backwards through the history of your selections"));
    btn_navBack->setAutoRaise(true);
    connect(btn_navBack, SIGNAL(clicked()), &m_navStack, SLOT(undo()));
    ui->toolBar->addWidget(btn_navBack);

    btn_navForward = new QToolButton(this);
    btn_navForward->setIcon(QIcon(":/grypto/icons/rightarrow.png"));
    btn_navForward->setToolTip(tr("Navigate Forwards"));
    btn_navBack->setWhatsThis(tr("Navigate forwards through the history of your selections"));
    btn_navForward->setAutoRaise(true);
    connect(btn_navForward, SIGNAL(clicked()), &m_navStack, SLOT(redo()));
    ui->toolBar->addWidget(btn_navForward);

    // Restore search parameters
    if(m_settings->Contains(MAINWINDOW_SEARCH_SETTING)){
        ui->searchWidget->SetFilter(
                    FilterInfo_t::FromXml(m_settings->Value(MAINWINDOW_SEARCH_SETTING).toString()));
    }

    connect(ui->actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->actionNewOpenDB, SIGNAL(triggered()), this, SLOT(_new_open_database()));
    connect(ui->action_Save_As, SIGNAL(triggered()), this, SLOT(_save_as()));
    connect(ui->action_export_ps, SIGNAL(triggered()), this, SLOT(_export_to_portable_safe()));
    connect(ui->action_import_ps, SIGNAL(triggered()), this, SLOT(_import_from_portable_safe()));
    connect(ui->action_Close, SIGNAL(triggered()), this, SLOT(_close_database()));
    connect(ui->actionNew_Entry, SIGNAL(triggered()), this, SLOT(_new_entry()));
    connect(ui->action_EditEntry, SIGNAL(triggered()), this, SLOT(_edit_entry()));
    connect(ui->action_DeleteEntry, SIGNAL(triggered()), this, SLOT(_delete_entry()));
    connect(&m_add_remove_favorites, SIGNAL(triggered()), this, SLOT(_add_remove_favorite()));
    connect(ui->action_Undo, SIGNAL(triggered()), this, SLOT(_undo()));
    connect(ui->action_Redo, SIGNAL(triggered()), this, SLOT(_redo()));
    connect(ui->action_Search, SIGNAL(triggered()), this, SLOT(_search()));
    connect(ui->actionLockUnlock, SIGNAL(triggered()), this, SLOT(_action_lock_unlock_interface()));
    connect(ui->action_cryptoTransform, SIGNAL(triggered()), this, SLOT(_cryptographic_transformations()));
    connect(ui->action_Favorites, SIGNAL(triggered()), this, SLOT(_organize_favorites()));
    connect(ui->action_Preferences, SIGNAL(triggered()), this, SLOT(_edit_preferences()));
    connect(ui->action_About, SIGNAL(triggered()), gApp, SLOT(About()));
    connect(&m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(_tray_icon_activated(QSystemTrayIcon::ActivationReason)));

    // Create a "what's this" action
    {
        QAction *a_whatsthis = QWhatsThis::createAction(this);
        ui->menu_Help->insertAction(ui->action_About, a_whatsthis);
        ui->menu_Help->insertSeparator(ui->action_About);
    }

    connect(ui->view_entry, SIGNAL(RowActivated(int)), this, SLOT(_entry_row_activated(int)));
    connect(ui->treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(_treeview_currentindex_changed(QModelIndex)));

    //connect(&m_undostack, SIGNAL(indexChanged(int)), this, SLOT(_update_undo_text()));
    connect(&m_navStack, SIGNAL(indexChanged(int)), this, SLOT(_nav_index_changed(int)));
    connect(&m_lockoutTimer, SIGNAL(Lock()), this, SLOT(Lock()));

    ui->treeView->installEventFilter(this);
    ui->searchWidget->installEventFilter(this);

    // Restore the previous session's window state
    if(m_settings->Contains(MAINWINDOW_GEOMETRY_SETTING))
        restoreGeometry(m_settings->Value(MAINWINDOW_GEOMETRY_SETTING).toByteArray());
    if(m_settings->Contains(MAINWINDOW_STATE_SETTING))
        restoreState(m_settings->Value(MAINWINDOW_STATE_SETTING).toByteArray());

    _update_ui_file_opened(false);
    _lock_unlock_interface(false);

    // Set up the recent files list and open the most recent
    _update_recent_files();

    if(m_settings->Value(GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE).toBool()){
        QList<QAction *> al = ui->menu_Recent_Files->actions();
        if(al.length() > 0 && QFile::exists(al[0]->data().toString()))
            al[0]->trigger();
    }

    // The window must be shown before setting up the taskbar button
    show();
#ifdef Q_OS_WIN
    m_taskbarButton.setWindow(this->windowHandle());
#endif // Q_OS_WIN
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::AboutToQuit()
{
    // This restores the state of all dock widgets
    _close_database();

    // Save the search settings before we close
    m_settings->SetValue(MAINWINDOW_SEARCH_SETTING, ui->searchWidget->GetFilter().ToXml());

    // Save the main window state so we can restore it next session
    m_settings->SetValue(MAINWINDOW_GEOMETRY_SETTING, saveGeometry());
    m_settings->SetValue(MAINWINDOW_STATE_SETTING, saveState());
    m_settings->CommitChanges();
}

void MainWindow::_hide()
{
    hide();
    if(!m_minimize_msg_shown){
        m_trayIcon.showMessage(tr("Minimized to Tray"),
                               tr(GRYPTO_APP_NAME" has been minimized to tray."
                                  "\nClick icon to reopen."));
        m_minimize_msg_shown = true;
    }
}

void MainWindow::_show()
{
    showNormal();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    if(m_settings->Value(GRYPTONITE_SETTING_CLOSE_MINIMIZES_TO_TRAY).toBool() &&
            IsFileOpen())
    {
        _hide();
    }
    else{
        QMainWindow::closeEvent(ev);
        QApplication::exit();
    }
}

bool MainWindow::_handle_key_pressed(QKeyEvent *ev)
{
    if(IsLocked())
        return false;

    bool ret = false;
    if(ev->modifiers().testFlag(::Qt::ControlModifier))
    {
        if(ev->key() == ::Qt::Key_Z)
        {
            _undo();
            ret = true;
        }
        else if(ev->key() == ::Qt::Key_Y)
        {
            _redo();
            ret = true;
        }
    }
    return ret;
}

void MainWindow::keyPressEvent(QKeyEvent *ev)
{
    if(_handle_key_pressed(ev))
        ev->accept();
    else
        QMainWindow::keyPressEvent(ev);
}

bool MainWindow::eventFilter(QObject *o, QEvent *ev)
{
    bool ret = false;

    // Depending on the event, we may need to reset the lockout timer
    _reset_lockout_timer(ev);

    if(ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
        ret = _handle_key_pressed(kev);
        if(!ret)
        {
            if(o == ui->treeView)
            {
                if(kev->key() == ::Qt::Key_Enter || kev->key() == ::Qt::Key_Return){
                    _treeview_doubleclicked(ui->treeView->currentIndex());
                    ret = true;
                }
                else if(kev->key() == ::Qt::Key_Delete){
                    _delete_entry();
                    ret = true;
                }
                else if(kev->key() == ::Qt::Key_Escape){
                    // The escape key nulls out the treeview selection
                    ui->treeView->setCurrentIndex(QModelIndex());
                }
            }
        }
    }
    else if(ev->type() == QEvent::ContextMenu && IsFileOpen())
    {
        QContextMenuEvent *cm = static_cast<QContextMenuEvent *>(ev);
        if(o == ui->treeView)
        {
            // Customize the menu a little bit
            QMenu *menu = new QMenu(this);
            QList<QAction*> actions = ui->menuEntry->actions();
            menu->addActions(actions);

            QAction *action_expand = new QAction(tr("Expand All"), this);
            QAction *action_collapse = new QAction(tr("Collapse All"), this);
            connect(action_expand, SIGNAL(triggered()), ui->treeView, SLOT(expandAll()));
            connect(action_collapse, SIGNAL(triggered()), ui->treeView, SLOT(collapseAll()));
            menu->insertAction(ui->action_Search, action_expand);
            menu->insertAction(ui->action_Search, action_collapse);
            menu->insertSeparator(ui->action_Search);

            Entry const *e = _get_currently_selected_entry();
            if(e){
                m_add_remove_favorites.setData(e->IsFavorite());
                if(e->IsFavorite())
                    m_add_remove_favorites.setText(tr("Remove from Favorites"));
                else
                    m_add_remove_favorites.setText(tr("Add to Favorites"));

                menu->insertAction(ui->action_Search, &m_add_remove_favorites);
                menu->insertSeparator(&m_add_remove_favorites);
            }
            menu->insertSeparator(ui->action_Search);
            menu->insertSeparator(ui->action_Search);

            menu->move(cm->globalPos());
            menu->show();
        }
    }
    return ret;
}

Entry const *MainWindow::_get_currently_selected_entry() const
{
    return _get_database_model()->GetEntryFromIndex(
                _get_proxy_model()->mapToSource(
                    ui->treeView->currentIndex()));
}

void MainWindow::_update_recent_files(const QString &latest_path)
{
    QStringList paths;
    if(m_settings->Contains(SETTING_RECENT_FILES))
        paths = m_settings->Value(SETTING_RECENT_FILES).toStringList();

    if(!latest_path.isEmpty())
    {
        int ind = paths.indexOf(latest_path);
        if(ind != -1)
        {
            // Move the path to the front
            paths.removeAt(ind);
        }
        paths.append(latest_path);

        // Trim the list to keep it within limits
        while(paths.length() > RECENT_FILE_LIMIT)
            paths.removeAt(0);

        m_settings->SetValue(SETTING_RECENT_FILES, paths);
        m_settings->CommitChanges();
    }

    // Refresh the menu
    if(m_recentFilesGroup)
        m_recentFilesGroup.Clear();

    ui->menu_Recent_Files->clear();
    if(paths.length() > 0)
    {
        ui->menu_Recent_Files->setEnabled(true);
        m_recentFilesGroup = new QActionGroup(this);

        for(int i = paths.length() - 1; i >= 0; --i)
        {
            QAction *a = m_recentFilesGroup->addAction(paths[i]);
            a->setData(paths[i]);
        }
        ui->menu_Recent_Files->addActions(m_recentFilesGroup->actions());

        connect(m_recentFilesGroup, SIGNAL(triggered(QAction*)), this, SLOT(_open_recent_database(QAction *)),
                ::Qt::QueuedConnection);
    }
    else{
        ui->menu_Recent_Files->setEnabled(false);
    }
}

void MainWindow::_install_new_database_model(DatabaseModel *dbm)
{
    connect(dbm, SIGNAL(NotifyFavoritesUpdated()),
            this, SLOT(_update_trayIcon_menu()));
    connect(dbm, SIGNAL(NotifyUndoStackChanged()),
            this, SLOT(_update_undo_text()));

    // Until I have time to resolve issues with pre-loading grandchildren before
    //  their ancestors, I will just load all entries up front
    dbm->FetchAllEntries();

    _get_proxy_model()->setSourceModel(dbm);
    _update_ui_file_opened(true);

    if(m_settings->Contains(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT))
        m_lockoutTimer.StartLockoutTimer(m_settings->Value(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT).toInt());

    ui->view_entry->SetDatabaseModel(dbm);

    m_fileLabel->setText(dbm->FilePath());

    // Add this to the recent files list
    _update_recent_files(dbm->FilePath());

    // Wire up the progress bar control
    connect(&m_progressBar, SIGNAL(Clicked()), dbm, SLOT(CancelAllBackgroundOperations()));
    connect(dbm, SIGNAL(NotifyProgressUpdated(int,QString)),
            this, SLOT(_progress_updated(int,QString)));
}

void MainWindow::_new_open_database(const QString &path)
{
    Credentials creds;
    QString open_path = path;

    bool existed = QFile::exists(path);
    bool file_updated = false;
    if(existed)
    {
        // Check the file's version and update if necessary
        try{
            PasswordDatabase::ValidateDatabase(path.toUtf8());
        }
        catch(...)
        {
            // If validation failed, try to upgrade the file
            // First we don't allow cancelling during the upgrade
            bool tmp = m_progressBar.IsCancellable();
            m_progressBar.SetIsCancellable(false);
            finally([&]{ m_progressBar.SetIsCancellable(tmp); });

            // Call the legacy manager to upgrade the database
            open_path = LegacyManager::UpgradeDatabase(path, creds, m_settings,
                [=](int p, const QString &ps){
                    this->_progress_updated(p, ps);
                },
                this);

            if(open_path.isNull())
                return;
            else
                file_updated = true;
        }
    }

    // Create a database model, which locks the database for us exclusively and prepares it to be opened
    SmartPointer<DatabaseModel> dbm(new DatabaseModel(open_path.toUtf8(),

        // A function to confirm overriding the lockfile
        [&](const PasswordDatabase::ProcessInfo &pi){
            return QMessageBox::Yes == QMessageBox::warning(this,
                        tr("Locked by another process"),
                        QString(tr("The database is currently locked by another process:\n"
                                   "\n\tProcess ID: %1"
                                   "\n\tHost Name: %2"
                                   "\n\tApp Name: %3\n"
                                   "\nDo you want to override the lock and open the file anyways?"
                                   " (NOT recommended unless you know what you're doing!)"))
                        .arg(pi.ProcessId)
                        .arg(pi.HostName.isEmpty() ? tr("(Unknown)") : pi.HostName)
                        .arg(pi.AppName),
                        QMessageBox::Yes | QMessageBox::Cancel,
                        QMessageBox::Cancel);
        })
    );

    // Get the user's credentials after successfully locking the database
    if(!existed){
        NewPasswordDialog dlg(m_settings, this);
        if(QDialog::Rejected == dlg.exec())
            return;
        creds = dlg.GetCredentials();
    }
    else if(!file_updated){
        GetPasswordDialog dlg(m_settings, QFileInfo(path).fileName(), this);
        if(QDialog::Rejected == dlg.exec())
            return;
        creds = dlg.GetCredentials();
    }

    // Close the database (if one was open)
    _close_database();

    try{
        dbm->Open(creds);
    }
    catch(const GUtil::AuthenticationException<> &){
        __show_access_denied(this, tr("Invalid Key"));
        return;
    }
    _install_new_database_model(dbm.Data());
    dbm.Relinquish();
}

static QString __get_new_database_filename(QWidget *parent,
                                           const QString &title,
                                           bool confirm_overwrite)
{
    return QFileDialog::getSaveFileName(parent, title,
                                        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                        "Grypto DB (*.gdb *.GPdb);;All Files (*)", 0,
                                        confirm_overwrite ? (QFileDialog::Option)0 : QFileDialog::DontConfirmOverwrite);
}

void MainWindow::_new_open_database()
{
    QString path = __get_new_database_filename(this, tr("File Location"), false);
    if(!path.isEmpty())
    {
        QFileInfo fi(path);
        if(!fi.exists() && fi.suffix() != "gdb")
            path.append(".gdb");
        _new_open_database(path);
    }
}

void MainWindow::_open_recent_database(QAction *a)
{
    QString path = a->data().toString();
    if(QFile::exists(path))
        _new_open_database(path);
    else
        QMessageBox::warning(this, tr("File not found"), QString("The file does not exist: %1").arg(path));
}

bool MainWindow::IsFileOpen() const
{
    return _get_proxy_model()->sourceModel() != NULL;
}

void MainWindow::_close_database(bool delete_model)
{
    if(IsFileOpen())
    {
        m_lockoutTimer.StopLockoutTimer();

        // Disconnect the database model
        QAbstractItemModel *old_model = _get_proxy_model()->sourceModel();
        _get_proxy_model()->setSourceModel(NULL);

        // Optionally delete it; maybe we still need it for something
        if(delete_model)
            delete old_model;

        _lock_unlock_interface(false);
        _update_ui_file_opened(false);
    }
}

void MainWindow::_save_as()
{
    GASSERT(IsFileOpen());
    QString fn = __get_new_database_filename(this, tr("Save as file"), true);
    if(fn.isEmpty())
        return;
    else if(QFileInfo(fn).absoluteFilePath() ==
            QFileInfo(_get_database_model()->FilePath()).absoluteFilePath())
        throw Exception<>("Cannot save to the same path as the original");

    // Create the database object, which only reserves a lock on the new database
    SmartPointer<DatabaseModel> dbm(new DatabaseModel(fn.toUtf8().constData()));

    NewPasswordDialog dlg(m_settings, this);
    if(QDialog::Accepted != dlg.exec())
        return;

    if(QFile::exists(fn))
        if(!QFile::remove(fn))
            throw Exception<>("Save as file already exists and I couldn't remove it");

    // Initialize the new database
    dbm->Open(dlg.GetCredentials());

    // Close the old database and install the new one on the main window
    DatabaseModel *old_model = _get_database_model();
    _close_database(false);     // Don't delete the old database model!
    _install_new_database_model(dbm);

    bool tmp = m_progressBar.IsCancellable();
    m_progressBar.SetIsCancellable(false);
    finally([&]{ m_progressBar.SetIsCancellable(tmp); });

    // Copy the old database to the new one
    dbm->ImportFromDatabase(*old_model);

    dbm.Relinquish();
    delete old_model;
}

void MainWindow::_export_to_portable_safe()
{
    QString fn = QFileDialog::getSaveFileName(this, tr("Export to Portable Safe"),
                                              QString(),
                                              "GUtil Portable Safe (*.gps)"
                                              ";;All Files(*)");
    if(fn.isEmpty())
        return;

    QFileInfo fi(fn);
    if(fi.suffix().isEmpty())
        fn.append(".gps");

    NewPasswordDialog dlg(m_settings, this);
    if(QDialog::Accepted != dlg.exec())
        return;

    _get_database_model()->ExportToPortableSafe(fn.toUtf8(), dlg.GetCredentials());
}

void MainWindow::_import_from_portable_safe()
{
    GASSERT(IsFileOpen());
    QString fn = QFileDialog::getOpenFileName(this, tr("Import from Portable Safe"),
                                              QString(),
                                              "GUtil Portable Safe (*.gps)"
                                              ";;All Files(*)");
    if(fn.isEmpty() || !QFile::exists(fn))
        return;

    GetPasswordDialog dlg(m_settings, fn, this);
    if(QDialog::Accepted != dlg.exec())
        return;

    _get_database_model()->ImportFromPortableSafe(fn.toUtf8(), dlg.GetCredentials());
}

void MainWindow::_new_entry()
{
    if(!IsFileOpen())
        return;

    Entry e;
    DatabaseModel *model = _get_database_model();
    QModelIndex ind = _get_proxy_model()->mapToSource(ui->treeView->currentIndex());
    Entry const *selected = model->GetEntryFromIndex(ind);

    if(selected)
    {
        e.SetParentId(selected->GetId());
        e.SetRow(model->rowCount(ind));
    }

    EntryEdit dlg(e, _get_database_model(), this);
    if(QDialog::Accepted == dlg.exec())
        model->AddEntry(dlg.GetEntry());
}

void MainWindow::_update_ui_file_opened(bool b)
{
    ui->treeView->setEnabled(b);
    ui->view_entry->setEnabled(b);
    ui->searchWidget->setEnabled(b);

    ui->treeView->setEnabled(b);
    ui->view_entry->setEnabled(b);
    ui->view_entry->SetEntry(Entry());

    ui->actionNew_Entry->setEnabled(b);
    ui->action_Favorites->setEnabled(b);
    m_add_remove_favorites.setEnabled(b);
    ui->action_Search->setEnabled(b);
    ui->actionLockUnlock->setEnabled(b);

    ui->action_Save_As->setEnabled(b);
    ui->action_Close->setEnabled(b);
    ui->menuEntry->setEnabled(b);
    ui->menu_Export->setEnabled(b);
    ui->menu_Import->setEnabled(b);

    if(!b)
    {
        if(_get_database_model())
            _get_database_model()->ClearUndoStack();
        m_navStack.clear();

        btn_navBack->setEnabled(false);
        btn_navForward->setEnabled(false);

        ui->searchWidget->Clear();
        m_fileLabel->clear();
    }
    _update_undo_text();
    _update_trayIcon_menu();
    _treeview_currentindex_changed(ui->treeView->currentIndex());
}

void MainWindow::_favorite_action_clicked(QAction *a)
{
    Entry e = _get_database_model()->FindEntryById(a->data().value<EntryId>());

    // Open any URL fields with QDesktopServices
    if(m_settings->Value(GRYPTONITE_SETTING_AUTOLAUNCH_URLS).toBool()){
        for(const SecretValue &sv : e.Values()){
            if(sv.GetName().toLower() == "url"){
                if(!QDesktopServices::openUrl(QUrl(sv.GetValue()))){
                    QMessageBox::warning(this, tr("Opening URL Failed"),
                                         QString(tr("Could not open URL: %1"))
                                         .arg(sv.GetValue().constData()));
                }
            }
        }
    }

    EntryPopup *ep = new EntryPopup(e, m_settings, this);
    connect(ep, SIGNAL(SelectInMainWindow(const Grypt::EntryId &)),
            this, SLOT(ShowEntryById(const Grypt::EntryId &)));

    if(m_entryView)
        m_entryView->deleteLater();
    (m_entryView = ep)->show();
    _hide();
}

static QMenu *__create_menu(DatabaseModel *dbm, QActionGroup &ag, const QModelIndex &ind, QWidget *parent)
{
    Entry const *e = dbm->GetEntryFromIndex(ind);
    GASSERT(e);

    SmartPointer<QMenu> ret(new QMenu(e->GetName(), parent));
    for(int i = 0; i < dbm->rowCount(ind); ++i){
        QModelIndex child_index = dbm->index(i, 0, ind);
        if(0 < dbm->rowCount(child_index)){
            // Continue recursing as long as we find children
            ret->addMenu(__create_menu(dbm, ag, child_index, parent));
        }
        else{
            QAction *a = ag.addAction(dbm->data(child_index, ::Qt::DisplayRole).toString());
            a->setData(dbm->data(child_index, DatabaseModel::EntryIdRole));
            ret->addAction(a);
        }
    }
    return ret.Relinquish();
}

void MainWindow::_update_trayIcon_menu()
{
    QMenu *old_menu = m_trayIcon.contextMenu();
    m_trayIcon.setContextMenu(new QMenu(this));

    // Add the user's favorites to the list
    DatabaseModel *dbm = _get_database_model();
    if(dbm && !IsLocked()){
        QList<Entry> favs = dbm->FindFavorites();
        QActionGroup *ag = new QActionGroup(this);
        connect(ag, SIGNAL(triggered(QAction*)), this, SLOT(_favorite_action_clicked(QAction*)));
        for(const Entry &fav : favs){
            QModelIndex ind = dbm->FindIndexById(fav.GetId());
            if(0 < dbm->rowCount(ind)){
                // If the favorite has children, we recurse and add them to the menu
                QMenu *m = __create_menu(dbm, *ag, ind, this);
                m->setIcon(QIcon(":/grypto/icons/star.png"));
                m_trayIcon.contextMenu()->addMenu(m);
            }
            else{
                QAction *a = ag->addAction(QIcon(":/grypto/icons/star.png"), fav.GetName());
                a->setData(QVariant::fromValue(fav.GetId()));
                m_trayIcon.contextMenu()->addAction(a);
            }
        }
        m_trayIcon.contextMenu()->addSeparator();
    }

    m_trayIcon.contextMenu()->addAction(ui->actionLockUnlock);
    m_trayIcon.contextMenu()->addAction(ui->action_Search);
    m_trayIcon.contextMenu()->addSeparator();
    m_trayIcon.contextMenu()->addAction(ui->actionQuit);

    if(old_menu) old_menu->deleteLater();
}

void MainWindow::_update_undo_text()
{
    DatabaseModel *m = _get_database_model();
    ui->action_Undo->setText(m && m->CanUndo() ? QString("Undo %1").arg(m->UndoText().ConstData()) : "Undo");
    ui->action_Undo->setEnabled(m && m->CanUndo());

    ui->action_Redo->setText(m && m->CanRedo() ? QString("Redo %1").arg(m->RedoText().ConstData()) : "Redo");
    ui->action_Redo->setEnabled(m && m->CanRedo());
}

FilteredDatabaseModel *MainWindow::_get_proxy_model() const
{
    QAbstractItemModel *model = ui->treeView->model();
    return NULL == model ? NULL : static_cast<FilteredDatabaseModel *>(model);
}

DatabaseModel *MainWindow::_get_database_model() const
{
    return static_cast<DatabaseModel *>(_get_proxy_model()->sourceModel());
}

void MainWindow::_edit_entry()
{
    _treeview_doubleclicked(ui->treeView->currentIndex());
}

void MainWindow::_delete_entry()
{
    QModelIndex ind = ui->treeView->currentIndex();
    if(ind.isValid()){
        DatabaseModel *model = _get_database_model();
        Entry const *e = model->GetEntryFromIndex(_get_proxy_model()->mapToSource(ind));
        if(QMessageBox::Yes == QMessageBox::question(
                    this, tr("Really?"),
                    QString(tr("Confirm that you really want to delete this and all child entries of: %1")).arg(e->GetName()),
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel))
        {
            model->RemoveEntry(*e);
        }
    }
}

void MainWindow::_add_remove_favorite()
{
    bool is_favorite = m_add_remove_favorites.data().toBool();
    Entry const *e = _get_currently_selected_entry();
    if(NULL == e)
        return;

    if(is_favorite)
        _get_database_model()->RemoveEntryFromFavorites(e->GetId());
    else
        _get_database_model()->AddEntryToFavorites(e->GetId());
}

void MainWindow::_edit_entry(const Entry &e)
{
    EntryEdit dlg(e, _get_database_model(), this);
    if(QDialog::Accepted == dlg.exec())
    {
        _get_database_model()->UpdateEntry(dlg.GetEntry());
        _nav_index_changed(m_navStack.index());
    }
}

void MainWindow::_treeview_doubleclicked(const QModelIndex &ind)
{
    if(ind.isValid())
    {
        Entry const *e = _get_database_model()->GetEntryFromIndex(_get_proxy_model()->mapToSource(ind));
        _edit_entry(*e);
    }
}

void MainWindow::_treeview_currentindex_changed(const QModelIndex &ind)
{
    Entry const *e = _get_database_model()->GetEntryFromIndex(_get_proxy_model()->mapToSource(ind));
    ui->action_EditEntry->setEnabled(e);
    ui->action_DeleteEntry->setEnabled(e);
    if(e)
        _select_entry(e->GetId());
}

void MainWindow::_entry_row_activated(int r)
{
    if(0 <= r){
        const SecretValue &v = ui->view_entry->GetEntry().Values()[r];
        m_clipboard.SetText(v.GetValue(),
                            v.GetIsHidden() ?
                                m_settings->Value(GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT).toInt() * 1000 :
                                0);
        ui->statusbar->showMessage(QString("Copied %1 to clipboard").arg(v.GetName()),
                                   STATUSBAR_MSG_TIMEOUT);
    }
}

void MainWindow::_undo()
{
    DatabaseModel *m = _get_database_model();
    if(m->CanUndo())
    {
        QString txt = m->UndoText().ToQString();
        m->Undo();
        ui->statusbar->showMessage(QString("Undid %1").arg(txt), STATUSBAR_MSG_TIMEOUT);
    }
}

void MainWindow::_redo()
{
    DatabaseModel *m = _get_database_model();
    if(m->CanRedo())
    {
        QString txt = m->RedoText().ToQString();
        m->Redo();
        ui->statusbar->showMessage(QString("Redid %1").arg(txt), STATUSBAR_MSG_TIMEOUT);
    }
}


class navigation_command : public QUndoCommand
{
public:
    navigation_command(const EntryId &entry_id)
        :CurEntryId(entry_id) {}

    EntryId CurEntryId;

};

void MainWindow::_nav_index_changed(int ind)
{
    ind -= 1;
    bool success = false;
    if(ind >= 0){
        try{
            Entry e = _get_database_model()->FindEntryById(
                static_cast<const navigation_command *>(m_navStack.command(ind))->CurEntryId);

            ui->view_entry->SetEntry(e);

            QItemSelectionModel *ism = ui->treeView->selectionModel();
            QModelIndex ind = _get_proxy_model()->mapFromSource(_get_database_model()->FindIndexById(e.GetId()));
            if(ind.isValid()){
                ism->select(ind, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
                ui->treeView->setSelectionModel(ism);
                ui->treeView->ExpandToIndex(ind);
            }
            success = true;
        } catch(...) {}
    }

    if(!success)
        ui->view_entry->SetEntry(Entry());

    btn_navBack->setEnabled(m_navStack.canUndo());
    btn_navForward->setEnabled(m_navStack.canRedo());
}

void MainWindow::_select_entry(const EntryId &id)
{
    bool allow = true;
    if(m_navStack.index() > 0)
    {
        allow = id !=
                static_cast<const navigation_command *>(m_navStack.command(m_navStack.index() - 1))->CurEntryId;
    }

    if(allow)
        m_navStack.push(new navigation_command(id));
}

void MainWindow::ShowEntryById(const Grypt::EntryId &id)
{
    showNormal();
    activateWindow();
    _select_entry(id);
}

void MainWindow::_filter_updated(const FilterInfo_t &fi)
{
    FilteredDatabaseModel *fm = _get_proxy_model();
    if(fm == NULL)
        return;

    // Apply the filter
    fm->SetFilter(fi);

    // Highlight any matching rows
    if(fi.IsValid)
    {
        QItemSelection is;

        foreach(QModelIndex ind, fm->GetUnfilteredRows())
        {
            ui->treeView->ExpandToIndex(ind);
            is.append(QItemSelectionRange(ind, fm->index(ind.row(), fm->columnCount() - 1, ind.parent())));
        }

        ui->treeView->selectionModel()->select(is, QItemSelectionModel::ClearAndSelect);
    }
    else
    {
        ui->treeView->selectionModel()->clearSelection();
    }
}

void MainWindow::_search()
{
    if(isMinimized() || isHidden())
        showNormal();

    activateWindow();
    ui->dw_search->show();
    ui->dw_search->raise();
    ui->searchWidget->setFocus();
}

void MainWindow::Lock()
{
    _lock_unlock_interface(true);
}

void MainWindow::_lock_unlock_interface(bool lock)
{
    if(lock)
    {
        if(IsLocked() || !IsFileOpen())
            return;

        m_savedState = saveState();
        ui->dw_treeView->hide();
        ui->dw_search->hide();
        ui->toolBar->hide();
        ui->stackedWidget->setCurrentIndex(0);
        ui->actionLockUnlock->setText(tr("&Unlock Application"));
        ui->actionLockUnlock->setData(false);
        m_isLocked = true;

        _hide();
        m_trayIcon.showMessage(tr("Locked"),
                               tr(GRYPTO_APP_NAME" has been locked."
                                  "\nClick icon to unlock."));
    }
    else
    {
        if(!IsLocked())
            return;

        restoreState(m_savedState);
        ui->stackedWidget->setCurrentIndex(1);
        ui->actionLockUnlock->setText(tr("&Lock Application"));
        ui->actionLockUnlock->setData(true);
        m_isLocked = false;
    }

    bool b = !lock && IsFileOpen();
    ui->action_Save_As->setEnabled(b);
    ui->menu_Export->setEnabled(b);
    ui->menu_Import->setEnabled(b);
    ui->actionNew_Entry->setEnabled(b);
    ui->action_EditEntry->setEnabled(b);
    ui->action_DeleteEntry->setEnabled(b);
    ui->action_Undo->setEnabled(b);
    ui->action_Redo->setEnabled(b);
    ui->action_Search->setEnabled(b);

    _update_trayIcon_menu();
}

void MainWindow::_action_lock_unlock_interface()
{
    if(ui->actionLockUnlock->data().toBool())
        Lock();
    else{
        if(isHidden())
            showNormal();
        RequestUnlock();
    }
}

void MainWindow::RequestUnlock()
{
    if(m_requesting_unlock)
        return;

    m_requesting_unlock = true;
    GetPasswordDialog dlg(m_settings, _get_database_model()->FilePath(), this);
    if(QDialog::Accepted == dlg.exec())
    {
        if(_get_database_model()->CheckCredentials(dlg.GetCredentials()))
            _lock_unlock_interface(false);
        else
            __show_access_denied(this, tr("Invalid Password"));
    }
    m_requesting_unlock = false;
}

bool MainWindow::event(QEvent *e)
{
    _reset_lockout_timer(e);
    return QMainWindow::event(e);
}

void MainWindow::_reset_lockout_timer(QEvent *e)
{
    switch(e->type())
    {
    // All of these events cause us to reset the lockout timer
    case QEvent::WindowStateChange:
    case QEvent::FocusIn:
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
        if(m_settings->Contains(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT))
            m_lockoutTimer.ResetLockoutTimer(m_settings->Value(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT).toInt());

        // For better debugging of this event, let's hear a beep whenever we reset the timer
        //QApplication::beep();
        break;
    default:
        break;
    }
}

void MainWindow::_cryptographic_transformations()
{
    if(!m_encryptDecryptWindow){
        m_encryptDecryptWindow = new CryptoTransformsWindow(
                    m_settings,
                    IsFileOpen() ? &_get_database_model()->Cryptor() : NULL
                                   );
        m_encryptDecryptWindow->setAttribute(::Qt::WA_DeleteOnClose, false);
    }
    m_encryptDecryptWindow->show();
    m_encryptDecryptWindow->activateWindow();
}

void MainWindow::_progress_updated(int progress, const QString &task_name)
{
    m_progressBar.SetProgress(progress, task_name);
#ifdef Q_OS_WIN
    m_taskbarButton.progress()->setValue(progress);
#endif // Q_OS_WIN

    if(progress == 100){
        ui->statusbar->showMessage(QString(tr("Finished %1")).arg(task_name), STATUSBAR_MSG_TIMEOUT);
#ifdef Q_OS_WIN
        m_taskbarButton.progress()->hide();
    }
    else if(!m_taskbarButton.progress()->isVisible()){
        m_taskbarButton.progress()->show();
#endif // Q_OS_WIN
    }
}

void MainWindow::_organize_favorites()
{
    GASSERT(IsFileOpen());
    DatabaseModel *dbm = _get_database_model();
    OrganizeFavoritesDialog dlg(dbm->FindFavorites(), this);
    if(QDialog::Accepted == dlg.exec()){
        dbm->SetFavoriteEntries(dlg.GetFavorites());
    }
}

void MainWindow::_edit_preferences()
{
    PreferencesEdit dlg(m_settings, this);
    if(QDialog::Accepted == dlg.exec()){
        // Update ourselves to reflect the new settings...
    }
}

void MainWindow::_tray_icon_activated(QSystemTrayIcon::ActivationReason ar)
{
    switch(ar)
    {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:
        if(isVisible())
            _hide();
        else{
            _show();
            if(IsLocked())
                RequestUnlock();
        }
        break;
    default:
        break;
    }
}
