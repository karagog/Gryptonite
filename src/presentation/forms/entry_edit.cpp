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

#include "entry_edit.h"
#include "ui_entry_edit.h"
#include "grypto_secretlabeldelegate.h"
#include "grypto_entrymodel.h"
#include "grypto_databasemodel.h"
#include "generatepassworddialog.h"
#include "noteseditdialog.h"
#include <QFileDialog>

NAMESPACE_GRYPTO;


EntryEdit::EntryEdit(DatabaseModel *m, QWidget *parent)
    :QDialog(parent),
      m_dbModel(m)
{
    _init(m);
}

EntryEdit::EntryEdit(const Entry &e, DatabaseModel *m, QWidget *parent)
    :QDialog(parent),
      m_dbModel(m),
      m_entry(e)
{
    _init(m);
    ui->labelEdit->setText(e.GetName());
    ui->descriptionEdit->setText(e.GetDescription());
    ui->fav_btn->setChecked(e.IsFavorite());

    if(!e.GetFileId().IsNull()){
        if(m->FileExists(e.GetFileId()))
            ui->lbl_fileStatus->setText(tr("Uploaded"));
        else
            ui->lbl_fileStatus->setText(tr("Missing"));
    }
}

void EntryEdit::_init(DatabaseModel *mdl)
{
    (ui = new Ui::EntryEdit)->setupUi(this);
    ui->tableView->setModel(new EntryModel(m_entry, mdl, this));
    ui->tableView->setItemDelegateForColumn(0, new SecretLabelDelegate(this));

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

EntryEdit::~EntryEdit()
{
    delete ui;
}

void EntryEdit::accept()
{
    m_entry.SetName(ui->labelEdit->text().trimmed());
    m_entry.SetDescription(ui->descriptionEdit->toPlainText());
    m_entry.SetModifyDate(QDateTime::currentDateTime());

    if(m_entry.IsFavorite())
    {
        if(!ui->fav_btn->isChecked())
            m_entry.SetFavoriteIndex(-1);
    }
    else if(ui->fav_btn->isChecked())
    {
        m_entry.SetFavoriteIndex(0);
    }

    EntryModel *em = static_cast<EntryModel *>(ui->tableView->model());
    m_entry.Values() = em->GetEntry().Values();

    QDialog::accept();
}

void EntryEdit::_add_secret()
{
    QAbstractItemModel *model = ui->tableView->model();
    QModelIndex ind = ui->tableView->currentIndex();
    int row;
    if(ind.isValid())
        row = ind.row() + 1;
    else
        row = model->rowCount();
    if(!model->insertRow(row))
        return;

    QStringList sl;
    for(uint i = 0; i < sizeof(SecretLabelDelegate::DefaultStrings)/sizeof(const char *); ++i)
        sl.append(SecretLabelDelegate::DefaultStrings[i]);

    QString s = sl[0];
    if(row > 0){
        int idx = sl.indexOf(model->index(row - 1, 0).data(Qt::EditRole).toString());
        if(idx != -1 && idx < (sl.length() - 1))
            s = sl[idx + 1];
    }

    QModelIndex new_ind = model->index(row, 0);
    model->setData(new_ind, s, Qt::EditRole);
    ui->tableView->setCurrentIndex(new_ind);
    ui->tableView->edit(new_ind);
}

void EntryEdit::_del_secret()
{
    QModelIndex ind = ui->tableView->currentIndex();
    if(!ind.isValid())
        return;

    int row = ind.row();
    QAbstractItemModel *model = ui->tableView->model();
    model->removeRow(row);
    if(row >= model->rowCount())
        row = model->rowCount() - 1;
    ui->tableView->setCurrentIndex(model->index(row, 0));
}

void EntryEdit::_toggle_secret()
{
    QModelIndex ind = ui->tableView->currentIndex();
    if(!ind.isValid())
        return;

    QAbstractItemModel *m = ui->tableView->model();
    m->setData(ind, !m->data(ind, EntryModel::IDSecret).toBool(), EntryModel::IDSecret);
}

void EntryEdit::_generate_value()
{
    QModelIndex ind = ui->tableView->currentIndex();
    if(!ind.isValid())
        return;

    GeneratePasswordDialog dlg(this);
    if(QDialog::Accepted == dlg.exec() && !dlg.Password().isEmpty())
    {
        QAbstractItemModel *m = ui->tableView->model();
        m->setData(m->index(ind.row(), 1, ind.parent()), dlg.Password(), Qt::EditRole);
    }
}

void EntryEdit::_edit_notes()
{
    QModelIndex ind = ui->tableView->currentIndex();
    if(!ind.isValid())
        return;

    NotesEditDialog dlg(ind.data(EntryModel::IDNotes).toString(), this);
    if(QDialog::Accepted == dlg.exec())
    {
        QAbstractItemModel *m = ui->tableView->model();
        m->setData(ind, dlg.Notes(), EntryModel::IDNotes);
    }
}

void EntryEdit::_move_up()
{
    QModelIndex ind = ui->tableView->currentIndex();
    if(!ind.isValid() || ind.row() <= 0)
        return;
    ((EntryModel *)ui->tableView->model())->SwapRows(ind.row(), ind.row() - 1);
    ui->tableView->selectRow(ind.row() - 1);
}

void EntryEdit::_move_down()
{
    QModelIndex ind = ui->tableView->currentIndex();
    if(!ind.isValid() || ind.row() >= (ui->tableView->model()->rowCount() - 1))
        return;
    ((EntryModel *)ui->tableView->model())->SwapRows(ind.row(), ind.row() + 1);
    ui->tableView->selectRow(ind.row() + 1);
}

void EntryEdit::_select_file()
{
    QModelIndex ind = ui->tableView->currentIndex();
    ind = ui->tableView->model()->index(ind.row(), 1);
    ui->tableView->closePersistentEditor(ind);

    QString fn = QFileDialog::getOpenFileName(this, tr("Select file to add to database"));
    if(fn.isEmpty())
        return;

    QFileInfo fi(fn);

    // Remember the original filename in the notes
    QAbstractItemModel *mdl = ui->tableView->model();
    mdl->insertRow(0);
    mdl->setData(mdl->index(0, 0), tr("Filename"));
    mdl->setData(mdl->index(0, 1), fi.fileName());

    ui->lbl_fileStatus->setText(fi.absolutePath());
}


END_NAMESPACE_GRYPTO;
