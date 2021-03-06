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

#include "server.h"
#include "about.h"
#include <QHash>
#include <QStringList>
#include <QCoreApplication>
using namespace std;

enum command_enum{
    roll_command        = 0,
    succeed_command     = 1,
    uniform_command     = 2,
    normal_command      = 3,
    geometric_command   = 4,
    exponential_command = 5,
    poisson_command     = 6,

    help_command        = 7,
    activate_command    = 8,
    quit_command        = 9,

    no_command          = -1,
    ambiguous_command   = -2,
    repeat_command      = -3
};

const char *__command_strings[] = {
    "roll",
    "succeed",
    "uniform",
    "normal",
    "geometric",
    "exponential",
    "poisson",
    "help",
    "activate",
    "quit"
};

struct trie_node_t
{
    command_enum command = no_command;
    QHash<char, trie_node_t *> children;

    ~trie_node_t(){ for(auto c : children) delete c; }
};

void __trie_insert(trie_node_t *n, const char *s, int len, command_enum c)
{
    // If len == 0 then we're at the insert node
    if(len == 0){
        // Enforce only unique keys
        GASSERT(no_command == n->command);
        n->command = c;
    }
    else{
        // Else we need to keep searching
        auto iter = n->children.find(*s);
        if(iter == n->children.end())
            iter = n->children.insert(*s, new trie_node_t);
        __trie_insert(*iter, s + 1, len - 1, c);
    }
}

Server::Server(QObject *parent)
    :QObject(parent),
      m_commandTrie(new trie_node_t)
{
    // Populate the command trie for fast lookups
    for(uint i = 0; i < sizeof(__command_strings)/sizeof(const char *); i++){
        __trie_insert(m_commandTrie, __command_strings[i], strlen(__command_strings[i]), (command_enum)i);
    }
    connect(&m_console, SIGNAL(ReadyRead()), this, SLOT(_new_data_ready()));

    m_console.WriteLine(tr(GRYPTO_RNG_APP_NAME " v" GRYPTO_VERSION_STRING " by George Karagoulis"));
    m_console.WriteLine(tr("\t(For a list of commands, type 'help')"));
}

Server::~Server()
{
    delete m_commandTrie;
}

void Server::_new_data_ready()
{
    while(m_console.HasDataAvailable()){
        _process_command(m_console.ReadLine().toLower());
    }
}

static command_enum __parse_command(trie_node_t *n, const char *cmd, int len)
{
    command_enum ret = no_command;
    if(0 == len){
        // auto-complete the command, as long as it's unambiguous
        trie_node_t *cur = n;
        while(cur->children.size() == 1)
            cur = *cur->children.begin();

        if(0 == cur->children.size()){
            GASSERT(cur->command != no_command);
            ret = cur->command;
        }
        else{
            ret = ambiguous_command;
        }
    }
    else{
        auto iter = n->children.find(*cmd);
        if(iter != n->children.end())
            ret = __parse_command(*iter, cmd+1, len-1);
    }
    return ret;
}

void Server::_process_command(const QString &text)
{
    QStringList args = text.split(' ', QString::SkipEmptyParts);
    QByteArray cmd, s;
    if(0 < args.length())
        cmd = args[0].toLatin1();

    command_enum ce = repeat_command;
    if(0 < cmd.length()){
        // Search for the command in the Trie, accepting all unambiguous abbreviations
        ce = __parse_command(m_commandTrie, cmd.constData(), cmd.length());
        s = text.right(text.length() - cmd.length()).trimmed().toLatin1();
    }

    bool command_executed = false;
    switch(ce)
    {
    case roll_command:
    {
        int n = 1, min = 1, max = 100;
        int cnt = sscanf(s.constData(), "%d %d %d", &min, &max, &n);
        if((cnt == EOF || 0 == cnt || cnt >= 2) && min <= max){
            _roll(n, min, max);
            command_executed = true;
        }
    }
        break;
    case succeed_command:
    {
        double p = 0.5;
        int n = 1;
        sscanf(s.constData(), "%lf %d", &p, &n);
        if(0.0 <= p && p <= 1.0){
            _succeed(n, p);
            command_executed = true;
        }
    }
        break;
    case uniform_command:
    {
        int n = 1;
        double min = 0.0, max = 1.0;
        int cnt = sscanf(s.constData(), "%lf %lf %d", &min, &max, &n);
        if((cnt == EOF || 0 == cnt || cnt >= 2) && min <= max){
            _uniform(n, min, max);
            command_executed = true;
        }
    }
        break;
    case normal_command:
    {
        int n = 1, discrete = 0;
        double mean = 0, stdev = 1;
        int cnt = sscanf(s.constData(), "%lf %lf %d %d", &mean, &stdev, &discrete, &n);
        if((cnt == EOF || 0 == cnt || cnt >= 2) && 0 == discrete){
            _normal(n, mean, stdev);
            command_executed = true;
        }
        else{
            _normal_discrete(n, mean, stdev);
            command_executed = true;
        }
    }
        break;
    case geometric_command:
    {
        int n = 1;
        double e;
        int cnt = sscanf(s.constData(), "%lf %d", &e, &n);
        if(cnt >= 1 && 1.0 <= e){
            _geometric(n, e);
            command_executed = true;
        }
    }
        break;
    case exponential_command:
    {
        int n = 1;
        double l;
        int cnt = sscanf(s.constData(), "%lf %d", &l, &n);
        if(cnt >= 1 && 0 < l){
            _exponential(n, l);
            command_executed = true;
        }
    }
        break;
    case poisson_command:
    {
        int n = 1;
        double e;
        int cnt = sscanf(s.constData(), "%lf %d", &e, &n);
        if(cnt >= 1 && 0 < e){
            _poisson(n, e);
            command_executed = true;
        }
    }
        break;
    case help_command:
        _show_commands();
        m_lastCommandText.clear();
        break;
    case activate_command:
        emit ActivateMainWindow();
        break;
    case quit_command:
        qApp->exit();
        break;
    case repeat_command:
        if(0 < m_lastCommandText.length())
            _process_command(m_lastCommandText);
        break;
    case ambiguous_command:
        // \todo Show the commands which could be autocompleted (right now there can be none, so don't worry about it)
    case no_command:
    default:
        m_lastCommandText.clear();
        break;
    }

    if(command_executed)
        m_lastCommandText = text;
}

void Server::_roll(int n, int min, int max)
{
    for(int i = 0; i < n; i++)
        m_console.WriteLine(QString("%1").arg(m_rng.U_Discrete(min, max)));
}

void Server::_succeed(int n, double p)
{
    for(int i = 0; i < n; i++)
        m_console.WriteLine(QString("%1").arg(m_rng.Succeed(p) ? 1 : 0));
}

void Server::_uniform(int n, double min, double max)
{
    for(int i = 0; i < n; i++)
        m_console.WriteLine(QString("%1").arg(m_rng.U(min, max), 0, 'f'));
}

void Server::_normal(int n, double mean, double stdev)
{
    int i;
    for(i = 0; i < (n-1); i+=2){
        auto p = m_rng.N2(mean, stdev);
        m_console.WriteLine(QString("%1\n%2").arg(p.First, 0, 'f').arg(p.Second, 0, 'f'));
    }
    if(i == n - 1)
        m_console.WriteLine(QString("%1").arg(m_rng.N(mean, stdev), 0, 'f'));
}

void Server::_normal_discrete(int n, double mean, double stdev)
{
    int i;
    for(i = 0; i < (n-1); i+=2){
        auto p = m_rng.N_Discrete2(mean, stdev);
        m_console.WriteLine(QString("%1\n%2").arg(p.First).arg(p.Second));
    }
    if(i == n - 1)
        m_console.WriteLine(QString("%1").arg(m_rng.N_Discrete(mean, stdev)));
}

void Server::_geometric(int n, double e)
{
    for(int i = 0; i < n; i++)
        m_console.WriteLine(QString("%1").arg(m_rng.Geometric(e)));
}

void Server::_exponential(int n, double l)
{
    for(int i = 0; i < n; i++)
        m_console.WriteLine(QString("%1").arg(m_rng.Exponential(l), 0, 'f'));
}

void Server::_poisson(int n, double e)
{
    for(int i = 0; i < n; i++)
        m_console.WriteLine(QString("%1").arg(m_rng.Poisson(e)));
}

void Server::_show_commands()
{
    m_console.WriteLine(tr("Command List:"));

    m_console.WriteLine(tr("roll MIN=1 MAX=100 N=1\n"
                           "\tDiscrete uniform distribution in the range [MIN, MAX]\n\n"));

    m_console.WriteLine(tr("uniform MIN=0 MAX=1 N=1\n"
                           "\tContinuous uniform distribution in the range [MIN, MAX]\n\n"));

    m_console.WriteLine(tr("succeed P=0.5 N=1\n"
                           "\tTrial where the probability of a 1 is P\n"
                           "\tP is a floating point value between [0, 1]\n\n"));

    m_console.WriteLine(tr("normal MEAN=0 STDEV=1 DISCRETE=0 N=1\n"
                           "\tNormal (Gaussian) distribution\n"
                           "\tDISCRETE is a boolean (0 or 1) that changes continuous or discrete results\n\n"));

    m_console.WriteLine(tr("geometric E N=1\n"
                           "\tGeometric distribution with expected value E\n\n"));

    m_console.WriteLine(tr("exponential L N=1\n"
                           "\tExponential distribution with rate parameter L (lambda)\n\n"));

    m_console.WriteLine(tr("poisson E N=1\n"
                           "\tPoisson distribution with expected value E\n\n"));
}
