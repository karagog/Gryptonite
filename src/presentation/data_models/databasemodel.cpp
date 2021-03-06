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

#include "databasemodel.h"
#include <grypto/passworddatabase.h>
#include <grypto/entry.h>
#include <QFont>
#include <QSet>
#include <QMimeData>
#include <QStringList>
#include <QIcon>
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GUTIL1(CryptoPP);
using namespace std;

#define GRYPTONITE_DATE_FORMAT_STRING "d MMM yyyy h:mm"

NAMESPACE_GRYPTO;

struct EntryContainer
{
    Entry entry;
    int child_count = -1;
    QList<EntryContainer *> children;
    bool deleted = false;

    EntryContainer(const Entry &e)
        :entry(e)
    {}
    ~EntryContainer();
};

class AddEntryCommand : public IUndoableAction {
    Entry m_entry;
    DatabaseModel *m_model;
public:
    AddEntryCommand(Entry &e, DatabaseModel *m) :m_entry(e), m_model(m) {
        m_entry.SetId(EntryId::NewId());
        e.SetId(m_entry.GetId());

        if(-1 == e.GetRow()){
            QModelIndex par_ind;
            if(e.GetParentId().IsNull() ||
                    (par_ind = m_model->FindIndexById(e.GetParentId())).isValid()){
                e.SetRow(m_model->rowCount(par_ind));
                m_entry.SetRow(e.GetRow());
            }
        }
    }
    void Do(){ m_model->_add_entry(m_entry, false); }
    void Undo(){ m_model->_del_entry(m_entry.GetId()); }
    String Text() const{
        return String::Format("Create: %s", m_entry.GetName().toUtf8().constData());
    }
};

class DeleteEntryCommand : public IUndoableAction {
    Entry m_entry;
    DatabaseModel *m_model;
public:
    DeleteEntryCommand(const Entry &e, DatabaseModel *m) :m_entry(e), m_model(m) {}
    void Do(){ m_model->_del_entry(m_entry.GetId()); }
    void Undo(){ m_model->_add_entry(m_entry, false); }
    String Text() const{
        return String::Format("Delete: %s", m_entry.GetName().toUtf8().constData());
    }
};

class EditEntryCommand : public IUndoableAction {
    Entry m_before;
    Entry m_after;
    DatabaseModel *m_model;
public:
    EditEntryCommand(const Entry &before, const Entry &after, DatabaseModel *m)
        :m_before(before), m_after(after), m_model(m) {}
    void Do(){ m_model->_edt_entry(m_after); }
    void Undo(){ m_model->_edt_entry(m_before); }
    String Text() const{
        return String::Format("Edit: %s", m_after.GetName().toUtf8().constData());
    }
};

class AddFavoriteEntryCommand : public IUndoableAction {
    DatabaseModel *m_model;
    EntryId m_id;
public:
    AddFavoriteEntryCommand(const EntryId &id, DatabaseModel *m)
        :m_model(m), m_id(id) {}
    void Do(){ m_model->_add_fav(m_id); }
    void Undo(){ m_model->_del_fav(m_id); }
    String Text() const{ return "Add to Favorites"; }
};

class RemoveFavoriteEntryCommand : public IUndoableAction {
    DatabaseModel *m_model;
    EntryId m_id;
public:
    RemoveFavoriteEntryCommand(const EntryId &id, DatabaseModel *m)
        :m_model(m), m_id(id) {}
    void Do(){ m_model->_del_fav(m_id); }
    void Undo(){ m_model->_add_fav(m_id); }
    String Text() const{ return "Remove from Favorites"; }
};

class SetFavoriteEntriesCommand : public IUndoableAction {
    DatabaseModel *m_model;
    QList<EntryId> m_favs;
    QList<EntryId> m_origFavs;
public:
    SetFavoriteEntriesCommand(const QList<EntryId> &favs,
                              DatabaseModel *m)
        :m_model(m), m_favs(favs), m_origFavs(m->FindFavoriteIds()) {}
    void Do(){ m_model->_set_favs(m_favs); }
    void Undo(){ m_model->_set_favs(m_origFavs); }
    String Text() const{ return "Update Favorites"; }
};

class MoveEntryCommand : public IUndoableAction {
    DatabaseModel *m_model;
    EntryId m_sourcePid;
    EntryId m_targetPid;
    int m_sourceRowFirst, m_sourceRowLast;
    int m_targetRow;
    String m_text;
public:
    MoveEntryCommand(const EntryId &source_parentid, int first, int last,
                     const EntryId &target_parentid, int target_row,
                     DatabaseModel *m)
        :m_model(m), m_sourcePid(source_parentid), m_targetPid(target_parentid),
          m_sourceRowFirst(first), m_sourceRowLast(last), m_targetRow(target_row),
          m_text(String::Format("Move: %s", m_model->index(m_sourceRowFirst, 0, m->FindIndexById(m_sourcePid)).data().toString().toUtf8().constData()))
    {}
    void Do(){
        m_model->_mov_entries(m_sourcePid, m_sourceRowFirst, m_sourceRowLast,
                              m_targetPid, m_targetRow);
    }
    void Undo(){
        int cnt = m_sourceRowLast - m_sourceRowFirst + 1;
        int source = m_targetRow;
        int dest = m_sourceRowFirst;
        if(m_targetPid == m_sourcePid){
            if(m_targetRow > m_sourceRowFirst)
                source -= cnt;
            else
                dest += cnt;
        }
        m_model->_mov_entries(m_targetPid, source, source + cnt - 1,
                              m_sourcePid, dest);
    }
    String Text() const{ return m_text; }
};


EntryContainer *DatabaseModel::_get_container_from_index(const QModelIndex &ind) const
{
    EntryContainer *ret;
    if(ind.isValid())
        ret = reinterpret_cast<EntryContainer *>(ind.internalPointer());
    else
        ret = 0;
    return ret;
}

QList<EntryContainer *> const &DatabaseModel::_get_child_list(const QModelIndex &par) const
{
    EntryContainer *ent = _get_container_from_index(par);
    return ent == NULL ? m_root : ent->children;
}

QList<EntryContainer *> &DatabaseModel::_get_child_list(const QModelIndex &par)
{
    EntryContainer *ent = _get_container_from_index(par);
    return ent == NULL ? m_root : ent->children;
}

static void __cleanup_entry_list(QList<EntryContainer *> &l)
{
    foreach(EntryContainer *ec, l)
        delete ec;

    l.clear();
}


EntryContainer::~EntryContainer()
{
    __cleanup_entry_list(this->children);
}


DatabaseModel::DatabaseModel(const char *f,
                             function<bool(const PasswordDatabase::ProcessInfo &)> ask_for_lock_override,
                             QObject *parent)
    :QAbstractItemModel(parent),
      m_db(f, ask_for_lock_override),
      m_undostack([&]{ emit NotifyUndoStackChanged(); }),
      m_timeFormat(true)
{
    connect(&m_db, SIGNAL(NotifyFavoritesUpdated()),
            this, SIGNAL(NotifyFavoritesUpdated()),
            Qt::QueuedConnection);
    connect(&m_db, SIGNAL(NotifyExceptionOnBackgroundThread(const std::shared_ptr<std::exception> &)),
            this, SLOT(_handle_database_worker_exception(const std::shared_ptr<std::exception> &)));
    connect(&m_db, SIGNAL(NotifyProgressUpdated(int, bool, QString)),
            this, SIGNAL(NotifyProgressUpdated(int, bool, QString)));
}

DatabaseModel::~DatabaseModel()
{
    // Clean up the index objects
    __cleanup_entry_list(m_root);
}

void DatabaseModel::Open(const Credentials &creds)
{
    m_db.Open(creds);

    // Fetch the root index
    fetchMore(QModelIndex());
}

void DatabaseModel::Open(const GUtil::CryptoPP::Cryptor &cryptor)
{
    m_db.Open(cryptor);

    // Fetch the root index
    fetchMore(QModelIndex());
}

void DatabaseModel::SaveAs(const QString &filename, const Credentials &creds)
{
    if(IsOpen()){
        ClearUndoStack();
        m_db.SaveAs(filename, creds);
        _reset_model();
    }
}

void DatabaseModel::CheckAndRepairDatabase()
{
    ClearUndoStack();
    connect(&m_db, SIGNAL(NotifyThreadIdle()), this, SLOT(_thread_finished_reset_model()));
    m_db.CheckAndRepairDatabase();
}

GUtil::CryptoPP::Cryptor const &DatabaseModel::Cryptor() const
{
    return m_db.Cryptor();
}

QModelIndex DatabaseModel::FindIndexById(const EntryId &id) const
{
    QModelIndex ret;
    if(m_index.contains(id) && !m_index[id]->deleted)
    {
        EntryContainer *c = m_index[id];
        ret = createIndex(c->entry.GetRow(), 0, (void *)c);
    }
    return ret;
}

Entry DatabaseModel::FindEntryById(const EntryId &id) const
{
    return m_db.FindEntry(id);
}

QList<Entry> DatabaseModel::FindFavorites() const
{
    return m_db.FindFavoriteEntries();
}

QList<EntryId> DatabaseModel::FindFavoriteIds() const
{
    return m_db.FindFavoriteIds();
}

Entry const *DatabaseModel::GetEntryFromIndex(const QModelIndex &ind) const
{
    EntryContainer *ec = _get_container_from_index(ind);
    return ec  && !ec->deleted ? &ec->entry : NULL;
}

bool DatabaseModel::HasAncestor(const EntryId &child, const EntryId &ancestor) const
{
    return m_db.HasAncestor(child, ancestor);
}

QModelIndex DatabaseModel::index(int row, int col, const QModelIndex &parent) const
{
    QModelIndex ret;
    if(0 <= col && col < columnCount(parent) && 0 <= row)
    {
        QList<EntryContainer *> const &children = _get_child_list(parent);
        if(row < children.count())
            ret = createIndex(row, col, (void *)children[row]);
    }
    return ret;
}

QModelIndex DatabaseModel::parent(const QModelIndex &ind) const
{
    QModelIndex ret;
    EntryContainer *cont = _get_container_from_index(ind);
    if(cont != NULL)
    {
        const EntryId pid = cont->entry.GetParentId();
        if(m_index.contains(pid))
        {
            EntryContainer *parent_cont = m_index.value(pid);
            ret = createIndex(parent_cont->entry.GetRow(), 0, (void *)parent_cont);
        }
    }
    return ret;
}

int DatabaseModel::rowCount(const QModelIndex &par) const
{
    return _get_child_list(par).count();
}

int DatabaseModel::columnCount(const QModelIndex &) const
{
    return 2;
}

Qt::ItemFlags DatabaseModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsEnabled |
            Qt::ItemIsSelectable |
            Qt::ItemIsDragEnabled |
            Qt::ItemIsDropEnabled;
}

void DatabaseModel::SetTimeFormat24Hours(bool tf)
{
    m_timeFormat = tf;
}

QVariant DatabaseModel::data(const QModelIndex &ind, int role) const
{
    QVariant ret;
    int col = ind.column();

    EntryContainer *ec = _get_container_from_index(ind);
    if(ec && 0 <= col && col < columnCount(ind))
    {
        switch(role)
        {
        case Qt::DisplayRole:
            if(col == 0)
                ret = ec->entry.GetName();
            else if(col == 1){
                ret = ec->entry.GetModifyDate().toString(
                    m_timeFormat ?
                        GRYPTONITE_DATE_FORMAT_STRING :
                        GRYPTONITE_DATE_FORMAT_STRING " ap");
            }
            break;
        case Qt::ToolTipRole:
            ret = ec->entry.GetDescription();
            break;
        case Qt::FontRole:
            ret = [=]{
                QFont f;
                f.setBold(col == 0 &&
                          (ec->child_count > 0 ||
                           ec->entry.IsFavorite()));
                return f;
            } ();
            break;
        case Qt::TextAlignmentRole:
            if(col == 0)
                ret = Qt::AlignLeft;
            else if(col == 1)
                ret = Qt::AlignRight;
            break;
        case Qt::DecorationRole:
            if(col == 0 && ec->entry.IsFavorite()){
                ret = QIcon(":/grypto/icons/star.png");
            }
            break;
        case EntryIdRole:
            ret = QVariant::fromValue(ec->entry.GetId());
            break;
        case FileIdRole:
            if(!ec->entry.GetFileId().IsNull())
                ret = QVariant::fromValue(ec->entry.GetFileId());
            break;
        default:
            break;
        }
    }
    return ret;
}

QVariant DatabaseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant ret;
    if(0 <= section && section < columnCount())
    {
        if(Qt::Horizontal == orientation)
        {
            switch(role)
            {
            case Qt::DisplayRole:
                if(0 == section)
                    ret = tr("Name");
                else if(1 == section)
                    ret = tr("Last Modified");
                break;
            case Qt::TextAlignmentRole:
                ret = Qt::AlignHCenter;
                break;
            default:
                break;
            }
        }
        else if(Qt::Vertical == orientation)
        {

        }
    }
    return ret;
}

bool DatabaseModel::hasChildren(const QModelIndex &par) const
{
    bool ret;
    if(par.isValid())
        ret = 0 < _get_container_from_index(par)->child_count;
    else
        ret = true;
    return ret;
}

bool DatabaseModel::canFetchMore(const QModelIndex &par) const
{
    bool ret = false;
    if(par.isValid())
    {
        EntryContainer *ent = _get_container_from_index(par);
        if(ent->child_count != ent->children.count())
            ret = true;
    }
    return ret;
}

void DatabaseModel::fetchMore(const QModelIndex &par)
{
    EntryContainer *cont = _get_container_from_index(par);
    EntryId id;
    if(cont == NULL)
    {
        if(m_root.count() > 0)
            return;
    }
    else
    {
        if(cont->child_count == cont->children.count())
            return;
        id = cont->entry.GetId();
    }

    QList<EntryContainer *> &lst = _get_child_list(par);
    GASSERT(lst.count() == 0);

    // Fetch the data inside a try block in case we hit an exception part-way through fetching
    QList<EntryContainer *> tmplist;
    try
    {
        for(Entry const &e : m_db.FindEntriesByParentId(id))
        {
            int cnt = m_db.CountEntriesByParentId(e.GetId());
            tmplist.append(new EntryContainer(e));
            tmplist.last()->child_count = cnt;
        }
    }
    catch(...)
    {
        __cleanup_entry_list(tmplist);
        throw;
    }

    if(cont)
        cont->child_count = tmplist.count();

    if(tmplist.count() > 0)
    {
        // Only after we have all entries do we add it to the model
        beginInsertRows(par, 0, tmplist.count() - 1);
        {
            lst.append(tmplist);

            // Add all the new children to the model's index
            foreach(EntryContainer *e, tmplist)
                m_index.insert(e->entry.GetId(), e);
        }
        endInsertRows();
    }
}

static void __fetch_node_recursive(QAbstractItemModel *m, const QModelIndex &ind)
{
    for(int i = 0; i < m->rowCount(ind); ++i){
        QModelIndex child = m->index(i, 0, ind);
        m->fetchMore(child);
        __fetch_node_recursive(m, child);
    }
}

void DatabaseModel::FetchAllEntries()
{
    __fetch_node_recursive(this, QModelIndex());
}

void DatabaseModel::AddEntry(Entry &e)
{
    m_undostack.Do(new AddEntryCommand(e, this));
}

void DatabaseModel::RemoveEntry(const Entry &e)
{
    m_undostack.Do(new DeleteEntryCommand(e, this));
}

void DatabaseModel::UpdateEntry(const Entry &e)
{
    m_undostack.Do(new EditEntryCommand(FindEntryById(e.GetId()), e, this));
}

void DatabaseModel::_add_entry(Entry &e, bool generate_id)
{
    m_db.AddEntry(e, generate_id);

    // Don't have to do anything if the parent is not present
    if(!e.GetParentId().IsNull() && !m_index.contains(e.GetParentId()))
        return;

    // Don't have to do anything if the parent's children have not been loaded
    QModelIndex par = FindIndexById(e.GetParentId());
    if(canFetchMore(par))
        return;

    if((int)e.GetRow() > rowCount(par))
        e.SetRow(rowCount(par));

    beginInsertRows(par, e.GetRow(), e.GetRow());
    {
        // Update the sibling rows
        QList<EntryContainer *> &children = _get_child_list(par);
        for(int i = e.GetRow(); i < children.count(); ++i)
            children[i]->entry.SetRow(i + 1);

        // Add the new entry to the model
        EntryContainer *ec = m_index.value(e.GetId(), NULL);
        if(ec){
            // If the entry was already in the model, then undelete it
            ec->deleted = false;
        }
        else{
            ec = new EntryContainer(e);
            ec->child_count = 0;
            m_index.insert(e.GetId(), ec);
        }
        children.insert(e.GetRow(), ec);
        if(par.isValid())
            _get_container_from_index(par)->child_count += 1;
    }
    endInsertRows();
}

void DatabaseModel::_del_entry(const EntryId &id)
{
    m_db.DeleteEntry(id);

    // Don't have to do anything if the entry wasn't loaded
    if(!m_index.contains(id))
        return;

    QModelIndex ind = FindIndexById(id);
    QList<EntryContainer *> &children = _get_child_list(ind.parent());
    beginRemoveRows(ind.parent(), ind.row(), ind.row());
    {
        children.removeAt(ind.row());
        if(ind.parent().isValid())
            _get_container_from_index(ind.parent())->child_count -= 1;

        // Update the sibling rows
        for(int i = ind.row(); i < children.count(); ++i)
            children[i]->entry.SetRow(i);

        // Mark as deleted
        _get_container_from_index(ind)->deleted = true;
    }
    endRemoveRows();
}

void DatabaseModel::_edt_entry(Entry &e)
{
    m_db.UpdateEntry(e);

    QModelIndex ind = FindIndexById(e.GetId());
    if(ind.isValid())
    {
        Entry &my_entry( _get_container_from_index(ind)->entry );
        my_entry = e;
        emit dataChanged(ind, index(ind.row(), columnCount() - 1, ind.parent()));
    }
}

void DatabaseModel::_set_favs(const QList<EntryId> &favs)
{
    QList<EntryId> orig_favs = m_db.FindFavoriteIds();

    m_db.SetFavoriteEntries(favs);

    for(const EntryId &fid : orig_favs){
        m_index[fid]->entry.SetFavoriteIndex(-1);
        if(!favs.contains(fid))
            _emit_row_changed(FindIndexById(fid));
    }

    int ctr = 1;
    for(const EntryId &fid : favs){
        m_index[fid]->entry.SetFavoriteIndex(ctr++);
        if(!orig_favs.contains(fid))
            _emit_row_changed(FindIndexById(fid));
    }
}

void DatabaseModel::_add_fav(const EntryId &id)
{
    m_db.AddFavoriteEntry(id);

    m_index[id]->entry.SetFavoriteIndex(0);

    _emit_row_changed(FindIndexById(id));
}

void DatabaseModel::_del_fav(const EntryId &id)
{
    m_db.RemoveFavoriteEntry(id);

    QList<Entry> favs = m_db.FindFavoriteEntries();
    for(const Entry &f : favs)
        m_index[f.GetId()]->entry.SetFavoriteIndex(f.GetFavoriteIndex());
    m_index[id]->entry.SetFavoriteIndex(-1);

    _emit_row_changed(FindIndexById(id));
}

void DatabaseModel::_mov_entries(const EntryId &pid, int r_first, int r_last,
                                 const EntryId &targ_pid, int &r_dest)
{
    int move_cnt = r_last - r_first + 1;
    QModelIndex parent_index = FindIndexById(pid);
    QModelIndex targ_parent_index = FindIndexById(targ_pid);
    GASSERT(0 <= r_first && r_first < rowCount(parent_index));
    GASSERT(0 <= r_last && r_last < rowCount(parent_index));
    GASSERT(move_cnt > 0);
    GASSERT(rowCount(targ_parent_index) >= r_dest);

    EntryContainer *ec = _get_container_from_index(parent_index);
    EntryContainer *ec_targ = _get_container_from_index(targ_parent_index);
    QList<EntryContainer *> &cl_targ = _get_child_list(targ_parent_index);

    // Dropping onto an item appends to its children
    if(r_dest < 0)
        r_dest = cl_targ.length();

    if(pid == targ_pid)
    {
        // Don't proceed if the source is the same as the destination
        if(r_dest >= r_first && r_dest <= r_last)
            return;
    }

    beginMoveRows(parent_index, r_first, r_last, targ_parent_index, r_dest);

    // Move the entries in the database
    m_db.MoveEntries(pid, r_first, r_last,
                      targ_pid, r_dest);


    // Now move them in the model
    int r_dest_cpy = r_dest;
    if(parent_index == targ_parent_index && r_dest > r_last)
        r_dest_cpy -= move_cnt;

    QList<EntryContainer *> &cl = _get_child_list(parent_index);
    QVector<EntryContainer *> to_move;
    to_move.resize(move_cnt);

    for(int i = r_last; i >= r_first; --i){
        to_move[i - r_first] = cl[i];
        cl.removeAt(i);
    }
    for(int i = 0; i < to_move.length(); ++i)
        cl_targ.insert(r_dest_cpy + i, to_move[i]);

    // Adjust rows at both source and dest
    for(int i = r_first; i < cl.length(); ++i)
        cl[i]->entry.SetRow(i);
    for(int i = r_dest_cpy; i < cl_targ.length(); ++i){
        EntryContainer *cur = cl_targ[i];
        cur->entry.SetRow(i);
        cur->entry.SetParentId(targ_pid);
    }
    if(ec) ec->child_count -= move_cnt;
    if(ec_targ) ec_targ->child_count += move_cnt;
    endMoveRows();
}

const QString &DatabaseModel::FilePath() const
{
    return m_db.FilePath();
}

bool DatabaseModel::CheckCredentials(const Credentials &creds) const
{
    return m_db.CheckCredentials(creds);
}

Credentials::TypeEnum DatabaseModel::GetCredentialsType() const
{
    return m_db.GetCredentialsType();
}

void DatabaseModel::AddFile(const FileId &id, const char *filepath)
{
    m_db.AddFile(id, filepath);
}

void DatabaseModel::DeleteFile(const FileId &id)
{
    m_db.DeleteFile(id);
}

bool DatabaseModel::FileExists(const FileId &id)
{
    return m_db.FileExists(id);
}

void DatabaseModel::ExportFile(const FileId &id, const char *export_file_path)
{
    m_db.ExportFile(id, export_file_path);
}

void DatabaseModel::ExportToPortableSafe(const QString &export_filename,
                                         const Credentials &creds)
{
    m_db.ExportToPortableSafe(export_filename, creds);
}

void DatabaseModel::ImportFromPortableSafe(const QString &import_filename,
                                           const Credentials &creds)
{
    ClearUndoStack();
    m_db.ImportFromPortableSafe(import_filename, creds);
    connect(&m_db, SIGNAL(NotifyThreadIdle()), this, SLOT(_thread_finished_reset_model()));
}

void DatabaseModel::ExportToXml(const QString &export_filename)
{
    m_db.ExportToXml(export_filename);
}

void DatabaseModel::ImportFromXml(const QString &import_filename)
{
    ClearUndoStack();
    m_db.ImportFromXml(import_filename);
    connect(&m_db, SIGNAL(NotifyThreadIdle()), this, SLOT(_thread_finished_reset_model()));
}

void DatabaseModel::_reset_model()
{
    beginResetModel();
    __cleanup_entry_list(m_root);
    m_index.clear();
    endResetModel();

    fetchMore();
    FetchAllEntries();
}

void DatabaseModel::_thread_finished_reset_model()
{
    disconnect(&m_db, SIGNAL(NotifyThreadIdle()), this, SLOT(_thread_finished_reset_model()));
    _reset_model();
    emit NotifyReadOnlyTransactionFinished();
}

void DatabaseModel::CancelAllBackgroundOperations()
{
    m_db.CancelFileTasks();
}

void DatabaseModel::_handle_database_worker_exception(const shared_ptr<exception> &ex)
{
    // We will throw the exception for the background worker, because here we'll catch
    //  it on the main thread.
    throw ex;
}

#define MIMETYPE_MOVE_ENTRY "gryptonite/moveentry"

QStringList DatabaseModel::mimeTypes() const
{
    return QStringList(MIMETYPE_MOVE_ENTRY);
}

QMimeData *DatabaseModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *ret = NULL;

    // Gather data for all indexes
    QList<EntryId> l;
    l.reserve(indexes.length());

    for(const QModelIndex &ind : indexes){
        if(ind.column() != 0) continue;     // Ignore duplicate rows
        EntryContainer *ec = _get_container_from_index(ind);
        if(!ec) continue;
        l.append(ec->entry.GetId());
    }

    if(l.length() > 0){
        // Serialize the move data
        QByteArray data;
        for(auto iter = l.begin(), e = l.end(); iter != e; ++iter){
            if(iter != l.begin())
                data.append(':');
            data.append(iter->ToString64().ConstData());
        }
        ret = new QMimeData;
        ret->setData(MIMETYPE_MOVE_ENTRY, data);
    }
    return ret;
}

bool DatabaseModel::dropMimeData(const QMimeData *mimedata,
                                 Qt::DropAction action,
                                 int row, int,
                                 const QModelIndex &parent)
{
    if(action != Qt::MoveAction && action != Qt::CopyAction)
        return false;

    QByteArray sd = mimedata->data(MIMETYPE_MOVE_ENTRY);
    if(sd.isEmpty())
        return false;

    QList<QByteArray> entries = sd.split(':');

    if(entries.length() > 1)
        throw NotImplementedException<>("Moving or copying more than one entry not implemented");
    else if(entries.isEmpty())
        return false;

    QModelIndex eind = FindIndexById(QByteArray::fromBase64(entries[0]));
    if(!eind.isValid())
        throw Exception<>("Source Entry Id not found in model");

    if(action == Qt::MoveAction)
    {
        MoveEntries(eind.parent(), eind.row(), eind.row(), parent, row);
    }
    else if(action == Qt::CopyAction)
    {
        Entry cpy = *GetEntryFromIndex(eind);
        cpy.SetParentId(data(parent, EntryIdRole).value<EntryId>());
        cpy.SetRow(row);
        cpy.SetModifyDate(QDateTime::currentDateTime());
        AddEntry(cpy);
    }
    return true;
}

void DatabaseModel::MoveEntries(const QModelIndex &src_parent, int src_first, int src_last,
                                const QModelIndex &dest_parent, int dest_row)
{
    m_undostack.Do(new MoveEntryCommand(src_parent.data(EntryIdRole).value<EntryId>(), src_first, src_last,
                                        dest_parent.data(EntryIdRole).value<EntryId>(), dest_row, this));
}

void DatabaseModel::AddEntryToFavorites(const EntryId &id)
{
    m_undostack.Do(new AddFavoriteEntryCommand(id, this));
}

void DatabaseModel::RemoveEntryFromFavorites(const EntryId &id)
{
    m_undostack.Do(new RemoveFavoriteEntryCommand(id, this));
}

void DatabaseModel::SetFavoriteEntries(const QList<EntryId> &favs)
{
    m_undostack.Do(new SetFavoriteEntriesCommand(favs, this));
}

void DatabaseModel::_emit_row_changed(const QModelIndex &ind)
{
    emit dataChanged(index(ind.row(), 0, ind.parent()),
                     index(ind.row(), columnCount() - 1, ind.parent()));
}

Qt::DropActions DatabaseModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

void DatabaseModel::WaitForBackgroundThreadIdle()
{
    m_db.WaitForThreadIdle();
}


END_NAMESPACE_GRYPTO;
