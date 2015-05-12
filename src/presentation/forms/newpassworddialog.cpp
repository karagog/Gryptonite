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

#include "newpassworddialog.h"
#include "ui_newpassworddialog.h"
#include <grypto/common.h>
#include <gutil/file.h>
#include <QMessageBox>
#include <QKeyEvent>
#include <QFileDialog>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL1(Qt);

// It doesn't make sense for this to be any larger than
//  the encryption key size for AES
#define KEYFILE_SIZE 32

#define SETTING_LAST_CB_INDEX "np_dlg_last_cb_index"

NAMESPACE_GRYPTO;


NewPasswordDialog::NewPasswordDialog(GUtil::Qt::Settings *settings,
                                     const QString &filename,
                                     QWidget *par)
    :QDialog(par),
      ui(new Ui::NewPassword),
      m_settings(settings)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModal);
    setWindowTitle(QString(tr("New Key Info for %1")).arg(filename));

    // We want to intercept when the user presses 'return'
    ui->lineEdit->installEventFilter(this);
    ui->lineEdit_2->installEventFilter(this);

    ui->lineEdit->setFocus();

    if(settings->Contains(SETTING_LAST_CB_INDEX)){
        int old_index = ui->comboBox->currentIndex();
        ui->comboBox->setCurrentIndex(settings->Value(SETTING_LAST_CB_INDEX).toInt());
        if(old_index == ui->comboBox->currentIndex())
            _combobox_indexchanged(old_index);
    }
    else
        _combobox_indexchanged(ui->comboBox->currentIndex());
}

bool NewPasswordDialog::eventFilter(QObject *o, QEvent *ev)
{
    bool ret = false;
    if(ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = (QKeyEvent *)ev;
        if(ke->key() == Qt::Key_Return)
        {
            if(o == ui->lineEdit){
                ui->lineEdit_2->setFocus();
                ui->lineEdit_2->selectAll();
            }
            else
                accept();
            ret = true;
        }
    }
    return ret;
}

NewPasswordDialog::~NewPasswordDialog()
{
    delete ui;
}

void NewPasswordDialog::accept()
{
    int cb_ind = ui->comboBox->currentIndex();
    QByteArray tmp_password;
    QByteArray tmp_keyfile;
    if(cb_ind == 0 || cb_ind == 2)
    {
        QString pw1 = ui->lineEdit->text();
        QString pw2 = ui->lineEdit_2->text();

        if(!(pw1.isEmpty() && pw2.isEmpty()) && pw1 != pw2)
        {
            QMessageBox::warning(this, "Password Mismatch", "The two passwords you have entered do not match");
            return;
        }

        if(pw1.isEmpty())
        {
            // Null passwords are allowed, but discouraged
            QMessageBox::StandardButton res =
                    QMessageBox::warning(this, tr("Empty Password"),
                                         tr("You have chosen an empty password."
                                            " Please confirm you want to do this.\n\n"
                                            "Your secrets will be obscured, but they will"
                                            " not be secured without a solid password."),
                                         QMessageBox::Ok | QMessageBox::Cancel,
                                         QMessageBox::Cancel);

            if(QMessageBox::Cancel == res)
                return;
        }
        else
        {
            tmp_password = pw2.toUtf8();
        }
    }

    if(cb_ind == 1 || cb_ind == 2)
    {
        tmp_keyfile = ui->le_filePath->text().trimmed().toUtf8();
        QFile kf(tmp_keyfile);
        if(kf.fileName().isEmpty()){
            QMessageBox::critical(this, tr("No Keyfile"),
                                  tr("You must provide a keyfile"));
            return;
        }
        if(!kf.exists()){
            QMessageBox::critical(this, tr("Keyfile Not Found"),
                                  QString(tr("Could not find the keyfile: %1")).arg(tmp_keyfile.constData()));
            return;
        }

        if(kf.size() == 0)
        {
            // Null keyfiles are allowed, but discouraged
            QMessageBox::StandardButton res =
                    QMessageBox::warning(this, tr("Empty Keyfile"),
                                         tr("You have chosen an empty keyfile."
                                            " Please confirm you want to do this.\n\n"

                                            "Your secrets will be obscured, but they will"
                                            " not be secured without a solid keyfile. \n\n"

                                            "A good Keyfile consists of random data."),
                                         QMessageBox::Ok | QMessageBox::Cancel,
                                         QMessageBox::Cancel);

            if(QMessageBox::Cancel == res)
                return;
        }
    }

    m_creds.Type = (Credentials::TypeEnum)ui->comboBox->currentIndex();
    m_creds.Password = tmp_password.constData();
    m_creds.Keyfile = tmp_keyfile.constData();
    m_settings->SetValue(SETTING_LAST_CB_INDEX, ui->comboBox->currentIndex());
    m_settings->CommitChanges();
    QDialog::accept();
}

void NewPasswordDialog::_select_keyfile()
{
    QString dir = GetKeyfileLocation();

    // Open the dialog in the user's home directory if no path is given
    if(dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QString fn = QFileDialog::getOpenFileName(this, tr("Select Keyfile"), dir);
    if(fn.isEmpty())
        return;

    ui->le_filePath->setText(fn);
}

void NewPasswordDialog::_generate_keyfile()
{
    QString fn;
    auto get_keyfile_loc = [&]{
        return QFileDialog::getSaveFileName(this, tr("New Keyfile Location"),
                                            QString(), QString(), NULL,
                                            QFileDialog::DontConfirmOverwrite);
    };

    fn = get_keyfile_loc();
    while(QFile::exists(fn)){
        QMessageBox::warning(this, tr("File Exists"),
                             tr("For your protection, I cannot overwrite an existing file.\n"
                                "Please try again."));
        fn = get_keyfile_loc();
    }

    if(fn.isEmpty())
        return;

    fn = QFileInfo(fn).absoluteFilePath();
    {
        byte data[KEYFILE_SIZE];
        GUtil::GlobalRNG()->Fill(data, KEYFILE_SIZE);

        QFile f(fn);
        if(!f.open(QFile::ReadWrite | QFile::Truncate))
            throw GUtil::Exception<>(QString("Cannot open file: %1")
                                     .arg(f.errorString()).toUtf8());
        f.write((const char *)data, KEYFILE_SIZE);
    }
    ui->le_filePath->setText(fn);
}

void NewPasswordDialog::_combobox_indexchanged(int ind)
{
    ui->gb_keyfile->hide();
    ui->gb_password->hide();
    resize(width(), 0);

    switch(ind)
    {
    case 0:
        ui->gb_password->show();
        break;
    case 1:
        ui->gb_keyfile->show();
        break;
    case 2:
        ui->gb_keyfile->show();
        ui->gb_password->show();
        break;
    default:
        break;
    }
}

QString NewPasswordDialog::GetKeyfileLocation() const
{
    return ui->le_filePath->text();
}


END_NAMESPACE_GRYPTO;
