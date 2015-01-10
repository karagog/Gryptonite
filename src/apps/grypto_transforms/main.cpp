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

#include <grypto_cryptotransformswindow.h>
#include <grypto_common.h>
#include <gutil/application.h>
#include <gutil/qt_settings.h>
#include <gutil/globallogger.h>
#include <gutil/filelogger.h>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GRYPTO;

#define APPLICATION_LOG "grypto_transforms.log"

int main(int argc, char *argv[])
{
    // Set up a global file logger, so the application can log errors somewhere
    SetGlobalLogger(new FileLogger(
                        String::Format("%s/" APPLICATION_LOG,
                                       QStandardPaths::writableLocation(QStandardPaths::DataLocation).toUtf8().constData())));

    GUtil::Qt::Application a(argc, argv, GRYPTO_APP_NAME, GRYPTO_VERSION_STRING);

    // Don't allow exceptions to crash us. You can read the log to find exception details.
    a.SetTrapExceptions(true);

    // Initialize a settings object for persistent data
    GUtil::Qt::Settings settings(GRYPTO_SETTINGS_IDENTIFIER);

    // Create and show the crypto transforms window
    CryptoTransformsWindow w(&settings);
    w.show();

    // Execute the application event loop
    int ret = a.exec();

    // Clean up and return
    SetGlobalLogger(NULL);
    return ret;
}
