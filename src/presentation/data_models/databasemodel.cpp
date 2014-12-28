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

#include "databasemodel.h"
#include "grypto_passworddatabase.h"
#include "gutil_globals.h"
#include <QFont>
#include <QSet>
#include <QMimeData>
#include <QStringList>
#include <unordered_map>
//#include "../gutil/src/test/modeltest.h"
USING_NAMESPACE_GUTIL;
using namespace std;

NAMESPACE_GRYPTO;

struct EntryContainer
{
    Entry entry;
    int child_count;
    QList<EntryContainer *> children;

    EntryContainer() :child_count(-1) {}
    EntryContainer(const Entry &e)
        :entry(e),
          child_count(-1)
    {}
    ~EntryContainer();
};

class AddEntryCommand : public IUndoableAction {
    Entry m_entry;
    DatabaseModel *m_model;
public:
    AddEntryCommand(const Entry &e, DatabaseModel *m) :m_entry(e), m_model(m) {
        m_entry.SetId(EntryId::NewId());
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

class MoveEntryCommand : public IUndoableAction {
    DatabaseModel *m_model;
    const QPersistentModelIndex m_sourcePind;
    const QPersistentModelIndex m_targetPind;
    int m_sourceRowFirst, m_sourceRowLast;
    int m_targetRow;
public:
    MoveEntryCommand(const QModelIndex &source_parentid, int first, int last,
                     const QModelIndex &target_parentid, int target_row,
                     DatabaseModel *m)
        :m_model(m), m_sourcePind(source_parentid), m_targetPind(target_parentid),
          m_sourceRowFirst(first), m_sourceRowLast(last), m_targetRow(target_row)
    {}
    void Do(){
        m_model->_mov_entries(m_sourcePind, m_sourceRowFirst, m_sourceRowLast,
                              m_targetPind, m_targetRow);
    }
    void Undo(){
        m_model->_mov_entries(m_targetPind, m_targetRow,
                              m_targetRow + m_sourceRowLast - m_sourceRowFirst,
                              m_sourcePind, m_sourceRowFirst);
    }
    String Text() const{ return String::Format("Move: "); }
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


DatabaseModel::DatabaseModel(const char *f, const char *p, const char *k, QObject *parent)
    :QAbstractItemModel(parent),
      m_db(new PasswordDatabase(f, p, k))
{
    connect(m_db.data(), SIGNAL(NotifyFavoritesUpdated()),
            this, SIGNAL(NotifyFavoritesUpdated()));
    connect(m_db.data(), SIGNAL(NotifyExceptionOnBackgroundThread(const std::shared_ptr<GUtil::Exception<>> &)),
            this, SLOT(_handle_database_worker_exception(const std::shared_ptr<GUtil::Exception<>> &)));
    connect(m_db.data(), SIGNAL(NotifyProgressUpdated(int, QString)),
            this, SIGNAL(NotifyProgressUpdated(int, QString)));

    // We need to manually fetch the root
    fetchMore(QModelIndex());

    //new ModelTest(this);
}

DatabaseModel::~DatabaseModel()
{}

GUtil::CryptoPP::Cryptor const &DatabaseModel::Cryptor() const
{
    return m_db->Cryptor();
}

QModelIndex DatabaseModel::FindIndexById(const EntryId &id) const
{
    QModelIndex ret;
    if(m_index.contains(id))
    {
        EntryContainer *c = m_index[id];
        ret = createIndex(c->entry.GetRow(), 0, (void *)c);
    }
    return ret;
}

Entry DatabaseModel::FindEntryById(const EntryId &id) const
{
    return m_db->FindEntry(id);
}

vector<Entry> DatabaseModel::FindFavorites() const
{
    return m_db->FindFavoriteEntries();
}

Entry const *DatabaseModel::GetEntryFromIndex(const QModelIndex &ind) const
{
    EntryContainer *ec = _get_container_from_index(ind);
    return ec ? &ec->entry : NULL;
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
            else if(col == 1)
                ret = ec->entry.GetModifyDate();
            break;
        case Qt::ToolTipRole:
            ret = ec->entry.GetDescription();
            break;
        case Qt::FontRole:
            ret = [=]{ QFont f; f.setBold(col == 0 && ec->child_count > 0); return f; } ();
            break;
        case Qt::TextAlignmentRole:
            if(col == 0)
                ret = Qt::AlignLeft;
            else if(col == 1)
                ret = Qt::AlignRight;
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
        for(Entry const &e : m_db->FindEntriesByParentId(id))
        {
            int cnt = m_db->CountEntriesByParentId(e.GetId());
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
    m_db->AddEntry(e, generate_id);

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
        EntryContainer *ec = new EntryContainer(e);
        ec->child_count = 0;
        m_index.insert(e.GetId(), ec);
        children.insert(e.GetRow(), ec);
        if(par.isValid())
            _get_container_from_index(par)->child_count += 1;
    }
    endInsertRows();
}

void DatabaseModel::_del_entry(const EntryId &id)
{
    m_db->DeleteEntry(id);

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

        // Remove from the index
        m_index.remove(id);
    }
    endRemoveRows();
}

void DatabaseModel::_edt_entry(const Entry &e)
{
    m_db->UpdateEntry(e);

    QModelIndex ind = FindIndexById(e.GetId());
    if(ind.isValid())
    {
        Entry &my_entry( _get_container_from_index(ind)->entry );
        my_entry = e;
        emit dataChanged(ind, index(ind.row(), columnCount() - 1, ind.parent()));
    }
}

void DatabaseModel::_mov_entries(const QModelIndex &pind, int r_first, int r_last,
                                 const QModelIndex &targ_pind, int &r_dest)
{
    int move_cnt = r_last - r_first + 1;
    GASSERT(0 <= r_first && r_first < rowCount(pind));
    GASSERT(0 <= r_last && r_last < rowCount(pind));
    GASSERT(move_cnt > 0);
    GASSERT(rowCount(targ_pind) >= r_dest);

    EntryContainer *ec = _get_container_from_index(pind);
    EntryContainer *ec_targ = _get_container_from_index(targ_pind);

    const EntryId pid = pind.isValid() ? ec->entry.GetId() : EntryId::Null();
    const EntryId pid_targ = targ_pind.isValid() ? ec_targ->entry.GetId() : EntryId::Null();
    QList<EntryContainer *> &cl_targ = _get_child_list(targ_pind);

    // Dropping onto an item appends to its children
    if(r_dest < 0)
        r_dest = cl_targ.length();

    // If dropping onto the same parent, then adjust the rows
    if(pid == pid_targ)
    {
        if(r_dest > r_last)
            r_dest -= move_cnt;

        // Don't proceed if the source is the same as the destination
        if(r_dest >= r_first && r_dest <= r_last)
            return;
    }

    // Move the entries in the database
    m_db->MoveEntries(pid, r_first, r_last,
                      pid_targ, r_dest);

    // Now move them in the model
    beginMoveRows(pind, r_first, r_last, targ_pind, r_dest);
    QList<EntryContainer *> &cl = _get_child_list(pind);
    Vector<EntryContainer *> to_move((EntryContainer *)NULL, move_cnt, true);
    for(int i = r_last; i >= r_first; --i){
        to_move[i - r_first] = cl[i];
        cl.removeAt(i);
    }
    for(uint i = 0; i < to_move.Length(); ++i)
        cl_targ.insert(r_dest + i, to_move[i]);

    // Adjust rows at both source and dest
    for(int i = r_first; i < cl.length(); ++i)
        cl[i]->entry.SetRow(i);
    for(int i = r_dest; i < cl_targ.length(); ++i){
        EntryContainer *cur = cl_targ[i];
        cur->entry.SetRow(i);
        cur->entry.SetParentId(pid_targ);
    }
    if(ec) ec->child_count -= move_cnt;
    if(ec_targ) ec_targ->child_count += move_cnt;
    endMoveRows();
}

QByteArray const &DatabaseModel::FilePath() const
{
    return m_db->FilePath();
}

bool DatabaseModel::CheckPassword(const char *password, const char *keyfile) const
{
    return m_db->CheckPassword(password, keyfile);
}

void DatabaseModel::UpdateFile(const FileId &id, const char *filepath)
{
    m_db->AddUpdateFile(id, filepath);
}

void DatabaseModel::DeleteFile(const FileId &id)
{
    m_db->DeleteFile(id);
}

bool DatabaseModel::FileExists(const FileId &id)
{
    return m_db->FileExists(id);
}

void DatabaseModel::ExportFile(const FileId &id, const char *export_file_path)
{
    m_db->ExportFile(id, export_file_path);
}

/** Exports the entire database in the portable safe format. */
void DatabaseModel::ExportToPortableSafe(const char *export_filename,
                                         const char *password,
                                         const char *keyfile)
{
    m_db->ExportToPortableSafe(export_filename, password, keyfile);
}

vector<pair<FileId, quint32> > DatabaseModel::GetFileSummary()
{
    return m_db->GetFileSummary();
}

void DatabaseModel::_append_referenced_files(const QModelIndex &ind, QSet<QByteArray> &s)
{
    if(canFetchMore(ind))
        fetchMore(ind);

    if(ind.isValid()){
        EntryContainer *ec = _get_container_from_index(ind);
        if(!ec->entry.GetFileId().IsNull())
            s.insert((QByteArray)ec->entry.GetFileId());
    }

    for(int i = 0; i < rowCount(ind); ++i)
        _append_referenced_files(index(i, 0, ind), s);
}

QSet<QByteArray> DatabaseModel::GetReferencedFiles()
{
    QSet<QByteArray> ret;
    _append_referenced_files(QModelIndex(), ret);
    return ret;
}

void DatabaseModel::CancelAllBackgroundOperations()
{
    m_db->CancelFileTasks();
}

void DatabaseModel::_handle_database_worker_exception(const shared_ptr<Exception<>> &ex)
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

    // Gather data for all indexes.
    Vector<EntryId> l(indexes.length());
    for(const QModelIndex &ind : indexes){
        if(ind.column() != 0) continue;     // Ignore duplicate rows
        EntryContainer *ec = _get_container_from_index(ind);
        if(!ec) continue;
        l.PushBack(ec->entry.GetId());
    }

    if(l.Length() > 0){
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

bool DatabaseModel::dropMimeData(const QMimeData *data,
                                 Qt::DropAction action,
                                 int row, int,
                                 const QModelIndex &parent)
{
    if(action != Qt::MoveAction)
        return false;

    QByteArray sd = data->data(MIMETYPE_MOVE_ENTRY);
    if(!sd.isEmpty()){
        QList<QByteArray> entries = sd.split(':');

        if(entries.length() > 1)
            throw NotImplementedException<>("Moving more than one entry not implemented");
        else if(entries.isEmpty())
            return false;

        QModelIndex eind = FindIndexById(QByteArray::fromBase64(entries[0]));
        if(!eind.isValid())
            throw Exception<>("Source Entry Id not found in model");

        m_undostack.Do(new MoveEntryCommand(eind.parent(), eind.row(), eind.row(),
                                            parent, row, this));
    }
    return false;
}

Qt::DropActions DatabaseModel::supportedDropActions() const
{
    return Qt::MoveAction;
}


END_NAMESPACE_GRYPTO;
