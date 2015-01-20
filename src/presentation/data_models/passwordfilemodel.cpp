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

#include "passwordfilemodel.h"
#include <QUuid>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
USING_GRYPTO_NAMESPACE2(GUI, DataModels);
USING_GRYPTO_NAMESPACE2(Common, DataObjects);

PasswordFileModel::PasswordFileModel(QObject *parent)
    :QAbstractItemModel(parent),
      _p_PasswordFile(0)
{}

QModelIndex PasswordFileModel::index(int row, int col, const QModelIndex &parent) const
{
    QModelIndex ret;

    if(!GetPasswordFile() ||
            row < 0 ||
            col < 0)
        return ret;

    Entry *tmp;

    if(!parent.isValid())
        tmp = GetPasswordFile()->At(row);
    else
        tmp = (Entry *)parent.internalPointer();

    return createIndex(row, col, tmp);
}

QModelIndex PasswordFileModel::parent(const QModelIndex &item) const
{
    QModelIndex ret;
    if(!item.isValid())
        return ret;

    Entry *tmp(((Entry *)item.internalPointer())->GetParent());

    if(tmp != 0)
        ret = createIndex(tmp->GetRow(), 0, tmp);

    return ret;
}

int PasswordFileModel::rowCount(const QModelIndex &item) const
{
    Entry *tmp(0);
    if(item.isValid() && item.column() == 0)
        tmp = (Entry *)item.internalPointer();

    if(tmp)
        return tmp->Count();
    return 0;
}

int PasswordFileModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant PasswordFileModel::data(const QModelIndex &ind, int role) const
{
    QVariant res;

    if(ind.isValid())
    {
        Entry *tmp((Entry *)ind.internalPointer());

        switch((Qt::ItemDataRole)role)
        {
        case Qt::DisplayRole:

            if(ind.column() == 0)
                res = tmp->GetLabel();
            else if(ind.column() == 1)
                res = tmp->GetModifyDate();

            break;
        default:
            break;
        }
    }

    return res;
}

QVariant PasswordFileModel::headerData(int, Qt::Orientation, int) const
{
    return QVariant();
}

bool PasswordFileModel::setHeaderData(int, Qt::Orientation, const QVariant &, int)
{
    return false;
}

Qt::ItemFlags PasswordFileModel::flags(const QModelIndex &ind ) const
{
    Qt::ItemFlags ret(Qt::ItemIsDragEnabled |
            Qt::ItemIsSelectable |
            Qt::ItemIsEnabled |
            Qt::ItemIsDropEnabled);

    return ret;
}

Qt::DropActions PasswordFileModel::supportedDropActions () const
{
    return Qt::MoveAction;
}

bool PasswordFileModel::setData ( const QModelIndex & item, const QVariant & value, int role)
{
    bool res(false);

    if(item.isValid())
    {
        Entry * ref((Entry *)item.internalPointer());

        switch((Qt::ItemDataRole)role)
        {
        case Qt::DisplayRole:

            ref->SetLabel(value.toString());
            res = true;

            break;
        case Qt::EditRole:

            if(!value.toString().trimmed().isEmpty())
            {
                ref->SetLabel(value.toString());
                res = true;
            }

            break;
        case Qt::UserRole:

            // We should have received a QVariantList from our 'data' function
        {
            QVariantList ol(value.toList());
            Q_ASSERT(ol.count() == 2);

            ref->SetLabel(ol.at(0).toString());
            ref->SetDescription(ol.at(1).toString());
        }

            break;
        case Qt::UserRole + 1:

            ref->GetModifyDate() = value.toDateTime();

            break;
        default:
            break;
        }
    }

    if(res)
        dataChanged(item, item);
    return res;
}

QStringList PasswordFileModel::mimeTypes() const
{
    QStringList types;
    types << "Gryptonite/entry_tree";
    return types;
}

QMimeData *PasswordFileModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();

    QString encodedData;

    QXmlStreamWriter x(&encodedData);

    // Remove items that are children of other items (otherwise they get duplicated)
    QModelIndexList copyList = indexes;
    RetainParents(copyList);

    int count(copyList.count());
    x.writeStartElement("packet");
    x.writeAttribute("s", QString("%1").arg(count));

    for(int i = 0; i < count; i++)
    {
        QModelIndex ind = copyList[i];
        Q_ASSERT(ind.isValid());

        ((Entry *)ind.internalPointer())->WriteXml(x);
    }

    x.writeEndElement();

    mimeData->setData("Gryptonite/entry_tree", encodedData.toAscii());

    return mimeData;
}

QList<Entry *> PasswordFileModel::RetainParents(QModelIndexList &indexes)
{
    // Remove all the indexes in the first column, because they mean nothing to this algorithm
    for(int i = indexes.count() - 1; i >= 0; i--)
    {
        if(indexes[i].column() == 1)
            indexes.removeAt(i);
    }

    int pointer_index = 0;

    for(int i = indexes.count(); i > 0; i--)
    {
        QModelIndex in_question = indexes[pointer_index];
        Entry *e = (Entry *)in_question.internalPointer();

        bool noevent = true;
        for(int j = 0; j < indexes.count(); j++)
        {
            QModelIndex trythis = indexes[j];
            Entry *f = (Entry *)trythis.internalPointer();

            if(e != f &&
               e->IsChildOf(*f))
            {
                indexes.removeAt(pointer_index);
                noevent = false;
                break;
            }
        }

        if(noevent)
            pointer_index++;
    }

    QList<Entry *> ret;
    for(int i = 0; i < indexes.count(); i++)
    {
        Entry *f = (Entry *)indexes[i].internalPointer();
        ret.append(f->GetAllEntries());
    }

    return ret;
}

bool PasswordFileModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int r, int c, const QModelIndex &par)
{
    if(action == Qt::CopyAction ||
            action == Qt::IgnoreAction ||
            !data->hasFormat("Gryptonite/entry_tree"))
        return false;

    if (c > 0)
    {
        // If the user drops on the second column,
        //  treat it as if they dropped it on the first column
        c = 0;
    }

    FileEntryContainer *container(par.isValid() ?
                                (FileEntryContainer *)par.internalPointer() :
                                (FileEntryContainer *)GetPasswordFile());

    QByteArray dat = data->data("Gryptonite/entry_tree");
    QXmlStreamReader doc(dat);
    doc.readNextStartElement();

    if(r == -1)
        r = container->Count();

    int cnt = doc.attributes().value("s").toString().toInt();
    if(cnt <= 0)
        return false;

    insertRows(r, cnt, par);

    for(int i = 0;
        i < cnt && doc.readNextStartElement();
        i++)
    {
        container->At(r+i)->ReadXml(doc);

        while(doc.readNext() && doc.tokenType() != doc.EndElement);
    }

    dataChanged(index(r, 0, par), index(r + cnt - 1, 1, par));
    return true;
}

bool PasswordFileModel::insertRows(int row, int count, const QModelIndex & par = QModelIndex())
{
    if(!GetPasswordFile() || par.column() > 0)
        return false;

    FileEntryContainer *container(par.isValid() ?
                                      (FileEntryContainer *)par.internalPointer() :
                                      (FileEntryContainer *)GetPasswordFile());

    if(row == -1)
        row = container->Count();

    beginInsertRows(par, row, row + count - 1);
    {
        for(int i = row; i < row + count; i++)
            container->Insert(container->CreateChildEntry(), row);
    }
    endInsertRows();

    return true;
}

bool PasswordFileModel::removeRows ( int row, int count, const QModelIndex & par)
{
    if(!GetPasswordFile() ||
            par.column() > 0)
        return false;

    FileEntryContainer *container(par.isValid() ?
                                      (FileEntryContainer *)par.internalPointer() :
                                      (FileEntryContainer *)GetPasswordFile());

    beginRemoveRows(par, row, row + count - 1);
    {
        for(int i = 0; i < count; i++)
            container->Remove(row);
    }
    endRemoveRows();
    return true;
}

void PasswordFileModel::Clear()
{
    if(GetPasswordFile())
    {
        GetPasswordFile()->Clear();
        reset();
    }
}
