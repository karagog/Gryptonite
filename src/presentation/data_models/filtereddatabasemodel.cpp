/*Copyright 2014 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "filtereddatabasemodel.h"
#include "databasemodel.h"
#include "grypto_entry.h"

NAMESPACE_GRYPTO;


FilteredDatabaseModel::FilteredDatabaseModel(QObject *parent)
    :QSortFilterProxyModel(parent)
{}

void FilteredDatabaseModel::setSourceModel(QAbstractItemModel *m)
{
    if(dynamic_cast<DatabaseModel *>(m) == NULL)
        qDebug("Refusing to set model because it's not a DatabaseModel");
    else
        QSortFilterProxyModel::setSourceModel(m);
}

DatabaseModel *FilteredDatabaseModel::_get_database_model() const
{
    return static_cast<DatabaseModel *>(sourceModel());
}

void FilteredDatabaseModel::SetFilter(const FilterInfo_t &fi)
{
    m_index.clear();

    if(fi.IsValid)
    {
        QRegExp rx(fi.SearchString,
                   fi.IgnoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive,
                   fi.SearchStringType == fi.Wildcard ? QRegExp::Wildcard : QRegExp::RegExp);

        // Iterate through the entire model and populate the filter index
        _update_index(QModelIndex(), rx, fi);

        setFilterRegExp(rx);
    }
    else
    {
        invalidateFilter();
    }
}

// Returns true if the given row matches or a child row matches
bool FilteredDatabaseModel::_update_index(const QModelIndex &src_ind, const QRegExp &rx, const FilterInfo_t &fi)
{
    bool row_matches = false;
    bool show_anyway = false;
    DatabaseModel *m = _get_database_model();

    // First figure out if a child matches
    // Searching causes us to load the entire model
    if(sourceModel()->canFetchMore(src_ind))
        sourceModel()->fetchMore(src_ind);

    int rc = sourceModel()->rowCount(src_ind);
    for(int i = 0; i < rc; ++i)
    {
        show_anyway |= _update_index(sourceModel()->index(i, 0, src_ind), rx, fi);
    }

    // Figure out if this row matches
    if(src_ind.isValid())
    {
        Entry const &e = *m->GetEntryFromIndex(src_ind);
        bool matches_time = true;

        // If it's outside the given date range then it doesn't match
        if(fi.StartTime.isValid() || fi.EndTime.isValid())
        {
            if(fi.StartTime.isValid() && fi.EndTime.isValid())
                matches_time = fi.StartTime <= e.GetModifyDate() &&
                        e.GetModifyDate() <= fi.EndTime;
            else if(fi.StartTime.isValid())
                matches_time = fi.StartTime <= e.GetModifyDate();
            else
                matches_time = e.GetModifyDate() <= fi.EndTime;
        }

        if(matches_time)
        {
            if(rx.pattern().isEmpty())
                show_anyway = true;
            else
            {
                row_matches =
                        -1 != e.GetName().indexOf(rx) ||
                        -1 != e.GetDescription().indexOf(rx);

                for(int i = 0; !row_matches && i < e.Values().count(); ++i)
                {
                    row_matches = -1 != e.Values()[i].GetNotes().indexOf(rx);
                }
            }
        }

        m_index.insert(e.GetId(), filtered_state_t(show_anyway, row_matches));
    }

    return row_matches || show_anyway;
}

bool FilteredDatabaseModel::filterAcceptsRow(int src_row, const QModelIndex &src_par) const
{
    bool ret = true;
    if(!m_index.isEmpty())
    {
        Entry const *e = _get_database_model()->
                GetEntryFromIndex(sourceModel()->index(src_row, 0, src_par));

        if(m_index.contains(e->GetId())){
            filtered_state_t fs = m_index[e->GetId()];
            ret = fs.show_anyway | fs.row_matches;
        }
    }
    return ret;
}

QModelIndexList FilteredDatabaseModel::GetUnfilteredRows() const
{
    QModelIndexList ret;
    if(!m_index.isEmpty())
    {
        DatabaseModel *m = _get_database_model();
        foreach(const EntryId &eid, m_index.keys())
        {
            if(m_index[eid].row_matches)
            {
                QModelIndex src_ind = m->FindIndexById(eid);
                if(src_ind.isValid())
                    ret.append(mapFromSource(src_ind));
            }
        }
    }
    return ret;
}


END_NAMESPACE_GRYPTO;
