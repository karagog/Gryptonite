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

#include "ui_entry_view.h"
#include "grypto_globals.h"
#include "entry_view.h"
#include "grypto_entrymodel.h"
#include <QCloseEvent>
#include <QClipboard>

NAMESPACE_GRYPTO;


EntryView::EntryView(const Entry &e, QWidget *parent)
    :QWidget(parent, Qt::Dialog | Qt::WindowStaysOnTopHint),
      ui(new Ui::EntryView),
      m_entry(e)
{
    ui->setupUi(this);
    setWindowTitle(e.GetName());

    ui->tableView->setModel(new EntryModel(e, NULL, this));

    // Set up the context menu
    QAction *action_copy = new QAction(tr("Copy to Clipboard"), this);
    connect(action_copy, SIGNAL(triggered()), this, SLOT(copy_to_clipboard()));
    ui->tableView->addAction(action_copy);
}

EntryView::~EntryView()
{
    delete ui;
}

void EntryView::copy_to_clipboard()
{

}


void EntryView::_show_in_main_window()
{
    close();
}


END_NAMESPACE_GRYPTO;
