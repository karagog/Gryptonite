/*Copyright 2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef LEGACYMANAGER_H
#define LEGACYMANAGER_H

#include <grypto_common.h>
#include <functional>
#include <QString>
class QWidget;

namespace GUtil{ namespace Qt{
class Settings;
}}

/** Manages legacy functions like updating old database formats. */
class LegacyManager
{
public:
    /** Takes the user through the process of upgrading the database, and
     *  returns the path of the upgraded database, if successful.
     *
     *  In general you should only call this if it is known that the database
     *  is not in the latest format, such as when it fails while opening. This
     *  function will take care to detect the file version (if valid) and upgrade
     *  the database.
     *
     *  \param path The path to the file to be upgraded
     *  \param new_creds A credentials object to store the user's credentials, if they
     *          successfully updated the file. This is so you don't have to ask them for it again.
     *  \param settings A settings object used by the child dialogs
     *  \param progress_cb A callback that exports the task's progress
     *  \param parent The parent widget for all widgets this function will create
     *
     *  \returns The path to the upgraded database, if it was successful. If the user
     *  aborted the operation it returns a null string. If there was an error during
     *  upgrade then an exception is thrown.
     *
     *  \throws An exception on error, but if the user cancelled you get a null string
     *  in the return value.
    */
    static QString UpgradeDatabase(const QString &path,
                                   Grypt::Credentials &new_creds,
                                   GUtil::Qt::Settings *settings,
                                   std::function<void(int, const QString &)> progress_cb,
                                   QWidget *parent);
};

#endif // LEGACYMANAGER_H
