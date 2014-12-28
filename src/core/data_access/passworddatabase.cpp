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
#include "gutil_cryptopp_cryptor.h"
#include "gutil_cryptopp_rng.h"
#include "gutil_gpsutils.h"
#include "gutil_databaseutils.h"
#include "gutil_sourcesandsinks.h"
#include "gutil_qtsourcesandsinks.h"
#include "gutil_smartpointer.h"
#include "gutil_file.h"
#include <queue>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <QString>
#include <QFileInfo>
#include <QResource>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDomDocument>
USING_NAMESPACE_GUTIL1(Qt);
USING_NAMESPACE_GUTIL1(CryptoPP);
USING_NAMESPACE_GUTIL;
using namespace std;

// Upload files in chunks of this size
#define DEFAULT_CHUNK_SIZE  65535

// The length of the salt field of the database
#define SALT_LENGTH 32

// The length of the nonce used by the cryptor
#define NONCE_LENGTH 8

namespace Grypt{
class bg_worker_command;
}

// A cached entry row from the database
struct entry_cache{
    Grypt::EntryId id, parentid;
    uint row;
    int favoriteindex;
    QByteArray crypttext;
    bool dirty = false;
    bool exists = true;

    entry_cache() {}

    // Sets everything but the crypttext
    entry_cache(const Grypt::Entry &e)
        :id(e.GetId()),
          parentid(e.GetParentId()),
          row(e.GetRow()),
          favoriteindex(e.GetFavoriteIndex())
    {}
};

namespace
{
struct d_t
{
    // Main thread member variables
    QByteArray filePath;
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

    mutex entry_thread_lock;
    condition_variable wc_entry_thread;
    queue<Grypt::bg_worker_command *> entry_thread_commands;

    bool closing;

    // The index variables are maintained by both the main thread and
    //  the background threads.
    unordered_map<Grypt::EntryId, entry_cache> index;
    unordered_map<Grypt::EntryId, Vector<Grypt::EntryId> > parent_index;
    Vector<Grypt::EntryId> favorite_index;
    mutex index_lock;
    condition_variable wc_index;

    d_t()
        :cancel_file_thread(false),
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

static void __int_to_nonce(GINT64 i, byte n[NONCE_LENGTH])
{
    GASSERT(NONCE_LENGTH == sizeof(i));
    for(int j = NONCE_LENGTH - 1; j >= 0; --j){
        n[j] = i & 0x0FF;
        i = i >> 8;
    }
}

// Returns the current nonce and increments it in the database
//  This should be used inside a transaction
static void __get_nonce(const QString &conn_str, byte nonce[NONCE_LENGTH])
{
    QSqlQuery q("SELECT Nonce FROM Version", QSqlDatabase::database(conn_str));
    if(q.next()){
        GINT64 n = q.record().value(0).toLongLong();
        __int_to_nonce(n++, nonce);

        q.prepare("UPDATE Version SET Nonce=?");
        q.addBindValue(n);
        DatabaseUtils::ExecuteQuery(q);
    }
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
        FetchEntrysByParentId,
        RefreshFavoriteEntries,
        SetFavoriteEntries,

        // File commands
        UpdateFile,
        DeleteFile,
        ExportFile,
        ExportToPS,
        ImportFromPS
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

class cache_entries_by_parentid_command : public bg_worker_command
{
public:
    cache_entries_by_parentid_command(const EntryId &id)
        :bg_worker_command(FetchEntrysByParentId),
          Id(id)
    {}
    const EntryId Id;
};

class refresh_favorite_entries_command : public bg_worker_command
{
public:
    refresh_favorite_entries_command() :bg_worker_command(RefreshFavoriteEntries){}
};

class set_favorite_entries_command : public bg_worker_command
{
public:
    set_favorite_entries_command(const Vector<EntryId> &favs)
        :bg_worker_command(SetFavoriteEntries),
          Favorites(favs)
    {}

    const Vector<EntryId> Favorites;
};

class update_file_command : public bg_worker_command
{
public:
    update_file_command(const FileId &id, const char *path)
        :bg_worker_command(UpdateFile),
          ID(id),
          FilePath(path)
    {}
    const FileId ID;
    const QByteArray FilePath;
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
    export_to_ps_command(const char *filepath, const char *password, const char *keyfile)
        :bg_worker_command(ExportToPS),
          FilePath(filepath),
          Password(password),
          KeyFile(keyfile)
    {}
    const QByteArray FilePath;
    const QByteArray Password;
    const QByteArray KeyFile;
};

class import_from_ps_command : public bg_worker_command
{
public:
    import_from_ps_command(const char *filepath, const char *password, const char *keyfile)
        :bg_worker_command(ImportFromPS),
          FilePath(filepath),
          Password(password),
          KeyFile(keyfile)
    {}
    const QByteArray FilePath;
    const QByteArray Password;
    const QByteArray KeyFile;
};


// Creates a database connection and returns the qt connection string
static QString __create_connection(const QString &file_path, const QString &force_conn_str = QString())
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

void PasswordDatabase::_init_cryptor(const char *p, const char *k, const byte *s, GUINT32 s_l)
{
    G_D;
    GASSERT(!d->cryptor);
    d->cryptor = new GUtil::CryptoPP::Cryptor(p, k, NONCE_LENGTH, new Cryptor::DefaultKeyDerivation(s, s_l));
}

// Tells the entry worker to cache the child entries of the given parent
void __command_cache_entries_by_parentid(d_t *d, const EntryId &id)
{
    d->entry_thread_lock.lock();
    d->entry_thread_commands.push(new cache_entries_by_parentid_command(id));
    d->entry_thread_lock.unlock();
    d->wc_entry_thread.notify_one();
}

PasswordDatabase::PasswordDatabase(const char *file_path, const char *password, const char *keyfile, QObject *par)
    :QObject(par)
{
    G_D_INIT();
    G_D;

    bool file_exists = File::Exists(d->filePath = file_path);
    d->dbString = __create_connection(d->filePath);   // After we check if the file exists
    try
    {
        QSqlDatabase db = QSqlDatabase::database(d->dbString);
        QSqlQuery q(db);

        // Initialize the new database if it doesn't exist
        if(!file_exists)
        {
            __init_sql_resources();
            QResource rs(":/sql/create_db.sql");
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
            _init_cryptor(password, keyfile, salt, SALT_LENGTH);

            // Prepare the keycheck data
            QByteArray keycheck_ct;
            byte nonce[NONCE_LENGTH];
            __int_to_nonce(GINT64_MIN, nonce);
            {
                ByteArrayInput auth_in(__keycheck_string, sizeof(__keycheck_string));
                QByteArrayOutput ba_out(keycheck_ct);
                d->cryptor->EncryptData(&ba_out, NULL, &auth_in, nonce);
            }

            // Insert a version record
            q.prepare("INSERT INTO Version (Version,Nonce,Salt,KeyCheck)"
                        " VALUES (?,?,?,?)");
            q.addBindValue(GRYPTO_VERSION_STRING);
            q.addBindValue(GINT64_MIN + 1);  // The first nonce was already used in the keycheck
            q.addBindValue(QByteArray((const char *)salt, sizeof(salt)));
            q.addBindValue(keycheck_ct);
            DatabaseUtils::ExecuteQuery(q);
        }

        // Check the version record to see if it is valid
        q.prepare("SELECT Version,KeyCheck,Salt FROM Version");
        DatabaseUtils::ExecuteQuery(q);

        int cnt = 0;
        while(q.next()){
            if(++cnt > 1)
                throw Exception<>("Found too many version rows; there should be exactly one");

            QString ver = q.record().value("Version").toString();
            if(ver != GRYPTO_VERSION_STRING)
                throw Exception<>(QString("Wrong database version: %1").arg(ver).toUtf8());

            if(!d->cryptor){
                QByteArray salt_ba = q.record().value("Salt").toByteArray();
                _init_cryptor(password, keyfile, (byte const *)salt_ba.constData(), salt_ba.length());
            }

            QByteArrayInput bai_ct(q.record().value("KeyCheck").toByteArray());
            ByteArrayInput auth_in(__keycheck_string, sizeof(__keycheck_string));

            // This will throw an exception if the key was bad
            d->cryptor->DecryptData(NULL, &bai_ct, &auth_in);
        }
        if(cnt == 0)
            throw Exception<>("Did not find a version row");
    }
    catch(...)
    {
        QSqlDatabase::removeDatabase(d->dbString);
        throw;
    }

    // Give the workers something to do, even before they've arrived for the day. That's the
    //  kind of brutal task master I am ;)
    __command_cache_entries_by_parentid(d, EntryId::Null());    // Start loading the root node
    RefreshFavorites();

    // Start up the background threads
    // The entry worker goes first, since he gets most of the action at the start
    d->entry_worker = std::thread(&PasswordDatabase::_entry_worker, this, new GUtil::CryptoPP::Cryptor(*d->cryptor));
    d->file_worker = std::thread(&PasswordDatabase::_file_worker, this, new GUtil::CryptoPP::Cryptor(*d->cryptor));
}

PasswordDatabase::~PasswordDatabase()
{
    G_D;
    d->file_thread_lock.lock();
    d->entry_thread_lock.lock();
    d->closing = true;
    d->entry_thread_lock.unlock();
    d->file_thread_lock.unlock();

    d->wc_file_thread.notify_one();
    d->wc_entry_thread.notify_one();

    QSqlDatabase::removeDatabase(d->dbString);
    d->entry_worker.join();
    d->file_worker.join();
    G_D_UNINIT();
}

bool PasswordDatabase::CheckPassword(const char *password, const char *keyfile) const
{
    G_D;
    bool ret = false;

    // Fetch the salt from the version row
    QSqlQuery q("SELECT Salt FROM Version", QSqlDatabase::database(d->dbString));
    if(q.next()){
        QByteArray salt = q.record().value(0).toByteArray();
        ret = d->cryptor->CheckPassword(password, keyfile);
    }
    return ret;
}

void PasswordDatabase::_ew_add_entry(const QString &conn_str, GUtil::CryptoPP::Cryptor &cryptor, const Entry &e)
{
    G_D;
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QSqlQuery q(db);
    db.transaction();
    {
        bool success = false;
        finally([&]{
            if(success) db.commit();
            else        db.rollback();
        });

        // Update the surrounding entries' rows before insertion
        int child_count = __count_entries_by_parent_id(q, e.GetParentId());
        int row = e.GetRow();
        if(row > child_count)
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

        // Generate the crypttext
        QByteArray crypttext;
        {
            // Get a fresh nonce
            byte nonce[NONCE_LENGTH];
            __get_nonce(conn_str, nonce);

            QByteArrayInput i(XmlConverter::ToXmlString(e));
            QByteArrayOutput o(crypttext);
            cryptor.EncryptData(&o, &i, NULL, nonce);
        }

        // Update the index
        {
            lock_guard<mutex> lkr(d->index_lock);

            // If the index is not present, then it was already deleted
            if(d->index.find(e.GetId()) == d->index.end())
                return;

            d->index[e.GetId()].crypttext = crypttext;
            d->index[e.GetId()].dirty = false;
            d->wc_index.notify_all();
        }

        q.prepare(QString("INSERT INTO Entry (ID,%1Row,Favorite,Data)"
                  " VALUES (?,%2?,?,?)")
                  .arg(pid_isnull ? "" : "ParentID,")
                  .arg(pid_isnull ? "" : "?,"));
        q.addBindValue((QByteArray)e.GetId());
        if(!pid_isnull)
            q.addBindValue((QByteArray)e.GetParentId());
        q.addBindValue(row);
        q.addBindValue(e.GetFavoriteIndex());
        q.addBindValue(crypttext);
        DatabaseUtils::ExecuteQuery(q);

        success = true;
    }

    if(e.IsFavorite())
        emit NotifyFavoritesUpdated();

    // Add any new files to the database
    __add_new_files(this, e);
}

void PasswordDatabase::_ew_update_entry(const QString &conn_str, GUtil::CryptoPP::Cryptor &cryptor, const Entry &e)
{
    G_D;
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    int old_favorite_index;
    db.transaction();
    try{
        QSqlQuery q(db);
        q.prepare("SELECT Favorite FROM Entry WHERE Id=?");
        q.addBindValue((QByteArray)e.GetId());
        DatabaseUtils::ExecuteQuery(q);
        bool res = q.next();
        GASSERT(res);
        old_favorite_index = q.record().value(0).toInt();

        entry_cache er = e;
        {
            // Get a fresh nonce
            byte nonce[NONCE_LENGTH];
            __get_nonce(conn_str, nonce);

            QByteArrayOutput o(er.crypttext);
            QByteArrayInput i(XmlConverter::ToXmlString(e));
            cryptor.EncryptData(&o, &i, NULL, nonce);
        }

        // Update the index
        d->index_lock.lock();
        d->index[e.GetId()] = er;
        if(old_favorite_index != e.GetFavoriteIndex()){
            if(e.IsFavorite() && old_favorite_index < 0)
                d->favorite_index.PushBack(e.GetId());
            else if(!e.IsFavorite() && old_favorite_index >= 0)
                d->favorite_index.RemoveOne(e.GetId());
        }
        d->index_lock.unlock();
        d->wc_index.notify_all();

        q.prepare("UPDATE Entry SET Data=?,Favorite=? WHERE Id=?");
        q.addBindValue(er.crypttext);
        q.addBindValue(er.favoriteindex);
        q.addBindValue((QByteArray)e.GetId());
        DatabaseUtils::ExecuteQuery(q);
    } catch(...) {
        db.rollback();
        throw;
    }
    db.commit();

    if(old_favorite_index != e.GetFavoriteIndex())
        emit NotifyFavoritesUpdated();

    // We never remove old files, but we may add new ones here
    __add_new_files(this, e);
}

void PasswordDatabase::_ew_delete_entry(const QString &conn_str,
                                        const EntryId &id)
{
    G_D;
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QSqlQuery q(db);
    int old_favorite_index;
    db.transaction();
    try
    {
        q.prepare("SELECT ParentId,Row,Favorite FROM Entry WHERE Id=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);
        if(!q.next()) throw Exception<>("Entry not found");
        EntryId pid = q.record().value(0).toByteArray();
        uint row = q.record().value(1).toInt();
        old_favorite_index = q.record().value(2).toInt();
        uint child_count = __count_entries_by_parent_id(q, pid);

        // Remove it from the index
        d->index_lock.lock();
        {
            auto i = d->index.find(id);
            auto pi = d->parent_index.find(pid);
            if(i != d->index.end()){
                d->index.erase(i);
                d->parent_index.erase(id);
            }
            if(pi != d->parent_index.end())
                pi->second.RemoveAt(row);
            if(old_favorite_index >= 0)
                d->favorite_index.RemoveOne(id);
        }
        d->index_lock.unlock();
        d->wc_index.notify_all();

        q.prepare("DELETE FROM Entry WHERE ID=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);

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
    }
    catch(...)
    {
        db.rollback();
        throw;
    }
    db.commit();

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
    db.transaction();
    try
    {
        // First move the source rows to a dummy parent with ID=0
        for(int i = 0; i < row_cnt; ++i){
            q.prepare(QString("UPDATE Entry SET ParentID=?,Row=? WHERE ParentID%1 AND Row=?")
                      .arg(src_parent.IsNull() ? " IS NULL" : "=?"));
            q.addBindValue(EntryId::Null().ToQByteArray());
            q.addBindValue(row_dest + i);
            if(!src_parent.IsNull())
                q.addBindValue(src_parent.ToQByteArray());
            q.addBindValue(row_first + i);
            DatabaseUtils::ExecuteQuery(q);
        }

        // Update the siblings at the source
        int siblings_cnt = __count_entries_by_parent_id(q, src_parent);
        for(int i = row_last + 1; i < siblings_cnt + row_cnt; ++i){
            q.prepare(QString("UPDATE Entry SET Row=? WHERE ParentID%1 AND Row=?")
                      .arg(src_parent.IsNull() ? " IS NULL" : "=?"));
            q.addBindValue(i - row_cnt);
            if(!src_parent.IsNull())
                q.addBindValue(src_parent.ToQByteArray());
            q.addBindValue(row_first + row_cnt);
            DatabaseUtils::ExecuteQuery(q);
        }

        // Update the siblings at the dest
        siblings_cnt = __count_entries_by_parent_id(q, dest_parent);
        for(int i = row_dest; i < siblings_cnt; ++i){
            q.prepare(QString("UPDATE Entry SET Row=? WHERE ParentID%1 AND Row=?")
                      .arg(dest_parent.IsNull() ? " IS NULL" : "=?"));
            q.addBindValue(i + row_cnt);
            if(!dest_parent.IsNull())
                q.addBindValue(dest_parent.ToQByteArray());
            q.addBindValue(i);
            DatabaseUtils::ExecuteQuery(q);
        }

        // Finally move the rows into the dest
        q.prepare("UPDATE Entry SET ParentID=? WHERE ParentID=?");
        if(dest_parent.IsNull())
            q.addBindValue(QByteArray());
        else
            q.addBindValue(dest_parent.ToQByteArray());
        q.addBindValue(EntryId::Null().ToQByteArray());
        DatabaseUtils::ExecuteQuery(q);
    }
    catch(...)
    {
        db.rollback();
        throw;
    }
    db.commit();
}

void PasswordDatabase::AddEntry(Entry &e, bool gen_id)
{
    G_D;
    if(gen_id)
        e.SetId(EntryId::NewId());

    // Mark the index as dirty
    d->index_lock.lock();
    {
        // Add to the appropriate parent
        auto pi = d->parent_index.find(e.GetParentId());
        if(pi != d->parent_index.end()){
            Vector<EntryId> &child_ids = pi->second;
            if(e.GetRow() > (uint)child_ids.Length())
                e.SetRow(child_ids.Length());

            child_ids.Insert(e.GetId(), e.GetRow());

            // Adjust the siblings
            for(auto i = e.GetRow() + 1; i < child_ids.Length(); ++i){
                auto iter = d->index.find(child_ids[i]);
                if(iter != d->index.end())
                    iter->second.row = i;
            }
        }
        if(e.IsFavorite())
            d->favorite_index.PushBack(e.GetId());

        entry_cache ec(e);
        ec.dirty = true;
        d->index.emplace(e.GetId(), ec);
    }
    d->index_lock.unlock();

    // Tell the worker thread to add it to the database
    d->entry_thread_lock.lock();
    d->entry_thread_commands.push(new add_entry_command(e));
    d->entry_thread_lock.unlock();
    d->wc_entry_thread.notify_one();

    // Clear the file path so we don't add the same file twice
    e.SetFilePath(QString::null);
}

void PasswordDatabase::UpdateEntry(Entry &e)
{
    G_D;

    // Mark the index as dirty
    {
        unique_lock<mutex> lkr(d->index_lock);
        auto iter = d->index.find(e.GetId());
        if(iter == d->index.end())
            return; // The entry was already deleted

        // Do not update a dirty record
        d->wc_index.wait(lkr, [&]{
            return !iter->second.dirty;
        });

        iter->second.dirty = true;
    }

    d->entry_thread_lock.lock();
    d->entry_thread_commands.push(new update_entry_command(e));
    d->entry_thread_lock.unlock();
    d->wc_entry_thread.notify_one();

    // Clear the file path so we don't add the same file twice
    e.SetFilePath(QString::null);
}

void PasswordDatabase::DeleteEntry(const EntryId &id)
{
    G_D;
    if(id.IsNull())
        return;

    // Update the index
    d->index_lock.lock();
    auto i = d->index.find(id);
    if(i != d->index.end()){
        auto pi = d->parent_index.find(i->second.parentid);
        if(pi != d->parent_index.end()){
            pi->second.RemoveAt(i->second.row);

            // Update the siblings of the item we just removed
            for(auto r = i->second.row; r < pi->second.Length(); ++r){
                auto ci = d->index.find(pi->second[r]);
                if(ci != d->index.end())
                    ci->second.row = r;
            }
        }
        d->index.erase(i);
    }
    d->index_lock.unlock();

    // Remove it from the database
    d->entry_thread_lock.lock();
    d->entry_thread_commands.push(new delete_entry_command(id));
    d->entry_thread_lock.unlock();
    d->wc_entry_thread.notify_one();
}

void PasswordDatabase::MoveEntries(const EntryId &parentId_src, quint32 row_first, quint32 row_last,
                                   const EntryId &parentId_dest, quint32 row_dest)
{
    G_D;
    int move_cnt = row_last - row_first + 1;
    bool same_parents = parentId_src == parentId_dest;
    if(0 > move_cnt ||
            (same_parents && row_first <= row_dest && row_dest <= row_last))
        throw Exception<>("Invalid move parameters");

    // Update the index
    unique_lock<mutex> lkr(d->index_lock);
    Vector<EntryId> &src = d->parent_index.find(parentId_src)->second;
    Vector<EntryId> &dest = d->parent_index.find(parentId_dest)->second;
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
    for(int i = 0; i < move_cnt; ++i)
        dest.Insert(moving_rows[i], row_dest + i);

    // Update the siblings at the dest
    for(quint32 i = row_dest; i < dest.Length(); ++i)
        d->index.find(dest[i])->second.row = i;
    lkr.unlock();

    // Update the database
    d->entry_thread_lock.lock();
    d->entry_thread_commands.push(new move_entry_command(parentId_src, row_first, row_last, parentId_dest, row_dest));
    d->entry_thread_lock.unlock();
    d->wc_entry_thread.notify_one();
}

Entry PasswordDatabase::FindEntry(const EntryId &id)
{
    G_D;
    __command_cache_entries_by_parentid(d, id);

    bool found = false;
    entry_cache ec;
    {
        unique_lock<mutex> lkr(d->index_lock);

        // Wait 'til the entry is fully loaded
        d->wc_index.wait(lkr, [&]{
            auto i = d->index.find(id);
            return i != d->index.end() && i->second.exists && !i->second.dirty;
        });

        auto i = d->index.find(id);
        if(i != d->index.end() && i->second.exists){
            ec = i->second;
            found = true;
        }
    }
    if(!found)
        throw Exception<>("Entry not found");
    return __convert_cache_to_entry(ec, *d->cryptor);
}

int PasswordDatabase::CountEntriesByParentId(const EntryId &id)
{
    G_D;
    unique_lock<mutex> lkr(d->index_lock);
    auto pi = d->parent_index.find(id);
    if(pi == d->parent_index.end())
        __command_cache_entries_by_parentid(d, id);

    d->wc_index.wait(lkr, [&]{
        return (pi = d->parent_index.find(id)) != d->parent_index.end();
    });
    return pi->second.Length();
}

vector<Entry> PasswordDatabase::FindEntriesByParentId(const EntryId &pid)
{
    G_D;
    // Let the cache know we are accessing this parent id, even if it's
    //  already in the cache, so we can preload its grandchildren
    __command_cache_entries_by_parentid(d, pid);

    vector<Entry> ret;
    {
        unique_lock<mutex> lkr(d->index_lock);

        // Wait until the parent is fully loaded
        d->wc_index.wait(lkr, [&]{
            bool loaded = false;
            auto pi = d->parent_index.find(pid);
            if(pi != d->parent_index.end()){
                loaded = true;
                for(const EntryId &id : pi->second){
                    auto i = d->index.find(id);
                    if(i == d->index.end() || i->second.dirty){
                        loaded = false;
                        break;
                    }
                }
            }
            return loaded;
        });

        foreach(const EntryId &child_id, d->parent_index[pid])
            ret.emplace_back(__convert_cache_to_entry(d->index[child_id], *d->cryptor));
    }
    return ret;
}

void PasswordDatabase::RefreshFavorites()
{
    G_D;
    unique_lock<mutex> lkr(d->entry_thread_lock);
    d->entry_thread_commands.push(new refresh_favorite_entries_command);
    d->wc_entry_thread.notify_one();
}

void PasswordDatabase::SetFavoriteEntries(const Vector<EntryId> &favs)
{
    G_D;
    unique_lock<mutex> lkr(d->entry_thread_lock);
    d->entry_thread_commands.push(new set_favorite_entries_command(favs));
    d->wc_entry_thread.notify_one();
}

vector<Entry> PasswordDatabase::FindFavoriteEntries()
{
    G_D;
    vector<entry_cache> rows;
    unique_lock<mutex> lkr(d->index_lock);
    for(const EntryId &id : d->favorite_index)
        rows.push_back(d->index[id]);
    lkr.unlock();

    vector<Entry> ret;
    for(const entry_cache &row : rows)
        ret.emplace_back(__convert_cache_to_entry(row, *d->cryptor));
    return ret;
}

bool PasswordDatabase::FileExists(const FileId &id)
{
    G_D;
    QSqlQuery q(QSqlDatabase::database(d->dbString));
    return _file_exists(q, id);
}

bool PasswordDatabase::_file_exists(QSqlQuery &q, const FileId &id)
{
    bool ret = false;
    q.prepare("SELECT COUNT(*) FROM File WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    if(q.next())
        ret = 0 < q.record().value(0).toInt();
    return ret;
}

void PasswordDatabase::CancelFileTasks()
{
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

vector<pair<FileId, quint32> > PasswordDatabase::GetFileSummary()
{
    G_D;
    QSqlQuery q(QSqlDatabase::database(d->dbString));
    q.prepare("SELECT ID,Length FROM File");
    DatabaseUtils::ExecuteQuery(q);

    vector<pair<FileId, quint32> > ret;
    while(q.next()){
        QByteArray ba_fid = q.record().value(0).toByteArray();
        if(ba_fid.length() == FileId::Size){
            ret.emplace_back(pair<FileId, quint32>(FileId(ba_fid.constData()), q.record().value(1).toInt()));
        }
    }
    return ret;
}

void PasswordDatabase::AddUpdateFile(const FileId &id, const char *filename)
{
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new update_file_command(id, filename));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::DeleteFile(const FileId &id)
{
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new delete_file_command(id));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::ExportFile(const FileId &id, const char *export_path)
{
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new export_file_command(id, export_path));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::ExportToPortableSafe(const char *export_filename,
                                            const char *password,
                                            const char *keyfile)
{
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new export_to_ps_command(export_filename, password, keyfile));
    d->wc_file_thread.notify_one();
}

void PasswordDatabase::ImportFromPortableSafe(const char *import_filename,
                                              const char *password,
                                              const char *keyfile)
{
    G_D;
    unique_lock<mutex> lkr(d->file_thread_lock);
    d->file_thread_commands.push(new import_from_ps_command(import_filename, password, keyfile));
    d->wc_file_thread.notify_one();
}

static void  __cache_children(QSqlQuery &q, d_t *d,
                              const EntryId &parent_id,
                              int cur_lvl, int target_lvl)
{
    // I don't need a read lock on the parent index, because I'm the only thread that is
    //  allowed to write to it. Only when I write to it I need the write lock.
    if(cur_lvl == target_lvl){
        if(d->parent_index.find(parent_id) == d->parent_index.end()){
            vector<entry_cache> child_rows = __fetch_entry_rows_by_parentid(q, parent_id);
            Vector<EntryId> child_ids(child_rows.size());
            for(const entry_cache &er : child_rows)
                child_ids.PushBack(er.id);

            // Add the parent id with its children to the parent index, and add
            //  all the children to the main index
            d->index_lock.lock();
            for(const entry_cache &e : child_rows){
                auto i = d->index.find(e.id);
                if(i == d->index.end())
                    d->index.emplace(e.id, e);
                else
                    i->second = e;
            }
            d->parent_index.emplace(parent_id, child_ids);
            d->index_lock.unlock();
            d->wc_index.notify_all();
        }
    }
    else if(cur_lvl < target_lvl){
        GASSERT(d->parent_index.find(parent_id) != d->parent_index.end());

        // recurse one level deeper
        for(const EntryId &eid : d->parent_index[parent_id])
            __cache_children(q, d, eid, cur_lvl + 1, target_lvl);
    }
}

void PasswordDatabase::_ew_cache_entries_by_parentid(const QString &conn_str,
                                                     const EntryId &id)
{
    G_D;
    QSqlQuery q(QSqlDatabase::database(conn_str));

    // Make sure the parent id is in the index
    if(!id.IsNull() && d->index.find(id) == d->index.end()){
        entry_cache ec = __fetch_entry_row_by_id(q, id);
        d->index_lock.lock();
        if(ec.id.IsNull())
            ec.exists = false;
        d->index.emplace(id, ec);
        d->index_lock.unlock();
        d->wc_index.notify_all();
    }

    // Fetch the parent's children and 2 levels of grandchildren to maximize
    //  GUI response time because they display in a treeview
    for(int lvl = 0; lvl < 3; ++lvl)
        __cache_children(q, d, id, 0, lvl);
}

void PasswordDatabase::_ew_refresh_favorites(const QString &conn_str)
{
    G_D;
    QSqlQuery q("SELECT * FROM Entry WHERE Favorite >= 0 ORDER BY Favorite ASC",
                QSqlDatabase::database(conn_str));
    Vector<EntryId> ids;
    vector<entry_cache> rows;
    while(q.next()){
        rows.emplace_back(__convert_record_to_entry_cache(q.record()));
        ids.PushBack(rows.back().id);
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

void PasswordDatabase::_ew_set_favorites(const QString &conn_str, const Vector<EntryId> &favs)
{
    G_D;
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    Vector<entry_cache> rows;
    db.transaction();
    try{
        // First clear all existing favorites
        QSqlQuery q("UPDATE Entry SET Favorite=-1", db);

        // Then apply favorites in the order they were given
        for(uint i = 0; i < favs.Length(); ++i){
            q.prepare("UPDATE Entry SET Favorite=? WHERE Id=?");
            q.addBindValue(i + 1);
            q.addBindValue((QByteArray)favs[i]);
            DatabaseUtils::ExecuteQuery(q);

            rows.PushBack(__fetch_entry_row_by_id(q, favs[i]));
        }
    }
    catch(...){
        db.rollback();
        throw;
    }
    db.commit();

    // Update the index, now that the database was updated
    d->index_lock.lock();
    {
        // Update the favorite index
        d->favorite_index = favs;

        // Make sure we update the cache too
        for(const entry_cache &row : rows)
            d->index[row.id] = row;
    }
    d->index_lock.unlock();
    d->wc_index.notify_all();
    emit NotifyFavoritesUpdated();
}


void PasswordDatabase::_entry_worker(GUtil::CryptoPP::Cryptor *c)
{
    G_D;
    // We will delete the cryptor
    SmartPointer<GUtil::CryptoPP::Cryptor> bgCryptor(c);
    const QString conn_str = __create_connection(d->filePath);

    unique_lock<mutex> lkr(d->entry_thread_lock);
    while(!d->closing)
    {
        // Wait for something to do
        d->wc_entry_thread.wait(lkr, [&]{
            return d->closing || !d->entry_thread_commands.empty();
        });

        // Empty the command queue, even if closing
        while(!d->entry_thread_commands.empty())
        {
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
                    _ew_add_entry(conn_str, *bgCryptor, aec->entry);
                }
                    break;
                case bg_worker_command::EditEntry:
                {
                    update_entry_command *uec = static_cast<update_entry_command *>(cmd.Data());
                    _ew_update_entry(conn_str, *bgCryptor, uec->entry);
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
                case bg_worker_command::FetchEntrysByParentId:
                {
                    cache_entries_by_parentid_command *fepc = static_cast<cache_entries_by_parentid_command *>(cmd.Data());
                    _ew_cache_entries_by_parentid(conn_str, fepc->Id);
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
}

void PasswordDatabase::_file_worker(GUtil::CryptoPP::Cryptor *c)
{
    G_D;
    // We will delete the cryptor
    SmartPointer<GUtil::CryptoPP::Cryptor> bgCryptor(c);
    const QString conn_str = __create_connection(d->filePath);

    // Begin the thread loop
    unique_lock<mutex> lkr(d->file_thread_lock);
    while(!d->closing)
    {
        // Wait for something to do
        d->wc_file_thread.wait(lkr, [&]{
            d->cancel_file_thread = false;
            return d->closing || !d->file_thread_commands.empty();
        });

        // Empty the command queue, even if closing
        while(!d->file_thread_commands.empty())
        {
            SmartPointer<bg_worker_command> cmd(d->file_thread_commands.front());
            d->file_thread_commands.pop();
            lkr.unlock();
            try
            {
                // Always notify that the task is complete, even if it's an error
                finally([=]{ emit NotifyProgressUpdated(100); });

                // Process the command (long task)
                switch(cmd->CommandType)
                {
                case bg_worker_command::UpdateFile:
                {
                    update_file_command *afc = static_cast<update_file_command *>(cmd.Data());
                    _fw_add_file(conn_str, *bgCryptor, afc->ID, afc->FilePath);
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
                    _fw_export_to_gps(conn_str, *bgCryptor, e2ps->FilePath, e2ps->Password, e2ps->KeyFile);
                }
                    break;
                case bg_worker_command::ImportFromPS:
                {
                    import_from_ps_command *e2ps = static_cast<import_from_ps_command *>(cmd.Data());
                    _fw_import_from_gps(conn_str, *bgCryptor, e2ps->FilePath, e2ps->Password, e2ps->KeyFile);
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
}

void PasswordDatabase::_fw_add_file(const QString &conn_str,
                                     GUtil::CryptoPP::Cryptor &cryptor,
                                     const FileId &id,
                                     const char *filepath)
{
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);
    const QString filename(QFileInfo(filepath).fileName());

    // First upload and encrypt the file
    QByteArray file_data;
    {
        QByteArrayOutput o(file_data);
        File f(filepath);
        f.Open(File::OpenRead);

        // Get a fresh nonce
        byte nonce[NONCE_LENGTH];
        db.transaction();
        try{
            __get_nonce(conn_str, nonce);
        } catch(...) {
            db.rollback();
            throw;
        }
        db.commit();

        m_progressMin = 0, m_progressMax = 75;
        m_curTaskString = QString("Download and encrypt: %1").arg(filename);
        cryptor.EncryptData(&o, &f, NULL, nonce, DEFAULT_CHUNK_SIZE, this);
    }

    _fw_fail_if_cancelled();
    m_curTaskString = QString("Inserting: %1").arg(filename);
    emit NotifyProgressUpdated(m_progressMax, m_curTaskString);

    db.transaction();
    try{
        bool exists = _file_exists(q, id);
        _fw_fail_if_cancelled();
        if(exists)
        {
            // Update the existing file
            q.prepare("UPDATE File SET Length=?,Data=? WHERE ID=?");
        }
        else
        {
            // Insert a new file
            q.prepare("INSERT INTO File (Length, Data, ID) VALUES (?,?,?)");
        }
        q.addBindValue(file_data.length());
        q.addBindValue(file_data, QSql::In | QSql::Binary);
        q.addBindValue((QByteArray)id, QSql::In | QSql::Binary);
        DatabaseUtils::ExecuteQuery(q);

        // One last chance before we commit
        _fw_fail_if_cancelled();
    }
    catch(...){
        db.rollback();
        throw;
    }
    db.commit();
    emit NotifyProgressUpdated(100, m_curTaskString);
}

void PasswordDatabase::_fw_exp_file(const QString &conn_str,
                                     GUtil::CryptoPP::Cryptor &cryptor,
                                     const FileId &id,
                                     const char *filepath)
{
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);
    const QString filename(QFileInfo(filepath).fileName());

    m_curTaskString = QString(tr("Decrypt and Export: %1")).arg(filename);
    emit NotifyProgressUpdated(0, m_curTaskString);

    // First fetch the file from the database
    q.prepare("SELECT Data FROM File WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);

    if(!q.next())
        throw Exception<>("File ID not found");
    _fw_fail_if_cancelled();
    emit NotifyProgressUpdated(25, m_curTaskString);

    const QByteArray encrypted_data(q.record().value(0).toByteArray());
    {
        QByteArrayInput i(encrypted_data);
        File f(filepath);
        f.Open(File::OpenReadWriteTruncate);

        _fw_fail_if_cancelled();
        emit NotifyProgressUpdated(35, m_curTaskString);

        m_progressMin = 35, m_progressMax = 100;
        cryptor.DecryptData(&f, &i, NULL, DEFAULT_CHUNK_SIZE, this);
    }
    emit NotifyProgressUpdated(100, m_curTaskString);
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
        QDomNode new_node = XmlConverter::AppendToXmlNode(e, n, xdoc);
        __append_children_to_xml(xdoc, new_node, q, cryptor, e.GetId());
    }
}

void PasswordDatabase::_fw_export_to_gps(const QString &conn_str,
                                         GUtil::CryptoPP::Cryptor &my_cryptor,
                                         const char *ps_filepath,
                                         const char *password, const char *keyfile)
{
    int progress_counter = 0;
    m_curTaskString = QString(tr("Exporting to Portable Safe: %1"))
                            .arg(QFileInfo(ps_filepath).fileName());
    emit NotifyProgressUpdated(progress_counter, m_curTaskString);

    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);

    // We export inside a database transaction. Even though we're reading only
    //  this makes sure nobody changes anything while we're exporting
    db.transaction();
    try{
        emit NotifyProgressUpdated(progress_counter+=5, m_curTaskString);

        // First get the main payload: the entire entry structure in XML
        QDomDocument xdoc;
        QDomNode root = xdoc.appendChild(xdoc.createElement("Grypto_XML"));
        __append_children_to_xml(xdoc, root, q, my_cryptor, EntryId::Null());

        emit NotifyProgressUpdated(progress_counter+=15, m_curTaskString);
        _fw_fail_if_cancelled();

        // Open the target file
        GPSFile_Export gps_file(ps_filepath, password, keyfile);
        emit NotifyProgressUpdated(progress_counter+=5, m_curTaskString);
        _fw_fail_if_cancelled();

        // Compress the entry data and write it as the main payload
        {
            const QByteArray xml_compressed(qCompress(xdoc.toByteArray(-1), 9));
            gps_file.AppendPayload((byte const *)xml_compressed.constData(),
                                   xml_compressed.length());
        }
        emit NotifyProgressUpdated(progress_counter+=10, m_curTaskString);
        _fw_fail_if_cancelled();

        // Now let's export all the files as attachments
        int file_cnt = 0;
        const int remaining_progress = 100 - progress_counter;
        const vector<pair<FileId, quint32>> file_list = GetFileSummary();
        for(const pair<FileId, quint32> &p : file_list)
        {
            // Select a file from the database
            q.prepare("SELECT Data FROM File WHERE ID=?");
            q.addBindValue((QByteArray)p.first);
            DatabaseUtils::ExecuteQuery(q);
            _fw_fail_if_cancelled();

            if(!q.next())
                continue;

            // Decrypt it in memory
            Vector<char> pt;
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
                                   (byte const *)p.first.ConstData(), FileId::Size);

            ++file_cnt;
            emit NotifyProgressUpdated(
                        progress_counter + (remaining_progress*((float)file_cnt/file_list.size())),
                        m_curTaskString);
            _fw_fail_if_cancelled();
        }
    }
    catch(...){
        db.rollback();
        throw;
    }
    db.commit();    // Nothing should have changed, is a rollback better here?
    emit NotifyProgressUpdated(100, m_curTaskString);
}

void PasswordDatabase::_fw_import_from_gps(const QString &conn_str,
                                           GUtil::CryptoPP::Cryptor &my_cryptor,
                                           const char *ps_filepath,
                                           const char *password, const char *keyfile)
{
    throw NotImplementedException<>();
}


// This function belongs to the background thread and will always execute there
void PasswordDatabase::ProgressUpdated(int prg)
{
    // prg is between 0 and 100, so scale it to m_progressMax-m_progressMin
    emit NotifyProgressUpdated(m_progressMin + ((float)prg*(m_progressMax-m_progressMin))/100,
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

QByteArray const &PasswordDatabase::FilePath() const
{
    G_D;
    return d->filePath;
}

GUtil::CryptoPP::Cryptor const &PasswordDatabase::Cryptor() const
{
    G_D;
    return *d->cryptor;
}


END_NAMESPACE_GRYPTO;

