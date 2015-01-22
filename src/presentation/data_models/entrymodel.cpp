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

#include "entrymodel.h"
#include "databasemodel.h"
#include <QMimeData>
#include <QFont>
#include <QIcon>

#define BINARY_DATA_STRING_IDENTIFIER "(** BINARY DATA **)"

NAMESPACE_GRYPTO;


EntryModel::EntryModel(QObject *parent)
    :QAbstractTableModel(parent)
{}

EntryModel::EntryModel(const Entry &e, QObject *parent)
    :QAbstractTableModel(parent),
      m_entry(e)
{}

void EntryModel::SetEntry(const Entry &e)
{
    beginResetModel();
    m_entry = e;
    endResetModel();
}

void EntryModel::SwapRows(int first, int second)
{
    if(first < 0 || first >= m_entry.Values().count() ||
            second < 0 || second >= m_entry.Values().count())
        return;

    SecretValue cpy1 = m_entry.Values()[first];
    SecretValue cpy2 = m_entry.Values()[second];
    m_entry.Values().removeAt(second);
    m_entry.Values().insert(second, cpy1);
    m_entry.Values().removeAt(first);
    m_entry.Values().insert(first, cpy2);
    emit dataChanged(index(qMin(first, second), 0),
                     index(qMax(first, second), columnCount() - 1));
}

int EntryModel::rowCount(const QModelIndex &parent) const
{
    int ret = 0;
    if(!parent.isValid())
        ret = m_entry.Values().count();
    return ret;
}

int EntryModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant EntryModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int col = index.column();
    QVariant ret;

    QList<SecretValue> ls = GetEntry().Values();
    if(row >= 0 && row < ls.length() &&
            col >= 0 && col < 2)
    {
        SecretValue sv = ls[row];
        switch(role)
        {
        case Qt::DisplayRole:
            if(col == 0)
            {
                ret = sv.GetName() + ":";
            }
            else if(col == 1)
            {
                if(sv.GetIsHidden())
                    ret = "**********";
                else
                    ret = sv.GetValue();
            }
            break;
        case Qt::FontRole:
            ret = [=]{ QFont f; f.setBold(col == 0); return f; }();
            break;
        case Qt::EditRole:
            if(col == 0)
                ret = sv.GetName();
            else if(col == 1)
                ret = sv.GetValue();
            break;
        case Qt::ToolTipRole:
            ret = sv.GetNotes();
            break;
        case Qt::DecorationRole:
            if(col == 0)
            {
                if(sv.GetNotes().length() > 0)
                    ret = QIcon(":/grypto/icons/notes.png");
            }
            break;
        case IDSecret:
            ret = sv.GetIsHidden();
            break;
        case IDNotes:
            ret = sv.GetNotes();
            break;
        default:
            break;
        }
    }
    return ret;
}

bool EntryModel::setData(const QModelIndex &item, const QVariant & value, int role)
{
    bool res(false);
    if(item.row() >= 0 && item.row() < GetEntry().Values().length() &&
            item.column() >= 0 && item.column() < columnCount())
    {
        SecretValue &sv = m_entry.Values()[item.row()];

        switch(role)
        {
        case Qt::EditRole:
            if(item.column() == 0){
                sv.SetName(value.toString().toUtf8());
                res = true;
            }
            else if(item.column() == 1){
                sv.SetValue(value.toString().trimmed().toUtf8());
                res = true;
            }
            break;
        case IDSecret:
            sv.SetIsHidden(value.toBool());
            res = true;
            break;
        case IDNotes:
            sv.SetNotes(value.toString());
            res = true;
            break;
        default:
            break;
        }
    }

    if(res)
        dataChanged(index(item.row(), 0),
                    index(item.row(), columnCount() - 1));
    return res;
}

bool EntryModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || row > m_entry.Values().count() || parent.isValid())
        return false;

    beginInsertRows(QModelIndex(), row, row + count - 1);
    {
        for(int i = row; i < row + count; ++i)
            m_entry.Values().insert(i, SecretValue());
    }
    endInsertRows();
    return true;
}

bool EntryModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || (row + count) > m_entry.Values().count() || parent.isValid())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    {
        for(int i = row + count - 1; i >= row; --i)
            m_entry.Values().removeAt(i);
    }
    endRemoveRows();
    return true;
}

QVariant EntryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant ret;
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
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
    }
    return ret;
}

Qt::ItemFlags EntryModel::flags(const QModelIndex &) const
{
    return  Qt::ItemIsEnabled |
            Qt::ItemIsSelectable |
            Qt::ItemIsDropEnabled |
            Qt::ItemIsEditable |
            Qt::ItemIsDragEnabled;
}

QStringList EntryModel::mimeTypes() const
{
    QStringList types;
    types.append("text/plain");
    return types;
}

QMimeData *EntryModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();

    mimeData->setData("text/plain", data(indexes[1], Qt::EditRole).toString().toUtf8());

    return mimeData;
}

bool EntryModel::dropMimeData(const QMimeData *data,
                              Qt::DropAction action, int , int , const QModelIndex &par)
{
    bool ret(false);
    if (action == Qt::IgnoreAction);
    else if(action == Qt::MoveAction);
    else if(action == Qt::CopyAction)
    {
        if(data->hasFormat("text/plain"))
            setData(index(par.row(), 1), data->data("text/plain"), Qt::EditRole);
    }

    return ret;
}

Qt::DropActions EntryModel::supportedDropActions() const
{
    return Qt::CopyAction;
}


END_NAMESPACE_GRYPTO;
