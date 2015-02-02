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

#ifndef ABOUTBASE_H
#define ABOUTBASE_H

#include <gutil/about.h>

namespace Grypt{


/** The base class for all Grypto-suite about windows. This
 *  helps keep a common theme for all the windows.
*/
class AboutBase : public GUtil::Qt::About
{
    Q_OBJECT
public:
    AboutBase(const QString &appname, const QString &version, QWidget *parent = 0);

private slots:
    void _donate();
};


}

#endif // ABOUTBASE_H
