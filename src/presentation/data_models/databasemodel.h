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

#include "grypto_common.h"
#include <gutil/undostack.h>
#include <gutil/cryptopp_cryptor.h>
#include <memory>
#include <QScopedPointer>
#include <QAbstractItemModel>

namespace Grypt
{

class PasswordDatabase;
class Entry;
struct EntryContainer;


/** A lazy-loading tree model of database entries. */
class DatabaseModel :
        public QAbstractItemModel
{
    Q_OBJECT
public:

    explicit DatabaseModel(const char *file_path,
                           const Credentials &,
                           QObject *parent = 0);
    ~DatabaseModel();

    QByteArray const &FilePath() const;

    /** Returns true if this is the correct password for the database. */
    bool CheckCredentials(const Credentials &) const;

    /** Returns a reference to the cryptor used by the database object. */
    GUtil::CryptoPP::Cryptor const &Cryptor() const;


    /** Returns the model index of the entry, or an invalid one if it can't be found.
     *  If the entry has not been loaded it will return invalid. Use FetchAllEntries()
     *  and then use this function if you want to definitely find it.
    */
    QModelIndex FindIndexById(const EntryId &) const;

    /** Returns the entry given by id, or throws an exception if it wasn't found. */
    Entry FindEntryById(const EntryId &) const;

    /** Returns the sorted list of favorites. */
    std::vector<Entry> FindFavorites() const;

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

    /** Returns the complete list of file ids that are referenced by entries. */
    QSet<QByteArray> GetReferencedFiles();

    /** Returns the complete list of files in the database, along with their sizes. */
    std::vector<std::pair<FileId, quint32> > GetFileSummary();

    /** Decrypts and exports the file. */
    void ExportFile(const FileId &, const char *export_file_path);

    /** Exports the entire database in the portable safe format. */
    void ExportToPortableSafe(const char *export_filename,
                              const Credentials &);

    /** Imports data from the portable safe file. */
    void ImportFromPortableSafe(const char *export_filename,
                                const Credentials &);

    /** Cleans up orphan entries and files. */
    void DeleteOrphans();

    /** Loads all entries from the database. */
    void FetchAllEntries();

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
    virtual void fetchMore(const QModelIndex &);


public slots:

    /** Instructs the model to cancel any outstanding background operations. */
    void CancelAllBackgroundOperations();

    /** Undoes the last action, or does nothing if there was no action. */
    void Undo(){ m_undostack.Undo(); }

    /** Redoes the last Undo, or does nothing if there was nothing to Redo. */
    void Redo(){ m_undostack.Redo(); }


signals:

    void NotifyFavoritesUpdated();
    void NotifyProgressUpdated(int, const QString &);


private slots:

    void _handle_database_worker_exception(const std::shared_ptr<GUtil::Exception<>> &);


private:

    QScopedPointer<PasswordDatabase> m_db;
    QList<EntryContainer *> m_root;
    QHash<EntryId, EntryContainer *> m_index;
    GUtil::UndoStack m_undostack;

    EntryContainer *_get_container_from_index(const QModelIndex &) const;
    QList<EntryContainer *> const &_get_child_list(const QModelIndex &) const;
    QList<EntryContainer *> &_get_child_list(const QModelIndex &);

    void _append_referenced_files(const QModelIndex &, QSet<QByteArray> &);

    void _add_entry(Entry &, bool);
    void _del_entry(const EntryId &);
    void _edt_entry(Entry &);
    void _mov_entries(const QModelIndex &pind, int r_first, int r_last,
                      const QModelIndex &target_pind, int &r_dest);

    // These are private helper classes only used in the cpp file
    friend class AddEntryCommand;
    friend class EditEntryCommand;
    friend class DeleteEntryCommand;
    friend class MoveEntryCommand;
};


}

#endif // GRYPTO_DATABASEMODEL_H
