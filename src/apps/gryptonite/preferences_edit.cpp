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

#include "preferences_edit.h"
#include "settings.h"
#include <QCloseEvent>
#include <QStringList>
USING_NAMESPACE_GUTIL1(Qt);

PreferencesEdit::PreferencesEdit(Settings *s, QWidget *p)
    :QDialog(p),
      m_settings(s)
{
    ui.setupUi(this);
    bool settings_saved = m_settings->Contains(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT);

    if(settings_saved){
        ui.autoLaunch->setChecked(m_settings->Value(GRYPTONITE_SETTING_AUTOLAUNCH_URLS).toBool());
        ui.autoload->setChecked(m_settings->Value(GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE).toBool());
        ui.minimize_on_close->setChecked(m_settings->Value(GRYPTONITE_SETTING_CLOSE_MINIMIZES_TO_TRAY).toBool());
        ui.timeFrmt->setChecked(m_settings->Value(GRYPTONITE_SETTING_TIME_FORMAT_24HR).toBool());

        ui.delaySlider->setValue(m_settings->Value(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT).toInt());
        ui.clipboard_slider->setValue(m_settings->Value(GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT).toInt());
    }

    // We always call this because if the original value is the same as the value restored
    //  from settings, the signal doesn't get executed to refresh the display
    _lockout_slider_changed(ui.delaySlider->value());
    _clipboard_slider_changed(ui.clipboard_slider->value());
}

void PreferencesEdit::accept()
{
    // Save the user's preferences when they accept this dialog
    m_settings->SetValue(GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE, ui.autoload->isChecked());
    m_settings->SetValue(GRYPTONITE_SETTING_AUTOLAUNCH_URLS, ui.autoLaunch->isChecked());
    m_settings->SetValue(GRYPTONITE_SETTING_CLOSE_MINIMIZES_TO_TRAY, ui.minimize_on_close->isChecked());
    m_settings->SetValue(GRYPTONITE_SETTING_TIME_FORMAT_24HR, ui.timeFrmt->isChecked());

    m_settings->SetValue(GRYPTONITE_SETTING_LOCKOUT_TIMEOUT, ui.delaySlider->value());
    m_settings->SetValue(GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT, ui.clipboard_slider->value());

    m_settings->CommitChanges();
    QDialog::accept();
}


void PreferencesEdit::_lockout_slider_changed(int val)
{
    ui.lbl_delay->setText((val == -1) ?
                              "Disabled" : QVariant(val).toString() + " minutes");
}

void PreferencesEdit::_clipboard_slider_changed(int val)
{
    ui.lbl_clipboard->setText(
                (val == 0) ?
                    "Disabled" : QVariant(val).toString() + " seconds");
}
