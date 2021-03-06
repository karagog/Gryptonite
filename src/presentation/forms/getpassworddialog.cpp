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

#include <grypto/common.h>
#include "getpassworddialog.h"
#include "ui_getpassworddialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
USING_NAMESPACE_GUTIL1(Qt);

#define SETTING_LAST_CB_INDEX "gp_dlg_last_cb_index"

NAMESPACE_GRYPTO;


GetPasswordDialog::GetPasswordDialog(
        Settings *settings,
        const QString &filename,
        Credentials::TypeEnum creds_type,
        const QString &keyfile_loc,
        QWidget *parent)
    :QDialog(parent),
      ui(new Ui::GetPasswordDialog),
      m_settings(settings)
{
    if(filename.isEmpty())
        setWindowTitle(tr("Enter Key Info"));
    else
        setWindowTitle(QString(tr("Enter Key Info for %1")).arg(filename));
    setWindowModality(Qt::WindowModal);
    ui->setupUi(this);

    if(!keyfile_loc.isEmpty())
        ui->le_keyfile->setText(keyfile_loc);

    int old_index = ui->comboBox->currentIndex();
    switch(creds_type)
    {
    case Credentials::PasswordType:
        ui->comboBox->setCurrentIndex(0);
        break;
    case Credentials::KeyfileType:
        ui->comboBox->setCurrentIndex(1);
        break;
    case Credentials::PasswordAndKeyfileType:
        ui->comboBox->setCurrentIndex(2);
        break;
    case Credentials::NoType:
    default:
        if(settings->Contains(SETTING_LAST_CB_INDEX))
            ui->comboBox->setCurrentIndex(settings->Value(SETTING_LAST_CB_INDEX).toInt());
        break;
    }

    if(old_index == ui->comboBox->currentIndex())
        _combobox_indexchanged(old_index);
    ui->le_password->setFocus();
}

GetPasswordDialog::~GetPasswordDialog()
{
    delete ui;
}

void GetPasswordDialog::accept()
{
    int cb_ind = ui->comboBox->currentIndex();
    QByteArray tmp_password;
    QByteArray tmp_keyfile;
    if(cb_ind == 0 || cb_ind == 2)
        tmp_password = ui->le_password->text().toUtf8();

    if(cb_ind == 1 || cb_ind == 2)
    {
        tmp_keyfile = ui->le_keyfile->text().trimmed().toUtf8();
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

void GetPasswordDialog::_select_keyfile()
{
    QString dir = GetKeyfileLocation();

    // Open the dialog in the user's home directory if no path is given
    if(dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QString fn = QFileDialog::getOpenFileName(this, tr("Select Keyfile"), dir);
    if(fn.isEmpty())
        return;

    ui->le_keyfile->setText(fn);
}

void GetPasswordDialog::_combobox_indexchanged(int ind)
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

QString GetPasswordDialog::GetKeyfileLocation() const
{
    return ui->le_keyfile->text();
}


END_NAMESPACE_GRYPTO;
