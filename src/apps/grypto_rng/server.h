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

#ifndef SERVER_H
#define SERVER_H

#include <gutil/consoleiodevice.h>
#include <gutil/cryptopp_rng.h>
#include <QObject>

class trie_node_t;

class Server : public QObject
{
    Q_OBJECT
    GUtil::CryptoPP::RNG m_rng;
    GUtil::Qt::ConsoleIODevice m_console;
    trie_node_t *m_commandTrie;
    QString m_lastCommandText;
public:
    explicit Server(QObject *parent = 0);
    ~Server();

signals:
    void ActivateMainWindow();

private slots:
    void _new_data_ready();

private:
    void _process_command(const QString &);

    void _roll(int n, int min, int max);
    void _succeed(int n, double p);
    void _uniform(int n, double min, double max);
    void _normal(int n, double mean, double stdev);
    void _normal_discrete(int n, double mean, double stdev);
    void _geometric(int n, double e);
    void _exponential(int n, double l);
    void _poisson(int n, double e);

    void _show_commands();
};

#endif // SERVER_H
