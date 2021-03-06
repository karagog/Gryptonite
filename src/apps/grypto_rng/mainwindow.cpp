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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "about.h"
#include <gutil/application.h>
#include <QWhatsThis>

#define SETTING_LAST_GEOMETRY   "mw_geometry"
#define SETTING_LAST_STATE      "mw_state"

MainWindow::MainWindow(GUtil::Qt::Settings *settings, QWidget *parent)
    :QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_settings(settings)
{
    ui->setupUi(this);
    setWindowTitle(GRYPTO_RNG_APP_NAME);
    if(settings->Contains(SETTING_LAST_GEOMETRY)){
        restoreGeometry(settings->Value(SETTING_LAST_GEOMETRY).toByteArray());
        restoreState(settings->Value(SETTING_LAST_STATE).toByteArray());
        
        ui->diceRoller->RestoreParameters(settings);
        ui->coinFlipper->RestoreParameters(settings);
        ui->dataGenerator->RestoreParameters(settings);
    }

    ui->menu_Help->insertAction(ui->action_About, QWhatsThis::createAction(this));
    ui->menu_Help->insertSeparator(ui->action_About);

    connect(ui->action_About, SIGNAL(triggered()), gApp, SLOT(About()));
    connect(ui->action_Quit, SIGNAL(triggered()), gApp, SLOT(Quit()));
    connect(ui->action_CoinFlipper, SIGNAL(triggered()), this, SLOT(_coin_tosser()));
    connect(ui->action_DataGenerator, SIGNAL(triggered()), this, SLOT(_data_generator()));
    
    connect(ui->dataGenerator, SIGNAL(NotifyInfo(QString)), ui->statusBar, SLOT(showMessage(QString)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::AboutToQuit()
{
    m_settings->SetValue(SETTING_LAST_GEOMETRY, saveGeometry());
    m_settings->SetValue(SETTING_LAST_STATE, saveState());
    
    // Save the last-used parameters for next session
    ui->diceRoller->SaveParameters(m_settings);
    ui->coinFlipper->SaveParameters(m_settings);
    ui->dataGenerator->SaveParameters(m_settings);
    
    m_settings->CommitChanges();
}

void MainWindow::_coin_tosser()
{
    ui->dw_coinFlipper->show();
    ui->dw_coinFlipper->activateWindow();
    ui->dw_coinFlipper->setFocus();
}

void MainWindow::_data_generator()
{
    ui->dw_dataGenerator->show();
    ui->dw_dataGenerator->activateWindow();
    ui->dw_dataGenerator->setFocus();
}
