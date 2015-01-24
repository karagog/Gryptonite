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

#ifndef _PREF_WINDOW_H
#define	_PREF_WINDOW_H

#include "ui_preferences_edit.h"
#include <gutil/qt_settings.h>


class PreferencesEdit : public QDialog
{
    Q_OBJECT
    Ui::PreferencesEdit ui;
    GUtil::Qt::Settings *m_settings;
public:

    PreferencesEdit(GUtil::Qt::Settings *, QWidget *p = 0);
    virtual void accept();

private slots:

    void _lockout_slider_changed(int);
    void _clipboard_slider_changed(int);
    void _recent_file_history_changed(int);

};


#endif	/* _PREF_WINDOW_H */

