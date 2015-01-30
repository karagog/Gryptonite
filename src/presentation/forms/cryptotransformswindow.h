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

#ifndef GRYPTO_CRYPTO_TRANSFORMS_WINDOW_H
#define GRYPTO_CRYPTO_TRANSFORMS_WINDOW_H

#include <gutil/smartpointer.h>
#include <QWidget>
#include <QProgressDialog>

namespace Ui {
class CryptoTransformsWindow;
}

namespace GUtil{ namespace CryptoPP{
class Cryptor;
}

namespace Qt{
class Settings;
}}

namespace Grypt
{

class CryptoTransformWorker;


class CryptoTransformsWindow : public QWidget
{
    Q_OBJECT
    Ui::CryptoTransformsWindow *ui;
    GUtil::Qt::Settings *m_settings;
    GUtil::SmartPointer<GUtil::CryptoPP::Cryptor> m_cryptor;
    GUtil::SmartPointer<CryptoTransformWorker> m_worker;
    QProgressDialog m_progressDialog;
    QByteArray m_sourceString;
    QByteArray m_destString;
    bool m_stateSaved;
public:

    explicit CryptoTransformsWindow(GUtil::Qt::Settings *, QWidget *parent = 0);
    ~CryptoTransformsWindow();

    virtual void closeEvent(QCloseEvent *);


private slots:

    void _select_file_src();
    void _select_file_dest();
    void _change_password();
    void _test_password();
    void _mode_changed(int);

    void _do_it();
    void _worker_finished();


private:

    void _update_key_status();
    bool _encrypting() const;
    bool _is_string_output() const;
    bool _is_file_output() const;
    void _save_state();

};


}

#endif // GRYPTO_CRYPTO_TRANSFORMS_WINDOW_H
