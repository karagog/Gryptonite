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

#include "cryptotransformswindow.h"
#include "ui_cryptotransformswindow.h"
#include "grypto_common.h"
#include "newpassworddialog.h"
#include "getpassworddialog.h"
#include <gutil/sourcesandsinks.h>
#include <gutil/qtsourcesandsinks.h>
#include "grypto_cryptotransformworker.h"
#include <gutil/cryptopp_cryptor.h>
#include <gutil/file.h>
#include <gutil/smartpointer.h>
#include <gutil/qt_settings.h>
#include <QMessageBox>
#include <QFileDialog>
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GUTIL1(Qt);
USING_NAMESPACE_GUTIL1(CryptoPP);

#define SETTING_LAST_MODE           "ctw_last_mode"

#define SETTING_LAST_SOURCE             "ctw_last_source"
#define SETTING_LAST_SOURCE_ENCODING    "ctw_last_source_encoding"

#define SETTING_LAST_DEST           "ctw_last_dest"
#define SETTING_LAST_DEST_ENCODING  "ctw_last_dest_encoding"

#define SETTING_LAST_HASH           "ctw_last_hash"

#define SETTING_GEOMETRY            "ctw_geometry"

NAMESPACE_GRYPTO;


CryptoTransformsWindow::CryptoTransformsWindow(GUtil::Qt::Settings *settings, Cryptor const *c, QWidget *parent)
    :QWidget(parent, ::Qt::Window),
      ui(new Ui::CryptoTransformsWindow),
      m_cryptor(c == NULL ? NULL : new Cryptor(*c)),
      m_progressDialog(tr("Processing..."), tr("Cancel"), 0, 100, this),
      m_settings(settings),
      m_stateSaved(false)
{
    ui->setupUi(this);

    m_progressDialog.setWindowModality(::Qt::WindowModal);

    // Set up the hash combobox:
    ui->cb_hashType->addItem("SHA3-512", SHA3_512);
    ui->cb_hashType->addItem("SHA3-384", SHA3_384);
    ui->cb_hashType->addItem("SHA3-256", SHA3_256);
    ui->cb_hashType->addItem("SHA3-224", SHA3_224);
    ui->cb_hashType->addItem("SHA-512", SHA2_512);
    ui->cb_hashType->addItem("SHA-384", SHA2_384);
    ui->cb_hashType->addItem("SHA-256", SHA2_256);
    ui->cb_hashType->addItem("SHA-224", SHA2_224);
    ui->cb_hashType->addItem("Whirlpool", Whirlpool);
    ui->cb_hashType->addItem("Tiger", Tiger);
    ui->cb_hashType->addItem("RIPEMD-320", RIPEMD320);
    ui->cb_hashType->addItem("RIPEMD-256", RIPEMD256);
    ui->cb_hashType->addItem("RIPEMD-160", RIPEMD160);
    ui->cb_hashType->addItem("RIPEMD-128", RIPEMD128);
    ui->cb_hashType->addItem("SHA-1", SHA1);
    ui->cb_hashType->addItem("MD5", MD5);
    ui->cb_hashType->addItem("MD4", MD4);
    ui->cb_hashType->addItem("MD2", MD2);

    if(settings->Contains(SETTING_GEOMETRY))
        restoreGeometry(settings->Value(SETTING_GEOMETRY).toByteArray());
    if(settings->Contains(SETTING_LAST_MODE))
        ui->cb_mode->setCurrentIndex(settings->Value(SETTING_LAST_MODE).toInt());
    if(settings->Contains(SETTING_LAST_HASH))
        ui->cb_hashType->setCurrentIndex(settings->Value(SETTING_LAST_HASH).toInt());
    if(settings->Contains(SETTING_LAST_SOURCE))
        ui->cb_source->setCurrentIndex(settings->Value(SETTING_LAST_SOURCE).toInt());
    if(settings->Contains(SETTING_LAST_SOURCE_ENCODING))
        ui->cb_sourceEncoding->setCurrentIndex(settings->Value(SETTING_LAST_SOURCE_ENCODING).toInt());
    if(settings->Contains(SETTING_LAST_DEST))
        ui->cb_dest->setCurrentIndex(settings->Value(SETTING_LAST_DEST).toInt());
    if(settings->Contains(SETTING_LAST_DEST_ENCODING))
        ui->cb_destEncoding->setCurrentIndex(settings->Value(SETTING_LAST_DEST_ENCODING).toInt());

    _update_key_status();
}

CryptoTransformsWindow::~CryptoTransformsWindow()
{
    _save_state();
    delete ui;
}

void CryptoTransformsWindow::closeEvent(QCloseEvent *ev)
{
    _save_state();
    QWidget::closeEvent(ev);
}

void CryptoTransformsWindow::_save_state()
{
    if(!m_stateSaved){
        m_settings->SetValue(SETTING_GEOMETRY, saveGeometry());
        m_settings->SetValue(SETTING_LAST_MODE, ui->cb_mode->currentIndex());
        m_settings->SetValue(SETTING_LAST_HASH, ui->cb_hashType->currentIndex());
        m_settings->SetValue(SETTING_LAST_SOURCE, ui->cb_source->currentIndex());
        m_settings->SetValue(SETTING_LAST_SOURCE_ENCODING, ui->cb_sourceEncoding->currentIndex());
        m_settings->SetValue(SETTING_LAST_DEST, ui->cb_dest->currentIndex());
        m_settings->SetValue(SETTING_LAST_DEST_ENCODING, ui->cb_destEncoding->currentIndex());
        m_settings->CommitChanges();
        m_stateSaved = true;
    }
}

void CryptoTransformsWindow::_update_key_status()
{
    if(m_cryptor){
        ui->lbl_keyStatus->setText(tr("Key Set"));
        ui->btn_testPassword->setEnabled(true);
    }
    else{
        ui->lbl_keyStatus->setText(tr("(No Key)"));
        ui->btn_testPassword->setEnabled(false);
    }
}

void CryptoTransformsWindow::_change_password()
{
    NewPasswordDialog dlg(m_settings, this);
    if(QDialog::Accepted == dlg.exec()){
        if(m_cryptor)
            m_cryptor->ChangeCredentials(dlg.GetCredentials());
        else
            m_cryptor = new Cryptor(dlg.GetCredentials());
    }
    _update_key_status();
}

void CryptoTransformsWindow::_test_password()
{
    GetPasswordDialog dlg(m_settings, QString(), this);
    if(QDialog::Accepted == dlg.exec()){
        if(!m_cryptor)
            QMessageBox::critical(this, tr("No Key"), tr("No key has been set"));
        else if(m_cryptor->CheckCredentials(dlg.GetCredentials()))
            QMessageBox::information(this, tr("OK"), tr("Key is correct"));
        else
            QMessageBox::critical(this, tr("Failed"), tr("Key is incorrect"));
    }
}

bool CryptoTransformsWindow::_encrypting() const
{
    return ui->cb_mode->currentIndex() == 0;
}

bool CryptoTransformsWindow::_is_string_output() const
{
    return ui->cb_dest->currentIndex() == 1;
}

bool CryptoTransformsWindow::_is_file_output() const
{
    return ui->cb_dest->currentIndex() == 0;
}

static QByteArray __convert_from_encoding(const QByteArray &raw_text, int encoding)
{
    QByteArray ret;
    switch(encoding)
    {
    case 0:
        // Hex string
        ret = QByteArray::fromHex(raw_text);
        break;
    case 1:
        // Base-64
        ret = QByteArray::fromBase64(raw_text);
        break;
    case 2:
        // Regular text
        ret = raw_text;
        break;
    default:
        GASSERT(false);
    }
    return ret;
}

static QByteArray __convert_to_encoding(const QByteArray &text, int encoding)
{
    QByteArray ret;
    switch(encoding)
    {
    case 0:
        // Hex string
        ret = text.toHex();
        break;
    case 1:
        // Base-64
        ret = text.toBase64();
        break;
    case 2:
        // Regular text (make sure it's valid UTF-8)
        if(String::IsValidUTF8(text.constData(), text.constData() + text.length()))
            ret = text;
        else
            throw ValidationException<>("Invalid UTF-8 sequence");
        break;
    default:
        GASSERT(false);
    }
    return ret;
}

void CryptoTransformsWindow::_do_it()
{
    if(m_worker)
        return;

    bool show_dialog = false;
    SmartPointer<IInput> input;
    SmartPointer<IOutput> output;
    try
    {
        // Make sure we have all the needed inputs

        if(ui->cb_mode->currentIndex() < 2 && !m_cryptor)
            throw Exception<>("No password set");

        if(ui->cb_source->currentIndex() == 0){
            // Source is a file
            QByteArray fn = ui->le_src->text().toUtf8();
            if(!File::Exists(fn))
                throw Exception<>("Source file doesn't exist");
            File *f = new File(fn);
            input = f;
            f->Open(File::OpenRead);
            show_dialog = true;
        }
        else{
            // Source is a string
            m_sourceString = __convert_from_encoding(ui->te_src->toPlainText().toUtf8(), ui->cb_sourceEncoding->currentIndex());
            if(!m_sourceString.isEmpty())
                input = new ByteArrayInput(m_sourceString.constData(), m_sourceString.length());
        }


        if(ui->cb_dest->currentIndex() == 0){
            // Dest is a file
            File *f = new File(ui->le_dest->text().trimmed().toUtf8());
            output = f;
            f->Open(File::OpenReadWriteTruncate);
            show_dialog = true;
        }
        else{
            // Dest is a string
            output = new QByteArrayOutput(m_destString);
        }
    }
    catch(const Exception<> &ex)
    {
        QMessageBox::warning(this, tr("Oops..."), QString::fromStdString(ex.Message()));
        return;
    }


    // Apply the cryptographic transformation
    if(ui->cb_mode->currentIndex() == 3)
    {
        // Pass-through mode (no worker needed)
        GUINT32 byte_len = input ? input->BytesAvailable() : 0;
        if(byte_len > 0){
            SmartArrayPointer<byte> ba(new byte[byte_len]);
            input->ReadBytes(ba, byte_len, byte_len);
            output->WriteBytes(ba, byte_len);
        }
        _worker_finished();
    }
    else
    {
        // Start up the background thread
        if(ui->cb_mode->currentIndex() == 2)
        {
            // Hashing mode
            m_worker = new HashingWorker(
                        (HashAlgorithmEnum)ui->cb_hashType->itemData(ui->cb_hashType->currentIndex()).toInt(),
                        input, output);
        }
        else
            m_worker = new EncryptionWorker(*m_cryptor, _encrypting(), input, output);

        connect(&m_progressDialog, SIGNAL(canceled()), m_worker, SLOT(Cancel()));
        connect(m_worker, SIGNAL(NotifyProgressUpdated(int)), &m_progressDialog, SLOT(setValue(int)));
        connect(m_worker, SIGNAL(finished()), this, SLOT(_worker_finished()));
        m_worker->start();

        // show the progress dialog if it's going to be a long operation
        if(show_dialog)
            m_progressDialog.show();

        // Relinquish the pointers because the worker will delete them
        output.Relinquish();
        input.Relinquish();
    }
}

void CryptoTransformsWindow::_worker_finished()
{
    QString err_str;
    if(m_worker)
    {
        err_str = m_worker->ErrorString();
        m_worker->deleteLater();
        m_worker.Relinquish();
        m_progressDialog.hide();
    }
    ui->te_dest->clear();

    // Attempt to convert to the destination encoding
    if(err_str.isEmpty()){
        if(_is_string_output()){
            try{
                m_destString = __convert_to_encoding(m_destString, ui->cb_destEncoding->currentIndex());
            }
            catch(const GUtil::Exception<> &ex){
                err_str = QString(tr("Output error: %1")).arg(ex.Message().data());
            }
        }
    }

    if(!err_str.isEmpty()){
        // Operation failed
        QMessageBox::critical(this, tr("Failure"), err_str);

        if(_is_file_output()){
            // Remove the incomplete dest file
            File f(ui->le_dest->text().trimmed().toUtf8());
            if(f.Exists())
                f.Delete();
        }
    }
    else
    {
        // Operation success
        if(_is_string_output())
            ui->te_dest->setPlainText(m_destString);
        else if(_is_file_output())
            QMessageBox::information(this, tr("Success"), tr("File operation successful"));
    }
    m_sourceString.clear();
    m_destString.clear();
}

void CryptoTransformsWindow::_select_file_src()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select source file"));
    if(!path.isEmpty())
        ui->le_src->setText(path);
}

void CryptoTransformsWindow::_select_file_dest()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Select destination file"));
    if(!path.isEmpty())
        ui->le_dest->setText(path);
}

void CryptoTransformsWindow::_mode_changed(int i)
{
    switch(i)
    {
    case 0:
    case 1:
        ui->sw_mode->setCurrentIndex(0);
        ui->sw_mode->show();
        break;
    case 2:
        ui->sw_mode->setCurrentIndex(1);
        ui->sw_mode->show();
        break;
    case 3:
        ui->sw_mode->hide();
        break;
    default:
        break;
    }
}


END_NAMESPACE_GRYPTO;
