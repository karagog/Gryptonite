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
#include <gutil/qt_settings.h>
#include <QWidget>
#include <QProgressDialog>

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
    
    void SaveParameters(GUtil::Qt::Settings *) const;
    void RestoreParameters(GUtil::Qt::Settings *);

private slots:
    void _flip();
    void _clear();
    void _progress_updated(int);

private:
    Ui::coinflipper *ui;
    CoinModel *m_model;
    QProgressDialog m_pd;

    void _update();
};

#endif // COINFLIPPER_H
