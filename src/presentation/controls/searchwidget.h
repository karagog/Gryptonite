/*Copyright 2014 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include "grypto_filtereddatabasemodel.h"
#include <QWidget>

namespace Ui {
class SearchWidget;
}

namespace Grypt
{


class SearchWidget :
        public QWidget
{
    Q_OBJECT
public:
    explicit SearchWidget(QWidget *parent = 0);
    ~SearchWidget();

    void SetFilter(const Grypt::FilterInfo_t &);
    Grypt::FilterInfo_t GetFilter() const;

public slots:
    void Clear();

signals:
    void FilterChanged(const Grypt::FilterInfo_t &);
    void NotifyUserActivity();

protected:
    virtual void focusInEvent(QFocusEvent *ev);
    virtual bool eventFilter(QObject *, QEvent *);
    virtual void hideEvent(QHideEvent *);

private slots:
    void _something_changed();

private:
    Ui::SearchWidget *ui;
    bool m_suppressUpdates;
};


}

#endif // SEARCHWIDGET_H
