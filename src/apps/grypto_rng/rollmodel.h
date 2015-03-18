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

#ifndef ROLLMODEL_H
#define ROLLMODEL_H

#include <gutil/cryptopp_rng.h>
#include <vector>
#include <QAbstractTableModel>

/** Simulates rolling a set of dice.

    You roll the dice by calling Roll() and passing it the minimum and maximum values, as well
    as the number of dice in the set. The values are picked randomly with a uniform distribution
    between the limits, and the model will be updated.

    You can get the sum of all dice from the Total() method.
*/
class RollModel :
        public QAbstractTableModel
{
    Q_OBJECT
public:
    RollModel(QObject * = 0);
    virtual ~RollModel();

    /** Returns the sum of all the dice rolled. */
    GINT64 Total() const{ return m_total; }

    int Max() const{ return m_max; }
    int Min() const{ return m_min; }
    double Mean() const{ return m_mean; }
    double Median() const{ return m_median; }
    const QList<int> &Mode() const{ return m_mode; }
    uint ModeCount() const{ return m_modeCount; }

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int, Qt::Orientation, int = Qt::DisplayRole) const;
    virtual int rowCount(const QModelIndex & = QModelIndex()) const;
    virtual int columnCount(const QModelIndex & = QModelIndex()) const;

public slots:

    /** Clears the previous results and rolls the given number of times.
        The results will all be contained in the range [min, max]
    */
    void Roll(int min = 1, int max = 6, int times = 1);

    /** Clears the model. */
    void Clear();

private:
    GUtil::CryptoPP::RNG m_rng;
    std::vector<int> m_data;
    GINT64 m_total;
    int m_min;
    int m_max;
    double m_mean;
    double m_median;
    QList<int> m_mode;
    uint m_modeCount;
};

#endif // ROLLMODEL_H
