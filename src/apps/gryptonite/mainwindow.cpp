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

#define SETTING_RECENT_FILES             "gryptonite_recent_files"

static void __show_access_denied(QWidget *w, const QString &msg)
{
    QMessageBox::critical(w, QObject::tr("Access Denied"), msg);
}

// Use this to help deal with modal dialogs, as certain actions should be disabled
//  while in a modal dialog
class modal_dialog_helper_t{
    MainWindow *m_mainWindow;

    QMenu *m_contextMenu;

    int m_wasLockoutTimerStarted;
    int m_lockoutMinutes;
public:
    modal_dialog_helper_t(MainWindow *mw)
        :m_mainWindow(mw),
          m_contextMenu(mw->m_trayIcon.contextMenu()),
          m_wasLockoutTimerStarted(mw->m_lockoutTimer.StopLockoutTimer()),
          m_lockoutMinutes(mw->m_lockoutTimer.Minutes())
    {
        m_mainWindow->m_trayIcon.setContextMenu(NULL);
        m_mainWindow->m_canHide = false;
    }
    ~modal_dialog_helper_t(){
        m_mainWindow->m_trayIcon.setContextMenu(m_contextMenu);
        m_mainWindow->m_canHide = true;
        if(m_wasLockoutTimerStarted)
            m_mainWindow->m_lockoutTimer.StartLockoutTimer(m_lockoutMinutes);
    }
};

// Use this class to disable column auto-size when items are expanded/collapsed
class disable_column_auroresize_t
{
    GUtil::Qt::TreeView *m_treeView;
public:
    disable_column_auroresize_t(GUtil::Qt::TreeView *tv) :m_treeView(tv){
        QObject::disconnect(m_treeView, SIGNAL(expanded(QModelIndex)), m_treeView, SLOT(ResizeColumnsToContents()));
        QObject::disconnect(m_treeView, SIGNAL(collapsed(QModelIndex)), m_treeView, SLOT(ResizeColumnsToContents()));
    }
    ~disable_column_auroresize_t(){
        QObject::connect(m_treeView, SIGNAL(expanded(QModelIndex)), m_treeView, SLOT(ResizeColumnsToContents()),
                         ::Qt::QueuedConnection);
        QObject::connect(m_treeView, SIGNAL(collapsed(QModelIndex)), m_treeView, SLOT(ResizeColumnsToContents()),
                         ::Qt::QueuedConnection);
    }
};


MainWindow::MainWindow(GUtil::Qt::Settings *s, const QString &open_file, QWidget *parent)
    :QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_trayIcon(QIcon(":/grypto/icons/main.png"), this),
      m_fileLabel(new QLabel(this)),
      m_settings(s),
      m_add_remove_favorites(this),
      m_action_new_child(tr("New &Child"), this),
      m_btn_pop_out(tr("Pop Out")),
      m_isLocked(true),
      m_grypto_transforms_visible(false),
      m_minimize_msg_shown(false),
      m_canHide(true),
      m_readonly(false),
      m_progressBar(true)
#ifdef Q_OS_WIN
      ,m_taskbarButton(this)
#endif
{
    ui->setupUi(this);
    setWindowTitle(GRYPTO_APP_NAME);
    ui->action_About->setText(tr("&About " GRYPTO_APP_NAME));
    m_action_new_child.setShortcut(::Qt::ControlModifier | ::Qt::ShiftModifier | ::Qt::Key_N);

    ui->treeView->setModel(new FilteredDatabaseModel(this));
    ui->treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    connect(ui->treeView, SIGNAL(expanded(QModelIndex)), ui->treeView, SLOT(ResizeColumnsToContents()),
            ::Qt::QueuedConnection);
    connect(ui->treeView, SIGNAL(collapsed(QModelIndex)), ui->treeView, SLOT(ResizeColumnsToContents()),
            ::Qt::QueuedConnection);
    connect(ui->treeView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(_treeview_doubleclicked(QModelIndex)),
            ::Qt::QueuedConnection);
    _update_time_format();

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
    btn_navForward->setWhatsThis(tr("Navigate forwards through the history of your selections"));
    btn_navForward->setAutoRaise(true);
    connect(btn_navForward, SIGNAL(clicked()), &m_navStack, SLOT(redo()));
    ui->toolBar->addWidget(btn_navForward);

    m_btn_pop_out.setToolTip(tr("View entry in a smaller window"));
    m_btn_pop_out.setWhatsThis(tr("Opens the current entry in a smaller window, and"
                                  " launches a URL if there is one"));
    connect(&m_btn_pop_out, SIGNAL(pressed()), this, SLOT(PopOutCurrentEntry()));
    ui->toolBar->addWidget(&m_btn_pop_out);

    // Restore search parameters
    if(m_settings->Contains(MAINWINDOW_SEARCH_SETTING)){
        ui->searchWidget->SetFilter(
                    FilterInfo_t::FromXml(m_settings->Value(MAINWINDOW_SEARCH_SETTING).toString()));
    }
    else{
        // The FilterInfo struct defines the defaults
        ui->searchWidget->SetFilter(FilterInfo_t());
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
    connect(&m_action_new_child, SIGNAL(triggered()), this, SLOT(_new_child_entry()));
    connect(ui->action_Undo, SIGNAL(triggered()), this, SLOT(_undo()));
    connect(ui->action_Redo, SIGNAL(triggered()), this, SLOT(_redo()));
    connect(ui->action_Search, SIGNAL(triggered()), this, SLOT(_search()));
    connect(ui->actionLockUnlock, SIGNAL(triggered()), this, SLOT(_action_lock_unlock_interface()));
    connect(ui->action_grypto_transforms, SIGNAL(triggered()), this, SLOT(_cryptographic_transformations()));
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
    connect(ui->searchWidget, SIGNAL(NotifyUserActivity()),
            this, SLOT(_reset_lockout_timer()));

    connect(&m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(_reset_lockout_timer()));

    // Restore the previous session's window state
    if(m_settings->Contains(MAINWINDOW_GEOMETRY_SETTING))
        restoreGeometry(m_settings->Value(MAINWINDOW_GEOMETRY_SETTING).toByteArray());
    if(m_settings->Contains(MAINWINDOW_STATE_SETTING)){
        restoreState(m_settings->Value(MAINWINDOW_STATE_SETTING).toByteArray());

        // As a "safety" measure, make sure the treeview is shown, because
        //  if it's not then there's no way to show it (it happened to me already,
        //  so I'm paranoid about post-release bugs like that)
        if(ui->dw_treeView->isHidden())
            ui->dw_treeView->show();

        if(ui->toolBar->isHidden())
            ui->toolBar->show();
    }
    else{
        // Prepare the default interface if there are no settings

        // The search widget is pretty bulky, so let's hide it by default
        ui->dw_search->hide();
    }

    _update_ui_file_opened(false);
    _lock_unlock_interface(false);

    // Set up the recent files list and open the most recent
    _update_recent_files();

    // The window must be shown before setting up the taskbar button
    show();
#ifdef Q_OS_WIN
    m_taskbarButton.setWindow(this->windowHandle());
#endif // Q_OS_WIN

    if(!open_file.isEmpty()){
        _new_open_database(open_file);
    }
    else if(m_settings->Value(GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE).toBool()){
        QList<QAction *> al = ui->menu_Recent_Files->actions();
        if(al.length() > 0 && QFile::exists(al[0]->data().toString()))
            al[0]->trigger();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::AboutToQuit()
{
    m_minimize_msg_shown = true;
    _hide();

    // We'll tell the grypto-transforms window to quit, and wait for it after
    //  we've finished cleaning ourselves up
    m_grypto_transforms_visible = QProcess::Running == m_grypto_transforms.state();
    if(m_grypto_transforms_visible)
        m_grypto_transforms.write("quit\n");

    // Save the search settings before we close
    m_settings->SetValue(MAINWINDOW_SEARCH_SETTING, ui->searchWidget->GetFilter().ToXml());

    // Save the main window state so we can restore it next session
    m_settings->SetValue(MAINWINDOW_GEOMETRY_SETTING, saveGeometry());

    // If the main window was hidden, then restore the state prior to hiding
    if(!m_lockedState.isEmpty())
        m_settings->SetValue(MAINWINDOW_STATE_SETTING, m_lockedState);
    else if(!m_savedState.isEmpty())
        m_settings->SetValue(MAINWINDOW_STATE_SETTING, m_savedState);
    else
        m_settings->SetValue(MAINWINDOW_STATE_SETTING, saveState());

    m_settings->CommitChanges();

    _close_database();

    if(m_grypto_transforms_visible)
        m_grypto_transforms.waitForFinished();
}

void MainWindow::_hide()
{
    if(!m_canHide)
        return;

    if(isVisible()){
        // Only save the state if we were visible and unlocked
        if(!IsLocked())
            m_savedState = saveState();

        if(!m_minimize_msg_shown){
            m_trayIcon.showMessage(tr("Minimized to Tray"),
                                   tr(GRYPTO_APP_NAME" has been minimized to tray."
                                      "\nClick icon to reopen."));
            m_minimize_msg_shown = true;
        }
    }

    // Make doubly sure to hide these things, even if the main window was not visible
    hide();
    ui->dw_search->hide();
    ui->dw_treeView->hide();
    ui->toolBar->hide();
}

void MainWindow::_show()
{
    showNormal();
    activateWindow();
    if(!IsLocked() && !m_savedState.isEmpty()){
        restoreState(m_savedState);
        m_savedState.clear();
    }
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
        // Each one triggers an action, and the action keeps track
        //  of whether or not the action is available in the current
        //  application state (locked, file closed, etc...)
        if(ev->key() == ::Qt::Key_F){
            ui->action_Search->trigger();
            ret = true;
        }
        else if(ev->key() == ::Qt::Key_O){
            ui->action_Open->trigger();
            ret = true;
        }
        else if(ev->key() == ::Qt::Key_L){
            ui->actionLockUnlock->trigger();
            ret = true;
        }
        else if(ev->key() == ::Qt::Key_Escape){
            if(ev->modifiers().testFlag(::Qt::ShiftModifier))
                ui->actionQuit->trigger();
            else
                ui->action_Close->trigger();
            ret = true;
        }
        else if(ev->modifiers().testFlag(::Qt::ShiftModifier) && ev->key() == ::Qt::Key_S){
            ui->action_Save_As->trigger();
            ret = true;
        }
        else if(!IsReadOnly()){
            // Put all writing actions in this block to disable during readonly mode
            if(ev->key() == ::Qt::Key_Z)
            {
                ui->action_Undo->trigger();
                ret = true;
            }
            else if(ev->key() == ::Qt::Key_Y)
            {
                ui->action_Redo->trigger();
                ret = true;
            }
            else if(ev->key() == ::Qt::Key_N){
                if(ev->modifiers().testFlag(::Qt::ShiftModifier))
                    m_action_new_child.trigger();
                else
                    ui->actionNew_Entry->trigger();
                ret = true;
            }
            else if(ev->key() == ::Qt::Key_E){
                ui->action_EditEntry->trigger();
                ret = true;
            }
        }
    }
    else if(!ev->modifiers())
    {
        // No keyboard modifiers
        if(ev->key() == ::Qt::Key_Escape){
            // The escape key nulls out the selection, clears the views and search filter
            ui->searchWidget->Clear();
            ui->treeView->setCurrentIndex(QModelIndex());
            ui->treeView->collapseAll();
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
    if(Lockout::IsUserActivity(ev))
        _reset_lockout_timer();

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
                else if(!IsReadOnly()){
                    // Put all write actions in here, so they're safely disabled
                    //  while in readonly mode
                    if(kev->key() == ::Qt::Key_Delete){
                        _delete_entry();
                        ret = true;
                    }
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

            QAction *action_expand = new QAction(tr("E&xpand All"), this);
            QAction *action_collapse = new QAction(tr("Co&llapse All"), this);
            connect(action_expand, SIGNAL(triggered()), this, SLOT(_expand_all()));
            connect(action_collapse, SIGNAL(triggered()), this, SLOT(_collapse_all()));

            menu->addAction(ui->actionNew_Entry);
            menu->addAction(&m_action_new_child);
            menu->addAction(ui->action_EditEntry);
            menu->addAction(ui->action_DeleteEntry);
            menu->addSeparator();

            Entry const *e = _get_currently_selected_entry();
            if(e){
                m_add_remove_favorites.setData(e->IsFavorite());
                if(e->IsFavorite())
                    m_add_remove_favorites.setText(tr("&Remove from Favorites"));
                else
                    m_add_remove_favorites.setText(tr("&Add to Favorites"));
                menu->addAction(&m_add_remove_favorites);
                menu->addSeparator();
            }

            menu->addAction(action_expand);
            menu->addAction(action_collapse);
            menu->addSeparator();
            menu->addAction(ui->action_Search);

            menu->move(cm->globalPos());
            menu->show();
            ret = true;
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
    int history_len = 10;
    if(m_settings->Contains(SETTING_RECENT_FILES)){
        paths = m_settings->Value(SETTING_RECENT_FILES).toStringList();
        history_len = m_settings->Value(GRYPTONITE_SETTING_RECENT_FILES_LENGTH).toInt();
    }

    bool paths_changed = false;
    if(!latest_path.isEmpty()){
        QString path = QDir::fromNativeSeparators(latest_path);
        int ind = paths.indexOf(path);
        if(ind != -1)
        {
            // Move the path to the front
            paths.removeAt(ind);
        }
        paths.append(path);
        paths_changed = true;
    }

    // Trim the list to keep it within limits
    while(paths.length() > history_len){
        paths.removeAt(0);
        paths_changed = true;
    }

    if(paths_changed){
        m_settings->SetValue(SETTING_RECENT_FILES, paths);
        m_settings->CommitChanges();
    }

    _create_recent_files_menu(paths);
}

void MainWindow::_create_recent_files_menu(const QStringList &paths)
{
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

    // The model supports lazy-loading, but the data backend loads everything
    //  up front anyways
    dbm->FetchAllEntries();

    _get_proxy_model()->setSourceModel(dbm);
    _update_ui_file_opened(true);
    ui->treeView->ResizeColumnsToContents();

    if(m_settings->Contains(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT)){
        int val = m_settings->Value(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT).toInt();
        if(val > 0)
            m_lockoutTimer.StartLockoutTimer(val);
    }

    ui->view_entry->SetDatabaseModel(dbm);

    m_fileLabel->setText(dbm->FilePath());

    // Add this to the recent files list
    _update_recent_files(dbm->FilePath());

    _update_available_actions();

    // Wire up the progress bar control
    connect(&m_progressBar, SIGNAL(Clicked()), dbm, SLOT(CancelAllBackgroundOperations()));
    connect(dbm, SIGNAL(NotifyProgressUpdated(int, bool, QString)),
            this, SLOT(_progress_updated(int, bool, QString)));
}

void MainWindow::_new_open_database(const QString &path)
{
    Credentials creds;
    QString open_path = path;
    SmartPointer<DatabaseModel> dbm;
    ui->statusbar->showMessage(QString(tr("Opening %1...")).arg(path));
    {
        modal_dialog_helper_t mh(this);
        finally([&]{ ui->statusbar->clearMessage(); });

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
                // Call on the legacy manager to upgrade the database
                try{
                    open_path = LegacyManager().UpgradeDatabase(path, creds, m_settings,
                                                                [=](int p, const QString &ps){
                        this->_progress_updated(p, false, ps);
                    },
                    this);
                }
                catch(const AuthenticationException<> &ex){
                    QMessageBox::critical(this, tr("Authentication Failed"),
                                          QString(tr("Unable to upgrade legacy database because decryption failed"
                                                     " using the credentials you gave. Most likely you gave"
                                                     " the wrong password/keyfile, but it could be that the file"
                                                     " is invalid or corrupted.\n\n"
                                                     "Message from Crypto++:\n%1"))
                                                 .arg(ex.Message().data()));
                    return;
                }

                if(open_path.isNull())
                    return;
                else
                    file_updated = true;
            }
        }
        ui->statusbar->showMessage(QString(tr("Opening %1...")).arg(open_path));

        // Create a database model, which locks the database for us exclusively and prepares it to be opened
        try
        {
            DatabaseModel *tmp_dbm = new DatabaseModel(open_path.toUtf8(),

                                                       // A function to confirm overriding the lockfile
                                                       [&](const PasswordDatabase::ProcessInfo &pi){
                return QMessageBox::Yes ==
                        QMessageBox::warning(this,
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
            });

            dbm = tmp_dbm;
        }
        catch(const LockException<> &)
        {
            // Return silently, as this means that the user decided not to override the lock
            //  (other exceptions will escape, leading to the display of proper error messages)
            return;
        }

        // Get the user's credentials after successfully locking the database
        const QString filename = QFileInfo(open_path).fileName();
        if(!existed){
            modal_dialog_helper_t mh(this);
            NewPasswordDialog dlg(m_settings, filename, this);
            if(QDialog::Rejected == dlg.exec())
                return;
            creds = dlg.GetCredentials();
        }
        else if(!file_updated){
            modal_dialog_helper_t mh(this);
            GetPasswordDialog dlg(m_settings, filename, this);
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
    modal_dialog_helper_t mh(this);
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
    else{
        // Remove the path from the recent files list, because it doesn't exist
        QStringList paths = m_settings->Value(SETTING_RECENT_FILES).toStringList();
        paths.removeOne(path);
        _create_recent_files_menu(paths);
        m_settings->SetValue(SETTING_RECENT_FILES, paths);
        m_settings->CommitChanges();

        QMessageBox::warning(this, tr("File not found"), QString("The file does not exist: %1").arg(path));
    }
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
        ui->statusbar->showMessage(tr("Closing database..."));

        // Disconnect the database model
        QAbstractItemModel *old_model = _get_proxy_model()->sourceModel();
        _get_proxy_model()->setSourceModel(NULL);

        // Optionally delete it; maybe we still need it for something
        if(delete_model)
            delete old_model;

        m_readonly = false;
        setWindowTitle(GRYPTO_APP_NAME);
        _lock_unlock_interface(false);
        _update_ui_file_opened(false);
    }
}

bool MainWindow::_verify_credentials()
{
    bool ret = false;
    GetPasswordDialog dlg(m_settings,
                          QFileInfo(_get_database_model()->FilePath()).fileName(),
                          this);
    if(QDialog::Accepted == dlg.exec()){
        if(_get_database_model()->CheckCredentials(dlg.GetCredentials())){
            ret = true;
        }
        else{
            QMessageBox::information(this,
                                     tr("Password Incorrect"),
                                     tr("The password you entered is incorrect"));
        }
    }
    return ret;
}

void MainWindow::_save_as()
{
    if(!IsFileOpen())
        return;

    modal_dialog_helper_t mh(this);
    if(!_verify_credentials())
        return;

    QString fn = __get_new_database_filename(this, tr("Save as file"), true);
    if(fn.isEmpty())
        return;
    else if(QFileInfo(fn).absoluteFilePath() ==
            QFileInfo(_get_database_model()->FilePath()).absoluteFilePath())
        throw Exception<>("Cannot save to the same path as the original");

    // Create the database object, which only reserves a lock on the new database
    SmartPointer<DatabaseModel> dbm(new DatabaseModel(fn.toUtf8().constData()));

    NewPasswordDialog dlg(m_settings, fn, this);
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

    // Copy the old database to the new one
    dbm->ImportFromDatabase(*old_model);

    _install_new_database_model(dbm);

    dbm.Relinquish();
    delete old_model;
}

void MainWindow::_export_to_portable_safe()
{
    modal_dialog_helper_t mh(this);
    if(!_verify_credentials())
        return;

    QString fn = QFileDialog::getSaveFileName(this, tr("Export to Portable Safe"),
                                              QString(),
                                              "GUtil Portable Safe (*.gps)"
                                              ";;All Files(*)");
    if(fn.isEmpty())
        return;
    else if(QFileInfo(fn).suffix().isEmpty())
        fn.append(".gps");

    NewPasswordDialog dlg(m_settings, QFileInfo(fn).fileName(), this);
    if(QDialog::Accepted != dlg.exec())
        return;

    _get_database_model()->ExportToPortableSafe(fn.toUtf8(), dlg.GetCredentials());
}

void MainWindow::_import_from_portable_safe()
{
    if(!IsFileOpen())
        return;

    QString fn = QFileDialog::getOpenFileName(this, tr("Import from Portable Safe"),
                                              QString(),
                                              "GUtil Portable Safe (*.gps)"
                                              ";;All Files(*)");
    if(fn.isEmpty() || !QFile::exists(fn))
        return;

    GetPasswordDialog dlg(m_settings, QFileInfo(fn).fileName(), this);
    {
        modal_dialog_helper_t mh(this);
        if(QDialog::Accepted != dlg.exec())
            return;
    }

    _get_database_model()->ImportFromPortableSafe(fn.toUtf8(), dlg.GetCredentials());
}

void MainWindow::_new_entry()
{
    if(!IsFileOpen())
        return;

    modal_dialog_helper_t mh(this);

    DatabaseModel *model = _get_database_model();
    QModelIndex ind = _get_proxy_model()->mapToSource(ui->treeView->currentIndex());
    Entry const *selected = model->GetEntryFromIndex(ind);

    EntryEdit dlg(this);
    if(QDialog::Accepted == dlg.exec()){
        if(selected){
            dlg.GetEntry().SetParentId(selected->GetParentId());
            dlg.GetEntry().SetRow(selected->GetRow() + 1);
        }
        model->AddEntry(dlg.GetEntry());
        _select_entry(dlg.GetEntry().GetId());
        ui->treeView->ResizeColumnsToContents();
    }
}

void MainWindow::_new_child_entry()
{
    if(!IsFileOpen())
        return;

    modal_dialog_helper_t mh(this);

    DatabaseModel *model = _get_database_model();
    QModelIndex ind = _get_proxy_model()->mapToSource(ui->treeView->currentIndex());
    Entry const *selected = model->GetEntryFromIndex(ind);

    EntryEdit dlg(this);
    if(QDialog::Accepted == dlg.exec())
    {
        if(selected){
            dlg.GetEntry().SetParentId(selected->GetId());
            dlg.GetEntry().SetRow(model->rowCount(ind));
        }
        model->AddEntry(dlg.GetEntry());
        _select_entry(dlg.GetEntry().GetId());
    }
}

void MainWindow::_update_ui_file_opened(bool b)
{
    ui->treeView->setEnabled(b);
    ui->view_entry->setEnabled(b);
    ui->searchWidget->setEnabled(b);

    ui->treeView->setEnabled(b);
    ui->view_entry->setEnabled(b);
    ui->view_entry->SetEntry(Entry());

    // Even though the menu this belongs to is invisible, we still
    //  have to disable the action because of key shortcuts
    m_action_new_child.setEnabled(b);

    ui->actionNew_Entry->setEnabled(b);
    ui->action_EditEntry->setEnabled(b);
    ui->action_DeleteEntry->setEnabled(b);
    ui->action_Favorites->setEnabled(b);
    ui->action_Search->setEnabled(b);
    ui->actionLockUnlock->setEnabled(b);

    ui->action_Save_As->setEnabled(b);
    ui->action_Close->setEnabled(b);

    ui->menu_Export->setEnabled(b);
    ui->menu_Import->setEnabled(b);

    if(b){
        ui->statusbar->showMessage(tr("Database Opened Successfully"), STATUSBAR_MSG_TIMEOUT);
    }
    else
    {
        ui->statusbar->showMessage(tr("Closed"), STATUSBAR_MSG_TIMEOUT);

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
    // Select the entry first
    _select_entry(a->data().value<EntryId>());

    // Then pop it out
    PopOutCurrentEntry();
}

static QMenu *__create_menu(DatabaseModel *dbm, QActionGroup &ag, const QModelIndex &ind, QWidget *parent)
{
    Entry const *e = dbm->GetEntryFromIndex(ind);
    GASSERT(e);
    SmartPointer<QMenu> ret;

    if(e){
        ret = new QMenu(e->GetName(), parent);
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

    if(old_menu)
        old_menu->deleteLater();
}

void MainWindow::_update_undo_text()
{
    if(IsLocked()){
        ui->action_Undo->setText(tr("&Undo"));
        ui->action_Redo->setText(tr("&Redo"));
    }
    else{
        DatabaseModel *m = _get_database_model();
        ui->action_Undo->setEnabled(m && m->CanUndo());
        ui->action_Undo->setText(m && m->CanUndo() ?
                                     QString(tr("&Undo %1")).arg(m->UndoText().ConstData()) :
                                     tr("&Undo"));

        ui->action_Redo->setEnabled(m && m->CanRedo());
        ui->action_Redo->setText(m && m->CanRedo() ?
                                     QString(tr("&Redo %1")).arg(m->RedoText().ConstData()) :
                                     tr("&Redo"));
    }
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
    Entry const *e = _get_currently_selected_entry();
    if(e)
        _edit_entry(*e);
}

void MainWindow::_delete_entry()
{
    if(!IsFileOpen())
        return;

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
            ui->treeView->ResizeColumnsToContents();
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
    modal_dialog_helper_t mh(this);

    EntryEdit dlg(e, _get_database_model(), this);
    if(QDialog::Accepted == dlg.exec())
    {
        _get_database_model()->UpdateEntry(dlg.GetEntry());
        _nav_index_changed(m_navStack.index());
        ui->treeView->ResizeColumnsToContents();
    }
}

void MainWindow::_treeview_doubleclicked(const QModelIndex &ind)
{
    if(ind.isValid())
    {
        if(QApplication::keyboardModifiers().testFlag(::Qt::ShiftModifier)){
            _edit_entry();
        }
        else{
            PopOutCurrentEntry();
        }
    }
}

void MainWindow::_treeview_currentindex_changed(const QModelIndex &ind)
{
    Entry const *e = _get_database_model()->GetEntryFromIndex(_get_proxy_model()->mapToSource(ind));
    ui->action_EditEntry->setEnabled(!IsReadOnly() && e);
    ui->action_DeleteEntry->setEnabled(!IsReadOnly() && e);
    m_btn_pop_out.setEnabled(e);
    m_action_new_child.setEnabled(!IsReadOnly() && e);
    if(e)
        _select_entry(e->GetId());
    else
        ui->view_entry->SetEntry(Entry());
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

void MainWindow::_nav_index_changed(int nav_index)
{
    nav_index -= 1;
    bool success = false;
    if(nav_index >= 0){
        try{
            Entry e = _get_database_model()->FindEntryById(
                static_cast<const navigation_command *>(m_navStack.command(nav_index))->CurEntryId);

            ui->view_entry->SetEntry(e);

            QItemSelectionModel *ism = ui->treeView->selectionModel();
            QModelIndex ind = _get_proxy_model()->mapFromSource(_get_database_model()->FindIndexById(e.GetId()));
            if(ind.isValid()){
                ism->select(ind, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
                ui->treeView->setSelectionModel(ism);
                ui->treeView->ExpandToIndex(ind);
                ui->treeView->setCurrentIndex(ind);
            }
            success = true;
        } catch(...) {}
    }

    if(!success){
        ui->view_entry->SetEntry(Entry());
    }

    btn_navBack->setEnabled(m_navStack.canUndo());
    btn_navForward->setEnabled(m_navStack.canRedo());
    m_btn_pop_out.setEnabled(success);
}

void MainWindow::_select_entry(const EntryId &id)
{
    bool push = true;
    if(m_navStack.index() > 0)
    {
        push = id !=
                static_cast<const navigation_command *>(m_navStack.command(m_navStack.index() - 1))->CurEntryId;
    }

    if(push)
        m_navStack.push(new navigation_command(id));
    else{
        // Refresh the view even if the id was at the top of the nav stack
        //  (it could have been cleared from the view)
        ui->view_entry->SetEntry(_get_database_model()->FindEntryById(id));
    }
}

void MainWindow::ShowEntryById(const Grypt::EntryId &id)
{
    showNormal();
    activateWindow();
    _select_entry(id);
}

void MainWindow::PopOutCurrentEntry()
{
    Entry e = ui->view_entry->GetEntry();
    if(e.GetId().IsNull()){
        QMessageBox::information(this, tr("Select an Entry"),
                                 tr("You must first select an entry"));
        return;
    }

    // Open any URL fields with QDesktopServices
    bool autolaunch = m_settings->Value(GRYPTONITE_SETTING_AUTOLAUNCH_URLS).toBool();
    bool suppress_popup = false;

    // The ctrl key does the opposite
    if(QApplication::keyboardModifiers() == ::Qt::ControlModifier)
        autolaunch = !autolaunch;

    if(autolaunch){
        // Open all URL fields using the OS-registered URL handler
        int url_cnt = 0;
        for(const SecretValue &sv : e.Values()){
            if(sv.GetName().toLower() == "url"){
                url_cnt++;
                if(!QDesktopServices::openUrl(QUrl(sv.GetValue()))){
                    QMessageBox::warning(this, tr("Opening URL Failed"),
                                         QString(tr("Could not open URL: %1"))
                                         .arg(sv.GetValue()));
                }
            }
        }

        // If every value is a URL, then don't show the popup. This allows you to use
        //  the app as a simple URL launcher without using the popup
        if(url_cnt == e.Values().length())
            suppress_popup = true;
    }

    if(!suppress_popup){
        EntryPopup *ep = new EntryPopup(e, _get_database_model(), m_settings, this);
        connect(ep, SIGNAL(CedeControl()), this, SLOT(_show()));

        if(m_entryView)
            m_entryView->deleteLater();
        (m_entryView = ep)->show();
    }
    _hide();
}

void MainWindow::_filter_updated(const FilterInfo_t &fi)
{
    FilteredDatabaseModel *fm = _get_proxy_model();
    if(fm == NULL)
        return;

    disconnect(ui->treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
               this, SLOT(_treeview_currentindex_changed(QModelIndex)));

    // Apply the filter
    fm->SetFilter(fi);

    QString title = tr("Password Database");

    // Highlight and expand to show all matching rows
    if(fi.IsValid)
    {
        disable_column_auroresize_t d(ui->treeView);
        QItemSelection is;
        foreach(QModelIndex ind, fm->GetUnfilteredRows())
        {
            ui->treeView->ExpandToIndex(ind);
            is.append(QItemSelectionRange(ind, fm->index(ind.row(), fm->columnCount() - 1,
                                                         ind.parent())));
        }
        ui->treeView->selectionModel()->select(is, QItemSelectionModel::ClearAndSelect);

        if(fi.FilterResults)
            title.append(tr(" (filter applied)"));
    }
    else
    {
        ui->treeView->selectionModel()->clearSelection();
    }
    ui->treeView->ResizeColumnsToContents();
    ui->dw_treeView->setWindowTitle(title);

    connect(ui->treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(_treeview_currentindex_changed(QModelIndex)));
}

void MainWindow::_search()
{
    if(isMinimized() || isHidden())
        _show();

    ui->dw_treeView->activateWindow();
    ui->dw_search->show();
    if(ui->dw_search->isFloating())
        ui->dw_search->activateWindow();
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

        if(m_entryView)
            m_entryView->close();

        if(isVisible())
            m_lockedState = saveState();
        else{
            m_lockedState = m_savedState;
            m_savedState.clear();
        }

        ui->dw_treeView->hide();
        ui->dw_search->hide();
        ui->toolBar->hide();
        ui->stackedWidget->setCurrentIndex(0);
        ui->actionLockUnlock->setText(tr("&Unlock Application"));
        ui->actionLockUnlock->setData(false);
        m_isLocked = true;

        _hide();
        m_grypto_transforms_visible = false;
        if(QProcess::Running == m_grypto_transforms.state()){
            m_grypto_transforms.write("hide\n");
            m_grypto_transforms_visible = true;
        }
        m_trayIcon.setToolTip(tr(GRYPTO_APP_NAME " Locked"));
        m_trayIcon.showMessage(tr("Locked"),
                               tr(GRYPTO_APP_NAME " has been locked."
                                  "\nClick icon to unlock."));

        // Sometimes the user manually locks, so we have to be sure to disable
        //  the lockout timer
        m_lockoutTimer.StopLockoutTimer();
    }
    else
    {
        if(!IsLocked())
            return;

        if(m_grypto_transforms_visible)
            m_grypto_transforms.write("show\n");

        restoreState(m_lockedState);
        m_lockedState.clear();
        GASSERT(m_savedState.isEmpty());
        ui->stackedWidget->setCurrentIndex(1);
        ui->actionLockUnlock->setText(tr("&Lock Application"));
        ui->actionLockUnlock->setData(true);
        m_trayIcon.setToolTip(tr(GRYPTO_APP_NAME " Encrypted Secrets"));
        m_isLocked = false;

        // We have to start the lockout timer again, because it was stopped when we locked
        int minutes = m_settings->Value(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT).toInt();
        if(minutes > 0)
            m_lockoutTimer.StartLockoutTimer(minutes);
    }

    _update_available_actions();
    _update_undo_text();
}

void MainWindow::_update_available_actions()
{
    DatabaseModel *dbm = _get_database_model();

    bool enable_if_unlocked = !IsLocked() && IsFileOpen();
    bool enable_if_writable = enable_if_unlocked && !IsReadOnly();
    ui->action_Save_As->setEnabled(enable_if_unlocked);
    ui->menu_Export->setEnabled(enable_if_unlocked);
    ui->menu_Import->setEnabled(enable_if_writable);
    m_action_new_child.setEnabled(enable_if_writable);
    ui->actionNew_Entry->setEnabled(enable_if_writable);
    ui->action_EditEntry->setEnabled(enable_if_writable);
    ui->action_DeleteEntry->setEnabled(enable_if_writable);
    ui->action_Undo->setEnabled(dbm ? enable_if_writable && dbm->CanUndo() : false);
    ui->action_Redo->setEnabled(dbm ? enable_if_writable && dbm->CanRedo() : false);
    ui->action_Favorites->setEnabled(enable_if_writable);
    ui->action_Search->setEnabled(enable_if_unlocked);
    m_add_remove_favorites.setEnabled(enable_if_writable);
    m_action_new_child.setEnabled(enable_if_writable);

    ui->action_grypto_transforms->setEnabled(!IsLocked());
    ui->treeView->setDragEnabled(enable_if_writable);

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
    {
        bool unlock = false;
        modal_dialog_helper_t mh(this);
        GetPasswordDialog dlg(m_settings,
                              QFileInfo(_get_database_model()->FilePath()).fileName(),
                              this);
        if(QDialog::Accepted == dlg.exec()){
            if(_get_database_model()->CheckCredentials(dlg.GetCredentials()))
                unlock = true;
            else
                __show_access_denied(this, tr("Invalid Password"));
        }

        if(!unlock)
            return;
    }

    _lock_unlock_interface(false);
}

bool MainWindow::event(QEvent *e)
{
    if(Lockout::IsUserActivity(e))
        _reset_lockout_timer();
    return QMainWindow::event(e);
}

void MainWindow::_reset_lockout_timer()
{
    if(m_settings->Contains(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT))
        m_lockoutTimer.ResetLockoutTimer(m_settings->Value(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT).toInt());

    // For better debugging of this event, let's hear a beep whenever we reset the timer
    //QApplication::beep();
}

void MainWindow::_cryptographic_transformations()
{
    if(QProcess::Running == m_grypto_transforms.state()){
        m_grypto_transforms.write("activate\n");
    }
    else{
        QString path = QString("\"%1/%2\"")
                .arg(QApplication::applicationDirPath())
                .arg("grypto_transforms");
        m_grypto_transforms.start(path);
        if(!m_grypto_transforms.waitForStarted()){
            QMessageBox::critical(this, tr("Error"),
                                  QString(tr("Unable to start %1: %2")
                                          .arg(path)
                                          .arg(m_grypto_transforms.errorString())));
        }
    }
}

void MainWindow::_progress_updated(int progress, bool cancellable, const QString &task_name)
{
    m_progressBar.SetIsCancellable(cancellable);
    m_progressBar.SetProgress(progress, task_name);
#ifdef Q_OS_WIN
    m_taskbarButton.progress()->setValue(progress);
#endif // Q_OS_WIN

    if(progress == 100){
        ui->statusbar->showMessage(QString(tr("Finished %1")).arg(task_name), STATUSBAR_MSG_TIMEOUT);

        // Refresh the entry in the view, because maybe its status changed
        //  now that the task is complete
        Entry const *e = _get_currently_selected_entry();
        if(e)
            ui->view_entry->SetEntry(*e);

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
    if(!IsFileOpen())
        return;

    QList<EntryId> favorites;
    DatabaseModel *dbm = _get_database_model();
    {
        modal_dialog_helper_t mh(this);
        OrganizeFavoritesDialog dlg(dbm->FindFavorites(), this);
        if(QDialog::Accepted == dlg.exec())
            favorites = dlg.GetFavorites();
    }

    if(!favorites.isEmpty())
        dbm->SetFavoriteEntries(favorites);
}

void MainWindow::_update_time_format()
{
    if(_get_database_model())
        _get_database_model()->SetTimeFormat24Hours(
                    m_settings->Value(GRYPTONITE_SETTING_TIME_FORMAT_24HR).toBool());
}

void MainWindow::_edit_preferences()
{
    modal_dialog_helper_t mh(this);

    PreferencesEdit dlg(m_settings, this);
    if(QDialog::Accepted == dlg.exec()){
        // Update ourselves to reflect the new settings...
        _update_time_format();
        _update_recent_files();
    }
}

void MainWindow::_tray_icon_activated(QSystemTrayIcon::ActivationReason ar)
{
    switch(ar)
    {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:
        if(isVisible()){
            if(m_canHide)
                _hide();
            else
                activateWindow();
        }
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

void MainWindow::_expand_all()
{
    disable_column_auroresize_t d(ui->treeView);
    ui->treeView->expandAll();
    ui->treeView->ResizeColumnsToContents();
}

void MainWindow::_collapse_all()
{
    disable_column_auroresize_t d(ui->treeView);
    ui->treeView->collapseAll();
    ui->treeView->ResizeColumnsToContents();
}

void MainWindow::DropToReadOnly()
{
    if(!IsReadOnly()){
        setWindowTitle(tr(GRYPTO_APP_NAME " (Read Only)"));
        ui->statusbar->showMessage(tr("Now in Read-Only mode for your protection"));
        m_fileLabel->setText(m_fileLabel->text().append(" (Read Only)"));

        m_readonly = true;
        _update_available_actions();
    }
}
