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

#include "searchwidget.h"
#include "ui_searchwidget.h"
#include <grypto/common.h>
#include <grypto/lockout.h>
#include <QKeyEvent>

NAMESPACE_GRYPTO;


SearchWidget::SearchWidget(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::SearchWidget),
      m_suppressUpdates(false)
{
    ui->setupUi(this);
    Clear();

    // By default show dates for the past month
    QDate curdate = QDate::currentDate();
    ui->de_start->setDate(curdate.addMonths(-1));
    ui->de_end->setDate(curdate);

    ui->lineEdit->installEventFilter(this);
    ui->chk_caseSensitive->installEventFilter(this);
    ui->chk_filter_results->installEventFilter(this);
    ui->chk_onlyFavorites->installEventFilter(this);
    ui->chk_onlyFiles->installEventFilter(this);
    ui->chk_alsoSecrets->installEventFilter(this);
    ui->rdo_regexp->installEventFilter(this);
    ui->rdo_wildCard->installEventFilter(this);
    ui->chk_start->installEventFilter(this);
    ui->de_start->installEventFilter(this);
    ui->chk_end->installEventFilter(this);
    ui->de_end->installEventFilter(this);
    ui->btn_clear->installEventFilter(this);
    ui->gb_time->installEventFilter(this);
}

SearchWidget::~SearchWidget()
{
    delete ui;
}

bool SearchWidget::eventFilter(QObject *, QEvent *ev)
{
    if(Lockout::IsUserActivity(ev))
        emit NotifyUserActivity();

    bool ret = false;
    if(ev->type() == QEvent::KeyPress){
        QKeyEvent *kev = (QKeyEvent*)ev;
        if(::Qt::Key_Escape == kev->key())
            Clear();
    }
    return ret;
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
    ui->de_start->setDateTime(fi.StartTime);
    ui->de_end->setDateTime(fi.EndTime);
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
            ret.StartTime = ui->de_start->dateTime();
        if(ui->chk_end->isChecked())
            ret.EndTime = ui->de_end->dateTime();
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
    //Clear();
    QWidget::hideEvent(ev);
}

void SearchWidget::_something_changed()
{
    if(m_suppressUpdates)
        return;

    emit FilterChanged(GetFilter());
}


END_NAMESPACE_GRYPTO;
