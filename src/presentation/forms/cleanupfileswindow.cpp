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

#include <grypto_common.h>
#include "cleanupfileswindow.h"
#include "ui_cleanupfileswindow.h"
#include <grypto_databasemodel.h>
#include <gutil/qt_settings.h>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFileDialog>
USING_NAMESPACE_GUTIL1(Qt);
using namespace std;

#define SETTING_LAST_GEOMETRY "cfw_last_geometry"

NAMESPACE_GRYPTO;


CleanupFilesWindow::CleanupFilesWindow(DatabaseModel *m, Settings *settings, QWidget *parent) :
    QWidget(parent, Qt::Window),
    ui(new Ui::CleanupFilesWindow),
    m_settings(settings),
    m_dbModel(m)
{
    ui->setupUi(this);
    if(settings && settings->Contains(SETTING_LAST_GEOMETRY))
        restoreGeometry(settings->Value(SETTING_LAST_GEOMETRY).toByteArray());

    ui->listWidget->installEventFilter(this);
    connect(ui->listWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(_export_item(QListWidgetItem*)));

    QHash<FileId, PasswordDatabase::FileInfo_t> files( m->GetOrphanedFiles() );
    for(const FileId &k : files.keys())
    {
        const int size = files[k].Size;
        QString txt;
        if(size >= 1000000)
            txt = QString("%1 MB").arg((double)size/1000000, 0, 'f', 2);
        else if(size >= 1000)
            txt = QString("%1 KB").arg((double)size/1000, 0, 'f', 2);
        else
            txt = QString("%1 B").arg(size);

        (new QListWidgetItem(txt, ui->listWidget))
            ->setData(Qt::UserRole, QVariant::fromValue<FileId>(k));
    }
    ui->lbl_status->setText(QString(tr("Found %1 orphan files:").arg(files.count())));

    if(files.count() == 0)
        ui->pushButton->setEnabled(false);
}

CleanupFilesWindow::~CleanupFilesWindow()
{
    delete ui;
}

void CleanupFilesWindow::closeEvent(QCloseEvent *ev)
{
    if(m_settings){
        m_settings->SetValue(SETTING_LAST_GEOMETRY, saveGeometry());
        m_settings->CommitChanges();
    }
    QWidget::closeEvent(ev);
}

bool CleanupFilesWindow::eventFilter(QObject *o, QEvent *e)
{
    bool res = false;
    if(o == ui->listWidget){
        if(e->type() == QEvent::ContextMenu){
            QContextMenuEvent *cme = static_cast<QContextMenuEvent *>(e);
            QMenu *menu = new QMenu(this);
            menu->addAction(tr("&Export"), this, SLOT(_export_item()));
            menu->move(cme->globalPos());
            menu->show();
        }
    }
    return res;
}

void CleanupFilesWindow::_remove_all()
{
    int res = QMessageBox::warning(this, tr("Are you sure?"),
                                   tr("Are you sure you want to remove all the files from the database?"),
                                   QMessageBox::No, QMessageBox::Yes);
    if(res == QMessageBox::No)
        return;

    close();

    for(int i = 0; i < ui->listWidget->count(); ++i)
        m_dbModel->DeleteFile(ui->listWidget->item(i)->data(Qt::UserRole).value<FileId>());
}

void CleanupFilesWindow::_export_item(QListWidgetItem *lwi)
{
    if(lwi == NULL)
        lwi = ui->listWidget->currentItem();

    if(lwi){
        QString fn = QFileDialog::getSaveFileName(this, tr("Choose Export File Name"));
        if(!fn.isEmpty()){
            m_dbModel->ExportFile(lwi->data(Qt::UserRole).value<FileId>(), fn.toUtf8());
        }
    }
}


END_NAMESPACE_GRYPTO;
