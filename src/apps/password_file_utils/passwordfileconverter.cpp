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

#include "passwordfileconverter.h"
#include "CommonLib/DataAccess/da_passwordfile.h"
#include "CommonLib/DataObjects/Password_File.h"
#include "CommonLib/DataObjects/Entry.h"
#include "CommonLib/DataObjects/entrytable.h"
#include "GUtil/Core/exception.h"
#include "GUtil/Core/Utils/encryption.h"
#include "GUtil/BusinessObjects/BinaryDataStore.h"
#include "Legacy/encryption.h"
#include "grypto_global.h"
#include <QFile>
#include <string>
using namespace std;
using namespace GUtil::Core;
GUTIL_USING_CORE_NAMESPACE(Utils);
GUTIL_USING_NAMESPACE(BusinessObjects);
USING_GRYPTO_NAMESPACE2(Common, DataObjects);

void PasswordFileConverter::ConvertPasswordFile(const QString &filename,
                                                const QString &password)
{
    QByteArray dat;
    QFile f(filename);
    if(!f.exists())
        THROW_NEW_GUTIL_EXCEPTION2(ArgumentException,
                                   QString("File does not exist: %1")
                                   .arg(filename).toStdString());

    if(!f.open(QFile::ReadOnly))
        THROW_NEW_GUTIL_EXCEPTION2(Exception,
                                   QString("Unable to open file: %1\n%2")
                                   .arg(filename)
                                   .arg(f.errorString()).toStdString());

    QString __version;
    bool is_version_1(false),
            is_version_2(false),
            is_version_3(false);

    {
        char c;
        while(f.getChar(&c) && c != '|')
            __version.append(c);

        if(__version.size() == f.size())
            is_version_1 = true;
        else
        {
            if(__version == "3")
            {
                is_version_3 = true;

                try
                {
                    //dat = f.read(DA_PasswordFile::_fourCharsToInt(f.read(4)));
                }
                catch(Exception &ex)
                {
                    THROW_NEW_GUTIL_EXCEPTION3(Exception, "Unable to read file",
                                               ex);
                }
            }
            else if(__version == "2")
            {
                is_version_2 = true;
                dat = f.readAll();
            }
            else
            {
                bool valid_version_number;
                __version.toInt(&valid_version_number);

                QString msg = "The file you have selected is not recognized for use "
                              "with this version of the application.";
                if(valid_version_number)
                    msg.append(QString("\n\nHint: The file is from version %1")
                               .arg(__version));

                Exception ex("Unrecognized file");
                ex.SetData("message", msg.toStdString());
                THROW_GUTIL_EXCEPTION(ex);
            }
        }
    }

    QString res = QString::fromStdString(string(dat.constData(), dat.length()));

    bool is_prior_to_version_3(false);
    if(is_version_3)
    {
        try
        {
            res = QString::fromStdString(
                    CryptoHelpers::decompress(
                            CryptoHelpers::decryptString(
                                    res.toStdString(), password.toStdString())));
        }
        catch(Exception &ex)
        {
            THROW_NEW_GUTIL_EXCEPTION3(Exception, "Password Incorrect", ex);
        }
    }
    else
    {
        is_prior_to_version_3 = true;

        // Proceed with the file conversion...
        if(is_version_2)
        {
            res = QString::fromStdString(LegacyRoutines::DecryptString_v2(
                    res.toStdString(), password.toStdString()));
            if(res == "")
                THROW_NEW_GUTIL_EXCEPTION2(Exception, "Password Incorrect");
        }
        else if(is_version_1)
        {
            try
            {
                res = LegacyRoutines::DecryptString_v1(res.toStdString().c_str(), password.toStdString().c_str());
            }
            catch(std::exception &)
            {
                THROW_NEW_GUTIL_EXCEPTION2(Exception, "Password Incorrect");
            }
        }


        QString suffix;
        if(is_prior_to_version_3)
            suffix = "GPdb";
        else
            suffix = "Gdb";

//        QString backup_filename = QString("%1.backup.%2")
//                .arg(tmpfile_loc.left(tmpfile_loc.lastIndexOf('.')))
//                .arg(suffix);

        //DA_PasswordFile::BackupFile(tmpfile_loc, backup_filename);
    }

    {
        Password_File pf;
        BinaryDataStore ds(PROGRAM_STRING "_temp_conversion");

        pf.FromXmlQString(res);

        if(is_version_2)
        {
            // Move the binary data into the binary data store
            foreach(Entry *fe, pf.GetAllEntries())
            {
                int ind(fe->BinaryDataIndex());
                if(ind != -1)
                {
                    string tmps(CryptoHelpers::compress(
                                    CryptoHelpers::fromBase64(
                                        fe->GetDataTable()[ind]
                                        .GetData().toStdString())));

                    fe->GetDataTable()[ind]
                            .SetData(QString("%1")
                                     .arg(ds.AddFile(QByteArray(tmps.c_str(),
                                                                tmps.size()))));
                }
            }
        }


    }
}
