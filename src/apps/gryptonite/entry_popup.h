/*Copyright 2014-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef ENTRY_POPUP_H
#define ENTRY_POPUP_H

#include "ui_entry_popup.h"
#include "settings.h"
#include <grypto_clipboardaccess.h>
#include <grypto_databasemodel.h>
#include <gutil/qt_settings.h>

#define SETTING_ENTRY_POPUP_GEOMETRY "ep_geometry"


class EntryPopup : public QWidget
{
    Q_OBJECT
    Ui::EntryPopup ui;
    GUtil::Qt::Settings *m_settings;
    Grypt::ClipboardAccess m_clipboard;
public:

    EntryPopup(const Grypt::Entry &e, Grypt::DatabaseModel *mdl, GUtil::Qt::Settings *settings = 0, QWidget *parent = 0)
        :QWidget(parent, Qt::Dialog | Qt::WindowStaysOnTopHint),
          m_settings(settings)
    {
        ui.setupUi(this);
        setWindowModality(Qt::NonModal);
        setWindowTitle(e.GetName());

        if(settings && settings->Contains(SETTING_ENTRY_POPUP_GEOMETRY))
            restoreGeometry(settings->Value(SETTING_ENTRY_POPUP_GEOMETRY).toByteArray());

        ui.view_entry->SetDatabaseModel(mdl);
        ui.view_entry->SetEntry(e);
        connect(ui.view_entry, SIGNAL(RowActivated(int)),
                this, SLOT(_entry_row_activated(int)));
    }

signals:

    /** Indicates that the user wants to give control back to the calling window */
    void CedeControl();

protected:

    virtual void closeEvent(QCloseEvent *ev){
        if(m_settings){
            m_settings->SetValue(SETTING_ENTRY_POPUP_GEOMETRY, saveGeometry());
        }
        QWidget::closeEvent(ev);
    }

private slots:
    void _cede_control(){
        emit CedeControl();
        close();
    }
    void _entry_row_activated(int r){
        Grypt::SecretValue const &e = ui.view_entry->GetEntry().Values()[r];
        m_clipboard.SetText(e.GetValue(),
                            e.GetIsHidden() ? m_settings->Value(GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT).toInt() * 1000
                                            : 0);
    }
};


#endif // ENTRY_POPUP_H
