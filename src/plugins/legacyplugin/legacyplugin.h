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

#ifndef LEGACYPLUGIN_H
#define LEGACYPLUGIN_H

#include <grypto_ilegacy.h>
#include <QObject>

namespace Grypt{


class LegacyPlugin :
        public QObject,
        public ILegacy
{
    Q_OBJECT
    Q_INTERFACES(Grypt::ILegacy)
    Q_PLUGIN_METADATA(IID "Grypt.LegacyPlugin")
public:
    LegacyPlugin(QObject *parent = 0);

    virtual QString UpgradeDatabase(const QString &,
                                    Grypt::Credentials &,
                                    GUtil::Qt::Settings *,
                                    std::function<void(int, const QString &)>,
                                    QWidget *);
};


}

#endif // LEGACYPLUGIN_H
