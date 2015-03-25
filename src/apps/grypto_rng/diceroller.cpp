/*Copyright 2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "diceroller.h"
#include "ui_diceroller.h"
#include "rollmodel.h"
#include <QDateTime>
#include <QSortFilterProxyModel>
USING_NAMESPACE_GUTIL;

DiceRoller::DiceRoller(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::DiceRoller),
      m_model(new RollModel(this)),
      m_suppress(false)
{
    ui->setupUi(this);

    QSortFilterProxyModel *fpm = new QSortFilterProxyModel(this);
    fpm->setSourceModel(m_model);
    ui->tbl_results->setModel(fpm);
}

DiceRoller::~DiceRoller()
{
    delete ui;
}

void DiceRoller::_roll()
{
    int min = ui->spn_min->value();
    int max = ui->spn_max->value();
    if(min >= max)
        throw Exception<>("Invalid range: The minimum must be less than the maximum");

    m_model->Roll(min, max, ui->spn_number->value());
    ui->lbl_lastRoll->setText(QDateTime::currentDateTime().toString("h:mm:ss.zzz"));
    ui->lbl_total->setText(QString("%1").arg(m_model->Total()));
    ui->lbl_range->setText(QString("[%1, %2]").arg(m_model->Min()).arg(m_model->Max()));
    ui->lbl_mean->setText(QString("%1").arg(m_model->Mean()));
    ui->lbl_median->setText(QString("%1").arg(m_model->Median()));
    
    QString mode_string;
    if(m_model->Mode().size()){
        int tmp = m_model->Mode().size() - 1;
        for(int m : m_model->Mode())
            mode_string.append(QString("%1%2 ").arg(m).arg(tmp-- != 0 ? "," : ""));
        mode_string.append(QString("(x%1)").arg(m_model->ModeCount()));
    }
    else{
        mode_string.append(tr("(none)"));
    }
    ui->lbl_mode->setText(mode_string);
}
