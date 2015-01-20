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

#include "clipboardaccess.h"
#include "grypto_globals.h"
#include <QClipboard>
#include <QApplication>
#include <QTimerEvent>
#include <QTime>

NAMESPACE_GRYPTO;

#define TIMER_RESOLUTION 100


ClipboardAccess::ClipboardAccess(QObject *parent)
    :QObject(parent),
      m_timerId(-1),
      m_counter(0)
{}

void ClipboardAccess::SetText(const QString &txt, int timeout_ms)
{
    qApp->clipboard()->setText(txt);
    if(timeout_ms > 0)
    {
        m_counter = timeout_ms;

        if(m_timerId == -1)
            m_timerId = startTimer(TIMER_RESOLUTION);
    }
}

QString ClipboardAccess::GetText() const
{
    return qApp->clipboard()->text();
}

void ClipboardAccess::timerEvent(QTimerEvent *ev)
{
    if(ev->timerId() == m_timerId)
    {
        ev->accept();
        m_counter -= TIMER_RESOLUTION;

        if(0 >= m_counter)
        {
            qApp->clipboard()->clear();
            killTimer(m_timerId);
            m_timerId = -1;
            m_counter = 0;
        }
    }
}


END_NAMESPACE_GRYPTO;
