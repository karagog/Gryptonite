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
#include <QPluginLoader>
#include <QCoreApplication>
#include <QDir>
USING_NAMESPACE_GRYPTO;
USING_NAMESPACE_GUTIL;
using namespace std;

#define LEGACY_PLUGIN_BASE_NAME "grypto_legacy_plugin"

#if defined(__WIN32)
#define LEGACY_PLUGIN_NAME  LEGACY_PLUGIN_BASE_NAME GUTIL_SHAREDLIBRARY_SUFFIX_WINDOWS
#elif defined(__unix__)
#define LEGACY_PLUGIN_NAME  "lib" LEGACY_PLUGIN_BASE_NAME GUTIL_SHAREDLIBRARY_SUFFIX_LINUX
#endif

// This is here so we don't have to include QPluginLoader in the header
LegacyManager::LegacyManager(){}

LegacyManager::~LegacyManager()
{
    if(m_pl)
        m_pl->unload();
}

QString LegacyManager::UpgradeDatabase(const QString &path,
                                       Credentials &new_creds,
                                       GUtil::Qt::Settings *settings,
                                       function<void(int, const QString &)> progress_cb,
                                       QWidget *parent)
{
    if(!m_pl){
        // Load the plugin
        m_pl = new QPluginLoader(QDir::toNativeSeparators(QString("%1/%2")
                                                          .arg(QCoreApplication::applicationDirPath())
                                                          .arg(LEGACY_PLUGIN_NAME)));
    }

    QString ret;
    const QByteArray fn = m_pl->fileName().toUtf8();

    if(m_pl->isLoaded() || m_pl->load()){
        ILegacy *iface = qobject_cast<Grypt::ILegacy *>(m_pl->instance());
        if(iface){
            // Forward the parameters to the plugin
            ret = iface->UpgradeDatabase(path, new_creds, settings, progress_cb, parent);
        }
        else
        {
            m_pl->unload();
            m_pl.Clear();
            throw Exception<>(String::Format("Plugin is of unknown type: %s", fn.constData()));
        }
    }
    else{
        QByteArray err = m_pl->errorString().toUtf8();
        m_pl.Clear();
        throw Exception<>(String::Format("Unable to load legacy plugin %s:\n%s",
                                         fn.constData(),
                                         err.constData()));
    }
    return ret;
}

