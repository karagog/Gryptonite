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

#ifndef CLIPBOARDACCESS_H
#define CLIPBOARDACCESS_H

#include <QObject>

namespace Grypt
{


class ClipboardAccess : public QObject
{
    Q_OBJECT
    int m_timerId;
    int m_counter;
public:

    explicit ClipboardAccess(QObject *parent = 0);

    /** Exports the text to clipboard and optionally
     * clears it after a given timeout.
    */
    void SetText(const QString &, int timeout_ms = 0);

    /** Returns the current text in the clipboard */
    QString GetText() const;


protected:

    virtual void timerEvent(QTimerEvent *);

};


}

#endif // CLIPBOARDACCESS_H
