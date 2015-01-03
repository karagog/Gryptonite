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

#ifndef GRYPTO_ENTRY_H
#define	GRYPTO_ENTRY_H

#include "grypto_common.h"
#include "grypto_secret_value.h"
#include <QList>
#include <QDateTime>

class QDomNode;
class QDomDocument;

NAMESPACE_GRYPTO;


class Entry
{
    QList<SecretValue> m_values;
public:

    Entry()
        :_p_Row(0),
          _p_FavoriteIndex(-1)
    {}

    PROPERTY(Id, EntryId);
    PROPERTY(ParentId, EntryId);
    PROPERTY(Row, quint32);

    PROPERTY(Name, QString);
    PROPERTY(Description, QString);
    PROPERTY(FavoriteIndex, int);
    PROPERTY(ModifyDate, QDateTime);

    /** The FileId refers to the database ID. */
    PROPERTY(FileId, FileId);

    /** The FileName is the moniker for the file. */
    PROPERTY(FileName, QString);

    /** The FilePath property refers to the absolute location of the file on disk. */
    PROPERTY(FilePath, QString);

    QList<SecretValue> &Values(){ return m_values; }
    QList<SecretValue> const &Values() const{ return m_values; }

    bool IsFavorite() const{
        return 0 <= GetFavoriteIndex();
    }

};


END_NAMESPACE_GRYPTO;

#endif	/* GRYPTO_ENTRY_H */

