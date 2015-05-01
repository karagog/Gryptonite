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

#include "noteseditdialog.h"
#include "ui_noteseditdialog.h"
#include <grypto/common.h>

NAMESPACE_GRYPTO;


NotesEditDialog::NotesEditDialog(const QString &txt, QWidget *parent)
    :QDialog(parent),
      ui(new Ui::NotesEditDialog)
{
    ui->setupUi(this);
    ui->textEdit->setPlainText(txt);
}

NotesEditDialog::~NotesEditDialog()
{
    delete ui;
}

void NotesEditDialog::accept()
{
    notes = ui->textEdit->toPlainText();
    QDialog::accept();
}

END_NAMESPACE_GRYPTO;
