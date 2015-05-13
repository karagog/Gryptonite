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

#ifndef DATAGENERATOR_H
#define DATAGENERATOR_H

#include <gutil/qt_settings.h>
#include <gutil/cryptopp_rng.h>
#include <QWidget>
#include <QPointer>
#include <QProgressDialog>

class QThread;

namespace Ui {
class DataGenerator;
}

class DataGenerator : public QWidget
{
    Q_OBJECT
    friend class raw_data_exporter;
    friend class uniform_exporter;
    friend class normal_exporter;
    friend class geometric_exporter;
    friend class exponential_exporter;
    friend class poisson_exporter;
public:
    explicit DataGenerator(QWidget *parent = 0);
    ~DataGenerator();

    void SaveParameters(GUtil::Qt::Settings *) const;
    void RestoreParameters(GUtil::Qt::Settings *);

signals:
    void ProgressUpdated(int);
    void NotifyInfo(const QString &);
    void NotifyError(const std::shared_ptr<std::exception> &);

private slots:
    void _select_file();
    void _mode_changed(int);
    void _distribution_changed(int);
    void _distribution_type_changed(int);

    void _generate();
    void _cancel();

    void _handle_progress(int);
    void _handle_error(const std::shared_ptr<std::exception> &);

private:
    Ui::DataGenerator *ui;
    GUtil::CryptoPP::RNG m_rng;
    QPointer<QProgressDialog> m_progressDialog;

    QScopedPointer<QThread> m_worker;
    bool m_cancel;

    void _export_raw_data(GUINT32 num_bytes, const QString &outfile);
    void _gen_dist_uniform(GUINT32 N, double min, double max, bool discrete, const QString &outfile);
    void _gen_dist_normal(GUINT32 N, double mean, double sigma, bool discrete, const QString &outfile);
    void _gen_dist_geometric(GUINT32 N, double E, const QString &outfile);
    void _gen_dist_exponential(GUINT32 N, double lambda, const QString &outfile);
    void _gen_dist_poisson(GUINT32 N, double E, const QString &outfile);
};

#endif // DATAGENERATOR_H
