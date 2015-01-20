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

#include "searchwidget.h"
#include "ui_searchwidget.h"
#include "grypto_globals.h"

NAMESPACE_GRYPTO;


SearchWidget::SearchWidget(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::SearchWidget)
{
    ui->setupUi(this);
    setFocusProxy(ui->lineEdit);
    Clear();
}

SearchWidget::~SearchWidget()
{
    delete ui;
}

void SearchWidget::focusInEvent(QFocusEvent *ev)
{
    QWidget::focusInEvent(ev);
    ui->lineEdit->setFocus();
}

void SearchWidget::Clear()
{
    ui->lineEdit->clear();
    ui->gb_time->setChecked(false);
    ui->chk_start->setChecked(false);
    ui->chk_end->setChecked(false);
    ui->dte_start->setDateTime(QDateTime::currentDateTime());
    ui->dte_end->setDateTime(QDateTime::currentDateTime());
}

void SearchWidget::_something_changed()
{
    FilterInfo_t fi(ui->lineEdit->text().trimmed(),
                    !ui->chk_caseSensitive->isChecked(),
                    ui->rdo_wildCard->isChecked() ?
                                    FilterInfo_t::Wildcard :
                                    FilterInfo_t::RegExp);

    if(ui->gb_time->isChecked())
    {
        if(ui->chk_start->isChecked())
            fi.StartTime = ui->dte_start->dateTime();
        if(ui->chk_end->isChecked())
            fi.EndTime = ui->dte_end->dateTime();
    }

    emit FilterChanged(fi);
}

void SearchWidget::_clear_filter()
{
    emit FilterChanged(FilterInfo_t());
}


END_NAMESPACE_GRYPTO;
