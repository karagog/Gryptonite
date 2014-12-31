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

#ifndef GRYPTO_CRYPTOTRANSFORM_WORKER_H
#define	GRYPTO_CRYPTOTRANSFORM_WORKER_H

#include <gutil/cryptopp_cryptor.h>
#include <gutil/cryptopp_hash.h>
#include <gutil/smartpointer.h>
#include <QThread>

namespace Grypt
{


class CryptoTransformWorker :
    public QThread,
    protected GUtil::IProgressHandler
{
    Q_OBJECT
    bool m_cancel;
    bool m_ran;
    QString m_errorString;
public:

    QString const &ErrorString() const{ return m_errorString; }

    virtual ~CryptoTransformWorker();


signals:

    /** This signal is emitted whenever the progress has been updated by the cryptor. */
    void NotifyProgressUpdated(int);


public slots:

    /** You can call this to cause the operation to cancel. */
    void Cancel();


protected:

    /** This is an abstract base class; only derived classes can instantiate us. */
    explicit CryptoTransformWorker(QObject *p = 0);

    /** The worker thread code. */
    virtual void run();

    /** Instructs the subclasses to do their long, arduous work. */
    virtual void do_work() = 0;

    /** \name The progress handler interface
     *  \{
    */
    virtual void ProgressUpdated(int);
    virtual bool ShouldOperationCancel();
    /** \} */

};


class EncryptionWorker : public CryptoTransformWorker
{
    GUtil::CryptoPP::Cryptor m_cryptor;
    GUtil::SmartPointer<GUtil::IInput> m_in;
    GUtil::SmartPointer<GUtil::IOutput> m_out;
    GUtil::SmartPointer<GUtil::IInput> m_aData;
    const bool m_encrypt;
public:

    /** Creates and initializes a thread that will encrypt/decrypt data.

        No exceptions will leave the worker thread code, so you have to check errors after the
        thread finishes. If there was an error, then ErrorString() will have a description of what failed.
        If the error string is empty, then the operation was successful.

        \param cryptor The cryptor to use.
        \param encrypt True encrypts, false decrypts
        \param in The input data. If you're encrypting then this is the plaintext, if you're
                decrypting this is the crypttext.
        \param out The output will be pushed to this interface.
        \param aData Optional authenticated data.
    */
    EncryptionWorker(const GUtil::CryptoPP::Cryptor &cryptor,
                     bool encrypt,
                     GUtil::IInput *in,
                     GUtil::IOutput *out,
                     GUtil::IInput *aData = NULL,
                     QObject * = 0);


protected:

    virtual void do_work();

};


class HashingWorker : public CryptoTransformWorker
{
    GUtil::SmartPointer<GUtil::IInput> m_dataIn;
    GUtil::SmartPointer<GUtil::IOutput> m_digestOut;
    GUtil::SmartPointer<GUtil::IHash> m_hash;
public:

    /** Creates and initializes a thread that will hash data.
     *
     *  \param algorithm Select a hash algorithm.
     *  \param in The input data
     *  \param out The digest goes here. It must be the correct size for the hashing algorithm.
    */
    HashingWorker(GUtil::CryptoPP::HashAlgorithmEnum algorithm,
                  GUtil::IInput *in,
                  GUtil::IOutput *out,
                  QObject * = 0);

protected:

    virtual void do_work();

};


}

#endif	/* GRYPTO_CRYPTOTRANSFORM_WORKER_H */

