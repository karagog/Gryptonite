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

#ifndef PASSWORDFILEMODEL_H
#define PASSWORDFILEMODEL_H

#include "grypto_entry.h"
#include <QAbstractItemModel>

class QUuid;

namespace Gryptonite{ namespace GUI{ namespace DataModels{


class PasswordFileModel :
        public QAbstractItemModel
{
    Q_OBJECT
public:

    explicit PasswordFileModel(QObject *parent = 0);

    // Set the password file with this property
    PROPERTY_POINTER(PasswordFile, Common::DataObjects::Password_File);

    // Clears the data and resets the model
    void Clear();

    // To satisfy the QAbstractItemModel virtual function requirements:
    QModelIndex index(int, int, const QModelIndex & parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    int rowCount(const QModelIndex &item = QModelIndex()) const;
    int columnCount(const QModelIndex& p = QModelIndex()) const;
    QVariant data(const QModelIndex &, int) const;
    bool setData ( const QModelIndex & item, const QVariant & value, int role);


    // For drag and drop
    QStringList mimeTypes() const;
    QMimeData * mimeData(const QModelIndexList &) const;
    bool dropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex &);
    Qt::DropActions supportedDropActions() const;


    //bool hasChildren(const QModelIndex &) const;
    bool insertRows(int, int, const QModelIndex &);
    bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());


    QVariant headerData(int, Qt::Orientation, int) const;
    bool setHeaderData(int, Qt::Orientation, const QVariant &, int);
    Qt::ItemFlags flags(const QModelIndex & ) const;

    // Removes the indexes whose ancestors are also present in the list
    //  Returns a list of all children of all the indexes in the list
    static QList<Gryptonite::Common::DataObjects::Entry *> RetainParents(QModelIndexList &);


signals:

    void EntryMoved(Common::DataObjects::Entry *);

};


}}}

#endif // PASSWORDFILEMODEL_H
