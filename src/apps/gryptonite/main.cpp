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

#include "application.h"
#include <grypto_common.h>
#include <gutil/globallogger.h>
#include <gutil/grouplogger.h>
#include <gutil/filelogger.h>
#include <gutil/messageboxlogger.h>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL;

#define APPLICATION_LOG     GRYPTO_APP_NAME ".log"

int main(int argc, char *argv[])
{
    int ret = 100;

    // Log global messages to a group logger, which writes to all loggers in the group
    SetGlobalLogger(new GroupLogger{

                        // Comment this line for release (silently show errors only in log)
                        new GUtil::Qt::MessageBoxLogger,

                        new FileLogger(QString("%1/" APPLICATION_LOG)
                            .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).toUtf8()),
                    });

    try
    {
        ret = Application(argc, argv).exec();
    }
    catch(const std::exception &ex)
    {
        // Note: This only catches exceptions in the Application's constructor.
        //  Exceptions hit during the event loop of the application are handled
        //  automatically by the application's handle_exception() method
        GlobalLogger().LogException(ex);
        ret = 2;
    }

    SetGlobalLogger(NULL);
    return ret;
}
