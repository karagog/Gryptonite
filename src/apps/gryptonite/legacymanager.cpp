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

#include "legacymanager.h"
#include <gutil/pluginutils.h>
#include <QPluginLoader>
#include <QDir>
USING_NAMESPACE_GRYPTO;
USING_NAMESPACE_GUTIL;
using namespace std;

#define LEGACY_PLUGIN_NAME "grypto_legacy_plugin"


QString LegacyManager::UpgradeDatabase(const QString &path,
                                       Credentials &new_creds,
                                       GUtil::Qt::Settings *settings,
                                       function<void(int, const QString &)> progress_cb,
                                       QWidget *parent)
{
    QPluginLoader pl;
    return GUtil::Qt::PluginUtils::LoadPlugin<ILegacy>(pl, LEGACY_PLUGIN_NAME)
        ->UpgradeDatabase(path, new_creds, settings, progress_cb, parent);
}

