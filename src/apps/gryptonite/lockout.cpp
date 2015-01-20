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
#include <QTimerEvent>

#define TIMER_RESOLUTION 60001

Lockout::Lockout(QObject *p)
    :QObject(p),
      timerId(-1)
{}

void Lockout::StartLockoutTimer(int minutes)
{
    QMutexLocker lkr(&lock);
    if(timerId == -1)
    {
        timeout = QDateTime::currentDateTime().addSecs(minutes * 60);        
        timerId = startTimer(TIMER_RESOLUTION);
    }
}

void Lockout::StopLockoutTimer()
{
    QMutexLocker lkr(&lock);
    if(timerId != -1)
        _kill_timer();
}

void Lockout::ResetLockoutTimer(int minutes)
{
    QMutexLocker lkr(&lock);
    if(timerId != -1)
        timeout = QDateTime::currentDateTime().addSecs(minutes * 60);        
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
