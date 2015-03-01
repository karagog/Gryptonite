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

#ifndef DICEROLLER_H
#define DICEROLLER_H

#include <QWidget>

namespace Ui {
class DiceRoller;
}

class RollModel;

class DiceRoller : public QWidget
{
    Q_OBJECT
public:
    explicit DiceRoller(QWidget *parent = 0);
    ~DiceRoller();

private slots:
    void _roll();

private:
    Ui::DiceRoller *ui;
    RollModel *m_model;
    bool m_suppress;
};

#endif // DICEROLLER_H
