/*Copyright 2010 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "ui_entry_popup.h"
#include "grypto_common.h"
#include "entry_popup.h"
#include "grypto_entrymodel.h"
#include <QCloseEvent>
#include <QClipboard>

NAMESPACE_GRYPTO;


EntryPopup::EntryPopup(const Entry &e, QWidget *parent)
    :QWidget(parent, Qt::Dialog | Qt::WindowStaysOnTopHint),
      ui(new Ui::EntryPopup),
      m_entry(e)
{
    ui->setupUi(this);
    setWindowTitle(e.GetName());

    ui->tableView->setModel(new EntryModel(e, this));

    // Set up the context menu
    QAction *action_copy = new QAction(tr("Copy to Clipboard"), this);
    connect(action_copy, SIGNAL(triggered()), this, SLOT(copy_to_clipboard()));
    ui->tableView->addAction(action_copy);
}

EntryPopup::~EntryPopup()
{
    delete ui;
}

void EntryPopup::copy_to_clipboard()
{

}


void EntryPopup::_show_in_main_window()
{
    close();
}


END_NAMESPACE_GRYPTO;
