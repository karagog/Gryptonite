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

#ifndef GRYPTONITE_SETTINGS_H
#define GRYPTONITE_SETTINGS_H

/** \file
 *  This file keeps track of the various application settings available
 *  to the user, and connects them with their key identifiers so we know
 *  what they're called in the GUtil settings object.
*/

#define GRYPTONITE_SETTING_AUTOLOAD_LAST_FILE       "gryptonite_autoload"

#define GRYPTONITE_SETTING_RECENT_FILES_LENGTH      "gryptonite_recent_files_length"

#define GRYPTONITE_SETTING_AUTOLAUNCH_URLS          "gryptonite_autolaunch_urls"

#define GRYPTONITE_SETTING_CLOSE_MINIMIZES_TO_TRAY  "gryptonite_close_minimizes"

#define GRYPTONITE_SETTING_TIME_FORMAT_24HR         "gryptonite_time_format"

#define GRYPTONITE_SETTING_LOCKOUT_TIMEOUT          "gryptonite_lockout_timeout"

#define GRYPTONITE_SETTING_CLIPBOARD_TIMEOUT        "gryptonite_clipboard_timeout"


#endif // GRYPTONITE_SETTINGS_H

