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

#ifndef FILEENTRYMODEL_H
#define FILEENTRYMODEL_H

#include "grypto_globals.h"
#include "grypto_entry.h"
#include <QAbstractTableModel>

namespace Grypt{
class DatabaseModel;


class EntryModel :
        public QAbstractTableModel
{
    Q_OBJECT
    Entry m_entry;
    DatabaseModel *m_dbModel;
public:

    explicit EntryModel(DatabaseModel * = 0, QObject *parent = 0);
    explicit EntryModel(const Entry &, DatabaseModel * = 0, QObject *parent = 0);

    void SetEntry(const Entry &);
    Entry const &GetEntry() const{ return m_entry; }

    virtual int rowCount(const QModelIndex & = QModelIndex()) const;
    virtual int columnCount(const QModelIndex & = QModelIndex()) const;
    virtual Qt::ItemFlags flags( const QModelIndex & ) const;

    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QModelIndexList &) const;
    virtual bool dropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex &);
    virtual Qt::DropActions supportedDropActions() const;

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int, Qt::Orientation, int) const;
    virtual bool setData(const QModelIndex &, const QVariant &, int);
    virtual bool insertRows(int row, int count, const QModelIndex &parent);
    virtual bool removeRows(int row, int count, const QModelIndex &parent);

    /** Exchanges the two rows with each other. */
    void SwapRows(int first, int second);

    // Use these for indexing what data you're interested in
    enum DataReferenceIDs
    {
        IDSecret = Qt::UserRole + 1,
        IDNotes = Qt::UserRole + 2
    };

};


}

#endif // FILEENTRYMODEL_H
