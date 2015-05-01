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

#ifndef GRYPTO_PASSWORDDATABASE_H
#define GRYPTO_PASSWORDDATABASE_H

#include <grypto/common.h>
#include <gutil/exception.h>
#include <QString>
#include <QObject>
#include <memory>
#include <functional>
#include <map>
#include <set>

class QSqlRecord;
class QSqlQuery;
class QLockFile;

namespace Grypt{
class Entry;


/** Manages access to the password file.
    It throws GUtil::Exception to indicate errors.
*/
class PasswordDatabase :
        public QObject
{
    Q_OBJECT
    void *d;
    const QString m_filepath;
    std::unique_ptr<QLockFile> m_lockfile;
public:

    /** Holds information about a process */
    struct ProcessInfo{
        qint64  ProcessId = -1;
        QString HostName;
        QString AppName;
    };

    struct FileInfo_t
    {
        // The file's size in bytes
        uint Size;

        FileInfo_t(uint size = 0) :Size(size) {}
    };

    /** Creates a new PasswordDatabase object. Before you use it, you must call Open() with the
     *  proper credentials.
     *
     *  \param ask_for_lock_override A function to ask the user if they want
     *      to override the lock on the database. This is only called if the
     *      database was locked when you tried to open it, and if it returns false
     *      an exception will be thrown, thus aborting construction of the database object.
     *      The default always returns false.
     *      The argument contains information about the process that has locked the database.
    */
    PasswordDatabase(const QString &file_path,
                     std::function<bool(const ProcessInfo &)> ask_for_lock_override =
                            [](const ProcessInfo &){ return false; },
                     QObject * = NULL);

    ~PasswordDatabase();

    /** Opens or creates the database with the given credentials.
     *  Throws an Exception if the password is wrong, or if something else goes wrong.
     *
     *  This class' behavior is undefined until you call this function without it throwing an exception.
     *
     *  \note There is no corresponding Close() function, as the file automatically closes in the
     *      destructor. The reason Open() is separate from the constructor is to support delayed
     *      opening. Thus you can lock the file before asking the user for credentials, so you don't
     *      waste their time typing in their password if the database is already locked by another process.
     *      This also allows you to try your credentials multiple times without closing and re-locking
     *      the database.
    */
    void Open(const Credentials &creds);

    /** This version of Open() uses a cryptor, which does not require you to know the password/keyfile,
        only the actual key used to encrypt/decrypt. Otherwise this function behaves identically to the other Open().
    */
    void Open(const GUtil::CryptoPP::Cryptor &);

    /** Returns true if the database was already opened. */
    bool IsOpen() const;

    /** Throws an exception if the database is not opened. */
    void FailIfNotOpen() const{ if(!IsOpen()) throw GUtil::Exception<>("Database not open"); }

    /** Returns the filepath. */
    QString const &FilePath() const{ return m_filepath; }

    /** Returns true if the password and keyfile are correct. */
    bool CheckCredentials(const Credentials &) const;

    /** Returns a reference to the cryptor for you to use (but not change). */
    GUtil::CryptoPP::Cryptor const &Cryptor() const;

    /** This function does a sanity check on the database to see if it is
     *  the right format for this software version.
    */
    static void ValidateDatabase(const char *filepath);

    /** Iteratively counts all entries in the database. This can be a slow operation,
     *  and it's used for calculating total progress.
    */
    int CountAllEntries() const;

    /** This function allows you to synchronize with the background entry thread.
     *  Call this to wait until the background thread is done working.
    */
    void WaitForThreadIdle() const;


    /** \name Entry Access
        \{
    */

    /** Adds the entry to the database and shifts the surrounding entries in the hierarchy. */
    void AddEntry(Entry &, bool generate_id = true);

    /** Returns the entry given by id or throws an exception if it can't be found. */
    Entry FindEntry(const EntryId &) const;

    /** Returns the number of children for the parent id. */
    int CountEntriesByParentId(const EntryId &) const;

    /** Returns a list of entries for the given parent id sorted by row number. */
    QList<Entry> FindEntriesByParentId(const EntryId &) const;

    /** Returns a sorted list of the user's favorite entries. */
    QList<Entry> FindFavoriteEntries() const;

    /** Returns a sorted list of the user's favorite entry ids. */
    QList<EntryId> FindFavoriteIds() const;

    /** Sets the given entries as favorites, in the order they are given. */
    void SetFavoriteEntries(const QList<EntryId> &);

    /** Adds the entry to favorites. */
    void AddFavoriteEntry(const EntryId &);

    /** Removes the entry from favorites. */
    void RemoveFavoriteEntry(const EntryId &);

    /** Instructs a background worker to refresh favorites. */
    void RefreshFavorites();

    /** Updates the given entry's data. Note this function is not used to move entries
     *  around the hierarchy.
    */
    void UpdateEntry(Entry &);

    /** Moves a block of entries.

        \param parentId_src The source parent ID
        \param row_first The first row of the source parent to be moved
        \param row_last The last row of the source parent to be moved
        \param parentId_dest The destination parent ID
        \param row_dest The row of the destination parent to which to move the rows
    */
    void MoveEntries(const EntryId &parentId_src, quint32 row_first, quint32 row_last,
                     const EntryId &parentId_dest, quint32 row_dest);

    /** Removes the entry given by id.
     *  This does not delete any files referenced by the entry.
    */
    void DeleteEntry(const EntryId &id);

    /** \} */


    /** \name File Access
        \{
    */

    /** Returns true if the file exists in the database. */
    bool FileExists(const FileId &) const;

    /** Decrypts and exports the file to the given path. */
    void ExportFile(const FileId &, const char *export_path) const;

    /** Adds a new file to the database, or updates an existing one.
     *  This works on a background thread.
    */
    void AddFile(const FileId &, const char *filename);

    /** This version adds a file by its contents, rather than file path. */
    void AddFile(const FileId &, const QByteArray &contents);

    /** Removes the file from the database. */
    void DeleteFile(const FileId &);

    /** Decrypts the file and returns its contents. This is a slow operation
     *  that happens on the main thread. Use carefully.
    */
    QByteArray GetFile(const FileId &) const;

    /** Returns the complete list of file ids and associated file sizes.
     *  Note that some files may not be referenced by an entry id.
    */
    QHash<FileId, FileInfo_t> QueryFileSummary() const;

    /** Returns the set of file ids which are referenced by the entry ids. */
    QSet<FileId> GetReferencedFileIds() const;

    /** \} */

    /** Cancels all background tasks. */
    void CancelFileTasks();

    /** Exports the entire database in the portable safe format. */
    void ExportToPortableSafe(const char *export_filename,
                              const Credentials &) const;

    /** Imports data from the portable safe file. */
    void ImportFromPortableSafe(const char *export_filename,
                                const Credentials &);

    /** Imports data from the other database. New ID's will be given to
     *  every entry and file, so there is no possibility of collision.
    */
    void ImportFromDatabase(const PasswordDatabase &);


    /** Call this function to iterate through all entries, deleting
     *  those that don't have parents. Afterwards it will iterate through
     *  files and delete those that are not referenced by an entry.
    */
    void DeleteOrphans();


signals:

    /** Notifies when the favorites have changed, either the order or the set of them. */
    void NotifyFavoritesUpdated();

    /** Notifies that progress of a background task has updated.
     *  The task string provides a description of the task being done.
    */
    void NotifyProgressUpdated(int progress, bool cancellable, const QString &task_string = QString());

    /** Notifies that the exception was received on the background thread. */
    void NotifyExceptionOnBackgroundThread(const std::shared_ptr<std::exception> &);


private:

    // Main thread methods
    void _init_cryptor(const Credentials &, const byte *salt, GUINT32 salt_len);
    void _init_cryptor(const GUtil::CryptoPP::Cryptor &);
    void _open(std::function<void(byte const *)> init_cryptor);

    // Worker thread bodies
    void _background_worker(GUtil::CryptoPP::Cryptor *);

    // Utility functions
    void _convert_to_readonly_exception_and_notify(const GUtil::Exception<> &);
    bool _progress_callback(int);
    bool _should_operation_cancel();

    // Background worker methods
    void _bw_add_entry(const QString &, const Entry &);
    void _bw_update_entry(const QString &, const Entry &);
    void _bw_delete_entry(const QString &, const EntryId &);
    void _bw_move_entry(const QString &, const EntryId &, quint32, quint32, const EntryId &, quint32);
    void _bw_cache_entries_by_parentid(const QString &, const EntryId &);
    void _bw_cache_all_entries(const QString &);
    void _bw_refresh_favorites(const QString &);
    void _bw_set_favorites(const QString &, const QList<EntryId> &sorted_favorites);
    void _bw_add_favorite(const QString &, const EntryId &);
    void _bw_remove_favorite(const QString &, const EntryId &);
    void _bw_dispatch_orphans(const QString &);

    void _bw_add_file(const QString &, GUtil::CryptoPP::Cryptor&, const FileId &, const QByteArray &, bool);
    void _bw_exp_file(const QString &, GUtil::CryptoPP::Cryptor&, const FileId &, const char *);
    void _bw_del_file(const QString &, const FileId &);
    void _bw_export_to_gps(const QString &, GUtil::CryptoPP::Cryptor&, const char *ps_filepath, const Credentials &);
    void _bw_import_from_gps(const QString &, GUtil::CryptoPP::Cryptor&, const char *ps_filepath, const Credentials &);
    void _bw_fail_if_cancelled();
    int m_progressMin, m_progressMax;
    QString m_curTaskString;

};


}

#endif // GRYPTO_PASSWORDDATABASE_H
