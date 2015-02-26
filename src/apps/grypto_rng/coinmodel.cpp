/*Copyright 2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "coinmodel.h"
using namespace std;

#define FETCH_SIZE  100

CoinModel::CoinModel(QObject *p)
    :QAbstractTableModel(p),
      m_lazySize(0),
      m_heads(0),
      m_tails(0)
{}

CoinModel::~CoinModel()
{}

int CoinModel::rowCount(const QModelIndex &par) const
{
    return par.isValid() ? 0 : m_lazySize;
}

int CoinModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant CoinModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    if(index.isValid() &&
            0 <= index.column() && index.column() < columnCount() &&
            0 <= index.row() && index.row() < m_lazySize)
    {
        const bool value = m_data[index.row()];
        switch(role)
        {
        case Qt::DisplayRole:
            if(index.column() == 0)
                ret = index.row() + 1;
            else if(index.column() == 1)
                ret = value ? tr("Heads") : tr("Tails");
            break;
        case Qt::TextAlignmentRole:
            ret = Qt::AlignCenter;
            break;
        default:
            break;
        }
    }
    return ret;
}

QVariant CoinModel::headerData(int section, Qt::Orientation o, int role) const
{
    QVariant ret;
    if(o == Qt::Horizontal && 0 <= section && section < columnCount()){
        switch(role){
        case Qt::DisplayRole:
            if(section == 0)
                ret = tr("Trial");
            else if(section == 1)
                ret = tr("Result");
            break;
        default:
            break;
        }
    }
    return ret;
}

bool CoinModel::canFetchMore(const QModelIndex &parent) const
{
    return parent.isValid() ? false : m_lazySize != (int)m_data.size();
}

void CoinModel::fetchMore(const QModelIndex &parent)
{
    if(!canFetchMore(parent))
        return;

    int diff = m_data.size() - m_lazySize;
    int fetch_size = diff < FETCH_SIZE ? diff : FETCH_SIZE;
    beginInsertRows(QModelIndex(), m_lazySize, m_lazySize + fetch_size);
    m_lazySize += fetch_size;
    endInsertRows();
}

void CoinModel::Flip(int times)
{
    int head_cnt = 0;
    int num_bytes = times / 8;
    int remainder = times % 8;
    if(remainder)
        num_bytes++;

    vector<byte> random_data(num_bytes);
    m_rng.Fill(random_data.data(), num_bytes);

    int old_size = m_data.size();
    int bit_index = 0;
    byte *cur_byte = random_data.data();

    m_data.resize(old_size + times);
    for(int i = 0; i < times; i++)
    {
        bool val = *cur_byte & 1;
        *cur_byte = *cur_byte >> 1;

        m_data[old_size + i] = val;
        if(val)
            head_cnt++;

        if(++bit_index == 8){
            bit_index = 0;
            cur_byte += 1;
        }
    }
    m_heads += head_cnt;
    m_tails += times - head_cnt;
}

void CoinModel::Clear()
{
    beginResetModel();
    m_data.clear();
    m_lazySize = 0;
    m_heads = 0;
    m_tails = 0;
    endResetModel();
}

