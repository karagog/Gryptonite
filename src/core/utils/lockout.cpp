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

#include "lockout.h"
#include <gutil/globals.h>
#include <QTimerEvent>

#define TIMER_RESOLUTION 2500

namespace Grypt{


Lockout::Lockout(QObject *p)
    :QObject(p),
      timerId(-1)
{}

void Lockout::StartLockoutTimer(int mins)
{
    GASSERT(mins >= 0);

    QMutexLocker lkr(&lock);
    if(timerId == -1)
    {
        timeout = QDateTime::currentDateTime().addSecs(mins * 60);
        timerId = startTimer(TIMER_RESOLUTION);
        minutes = mins;
    }
}

bool Lockout::StopLockoutTimer()
{
    bool ret = false;
    QMutexLocker lkr(&lock);
    if(timerId != -1){
        _kill_timer();
        ret = true;
    }
    return ret;
}

bool Lockout::ResetLockoutTimer(int mins)
{
    bool ret = false;
    QMutexLocker lkr(&lock);
    if(timerId != -1){
        timeout = QDateTime::currentDateTime().addSecs(mins * 60);
        minutes = mins;
        ret = true;
    }
    return ret;
}

void Lockout::timerEvent(QTimerEvent *ev)
{
    QMutexLocker lkr(&lock);
    if(ev->timerId() == timerId)
    {
        if(timeout <= QDateTime::currentDateTime())
        {
            _kill_timer();
            emit Lock();
        }
    }
}

void Lockout::_kill_timer()
{
    killTimer(timerId);
    timerId = -1;
}

bool Lockout::IsUserActivity(QEvent *e)
{
    bool ret = false;
    switch(e->type())
    {
    // All of these events cause us to reset the lockout timer
    case QEvent::ActivationChange:
    case QEvent::WindowStateChange:
    case QEvent::FocusIn:
    case QEvent::Move:
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
        ret = true;
        break;
    default:
        break;
    }
    return ret;
}


}
