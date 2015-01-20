#include <iostream>
#include <cstdlib>
#include <string>
#include <QString>
#include <QFile>
#include "GUtil/Core/exception.h"
#include "GUtil/Core/Utils/encryption.h"
#include "GUtil/Core/Utils/commandlineargs.h"
using namespace std;
using namespace GUtil::Core::Utils;
GUTIL_USING_CORE_NAMESPACE(Utils);

int abort_func();
void show_usage();

int main(int argc, char **argv)
{
    {
        CommandLineArgs args(argc, argv);
        int tmpind;

        if((tmpind = args.FindArgument("-c", false)) != -1 &&
                args.Count() > tmpind + 1)
        {
            QString str(QString::fromStdString(args[tmpind + 1]));

        }
        else
        {
            show_usage();
            return -1;
        }
    }

    QString output;
    string password;

    if(argc != 2)
    {
        cout<<"Usage:  <program>  <file path of .GPdb file>"<<endl;
        return abort_func();
    }

    QFile pfile(QString::fromAscii(argv[1]));
    if(!pfile.exists())
    {
        cout<<"The file '"<<argv[1]<<"' doesn't exist"<<endl;
        return abort_func();
    }

    pfile.open(QFile::ReadOnly);
    QString input = QString::fromAscii(pfile.readAll().constData());
    pfile.close();

    cout<<"Enter password to decrypt (careful that nobody's watching!): ";
    cin>>password;

    bool success;
    bool tried = false;
    do
    {
        success = true;

        try
        {
            output = QString::fromStdString(
                        CryptoHelpers::decryptString(
                            input.toStdString(), password));
        }
        catch(GUtil::Core::Exception &ex)
        {
            throw;
//            if(!tried)
//            {
//                tried = true;
//                if(QString::fromStdString(ex.).contains("passphrase", Qt::CaseInsensitive))
//                {
//                    cout<<"Wrong password"<<endl;
//                    return abort_func();
//                }
//            }

//            // If we failed decryption, try taking bytes off one by one until it decrypts,
//            // or the file runs out of bytes to strip!
//            success = false;
//            input.resize(input.length() - 1);
        }

    }while(success == false && input.length() > 0);

    QFile ofile(pfile.fileName() + ".recovered");
    ofile.open(QFile::WriteOnly | QFile::Truncate);
    if(ofile.write(input.toAscii()) == input.length())
    {
        cout<<"Passwords exported to '"<<ofile.fileName().toStdString()<<"'"<<endl;
    }
    ofile.close();

    return abort_func();
}


int abort_func()
{
    cout<<endl<<"Press any key..."<<endl;
    cin.get();
    cin.get();

    return 0;
}

void show_usage()
{
    cout<<"Usage: <program> [-c <password file>]\n";
}
