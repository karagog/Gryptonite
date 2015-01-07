/*Copyright 2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "legacyutils.h"
#include "encryption.h"
#include "password_file.h"
#include <grypto_entry.h>
#include <cryptopp/cryptlib.h>
#include <QFile>
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GUTIL1(CryptoPP);
using namespace std;

namespace Grypt{ namespace Legacy{


LegacyUtils::FileVersionEnum LegacyUtils::GetFileVersion(const char *file_path)
{
    // First check if it's the latest version
    try{
        PasswordDatabase::ValidateDatabase(file_path);
        return CurrentVersion;
    }
    catch(...){}

    // If we made it this far, then it's not the latest version, so let's
    //  figure out what legacy version it is
    QByteArray version_string;
    QFile f(file_path);
    f.open(QFile::ReadOnly);

    char c = 0;
    f.getChar(&c);
    while(c != '|'){
        if(f.atEnd()){
            // In version 1 we didn't use a version identifier
            return Version1;
        }
        version_string.append(c);
        f.getChar(&c);
    }

    bool ok = false;
    int version_int = version_string.toInt(&ok);
    if(!ok || version_int >= 4 || version_int <= 1)
        throw Exception<>("Unrecognized file type");

    return (FileVersionEnum)version_int;
}


static int __read_four_chars_into_int(QFile *f)
{
    QByteArray ba = f->read(4);
    if(ba.length() != 4)
        throw GUtil::Exception<>(f->errorString().toUtf8().constData());

    int ret = 0;
    for(int i = 3; i >= 0; i--)
        ret |= (int)((unsigned char)ba.at(3 - i)) << (i * 8);

    return ret;
}


void __add_children_to_database(V3::File_Entry *fe,
                                PasswordDatabase &pdb,
                                const EntryId &parent_id,
                                int &entry_ctr,
                                function<void(int)> progress_cb)
{
    GASSERT(fe->getType() == V3::File_Entry::directory);

    for(uint i = 0; i < fe->getContents().size(); ++i)
    {
        progress_cb(++entry_ctr);

        V3::File_Entry *cur = fe->getContents()[i];
        Entry e;
        e.SetParentId(parent_id);
        e.SetRow(cur->getRow());
        e.SetName(cur->getLabel().getContent());
        e.SetDescription(cur->getDescription().getContent());
        e.SetFavoriteIndex(cur->getFavorite());
        e.SetModifyDate(*cur->getModifyDate());

        if(V3::File_Entry::password == cur->getType()){
            for(const V3::Attribute &a : cur->getAttributes()){
                SecretValue sv;
                sv.SetName(a.getLabel());
                sv.SetValue(a.getContent().toUtf8());
                sv.SetIsHidden(a.secret);
                sv.SetNotes(a.notes);
                e.Values().append(sv);
            }
        }
        pdb.AddEntry(e, true);

        // We want to show true progress, which means waiting
        //  after each item until the background thread is done adding it
        pdb.WaitForEntryThreadIdle();

        if(V3::File_Entry::directory == cur->getType())
            __add_children_to_database(cur, pdb, e.GetId(), entry_ctr, progress_cb);
    }
}

static int __count_legacy_entries(V3::File_Entry *fe)
{
    int ret = fe->getContents().size();
    if(0 < ret){
        for(V3::File_Entry *fec : fe->getContents())
            ret += __count_legacy_entries(fec);
    }
    return ret;
}

void LegacyUtils::UpdateFileToCurrentVersion(
        const char *file_path,
        FileVersionEnum file_version,
        const char *new_path,
        const Credentials &old_creds,
        const Credentials &new_creds,
        function<void(int, const QString &)> progress_cb)
{
    if(0 == strcmp(file_path, new_path))
        throw Exception<>("Refusing to update if source and dest are the same");

    // Remove the target path if it exists
    if(QFile::exists(new_path))
        QFile::remove(new_path);

    QString progress_msg = QObject::tr("Parsing legacy database...");
    progress_cb(0, progress_msg);

    // First make sure we can parse the input file
    std::string xml;
    std::string file_data;
    QFile f(file_path);
    f.open(QFile::ReadOnly);

    // Skip over the version info; we already know it
    if(Version1 != file_version){
        char c;
        while(f.getChar(&c) && c != '|');
    }

    progress_cb(3, progress_msg);

    try{
        QByteArray ba;

        switch(file_version)
        {
        case Version1:
            // In version 1 the whole file is encrypted, with no version identifier
            ba = f.readAll();
            file_data = string(ba.constData(), ba.length());
            file_data.append((char)0);  // Make sure it's null-terminated
            xml = V1::Encryption::DecryptString(file_data.data(), old_creds.Password);
            break;
        case Version2:
            ba = f.readAll();
            file_data = string(ba.constData(), ba.length());
            xml = V2::Encryption::DecryptString(file_data, old_creds.Password);
            break;
        case Version3:
        {
            // In version 3 we also need to know the length of the payload
            uint len = __read_four_chars_into_int(&f);
            ba = f.read(len);
            file_data = string(ba.constData(), ba.length());
            if(file_data.length() != len)
                throw Exception<>("Error while reading file");
            xml = V3::Encryption::DecryptString(file_data, old_creds.Password);
        }
            break;
        default:
            GASSERT(false);
            break;
        }
    }
    catch(const ::CryptoPP::Exception &ex){
        throw Exception<>(ex.what());
    }

    progress_cb(7, progress_msg);

    // Parse the XML using the legacy object
    V3::Password_File pf;
    pf.readXML(QByteArray(xml.data(), xml.length()), file_version < 3);

    // Count the number of items so we know how much progress we're making
    int entry_cnt = __count_legacy_entries(pf.getContents());
    int entry_ctr = 0;

    progress_msg = QObject::tr("Populating new database...");
    progress_cb(10, progress_msg);

    // Create the updated database
    PasswordDatabase pdb(new_path, new_creds);

    // Iterate through the legacy object and populate the new object
    __add_children_to_database(pf.getContents(), pdb, EntryId::Null(), entry_ctr,
        [&](int entry_ctr){
            progress_msg = QString(QObject::tr("Populating new database (entry %1 of %2)..."))
                    .arg(entry_ctr)
                    .arg(entry_cnt);
            progress_cb(10 + 90.0 * ((float)entry_ctr / entry_cnt), progress_msg);
    });

    progress_msg = QObject::tr("Updated database");
    progress_cb(100, progress_msg);
}


}}
