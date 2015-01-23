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
#include <grypto_common.h>
#include <grypto_lockout.h>

NAMESPACE_GRYPTO;


SearchWidget::SearchWidget(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::SearchWidget),
      m_suppressUpdates(false)
{
    ui->setupUi(this);
    Clear();

    // By default show dates for the past month
    QDateTime curdate = QDateTime::currentDateTime();
    ui->dte_start->setDateTime(curdate.addMonths(-1));
    ui->dte_end->setDateTime(curdate);

    ui->lineEdit->installEventFilter(this);
    ui->chk_caseSensitive->installEventFilter(this);
    ui->chk_filter_results->installEventFilter(this);
    ui->rdo_regexp->installEventFilter(this);
    ui->rdo_wildCard->installEventFilter(this);
    ui->chk_start->installEventFilter(this);
    ui->dte_start->installEventFilter(this);
    ui->chk_end->installEventFilter(this);
    ui->dte_end->installEventFilter(this);
    ui->btn_clear->installEventFilter(this);
}

SearchWidget::~SearchWidget()
{
    delete ui;
}

bool SearchWidget::eventFilter(QObject *, QEvent *ev)
{
    if(Lockout::IsUserActivity(ev))
        emit NotifyUserActivity();
    return false;
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
    ui->chk_onlyFavorites->setChecked(fi.ShowOnlyFavorites);
    ui->chk_onlyFiles->setChecked(fi.ShowOnlyFiles);
    ui->chk_alsoSecrets->setChecked(fi.AlsoSearchSecrets);
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
                     ui->chk_onlyFavorites->isChecked(),
                     ui->chk_onlyFiles->isChecked(),
                     ui->chk_alsoSecrets->isChecked(),
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

    emit FilterChanged(FilterInfo_t());
    m_suppressUpdates = false;
}

void SearchWidget::hideEvent(QHideEvent *ev)
{
    Clear();
    QWidget::hideEvent(ev);
}

void SearchWidget::_something_changed()
{
    if(m_suppressUpdates)
        return;

    emit FilterChanged(GetFilter());
}


END_NAMESPACE_GRYPTO;
