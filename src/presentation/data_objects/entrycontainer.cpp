/*Copyright 2010 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "fileentrycontainer.h"
#include "Entry.h"
USING_GRYPTO_NAMESPACE2(Common, DataObjects);

FileEntryContainer::FileEntryContainer(Entry *parent)
    :_p_ParentEntry(parent)
{}

FileEntryContainer::FileEntryContainer(const QList<Entry *> &l)
    :GUtil::DataObjects::Collection<Entry*>(l),
      _p_ParentEntry(0)
{}

FileEntryContainer::~FileEntryContainer()
{
    Clear();
}

Entry *FileEntryContainer::CreateChildEntry()
{
    return new Entry(this, GetParentEntry());
}

void FileEntryContainer::on_remove(int i)
{
    delete At(i);
}

QList<Entry *> FileEntryContainer::GetAllEntries() const
{
    QList<Entry *> ret;
    foreach(Entry *e, (QList<Entry *>)*this)
    {
        ret.append(e);
        ret.append(e->GetAllEntries());
    }
    return ret;
}

Entry *FileEntryContainer::FindById(const QUuid &id) const
{
    Entry *ret(0);

    for(int i = 0; !ret && i < Count(); i++)
    {
        if(At(i)->GetId() == id)
            ret = At(i);
        else
            ret = At(i)->FindById(id);
    }

    return ret;
}

QList<int> FileEntryContainer::GetFileIds(bool valid) const
{
    QList<int> ret;

    for(int i = 0; i < Count(); i++)
    {
        for(int j = 0; j < At(i)->GetDataTable().RowCount(); j++)
        {
            if(At(i)->GetDataTable()[j].GetIsBinary())
            {
                if(!valid || !At(i)->GetDataTable()[j].GetIsBrokenBinaryReference())
                    ret.append(At(i)->GetDataTable()[j].GetData().toInt());
            }
        }

        ret.append(At(i)->GetFileIds());
    }

    return ret;
}

QList<Entry *> FileEntryContainer::GetFavorites(bool sort) const
{
    QList<Entry *> res;

    for(int i = 0; i < Count(); i++)
    {
        res.append(At(i));
        res.append(At(i)->GetFavorites(false));
    }

    if(sort)
        qSort(res.begin(), res.end(), Entry::_cmp_favorites_func);
    return res;
}
