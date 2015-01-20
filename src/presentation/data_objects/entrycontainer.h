/*Copyright 2010 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef FILEENTRYCONTAINER_H
#define FILEENTRYCONTAINER_H

#include "grypto_global.h"
#include "GUtil/gutil_macros.h"
#include "GUtil/DataObjects/collection.h"
#include <QList>

class QUuid;

NAMESPACE_GRYPTO(Common, DataObjects);


class Entry;

class FileEntryContainer :
        public GUtil::DataObjects::Collection<Entry *>
{
public:

    FileEntryContainer(Entry *parent = 0);
    FileEntryContainer(const QList<Entry *> &);
    ~FileEntryContainer();

    PROPERTY_POINTER(ParentEntry, Entry);

    Entry *CreateChildEntry();

    QList<Entry *> GetAllEntries() const;

    Entry *FindById(const QUuid &) const;

    QList<int> GetFileIds(bool valid = true) const;

    QList<Entry *> GetFavorites(bool sort = true) const;


private:

    void on_remove(int);

};


END_GRYPTO_NAMESPACE2;

#endif // FILEENTRYCONTAINER_H
