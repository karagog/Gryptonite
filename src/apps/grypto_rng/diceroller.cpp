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

DiceRoller::DiceRoller(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::DiceRoller),
      m_model(new RollModel(this)),
      m_suppress(false)
{
    ui->setupUi(this);
    ui->tbl_results->setModel(m_model);
}

DiceRoller::~DiceRoller()
{
    delete ui;
}

void DiceRoller::_min_updated(int val)
{
    if(!m_suppress && val >= ui->spn_max->value()){
        m_suppress = true;
        ui->spn_max->setValue(val + 1);
        m_suppress = false;
    }
}

void DiceRoller::_max_updated(int val)
{
    if(!m_suppress && val <= ui->spn_min->value()){
        m_suppress = true;
        ui->spn_min->setValue(val - 1);
        m_suppress = false;
    }
}

void DiceRoller::_roll()
{
    m_model->Roll(ui->spn_min->value(), ui->spn_max->value(), ui->spn_number->value());
    ui->lbl_lastRoll->setText(QDateTime::currentDateTime().toString("h:mm:ss.zzz"));
}
