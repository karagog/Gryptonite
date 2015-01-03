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

#include "cryptotransformworker.h"
#include "grypto_common.h"
#include <gutil/file.h>
USING_NAMESPACE_GUTIL;
USING_NAMESPACE_GUTIL1(CryptoPP);
using namespace std;

NAMESPACE_GRYPTO;

// Read files in ~16KB chunks
#define DEFAULT_CHUNK_SIZE 0x4000


CryptoTransformWorker::CryptoTransformWorker(QObject *p)
    :QThread(p),
      m_cancel(false),
      m_ran(false)
{}

CryptoTransformWorker::~CryptoTransformWorker()
{
    wait();
}

void CryptoTransformWorker::run()
{
    if(m_ran){
        m_errorString = tr("The worker thread is designed for one-time use");
        return;
    }

    m_ran = true;
    try
    {
        do_work();
    }
    catch(const GUtil::AuthenticationException<> &)
    {
        // Decryption failed due to a bad password
        m_errorString = tr("Authentication failed");
    }
    catch(const GUtil::CancelledOperationException<> &)
    {
        // The user cancelled
        m_errorString = tr("User cancelled operation");
    }
    catch(const GUtil::Exception<> &ex)
    {
        m_errorString = QString::fromStdString(ex.Message());
    }
    catch(...)
    {
        // Unknown problem (maybe the file could not be read?)
        m_errorString = tr("Unknown error");
    }
}

void CryptoTransformWorker::Cancel()
{
    m_cancel = true;
}

bool CryptoTransformWorker::ShouldOperationCancel()
{
    return m_cancel;
}

void CryptoTransformWorker::ProgressUpdated(int p)
{
    // This function is called on the background thread, so we emit a signal
    //  which will be queued on the receiver's event queue and executed on the main thread.
    emit NotifyProgressUpdated(p);
}


EncryptionWorker::EncryptionWorker(const Cryptor &c,
                                   bool encrypt,
                                   IInput *in,
                                   IOutput *out,
                                   IInput *aData,
                                   QObject *p)
    :CryptoTransformWorker(p),
      m_cryptor(c),
      m_in(in),
      m_out(out),
      m_aData(aData),
      m_encrypt(encrypt)
{}

void EncryptionWorker::do_work()
{
    if(m_encrypt)
        m_cryptor.EncryptData(m_out, m_in, m_aData, NULL, DEFAULT_CHUNK_SIZE, this);
    else
        m_cryptor.DecryptData(m_out, m_in, m_aData, DEFAULT_CHUNK_SIZE, this);
}

HashingWorker::HashingWorker(HashAlgorithmEnum algorithm,
                             IInput *in,
                             IOutput *out,
                             QObject *p)
    :CryptoTransformWorker(p),
      m_dataIn(in),
      m_digestOut(out),
      m_hash(GUtil::CryptoPP::Hash<>::CreateHash(algorithm))
{}

void HashingWorker::do_work()
{
    unique_ptr<byte[]> digest(new byte[m_hash->DigestSize()]);
    m_hash->AddDataFromDevice(m_dataIn, DEFAULT_CHUNK_SIZE, this);
    m_hash->Final(digest.get());
    m_digestOut->WriteBytes(digest.get(), m_hash->DigestSize());
}


END_NAMESPACE_GRYPTO;
