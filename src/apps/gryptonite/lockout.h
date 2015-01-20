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

#ifndef LOCKOUT_H
#define LOCKOUT_H

#include <QObject>
#include <QMutex>
#include <QDateTime>


/** A class to emit a lock signal after a specific timeout. */
class Lockout : public QObject
{
    Q_OBJECT

    QMutex lock;
    int timerId;
    QDateTime timeout;
public:

    explicit Lockout(QObject * = 0);

    /** Starts the lockout timer. */
    void StartLockoutTimer(int minutes);
    
    /** Stops the lockout timer where it's at, or does nothing if it wasn't running. */
    void StopLockoutTimer();
    
    /** Resets the lockout timer if it was already started, otherwise does nothing. */
    void ResetLockoutTimer(int minutes);
    
    
signals:

    /** Tells the receiver to lock when the time is up. */
    void Lock();
    
    
protected:

    void timerEvent(QTimerEvent *);
    
    
private:

    void _kill_timer();

};


#endif // LOCKOUT_H