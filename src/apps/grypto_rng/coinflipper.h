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

#ifndef COINFLIPPER_H
#define COINFLIPPER_H

#include "coinmodel.h"
#include <QWidget>

namespace Ui {
class coinflipper;
}

class CoinModel;

class CoinFlipper :
        public QWidget
{
    Q_OBJECT
public:
    explicit CoinFlipper(QWidget *parent = 0);
    ~CoinFlipper();

private slots:
    void _flip();
    void _clear();

private:
    Ui::coinflipper *ui;
    CoinModel *m_model;

    void _update();
};

#endif // COINFLIPPER_H
