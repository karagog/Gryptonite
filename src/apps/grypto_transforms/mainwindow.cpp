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
#include <grypto_cryptotransformswindow.h>
#include <QWhatsThis>
USING_NAMESPACE_GRYPTO;

#define SETTING_LAST_STATE      "mw_last_state"
#define SETTING_LAST_GEOMETRY   "mw_last_geometry"


MainWindow::MainWindow(GUtil::Qt::Settings *settings, QWidget *parent)
    :QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_settings(settings)
{
    ui->setupUi(this);
    setWindowTitle(GRYPTO_TRANSFORMS_APP_NAME);

    if(m_settings->Contains(SETTING_LAST_STATE)){
        restoreGeometry(m_settings->Value(SETTING_LAST_GEOMETRY).toByteArray());
        restoreState(m_settings->Value(SETTING_LAST_STATE).toByteArray());
    }

    CryptoTransformsWindow *ctw = new CryptoTransformsWindow(m_settings, this);
    setCentralWidget(ctw);
    ctw->show();

    ui->menu_Help->insertAction(ui->action_About, QWhatsThis::createAction(this));
    ui->menu_Help->insertSeparator(ui->action_About);

    connect(ui->action_Quit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->action_About, SIGNAL(triggered()), this, SLOT(_show_about()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    m_settings->SetValue(SETTING_LAST_STATE, saveState());
    m_settings->SetValue(SETTING_LAST_GEOMETRY, saveGeometry());
    m_settings->CommitChanges();
    QMainWindow::closeEvent(ev);
}

void MainWindow::_show_about()
{
    (new About(this))->ShowAbout();
}
