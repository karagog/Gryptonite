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

#ifndef GRYPTO_PASSWORDDATABASE_H
#define GRYPTO_PASSWORDDATABASE_H

#include "gutil_exception.h"
#include "gutil_smartpointer.h"
#include "gutil_iprogresshandler.h"
#include "grypto_entry.h"
#include <QString>
#include <memory>

class QSqlRecord;
class QSqlQuery;

NAMESPACE_GUTIL1(CryptoPP);
class Cryptor;
END_NAMESPACE_GUTIL1;

namespace Grypt{


/** Manages access to the password file.
    It throws GUtil::Exception to indicate errors.
*/
class PasswordDatabase :
        public QObject,
        protected GUtil::IProgressHandler
{
    Q_OBJECT
    void *d;
public:

    /** Opens or creates the database with the password and/or key file.
     *  Throws an AuthenticationException if the password is wrong.
    */
    PasswordDatabase(const char *file_path,
                     const char *password,
                     const char *keyfile = NULL,
                     QObject * = NULL);

    ~PasswordDatabase();

    /** Returns the filepath. */
    QByteArray const &FilePath() const;

    /** Returns true if the password and keyfile are correct. */
    bool CheckPassword(const char *password, const char *keyfile = 0) const;

    /** Returns a reference to the cryptor for you to use (but not change). */
    GUtil::CryptoPP::Cryptor const &Cryptor() const;


    /** \name Entry Access
        \{
    */

    /** Adds the entry to the database and shifts the surrounding entries in the hierarchy. */
    void AddEntry(Entry &, bool generate_id = true);

    /** Returns the entry given by id or throws an exception if it can't be found. */
    Entry FindEntry(const EntryId &);

    /** Returns the number of children for the parent id. */
    int CountEntriesByParentId(const EntryId &);

    /** Returns a list of entries for the given parent id sorted by row number. */
    std::vector<Entry> FindEntriesByParentId(const EntryId &);

    /** Returns a sorted list of the user's favorite entries. */
    std::vector<Entry> FindFavoriteEntries();

    /** Sets the given entries as favorites, in the order they are given. */
    void SetFavoriteEntries(const GUtil::Vector<EntryId> &);

    /** Updates the given entry's data. Note this function is not used to move entries
     *  around the hierarchy.
    */
    void UpdateEntry(const Entry &);

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
    bool FileExists(const FileId &);

    /** Decrypts and exports the file to the given path. */
    void ExportFile(const FileId &, const char *export_path);

    /** Adds a new file to the database, or updates an existing one.
     *  This works on a background thread.
    */
    void AddUpdateFile(const FileId &, const char *filename);

    /** Removes the file from the database. */
    void DeleteFile(const FileId &);

    /** Returns the complete list of file ids and associated file sizes. */
    std::vector<std::pair<FileId, quint32> > GetFileSummary();

    /** \} */

    /** Cancels all background tasks. */
    void CancelFileTasks();

    /** Exports the entire database in the portable safe format. */
    void ExportToPortableSafe(const char *export_filename,
                              const char *password,
                              const char *keyfile = NULL);


public slots:

    /** Instructs a background worker to refresh favorites. */
    void RefreshFavorites();


signals:

    /** Notifies when the favorites have changed, either the order or the set of them. */
    void NotifyFavoritesUpdated();

    /** Notifies that progress of a background task has updated.
     *  The task string provides a description of the task being done.
    */
    void NotifyProgressUpdated(int progress, const QString &task_string = QString());

    /** Notifies that the exception was received on the background thread. */
    void NotifyExceptionOnBackgroundThread(const std::shared_ptr<GUtil::Exception<>> &);


protected:

    /** \name GUtil::IProgressHandler interface
     *  \{
    */
    virtual void ProgressUpdated(int);
    virtual bool ShouldOperationCancel();
    /** \}*/


private:

    // Main thread methods
    void _init_cryptor(const char *password, const char *keyfile, const byte *salt, GUINT32 salt_len);

    // Worker thread bodies
    void _file_worker(GUtil::CryptoPP::Cryptor *);
    void _entry_worker(GUtil::CryptoPP::Cryptor *);

    // File worker methods and members
    void _fw_add_file(const QString &, GUtil::CryptoPP::Cryptor&, const FileId &, const char *);
    void _fw_exp_file(const QString &, GUtil::CryptoPP::Cryptor&, const FileId &, const char *);
    void _fw_del_file(const QString &, const FileId &);
    void _fw_export_to_gps(const QString &, GUtil::CryptoPP::Cryptor&, const char *ps_filepath, const char *password, const char *keyfile);
    void _fw_fail_if_cancelled();
    int m_progressMin, m_progressMax;
    QString m_curTaskString;

    // Entry worker methods
    void _ew_add_entry(const QString &, GUtil::CryptoPP::Cryptor&, const Entry &);
    void _ew_update_entry(const QString &, GUtil::CryptoPP::Cryptor&, const Entry &);
    void _ew_delete_entry(const QString &, const EntryId &);
    void _ew_cache_entries_by_parentid(const QString &, const EntryId &);
    void _ew_refresh_favorites(const QString &);
    void _ew_set_favorites(const QString &, const GUtil::Vector<EntryId> &sorted_favorites);

    // Any threads can use these methods:
    bool _file_exists(QSqlQuery &, const FileId &);

};


}

#endif // GRYPTO_PASSWORDDATABASE_H
