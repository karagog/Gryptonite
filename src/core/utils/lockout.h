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

#ifndef LOCKOUT_H
#define LOCKOUT_H

#include <QObject>
#include <QMutex>
#include <QDateTime>

namespace Grypt{


/** A class to emit a lock signal after a specific timeout. */
class Lockout : public QObject
{
    Q_OBJECT

    QMutex lock;
    int timerId;
    int minutes;
    QDateTime timeout;
public:

    explicit Lockout(QObject * = 0);

    /** Starts the lockout timer. */
    void StartLockoutTimer(int minutes);

    /** Returns the number of minutes that was last used in the Start function. */
    inline int Minutes() const{ return minutes; }

    /** Stops the lockout timer where it's at, or does nothing if it wasn't running.
     *  \returns true if the timer was running, false if it wasn't
    */
    bool StopLockoutTimer();

    /** Resets the lockout timer if it was already started, otherwise does nothing.
     *  \returns true if the timer was running
    */
    bool ResetLockoutTimer(int minutes);

    /** A static helper function that returns true if the event is of the type that
     *  is considered user activity (such as mouse clicks or focus events)
    */
    static bool IsUserActivity(QEvent *);


signals:

    /** Tells the receiver to lock when the time is up. */
    void Lock();


protected:

    void timerEvent(QTimerEvent *);


private:

    void _kill_timer();

};


}

#endif // LOCKOUT_H
