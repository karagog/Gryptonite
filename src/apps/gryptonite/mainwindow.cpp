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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "grypto_globals.h"
#include "grypto_newpassworddialog.h"
#include "grypto_getpassworddialog.h"
#include "grypto_databasemodel.h"
#include "grypto_filtereddatabasemodel.h"
#include "grypto_entry_edit.h"
#include "grypto_entry_popup.h"
#include "grypto_cryptotransformswindow.h"
#include "grypto_cleanupfileswindow.h"
#include "gutil_qt_settings.h"
#include "gutil_widget.h"
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QActionGroup>
#include <QToolButton>
#include <QLabel>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL1(Qt);
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GRYPTO;
using namespace std;

#define STATUSBAR_MSG_TIMEOUT 5000
#define CLIPBOARD_TIMEOUT 30000
#define RECENT_FILE_LIMIT 5

#define SETTING_RECENT_FILES "recent_files"

static void __show_access_denied(QWidget *w, const QString &msg)
{
    QMessageBox::critical(w, QObject::tr("Access Denied"), msg);
}


MainWindow::MainWindow(GUtil::Qt::Settings *s, QWidget *parent)
    :QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_trayIcon(QIcon(":/icons/main.png")),
      m_progressBar(true),
      m_fileLabel(new QLabel(this)),
      m_settings(s),
      m_isLocked(true)
{
    ui->setupUi(this);
    setWindowTitle(GRYPTO_APP_NAME);

    _update_trayIcon_menu();
    m_trayIcon.show();

    ui->statusbar->addPermanentWidget(&m_progressBar, 1);
    ui->statusbar->addPermanentWidget(m_fileLabel);

    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Set up the toolbar
    btn_navBack = new QToolButton(this);
    btn_navBack->setIcon(QIcon(":/icons/leftarrow.png"));
    btn_navBack->setToolTip(tr("Navigate Backwards"));
    btn_navBack->setAutoRaise(true);
    connect(btn_navBack, SIGNAL(clicked()), &m_navStack, SLOT(undo()));
    ui->toolBar->addWidget(btn_navBack);

    btn_navForward = new QToolButton(this);
    btn_navForward->setIcon(QIcon(":/icons/rightarrow.png"));
    btn_navForward->setToolTip(tr("Navigate Forwards"));
    btn_navForward->setAutoRaise(true);
    connect(btn_navForward, SIGNAL(clicked()), &m_navStack, SLOT(redo()));
    ui->toolBar->addWidget(btn_navForward);

    connect(ui->actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->actionNewOpenDB, SIGNAL(triggered()), this, SLOT(_new_open_database()));
    connect(ui->action_export_ps, SIGNAL(triggered()), this, SLOT(_export_to_portable_safe()));
    connect(ui->action_import_ps, SIGNAL(triggered()), this, SLOT(_import_from_portable_safe()));
    connect(ui->action_Close, SIGNAL(triggered()), this, SLOT(_close_database()));
    connect(ui->actionNew_Entry, SIGNAL(triggered()), this, SLOT(_new_entry()));
    connect(ui->action_EditEntry, SIGNAL(triggered()), this, SLOT(_edit_entry()));
    connect(ui->action_DeleteEntry, SIGNAL(triggered()), this, SLOT(_delete_entry()));
    connect(ui->action_Undo, SIGNAL(triggered()), this, SLOT(_undo()));
    connect(ui->action_Redo, SIGNAL(triggered()), this, SLOT(_redo()));
    connect(ui->action_Search, SIGNAL(triggered()), this, SLOT(_search()));
    connect(ui->actionLockUnlock, SIGNAL(triggered()), this, SLOT(_action_lock_unlock_interface()));
    connect(ui->actionCleanup_Files, SIGNAL(triggered()), this, SLOT(_cleanup_files()));
    connect(ui->action_cryptoTransform, SIGNAL(triggered()), this, SLOT(_cryptographic_transformations()));

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
    QList<QAction *> al = ui->menu_Recent_Files->actions();
    if(al.length() > 0 && QFile::exists(al[0]->data().toString()))
        al[0]->trigger();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::AboutToQuit()
{
    // This restores the state of all dock widgets
    _close_database();

    // Save the main window state so we can restore it next session
    m_settings->SetValue(MAINWINDOW_GEOMETRY_SETTING, saveGeometry());
    m_settings->SetValue(MAINWINDOW_STATE_SETTING, saveState());
    m_settings->CommitChanges();
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    QMainWindow::closeEvent(ev);
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
            }
        }
    }
    else if(ev->type() == QEvent::ContextMenu)
    {
        QContextMenuEvent *cm = static_cast<QContextMenuEvent *>(ev);
        if(o == ui->treeView){
            ui->menuEntry->move(cm->globalPos());
            ui->menuEntry->show();
        }
    }
    return ret;
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

void MainWindow::_new_open_database(const QString &path)
{
    QByteArray password, keyfile;
    if(QFile::exists(path))
    {
        QFileInfo fi(path);
        GetPasswordDialog dlg(m_settings, fi.fileName(), this);
        if(QDialog::Rejected == dlg.exec())
            return;

        password = dlg.Password();
        keyfile = dlg.KeyFile();
    }
    else
    {
        NewPasswordDialog dlg(m_settings, this);
        if(QDialog::Rejected == dlg.exec())
            return;

        password = dlg.Password();
        keyfile = dlg.KeyFile();
    }

    _close_database();

    DatabaseModel *dbm = NULL;
    try{
        dbm = new DatabaseModel(path.toUtf8(), password, keyfile, this);
    }
    catch(const GUtil::AuthenticationException<> &){
        __show_access_denied(this, tr("Invalid Key"));
        return;
    }
    connect(dbm, SIGNAL(NotifyFavoritesUpdated()),
            this, SLOT(_update_trayIcon_menu()));

    FilteredDatabaseModel *fm = new FilteredDatabaseModel(this);
    fm->setSourceModel(dbm);
    ui->treeView->setModel(fm);
    _update_ui_file_opened(true);
    m_lockoutTimer.StartLockoutTimer(30);

    ui->view_entry->SetDatabaseModel(dbm);

    m_fileLabel->setText(path);

    // Add this to the recent files list
    _update_recent_files(path);

    // Wire up the progress bar control
    connect(&m_progressBar, SIGNAL(Clicked()), dbm, SLOT(CancelAllBackgroundOperations()));
    connect(dbm, SIGNAL(NotifyProgressUpdated(int,QString)),
            this, SLOT(_progress_updated(int,QString)));
}

void MainWindow::_new_open_database()
{
    QString path = QFileDialog::getSaveFileName(this, tr("File Location"),
                                                QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                "Grypto DB (*.gdb);;All Files (*)", 0, QFileDialog::DontConfirmOverwrite);
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
    return ui->treeView->model() != NULL;
}

void MainWindow::_close_database()
{
    if(IsFileOpen())
    {
        m_lockoutTimer.StopLockoutTimer();

        disconnect(ui->searchWidget, SIGNAL(FilterChanged(Grypt::FilterInfo_t)),
                   this, SLOT(_filter_updated(Grypt::FilterInfo_t)));

        // Delete the old database model
        QAbstractItemModel *old_model = ui->treeView->model();
        ui->treeView->setModel(0);
        delete old_model;

        _lock_unlock_interface(false);
        _update_ui_file_opened(false);
    }
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

    _get_database_model()->ExportToPortableSafe(fn.toUtf8(), dlg.Password(), dlg.KeyFile());
}

void MainWindow::_import_from_portable_safe()
{
    QString fn = QFileDialog::getOpenFileName(this, tr("Import from Portable Safe"),
                                              QString(),
                                              "GUtil Portable Safe (*.gps)"
                                              ";;All Files(*)");
    if(fn.isEmpty() || !QFile::exists(fn))
        return;

    GetPasswordDialog dlg(m_settings, fn, this);
    if(QDialog::Accepted != dlg.exec())
        return;

    _get_database_model()->ImportFromPortableSafe(fn.toUtf8(), dlg.Password(), dlg.KeyFile());
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

    ui->view_entry->SetEntry(Entry());

    ui->actionNew_Entry->setEnabled(b);
    ui->action_EditEntry->setEnabled(b);
    ui->action_DeleteEntry->setEnabled(b);
    ui->action_Search->setEnabled(b);
    ui->actionLockUnlock->setEnabled(b);
    ui->actionCleanup_Files->setEnabled(b);

    ui->action_Save_As->setEnabled(b);
    ui->action_Close->setEnabled(b);
    ui->menuEntry->setEnabled(b);
    ui->menu_Export->setEnabled(b);

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
}

void MainWindow::_favorite_action_clicked(QAction *a)
{
    m_entryView = new EntryPopup(_get_database_model()->FindEntryById(a->data().value<EntryId>()));
    m_entryView->show();
}

void MainWindow::_update_trayIcon_menu()
{
    QMenu *old_menu = m_trayIcon.contextMenu();
    m_trayIcon.setContextMenu(new QMenu(this));

    // Add the user's favorites to the list
    DatabaseModel *dbm = _get_database_model();
    if(dbm){
        vector<Entry> favs = dbm->FindFavorites();
        QActionGroup *ag = new QActionGroup(this);
        connect(ag, SIGNAL(triggered(QAction*)), this, SLOT(_favorite_action_clicked(QAction*)));
        for(const Entry &fav : favs)
            ag->addAction(fav.GetName())->setData(QVariant::fromValue(fav.GetId()));
        m_trayIcon.contextMenu()->addActions(ag->actions());
        m_trayIcon.contextMenu()->addSeparator();
    }

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
    FilteredDatabaseModel *pm = _get_proxy_model();
    return NULL == pm ? NULL : static_cast<DatabaseModel *>(pm->sourceModel());
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

void MainWindow::_entry_row_activated(int r)
{
    if(0 < r){
        const SecretValue &v = ui->view_entry->GetEntry().Values()[r];
        m_clipboard.SetText(v.GetValue(), v.GetIsHidden() ? CLIPBOARD_TIMEOUT : 0);
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
            success = true;
        } catch(...) {}
    }

    if(!success)
        ui->view_entry->SetEntry(Entry());

    btn_navBack->setEnabled(m_navStack.canUndo());
    btn_navForward->setEnabled(m_navStack.canRedo());
}

void MainWindow::_treeview_clicked(const QModelIndex &ind)
{
    if(ind.isValid())
    {
        Entry const *e =  _get_database_model()->GetEntryFromIndex(_get_proxy_model()->mapToSource(ind));
        bool allow = true;
        if(m_navStack.index() > 0)
        {
            allow = e->GetId() !=
                    static_cast<const navigation_command *>(m_navStack.command(m_navStack.index() - 1))->CurEntryId;
        }

        if(allow)
            m_navStack.push(new navigation_command(e->GetId()));
    }
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
    if(isMinimized()){
        showNormal();
        activateWindow();
    }

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
        ui->actionLockUnlock->setText(tr("&Unlock Interface"));
        ui->actionLockUnlock->setData(false);
        m_isLocked = true;
    }
    else
    {
        if(!IsLocked())
            return;

        restoreState(m_savedState);
        ui->stackedWidget->setCurrentIndex(1);
        ui->actionLockUnlock->setText(tr("&Lock Interface"));
        ui->actionLockUnlock->setData(true);
        m_isLocked = false;
    }

    ui->action_Save_As->setEnabled(!lock);
    ui->actionNew_Entry->setEnabled(!lock);
    ui->action_EditEntry->setEnabled(!lock);
    ui->action_DeleteEntry->setEnabled(!lock);
    ui->action_Undo->setEnabled(!lock);
    ui->action_Redo->setEnabled(!lock);
    ui->action_Search->setEnabled(!lock);
}

void MainWindow::_action_lock_unlock_interface()
{
    if(ui->actionLockUnlock->data().toBool())
        Lock();
    else
        RequestUnlock();
}

void MainWindow::RequestUnlock()
{
    GetPasswordDialog dlg(m_settings, _get_database_model()->FilePath(), this);
    if(QDialog::Accepted == dlg.exec())
    {
        if(_get_database_model()->CheckPassword(dlg.Password(), dlg.KeyFile()))
            _lock_unlock_interface(false);
        else
            __show_access_denied(this, tr("Invalid Password"));
    }
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
        m_lockoutTimer.ResetLockoutTimer(30);

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
}

void MainWindow::_cleanup_files()
{
    if(m_cleanupFilesWindow)
        m_cleanupFilesWindow = NULL;
    m_cleanupFilesWindow = new CleanupFilesWindow(_get_database_model(), m_settings, this);
    m_cleanupFilesWindow->show();
}

void MainWindow::_progress_updated(int progress, const QString &task_name)
{
    m_progressBar.SetProgress(progress, task_name);

    if(progress == 100)
        ui->statusbar->showMessage(QString(tr("Finished %1")).arg(task_name), STATUSBAR_MSG_TIMEOUT);
}
