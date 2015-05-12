/*Copyright 2014-2015 George Karagoulis

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
#include <grypto/entry.h>
#include <gutil/cryptopp_rng.h>
#include <gutil/gpsutils.h>
#include <gutil/databaseutils.h>
#include <gutil/sourcesandsinks.h>
#include <gutil/qtsourcesandsinks.h>
#include <queue>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <QString>
#include <QSet>
#include <QQueue>
#include <QFileInfo>
#include <QFile>
#include <QResource>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDomDocument>
#include <QLockFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QTemporaryFile>
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

#define GRYPTO_XML_VERSION  "3.0"

#define MAX_TRY_COUNT 20

namespace Grypt{
class bg_worker_command;
}

// A cached entry row from the database
struct entry_cache{
    Grypt::EntryId id, parentid;
    Grypt::FileId file_id;
    int row;
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
    QList<Grypt::EntryId> children;
};

struct file_cache{
    Grypt::FileId id;
    int length;

    file_cache() :length(-1) {}
    file_cache(const Grypt::FileId &fid, int len = -1)
        :id(fid), length(len) {}
};

namespace
{
struct d_t
{
    // Main thread member variables
    QString dbString;
    unique_ptr<GUtil::CryptoPP::Cryptor> cryptor;

    // Background thread
    thread worker;

    // Variables for the background threads
    mutex thread_lock;
    condition_variable wc_thread;
    queue<Grypt::bg_worker_command *> thread_commands;
    bool cancel_thread;
    bool thread_cancellable;
    bool thread_idle;

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
        :cancel_thread(false),
          thread_cancellable(false),
          thread_idle(false),
          closing(false)
    {}
};
}


class QFileIO : public GUtil::IInput, public GUtil::IOutput
{
    QFile &ref;
public:
    QFileIO(QFile &f) :ref(f) {}
    virtual GUINT32 ReadBytes(byte *buffer, GUINT32 buffer_len, GUINT32 bytes_to_read){
        return ref.read((char *)buffer, Min(buffer_len, bytes_to_read));
    }
    virtual GUINT32 WriteBytes(const byte *data, GUINT32 len){
        return ref.write((const char *)data, len);
    }

    virtual GUINT32 BytesAvailable() const{ return ref.bytesAvailable(); }
    virtual void Flush(){ ref.flush(); }
};


static void __open_file_or_die(QFile &f, QFile::OpenMode mode)
{
    if(!f.open(mode))
        throw Exception<>(QString("Cannot open file: %1")
                          .arg(f.errorString()).toUtf8());
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
        pdb->AddFile(e.GetFileId(), e.GetFilePath().toUtf8().constData());
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

static QByteArray __generate_crypttext(Cryptor &cryptor, const Entry &e)
{
    QByteArray crypttext;
    QByteArrayInput i(XmlConverter::ToXmlString(e));
    QByteArrayOutput o(crypttext);
    cryptor.EncryptData(&o, &i);
    return crypttext;
}

static entry_cache __convert_entry_to_cache(const Entry &e, Cryptor &cryptor)
{
    entry_cache ret(e);
    ret.crypttext = __generate_crypttext(cryptor, e);
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

static QList<entry_cache> __fetch_entry_rows_by_parentid(QSqlQuery &q, const EntryId &id)
{
    QList<entry_cache> ret;
    bool isnull = id.IsNull();
    q.prepare(QString("SELECT * FROM Entry WHERE ParentID%1 ORDER BY Row ASC")
              .arg(isnull ? " IS NULL" : "=?"));
    if(!isnull)
        q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
    while(q.next())
        ret.append(__convert_record_to_entry_cache(q.record()));
    return ret;
}

static QList<Entry> __find_entries_by_parent_id(QSqlQuery &q,
                                                GUtil::CryptoPP::Cryptor &cryptor,
                                                const EntryId &id)
{
    QList<Entry> ret;
    foreach(const entry_cache &er, __fetch_entry_rows_by_parentid(q, id))
        ret.append(__convert_cache_to_entry(er, cryptor));
    return ret;
}

static void __delete_file_by_id(const QString &conn_str, const FileId &id)
{
    QSqlQuery q(QSqlDatabase::database(conn_str));
    q.prepare("DELETE FROM File WHERE ID=?");
    q.addBindValue((QByteArray)id);
    DatabaseUtils::ExecuteQuery(q);
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
        AddFile,
        DeleteFile,
        ExportFile,
        ExportToPS,
        ImportFromPS,
        ExportToXML,
        ImportFromXML,

        // Misc commands
        DispatchOrphans,
        CheckAndRepair
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

class add_file_command : public bg_worker_command
{
public:
    add_file_command(const FileId &id, const char *path)
        :bg_worker_command(AddFile),
          ID(id),
          FilePath(path)
    {}
    add_file_command(const FileId &id, const QByteArray &contents)
        :bg_worker_command(AddFile),
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
    export_to_ps_command(const QString &filepath, const Credentials &creds)
        :bg_worker_command(ExportToPS),
          FilePath(filepath),
          Creds(creds)
    {}
    const QString FilePath;
    Credentials Creds;
};

class import_from_ps_command : public bg_worker_command
{
public:
    import_from_ps_command(const QString &filepath, const Credentials &creds)
        :bg_worker_command(ImportFromPS),
          FilePath(filepath),
          Creds(creds)
    {}
    const QString FilePath;
    Credentials Creds;
};

class export_to_xml_command : public bg_worker_command
{
public:
    export_to_xml_command(const QString &filepath)
        :bg_worker_command(ExportToXML),
          FilePath(filepath)
    {}
    const QString FilePath;
};

class import_from_xml_command : public bg_worker_command
{
public:
    import_from_xml_command(const QString &filepath)
        :bg_worker_command(ImportFromXML),
          FilePath(filepath)
    {}
    const QString FilePath;
};

class check_and_repair_command : public bg_worker_command
{
public:
    check_and_repair_command()
        :bg_worker_command(CheckAndRepair)
    {}
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

static void __init_cryptor(d_t *d, GUtil::CryptoPP::Cryptor *ctor)
{
    GASSERT(!d->cryptor);
    d->cryptor.reset(ctor);
}

static GUtil::CryptoPP::Cryptor *__produce_cryptor(const Credentials &creds,
                                                   const byte *salt,
                                                   GUINT32 salt_len)
{
    return new GUtil::CryptoPP::Cryptor(creds, NONCE_LENGTH,
                                        new Cryptor::DefaultKeyDerivation(salt, salt_len));
}

void PasswordDatabase::_init_cryptor(const Credentials &creds, const byte *s, GUINT32 s_l)
{
    G_D;
    __init_cryptor(d, __produce_cryptor(creds, s, s_l));
}

void PasswordDatabase::_init_cryptor(const GUtil::CryptoPP::Cryptor &cryptor)
{
    G_D;
    __init_cryptor(d, new GUtil::CryptoPP::Cryptor(cryptor));
}

static void __queue_command(d_t *d, bg_worker_command *cmd)
{
    unique_lock<mutex> lkr(d->thread_lock);
    d->thread_commands.push(cmd);
    d->thread_idle = false;
    d->wc_thread.notify_one();
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
    if(!QFile::exists(filepath))
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

bool PasswordDatabase::_try_lock_file()
{
    QFileInfo fi(m_filepath);
    m_lockfile.reset(new QLockFile(QString("%1.LOCKFILE")
                                   .arg(fi.absoluteFilePath())));
    m_lockfile->setStaleLockTime(0);
    return m_lockfile->tryLock(0);
}

PasswordDatabase::PasswordDatabase(const QString &file_path,
                                   function<bool(const ProcessInfo &)> ask_for_lock_override,
                                   QObject *par)
    :QObject(par),
      m_filepath(file_path)
{
    G_D_INIT();

    // Note: Here we don't even check that the file exists, because maybe it wasn't created yet,
    //  but we can still lock the future location of the file path so it's ready when we want to create it.

    // Attempt to lock the database
    if(!_try_lock_file()){
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
    // Cache the entire entry table in one query
    QSqlQuery q("SELECT * FROM Entry ORDER BY ParentId,Row ASC",
                QSqlDatabase::database(d->dbString));
    {
        QHash<EntryId, QList<EntryId>> hierarchy;
        QHash<EntryId, entry_cache> entries;
        while(q.next()){
            entry_cache ec = __convert_record_to_entry_cache(q.record());
            hierarchy[ec.parentid].append(ec.id);
            entries[ec.id] = ec;
        }

        // Load all entries connected to the root
        function<void(const EntryId &)> parse_child_entries;
        parse_child_entries = [&](const EntryId &pid){
            QList<EntryId> &child_list =
                    d->parent_index.emplace(pid, parent_cache()).first->second.children;

            for(const EntryId &cid : hierarchy[pid]){
                child_list.append(cid);

                // Add the entry to the cache
                const entry_cache &ec = entries[cid];
                d->index.emplace(cid, ec);

                // We'll sort the favorites at the end
                if(0 <= ec.favoriteindex)
                    d->favorite_index.append(cid);
            }

            for(const EntryId &cid : child_list)
                parse_child_entries(cid);
        };
        parse_child_entries(EntryId::Null());
    }

    // Then cache the file ID's
    q.exec("SELECT ID,Length FROM File");
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
                    int lhs_fav = d->index[lhs].favoriteindex;
                    int rhs_fav = d->index[rhs].favoriteindex;
                    return lhs_fav != 0 && (rhs_fav == 0 || lhs_fav < rhs_fav);
                }
    );
}

void PasswordDatabase::Open(const Credentials &creds)
{
    _open([&](byte const *salt){
        vector<byte> s(SALT_LENGTH);
        if(salt){
            // Use the salt we were given
            memcpy(s.data(), salt, SALT_LENGTH);
        }
        else{
            // Generate new random salt
            GUtil::CryptoPP::RNG().Fill(s.data(), SALT_LENGTH);
        }
        _init_cryptor(creds, s.data(), s.size());
    });
}

void PasswordDatabase::Open(const GUtil::CryptoPP::Cryptor &c)
{
    _open([&](byte const *){
        // Initialize the cryptor as a copy of the input (the salt is ignored)
        _init_cryptor(c);
    });
}

static void __insert_entry(const entry_cache &ec, QSqlQuery &q)
{
    q.prepare("INSERT INTO Entry (ID,ParentID,Row,Favorite,FileID,Data)"
              " VALUES (?,?,?,?,?,?)");
    q.addBindValue((QByteArray)ec.id);
    q.addBindValue((QByteArray)ec.parentid);
    q.addBindValue(ec.row);
    q.addBindValue(ec.favoriteindex);
    q.addBindValue((QByteArray)ec.file_id);
    q.addBindValue(ec.crypttext);
    DatabaseUtils::ExecuteQuery(q);
}

static void __create_new_database(QSqlDatabase &db,
                                  GUtil::CryptoPP::Cryptor &cryptor)
{
    __init_sql_resources();
    QResource rs(":/grypto/sql/create_db.sql");
    GASSERT(rs.isValid());

    QByteArray sql;
    if(rs.isCompressed())
        sql = qUncompress(rs.data(), rs.size());
    else
        sql = QByteArray((const char *)rs.data(), rs.size());
    DatabaseUtils::ExecuteScript(db, sql);

    // Prepare the keycheck data
    QByteArray keycheck_ct;
    {
        ByteArrayInput auth_in(__keycheck_string, strlen(__keycheck_string));
        QByteArrayOutput ba_out(keycheck_ct);
        cryptor.EncryptData(&ba_out, NULL, &auth_in);
    }

    Cryptor::DefaultKeyDerivation const &kdf =
        (const Cryptor::DefaultKeyDerivation &)cryptor.GetKeyDerivationFunction();

    // Insert a version record
    QSqlQuery q(db);
    q.prepare("INSERT INTO Version (Version,Salt,KeyCheck)"
                " VALUES (?,?,?)");
    q.addBindValue(GRYPTO_DATABASE_VERSION);
    q.addBindValue(QByteArray((const char *)kdf.Salt(), kdf.SaltLength()));
    q.addBindValue(keycheck_ct);
    DatabaseUtils::ExecuteQuery(q);
}

void PasswordDatabase::_open(function<void(byte const *)> init_cryptor)
{
    if(IsOpen())
        throw Exception<>("Database already opened");

    G_D;
    bool file_exists = QFile::exists(m_filepath);
    QString dbstring = __create_connection(m_filepath);   // After we check if the file exists
    try
    {
        QSqlDatabase db = QSqlDatabase::database(dbstring);
        QSqlQuery q(db);

        if(!file_exists){
            // Initialize the new database if it doesn't exist
            init_cryptor(NULL);
            __create_new_database(db, *d->cryptor);
        }

        // Check the version record to see if it is valid
        __check_version(dbstring);

        // Validate the keycheck information
        q.prepare("SELECT KeyCheck,Salt FROM Version");
        DatabaseUtils::ExecuteQuery(q);

        if(q.next()){
            if(!d->cryptor){
                QByteArray salt_ba = q.record().value("Salt").toByteArray();
                init_cryptor((byte const *)salt_ba.constData());
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
        d->cryptor.reset(NULL);
        throw;
    }

    // Set the db string to denote that we've opened the file
    d->dbString = dbstring;

    // Initialize the cache before starting the workers
    __initialize_cache(d);

    // Start up the background threads
    d->worker = std::thread(&PasswordDatabase::_background_worker, this, new GUtil::CryptoPP::Cryptor(*d->cryptor));

    // We must wait for the background thread to idle to avoid a race condition
    WaitForThreadIdle();
}

bool PasswordDatabase::IsOpen() const
{
    G_D;
    return !d->dbString.isEmpty();
}

void PasswordDatabase::SaveAs(const QString &filename, const Credentials &creds)
{
    G_D;
    QString file_path = QFileInfo(filename).absoluteFilePath();
    if(file_path == QFileInfo(m_filepath).absoluteFilePath())
        throw Exception<>("Cannot save as current file");

    {
        QFile f(filename);
        if(f.exists()){
            if(!f.remove())
                throw Exception<>(QString(tr("Unable to remove existing file: %1"))
                                  .arg(f.errorString()).toUtf8());
        }
    }

    // Make sure the background thread has finished whatever it's working on
    WaitForThreadIdle();

    byte salt[SALT_LENGTH];
    GUtil::CryptoPP::RNG().Fill(salt, SALT_LENGTH);
    unique_ptr<GUtil::CryptoPP::Cryptor> cryptor(__produce_cryptor(creds, salt, SALT_LENGTH));

    QString dbString = __create_connection(file_path);
    try
    {
        QSqlDatabase db = QSqlDatabase::database(dbString);
        QSqlQuery q(QSqlDatabase::database(d->dbString));
        QSqlQuery q_new(db);

        // Create a blank new database
        __create_new_database(db, *cryptor);

        db.transaction();
        try{
            unique_lock<mutex> lkr(d->index_lock);

            // Write all the entries and files from the cache to the new database
            QList<FileId> file_list;
            function<void(const EntryId &)> write_child_entries;
            write_child_entries = [&](const EntryId &pid){
                for(const EntryId &cid : d->parent_index[pid].children){
                    // Decrypt the entry with the old cryptor
                    Entry e = __convert_cache_to_entry(d->index[cid], *d->cryptor);

                    // Encrypt the entry with the new cryptor and insert
                    __insert_entry(__convert_entry_to_cache(e, *cryptor), q_new);

                    if(!e.GetFileId().IsNull())
                        file_list.append(e.GetFileId());
                    write_child_entries(cid);
                }
            };
            write_child_entries(EntryId::Null());

            // Select each file one at a time and insert it into the new database
            for(const FileId &fid : file_list){
                q.prepare("SELECT Length,Data FROM File WHERE Id=?");
                q.addBindValue((QByteArray)fid);
                DatabaseUtils::ExecuteQuery(q);

                if(q.next()){
                    QByteArray crypttext = q.value("Data").toByteArray();
                    QByteArray tmp;
                    {
                        // First decrypt the data with the original cryptor
                        QByteArrayInput bai(crypttext);
                        QByteArrayOutput bao(tmp);
                        d->cryptor->DecryptData(&bao, &bai);
                    }
                    crypttext.clear();
                    {
                        // Then encrypt it with the new cryptor
                        QByteArrayInput bai(tmp);
                        QByteArrayOutput bao(crypttext);
                        cryptor->EncryptData(&bao, &bai);
                    }
                    tmp.clear();

                    q_new.prepare("INSERT INTO File (Id,Length,Data) VALUES (?,?,?)");
                    q_new.addBindValue((QByteArray)fid);
                    q_new.addBindValue(q.value("Length").toInt());
                    q_new.addBindValue(crypttext);
                    DatabaseUtils::ExecuteQuery(q_new);
                }
            }
        } catch(...) {
            db.rollback();
            throw;
        }
        db.commit();
    }
    catch(...)
    {
        QSqlDatabase::removeDatabase(dbString);
        throw;
    }
    QSqlDatabase::removeDatabase(dbString);

    // These commands completely close out and reset the state
    //  of this object. It's as if calling the destructor and constructor.
    _close();
    G_D_INIT();

    m_filepath = file_path;
    if(!_try_lock_file())
        throw Exception<>("Unable to lock newly saved file??");

    Open(*cryptor);
}

void PasswordDatabase::_close()
{
    G_D;
    if(IsOpen()){
        // Let's clean up any orphaned entries/files
        DeleteOrphans();

        d->thread_lock.lock();
        d->closing = true;
        d->wc_thread.notify_one();
        d->thread_lock.unlock();

        d->worker.join();

        QSqlDatabase::removeDatabase(d->dbString);
    }
    m_lockfile->unlock();
    G_D_UNINIT();
}

void PasswordDatabase::CheckAndRepairDatabase()
{
    G_D;
    __queue_command(d, new check_and_repair_command);
}

PasswordDatabase::~PasswordDatabase()
{
    _close();
}

bool PasswordDatabase::CheckCredentials(const Credentials &creds) const
{
    G_D;
    FailIfNotOpen();
    return d->cryptor->CheckCredentials(creds);
}

Credentials::TypeEnum PasswordDatabase::GetCredentialsType() const
{
    G_D;
    FailIfNotOpen();
    return d->cryptor->GetCredentialsType();
}

static void __commit_transaction(QSqlDatabase &db)
{
    if(!db.commit())
        throw Exception<>(db.lastError().text().toUtf8().constData());
}

void PasswordDatabase::_bw_add_entry(const QString &conn_str, const Entry &e)
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
        d->index_lock.lock();

        // If the index is not present, then it was already deleted
        auto iter = d->index.find(e.GetId());
        if(iter == d->index.end())
            return;

        iter->second.row = row;
        entry_cache const &ec = iter->second;
        d->index_lock.unlock();

        __insert_entry(ec, q);

        emit NotifyProgressUpdated(65, false, task_string);
        success = true;
    }

    // Add any new files to the database
    __add_new_files(this, e);
}

void PasswordDatabase::_bw_update_entry(const QString &conn_str, const Entry &e)
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

void PasswordDatabase::_bw_delete_entry(const QString &conn_str,
                                        const EntryId &id)
{
    QSqlDatabase db = QSqlDatabase::database(conn_str);
    QSqlQuery q(db);
    QString task_string = tr("Deleting entry");
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

        q.prepare("SELECT ParentId,Row FROM Entry WHERE Id=?");
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);
        if(!q.next())
            throw Exception<>("Entry not found");

        emit NotifyProgressUpdated(10, false, task_string);

        pid = q.record().value("ParentId").toByteArray();
        row = q.record().value("Row").toInt();

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
}

void PasswordDatabase::_bw_move_entry(const QString &conn_str,
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

void PasswordDatabase::AddEntry(Entry &e, bool gen_id)
{
    FailIfNotOpen();
    G_D;

    if(gen_id)
        e.SetId(EntryId::NewId());

    // Generate the entry cache
    entry_cache ec(__convert_entry_to_cache(e, *d->cryptor));

    // Update the index
    bool favs_updated = false;
    unique_lock<mutex> lkr(d->index_lock);
    {
        // Add to the appropriate parent
        auto pi = d->parent_index.find(e.GetParentId());
        QList<EntryId> &child_ids = pi->second.children;
        if(0 > e.GetRow() || e.GetRow() > child_ids.length())
            e.SetRow(child_ids.length());

        child_ids.insert(e.GetRow(), e.GetId());

        // Adjust the siblings
        for(int i = e.GetRow() + 1; i < child_ids.length(); ++i){
            auto iter = d->index.find(child_ids[i]);
            if(iter != d->index.end())
                iter->second.row = i;
        }

        d->index[e.GetId()] = ec;
        if(d->deleted_entries.contains(e.GetId())){
            d->deleted_entries.remove(e.GetId());

            // Restore any children which happen to be favorites
            QMap<int, EntryId> favorites;
            function<void(const EntryId &)> restore_favorites;
            restore_favorites = [&](const EntryId &eid){
                // If this ID is a favorite, add it to the index
                auto i = d->index.find(eid);
                if(i == d->index.end())
                    return;
                else if(0 <= i->second.favoriteindex){
                    if(0 < i->second.favoriteindex){
                        GASSERT(i->second.favoriteindex <= d->favorite_index.length());
                        favorites.insert(i->second.favoriteindex-1, eid);
                    }
                    else
                        d->favorite_index.append(eid);
                    favs_updated = true;
                }

                // Recursively check children
                auto pi = d->parent_index.find(eid);
                if(pi != d->parent_index.end()){
                    for(const EntryId &cid : pi->second.children)
                        restore_favorites(cid);
                }
            };
            restore_favorites(e.GetId());

            for(int k : favorites.keys())
                d->favorite_index.insert(k, favorites[k]);
        }

        if(d->parent_index.find(e.GetId()) == d->parent_index.end())
            d->parent_index[e.GetId()] = parent_cache();
    }
    lkr.unlock();
    d->wc_index.notify_all();

    // Tell the worker thread to add it to the database
    __queue_command(d, new add_entry_command(e));

    // Clear the file path so we don't add the same file twice
    e.SetFilePath(QString::null);

    // New favorites must be unordered when added to the database
    if(e.IsFavorite())
        e.SetFavoriteIndex(0);

    if(favs_updated)
        emit NotifyFavoritesUpdated();
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

    __queue_command(d, new update_entry_command(e));

    // Clear the file path so we don't add the same file twice
    e.SetFilePath(QString::null);

    if(old_favorite_index != e.GetFavoriteIndex())
        emit NotifyFavoritesUpdated();
}

bool PasswordDatabase::HasAncestor(const EntryId &child, const EntryId &ancestor) const
{
    G_D;
    lock_guard<mutex> lkr(d->index_lock);
    return _has_ancestor(child, ancestor);
}

bool PasswordDatabase::_has_ancestor(const EntryId &child, const EntryId &ancestor) const
{
    G_D;
    if(child == ancestor)
        return true;

    auto i = d->index.find(child);
    if(i == d->index.end())
        return false;

    return _has_ancestor(i->second.parentid, ancestor);
}

void PasswordDatabase::DeleteEntry(const EntryId &id)
{
    FailIfNotOpen();
    G_D;
    if(id.IsNull())
        return;

    // Update the index
    bool favs_updated = false;
    unique_lock<mutex> lkr(d->index_lock);
    auto iter = d->index.find(id);
    if(iter != d->index.end()){
        auto piter = d->parent_index.find(iter->second.parentid);
        piter->second.children.removeAt(iter->second.row);

        // Update the siblings of the item we just removed
        for(auto r = iter->second.row; r < piter->second.children.length(); ++r){
            auto ci = d->index.find(piter->second.children[r]);
            if(ci != d->index.end())
                ci->second.row = r;
        }

        // Remove this or any children from the favorites list
        for(int i = d->favorite_index.length() - 1; i >= 0; i--){
            if(_has_ancestor(d->favorite_index[i], iter->first)){
                d->favorite_index.removeAt(i);
                favs_updated = true;
            }
        }

        d->index.erase(iter);
        d->deleted_entries.insert(id);

        // Don't remove from the parent index, because they may un-delete it
        //d->parent_index.erase(id);
    }
    lkr.unlock();
    d->wc_index.notify_all();

    // Remove it from the database
    __queue_command(d, new delete_entry_command(id));

    if(favs_updated)
        emit NotifyFavoritesUpdated();
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

    QList<EntryId> &src = d->parent_index.find(parentId_src)->second.children;
    QList<EntryId> &dest = d->parent_index.find(parentId_dest)->second.children;
    QList<EntryId> moving_rows;
    moving_rows.reserve(move_cnt);

    if((int)row_first >= src.length() || (int)row_last >= src.length() ||
            (same_parents && (int)row_dest > dest.length()) ||
            (!same_parents && (int)row_dest > dest.length()))
        throw Exception<>("Invalid move parameters");

    // Extract the rows from the source parent
    for(uint i = row_first; i <= row_last; ++i){
        moving_rows.append(src[row_first]);
        src.removeAt(row_first);
    }

    // Update the siblings at the source
    for(int i = row_first; i < src.length(); ++i)
        d->index.find(src[i])->second.row = i;

    // Insert the rows at the dest parent
    if(same_parents && row_dest > row_first)
        row_dest -= move_cnt;

    for(int i = 0; i < move_cnt; ++i){
        dest.insert(row_dest + i, moving_rows[i]);
        d->index.find(moving_rows[i])->second.parentid = parentId_dest;
    }

    // Update the siblings at the dest
    for(int i = row_dest; i < dest.length(); ++i)
        d->index.find(dest[i])->second.row = i;
    lkr.unlock();

    // Update the database
    __queue_command(d, new move_entry_command(parentId_src, row_first, row_last, parentId_dest, row_dest_orig));
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
        ret = pi->second.children.length();
    return ret;
}

QList<Entry> PasswordDatabase::FindEntriesByParentId(const EntryId &pid) const
{
    FailIfNotOpen();
    G_D;

    QList<Entry> ret;
    {
        unique_lock<mutex> lkr(d->index_lock);

        foreach(const EntryId &child_id, d->parent_index[pid].children){
            GASSERT(d->index.find(child_id) != d->index.end());
            ret.append(__convert_cache_to_entry(d->index[child_id], *d->cryptor));
        }
    }
    return ret;
}

void PasswordDatabase::RefreshFavorites()
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new refresh_favorite_entries_command);
}

void PasswordDatabase::SetFavoriteEntries(const QList<EntryId> &favs)
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new set_favorite_entries_command(favs));

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
    __queue_command(d, new add_favorite_entry(id));

    d->index_lock.lock();
    {
        auto iter = d->index.find(id);
        if(iter != d->index.end()){
            d->favorite_index.append(id);
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
    __queue_command(d, new remove_favorite_entry(id));

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
    for(const EntryId &id : d->favorite_index){
        GASSERT(d->index.find(id) != d->index.end());
        rows.append(d->index[id]);
    }
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
    __queue_command(d, new dispatch_orphans_command);
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
    unique_lock<mutex> lkr(d->thread_lock);
    d->cancel_thread = true;

    // Remove all pending commands
    while(!d->thread_commands.empty()){
        delete d->thread_commands.front();
        d->thread_commands.pop();
    }

    // Wake the thread in case he's sleeping, we want him to lower
    //  the cancel flag immediately so it doesn't cancel the next operation
    d->wc_thread.notify_one();
}

QHash<FileId, PasswordDatabase::FileInfo_t> PasswordDatabase::QueryFileSummary() const
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
    WaitForThreadIdle();
    G_D;

    QSet<FileId> ret;
    unique_lock<mutex> lkr(d->index_lock);
    __add_referenced_file_ids(d, ret, EntryId::Null());
    return ret;
}

void PasswordDatabase::AddFile(const FileId &id, const char *filename)
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new add_file_command(id, filename));
}

void PasswordDatabase::AddFile(const FileId &id, const QByteArray &contents)
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new add_file_command(id, contents));
}

void PasswordDatabase::DeleteFile(const FileId &id)
{
    FailIfNotOpen();
    G_D;
    d->index_lock.lock();
    d->file_index.erase(id);
    d->index_lock.unlock();

    __queue_command(d, new delete_file_command(id));
}

void PasswordDatabase::ExportFile(const FileId &id, const char *export_path) const
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new export_file_command(id, export_path));
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

void PasswordDatabase::ExportToPortableSafe(const QString &export_filename,
                                            const Credentials &creds) const
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new export_to_ps_command(export_filename, creds));
}

void PasswordDatabase::ImportFromPortableSafe(const QString &import_filename,
                                              const Credentials &creds)
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new import_from_ps_command(import_filename, creds));
}

void PasswordDatabase::ExportToXml(const QString &export_filename)
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new export_to_xml_command(export_filename));
}

void PasswordDatabase::ImportFromXml(const QString &import_filename)
{
    FailIfNotOpen();
    G_D;
    __queue_command(d, new import_from_xml_command(import_filename));
}

/** Recursively counts all children of the parent. */
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
        AddFile(file_list[i].second, cur_file);
        emit NotifyProgressUpdated((float)i/file_count*100, false, progress_label);
    }

    RefreshFavorites();
}

void PasswordDatabase::_bw_refresh_favorites(const QString &conn_str)
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

void PasswordDatabase::_bw_set_favorites(const QString &conn_str, const QList<EntryId> &favs)
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

void PasswordDatabase::_bw_add_favorite(const QString &conn_str, const EntryId &id)
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

void PasswordDatabase::_bw_remove_favorite(const QString &conn_str, const EntryId &id)
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

void PasswordDatabase::_bw_dispatch_orphans(const QString &conn_str)
{
    G_D;
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    db.transaction();
    try
    {
        struct entry_row{
            EntryId id;
            FileId file_id;
            int favorite;
        };

        QHash<EntryId, entry_row> entries;
        QHash<EntryId, QSet<EntryId>> parent_index;
        QSet<FileId> all_files;
        QSet<FileId> claimed_files;
        QSet<EntryId> favorites;
        QSet<EntryId> claimed_entries;
        QList<EntryId> deleted_entries;
        QList<FileId> deleted_files;

        // Populate an in-memory index of the hierarchy
        QSqlQuery q("SELECT ID,ParentID,FileID,Favorite FROM Entry", db);
        while(q.next()){
            EntryId eid = q.value("ID").toByteArray();
            EntryId pid = q.value("ParentID").toByteArray();
            FileId  fid = q.value("FileID").toByteArray();
            int     fav = q.value("Favorite").toInt();
            entries.insert(eid, {eid, fid, fav});
            parent_index[pid].insert(eid);
            if(0 <= fav)
                favorites.insert(eid);
        }

        // Get a comprehensive list of all files in the database
        q.exec("SELECT Id FROM File");
        while(q.next())
            all_files.insert(q.value("Id").toByteArray());

        // Starting at the root, populate a set of entries that can be reached,
        //  and remember any files that were referenced along the way
        function<void(const EntryId &)> add_children_to_claimed_entries;
        add_children_to_claimed_entries = [&](const EntryId &eid){
            if(parent_index.contains(eid)){
                claimed_entries.unite(parent_index[eid]);
                for(const EntryId &cid : parent_index[eid]){
                    if(!entries[cid].file_id.IsNull())
                        claimed_files.insert(entries[cid].file_id);
                    add_children_to_claimed_entries(cid);
                }
            }
        };
        add_children_to_claimed_entries(EntryId::Null());

        // Now delete any entries that are orphaned:
        for(const EntryId &eid : entries.keys()){
            if(!claimed_entries.contains(eid)){
                q.prepare("DELETE FROM Entry WHERE ID=?");
                q.addBindValue((QByteArray)eid);
                DatabaseUtils::ExecuteQuery(q);

                deleted_entries.append(eid);
                if(0 <= entries[eid].favorite)
                    favorites.remove(eid);
            }
        }

        // Update the favorites now that some may have been removed
        // Make a list of favorites sorted by their index
        QList<EntryId> sorted_favorites = favorites.toList();
        sort(sorted_favorites.begin(), sorted_favorites.end(),
          [&](const EntryId &lhs, const EntryId &rhs){
            return entries[lhs].favorite < entries[rhs].favorite;
        });

        int cur_fav = 1;
        for(const EntryId &eid : sorted_favorites){
            if(0 < entries[eid].favorite){
                if(cur_fav != entries[eid].favorite){
                    q.prepare("UPDATE Entry SET Favorite=? WHERE ID=?");
                    q.addBindValue(cur_fav);
                    q.addBindValue((QByteArray)eid);
                    DatabaseUtils::ExecuteQuery(q);
                }
                cur_fav++;
            }
        }

        if(0 < deleted_entries.count()){
            qDebug("Removed %d orphaned entries...", deleted_entries.count());
        }

        for(const FileId &fid : all_files){
            if(!claimed_files.contains(fid)){
                q.prepare("DELETE FROM File WHERE ID=?");
                q.addBindValue((QByteArray)fid);
                DatabaseUtils::ExecuteQuery(q);
                deleted_files.append(fid);
            }
        }

        if(0 < deleted_files.count()){
            qDebug("Removed %d orphaned files...", deleted_files.count());
        }

        // Update the index:
        unique_lock<mutex> lkr(d->index_lock);
        for(const EntryId &eid : deleted_entries){
            d->index.erase(eid);
            d->parent_index.erase(eid);
        }
        for(const FileId &fid : deleted_files){
            d->file_index.erase(fid);
        }
        for(const EntryId &eid : d->deleted_entries){
            // We delayed removing this from the parent index, but we can do it now
            d->parent_index.erase(eid);
        }
        d->deleted_entries.clear();
        d->favorite_index = sorted_favorites;
        d->wc_index.notify_all();
    }
    catch(...)
    {
        db.rollback();
        throw;
    }
    db.commit();
}


void PasswordDatabase::_background_worker(GUtil::CryptoPP::Cryptor *c)
{
    G_D;
    // We will delete the cryptor
    SmartPointer<GUtil::CryptoPP::Cryptor> bgCryptor(c);
    const QString conn_str = __create_connection(m_filepath);

    unique_lock<mutex> lkr(d->thread_lock);
    while(!d->closing)
    {
        if(!d->thread_idle)
            emit NotifyThreadIdle();

        // Tell everyone who's waiting that we're going idle
        d->thread_idle = true;
        d->wc_thread.notify_all();

        // Wait for something to do
        d->wc_thread.wait(lkr, [&]{
            d->cancel_thread = false;
            return d->closing || !d->thread_commands.empty();
        });

        // Empty the command queue, even if closing
        while(!d->thread_commands.empty())
        {
            // This needs to be set by the one assigning us work
            GASSERT(!d->thread_idle);

            SmartPointer<bg_worker_command> cmd(d->thread_commands.front());
            d->thread_commands.pop();

            // We flush the queue if the user cancelled
            if(d->cancel_thread)
                continue;

            lkr.unlock();
            try
            {
                // Process the command (long task)
                switch(cmd->CommandType)
                {
                case bg_worker_command::AddEntry:
                {
                    add_entry_command *aec = static_cast<add_entry_command *>(cmd.Data());
                    _bw_add_entry(conn_str, aec->entry);
                }
                    break;
                case bg_worker_command::EditEntry:
                {
                    update_entry_command *uec = static_cast<update_entry_command *>(cmd.Data());
                    _bw_update_entry(conn_str, uec->entry);
                }
                    break;
                case bg_worker_command::DeleteEntry:
                {
                    delete_entry_command *dec = static_cast<delete_entry_command *>(cmd.Data());
                    _bw_delete_entry(conn_str, dec->Id);
                }
                    break;
                case bg_worker_command::MoveEntry:
                {
                    move_entry_command *mec = static_cast<move_entry_command *>(cmd.Data());
                    _bw_move_entry(conn_str,
                                   mec->ParentSource, mec->RowFirst, mec->RowLast,
                                   mec->ParentDest, mec->RowDest);
                }
                    break;
                case bg_worker_command::RefreshFavoriteEntries:
                    _bw_refresh_favorites(conn_str);
                    break;
                case bg_worker_command::SetFavoriteEntries:
                {
                    set_favorite_entries_command *sfe = static_cast<set_favorite_entries_command *>(cmd.Data());
                    _bw_set_favorites(conn_str, sfe->Favorites);
                }
                    break;
                case bg_worker_command::AddFavoriteEntry:
                {
                    add_favorite_entry *afe = static_cast<add_favorite_entry *>(cmd.Data());
                    _bw_add_favorite(conn_str, afe->ID);
                }
                    break;
                case bg_worker_command::RemoveFavoriteEntry:
                {
                    remove_favorite_entry *rfe = static_cast<remove_favorite_entry *>(cmd.Data());
                    _bw_remove_favorite(conn_str, rfe->ID);
                }
                    break;
                case bg_worker_command::DispatchOrphans:
                {
                    //dispatch_orphans_command *doc = static_cast<dispatch_orphans_command*>(cmd.Data());
                    _bw_dispatch_orphans(conn_str);
                }
                    break;
                case bg_worker_command::AddFile:
                {
                    add_file_command *afc = static_cast<add_file_command *>(cmd.Data());
                    _bw_add_file(conn_str, *bgCryptor, afc->ID,
                                 afc->FilePath.isEmpty() ? afc->FileContents : afc->FilePath,
                                 !afc->FilePath.isEmpty());
                }
                    break;
                case bg_worker_command::ExportFile:
                {
                    export_file_command *efc = static_cast<export_file_command *>(cmd.Data());
                    _bw_exp_file(conn_str, *bgCryptor, efc->ID, efc->FilePath);
                }
                    break;
                case bg_worker_command::DeleteFile:
                {
                    delete_file_command *afc = static_cast<delete_file_command *>(cmd.Data());
                    _bw_del_file(conn_str, afc->ID);
                }
                    break;
                case bg_worker_command::ExportToPS:
                {
                    export_to_ps_command *e2ps = static_cast<export_to_ps_command *>(cmd.Data());
                    _bw_export_to_gps(conn_str, *bgCryptor, e2ps->FilePath, e2ps->Creds);
                }
                    break;
                case bg_worker_command::ImportFromPS:
                {
                    import_from_ps_command *ifps = static_cast<import_from_ps_command *>(cmd.Data());
                    _bw_import_from_gps(conn_str, *bgCryptor, ifps->FilePath, ifps->Creds);
                }
                    break;
                case bg_worker_command::ExportToXML:
                {
                    export_to_xml_command *e2x = static_cast<export_to_xml_command *>(cmd.Data());
                    _bw_export_to_xml(conn_str, *bgCryptor, e2x->FilePath);
                }
                    break;
                case bg_worker_command::ImportFromXML:
                {
                    import_from_xml_command *ifx = static_cast<import_from_xml_command *>(cmd.Data());
                    _bw_import_from_xml(conn_str, *bgCryptor, ifx->FilePath);
                }
                    break;
                case bg_worker_command::CheckAndRepair:
                {
                    _bw_check_and_repair(conn_str, *bgCryptor);
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
            catch(const GUtil::Exception<> &ex){
                _convert_to_readonly_exception_and_notify(ex);
            }
            catch(...) {}
            lkr.lock();
        }
    }
    QSqlDatabase::removeDatabase(conn_str);
    d->thread_idle = true;
}

void PasswordDatabase::_convert_to_readonly_exception_and_notify(const GUtil::Exception<> &ex_rx)
{
    Exception<> *ex;
    if(NULL == dynamic_cast<CancelledOperationException<false>const*>(&ex_rx))
    {
        Exception<true> const *extended_ex = dynamic_cast<Exception<true> const *>(&ex_rx);
        if(NULL == extended_ex){
            // Exceptions on the background thread put us into read only mode
            ex = new ReadOnlyException<false>(ex_rx);
        }
        else{
            ex = new ReadOnlyException<true>(*extended_ex);
        }
    }
    else{
        ex = (Exception<>*)ex_rx.Clone();
    }
    emit NotifyExceptionOnBackgroundThread(shared_ptr<exception>((exception*)ex));
}

void PasswordDatabase::_bw_add_file(const QString &conn_str,
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

        // First upload and encrypt the file
        QByteArray encrypted_data;
        {
            QByteArrayOutput o(encrypted_data);
            SmartPointer<IInput> data_in;
            QFile f;

            // The data can be either a file path or the contents itself,
            //  depending on the value of the flag
            if(by_path){
                f.setFileName(data);
                data_in = new QFileIO(f);
                __open_file_or_die(f, QFile::ReadOnly);
                plaintext_length = f.size();
            }
            else{
                data_in = new QByteArrayInput(data);
                plaintext_length = data.length();
            }

            m_progressMin = 0, m_progressMax = 75;
            m_curTaskString = QString("Download and encrypt file");
            d->thread_cancellable = true;
            cryptor.EncryptData(&o, data_in, NULL, NULL, DEFAULT_CHUNK_SIZE,
                                [&](int p){ return _progress_callback(p); });
        }

        _bw_fail_if_cancelled();
        m_curTaskString = QString("Adding file");
        emit NotifyProgressUpdated(m_progressMax, true, m_curTaskString);

        _bw_fail_if_cancelled();

        // Insert a new file
        q.prepare("INSERT INTO File (Length, Data, ID) VALUES (?,?,?)");
        q.addBindValue(plaintext_length);
        q.addBindValue(encrypted_data);
        q.addBindValue((QByteArray)id);
        DatabaseUtils::ExecuteQuery(q);

        // One last chance before we commit
        emit NotifyProgressUpdated(m_progressMax + 10, true, m_curTaskString);
        _bw_fail_if_cancelled();

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

void PasswordDatabase::_bw_exp_file(const QString &conn_str,
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
    _bw_fail_if_cancelled();
    emit NotifyProgressUpdated(25, true, m_curTaskString);

    const QByteArray encrypted_data(q.record().value(0).toByteArray());
    {
        QByteArrayInput i(encrypted_data);
        QFile f(filepath);
        QFileIO fio(f);
        __open_file_or_die(f, QFile::ReadWrite|QFile::Truncate);

        _bw_fail_if_cancelled();
        emit NotifyProgressUpdated(35, true, m_curTaskString);

        m_progressMin = 35, m_progressMax = 100;
        d->thread_cancellable = true;
        cryptor.DecryptData(&fio, &i, NULL, DEFAULT_CHUNK_SIZE,
                            [&](int p){ return _progress_callback(p); });
    }
}

void PasswordDatabase::_bw_del_file(const QString &conn_str, const FileId &id)
{
    __delete_file_by_id(conn_str, id);
}

static void __append_children_to_xml(QDomDocument &xdoc, QDomNode &n,
                                     QSqlQuery &q,
                                     GUtil::CryptoPP::Cryptor &cryptor,
                                     const EntryId &eid)
{
    // Find my children and add them to xml
    QList<Entry> child_list = __find_entries_by_parent_id(q, cryptor, eid);
    foreach(const Entry &e, child_list){
        QDomNode new_node = XmlConverter::AppendToXmlNode(e, n, xdoc, true);
        __append_children_to_xml(xdoc, new_node, q, cryptor, e.GetId());
    }
}

void PasswordDatabase::_bw_export_to_gps(const QString &conn_str,
                                         GUtil::CryptoPP::Cryptor &my_cryptor,
                                         const QString &ps_filepath,
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
        _bw_fail_if_cancelled();

        // Open the target file
        GPSFile_Export gps_file(ps_filepath.toUtf8(), creds, FileId::Size);
        emit NotifyProgressUpdated(progress_counter+=5, true, m_curTaskString);
        _bw_fail_if_cancelled();

        // Compress the entry data and write it as the main payload
        {
            const QByteArray xml_compressed(qCompress(xdoc.toByteArray(-1), 9));
            gps_file.AppendPayload((byte const *)xml_compressed.constData(),
                                   xml_compressed.length());
        }
        emit NotifyProgressUpdated(progress_counter+=10, true, m_curTaskString);
        _bw_fail_if_cancelled();

        // Now let's export all the files as attachments
        int file_cnt = 0;
        const int remaining_progress = 100 - progress_counter;
        const QHash<FileId, FileInfo_t> files = QueryFileSummary();
        for(const FileId &fid : files.keys())
        {
            // Select a file from the database
            q.prepare("SELECT Data FROM File WHERE ID=?");
            q.addBindValue((QByteArray)fid);
            DatabaseUtils::ExecuteQuery(q);
            _bw_fail_if_cancelled();

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
            _bw_fail_if_cancelled();

            // Write it to the GPS, with the file ID in the metadata
            gps_file.AppendPayload((byte const *)pt.ConstData(), pt.Length(),
                                   (byte const *)fid.ConstData(), fid.Size);

            ++file_cnt;
            emit NotifyProgressUpdated(
                        progress_counter + (remaining_progress*((float)file_cnt/files.size())),
                        true,
                        m_curTaskString);
            _bw_fail_if_cancelled();
        }
    }
    catch(...){
        db.rollback();
        throw;
    }
    __commit_transaction(db);    // Nothing should have changed, is a rollback better here?
}

void PasswordDatabase::_bw_import_from_gps(const QString &conn_str,
                                           GUtil::CryptoPP::Cryptor &,
                                           const QString &ps_filepath,
                                           const Credentials &creds)
{
    int progress_counter = 0;
    m_curTaskString = QString(tr("Importing from Portable Safe: %1"))
                            .arg(QFileInfo(ps_filepath).fileName());
    emit NotifyProgressUpdated(progress_counter, true, m_curTaskString);

    // Always notify that the task is complete, even if it's an error
    finally([&]{ emit NotifyProgressUpdated(100, false, m_curTaskString); });

    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());
    QSqlQuery q(db);

    GPSFile_Import gps_import(ps_filepath.toUtf8(), creds, true);
    if(!gps_import.NextPayload())
        throw Exception<>("GPS file is empty");

    // Get the main payload, which has all the entries
    QByteArray ba;
    ba.resize(gps_import.CurrentPayloadSize());
    gps_import.GetCurrentPayload((byte *)ba.data());

    // Decompress the xml data
    ba = qUncompress(ba);

    QDomDocument xdoc;
    xdoc.setContent(ba);

    // Add a root node, under which to put the imported data
    Entry tmp_root;
    tmp_root.SetName(tr("Newly imported entries"));
    tmp_root.SetDescription(QString(tr("Imported from GPS file: %1")).arg(ps_filepath));
    AddEntry(tmp_root);
}

static void __write_entry_to_xml_writer(QXmlStreamWriter &sw, const Entry &e,
                                        QSet<FileId> &file_references,
                                        const QHash<EntryId, int> &entry_mapping,
                                        const QHash<FileId, int> &file_mapping)
{
    sw.writeStartElement("e");
    sw.writeAttribute("name", e.GetName());
    if(!e.GetDescription().isEmpty())
        sw.writeAttribute("desc", e.GetDescription());
    sw.writeAttribute("date", e.GetModifyDate().toString());
    if(e.IsFavorite())
        sw.writeAttribute("fav", QVariant(e.GetFavoriteIndex()).toString());
    if(!e.GetFileId().IsNull()){
        sw.writeAttribute("f_id", QVariant(file_mapping[e.GetFileId()]).toString());
        sw.writeAttribute("f_name", e.GetFileName());
        if(file_mapping.contains(e.GetFileId()))
            file_references.insert(e.GetFileId());
    }
    sw.writeAttribute("id", QVariant(entry_mapping[e.GetId()]).toString());
    if(!e.GetParentId().IsNull())
        sw.writeAttribute("pid", QVariant(entry_mapping[e.GetParentId()]).toString());
    sw.writeAttribute("row", QVariant(e.GetRow()).toString());

    if(0 < e.Values().length()){
        // Write the secret data key-value pairs
        for(const SecretValue &v : e.Values()){
            sw.writeStartElement("s");
            sw.writeAttribute("key", v.GetName());
            sw.writeAttribute("val", v.GetValue());
            if(!v.GetNotes().isEmpty())
                sw.writeAttribute("note", v.GetNotes());
            if(v.GetIsHidden())
                sw.writeAttribute("hide", "1");
            sw.writeEndElement();
        }
    }

    sw.writeEndElement();
}

void PasswordDatabase::_bw_export_to_xml(const QString &conn_str, GUtil::CryptoPP::Cryptor &my_cryptor, const QString &filepath)
{
    G_D;
    int progress_counter = 0;
    m_curTaskString = QString(tr("Exporting to XML: %1"))
                            .arg(QFileInfo(filepath).fileName());
    emit NotifyProgressUpdated(progress_counter, true, m_curTaskString);

    // Always notify that the task is complete, even if it's an error
    finally([&]{ emit NotifyProgressUpdated(100, false, m_curTaskString); });

    try{
        emit NotifyProgressUpdated(progress_counter+=5, true, m_curTaskString);

        QFile f(filepath);
        if(!f.open(QFile::ReadWrite | QFile::Truncate))
            throw Exception<>(QString(tr("Cannot open \"%1\"\n%2"))
                              .arg(QFileInfo(filepath).absoluteFilePath())
                              .arg(f.errorString()).toUtf8());

        QXmlStreamWriter sw(&f);
        sw.setAutoFormatting(true);

        // Start the XML document
        sw.writeStartDocument();
        sw.writeStartElement("grypto_data");
        sw.writeAttribute("version", GRYPTO_XML_VERSION);

        QHash<FileId, int> file_mapping;
        QHash<EntryId, int> entry_mapping;
        QSet<FileId> referenced_files;
        QList<entry_cache> entries;
        unordered_map<EntryId, parent_cache> parent_index_cpy;

        // Define a helper function for recursively adding entries
        int tmpid = 0;
        function<void(const EntryId &)> add_children_to_index;
        add_children_to_index = [&](const EntryId &pid)
        {
            if(!pid.IsNull())
                entry_mapping.insert(pid, tmpid++);

            for(const EntryId &cid : d->parent_index[pid].children){
                entries.append(d->index[cid]);
                add_children_to_index(cid);
            }
        };

        // We want to lock the index for the minimum time while we prepare our indexes
        d->index_lock.lock();

        // Add all entries recursively starting from the root
        add_children_to_index(EntryId::Null());

        tmpid = 0;
        for(auto p : d->file_index)
            file_mapping.insert(p.first, tmpid++);
        parent_index_cpy = d->parent_index;
        d->index_lock.unlock();

        // Write all entries in no particular order
        sw.writeStartElement("entries");
        for(auto ec : entries)
            __write_entry_to_xml_writer(sw, __convert_cache_to_entry(ec, my_cryptor),
                                        referenced_files, entry_mapping, file_mapping);
        sw.writeEndElement();

        emit NotifyProgressUpdated(progress_counter+=50, true, m_curTaskString);
        _bw_fail_if_cancelled();

        // Write the files
        if(0 < referenced_files.count()){
            sw.writeStartElement("files");
            for(const FileId &fid : referenced_files){
                sw.writeStartElement("f");
                sw.writeAttribute("id", QVariant(file_mapping[fid]).toString());

                // Export the file to a temp file, then read in the temp file and add it to xml
                QTemporaryFile tf;
                if(!tf.open())
                    throw Exception<>(QString(tr("Cannot open temp file: %1")).arg(tf.errorString()).toUtf8());

                _bw_exp_file(conn_str, my_cryptor, fid, tf.fileName().toUtf8());
                QByteArray file_data;
                bool compressed = false;
                {
                    QFile f(tf.fileName());
                    f.open(QFile::ReadOnly);
                    file_data = f.readAll();

                    // Try compressing it to see if it gets smaller
                    QByteArray file_data_comp = qCompress(file_data, 9);
                    if(file_data_comp.length() < file_data.length()){
                        file_data = file_data_comp;
                        compressed = true;
                    }
                }
                tf.remove();

                if(compressed)
                    sw.writeAttribute("comp", "1");

                sw.writeCharacters(file_data.toBase64());
                sw.writeEndElement();
            }
            sw.writeEndElement();
        }

        sw.writeEndElement();
        sw.writeEndDocument();
    }
    catch(...){
        throw;
    }
}

static void __parse_xml_entries(QXmlStreamReader &sr,
                                QHash<int, Entry> &entries,
                                QHash<int, QList<int>> &hierarchy,
                                QHash<int, file_cache> &file_mapping)
{
    int depth = 0;
    int cur_id = -1;
    Entry tmp_entry;
    while(0 <= depth){
        switch(sr.readNext()){
        case QXmlStreamReader::StartElement:
            depth++;
            if(sr.name() == "e"){
                tmp_entry.SetName(sr.attributes().value("name").toString());
                if(sr.attributes().hasAttribute("desc"))
                    tmp_entry.SetDescription(sr.attributes().value("desc").toString());
                tmp_entry.SetModifyDate(QDateTime::fromString(sr.attributes().value("date").toString()));
                if(sr.attributes().hasAttribute("fav"))
                    tmp_entry.SetFavoriteIndex(sr.attributes().value("fav").toInt());
                if(sr.attributes().hasAttribute("f_id")){
                    tmp_entry.SetFileName(sr.attributes().value("f_name").toString());

                    int fid_local = sr.attributes().value("f_id").toInt();
                    if(file_mapping.contains(fid_local)){
                        tmp_entry.SetFileId(file_mapping[fid_local].id);
                    }
                    else{
                        FileId fid(true);
                        file_mapping.insert(fid_local, fid);
                        tmp_entry.SetFileId(fid);
                    }
                }
                cur_id = sr.attributes().value("id").toInt();
                tmp_entry.SetRow(sr.attributes().value("row").toInt());

                int pid = -1;
                if(sr.attributes().hasAttribute("pid"))
                    pid = sr.attributes().value("pid").toInt();
                hierarchy[pid].append(cur_id);
            }
            else if(sr.name() == "s"){
                SecretValue sv;
                sv.SetName(sr.attributes().value("key").toString());
                sv.SetValue(sr.attributes().value("val").toString());
                if(sr.attributes().hasAttribute("note"))
                    sv.SetNotes(sr.attributes().value("note").toString());
                if(sr.attributes().hasAttribute("hide"))
                    sv.SetIsHidden(0 != sr.attributes().value("hide").toInt());
                tmp_entry.Values().append(sv);
            }
            break;
        case QXmlStreamReader::EndElement:
            depth--;
            if(0 == depth){
                entries.insert(cur_id, tmp_entry);

                tmp_entry = Entry();
                cur_id = -1;
            }
            break;
        default:
            break;
        }
    }
}

static void __parse_xml_files(QXmlStreamReader &sr, QHash<int, QString> &files)
{
    int depth = 0;
    int cur_id = -1;
    bool compressed = false;
    QString cur_path;
    while(0 <= depth){
        switch(sr.readNext()){
        case QXmlStreamReader::StartElement:
            depth++;
            if(sr.name() == "f"){
                cur_id = sr.attributes().value("id").toInt();
                if(sr.attributes().hasAttribute("comp"))
                    compressed = (0 != sr.attributes().value("comp").toInt());
            }
            break;
        case QXmlStreamReader::Characters:
            if(cur_id != -1)
            {
                QTemporaryFile tf;
                tf.setAutoRemove(false);
                if(!tf.open())
                    throw Exception<>("Cannot open temporary file");

                cur_path = tf.fileName();
                QByteArray data = QByteArray::fromBase64(sr.text().toLatin1());
                if(compressed)
                    data = qUncompress(data);
                tf.write(data);
            }
            break;
        case QXmlStreamReader::EndElement:
            depth--;
            if(0 == depth){
                files.insert(cur_id, cur_path);
                cur_id = -1;
                compressed = false;
                cur_path.clear();
            }
            break;
        default:
            break;
        }
    }
}

// Adds the children of the given parent to the database recursively
static void __add_children_from_xml(
        QHash<int, Entry> &entries,
        QHash<int, entry_cache> &entry_caches,
        const QHash<int, QList<int>> &hierarchy,
        const EntryId &parent_id,
        int local_parent_id,
        GUtil::CryptoPP::Cryptor &cryptor,
        QSqlQuery &q)
{
    const QList<int> &child_ids = hierarchy[local_parent_id];
    for(int i = 0; i < child_ids.length(); ++i){
        Entry &e = entries[child_ids[i]];
        e.SetId(EntryId::NewId());
        e.SetParentId(parent_id);
        e.SetRow(i);
        if(e.IsFavorite()){
            // Imported favorites lose their ordering
            e.SetFavoriteIndex(0);
        }

        auto iter = entry_caches.insert(child_ids[i], __convert_entry_to_cache(e, cryptor));
        __insert_entry(*iter, q);
        __add_children_from_xml(entries, entry_caches,
                                hierarchy,
                                e.GetId(), child_ids[i],
                                cryptor, q);
    }
}

static void __add_files_from_xml(const QHash<int, QString> &files,
                                 QHash<int, file_cache> &file_mapping,
                                 GUtil::CryptoPP::Cryptor &cryptor,
                                 QSqlQuery &q)
{
    for(int fid_local : files.keys()){
        QByteArray crypttext;
        {
            // Encrypt the file and move it into memory
            QFile f(files[fid_local]);
            QFileIO fio(f);
            QByteArrayOutput bao(crypttext);
            __open_file_or_die(f, QFile::ReadOnly);
            cryptor.EncryptData(&bao, &fio);
        }

        int pt_len = crypttext.length() - cryptor.GetCrypttextSizeDiff();
        file_mapping[fid_local].length = pt_len;

        q.prepare("INSERT INTO File (Length,Data,ID) VALUES (?,?,?)");
        q.addBindValue(pt_len);
        q.addBindValue(crypttext);
        q.addBindValue((QByteArray)file_mapping[fid_local].id);
        DatabaseUtils::ExecuteQuery(q);
    }
}

void PasswordDatabase::_bw_import_from_xml(const QString &conn_str,
                                           GUtil::CryptoPP::Cryptor &my_cryptor,
                                           const QString &filepath)
{
    int progress_counter = 0;
    const QString file_name = QFileInfo(filepath).fileName();
    m_curTaskString = QString(tr("Importing from XML: %1")).arg(file_name);
    emit NotifyProgressUpdated(progress_counter, true, m_curTaskString);

    QFile f(filepath);
    if(!f.open(QFile::ReadOnly))
        throw Exception<>(QString(tr("Cannot open \"%1\"\n%2"))
                          .arg(QFileInfo(filepath).absoluteFilePath())
                          .arg(f.errorString()).toUtf8());

    QHash<int, Entry> entries;
    QHash<int, entry_cache> entry_caches;
    QHash<int, QList<int>> hierarchy;
    QHash<int, QString> files;
    QHash<int, file_cache> file_mapping;
    bool xml_recognized = false;

    auto throw_xml_format_error = []{
        throw Exception<>("Unknown XML format");
    };

    QXmlStreamReader sr(&f);
    while(!sr.atEnd()){
        switch(sr.readNext()){
        case QXmlStreamReader::StartElement:
            if(sr.name() == "grypto_data"){
                if(sr.attributes().at(0).value() == GRYPTO_XML_VERSION)
                    xml_recognized = true;
                else{
                    emit NotifyProgressUpdated(100, true, m_curTaskString);
                    throw_xml_format_error();
                }
            }
            else if(sr.name() == "entries"){
                if(!xml_recognized)
                    throw_xml_format_error();
                __parse_xml_entries(sr, entries, hierarchy, file_mapping);
            }
            else if(sr.name() == "files"){
                if(!xml_recognized)
                    throw_xml_format_error();
                __parse_xml_files(sr, files);
            }
            break;
        default:
            break;
        }
    }

    // The hierarchy index child lists are unsorted, so
    //  sort them by the entrys' rows
    for(int pid : hierarchy.keys()){
        QList<int> &child_list = hierarchy[pid];
        sort(child_list.begin(), child_list.end(),
          [&](int lhs, int rhs) -> bool{
            return entries[lhs].GetRow() < entries[rhs].GetRow();
        });

        // The parent id's could not be resolved earlier, due to the arbitrary
        //  ordering of the entries in the XML, so set the parent id's now
        for(int cid : child_list)
            entries[cid].SetParentId(entries[pid].GetId());
    }


    // Cleanup code should always execute, even in case of exception
    finally([&]{
        // Make sure the temp files are cleaned up (no plaintext data left in /tmp)
        for(const QString &f : files.values())
            QFile::remove(f);

        emit NotifyProgressUpdated(100, false, m_curTaskString);
    });

    if(sr.hasError())
        throw Exception<>(QString(tr("XML has errors: %1").arg(sr.errorString())).toUtf8());

    // Now update the database
    QSqlDatabase db(QSqlDatabase::database(conn_str));
    GASSERT(db.isValid());

    db.transaction();
    try
    {
        QSqlQuery q(db);

        // Add a root node, under which to put the imported data
        Entry tmp_root;
        tmp_root.SetId(EntryId::NewId());
        tmp_root.SetName(tr("Newly imported entries"));
        tmp_root.SetDescription(QString(tr("Imported from XML document: %1")).arg(file_name));
        tmp_root.SetModifyDate(QDateTime::currentDateTime());
        tmp_root.SetRow(__count_entries_by_parent_id(q, EntryId::Null()));

        auto iter = entry_caches.insert(-1, __convert_entry_to_cache(tmp_root, my_cryptor));
        __insert_entry(*iter, q);

        __add_children_from_xml(entries, entry_caches, hierarchy, tmp_root.GetId(), -1, my_cryptor, q);
        __add_files_from_xml(files, file_mapping, my_cryptor, q);
    }
    catch(...)
    {
        db.rollback();
        throw;
    }
    db.commit();

    // Now update the index
    G_D;
    unique_lock<mutex> lkr(d->index_lock);
    for(const entry_cache &ec : entry_caches.values()){
        GASSERT(d->index.find(ec.id) == d->index.end());
        d->index.emplace(ec.id, ec);

        // We should have cleared the ordering of imported favorites
        GASSERT(0 >= ec.favoriteindex);
        if(0 <= ec.favoriteindex)
            d->favorite_index.append(ec.id);
    }

    // Populate the parent index now that all keys are inserted
    function<void(int)> populate_parent_index;
    populate_parent_index = [&](int local_pid){
        for(int cid : hierarchy[local_pid])
        {
            d->parent_index[entry_caches[local_pid].id]
                    .children.append(entry_caches[cid].id);

            if(hierarchy.contains(cid))
                populate_parent_index(cid);
        }
    };
    d->parent_index[EntryId::Null()].children.append(entry_caches[-1].id);
    populate_parent_index(-1);

    // Finally update the file index
    for(const file_cache &fc : file_mapping.values())
        d->file_index.emplace(fc.id, fc);

    d->wc_index.notify_all();
}

void PasswordDatabase::_bw_check_and_repair(const QString &conn_str, GUtil::CryptoPP::Cryptor&)
{
    G_D;
    QString final_report = "Check and Repair: ";
    finally([&]{
        emit NotifyProgressUpdated(100, false, final_report);
    });

    // First clean up orphaned entries and files
    emit NotifyProgressUpdated(5, false, "Removing orphaned entries and files...");
    _bw_dispatch_orphans(conn_str);

    // Then validate the hierarchy
    emit NotifyProgressUpdated(20, false, "Validating entry hierarchy...");
    QList<EntryId> reorder_parents;
    QHash<EntryId, QList<EntryId>> parent_index;
    QSqlQuery q("SELECT ID,ParentID,Row FROM Entry ORDER BY ParentID,Row ASC",
                QSqlDatabase::database(conn_str));
    unique_ptr<EntryId> prev;
    int expected_row = 0;
    while(q.next()){
        EntryId pid = q.value("ParentID").toByteArray();
        if(!prev)
            prev.reset(new EntryId(pid));
        else if(*prev != pid){
            expected_row = 0;
            *prev = pid;
        }

        if((reorder_parents.isEmpty() || pid != reorder_parents.back())
                && expected_row != q.value("Row").toInt())
            reorder_parents.append(pid);

        parent_index[pid].append(q.value("ID").toByteArray());
        expected_row++;
    }

    if(0 < reorder_parents.length()){
        emit NotifyProgressUpdated(40, false,
                                   QString("%1 parents with children out of order..."
                                           "Correcting this now...")
                                   .arg(reorder_parents.length()));

        QSqlDatabase db(QSqlDatabase::database(conn_str));
        db.transaction();
        bool success = true;
        try{
            for(const EntryId &pid : reorder_parents){
                for(int i = 0; i < parent_index[pid].length(); i++){
                    q.prepare("UPDATE Entry SET Row=? WHERE ID=?");
                    q.addBindValue(i);
                    q.addBindValue((QByteArray)parent_index[pid][i]);
                    DatabaseUtils::ExecuteQuery(q);
                }
            }
        }
        catch(...){
            db.rollback();
            success = false;
        }

        if(success)
            db.commit();

        // Update the index to make sure it matches what's in the database
        lock_guard<mutex> lkr(d->index_lock);
        for(const EntryId &pid : reorder_parents){
            for(int i = 0; i < parent_index[pid].length(); i++)
                d->index[parent_index[pid][i]].row = i;

            sort(d->parent_index[pid].children.begin(),
                 d->parent_index[pid].children.end(),
              [&](const EntryId &lhs, const EntryId &rhs){
                 return d->index[lhs].row < d->index[rhs].row;
            });
        }
        d->wc_index.notify_all();

        final_report.append(QString("Child ordering fixed for %1 parent entries")
                            .arg(reorder_parents.length()));
    }
    else{
        final_report.append("No issues found");
    }

    // Lastly reclaim unused space
    emit NotifyProgressUpdated(90, false, "Reclaiming unused file space...");
    q.exec("VACUUM");
}


bool PasswordDatabase::_should_operation_cancel()
{
    G_D;
    d->thread_lock.lock();
    bool ret = d->cancel_thread;
    d->thread_lock.unlock();
    return ret;
}

bool PasswordDatabase::_progress_callback(int prg)
{
    // prg is between 0 and 100, so scale it to m_progressMax-m_progressMin
    G_D;
    emit NotifyProgressUpdated(m_progressMin + ((float)prg*(m_progressMax-m_progressMin))/100,
                               d->thread_cancellable,
                               m_curTaskString);
    return _should_operation_cancel();
}

void PasswordDatabase::_bw_fail_if_cancelled()
{
    if(_should_operation_cancel())
        throw CancelledOperationException<>();
}

GUtil::CryptoPP::Cryptor const &PasswordDatabase::Cryptor() const
{
    FailIfNotOpen();
    G_D;
    return *d->cryptor;
}

void PasswordDatabase::WaitForThreadIdle() const
{
    FailIfNotOpen();
    G_D;
    unique_lock<mutex> lkr(d->thread_lock);
    d->wc_thread.wait(lkr, [&]{
        return d->thread_idle;
    });
}


END_NAMESPACE_GRYPTO;

