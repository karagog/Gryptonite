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

#ifndef GRYPTO_LEGACY_UTILS_H
#define GRYPTO_LEGACY_UTILS_H

#include <grypto_passworddatabase.h>
#include <functional>

namespace Grypt{ namespace Legacy{


/** Provides utility functions for parsing and updating legacy files. */
class LegacyUtils
{
public:
    enum FileVersionEnum
    {
        CurrentVersion,
        Version1 = 1,
        Version2 = 2,
        Version3 = 3
    };


    /** Attempts to read the file version at the given path. If it is
     *  unsuccessful you'll get an exception.
    */
    static FileVersionEnum GetFileVersion(const char *file_path);

    /** Attempts to upgrade the file to the latest version. You must supply
     *  the credentials to unlock the original file, as well as new credentials
     *  with which to lock the new file. Note that not all credential types are supported
     *  in the older file types.
     *
     *  If it is successful, you won't get an exception and the updated file will exist
     *  at the new location.
     *
     *  \param file_path The location of the source file to be updated
     *  \param file_version The version of the source file, obtained by calling GetFileVersion()
     *  \param new_path The export path for the updated file
     *  \param old_creds The credentials to unlock the source file
     *  \param new_creds The credentials to lock the updated file
     *  \param progress_callback A callback function to monitor progress between 0 and 100
    */
    static void UpdateFileToCurrentVersion(
            const char *file_path, FileVersionEnum file_version,
            const char *new_path,
            const Credentials &old_creds,
            const Credentials &new_creds,
            std::function<void(int, const QString &)> progress_callback = [](int, const QString &){});

};


}}

#endif // GRYPTO_LEGACY_UTILS_H
