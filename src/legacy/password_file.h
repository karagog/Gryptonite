/*Copyright 2010-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef _PASSWORD_FILE_H
#define	_PASSWORD_FILE_H

#include "file_entry.h"
#include <QAbstractItemModel>

class QDomNode;

// Represents a whole collection of passwords and their organizational structure

namespace Grypt{ namespace Legacy{ namespace V3{


class Password_File : public QAbstractItemModel
{
    Q_OBJECT
public:
    Password_File(QObject * = 0);
    Password_File(QObject *, const Password_File&);
    virtual ~Password_File();

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

    //bool hasChildren(const QModelIndex &) const;
    bool insertRows(int, int, const QModelIndex &);
    bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());

    QVariant headerData(int, Qt::Orientation, int) const;
    bool setHeaderData(int, Qt::Orientation, const QVariant &, int);
    Qt::ItemFlags flags(const QModelIndex & ) const;
    Qt::DropActions supportedDropActions() const;

    // Write the tree of contents out to an XML string
    void writeXML(QString &xmlOutput);

    // Read in an XML string and populate contents
    //  (versions prior to 3.0 used a slightly different xml format)
    void readXML(const QString &, bool pre_version_3);

    QModelIndex newEntry(File_Entry::ent_type, const QModelIndex &);
    void clear_model();

    void invalidate_search_indexes();

    QList<File_Entry *> get_favorites();

    File_Entry * _find_elt_by_name(QString);

    void update_broken_file_refs(const QList<int> &);

    // Returns a list of file ids contained in this data object, with an optional
    //   flag to denote whether the id needs to be valid
    QList<int> get_file_ids(bool valid = false);

    QString get_file_password();
    void create_new_file_password(const QString &newp = "");

    static QList<File_Entry *> preprocess_indexes(QModelIndexList &);

    File_Entry *getContents() const;

private:
    File_Entry *contents;

    bool _update_broken_file_refs(int, File_Entry *);

    void writeXML(QXmlStreamWriter &, File_Entry*) const;
    void readXML(QXmlStreamReader *x, File_Entry *e,
                 bool pre_version_3, bool root, bool existing = false);

    QModelIndex find_index(const QModelIndex &, File_Entry *) const;

    static void populate_label_and_desc(File_Entry *, QXmlStreamReader *);

    QString files_password;

    // These functions are only useful during an entry deletion
    class deletion_helpers
    {
    public:
        static QList<File_Entry *> _prepare_list_of_items_to_be_deleted(const QList<File_Entry*>&);
        static void _recurse_children(QList<File_Entry *>&, File_Entry *);
    };

    File_Entry * _find_elt_by_name(File_Entry *, QString);
    void _append_file_ids(File_Entry *, QList<int> &, bool);
};


}}}

#endif	/* _PASSWORD_FILE_H */

