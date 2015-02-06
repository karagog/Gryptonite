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

#include "passworddatabase.h"
#include "xmlconverter.h"
#include <grypto_entry.h>
#include <gutil/cryptopp_rng.h>
#include <gutil/gpsutils.h>
#include <gutil/databaseutils.h>
#include <gutil/sourcesandsinks.h>
#include <gutil/qtsourcesandsinks.h>
#include <gutil/smartpointer.h>
#include <gutil/file.h>
#include <gutil/thread.h>
#include <queue>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <QString>
#include <QSet>
#include <QFileInfo>
#include <QResource>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDomDocument>
#include <QLockFile>
USING_NAMESPACE_GUTIL1(Qt);
USING_NAMESPACE_GUTIL1(CryptoPP);
USING_NAMESPACE_GUTIL;
using namespace std;

// Upload files in chunks of this size
#define DEFAULT_CHUNK_SIZE  65535

// The length of the salt field of the database
#define SALT_LENGTH 32

// The length of the nonce used by the cryptor
#define NONCE_LENGTH 10

#define GRYPTO_DATABASE_VERSION "3.0.0"

#define MAX_TRY_COUNT 20

namespace Grypt{
class bg_worker_command;
}

// A cached entry row from the database
struct entry_cache{
    Grypt::EntryId id, parentid;
    Grypt::FileId file_id;
    uint row;
    int favoriteindex;
    QByteArray crypttext;
    bool exists = true;

    entry_cache() {}

    // Sets everything but the crypttext
    entry_cache(const Grypt::Entry &e)
        :id(e.GetId()),
          parentid(e.GetParentId()),
          file_id(e.GetFileId()),
          row(e.GetRow()),
          favoriteindex(e.GetFavoriteIndex())
    {}
};

// Cached parent information
struct parent_cache{
    Vector<Grypt::EntryId> children;
};

struct file_cache{
    Grypt::FileId id;
    uint length;

    file_cache() :length(0) {}
};

namespace
{
struct d_t
{
    // Main thread member variables
    QString dbString;
    GUtil::SmartPointer<GUtil::CryptoPP::Cryptor> cryptor;

    // Background threads
    thread file_worker;
    thread entry_worker;

    // Variables for the background threads
    mutex file_thread_lock;
    condition_variable wc_file_thread;
    queue<Grypt::bg_worker_command *> file_thread_commands;
    bool cancel_file_thread;
    bool file_thread_idle;
    bool file_thread_cancellable;

    mutex entry_thread_lock;
    condition_variable wc_entry_thread;
    queue<Grypt::bg_worker_command *> entry_thread_commands;
    bool entry_thread_idle;

    bool closing;

    // The index variables are maintained by both the main thread and
    //  the background threads.
    unordered_map<Grypt::EntryId, entry_cache> index;
    unordered_map<Grypt::EntryId, parent_cache> parent_index;
    unordered_map<Grypt::FileId, file_cache> file_index;
    QSet<Grypt::EntryId> deleted_entries;
    QList<Grypt::EntryId> favorite_index;
    mutex index_lock;
    condition_variable wc_index;

    d_t()
        :cancel_file_thread(false),
          file_thread_idle(true),
          file_thread_cancellable(false),
          entry_thread_idle(false),
          closing(false)
    {}
};
}


// You have to authenticate this very important message in the
//  database's keycheck field to unlock it.
static const char *__keycheck_string = "George is the greatest!";

static void __init_sql_resources()
{
    Q_INIT_RESOURCE(sql);
}

NAMESPACE_GRYPTO;
typedef pair<FileId, quint32> fileid_length_pair;


// Adds any new files in the entry to the database
// Returns a file id if a new one was created. Otherwise returns a null one.
static void __add_new_files(PasswordDatabase *pdb, const Entry &e)
{
    if(!e.GetFileId().IsNull() && !e.GetFilePath().isEmpty()){
        pdb->AddUpdateFile(e.GetFileId(), e.GetFilePath().toUtf8().constData());
    }
}

static int __count_entries_by_parent_id(QSqlQuery &q, const EntryId &id)
{
    int ret = 0;
    bool isnull = id.IsNull();
    q.prepare(QString("SELECT COUNT(*) FROM Entry WHERE ParentID%1")
              .arg(isnull ? " IS NULL" : "=?"));
    if(!isnull)
        q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    if(q.next())
        ret = q.record().value(0).toInt();
    return ret;
}

static entry_cache __convert_record_to_entry_cache(const QSqlRecord &rec)
{
    entry_cache ret;
    ret.parentid = rec.value("ParentID").toByteArray();
    ret.id = rec.value("ID").toByteArray();
    ret.row = rec.value("Row").toInt();
    ret.favoriteindex = rec.value("Favorite").toInt();
    ret.crypttext = rec.value("Data").toByteArray();
    ret.file_id = rec.value("FileID").toByteArray();
    return ret;
}

static Entry __convert_cache_to_entry(const entry_cache &er, Cryptor &cryptor)
{
    QByteArray pt;
    pt.reserve(er.crypttext.length() - cryptor.TagLength - cryptor.GetNonceSize());
    {
        QByteArrayOutput ba_out(pt);
        QByteArrayInput bai_ct(er.crypttext);
        cryptor.DecryptData(&ba_out, &bai_ct);
    }
    Entry ret = XmlConverter::FromXmlString<Entry>(pt);

    ret.SetParentId(er.parentid);
    ret.SetId(er.id);
    ret.SetRow(er.row);
    ret.SetFavoriteIndex(er.favoriteindex);
    ret.SetFileId(er.file_id);
    return ret;
}

static entry_cache __fetch_entry_row_by_id(QSqlQuery &q, const EntryId &id)
{
    entry_cache ret;
    q.prepare("SELECT * FROM Entry WHERE Id=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    if(q.next())
        ret = __convert_record_to_entry_cache(q.record());
    return ret;
}

static vector<entry_cache> __fetch_entry_rows_by_parentid(QSqlQuery &q, const EntryId &id)
{
    vector<entry_cache> ret;
    bool isnull = id.IsNull();
    q.prepare(QString("SELECT * FROM Entry WHERE ParentID%1 ORDER BY Row ASC")
              .arg(isnull ? " IS NULL" : "=?"));
    if(!isnull)
        q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    while(q.next())
        ret.emplace_back(__convert_record_to_entry_cache(q.record()));
    return ret;
}

static Vector<Entry> __find_entries_by_parent_id(QSqlQuery &q,
                                                GUtil::CryptoPP::Cryptor &cryptor,
                                                const EntryId &id)
{
    Vector<Entry> ret;
    foreach(const entry_cache &er, __fetch_entry_rows_by_parentid(q, id))
        ret.PushBack(__convert_cache_to_entry(er, cryptor));
    return ret;
}

static void __delete_file_by_id(const QString &conn_str, const FileId &id)
{
    QSqlQuery q(QSqlDatabase::database(conn_str));
    q.prepare("DELETE FROM File WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
}

// Generates a random nonce
static void __get_nonce(byte nonce[NONCE_LENGTH])
{
    GlobalRNG()->Fill(nonce, NONCE_LENGTH);
}


class bg_worker_command
{
public:
    enum CommandTypeEnum
    {
        // Entry commands
        AddEntry,
        EditEntry,
        DeleteEntry,
        MoveEntry,
        RefreshFavoriteEntries,
        SetFavoriteEntries,
        AddFavoriteEntry,
        RemoveFavoriteEntry,

        // File commands
        UpdateFile,
        DeleteFile,
        ExportFile,
        ExportToPS,
        ImportFromPS,

        // Misc commands
        DispatchOrphans
    } CommandType;

    virtual ~bg_worker_command(){}
protected:
    bg_worker_command(CommandTypeEnum c) :CommandType(c) {}
};

class add_entry_command : public bg_worker_command
{
public:
    add_entry_command(const Entry &e)
        :bg_worker_command(AddEntry),
          entry(e)
    {}
    const Entry entry;
};

class update_entry_command : public bg_worker_command
{
public:
    update_entry_command(const Entry &e)
        :bg_worker_command(EditEntry),
          entry(e)
    {}
    const Entry entry;
};

class delete_entry_command : public bg_worker_command
{
public:
    delete_entry_command(const EntryId &id)
        :bg_worker_command(DeleteEntry),
          Id(id)
    {}
    const EntryId Id;
};

class move_entry_command : public bg_worker_command
{
public:
    move_entry_command(const EntryId &parentId_src, quint32 row_first, quint32 row_last,
                       const EntryId &parentId_dest, quint32 row_dest)
        :bg_worker_command(MoveEntry),
          ParentSource(parentId_src), ParentDest(parentId_dest),
          RowFirst(row_first), RowLast(row_last), RowDest(row_dest)
    {}
    const EntryId ParentSource, ParentDest;
    quint32 RowFirst, RowLast, RowDest;
};

class refresh_favorite_entries_command : public bg_worker_command
{
public:
    refresh_favorite_entries_command() :bg_worker_command(RefreshFavoriteEntries){}
};

class set_favorite_entries_command : public bg_worker_command
{
public:
    set_favorite_entries_command(const QList<EntryId> &favs)
        :bg_worker_command(SetFavoriteEntries),
          Favorites(favs)
    {}

    const QList<EntryId> Favorites;
};

class add_favorite_entry : public bg_worker_command
{
public:
    add_favorite_entry(const EntryId &id)
        :bg_worker_command(AddFavoriteEntry),
          ID(id)
    {}
    const EntryId ID;
};

class remove_favorite_entry : public bg_worker_command
{
public:
    remove_favorite_entry(const EntryId &id)
        :bg_worker_command(RemoveFavoriteEntry),
          ID(id)
    {}
    const EntryId ID;
};

class dispatch_orphans_command : public bg_worker_command
{
public:
    dispatch_orphans_command()
        :bg_worker_command(DispatchOrphans)
    {}
};

class update_file_command : public bg_worker_command
{
public:
    update_file_command(const FileId &id, const char *path)
        :bg_worker_command(UpdateFile),
          ID(id),
          FilePath(path)
    {}
    update_file_command(const FileId &id, const QByteArray &contents)
        :bg_worker_command(UpdateFile),
          ID(id),
          FileContents(contents)
    {}
    const FileId ID;
    const QByteArray FilePath;
    const QByteArray FileContents;
};

class export_file_command : public bg_worker_command
{
public:
    export_file_command(const FileId &id, const char *path)
        :bg_worker_command(ExportFile),
          ID(id),
          FilePath(path)
    {}
    const FileId ID;
    const QByteArray FilePath;
};

class delete_file_command : public bg_worker_command
{
public:
    delete_file_command(const FileId &id)
        :bg_worker_command(DeleteFile),
          ID(id)
    {}
    const FileId ID;
};

class export_to_ps_command : public bg_worker_command
{
public:
    export_to_ps_command(const char *filepath, const Credentials &creds)
        :bg_worker_command(ExportToPS),
          FilePath(filepath),
          Creds(creds)
    {}
    const QByteArray FilePath;
    Credentials Creds;
};

class import_from_ps_command : public bg_worker_command
{
public:
    import_from_ps_command(const char *filepath, const Credentials &creds)
        :bg_worker_command(ImportFromPS),
          FilePath(filepath),
          Creds(creds)
    {}
    const QByteArray FilePath;
    Credentials Creds;
};


// Creates a database connection and returns the qt connection string
static QString __create_connection(const char *file_path, const QString &force_conn_str = QString())
{
    QString conn_str(force_conn_str.isEmpty() ? Id<10>::NewId().ToString16().ToQString() :
                                                force_conn_str);
    try{
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_str);
        db.setDatabaseName(file_path);
        if(!db.open())
            throw Exception<>(QString("Invalid Database: %1").arg(db.lastError().text()).toUtf8());
    } catch(...) {
        QSqlDatabase::removeDatabase(conn_str);
        throw;
    }
    return conn_str;
}

void PasswordDatabase::_init_cryptor(const Credentials &creds, const byte *s, GUINT32 s_l)
{
    G_D;
    GASSERT(!d->cryptor);
    d->cryptor = new GUtil::CryptoPP::Cryptor(creds, NONCE_LENGTH, new Cryptor::DefaultKeyDerivation(s, s_l));
}

static void __queue_entry_command(d_t *d, bg_worker_command *cmd)
{
    unique_lock<mutex> lkr(d->entry_thread_lock);
    d->entry_thread_commands.push(cmd);
    d->entry_thread_idle = false;
    d->wc_entry_thread.notify_one();
}

static void __check_version(const QString &dbstring)
{
    QSqlQuery q(QSqlDatabase::database(dbstring));
    q.prepare("SELECT Version FROM Version");
    DatabaseUtils::ExecuteQuery(q);

    int cnt = 0;
    while(q.next()){
        if(++cnt > 1)
            throw Exception<>("Found multiple version rows; there should be exactly one");

        QString ver = q.record().value("Version").toString();
        if(ver != GRYPTO_DATABASE_VERSION)
            throw Exception<>(String::Format("Wrong database version: %s", ver.toUtf8().constData()));
    }
    if(cnt == 0)
        throw Exception<>("Did not find a version row");
}

void PasswordDatabase::ValidateDatabase(const char *filepath)
{
    if(!File::Exists(filepath))
        throw Exception<>(String::Format("File does not exist: %s", filepath));

    // This will fail with an exception if the database is invalid sqlite
    QString dbstring = __create_connection(filepath);

    // Make sure that all the tables are there
    QStringList tables = QSqlDatabase::database(dbstring).tables();
    const char *expected_tables[] = {
        "Version", "Entry", "File"
    };
    for(uint i = 0; i < sizeof(expected_tables)/sizeof(const char *); ++i){
        if(!tables.contains(expected_tables[i], ::Qt::CaseInsensitive))
            throw Exception<>(String::Format("Table not found: %s", expected_tables[i]));
    }

    // Now check the version string
    __check_version(dbstring);

    QSqlDatabase::removeDatabase(dbstring);
}

PasswordDatabase::PasswordDatabase(const char *file_path,
                                   function<bool(const ProcessInfo &)> ask_for_lock_override,
                                   QObject *par)
    :QObject(par),
      m_filepath(file_path)
{
    G_D_INIT();

    // Note: Here we don't even check that the file exists, because maybe it wasn't created yet,
    //  but we can still lock the future location of the file path so it's ready when we want to create it.

    // Attempt to lock the database
    QFileInfo fi(file_path);
    m_lockfile = new QLockFile(QString("%1.LOCKFILE")
                                   .arg(fi.absoluteFilePath()));
    m_lockfile->setStaleLockTime(0);
    if(!m_lockfile->tryLock(0)){
        if(QLockFile::LockFailedError == m_lockfile->error()){
            // The file is locked by another process. Ask the user if they want to override it.
            ProcessInfo pi;
            m_lockfile->getLockInfo(&pi.ProcessId, &pi.HostName, &pi.AppName);
            if(ask_for_lock_override(pi)){
                if(!m_lockfile->removeStaleLockFile())
                    throw Exception<>("Unable to remove lockfile (is the locking process still alive?)");
                if(!m_lockfile->tryLock(0))
                    throw Exception<>("Database still locked even after removing the lockfile!");
            }
            else{
                // The LockException should be expected if the user decides not
                //  to override the lock. Otherwise expect another kind of exception.
                throw LockException<>();
            }
        }
        else{
            throw Exception<>("Unknown error while locking the database");
        }
    }
}

static void __initialize_cache(d_t *d)
{
    QSqlQuery q(QSqlDatabase::database(d->dbString));

    // First load all entries in bulk
    q.prepare("SELECT * FROM Entry");
    DatabaseUtils::ExecuteQuery(q);

    d->parent_index.emplace(EntryId::Null(), parent_cache());

    while(q.next()){
        entry_cache ec = __convert_record_to_entry_cache(q.record());

        // Add the entry to the cache
        d->index.emplace(ec.id, ec);
        d->parent_index.emplace(ec.id, parent_cache());

        // We'll sort the favorites at the end
        if(0 <= ec.favoriteindex)
            d->favorite_index.append(ec.id);
    }

    // Append each entry to its parent's child list
    for(const pair<EntryId, entry_cache> &ec : d->index){
        auto piter = d->parent_index.find(ec.second.parentid);
        if(piter != d->parent_index.end())
            piter->second.children.PushBack(ec.first);
    }

    // Sort each parent's child list
    for(auto tmp : d->parent_index){
        d->parent_index[tmp.first].children.Sort(
             [&](const EntryId &lhs, const EntryId &rhs) -> int{
                 int lhs_v = d->index[lhs].row;
                 int rhs_v = d->index[rhs].row;
                 return lhs_v < rhs_v ? -1 : 1;
             }
        );
    }

    // Then cache the file ID's
    q.prepare("SELECT ID,Length FROM File");
    DatabaseUtils::ExecuteQuery(q);

    while(q.next()){
        file_cache fc;
        fc.id = q.record().value("ID").toByteArray();
        fc.length = q.record().value("Length").toInt();
        d->file_index.emplace(fc.id, fc);
    }

    // Sort the favorite id's by their favorite index
    stable_sort(d->favorite_index.begin(),
                d->favorite_index.end(),
                [&](const EntryId &lhs, const EntryId &rhs) -> bool{
                    return d->index[lhs].favoriteindex < d->index[rhs].favoriteindex;
                }
    );
}

void PasswordDatabase::Open(const Credentials &creds)
{
    if(IsOpen())
        throw Exception<>("Database already opened");

    G_D;
    bool file_exists = File::Exists(m_filepath);
    QString dbstring = __create_connection(m_filepath);   // After we check if the file exists
    try
    {
        QSqlDatabase db = QSqlDatabase::database(dbstring);
        QSqlQuery q(db);

        if(!file_exists){
            // Initialize the new database if it doesn't exist
            __init_sql_resources();
            QResource rs(":/grypto/sql/create_db.sql");
            GASSERT(rs.isValid());

            QByteArray sql;
            if(rs.isCompressed())
                sql = qUncompress(rs.data(), rs.size());
            else
                sql = QByteArray((const char *)rs.data(), rs.size());
            DatabaseUtils::ExecuteScript(db, sql);

            // Prepare some salt and create the cryptor
            byte salt[SALT_LENGTH];
            GUtil::CryptoPP::RNG().Fill(salt, SALT_LENGTH);
            _init_cryptor(creds, salt, SALT_LENGTH);

            // Prepare the keycheck data
            QByteArray keycheck_ct;
            byte nonce[NONCE_LENGTH];
            __get_nonce(nonce);
            {
                ByteArrayInput auth_in(__keycheck_string, strlen(__keycheck_string));
                QByteArrayOutput ba_out(keycheck_ct);
                d->cryptor->EncryptData(&ba_out, NULL, &auth_in, nonce);
            }

            // Insert a version record
            q.prepare("INSERT INTO Version (Version,Salt,KeyCheck)"
                        " VALUES (?,?,?)");
            q.addBindValue(GRYPTO_DATABASE_VERSION);
            q.addBindValue(QByteArray((const char *)salt, sizeof(salt)));
            q.addBindValue(keycheck_ct);
            DatabaseUtils::ExecuteQuery(q);
        }

        // Check the version record to see if it is valid
        __check_version(dbstring);

        // Validate the keycheck information
        q.prepare("SELECT KeyCheck,Salt FROM Version");
        DatabaseUtils::ExecuteQuery(q);

        if(q.next()){
            if(!d->cryptor){
                QByteArray salt_ba = q.record().value("Salt").toByteArray();
                _init_cryptor(creds, (byte const *)salt_ba.constData(), salt_ba.length());
            }

            QByteArrayInput bai_ct(q.record().value("KeyCheck").toByteArray());
            ByteArrayInput auth_in(__keycheck_string, strlen(__keycheck_string));

            // This will throw an exception if the key was bad
            d->cryptor->DecryptData(NULL, &bai_ct, &auth_in);
        }
    }
    catch(...)
    {
        QSqlDatabase::removeDatabase(dbstring);
        d->cryptor.Clear();
        throw;
    }

    // Set the db string to denote that we've opened the file
    d->dbString = dbstring;

    // Initialize the cache before starting the workers
    __initialize_cache(d);

    // Start up the background threads
    d->entry_worker = std::thread(&PasswordDatabase::_entry_worker, this, new GUtil::CryptoPP::Cryptor(*d->cryptor));
    d->file_worker = std::thread(&PasswordDatabase::_file_worker, this, new GUtil::CryptoPP::Cryptor(*d->cryptor));

    // We must wait for the background thread to idle to avoid a race condition
    WaitForEntryThreadIdle();
}

bool PasswordDatabase::IsOpen() const
{
    G_D;
    return !d->dbString.isEmpty();
}

PasswordDatabase::~PasswordDatabase()
{
    G_D;
    if(IsOpen()){
        // Let's clean up any orphaned entries/files
        DeleteOrphans();

        d->file_thread_lock.lock();
        d->entry_thread_lock.lock();
        d->closing = true;
        d->wc_file_thread.notify_one();
        d->wc_entry_thread.notify_one();
        d->entry_thread_lock.unlock();
        d->file_thread_lock.unlock();

        d->entry_worker.join();
        d->file_worker.join();

        QSqlDatabase::removeDatabase(d->dbString);
    }

    m_lockfile->unlock();
    G_D_UNINIT();
}

bool PasswordDatabase::CheckCredentials(const Credentials &creds) const
{
    G_D;
    FailIfNotOpen();
    bool ret = false;

    // Fetch the salt from the version row
    QSqlQuery q("SELECT Salt FROM Version", QSqlDatabase::database(d->dbString));
    if(q.next()){
        QByteArray salt = q.record().value(0).toByteArray();
        ret = d->cryptor->CheckCredentials(creds);
    }
    return ret;
}

static void __commit_transaction(QSqlDatabase &db)
{
    int try_count = 0;
    while(try_count < MAX_TRY_COUNT && !db.commit()){
        try_count++;
        Thread::sleep(1);
    }
    if(MAX_TRY_COUNT == try_count)
        throw Exception<>(db.lastError().text().toUtf8().constData());
}

void PasswordDatabase::_ew_add_entry(const QString &conn_str, const Entry &e)
{
    G_D;
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QSqlQuery q(db);
    QString task_string = tr("Adding entry");
    db.transaction();
    emit NotifyProgressUpdated(0, false, task_string);
    {
        bool success = false;
        finally([&]{
            TryFinally([&]{
                if(success) __commit_transaction(db);
                else        db.rollback();
            }, [](std::exception &){},
            [&]{
                emit NotifyProgressUpdated(100, false, task_string);
            });
        });

        // Update the surrounding entries' rows before insertion
        int child_count = __count_entries_by_parent_id(q, e.GetParentId());
        int row = e.GetRow();
        if(0 > row || row > child_count)
            row = child_count;

        bool pid_isnull = e.GetParentId().IsNull();
        for(int i = child_count - 1; i >= row; --i)
        {
            q.prepare(QString("UPDATE Entry SET Row=? WHERE ParentID%1 AND Row=?")
                      .arg(pid_isnull ? " IS NULL" : "=?"));
            q.addBindValue(i + 1);
            if(!pid_isnull)
                q.addBindValue((QByteArray)e.GetParentId());
            q.addBindValue(i);
            DatabaseUtils::ExecuteQuery(q);
        }
        emit NotifyProgressUpdated(35, false, task_string);


        // Update the index immediately now that we have everything we need
        //  i.e. Do not wait to insert the actual entry
        QByteArray crypttext;
        {
            lock_guard<mutex> lkr(d->index_lock);

            // If the index is not present, then it was already deleted
            auto iter = d->index.find(e.GetId());
            if(iter == d->index.end())
                return;

            iter->second.row = row;
            crypttext = iter->second.crypttext;
        }

        q.prepare("INSERT INTO Entry (ID,ParentID,Row,Favorite,FileID,Data)"
                  " VALUES (?,?,?,?,?,?)");
        q.addBindValue((QByteArray)e.GetId());
        q.addBindValue((QByteArray)e.GetParentId());
        q.addBindValue(row);
        q.addBindValue(e.GetFavoriteIndex());
        q.addBindValue((QByteArray)e.GetFileId());
        q.addBindValue(crypttext);
        DatabaseUtils::ExecuteQuery(q);

        emit NotifyProgressUpdated(65, false, task_string);
        success = true;
    }

    if(e.IsFavorite())
        emit NotifyFavoritesUpdated();

    // Add any new files to the database
    __add_new_files(this, e);
}

void PasswordDatabase::_ew_update_entry(const QString &conn_str, const Entry &e)
{
    G_D;
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QString task_string = tr("Updating entry");
    emit NotifyProgressUpdated(25, false, task_string);
    {
        finally([&]{ emit NotifyProgressUpdated(100, false, task_string); });

        // First get the entry out of the cache
        entry_cache er;
        d->index_lock.lock();
        {
            GASSERT(d->index.find(e.GetId()) != d->index.end());
            er = d->index[e.GetId()];
        }
        d->index_lock.unlock();

        // Then update the database
        db.transaction();
        try{
            QSqlQuery q(db);
            q.prepare("UPDATE Entry SET Data=?,Favorite=?,FileID=? WHERE Id=?");
            q.addBindValue(er.crypttext);
            q.addBindValue(er.favoriteindex);
            q.addBindValue((QByteArray)e.GetFileId());
            q.addBindValue((QByteArray)e.GetId());
            DatabaseUtils::ExecuteQuery(q);
        } catch(...) {
            db.rollback();
            throw;
        }
        __commit_transaction(db);
    }

    // We never remove old files, but we may add new ones here
    __add_new_files(this, e);
}

void PasswordDatabase::_ew_delete_entry(const QString &conn_str,
                                        const EntryId &id)
{
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QSqlQuery q(db);
    QString task_string = tr("Deleting entry");
    int old_favorite_index;
    EntryId pid;
    uint row;
    uint child_count;

    db.transaction();
    emit NotifyProgressUpdated(0, false, task_string);
    {
        bool success = false;
        finally([&]{
            TryFinally([&]{
                if(success) __commit_transaction(db);
                else        db.rollback();
            }, [](std::exception &){},
            [&]{
                emit NotifyProgressUpdated(100, false, task_string);
            });
        });

        q.prepare("SELECT ParentId,Row,Favorite FROM Entry WHERE Id=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);
        if(!q.next())
            throw Exception<>("Entry not found");

        emit NotifyProgressUpdated(10, false, task_string);

        pid = q.record().value(0).toByteArray();
        row = q.record().value(1).toInt();
        old_favorite_index = q.record().value(2).toInt();

        // We have to manually count the children, because at this point the entry
        //  has been deleted from the index
        child_count = __count_entries_by_parent_id(q, pid);

        emit NotifyProgressUpdated(25, false, task_string);

        q.prepare("DELETE FROM Entry WHERE ID=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);

        emit NotifyProgressUpdated(65, false, task_string);

        // Do not delete any files; the user has to manually clean them up

        // Update the surrounding entries' rows after deletion
        bool pid_isnull = pid.IsNull();
        for(uint i = row + 1; i < child_count; ++i)
        {
            q.prepare(QString("UPDATE Entry SET Row=? WHERE ParentID%1 AND Row=?")
                      .arg(pid_isnull ? " IS NULL" : "=?"));
            q.addBindValue(i - 1);
            if(!pid_isnull)
                q.addBindValue((QByteArray)pid);
            q.addBindValue(i);
            DatabaseUtils::ExecuteQuery(q);
        }

        success = true;
    }

    if(old_favorite_index >= 0)
        emit NotifyFavoritesUpdated();
}

void PasswordDatabase::_ew_move_entry(const QString &conn_str,
                                      const EntryId &src_parent, quint32 row_first, quint32 row_last,
                                      const EntryId &dest_parent, quint32 row_dest)
{
    int row_cnt = row_last - row_first + 1;
    GASSERT(row_cnt > 0);

    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QSqlQuery q(db);
    QString task_string = tr("Moving entry");
    db.transaction();
    emit NotifyProgressUpdated(0, false, task_string);
    {
        bool success = false;
        finally([&]{
            TryFinally([&]{
                if(success) __commit_transaction(db);
                else        db.rollback();
            }, [](std::exception &){},
            [&]{
                emit NotifyProgressUpdated(100, false, task_string);
            });
        });

        QByteArray special_zero_id(EntryId::Size, (char)0);

        // Have to make a special adjustment in case moving lower in the same parent
        if(src_parent == dest_parent && row_dest > row_last){
            row_dest -= row_cnt;
        }

        // Remember the original sibling count before we move anything
        int src_siblings_cnt = __count_entries_by_parent_id(q, src_parent);

        // First move the source rows to a dummy parent with ID=0
        for(int i = 0; i < row_cnt; ++i){
            q.prepare(QString("UPDATE Entry SET ParentID=?,Row=? WHERE ParentID%1 AND Row=?")
                      .arg(src_parent.IsNull() ? " IS NULL" : "=?"));
            q.addBindValue(special_zero_id);
            q.addBindValue(row_dest + i);
            if(!src_parent.IsNull())
                q.addBindValue(src_parent.ToQByteArray());
            q.addBindValue(row_first + i);
            DatabaseUtils::ExecuteQuery(q);
        }

        emit NotifyProgressUpdated(15, false, task_string);

        // Count the siblings at the dest. We do this later because the source
        //  may be the dest, in which case we need to count after removing the source rows.
        int dest_siblings_cnt = __count_entries_by_parent_id(q, dest_parent);

        // Update the siblings at the source
        for(int i = row_last + 1; i < src_siblings_cnt; ++i){
            q.prepare(QString("UPDATE Entry SET Row=? WHERE ParentID%1 AND Row=?")
                      .arg(src_parent.IsNull() ? " IS NULL" : "=?"));
            q.addBindValue(i - row_cnt);
            if(!src_parent.IsNull())
                q.addBindValue(src_parent.ToQByteArray());
            q.addBindValue(i);
            DatabaseUtils::ExecuteQuery(q);
        }

        emit NotifyProgressUpdated(40, false, task_string);

        // Update the siblings at the dest
        for(int i = dest_siblings_cnt - 1; i >= (int)row_dest; --i){
            q.prepare(QString("UPDATE Entry SET Row=? WHERE ParentID%1 AND Row=?")
                      .arg(dest_parent.IsNull() ? " IS NULL" : "=?"));
            q.addBindValue(i + row_cnt);
            if(!dest_parent.IsNull())
                q.addBindValue(dest_parent.ToQByteArray());
            q.addBindValue(i);
            DatabaseUtils::ExecuteQuery(q);
        }

        emit NotifyProgressUpdated(65, false, task_string);

        // Finally move the rows into the dest
        q.prepare("UPDATE Entry SET ParentID=? WHERE ParentID=?");
        q.addBindValue((QByteArray)dest_parent);
        q.addBindValue(special_zero_id);
        DatabaseUtils::ExecuteQuery(q);

        emit NotifyProgressUpdated(85, false, task_string);
        success = true;
    }
}

static QByteArray __generate_crypttext(Cryptor &cryptor, const Entry &e)
{
    QByteArray crypttext;

    // Get a fresh nonce
    byte nonce[NONCE_LENGTH];
    __get_nonce(nonce);

    QByteArrayInput i(XmlConverter::ToXmlString(e));
    QByteArrayOutput o(crypttext);
    cryptor.EncryptData(&o, &i, NULL, nonce);

    return crypttext;
}

void PasswordDatabase::AddEntry(Entry &e, bool gen_id)
{
    FailIfNotOpen();
    G_D;

    if(gen_id)
        e.SetId(EntryId::NewId());

    // Generate the crypttext
    QByteArray crypttext = __generate_crypttext(*d->cryptor, e);

    // Update the index
    unique_lock<mutex> lkr(d->index_lock);
    {
        // Add to the appropriate parent
        auto pi = d->parent_index.find(e.GetParentId());
        Vector<EntryId> &child_ids = pi->second.children;
        if((uint)e.GetRow() > child_ids.Length())
            e.SetRow(child_ids.Length());

        child_ids.Insert(e.GetId(), e.GetRow());

        // Adjust the siblings
        for(uint i = e.GetRow() + 1; i < child_ids.Length(); ++i){
            auto iter = d->index.find(child_ids[i]);
            if(iter != d->index.end())
                iter->second.row = i;
        }

        if(e.IsFavorite())
            d->favorite_index.append(e.GetId());

        entry_cache ec(e);
        ec.crypttext = crypttext;
        d->index[e.GetId()] = ec;
        if(d->deleted_entries.contains(e.GetId()))
            d->deleted_entries.remove(e.GetId());

        if(d->parent_index.find(e.GetId()) == d->parent_index.end())
            d->parent_index[e.GetId()] = parent_cache();
    }
    lkr.unlock();
    d->wc_index.notify_all();

    // Tell the worker thread to add it to the database
    __queue_entry_command(d, new add_entry_command(e));

    // Clear the file path so we don't add the same file twice
    e.SetFilePath(QString::null);
}

void PasswordDatabase::UpdateEntry(Entry &e)
{
    FailIfNotOpen();
    G_D;

    // Update the index
    int old_favorite_index;
    QByteArray crypttext = __generate_crypttext(*d->cryptor, e);
    {
        unique_lock<mutex> lkr(d->index_lock);

        old_favorite_index = d->index[e.GetId()].favoriteindex;

        d->index[e.GetId()] = e;
        d->index[e.GetId()].crypttext = crypttext;
        if(old_favorite_index != e.GetFavoriteIndex()){
            if(e.IsFavorite() && old_favorite_index < 0)
                d->favorite_index.append(e.GetId());
            else if(!e.IsFavorite() && old_favorite_index >= 0)
                d->favorite_index.removeOne(e.GetId());
        }
    }

    __queue_entry_command(d, new update_entry_command(e));

    // Clear the file path so we don't add the same file twice
    e.SetFilePath(QString::null);

    if(old_favorite_index != e.GetFavoriteIndex())
        emit NotifyFavoritesUpdated();
}

void PasswordDatabase::DeleteEntry(const EntryId &id)
{
    FailIfNotOpen();
    G_D;
    if(id.IsNull())
        return;

    // Update the index
    unique_lock<mutex> lkr(d->index_lock);
    auto i = d->index.find(id);
    if(i != d->index.end()){
        auto pi = d->parent_index.find(i->second.parentid);
        pi->second.children.RemoveAt(i->second.row);

        // Update the siblings of the item we just removed
        for(auto r = i->second.row; r < pi->second.children.Length(); ++r){
            auto ci = d->index.find(pi->second.children[r]);
            if(ci != d->index.end())
                ci->second.row = r;
        }

        int fi = d->favorite_index.indexOf(id);
        if(fi != -1)
            d->favorite_index.removeAt(fi);
        d->index.erase(i);
        d->deleted_entries.insert(id);

        // Don't remove from the parent index, because they may un-delete it
        //d->parent_index.erase(id);
    }
    lkr.unlock();
    d->wc_index.notify_all();

    // Remove it from the database
    __queue_entry_command(d, new delete_entry_command(id));
}

void PasswordDatabase::MoveEntries(const EntryId &parentId_src, quint32 row_first, quint32 row_last,
                                   const EntryId &parentId_dest, quint32 row_dest)
{
    FailIfNotOpen();
    G_D;
    int move_cnt = row_last - row_first + 1;
    quint32 row_dest_orig = row_dest;
    bool same_parents = parentId_src == parentId_dest;
    if(0 > move_cnt ||
            (same_parents && row_first <= row_dest && row_dest <= row_last))
        throw Exception<>("Invalid move parameters");

    // Update the index
    unique_lock<mutex> lkr(d->index_lock);

    Vector<EntryId> &src = d->parent_index.find(parentId_src)->second.children;
    Vector<EntryId> &dest = d->parent_index.find(parentId_dest)->second.children;
    Vector<EntryId> moving_rows(move_cnt);

    if(row_first >= src.Length() || row_last >= src.Length() ||
            (same_parents && row_dest > dest.Length()) ||
            (!same_parents && row_dest > dest.Length()))
        throw Exception<>("Invalid move parameters");

    // Extract the rows from the source parent
    for(quint32 i = row_first; i <= row_last; ++i){
        moving_rows.PushBack(src[row_first]);
        src.RemoveAt(row_first);
    }

    // Update the siblings at the source
    for(quint32 i = row_first; i < src.Length(); ++i)
        d->index.find(src[i])->second.row = i;

    // Insert the rows at the dest parent
    if(same_parents && row_dest > row_first)
        row_dest -= move_cnt;

    for(int i = 0; i < move_cnt; ++i){
        dest.Insert(moving_rows[i], row_dest + i);
        d->index.find(moving_rows[i])->second.parentid = parentId_dest;
    }

    // Update the siblings at the dest
    for(quint32 i = row_dest; i < dest.Length(); ++i)
        d->index.find(dest[i])->second.row = i;
    lkr.unlock();

    // Update the database
    __queue_entry_command(d, new move_entry_command(parentId_src, row_first, row_last, parentId_dest, row_dest_orig));
}

Entry PasswordDatabase::FindEntry(const EntryId &id) const
{
    FailIfNotOpen();
    auto exit_not_found = []{ throw Exception<>("Entry not found"); };
    G_D;

    entry_cache ec;
    {
        unique_lock<mutex> lkr(d->index_lock);
        if(d->deleted_entries.contains(id))
            exit_not_found();

        auto i = d->index.find(id);
        if(i == d->index.end() || !i->second.exists)
            exit_not_found();

        ec = i->second;
    }
    return __convert_cache_to_entry(ec, *d->cryptor);
}

int PasswordDatabase::CountEntriesByParentId(const EntryId &id) const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->index_lock);
    int ret = 0;
    auto pi = d->parent_index.find(id);
    if(pi != d->parent_index.end())
        ret = pi->second.children.Length();
    return ret;
}

QList<Entry> PasswordDatabase::FindEntriesByParentId(const EntryId &pid) const
{
    FailIfNotOpen();
    G_D;

    QList<Entry> ret;
    {
        unique_lock<mutex> lkr(d->index_lock);

        foreach(const EntryId &child_id, d->parent_index[pid].children)
            ret.append(__convert_cache_to_entry(d->index[child_id], *d->cryptor));
    }
    return ret;
}

void PasswordDatabase::RefreshFavorites()
{
    FailIfNotOpen();
    G_D;
    __queue_entry_command(d, new refresh_favorite_entries_command);
}

void PasswordDatabase::SetFavoriteEntries(const QList<EntryId> &favs)
{
    FailIfNotOpen();
    G_D;
    __queue_entry_command(d, new set_favorite_entries_command(favs));

    d->index_lock.lock();
    {
        // Update the favorite index
        // Remove the old favorites from the index
        for(const EntryId &eid : d->favorite_index){
            auto iter = d->index.find(eid);
            if(iter != d->index.end())
                iter->second.favoriteindex = -1;
        }

        d->favorite_index = favs;

        int ctr = 0;
        for(const EntryId &eid : favs){
            ctr++;  // Start counting at 1
            auto iter = d->index.find(eid);
            if(iter != d->index.end())
                iter->second.favoriteindex = ctr;
        }
    }
    d->index_lock.unlock();
    d->wc_index.notify_all();
    emit NotifyFavoritesUpdated();
}

void PasswordDatabase::AddFavoriteEntry(const EntryId &id)
{
    FailIfNotOpen();
    G_D;
    __queue_entry_command(d, new add_favorite_entry(id));

    d->index_lock.lock();
    {
        auto iter = d->index.find(id);
        if(iter != d->index.end()){
            d->favorite_index.prepend(id);
            iter->second.favoriteindex = 0;
        }
    }
    d->index_lock.unlock();
    d->wc_index.notify_all();
    emit NotifyFavoritesUpdated();
}

void PasswordDatabase::RemoveFavoriteEntry(const EntryId &id)
{
    FailIfNotOpen();
    G_D;
    __queue_entry_command(d, new remove_favorite_entry(id));

    unique_lock<mutex> lkr(d->index_lock);
    {
        // Find it in the index and remove it
        int ind = d->favorite_index.indexOf(id);
        if(-1 == ind)
            return;
        d->favorite_index.removeAt(ind);

        auto iter = d->index.find(id);
        int old_index = iter->second.favoriteindex;
        if(0 < old_index){
            // Adjust the other favorites' rows
            for(int i = 0; i < d->favorite_index.length() - ind; ++i)
                d->index[d->favorite_index[ind + i]].favoriteindex = old_index + i;
        }
    }
    lkr.unlock();
    d->wc_index.notify_all();
    emit NotifyFavoritesUpdated();
}

QList<Entry> PasswordDatabase::FindFavoriteEntries() const
{
    FailIfNotOpen();
    G_D;
    QList<entry_cache> rows;
    unique_lock<mutex> lkr(d->index_lock);
    for(const EntryId &id : d->favorite_index)
        rows.append(d->index[id]);
    lkr.unlock();

    QList<Entry> ret;
    for(const entry_cache &row : rows)
        ret.append(__convert_cache_to_entry(row, *d->cryptor));
    return ret;
}

QList<EntryId> PasswordDatabase::FindFavoriteIds() const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->index_lock);
    return d->favorite_index;
}

void PasswordDatabase::DeleteOrphans()
{
    FailIfNotOpen();
    G_D;
    __queue_entry_command(d, new dispatch_orphans_command);
}

bool PasswordDatabase::FileExists(const FileId &id) const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->index_lock);
    return d->file_index.find(id) != d->file_index.end();
}

void PasswordDatabase::CancelFileTasks()
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->cancel_file_thread = true;

    // Remove all pending commands
    while(!d->file_thread_commands.empty()){
        delete d->file_thread_commands.front();
        d->file_thread_commands.pop();
    }

    // Wake the thread in case he's sleeping, we want him to lower
    //  the cancel flag immediately so it doesn't cancel the next operation
    d->wc_file_thread.notify_one();
}

QHash<FileId, PasswordDatabase::FileInfo_t> PasswordDatabase::GetFileSummary() const
{
    FailIfNotOpen();
    G_D;
    QSqlQuery q("SELECT ID,Length FROM File",
                QSqlDatabase::database(d->dbString));

    QHash<FileId, FileInfo_t> ret;
    while(q.next()){
        QByteArray ba_fid = q.record().value("ID").toByteArray();
        if(ba_fid.length() == FileId::Size){
            FileId fid = ba_fid;
            FileInfo_t finfo(q.record().value("Length").toInt());
            ret.insert(fid, finfo);
        }
    }
    return ret;
}

QHash<FileId, PasswordDatabase::FileInfo_t> PasswordDatabase::GetOrphanedFiles() const
{
    FailIfNotOpen();
    const QSet<FileId> referenced_fids( GetReferencedFileIds() );
    const QHash<FileId, FileInfo_t> files( GetFileSummary() );
    QHash<FileId, FileInfo_t> ret;

    for(auto k : files.keys()){
        if(referenced_fids.contains(k))
            ret.insert(k, files[k]);
    }
    return ret;
}

static void __add_referenced_file_ids(d_t *d, QSet<FileId> &s, const EntryId &id)
{
    // Add this entry's file reference
    if(!id.IsNull()){
        auto iter = d->index.find(id);
        if(iter != d->index.end() && !iter->second.file_id.IsNull())
            s.insert(iter->second.file_id);
    }

    // Descend into this entry's children
    auto piter = d->parent_index.find(id);
    if(piter != d->parent_index.end()){
        for(const EntryId &cid : piter->second.children)
            __add_referenced_file_ids(d, s, cid);
    }
}

QSet<FileId> PasswordDatabase::GetReferencedFileIds() const
{
    FailIfNotOpen();
    WaitForEntryThreadIdle();
    G_D;

    QSet<FileId> ret;
    unique_lock<mutex> lkr(d->index_lock);
    __add_referenced_file_ids(d, ret, EntryId::Null());
    return ret;
}

void PasswordDatabase::AddUpdateFile(const FileId &id, const char *filename)
{
    FailIfNotOpen();
    G_D;
    d->index_lock.lock();
    d->file_index.erase(id);
    d->index_lock.unlock();

    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new update_file_command(id, filename));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::AddUpdateFile(const FileId &id, const QByteArray &contents)
{
    FailIfNotOpen();
    G_D;
    d->index_lock.lock();
    d->file_index.erase(id);
    d->index_lock.unlock();

    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new update_file_command(id, contents));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::DeleteFile(const FileId &id)
{
    FailIfNotOpen();
    G_D;
    d->index_lock.lock();
    d->file_index.erase(id);
    d->index_lock.unlock();

    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new delete_file_command(id));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::ExportFile(const FileId &id, const char *export_path) const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new export_file_command(id, export_path));
    d->wc_file_thread.notify_one();
}

QByteArray PasswordDatabase::GetFile(const FileId &id) const
{
    FailIfNotOpen();
    G_D;
    QByteArray ret;
    QSqlQuery q(QSqlDatabase::database(d->dbString));
    q.prepare("SELECT Data FROM File WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    if(q.next()){
        const QByteArray encrypted = q.value(0).toByteArray();
        QByteArrayInput i(encrypted);
        QByteArrayOutput o(ret);
        d->cryptor->DecryptData(&o, &i);
    }
    return ret;
}

void PasswordDatabase::ExportToPortableSafe(const char *export_filename,
                                            const Credentials &creds) const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new export_to_ps_command(export_filename, creds));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::ImportFromPortableSafe(const char *import_filename,
                                              const Credentials &creds)
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new import_from_ps_command(import_filename, creds));
    d->wc_file_thread.notify_one();
}

static void __count_child_entries(QSqlDatabase &db, int &count, const EntryId &parent_id)
{
    QSqlQuery q(db);
    QList<EntryId> child_ids;
    q.prepare(QString("SELECT ID FROM Entry WHERE ParentID%1")
              .arg(parent_id.IsNull() ? " IS NULL" : "=?"));
    if(!parent_id.IsNull())
        q.addBindValue((QByteArray)parent_id);
    DatabaseUtils::ExecuteQuery(q);

    while(q.next()){
        EntryId cid = q.value("ID").toByteArray();
        child_ids.append(cid);
        __count_child_entries(db, count, cid);
    }
    count += child_ids.length();
}

int PasswordDatabase::CountAllEntries() const
{
    FailIfNotOpen();
    G_D;
    int ret = 0;
    QSqlDatabase db(QSqlDatabase::database(d->dbString));
    db.transaction();
    finally([&]{ db.rollback(); });
    __count_child_entries(db, ret, EntryId::Null());
    return ret;
}

static void __import_children(d_t *d,
                              PasswordDatabase &master,
                              const PasswordDatabase &other,
                              const EntryId &orig_parent_id,
                              const EntryId &new_parent_id,
                              QList<QPair<FileId,FileId>> &file_list,
                              int &progress_ctr,
                              function<void()> progress_cb)
{
    QList<Entry> children = other.FindEntriesByParentId(orig_parent_id);
    for(Entry &c : children){
        // Give every entry a new id
        EntryId orig_id = c.GetId();
        c.SetId(EntryId::NewId());
        c.SetParentId(new_parent_id);
        if(!c.GetFileId().IsNull()){
            // Give every file a new id, but remember the old one so we can reference it later
            file_list.append(QPair<FileId,FileId>(c.GetFileId(), FileId::NewId()));
            c.SetFileId(file_list.back().second);
        }
        master.AddEntry(c);
        ++progress_ctr;
        progress_cb();
        __import_children(d, master, other, orig_id, c.GetId(), file_list, progress_ctr, progress_cb);
    }
}

void PasswordDatabase::ImportFromDatabase(const PasswordDatabase &other)
{
    FailIfNotOpen();
    G_D;
    QString progress_label(QString(tr("Importing entries from %1")).arg(other.FilePath()));
    emit NotifyProgressUpdated(0, false, progress_label);
    finally([&]{ emit NotifyProgressUpdated(100, false, progress_label); });

    int progress_ctr = 0;
    int entry_count = other.CountAllEntries();
    emit NotifyProgressUpdated(10, false, progress_label);

    QList<QPair<FileId, FileId>> file_list;
    __import_children(d,
                      *this,
                      other,
                      EntryId::Null(),
                      EntryId::Null(),
                      file_list,
                      progress_ctr,
                      [&]{
        emit NotifyProgressUpdated((float)progress_ctr*100/entry_count * 0.9 + 10,
                                   false,
                                   progress_label);
    });

    progress_label = QString(tr("Importing files from %1")).arg(other.FilePath());
    emit NotifyProgressUpdated(0, false, progress_label);

    int file_count = file_list.length();
    for(int i = 0; i < file_count; ++i){
        QByteArray cur_file = other.GetFile(file_list[i].first);
        AddUpdateFile(file_list[i].second, cur_file);
        emit NotifyProgressUpdated((float)i/file_count*100, false, progress_label);
    }

    RefreshFavorites();
}

void PasswordDatabase::_ew_refresh_favorites(const QString &conn_str)
{
    G_D;
    QSqlQuery q("SELECT * FROM Entry WHERE Favorite >= 0 ORDER BY Favorite ASC",
                QSqlDatabase::database(conn_str));
    QList<EntryId> ids;
    QList<entry_cache> rows;
    while(q.next()){
        rows.append(__convert_record_to_entry_cache(q.record()));
        ids.append(rows.back().id);
    }

    unique_lock<mutex> lkr(d->index_lock);
    d->favorite_index = ids;
    for(const entry_cache &ec : rows){
        if(d->index.find(ec.id) == d->index.end())
            d->index.emplace(ec.id, ec);
    }
    lkr.unlock();
    d->wc_index.notify_all();
    emit NotifyFavoritesUpdated();
}

void PasswordDatabase::_ew_set_favorites(const QString &conn_str, const QList<EntryId> &favs)
{
    QString task_string = tr("Setting favorites");
    emit NotifyProgressUpdated(0, false, task_string);
    finally([&]{ emit NotifyProgressUpdated(100, false, task_string); });

    QSqlDatabase db(QSqlDatabase::database(conn_str));
    db.transaction();
    try{
        // First clear all existing favorites
        QSqlQuery q("UPDATE Entry SET Favorite=-1", db);
        emit NotifyProgressUpdated(50, false, task_string);

        // Then apply favorites in the order they were given
        for(int i = 0; i < favs.length(); ++i){
            q.prepare("UPDATE Entry SET Favorite=? WHERE Id=?");
            q.addBindValue(i + 1);
            q.addBindValue((QByteArray)favs[i]);
            DatabaseUtils::ExecuteQuery(q);
        }
    }
    catch(...){
        db.rollback();
        throw;
    }
    __commit_transaction(db);
}

void PasswordDatabase::_ew_add_favorite(const QString &conn_str, const EntryId &id)
{
    QString task_string = tr("Adding favorite");
    emit NotifyProgressUpdated(0, false, task_string);
    finally([&]{ emit NotifyProgressUpdated(100, false, task_string); });

    QSqlQuery q(QSqlDatabase::database(conn_str));
    q.prepare("SELECT Favorite FROM Entry WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    if(!q.next())
        return;

    emit NotifyProgressUpdated(15, false, task_string);

    if(0 > q.value(0).toInt()){
        q.prepare("UPDATE Entry SET Favorite=0 WHERE ID=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);
    }
}

void PasswordDatabase::_ew_remove_favorite(const QString &conn_str, const EntryId &id)
{
    QString task_string = tr("Removing favorite");
    emit NotifyProgressUpdated(0, false, task_string);
    finally([&]{ emit NotifyProgressUpdated(100, false, task_string); });

    QSqlDatabase db(QSqlDatabase::database(conn_str));
    QSqlQuery q(db);
    entry_cache ec = __fetch_entry_row_by_id(q, id);
    if(ec.id.IsNull() || -1 == ec.favoriteindex)
        return;

    db.transaction();
    try{
        // If the favorite was a sorted index, then we have to update the other favorites
        if(0 != ec.favoriteindex){
            q.prepare("SELECT ID FROM Entry WHERE Favorite>? ORDER BY Favorite ASC");
            q.addBindValue(ec.favoriteindex);
            DatabaseUtils::ExecuteQuery(q);

            emit NotifyProgressUpdated(10, false, task_string);

            int ctr = 0;
            QSqlQuery q2(db);
            while(q.next()){
                q2.prepare("UPDATE Entry SET Favorite=? WHERE ID=?");
                q2.addBindValue(ec.favoriteindex + ctr);
                q2.addBindValue(q.value("ID").toByteArray());
                DatabaseUtils::ExecuteQuery(q2);
                ctr++;
            }
        }
        emit NotifyProgressUpdated(65, false, task_string);

        q.prepare("UPDATE Entry SET Favorite=-1 WHERE ID=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);
    }
    catch(...){
        db.rollback();
        throw;
    }
    __commit_transaction(db);
}

void PasswordDatabase::_ew_dispatch_orphans(const QString &conn_str)
{
    G_D;
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    QSqlQuery q("SELECT ID,ParentID,FileID FROM Entry", db);

    Vector<tuple<EntryId, EntryId, FileId>> ids;
    QSet<EntryId> entry_ids;
    while(q.next()){
        EntryId cur_id = q.value("ID").toByteArray();

        if(!entry_ids.contains(cur_id))
            entry_ids.insert(cur_id);

        ids.PushBack(tuple<EntryId, EntryId, FileId>(
                          cur_id,
                          q.value("ParentID").toByteArray(),
                          q.value("FileID").toByteArray()));
    }

    // Delete all entries whose parent ids are missing
    vector<EntryId> delete_list_e;
    bool recheck = true;
    while(recheck){
        recheck = false;
        for(int i = ids.Length() - 1; i >= 0; --i){
            const tuple<EntryId, EntryId, FileId> &t = ids[i];
            if(!get<1>(t).IsNull() && entry_ids.find(get<1>(t)) == entry_ids.end()){
                delete_list_e.push_back(get<0>(t));
                _ew_delete_entry(conn_str, get<0>(t));

                // Every time we remove an id, we need to recheck the list
                //  because that id could have been someone's parent
                recheck = true;
                entry_ids.remove(get<0>(t));
                ids.RemoveAt(i);
            }
        }
    }

    if(0 < delete_list_e.size()){
        qDebug("Removed %d orphaned entries...", (int)delete_list_e.size());
    }

    // Update the index before removing the files (because it affects our calculation)
    d->index_lock.lock();
    for(const EntryId &eid : delete_list_e){
        d->index.erase(eid);
        d->parent_index.erase(eid);
    }
    d->index_lock.unlock();
    d->wc_index.notify_all();

    // Get the set of referenced file ids
    QSet<FileId> all_file_ids;
    for(const tuple<EntryId, EntryId, FileId> &t : ids){
        if(entry_ids.find(get<0>(t)) != entry_ids.end())
            all_file_ids.insert(get<2>(t));
    }


    // Now delete all the orphan files
    q.prepare("SELECT ID FROM File");
    DatabaseUtils::ExecuteQuery(q);
    {
        vector<FileId> delete_list_f;
        while(q.next()){
            FileId cur = q.value("ID").toByteArray();
            if(!all_file_ids.contains(cur))
                delete_list_f.push_back(cur);
        }

        for(FileId const &fid : delete_list_f)
            _fw_del_file(conn_str, fid);

        if(0 < delete_list_f.size()){
            qDebug("Removed %d orphaned files...", (int)delete_list_f.size());
        }
    }
}


void PasswordDatabase::_entry_worker(GUtil::CryptoPP::Cryptor *c)
{
    G_D;
    // We will delete the cryptor
    SmartPointer<GUtil::CryptoPP::Cryptor> bgCryptor(c);
    const QString conn_str = __create_connection(m_filepath);

    unique_lock<mutex> lkr(d->entry_thread_lock);
    while(!d->closing)
    {
        // Tell everyone who's waiting that we're going idle
        d->entry_thread_idle = true;
        d->wc_entry_thread.notify_all();

        // Wait for something to do
        d->wc_entry_thread.wait(lkr, [&]{
            return d->closing || !d->entry_thread_commands.empty();
        });

        // Empty the command queue, even if closing
        while(!d->entry_thread_commands.empty())
        {
            // This needs to be set by the one assigning us work
            GASSERT(!d->entry_thread_idle);

            SmartPointer<bg_worker_command> cmd(d->entry_thread_commands.front());
            d->entry_thread_commands.pop();
            lkr.unlock();
            try
            {
                // Process the command (long task)
                switch(cmd->CommandType)
                {
                case bg_worker_command::AddEntry:
                {
                    add_entry_command *aec = static_cast<add_entry_command *>(cmd.Data());
                    _ew_add_entry(conn_str, aec->entry);
                }
                    break;
                case bg_worker_command::EditEntry:
                {
                    update_entry_command *uec = static_cast<update_entry_command *>(cmd.Data());
                    _ew_update_entry(conn_str, uec->entry);
                }
                    break;
                case bg_worker_command::DeleteEntry:
                {
                    delete_entry_command *dec = static_cast<delete_entry_command *>(cmd.Data());
                    _ew_delete_entry(conn_str, dec->Id);
                }
                    break;
                case bg_worker_command::MoveEntry:
                {
                    move_entry_command *mec = static_cast<move_entry_command *>(cmd.Data());
                    _ew_move_entry(conn_str,
                                   mec->ParentSource, mec->RowFirst, mec->RowLast,
                                   mec->ParentDest, mec->RowDest);
                }
                    break;
                case bg_worker_command::RefreshFavoriteEntries:
                    _ew_refresh_favorites(conn_str);
                    break;
                case bg_worker_command::SetFavoriteEntries:
                {
                    set_favorite_entries_command *sfe = static_cast<set_favorite_entries_command *>(cmd.Data());
                    _ew_set_favorites(conn_str, sfe->Favorites);
                }
                    break;
                case bg_worker_command::AddFavoriteEntry:
                {
                    add_favorite_entry *afe = static_cast<add_favorite_entry *>(cmd.Data());
                    _ew_add_favorite(conn_str, afe->ID);
                }
                    break;
                case bg_worker_command::RemoveFavoriteEntry:
                {
                    remove_favorite_entry *rfe = static_cast<remove_favorite_entry *>(cmd.Data());
                    _ew_remove_favorite(conn_str, rfe->ID);
                }
                    break;
                case bg_worker_command::DispatchOrphans:
                {
                    //dispatch_orphans_command *doc = static_cast<dispatch_orphans_command*>(cmd.Data());
                    _ew_dispatch_orphans(conn_str);
                }
                    break;
                default:
                    break;
                }

                // This is to see how we handle exceptions from the background thread
//                throw DataTransportException<true>("This is a test", {
//                                                 {"Oh", "Yeah"}
//                                             });
            }
            catch(const GUtil::Exception<> &ex)
            {
                emit NotifyExceptionOnBackgroundThread(shared_ptr<Exception<>>((Exception<> *)ex.Clone()));
            }
            catch(...) {}
            lkr.lock();
        }
    }
    QSqlDatabase::removeDatabase(conn_str);
    d->entry_thread_idle = true;
}

void PasswordDatabase::_file_worker(GUtil::CryptoPP::Cryptor *c)
{
    G_D;
    // We will delete the cryptor
    SmartPointer<GUtil::CryptoPP::Cryptor> bgCryptor(c);
    const QString conn_str = __create_connection(m_filepath);

    // Begin the thread loop
    unique_lock<mutex> lkr(d->file_thread_lock);
    while(!d->closing)
    {
        // Tell everyone who's waiting that we're going idle
        d->file_thread_idle = true;
        d->wc_file_thread.notify_all();

        // Wait for something to do
        d->wc_file_thread.wait(lkr, [&]{
            d->cancel_file_thread = false;
            return d->closing || !d->file_thread_commands.empty();
        });
        d->file_thread_idle = false;

        // Empty the command queue, even if closing
        while(!d->file_thread_commands.empty())
        {
            SmartPointer<bg_worker_command> cmd(d->file_thread_commands.front());
            d->file_thread_commands.pop();
            lkr.unlock();
            try
            {
                // Process the command (long task)
                switch(cmd->CommandType)
                {
                case bg_worker_command::UpdateFile:
                {
                    update_file_command *afc = static_cast<update_file_command *>(cmd.Data());
                    _fw_add_file(conn_str, *bgCryptor, afc->ID,
                                 afc->FilePath.isEmpty() ? afc->FileContents : afc->FilePath,
                                 !afc->FilePath.isEmpty());
                }
                    break;
                case bg_worker_command::ExportFile:
                {
                    export_file_command *efc = static_cast<export_file_command *>(cmd.Data());
                    _fw_exp_file(conn_str, *bgCryptor, efc->ID, efc->FilePath);
                }
                    break;
                case bg_worker_command::DeleteFile:
                {
                    delete_file_command *afc = static_cast<delete_file_command *>(cmd.Data());
                    _fw_del_file(conn_str, afc->ID);
                }
                    break;
                case bg_worker_command::ExportToPS:
                {
                    export_to_ps_command *e2ps = static_cast<export_to_ps_command *>(cmd.Data());
                    _fw_export_to_gps(conn_str, *bgCryptor, e2ps->FilePath, e2ps->Creds);
                }
                    break;
                case bg_worker_command::ImportFromPS:
                {
                    import_from_ps_command *ifps = static_cast<import_from_ps_command *>(cmd.Data());
                    _fw_import_from_gps(conn_str, *bgCryptor, ifps->FilePath, ifps->Creds);
                }
                    break;
                default:
                    break;
                }
            }
            catch(const GUtil::Exception<> &ex){
                emit NotifyExceptionOnBackgroundThread(shared_ptr<Exception<>>((Exception<> *)ex.Clone()));
            }
            catch(...) { /* Don't let any exception crash us */ }
            lkr.lock();
        }
    }
    QSqlDatabase::removeDatabase(conn_str);
    d->file_thread_idle = true;
}

static bool __file_exists(QSqlQuery &q, const FileId &id)
{
    bool ret = false;

    // Sometimes the database is busy when we want to access it, so
    //  just wait here
    int try_cnt = 0;
    while(try_cnt < MAX_TRY_COUNT &&
          !q.prepare("SELECT COUNT(*) FROM File WHERE ID=:fid"))
    {
        try_cnt++;
        Thread::sleep(1);
    }

    if(try_cnt == MAX_TRY_COUNT)
        throw Exception<>(q.lastError().text().toUtf8().constData());

    q.bindValue(":fid", (QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    if(q.next())
        ret = 0 < q.record().value(0).toInt();
    return ret;
}

void PasswordDatabase::_fw_add_file(const QString &conn_str,
                                    GUtil::CryptoPP::Cryptor &cryptor,
                                    const FileId &id,
                                    const QByteArray &data,
                                    bool by_path = true)
{
    G_D;
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);

    // Always notify that the task is complete, even if it's an error
    finally([&]{ emit NotifyProgressUpdated(100, false, m_curTaskString); });
    emit NotifyProgressUpdated(0, false, tr("Adding file..."));

    uint plaintext_length;
    db.transaction();
    {
        bool success = false;
        finally([&]{
            TryFinally([&]{
                if(success) __commit_transaction(db);
                else        db.rollback();
            }, [](std::exception &){},
            [&]{
                emit NotifyProgressUpdated(100, false, "Adding file");
            });
        });

        // First remove the existing file, if there is one. We do this right away
        //  because we want this to happen even if the user cancels.
        bool exists = __file_exists(q, id);
        if(exists){
            q.prepare("DELETE FROM File WHERE ID=?");
            q.addBindValue((QByteArray)id);
            DatabaseUtils::ExecuteQuery(q);
        }

        // First upload and encrypt the file
        QByteArray encrypted_data;
        {
            QByteArrayOutput o(encrypted_data);
            SmartPointer<IInput> data_in;

            // The data can be either a file path or the contents itself,
            //  depending on the value of the flag
            if(by_path){
                File *f = new File(data);
                data_in = f;
                f->Open(File::OpenRead);
                plaintext_length = f->Length();
            }
            else{
                data_in = new QByteArrayInput(data);
                plaintext_length = data.length();
            }

            // Get a fresh nonce
            byte nonce[NONCE_LENGTH];
            __get_nonce(nonce);

            m_progressMin = 0, m_progressMax = 75;
            m_curTaskString = QString("Download and encrypt file");
            d->file_thread_cancellable = true;
            cryptor.EncryptData(&o, data_in, NULL, nonce, DEFAULT_CHUNK_SIZE, this);
        }

        _fw_fail_if_cancelled();
        m_curTaskString = QString("Adding file");
        emit NotifyProgressUpdated(m_progressMax, true, m_curTaskString);

        _fw_fail_if_cancelled();

        // Insert a new file
        q.prepare("INSERT INTO File (Length, Data, ID) VALUES (?,?,?)");
        q.addBindValue(plaintext_length);
        q.addBindValue(encrypted_data, QSql::In | QSql::Binary);
        q.addBindValue((QByteArray)id, QSql::In | QSql::Binary);
        DatabaseUtils::ExecuteQuery(q);

        // One last chance before we commit
        emit NotifyProgressUpdated(m_progressMax + 10, true, m_curTaskString);
        _fw_fail_if_cancelled();

        success = true;
    }

    // Add the file to the index and notify
    unique_lock<mutex> lkr(d->index_lock);
    file_cache fc;
    fc.id = id;
    fc.length = plaintext_length;
    d->file_index.emplace(fc.id, fc);
    d->wc_index.notify_all();
}

void PasswordDatabase::_fw_exp_file(const QString &conn_str,
                                     GUtil::CryptoPP::Cryptor &cryptor,
                                     const FileId &id,
                                     const char *filepath)
{
    G_D;
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);
    const QString filename(QFileInfo(filepath).fileName());

    m_curTaskString = QString(tr("Decrypt and Export: %1")).arg(filename);
    emit NotifyProgressUpdated(0, true, m_curTaskString);

    // Always notify that the task is complete, even if it's an error
    finally([&]{ emit NotifyProgressUpdated(100, false, m_curTaskString); });

    // First fetch the file from the database
    q.prepare("SELECT Data FROM File WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);

    if(!q.next())
        throw Exception<>("File ID not found");
    _fw_fail_if_cancelled();
    emit NotifyProgressUpdated(25, true, m_curTaskString);

    const QByteArray encrypted_data(q.record().value(0).toByteArray());
    {
        QByteArrayInput i(encrypted_data);
        File f(filepath);
        f.Open(File::OpenReadWriteTruncate);

        _fw_fail_if_cancelled();
        emit NotifyProgressUpdated(35, true, m_curTaskString);

        m_progressMin = 35, m_progressMax = 100;
        d->file_thread_cancellable = true;
        cryptor.DecryptData(&f, &i, NULL, DEFAULT_CHUNK_SIZE, this);
    }
}

void PasswordDatabase::_fw_del_file(const QString &conn_str, const FileId &id)
{
    __delete_file_by_id(conn_str, id);
}

static void __append_children_to_xml(QDomDocument &xdoc, QDomNode &n,
                                     QSqlQuery &q,
                                     GUtil::CryptoPP::Cryptor &cryptor,
                                     const EntryId &eid)
{
    // Find my children and add them to xml
    Vector<Entry> child_list = __find_entries_by_parent_id(q, cryptor, eid);
    foreach(const Entry &e, child_list){
        QDomNode new_node = XmlConverter::AppendToXmlNode(e, n, xdoc, true);
        __append_children_to_xml(xdoc, new_node, q, cryptor, e.GetId());
    }
}

void PasswordDatabase::_fw_export_to_gps(const QString &conn_str,
                                         GUtil::CryptoPP::Cryptor &my_cryptor,
                                         const char *ps_filepath,
                                         const Credentials &creds)
{
    int progress_counter = 0;
    m_curTaskString = QString(tr("Exporting to Portable Safe: %1"))
                            .arg(QFileInfo(ps_filepath).fileName());
    emit NotifyProgressUpdated(progress_counter, true, m_curTaskString);

    // Always notify that the task is complete, even if it's an error
    finally([&]{ emit NotifyProgressUpdated(100, false, m_curTaskString); });

    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);

    // We export inside a database transaction. Even though we're reading only
    //  this makes sure nobody changes anything while we're exporting
    db.transaction();
    try{
        emit NotifyProgressUpdated(progress_counter+=5, true, m_curTaskString);

        // First get the main payload: the entire entry structure in XML
        QDomDocument xdoc;
        QDomNode root = xdoc.appendChild(xdoc.createElement("Grypto_XML"));
        __append_children_to_xml(xdoc, root, q, my_cryptor, EntryId::Null());

        emit NotifyProgressUpdated(progress_counter+=15, true, m_curTaskString);
        _fw_fail_if_cancelled();

        // Open the target file
        GPSFile_Export gps_file(ps_filepath, creds);
        emit NotifyProgressUpdated(progress_counter+=5, true, m_curTaskString);
        _fw_fail_if_cancelled();

        // Compress the entry data and write it as the main payload
        {
            const QByteArray xml_compressed(qCompress(xdoc.toByteArray(-1), 9));
            gps_file.AppendPayload((byte const *)xml_compressed.constData(),
                                   xml_compressed.length());
        }
        emit NotifyProgressUpdated(progress_counter+=10, true, m_curTaskString);
        _fw_fail_if_cancelled();

        // Now let's export all the files as attachments
        int file_cnt = 0;
        const int remaining_progress = 100 - progress_counter;
        const QHash<FileId, FileInfo_t> files = GetFileSummary();
        for(const FileId &k : files.keys())
        {
            // Select a file from the database
            q.prepare("SELECT Data FROM File WHERE ID=?");
            q.addBindValue((QByteArray)k);
            DatabaseUtils::ExecuteQuery(q);
            _fw_fail_if_cancelled();

            if(!q.next())
                continue;

            // Decrypt it in memory
            Vector<byte> pt;
            {
                const QByteArray ct(q.record().value(0).toByteArray());
                pt.ReserveExactly(ct.length() -
                                  (my_cryptor.GetNonceSize() +
                                   my_cryptor.TagLength));
                QByteArrayInput i(ct);
                VectorByteArrayOutput o(pt);
                my_cryptor.DecryptData(&o, &i);
            }
            _fw_fail_if_cancelled();

            // Write it to the GPS, with the file ID in the metadata
            gps_file.AppendPayload((byte const *)pt.ConstData(), pt.Length(),
                                   (byte const *)k.ConstData(), k.Size);

            ++file_cnt;
            emit NotifyProgressUpdated(
                        progress_counter + (remaining_progress*((float)file_cnt/files.size())),
                        true,
                        m_curTaskString);
            _fw_fail_if_cancelled();
        }
    }
    catch(...){
        db.rollback();
        throw;
    }
    __commit_transaction(db);    // Nothing should have changed, is a rollback better here?
}

void PasswordDatabase::_fw_import_from_gps(const QString &,
                                           GUtil::CryptoPP::Cryptor &,
                                           const char *,
                                           const Credentials &)
{
    // Always notify that the task is complete, even if it's an error
    //finally([&]{ emit NotifyProgressUpdated(100, m_curTaskString); });

    throw NotImplementedException<>();
}


// This function belongs to the background thread and will always execute there
void PasswordDatabase::ProgressUpdated(int prg)
{
    // prg is between 0 and 100, so scale it to m_progressMax-m_progressMin
    G_D;
    emit NotifyProgressUpdated(m_progressMin + ((float)prg*(m_progressMax-m_progressMin))/100,
                               d->file_thread_cancellable,
                               m_curTaskString);
}

bool PasswordDatabase::ShouldOperationCancel()
{
    G_D;
    d->file_thread_lock.lock();
    bool ret = d->cancel_file_thread;
    d->file_thread_lock.unlock();
    return ret;
}

void PasswordDatabase::_fw_fail_if_cancelled()
{
    if(ShouldOperationCancel())
        throw CancelledOperationException<>();
}

GUtil::CryptoPP::Cryptor const &PasswordDatabase::Cryptor() const
{
    FailIfNotOpen();
    G_D;
    return *d->cryptor;
}

void PasswordDatabase::WaitForEntryThreadIdle() const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->entry_thread_lock);
    d->wc_entry_thread.wait(lkr, [&]{
        return d->entry_thread_idle;
    });
}


END_NAMESPACE_GRYPTO;

