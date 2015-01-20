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

#include "ui_preferences_window.h"
#include "grypto_global.h"
#include "DataObjects/settings.h"
#include "GUtil/Core/Interfaces/iupdatable.h"
#include <QList>

NAMESPACE_GRYPTO(Common, DataObjects);
    class Entry;
END_GRYPTO_NAMESPACE2;

class QCloseEvent;

class pref_window :
        public QWidget,
        public GUtil::Core::Interfaces::IUpdatable
{
    Q_OBJECT
public:

    pref_window(const QList<Gryptonite::Common::DataObjects::Entry *> &favs,
                QWidget *p = 0);


signals:

    void edit_finished(bool, const QList<Gryptonite::Common::DataObjects::Entry *> &);

protected:

    void closeEvent(QCloseEvent *);
    void showEvent(QShowEvent *);


private slots:

    void accept_changes();

    void move_up();
    void move_down();

    void autoload(int);
    void autolaunch(int);
    void minimize_close(int);
    void dateformat(int);
    void slider_changed(int);
    void clipboard_slider_changed(int);

    void delete_favorite();
    void make_dirty();


private:

    Ui::pref_window widget;

    Gryptonite::GUI::DataObjects::GUISettings sett;
    QList<Gryptonite::Common::DataObjects::Entry *> *favorites;

    bool cancelled;

};


#endif	/* _PREF_WINDOW_H */

