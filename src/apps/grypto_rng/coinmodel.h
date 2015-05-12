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

#ifndef COINMODEL_H
#define COINMODEL_H

#include <gutil/cryptopp_rng.h>
#include <vector>
#include <QAbstractTableModel>
#include <QFuture>
#include <QMutex>

class CoinModel :
        public QAbstractTableModel
{
    Q_OBJECT
public:
    CoinModel(QObject * = 0);
    virtual ~CoinModel();

    uint Heads() const{ return m_heads; }
    uint Tails() const{ return m_tails; }

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int, Qt::Orientation, int = Qt::DisplayRole) const;
    virtual int rowCount(const QModelIndex & = QModelIndex()) const;
    virtual int columnCount(const QModelIndex & = QModelIndex()) const;
    virtual bool canFetchMore(const QModelIndex & = QModelIndex()) const;
    virtual void fetchMore(const QModelIndex & = QModelIndex());

public slots:

    /** Flips a coin as many times as given*/
    void Flip(int times = 1);

    /** Clears the model. */
    void Clear();

signals:

    void NotifyProgressUpdated(int progress);

private:
    // These members are used only by the main thread
    QFuture<void> m_worker;
    int m_lazySize;

    // These members are used by both the main and worker threads
    QScopedPointer<QMutex> m_lock;
    std::vector<bool> m_data;
    uint m_heads;
    uint m_tails;

    // These members are only used by the worker thread
    GUtil::CryptoPP::RNG m_rng;

    void _worker_flip(int times);
    bool _can_fetch_more(const QModelIndex &) const;
    void _fail_if_worker_busy();
};

#endif // COINMODEL_H
