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

#include "entryview.h"
#include "ui_entryview.h"
#include <grypto_entrymodel.h>
#include <grypto_databasemodel.h>
#include <QKeyEvent>
#include <QFileDialog>

namespace Grypt{


EntryView::EntryView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EntryView),
    m_dbModel(NULL)
{
    ui->setupUi(this);

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->installEventFilter(this);
    ui->tableView->setModel(new EntryModel(this));
}

EntryView::~EntryView()
{
    delete ui;
}

void EntryView::SetDatabaseModel(DatabaseModel *dbm)
{
    m_dbModel = dbm;
}

void EntryView::SetEntry(const Entry &e)
{
    ui->lbl_name->setText(e.GetName());
    ui->lbl_description->setText(e.GetDescription());

    ui->btn_exportFile->setEnabled(!e.GetFileId().IsNull());
    ui->lbl_file->setEnabled(!e.GetFileId().IsNull());
    ui->lbl_fileStatus->setEnabled(!e.GetFileId().IsNull());
    ui->lbl_fileName->setEnabled(!e.GetFileId().IsNull());

    if(e.GetFileId().IsNull() || NULL == m_dbModel){
        ui->lbl_fileStatus->clear();
        ui->lbl_fileName->clear();
    }
    else
    {
        ui->lbl_fileName->setText(e.GetFileName());
        if(m_dbModel->FileExists(e.GetFileId()))
            ui->lbl_fileStatus->setText(tr("(Uploaded)"));
        else
            ui->lbl_fileStatus->setText(tr("(Missing)"));
    }

    _get_entry_model()->SetEntry(e);
    m_entry = e;
}

EntryModel *EntryView::_get_entry_model() const
{
    return static_cast<EntryModel *>(ui->tableView->model());
}

bool EntryView::eventFilter(QObject *, QEvent *ev)
{
    bool ret = false;
    if(ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *kev = static_cast<QKeyEvent *>(ev);

        if(kev->key() == ::Qt::Key_Enter || kev->key() == ::Qt::Key_Return){
            emit RowActivated(ui->tableView->currentIndex().row());
            ret = true;
        }
    }
    return ret;
}

void EntryView::_export_file()
{
    GASSERT(NULL != m_dbModel);

    QString file_path = QFileDialog::getSaveFileName(this, tr("Export File"), m_entry.GetFileName());
    if(!file_path.isEmpty())
        m_dbModel->ExportFile(m_entry.GetFileId(), file_path.toUtf8());
}


}
