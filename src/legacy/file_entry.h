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

#ifndef _FILE_ENTRY_H
#define	_FILE_ENTRY_H

#include "attribute.h"
#include <vector>
#include <QModelIndex>
#include <QAbstractTableModel>
using namespace std;

class QDateTime;
class QXmlStreamWriter;
class QXmlStreamReader;
class QDomElement;
class QRegExp;

// The baseclass item that can exist as part of a file
// i.e. Either a password entry or a folder entry (which contains a list
// of more file_entries)

namespace Grypt{ namespace Legacy{ namespace V3{


class File_Entry : public QAbstractTableModel
{
    Q_OBJECT
public:
    File_Entry(int t = password, QObject * par = 0);
    File_Entry(const File_Entry &, QObject * par = 0);
    virtual ~File_Entry();

    void operator =(const File_Entry & other);

    QModelIndex parent ( const QModelIndex & index ) const;
    int rowCount ( const QModelIndex & par = QModelIndex()) const;
    int columnCount( const QModelIndex & par = QModelIndex() ) const;
    QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
    bool setData ( const QModelIndex & item, const QVariant & value, int role);

    QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
    Qt::ItemFlags flags ( const QModelIndex & ) const;
    Qt::DropActions supportedDropActions () const;

    // These functions should be overwritten in the subclasses
    void get_file_index(int, int, File_Entry **) const;
    int countRows() const;
    QVariant data(int, int col = 0) const;
    bool insertRows(int, int, const QModelIndex & par = QModelIndex());
    bool removeRows(int, int, const QModelIndex & par = QModelIndex());

    QStringList mimeTypes() const;
    QMimeData * mimeData(const QModelIndexList &) const;
    bool dropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex &);

    File_Entry* getParent() const;
    void set_file_parent(File_Entry *);

    // The parts for folder entries:
    bool insert_file_rows(int &, int, QObject *);
    bool remove_file_rows ( int r, int count);

    void swap_attributes(int i1, int i2);
    static void mark_not_visited(File_Entry *);

    void append_favorites(QList<File_Entry *> *);

    static bool favorites_lessthan(File_Entry *, File_Entry *);
    bool is_child_of(File_Entry *other);

    bool is_favorite(bool include_root = true) const;
    static bool is_favorite(const File_Entry *);

    // Use these as accessors for various members
    QDateTime *getModifyDate();

    int getFavorite() const;
    void setFavorite(int);

    vector<File_Entry *> &getContents();
    vector<Attribute> &getAttributes();

    Attribute &getLabel();
    Attribute &getDescription();

    int getRow() const;
    void setRow(int);
    int getType() const;
    void setType(int t);


    // Use these for indexing what data you're interested in
    static const int IDBinary = Qt::UserRole;
    static const int IDBinaryData = Qt::UserRole + 1;
    static const int IDSecret = Qt::UserRole + 2;
    static const int IDNotes = Qt::UserRole + 3;
    static const int IDBrokenFile = Qt::UserRole + 4;

    enum ent_type
    {
        basetype,
        directory,
        password
    };

private:
    // Is it a password or a directory, or a file base type?
    int type;
    int favorite;

    // Attributes that every entry must have
    Attribute label;
    Attribute description;

    // The parts of a folder entry
    vector<File_Entry *> contents;

    // The parts of a password entry:
    vector<Attribute> attributes;

    QDateTime * modify_date;

    // The parent File_Entry to this one
    File_Entry * pf_parent;

    // A reference to the row this entry falls under its parent
    int row;

    bool check_index(int);

    // This stores whether or not the regexp search matches me or not
    bool search_match;
    bool exp_matched;
    bool date_matched;

    static bool _is_favorite_child(const File_Entry *, bool include_root = true);
    static bool _is_binary_entry(File_Entry *);

    // Have we already visited this entry on this search?
    bool visited;

};


}}}

#endif	/* _FILE_ENTRY_H */

