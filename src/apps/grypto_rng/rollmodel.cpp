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

#include "rollmodel.h"
#include <gutil/heap.h>
using namespace std;
USING_NAMESPACE_GUTIL;

#define FETCH_SIZE  100

RollModel::RollModel(QObject *p)
    :QAbstractTableModel(p),
      m_total(0),
      m_min(GINT32_MAX),
      m_max(GINT32_MIN),
      m_mean(0.0),
      m_median(0.0),
      m_modeCount(0)
{}

RollModel::~RollModel()
{}

int RollModel::rowCount(const QModelIndex &par) const
{
    return par.isValid() ? 0 : m_data.size();
}

int RollModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant RollModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    if(index.isValid() &&
            0 <= index.column() && index.column() < columnCount() &&
            0 <= index.row() && index.row() < rowCount())
    {
        const int value = m_data[index.row()];
        switch(role)
        {
        case Qt::DisplayRole:
            if(index.column() == 0)
                ret = value;
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

QVariant RollModel::headerData(int section, Qt::Orientation o, int role) const
{
    QVariant ret;
    if(o == Qt::Horizontal && 0 <= section && section < columnCount()){
        switch(role){
        case Qt::DisplayRole:
//            if(section == 0)
//                ret = tr("Results");
            break;
        default:
            break;
        }
    }
    return ret;
}

void RollModel::Roll(int min, int max, int times)
{
    struct occurrence_t{
        int value;
        uint count;
        bool operator < (const occurrence_t &o) const{
            return count > o.count ||
                    (count == o.count && value < o.value);
        }
    };

    beginResetModel();
    m_data.resize(times);
    m_total = 0;
    m_min = GINT32_MAX;
    m_max = GINT32_MIN;
    m_mode.clear();
    m_modeCount = 0;
    m_mean = 0.0;
    m_median = 0.0;
    QHash<int, uint> occurrences;

    for(int i = 0; i < times; i++){
        int X = m_rng.U_Discrete(min, max);
        m_data[i] = X;
        m_total += X;

        if(X < m_min)
            m_min = X;
        if(X > m_max)
            m_max = X;

        if(occurrences.contains(X))
            occurrences[X]++;
        else
            occurrences.insert(X, 1);
    }
    m_mean = (double)m_total/times;
    
    // Compute the median
    vector<int> sorted_data = m_data;
    std::sort(sorted_data.begin(), sorted_data.end());
    int median_index = sorted_data.size() / 2;
    int val1 = sorted_data[median_index];
    int val2 = (sorted_data.size() & 1) ? val1 : sorted_data[median_index - 1];
    m_median = ((double)val1 + val2) / 2;

    // Compute the mode (using heap sort)
    Heap<occurrence_t> heap;
    for(int k : occurrences.keys())
        heap.Push({k, occurrences[k]});

    if(heap.Count()){
        occurrence_t *t = heap.Top();
        uint mode_count = t->count;
        int cnt = 0;
        QList<int> tmp_mode;
        while(t && (mode_count == t->count)){
            tmp_mode.append(t->value);
            heap.Pop();
            t = heap.Top();
            cnt++;
        }

        // If every item is the mode, then there is no mode
        if(cnt < occurrences.keys().count()){
            m_modeCount = mode_count;
            m_mode = tmp_mode;
        }
    }
    endResetModel();
}

void RollModel::Clear()
{
    beginResetModel();
    m_data.clear();
    m_total = 0;
    m_min = GINT32_MAX;
    m_max = GINT32_MIN;
    m_mean = 0.0;
    m_median = 0.0;
    m_mode.clear();
    m_modeCount = 0;
    endResetModel();
}

