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

#include "file_entry.h"
#include "default_attribute_info.h"
#include <QXmlStreamWriter>
#include <QDateTime>
#include <QIcon>
#include <QFont>
#include <QMimeData>
#include <QRegExp>

namespace Grypt{ namespace Legacy{ namespace V3{

#define BINARY_DATA_STRING_IDENTIFIER "(** BINARY DATA **)"


File_Entry::File_Entry(int t, QObject * par)
    : QAbstractTableModel(par)
{
    label.setLabel(S_LABEL);
    description.setLabel(S_DESCRIPTION);
    label.setContent("No Content");

    modify_date = new QDateTime(QDateTime::currentDateTime());
    visited = false;

    type = t;
    favorite = -1;

    pf_parent = 0;
    row = 0;

    date_matched = true;
}

File_Entry::File_Entry(const File_Entry & other, QObject * par)
    : QAbstractTableModel(par)
{
    // I didn't plan for this to ever run, but it should work anyways
    Q_ASSERT(false);

    *this = File_Entry(other.type, par);
}

File_Entry::~File_Entry()
{
    for(int i = 0; i < (int)contents.size(); i++)
    {
        delete contents[i];
    }
    delete modify_date;
}

void File_Entry::operator =(const File_Entry & other)
                           {
    label = other.label;
    description = other.description;
    row = other.row;
    favorite = other.favorite;

    pf_parent = other.pf_parent;

    if(type == directory)
    {
        // Clean up
        for(vector<File_Entry *>::iterator it = contents.begin();
        it != contents.end(); it++)
        {
            delete *it;
        }
        contents.resize(0);

        if(other.type == directory)
        {
            for(int i = 0; i < (int)contents.size(); i++)
            {
                contents.push_back(new File_Entry(*(other.contents[i])));
            }
        }
        else
        {
            for(int i = 0; i < (int)other.attributes.size(); i++)
            {
                attributes.push_back(other.attributes.at(i));
            }
        }
    }
    else
    {
        attributes.resize(0);

        if(other.type == password)
        {
            for(int i = 0; i < (int)other.attributes.size(); i++)
            {
                attributes.push_back(other.attributes.at(i));
            }
        }
        else
        {
            for(int i = 0; i < (int)contents.size(); i++)
            {
                contents.push_back(new File_Entry(*(other.contents[i])));
            }
        }
    }

    type = other.type;
}


QModelIndex File_Entry::parent ( const QModelIndex & ) const
{
    // This is a flat table model, nobody has a valid parent
    return QModelIndex();
}

int File_Entry::rowCount (const QModelIndex &par) const
{
    if(!par.isValid())
    {
        if(type == password)
            return attributes.size();
        else if(type == directory)
            return contents.size();
    }
    return 0;
}

int File_Entry::columnCount( const QModelIndex &) const
{
    if(type == password)
        return 2;
    else if(type == directory)
        return 2;
    else
    {
        Q_ASSERT(false);
        return 0;
    }
}

QVariant File_Entry::data ( const QModelIndex & index, int role) const
{
    int row = index.row();
    int col = index.column();
    QVariant ret;

    if(row < 0 ||
       (type == password && row >= (int)attributes.size()) ||
       (type == directory && row >= (int)contents.size()))
        return ret;

    switch(role)
    {
    case Qt::DisplayRole:
        if(type == password)
        {
            if(col == 0)
            {
                ret = attributes.at(row).getLabel() + ":";
            }
            else if(col == 1)
            {
                QString retstr;

                if(attributes.at(row).secret)
                {
                    retstr = "**********";
                }
                else
                {
                    if(attributes.at(row).binary)
                        retstr = "(Binary Data)";
                    else
                        retstr = attributes.at(row).getContent();
                }

                ret = retstr;
            }
        }
        else if(type == directory)
        {
            File_Entry *targ = contents.at(row);

            if(col == 0)
            {
                ret = targ->getLabel().getContent();
            }
            else if(col == 1)
            {
                if(targ->type == password)
                {
                    ret = targ->modify_date->toString(AP_DATE_FORMAT);
                }
            }
        }
        break;
    case Qt::FontRole:
        if(type == password)
        {
            if(col == 0)
            {
                QFont tmpfont;
                tmpfont.setBold(true);
                ret = tmpfont;
            }
            else if(col == 1)
            {
                if(!attributes.at(row).secret && attributes.at(row).binary)
                {
                    QFont tmpfont;
                    tmpfont.setItalic(true);
                    ret = tmpfont;
                }
            }
        }
        else if(type == directory)
        {
            File_Entry *targ = contents.at(row);

            if(targ->type == directory)
            {
                QFont tmpfont;
                tmpfont.setBold(true);
                ret = tmpfont;
            }
        }
        break;
    case Qt::EditRole:
        if(type == password)
        {
            if(col == 0)
                ret = attributes.at(row).getLabel();
            else if(col == 1)
            {
                if(attributes.at(row).binary)
                {
                    ret = BINARY_DATA_STRING_IDENTIFIER;
                }
                else
                {
                    ret = attributes.at(row).getContent();
                }
            }
        }
        break;
    case Qt::ToolTipRole:
        if(type == directory)
        {
            File_Entry *targ = contents[row];

            ret = targ->getDescription().getContent();
        }
        else if(type == password)
        {
            ret = attributes.at(row).notes;

            if(attributes.at(row).broken_binary_data_reference)
                ret = QString("** BROKEN FILE REFERENCE **") +
                      (ret.toString().length() > 0 ? "\n\n" : "") +
                      ret.toString();
        }
        break;
    case Qt::DecorationRole:
        if(type == directory)
        {
            if(col > 0)
                break;

            File_Entry *targ = contents[row];
            if(targ->type == directory)
            {
                if(is_favorite(targ))
                    ret = QIcon(":/icons/star.png");
            }
            else if(targ->type == password)
            {
                if(targ->is_favorite(false))
                    ret = QIcon(":/icons/star.png");
                else
                    ret = QIcon(":/icons/key.png");
            }
        }
        else if(type == password)
        {
            if(col == 0)
            {
                if(attributes.at(row).notes.length() > 0)
                    ret = QIcon(":/icons/notes.png");
            }
            else if(col == 1)
            {
                if(attributes.at(row).broken_binary_data_reference)
                    ret = QIcon(":/icons/redX.png");
            }
        }
        break;
    case IDSecret:
        if(type == password)
        {
            ret = attributes.at(row).secret;
        }
        break;
    case IDBinary:
        if(type == password)
            ret = attributes.at(row).binary;
        break;
    case IDBinaryData:
        if(type == password)
            ret = attributes.at(row).getContent();
        break;
    case IDNotes:
        if(type == password)
            ret = attributes.at(row).notes;
        break;
    case IDBrokenFile:
        if(type == password)
            ret = attributes.at(row).broken_binary_data_reference;
        break;
    case Qt::TextAlignmentRole:
        // Align modify date on the right
        if(type == directory)
        {
            if(col == 1)
                ret = QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        break;
    default:
        break;
    }

    return ret;
}

bool File_Entry::setData ( const QModelIndex & item, const QVariant & value, int role)
{
    bool res = false;
    switch(role)
    {
    case Qt::DisplayRole:
        if(item.column() == 0)
        {
            attributes.at(item.row()).setLabel(value.toString());
            res = true;
        }
        else if(item.column() == 1)
        {
            attributes.at(item.row()).setContent(value.toString());
            res = true;
        }
        break;
    case Qt::EditRole:
        if(item.column() == 0)
        {
            attributes.at(item.row()).setLabel(value.toString());
            res = true;
        }
        else if(item.column() == 1)
        {
            attributes.at(item.row()).setContent(value.toString());
            res = true;
        }
        break;
     case IDSecret:
        attributes.at(item.row()).secret = value.toBool();
        break;
     case IDBinary:
        attributes.at(item.row()).binary = value.toBool();
        break;
     case IDNotes:
        attributes.at(item.row()).notes = value.toString();
        break;
     case IDBrokenFile:
        attributes.at(item.row()).broken_binary_data_reference = value.toBool();
        break;
    default:
        break;
    }

    dataChanged(item, item);
    return res;
}

bool File_Entry::insertRows(int r, int c, const QModelIndex &it)
{
    Q_ASSERT(!it.isValid());
    Q_ASSERT(r >= 0 && r <= rowCount());

    beginInsertRows(it, r, r + c - 1);
    for(int i = 0; i < c; i++)
    {
        attributes.insert(attributes.begin() + r + i, Attribute());
    }
    endInsertRows();
    return true;
}

bool File_Entry::removeRows(int r, int c, const QModelIndex & par)
{
    int cnt = 0;

    beginRemoveRows(par, r, r + c - 1);
    for(int i = 0; i < c && r < (int)attributes.size(); i++)
    {
        attributes.erase(attributes.begin() + r);
        cnt++;
    }
    endRemoveRows();

    if(cnt == c)
        return true;
    return false;
}

QStringList File_Entry::mimeTypes() const
{
    QStringList types;
    types.append("text/plain");
    return types;
}

QMimeData * File_Entry::mimeData(const QModelIndexList &indexes) const
{
    Q_ASSERT(indexes.count() == 2);
    QMimeData *mimeData = new QMimeData();

    if(type == password)
    {
        mimeData->setData("text/plain", data(indexes[1], Qt::EditRole).toString().toLatin1());
    }

    return mimeData;
}

bool File_Entry::dropMimeData(const QMimeData *data,
                              Qt::DropAction action, int , int , const QModelIndex &par)
{
    if(type == directory)
        return false;

    if (action == Qt::IgnoreAction);
    else if(action == Qt::MoveAction)
    {
        return false;
    }
    else if(action == Qt::CopyAction)
    {
        if(data->hasFormat("text/plain"))
        {
            setData(index(par.row(), 1), data->data("text/plain"), Qt::DisplayRole);

            // We're no longer holding binary data after this
            setData(index(par.row(), 1), false, IDBinary);
        }
    }
    else
    {
        return false;
    }

    return true;
}

void File_Entry::get_file_index(int r, int c, File_Entry ** p) const
{
    Q_ASSERT(type != password);
    if(r < 0 || c < 0 || c > 1 || r >= (int)contents.size())
    {
        *p = 0;
    }
    else
    {
        *p = contents[r];
    }
}

int File_Entry::countRows() const
{
    if(type == directory)
    {
        return contents.size();
    }
    return 0;
}

QVariant File_Entry::data(int role, int col) const
{
    QVariant ret;
    switch((Qt::ItemDataRole)role)
    {
    case Qt::DisplayRole:
        if(col == 0)
            ret = label.getContent();
        else if(col == 1 && type == password)
        {
            ret = modify_date->toString(AP_DATE_FORMAT);
        }
        break;
    case Qt::ToolTipRole:
        if(col > 1)
            break;
        ret = description.getContent();
        break;
    case Qt::DecorationRole:
        if(col > 0)
            break;

        if(type == directory)
        {
            if(is_favorite(this))
                ret = QIcon(":/icons/star.png");
        }
        else if(type == password)
        {
            if(is_favorite(false))
                ret = QIcon(":/icons/star.png");
            else
                ret = QIcon(":/icons/key.png");
        }

        break;
   case Qt::FontRole:
        if(col > 0)
            break;
        if(type == directory)
        {
            QFont f;
            f.setBold(true);
            ret = f;
        }
        break;
    case Qt::EditRole:
        ret = label.getContent();
    case Qt::TextAlignmentRole:
        if(col == 1)
            ret = Qt::AlignRight;
        break;
    default:
        break;
    }

    return ret;
}

QVariant File_Entry::headerData ( int section, Qt::Orientation orientation, int role) const
{
    QVariant ret;

    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            if(type == password)
            {
                switch(section)
                {
                case 0:
                    ret = "Secret:";
                    break;
                case 1:
                default:
                    break;
                }
            }
            else if(type == directory)
            {
                if(section == 1)
                    return "Last Modified:";
            }
        }
    }

    return ret;
}

Qt::ItemFlags File_Entry::flags ( const QModelIndex &ind) const
{
    Qt::ItemFlags ret = Qt::ItemIsEnabled |
                        Qt::ItemIsEditable |
                        Qt::ItemIsSelectable |
                        Qt::ItemIsDropEnabled;

    if(type == password && ind.isValid())
    {
        // Don't allow to drag binary data
        if(!data(ind, IDBinary).toBool())
        {
            ret |= Qt::ItemIsDragEnabled;
        }
    }

    return ret;
}

Qt::DropActions File_Entry::supportedDropActions () const
{
    return Qt::CopyAction;
}

File_Entry* File_Entry::getParent() const
{
    return pf_parent;
}

void File_Entry::set_file_parent(File_Entry * newp)
{
    pf_parent = newp;
}

bool File_Entry::insert_file_rows(int &r, int count, QObject * qparent)
{
    if(this->type == directory)
    {
        if(r >= 0 && r <= (int)contents.size() && count >=0)
        {
            for(int i = 0; i < count; i++)
            {
                File_Entry * e = new File_Entry(File_Entry::password, qparent);
                contents.insert(contents.begin() + r + i, e);

                // Go along and update the rows which were affected by the insert
                for(int j = r + i; j < (int)contents.size(); j++)
                {
                    contents[j]->setRow(j);
                }

                e->set_file_parent(this);
            }

            return true;
        }
    }

    return false;
}

bool File_Entry::remove_file_rows(int r, int count)
{
    int c = 0;
    for(int i = 0; i < count && r < (int)contents.size(); i++)
    {
        delete contents[r];
        contents.erase(contents.begin() + r);
        c++;
    }

    // Go along and update the row values
    for(int i = r; i < (int)contents.size(); i++)
    {
        contents[i]->setRow(i);
    }

    if(c == count)
        return true;
    return false;
}

bool File_Entry::check_index(int index)
{
    if(type != password || index < 0 || index >= (int)attributes.size())
    {
        return false;
    }
    return true;
}

int File_Entry::getRow() const
{
    return row;
}

void File_Entry::setRow(int newr)
{
    row = newr;
}

int File_Entry::getType() const
{
    return type;
}

void File_Entry::setType(int t)
{
    type = t;
}

void File_Entry::swap_attributes(int i1, int i2)
{
    Attribute tmp = attributes.at(i1);
    attributes.at(i1) = attributes.at(i2);
    attributes.at(i2) = tmp;

    int lesser = i1;
    int greater = i2;
    if(i2 < i1)
    {
        lesser = i2;
        greater = i1;
    }
    dataChanged(index(lesser, 0), index(greater, 1));
}

void File_Entry::mark_not_visited(File_Entry *e)
{
    Q_ASSERT(e->type == directory);

    e->visited = false;
    e->search_match = false;
    e->exp_matched = false;
    for(int i = 0; i < (int)e->contents.size(); i++)
    {
        if(e->contents[i]->type == password)
        {
            e->contents[i]->visited = false;
            e->contents[i]->search_match = false;
            e->contents[i]->exp_matched = false;
        }
        else if(e->contents[i]->type == directory)
        {
            e->mark_not_visited(e->contents[i]);
        }
    }
}

void File_Entry::append_favorites(QList<File_Entry *> *list)
{
    Q_ASSERT(type == directory);
    for(int i = 0; i < (int)contents.size(); i++)
    {
        if(contents[i]->favorite >= 0)
            list->append(contents[i]);

        if(contents[i]->type == directory)
            contents[i]->append_favorites(list);
    }
}

bool File_Entry::favorites_lessthan(File_Entry *lhs, File_Entry *rhs)
{
    return (lhs->favorite < rhs->favorite);
}


bool File_Entry::is_child_of(File_Entry *other)
{
    File_Entry *p = getParent();
    while(p != 0)
    {
        if(p == other)
            return true;

        p = p->getParent();
    }
    return false;
}

bool File_Entry::is_favorite(bool include_root) const
{
    return _is_favorite_child(this, include_root);
}

bool File_Entry::_is_favorite_child(const File_Entry *e, bool include_root)
{
    if(e == 0)
        return false;

    if(is_favorite(e))
        return true;

    File_Entry *p = e->getParent();
    Q_ASSERT(include_root || p != 0);
    if(!include_root &&
       (p->getParent() == 0))// p's parent will be zero if p is the root entry
    return false;

    return _is_favorite_child(p, include_root);
}

bool File_Entry::is_favorite(const File_Entry *e)
{
    return e->favorite >= 0;
}

QDateTime *File_Entry::getModifyDate()
{
    return modify_date;
}

void File_Entry::setFavorite(int val)
{
    favorite = val;
}

int File_Entry::getFavorite() const
{
    return favorite;
}

vector<File_Entry *> &File_Entry::getContents()
{
    return contents;
}

vector<Attribute> &File_Entry::getAttributes()
{
    return attributes;
}

Attribute &File_Entry::getLabel()
{
    return label;
}

Attribute &File_Entry::getDescription()
{
    return description;
}

bool File_Entry::_is_binary_entry(File_Entry *e)
{
    if(e->type == directory)
        return false;

    for(int i = e->attributes.size() - 1; i >= 0; i--)
    {
        if(e->attributes.at(i).binary)
            return true;
    }

    return false;
}


}}}
