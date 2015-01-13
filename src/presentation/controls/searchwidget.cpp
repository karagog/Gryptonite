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
#include "grypto_common.h"

NAMESPACE_GRYPTO;


SearchWidget::SearchWidget(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::SearchWidget),
      m_suppressUpdates(false)
{
    ui->setupUi(this);
    //setFocusProxy(ui->lineEdit);
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
    ui->lineEdit->selectAll();
}

void SearchWidget::SetFilter(const FilterInfo_t &fi)
{
    ui->lineEdit->setText(fi.SearchString);
    ui->chk_filter_results->setChecked(fi.FilterResults);
    ui->chk_caseSensitive->setChecked(!fi.IgnoreCase);
    ui->chk_start->setChecked(!fi.StartTime.isNull());
    ui->chk_end->setChecked(!fi.EndTime.isNull());
    ui->dte_start->setDateTime(fi.StartTime);
    ui->dte_end->setDateTime(fi.EndTime);
    ui->gb_time->setChecked(!fi.StartTime.isNull() || !fi.EndTime.isNull());
}

FilterInfo_t SearchWidget::GetFilter() const
{
    FilterInfo_t ret(ui->lineEdit->text().trimmed(),
                    ui->chk_filter_results->isChecked(),
                    !ui->chk_caseSensitive->isChecked(),
                    ui->rdo_wildCard->isChecked() ?
                                    FilterInfo_t::Wildcard :
                                    FilterInfo_t::RegExp);

    if(ui->gb_time->isChecked())
    {
        if(ui->chk_start->isChecked())
            ret.StartTime = ui->dte_start->dateTime();
        if(ui->chk_end->isChecked())
            ret.EndTime = ui->dte_end->dateTime();
    }
    return ret;
}

void SearchWidget::Clear()
{
    m_suppressUpdates = true;
    ui->lineEdit->clear();
    ui->gb_time->setChecked(false);
    ui->chk_start->setChecked(false);
    ui->chk_end->setChecked(false);
    ui->dte_start->setDateTime(QDateTime::currentDateTime());
    ui->dte_end->setDateTime(QDateTime::currentDateTime());

    emit FilterChanged(FilterInfo_t());
    m_suppressUpdates = false;
}

void SearchWidget::_something_changed()
{
    if(m_suppressUpdates)
        return;

    emit FilterChanged(GetFilter());
}


END_NAMESPACE_GRYPTO;
