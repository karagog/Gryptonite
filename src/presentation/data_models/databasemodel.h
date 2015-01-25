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

#ifndef GRYPTO_DATABASEMODEL_H
#define GRYPTO_DATABASEMODEL_H

#include <grypto_passworddatabase.h>
#include <gutil/undostack.h>
#include <QAbstractItemModel>

namespace GUtil{ namespace CryptoPP{
class Cryptor;
}}

namespace Grypt{

struct EntryContainer;


/** A lazy-loading tree model of database entries. */
class DatabaseModel :
        public QAbstractItemModel
{
    Q_OBJECT
public:

    /** Constructs a database model and opens the database with an exclusive lock.
        \param ask_for_lock_override A function to ask the user if they would like
                to override the lock file (if needed).
    */
    explicit DatabaseModel(const char *file_path,
                           std::function<bool(const PasswordDatabase::ProcessInfo &)> ask_for_lock_override
                                = [](const PasswordDatabase::ProcessInfo &){ return false; },
                           QObject *parent = 0);
    ~DatabaseModel();

    /** Opens the database with the given credentials. You can only open the database once, and
        it will close automatically when the object is deleted.
    */
    void Open(const Credentials &);

    /** Returns true if the database has been opened. */
    bool IsOpen() const{ return m_db.IsOpen(); }

    /** The path to the database on disk. */
    const char *FilePath() const;

    /** Returns true if this is the correct password for the database. */
    bool CheckCredentials(const Credentials &) const;

    /** Returns a reference to the cryptor used by the database object. */
    GUtil::CryptoPP::Cryptor const &Cryptor() const;
    
    /** Changes the time format shown in views */
    void SetTimeFormat24Hours(bool = true);


    /** Returns the model index of the entry, or an invalid one if it can't be found.
     *  If the entry has not been loaded it will return invalid. Use FetchAllEntries()
     *  and then use this function if you want to definitely find it.
    */
    QModelIndex FindIndexById(const EntryId &) const;

    /** Returns the entry given by id, or throws an exception if it wasn't found. */
    Entry FindEntryById(const EntryId &) const;

    /** Returns the sorted list of favorites. */
    QList<Entry> FindFavorites() const;

    /** Returns the sorted list of favorites entry ids. */
    QList<EntryId> FindFavoriteIds() const;

    /** Returns a reference to the entry held in the model, or a null pointer
     *  if the index is invalid.
    */
    Entry const *GetEntryFromIndex(const QModelIndex &) const;

    /** Blocks the current thread until the background thread goes idle. Use
     *  this to guarantee that your operation has completed before proceeding.
    */
    void WaitForBackgroundThreadIdle();

    /** \name Undoable actions
     *  You can call Undo() and Redo() to undo and redo these actions
     *  \{
    */
    void AddEntry(Entry &);
    void UpdateEntry(const Entry &);
    void RemoveEntry(const Entry &);
    void MoveEntries(const QModelIndex &src_parent, int src_first, int src_last,
                     const QModelIndex &dest_parent, int dest_row);

    void AddEntryToFavorites(const EntryId &);
    void RemoveEntryFromFavorites(const EntryId &);
    void SetFavoriteEntries(const QList<EntryId> &);
    /** \} */

    inline bool CanUndo() const{ return m_undostack.CanUndo(); }
    inline bool CanRedo() const{ return m_undostack.CanRedo(); }
    inline GUtil::String UndoText() const{ return m_undostack.GetUndoText(); }
    inline GUtil::String RedoText() const{ return m_undostack.GetRedoText(); }
    inline void ClearUndoStack(){ m_undostack.Clear(); }


    /** Adds or updates the file. */
    void UpdateFile(const FileId &, const char *filepath);

    /** Removes the file from the database. */
    void DeleteFile(const FileId &);

    /** Returns true if the file id is present in the database. */
    bool FileExists(const FileId &);

    /** Returns the complete list of files in the database, along with their sizes. */
    QHash<FileId, PasswordDatabase::FileInfo_t> GetFileSummary();

    /** Returns a set of all file ids which are not referenced by any entry id. */
    QHash<FileId, PasswordDatabase::FileInfo_t> GetOrphanedFiles();

    /** Returns the complete list of file ids that are referenced by entries. */
    QSet<FileId> GetReferencedFiles();

    /** Decrypts and exports the file. */
    void ExportFile(const FileId &, const char *export_file_path);

    /** Exports the entire database in the portable safe format. */
    void ExportToPortableSafe(const char *export_filename,
                              const Credentials &);

    /** Imports data from the portable safe file. */
    void ImportFromPortableSafe(const char *export_filename,
                                const Credentials &);

    /** Imports the contents from the other database model.
     *  All Entry and File ID's will be new. Both databases
     *  must already be open.
    */
    void ImportFromDatabase(const DatabaseModel &);

    /** Cleans up orphan entries and files. */
    void DeleteOrphans();

    /** Loads all entries from the database. */
    void FetchAllEntries();

    /** \name QAbstractItemModel interface
     *  \{
    */
    enum CustomDataRoles{
        EntryIdRole = Qt::UserRole,
        FileIdRole  = Qt::UserRole + 1
    };

    virtual QModelIndex index(int, int, const QModelIndex &) const;
    virtual QModelIndex parent(const QModelIndex &) const;
    virtual int rowCount(const QModelIndex & = QModelIndex()) const;
    virtual int columnCount(const QModelIndex & = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &, int) const;
    virtual Qt::ItemFlags flags(const QModelIndex &) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QModelIndexList &) const;
    virtual bool dropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex &);
    virtual Qt::DropActions supportedDropActions() const;

    virtual bool hasChildren(const QModelIndex &) const;
    virtual bool canFetchMore(const QModelIndex &) const;
    virtual void fetchMore(const QModelIndex & = QModelIndex());
    /** \} */


public slots:

    /** Instructs the model to cancel any outstanding background operations. */
    void CancelAllBackgroundOperations();

    /** Undoes the last action, or does nothing if there was no action. */
    void Undo(){ m_undostack.Undo(); }

    /** Redoes the last Undo, or does nothing if there was nothing to Redo. */
    void Redo(){ m_undostack.Redo(); }


signals:

    void NotifyFavoritesUpdated();
    void NotifyProgressUpdated(int, bool, const QString &);
    void NotifyUndoStackChanged();


private slots:

    void _handle_database_worker_exception(const std::shared_ptr<GUtil::Exception<>> &);


private:

    PasswordDatabase m_db;
    QList<EntryContainer *> m_root;
    QHash<EntryId, EntryContainer *> m_index;
    GUtil::UndoStack m_undostack;
    bool m_timeFormat;

    EntryContainer *_get_container_from_index(const QModelIndex &) const;
    QList<EntryContainer *> const &_get_child_list(const QModelIndex &) const;
    QList<EntryContainer *> &_get_child_list(const QModelIndex &);

    void _append_referenced_files(const QModelIndex &, QSet<QByteArray> &);

    void _add_entry(Entry &, bool);
    void _del_entry(const EntryId &);
    void _edt_entry(Entry &);
    void _mov_entries(const QModelIndex &pind, int r_first, int r_last,
                      const QModelIndex &target_pind, int &r_dest);
    void _set_favs(const QList<EntryId> &);
    void _add_fav(const EntryId &);
    void _del_fav(const EntryId &);

    void _emit_row_changed(const QModelIndex &);

    // These are private helper classes only used in the cpp file
    friend class AddEntryCommand;
    friend class EditEntryCommand;
    friend class DeleteEntryCommand;
    friend class MoveEntryCommand;
    friend class SetFavoriteEntriesCommand;
    friend class AddFavoriteEntryCommand;
    friend class RemoveFavoriteEntryCommand;
};


}

#endif // GRYPTO_DATABASEMODEL_H
