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

#include "password_file.h"
#include "default_attribute_info.h"
#include <gutil/string.h>
#include <gutil/exception.h>
#include <QXmlStreamWriter>
#include <QFile>
#include <QMimeData>
#include <QDateTime>
#include <QTextStream>
USING_NAMESPACE_GUTIL;
using namespace std;

namespace Grypt{ namespace Legacy{ namespace V3{


Password_File::Password_File(QObject * par)
    : QAbstractItemModel(par)
{
    contents = new File_Entry(File_Entry::directory, this);

    // To avoid the default "No Content" label that is desired for normal entries
    contents->getLabel().setContent("");
    contents->getDescription().setContent("");

    // Need to initialize the file password
    create_new_file_password();
}

Password_File::Password_File(QObject * par, const Password_File& orig)
    : QAbstractItemModel(par)
{
    contents = new File_Entry(orig.contents->getType(), this);
    *contents = *(orig.contents);
    files_password = orig.files_password;
}

Password_File::~Password_File()
{
    delete contents;
}


QModelIndex Password_File::index(int row, int col, const QModelIndex & parent) const
{
    File_Entry *tmp, *t;

    if(!parent.isValid()){
        tmp = contents;
    }
    else
    {
        tmp = (File_Entry *)parent.internalPointer();
        Q_ASSERT(tmp !=0);
    }

    if(tmp->getType() != File_Entry::directory)
        return QModelIndex();
    tmp->get_file_index(row, col, &t);

    if(t == 0)
        return QModelIndex();
    return createIndex(row, col, t);
}

QModelIndex Password_File::parent(const QModelIndex & item) const
{
    if(!item.isValid()){    // The root has no parents
        return QModelIndex();
    }

    File_Entry * tmp = (File_Entry *)item.internalPointer();
    if(tmp == 0)
        return QModelIndex();

    tmp = tmp->getParent();
    if(tmp == contents)
        return QModelIndex();
    else
        return createIndex(tmp->getRow(), 0, tmp);
}

int Password_File::rowCount(const QModelIndex & item) const
{
    File_Entry * tmp;
    if(!item.isValid())
    {
        tmp = contents;
    }
    else if(item.column() > 0)
        return 0;
    else
    {
        tmp = (File_Entry *)item.internalPointer();
        Q_ASSERT(tmp != 0);
    }

    return tmp->countRows();
}

int Password_File::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant Password_File::data(const QModelIndex & ind, int role) const
{
    File_Entry * tmp;
    QVariant res;

    if(!ind.isValid())      // The root
        return res;

    tmp = (File_Entry *)index(ind.row(), ind.column(), ind.parent()).internalPointer();
    if(tmp == 0)
        return res;

    res = tmp->data(role, ind.column());
    return res;
}

QVariant Password_File::headerData ( int, Qt::Orientation, int) const
{
    return QVariant();
}

bool Password_File::setHeaderData ( int, Qt::Orientation, const QVariant &, int)
{
    return false;
}

Qt::ItemFlags Password_File::flags ( const QModelIndex & ind ) const
{
    Qt::ItemFlags ret = Qt::ItemIsDragEnabled |
                        Qt::ItemIsSelectable |
                        Qt::ItemIsEnabled;

    int t;
    if(ind.column() > 0)
        return ret;

    if(!ind.isValid())
    {
        t = File_Entry::directory;
    }
    else
    {
        t = ((File_Entry *)ind.internalPointer())->getType();
    }

    if(t == File_Entry::directory)
    {
        ret |= Qt::ItemIsDropEnabled;
    }

    return ret;
}

Qt::DropActions Password_File::supportedDropActions () const
{
    return Qt::MoveAction;
}

bool Password_File::setData ( const QModelIndex & item, const QVariant & value, int role)
{
    if(!item.isValid())
        return false;

    bool res = false;
    QVariantList ol;
    File_Entry * ref;
    switch(role)
    {
    case Qt::DisplayRole:
        ((File_Entry *)item.internalPointer())->getLabel().setContent(value.toString());
        res = true;
        break;
    case Qt::EditRole:
        if(!value.toString().trimmed().isEmpty())
        {
            ((File_Entry *)item.internalPointer())->getLabel().setContent(value.toString());
            res = true;
        }

        break;
    case Qt::UserRole:
        ref = (File_Entry *)item.internalPointer();
        // We should have received a QObjectList from our 'data' function
        ol = value.toList();
        Q_ASSERT(ol.count() == 2);

        ref->getLabel().setContent(ol.at(0).toString());
        ref->getDescription().setContent(ol.at(1).toString());
        break;
    case Qt::UserRole + 1:
        *(((File_Entry *)item.internalPointer())->getModifyDate()) =
                QDateTime::fromString(value.toString());
        break;
        default:
        break;
    }

    dataChanged(item, item);
    return res;
}

QStringList Password_File::mimeTypes() const
{
    QStringList types;
    types << "Gryptonite/entry_tree";
    return types;
}

QMimeData * Password_File::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();

    QString encodedData;

    QXmlStreamWriter x(&encodedData);

    // Remove items that are children of other items (otherwise they get duplicated)
    QModelIndexList copyList = indexes;
    preprocess_indexes(copyList);

    int count = copyList.count();
    x.writeStartElement("packet");
    x.writeAttribute("count", QVariant(count).toString());

    for(int i = 0; i < copyList.count(); i++)
    {
        QModelIndex ind = copyList[i];
        Q_ASSERT(ind.isValid());

        writeXML(x, (File_Entry *)ind.internalPointer());
    }

    x.writeEndElement();

    mimeData->setData("Gryptonite/entry_tree", encodedData.toLatin1());

    return mimeData;
}

QList<File_Entry *> Password_File::preprocess_indexes(QModelIndexList &indexes)
{
    // Remove all the indexes in the first column, because they mean nothing to this algorithm
    for(int i = indexes.count() - 1; i >= 0; i--)
    {
        if(indexes[i].column() == 1)
        {
            indexes.removeAt(i);
        }
    }

    int pointer_index = 0;

    // return a list of all the items affected by the delete
    QList<File_Entry *> top_level_items;

    for(int i = indexes.count(); i > 0; i--)
    {
        QModelIndex in_question = indexes[pointer_index];
        File_Entry *e = (File_Entry *)in_question.internalPointer();

        bool noevent = true;
        for(int j = 0; j < indexes.count(); j++)
        {
            QModelIndex trythis = indexes[j];
            File_Entry *f = (File_Entry *)trythis.internalPointer();

            if(e != f &&
               e->is_child_of(f))
            {
                indexes.removeAt(pointer_index);
                noevent = false;
                break;
            }
        }

        if(noevent)
        {
            pointer_index++;
            top_level_items.append(e);
        }
    }

    // At this point there is no item in the list that is a child of another, so
    //  it is safe to recurse the children and just append them all to the list

    return deletion_helpers::_prepare_list_of_items_to_be_deleted(top_level_items);
}

void Password_File::populate_label_and_desc(File_Entry *target, QXmlStreamReader *x)
{
    if(x->attributes().hasAttribute(S_LABEL))
    {
        QString t =  x->attributes().value(S_LABEL).toString();
        target->getLabel().setContent(t);
    }
    if(x->attributes().hasAttribute(S_DESCRIPTION))
    {
        QString t = x->attributes().value(S_DESCRIPTION).toString();
        target->getDescription().setContent(t);
    }
    if(x->attributes().hasAttribute("fav"))
    {
        bool ok = false;
        int tmpint = x->attributes().value("fav").toString().toInt(&ok);
        target->setFavorite(ok ? tmpint : -1);
    }
}

QList<File_Entry *> Password_File::deletion_helpers::
        _prepare_list_of_items_to_be_deleted(const QList<File_Entry *> &items)
{
    QList<File_Entry *> ret;
    for(int i = items.count() - 1; i >= 0; i--)
    {
        File_Entry *e = items.at(i);

        _recurse_children(ret, e);
    }

    return ret;
}

void Password_File::deletion_helpers::
        _recurse_children(QList<File_Entry *> &lst, File_Entry *targ)
{
    // Add the item itself
    lst.append(targ);

    for(int i = targ->getContents().size() - 1; i >= 0; i--)
    {
        File_Entry *e = targ->getContents().at(i);

        int typ = e->getType();
        if(typ == File_Entry::directory)
        {
            // Recursively append my children
            _recurse_children(lst, e);
        }
        else if(typ == File_Entry::password)
        {
            // Add the children which are not directories (they will be added by
            //  the recursive call)
            lst.append(e);
        }
    }
}

bool Password_File::dropMimeData(const QMimeData *, Qt::DropAction,
                                 int, int, const QModelIndex &)
{
//    if (action == Qt::IgnoreAction)
//        return true;

//    // We don't do copying
//    if(action == Qt::CopyAction)
//        return false;

//    if (!data->hasFormat("Gryptonite/entry_tree"))
//        return false;

//    if (c > 0)
//    {
//        // If the user drops on the second column, then we treat it as if they dropped it on the first column
//        c = 0;
//    }

//    File_Entry * ref;
//    if(!par.isValid())
//    {
//        ref = contents;
//    }
//    else
//    {
//        ref = (File_Entry *)par.internalPointer();
//        Q_ASSERT(ref != 0);
//    }

//    QByteArray dat = data->data("Gryptonite/entry_tree");
//    QXmlStreamReader doc(dat);
//    doc.readNextStartElement();

//    if(r == -1)
//        r = ref->countRows();

//    int cnt = doc.attributes().value("count").toString().toInt();
//    if(cnt <= 0)
//        return false;

//    insertRows(r, cnt, par);

//    //File_Entry *selection_representative = 0;
//    bool is_root = ref == contents;
//    int i = 0;
//    while(doc.readNextStartElement())
//    {
//        QString xml_part = doc.InnerXml();
//        QXmlStreamReader x(xml_part);

//        File_Entry *te = ref->getContents().at(r+i);
//        readXML(&x, te, false, is_root, true);

//        // We want to be able to select one of the items that changed, so we emit
//        //  this information in a signal at the end
////        if(i == 0)
////            selection_representative = te;

//        dataChanged(index(r + i, 0, par), index(r + i, 0, par));

//        i++;
//    }

    return true;
}

bool Password_File::insertRows(int row, int count, const QModelIndex & par = QModelIndex())
{
    File_Entry * ref;

    if(par.column() > 0)
        return false;

    if(!par.isValid())  // If it's the root node
    {
        ref = contents;
    }
    else
    {
        ref = (File_Entry *)par.internalPointer();
        Q_ASSERT(ref != 0);
    }

    if(row == -1)
        row = ref->countRows();

    beginInsertRows(par, row, row + count - 1);
    bool res = ref->insert_file_rows(row, count, this);
    endInsertRows();

    return res;
}

bool Password_File::removeRows ( int row, int count, const QModelIndex & parent)
{
    bool res = false;
    File_Entry * ref;

    if(parent.column() > 0)
        return false;

    if(!parent.isValid())
    {
        ref = contents;
    }
    else
    {
        ref = (File_Entry *)parent.internalPointer();
        Q_ASSERT(ref != 0);
    }

    Q_ASSERT(ref->getType() != File_Entry::password);

    beginRemoveRows(parent, row, row + count - 1);
    res = ref->remove_file_rows(row, count);
    endRemoveRows();
    return res;
}

void Password_File::writeXML(QString &xmlOutput)
{
    QXmlStreamWriter x(&xmlOutput);

    x.writeStartElement("secrets");

    x.writeStartElement("fpw");
    x.writeAttribute("v", files_password);
    x.writeEndElement();

    writeXML(x, contents);

    x.writeEndElement();
    x.writeEndDocument();
}

void Password_File::writeXML(QXmlStreamWriter &sw, File_Entry*e) const
{
    int typ = e->getType();
    sw.writeStartElement(typ == File_Entry::directory ? "direntry" : "entry");

    QString t1, t2;

    // Convert labels and descriptions to base64
    t1 = e->getLabel().getLabel();
    t2 = e->getLabel().getContent();
    sw.writeAttribute(t1, t2);

    t1 = e->getDescription().getLabel();
    t2 = e->getDescription().getContent();
    sw.writeAttribute(t1, t2);

    sw.writeAttribute("fav", QVariant::fromValue<int>(e->getFavorite()).toString());

    if(typ == File_Entry::directory)
    {
        for(vector<File_Entry *>::iterator it = e->getContents().begin(); it != e->getContents().end(); it++)
        {
            writeXML(sw, *it);
        }
    }
    else if(typ == File_Entry::password)
    {
        sw.writeAttribute("date", e->getModifyDate()->toString());

        int _lim = e->getAttributes().size();
        for(int i = 0; i < _lim; i++)
        {
            e->getAttributes().at(i).writeXml(&sw);
        }
    }
    else
    {
        Q_ASSERT(false);
        return;
    }

    sw.writeEndElement();
}

void Password_File::readXML(const QString & ix, bool pre_version_3)
{
    QXmlStreamReader d(ix);
    if(!d.readNextStartElement())
        throw Exception<>(String::Format("Could not read passwords: %s", d.errorString().toUtf8().constData()));

    Q_ASSERT(d.name().toString() == "secrets");

    clear_model();

    if(!pre_version_3)
    {
        d.readNextStartElement();
        Q_ASSERT(d.name().toString() == "fpw");
        files_password = d.attributes().value("v").toString();

        while(d.readNext() != QXmlStreamReader::EndElement);
        d.readNextStartElement();
    }

    populate_label_and_desc(contents, &d);

    readXML(&d, contents, pre_version_3, true, false);
}

void Password_File::readXML(QXmlStreamReader *x, File_Entry *e,
                            bool pre_version_3, bool root, bool existing)
{
    // Note: 'existing' represents whether or not the file entry was already inserted before this call,
    // as in when mime data is dropped into the view.  It inserts the row and calls readXML on it.
    // Every other time readXML is called we put it in charge of inserting the rows
    int i = 0;
    int p;
    File_Entry * target;
    do
    {
        if(!x->readNextStartElement())
        {
            p = x->tokenType();
            if(p == QXmlStreamReader::EndElement || p == QXmlStreamReader::EndDocument ||
               p == QXmlStreamReader::Invalid)
                break;
        }


        {
            QModelIndex tmpind;
            if(!root)
            {
                tmpind = createIndex(e->getRow(), 0, e);
            }

            if(existing)
                target = e;
            else
            {
                insertRows(i, 1, tmpind);
                target = e->getContents().at(i);
            }
        }


        populate_label_and_desc(target, x);

        if(x->name().toString() == "direntry")
        {
            target->setType(File_Entry::directory);

            readXML(x, target, pre_version_3, false);
        }
        else if(x->name().toString() == "entry")
        {
            if(x->attributes().hasAttribute("date"))
            {
                QString t = x->attributes().value("date").toString();
                *(target->getModifyDate()) = QDateTime::fromString(t);
            }

            do
            {
                if(!x->readNextStartElement())
                {
                    break;
                }

                Attribute na;
                na.readXml(x);
                target->getAttributes().push_back(na);
            } while(1);
        }
        else
        {
            Q_ASSERT(false);
            break;
        }

        i++;
    } while(1);
}

QModelIndex Password_File::newEntry(File_Entry::ent_type t, const QModelIndex &loc)
{
    int r = loc.row();
    File_Entry *e;

    QModelIndex ind = loc;
    if(loc.column() > 0)	// Transform the index to be at column 0
        ind = index(r, 0, loc.parent());

    e = (File_Entry *)ind.internalPointer();

    if(!ind.isValid())
    {
        r = 0;
        e = contents;
    }
    else
    {
        int typ = e->getType();
        if(typ == File_Entry::directory)
        {
            r = 0;
        }
        else if(typ == File_Entry::password)
        {
            ind = loc.parent();
            e = (File_Entry *)ind.internalPointer();
            if(e == 0)
                e = contents;
        }
        else return QModelIndex();
    }

    Q_ASSERT(e != 0);

    insertRows(r, 1, ind);

    if(t == File_Entry::directory)
    {
        e->getContents().at(r)->setType(File_Entry::directory);
    }

    return index(r, 0, ind);
}

void Password_File::clear_model()
{
    // Reset the model before reading in XML
    beginResetModel();
    contents->getLabel().setContent("");
    contents->getDescription().setContent("");
    contents->remove_file_rows(0, contents->countRows());
    endResetModel();
}

void Password_File::invalidate_search_indexes()
{
    File_Entry::mark_not_visited(contents);
}


QList<File_Entry *> Password_File::get_favorites()
{
    QList<File_Entry *> res;

    if(File_Entry::is_favorite(contents))
        res.append(contents);

    contents->append_favorites(&res);

    // Sort res in order of favorite priority
    qSort(res.begin(), res.end(), File_Entry::favorites_lessthan);

    return res;
}

File_Entry * Password_File::_find_elt_by_name(QString s)
{
    return _find_elt_by_name(contents, s);
}

File_Entry * Password_File::_find_elt_by_name(File_Entry * e, QString s)
{
    File_Entry *res = 0;

    int typ = e->getType();
    if(typ == File_Entry::directory)
    {
        int _lim = e->getContents().size();
        for(int i = 0; i < _lim; i++)
        {
            if((res = _find_elt_by_name(e->getContents().at(i), s)) != 0)
                break;
        }
    }
    else if(typ == File_Entry::password)
    {
        if(e->getLabel().getContent() == s)
            res = e;
    }
    else
        throw Exception<>();

    return res;
}

QModelIndex Password_File::find_index(const QModelIndex &location, File_Entry *e) const
{
    File_Entry * tmp;
    if(!location.isValid())
        tmp = contents;
    else
        tmp = (File_Entry *)location.internalPointer();
    Q_ASSERT(tmp != 0);
    Q_ASSERT(tmp->getType() == File_Entry::directory);

    QModelIndex ret;
    int _lim = tmp->getContents().size();
    for(int i = 0; i < _lim; i++)
    {
        if(tmp->getContents().at(i) == e)
            return index(i, 0, location);

        if(tmp->getContents().at(i)->getType() == File_Entry::directory)
        {
            if((ret = find_index(index(i, 0, location), e)) != QModelIndex())
                return ret;
        }
    }

    return QModelIndex();
}

void Password_File::update_broken_file_refs(const QList<int> &l)
{
    foreach(int i, l)
    {
        _update_broken_file_refs(i, contents);
    }
}

bool Password_File::_update_broken_file_refs(int i, File_Entry *e)
{
    int typ = e->getType();
    if(typ == File_Entry::password)
    {
        int _lim = e->getAttributes().size();
        for(int j = 0; j < _lim; j++)
        {
            QModelIndex curindex = e->index(j, 1);
            if(e->data(curindex, File_Entry::IDBinary).toBool())
            {
                int tmpid = e->data(curindex, File_Entry::IDBinaryData).toInt();
                if(tmpid == i)
                {
                    e->setData(curindex, true, File_Entry::IDBrokenFile);
                    return true;
                }
            }
        }
    }
    else if(typ == File_Entry::directory)
    {
        int _lim = e->getContents().size();
        for(int j = 0; j < _lim; j++)
        {
            if(_update_broken_file_refs(i, e->getContents().at(j)))
                return true;
        }
    }

    return false;
}

QString Password_File::get_file_password()
{
    return files_password;
}

void Password_File::create_new_file_password(const QString &newp)
{
    if(newp.length() == 0)
    {
        // This password is twice as long as a normal one
//        files_password = Encryption_Utils::generate_random_string() +
//                         Encryption_Utils::generate_random_string();
    }
    else
    {
        files_password = newp;
    }
}

File_Entry *Password_File::getContents() const
{
    return contents;
}

QList<int> Password_File::get_file_ids(bool valid)
{
    QList<int> ret;
    _append_file_ids(contents, ret, valid);
    return ret;
}

void Password_File::_append_file_ids(File_Entry *e, QList<int> &l, bool valid)
{
    int typ = e->getType();
    if(typ == File_Entry::directory)
    {
        int _lim = e->getContents().size();
        for(int i = 0; i < _lim; i++)
        {
            _append_file_ids(e->getContents().at(i), l, valid);
        }
    }
    else if(typ == File_Entry::password)
    {
        int _lim = e->getAttributes().size();
        for(int i = 0; i < _lim; i++)
        {
            QModelIndex curindex = e->index(i, 0);
            if(e->data(curindex, File_Entry::IDBinary).toBool())
            {
                if(!valid || !e->data(curindex, File_Entry::IDBrokenFile).toBool())
                    l.append(e->data(e->index(i, 0), File_Entry::IDBinaryData).toInt());
            }
        }
    }
}


}}}
