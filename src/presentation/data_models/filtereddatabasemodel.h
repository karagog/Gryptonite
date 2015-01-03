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

#ifndef FILTEREDDATABASEMODEL_H
#define FILTEREDDATABASEMODEL_H

#include "grypto_common.h"
#include <QDateTime>
#include <QSortFilterProxyModel>

namespace Grypt{

class DatabaseModel;


/** The info needed to filter entries. */
struct FilterInfo_t
{
    const bool IsValid;

    QString SearchString;
    bool IgnoreCase;

    enum StringType{
        Wildcard,
        RegExp
    } SearchStringType;

    QDateTime StartTime;
    QDateTime EndTime;

    /** Constructs an invalid filter. */
    FilterInfo_t()
        :IsValid(false) {}

    /** Constructs a valid filter. */
    FilterInfo_t(const QString &ss, bool ignore_case, StringType type)
        :IsValid(true), SearchString(ss), IgnoreCase(ignore_case), SearchStringType(type) {}

};


class FilteredDatabaseModel :
        public QSortFilterProxyModel
{
    Q_OBJECT

    struct filtered_state_t{
        bool show_anyway;
        bool row_matches;
        filtered_state_t() :show_anyway(false), row_matches(false) {}
        filtered_state_t(bool cm, bool rm) :show_anyway(cm), row_matches(rm) {}
    };

    QHash<EntryId, filtered_state_t> m_index;
public:

    explicit FilteredDatabaseModel(QObject *parent = 0);

    /** Only database models are allowed. */
    virtual void setSourceModel(QAbstractItemModel *);

    /** Returns a list of indexes whose rows are not filtered.
     *  The list is only valid if there is a valid filter applied.
    */
    QModelIndexList GetUnfilteredRows() const;


public slots:

    /** Only use this function when setting a filter. */
    void SetFilter(const Grypt::FilterInfo_t &);


protected:

    virtual bool filterAcceptsRow(int, const QModelIndex &) const;


private:

    DatabaseModel *_get_database_model() const;

    bool _update_index(const QModelIndex &src_ind, const QRegExp &, const FilterInfo_t &);

};


}

#endif // FILTEREDDATABASEMODEL_H
