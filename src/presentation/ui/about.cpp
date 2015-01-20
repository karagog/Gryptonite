/*Copyright 2010 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "about.h"
#include "GUtil/Utils/widgethelpers.h"
#include <QCloseEvent>
#include <QResizeEvent>
using namespace GUtil::Utils;

about_window::about_window(QApplication *app, QWidget *p)
    :QWidget(p, Qt::Window |
          Qt::CustomizeWindowHint |
          Qt::WindowTitleHint)
{
    widget.setupUi(this);

    setWindowModality(Qt::WindowModal);

    connect(widget.aboutQt, SIGNAL(clicked()), app, SLOT(aboutQt()));

    setFixedSize(530, 250);
}

about_window::~about_window(){}

void about_window::closeEvent(QCloseEvent *ev)
{
    ev->ignore();
    hide();
}

void about_window::showEvent(QShowEvent *ev)
{
    ev->accept();
    WidgetHelpers::CenterOverWidget(parentWidget(), this);
}
