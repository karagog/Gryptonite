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

#include "datagenerator.h"
#include "ui_datagenerator.h"
#include <gutil/file.h>
#include <gutil/exception.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>
#include <functional>
#include <cstdio>
USING_NAMESPACE_GUTIL;

#define DEFAULT_CHUNK_SIZE  10000

#define DECIMAL_PRECISION 4

#define SETTING_DATAGEN_MODE            "dg_mode"
#define SETTING_DATAGEN_OUTPUT_FILE     "dg_outfile"
#define SETTING_DATAGEN_RAWDATA_AMOUNT  "dg_rawdata_amount"
#define SETTING_DATAGEN_RAWDATA_UNIT    "dg_rawdata_unit"
#define SETTING_DATAGEN_DISTTYPE        "dg_distType"
#define SETTING_DATAGEN_DISTRIBUTION    "dg_dist"
#define SETTING_DATAGEN_NSAMPLES        "dg_N"
#define SETTING_DATAGEN_U_MIN           "dg_U_min"
#define SETTING_DATAGEN_U_MAX           "dg_U_max"
#define SETTING_DATAGEN_N_MEAN          "dg_N_mean"
#define SETTING_DATAGEN_N_STDEV         "dg_N_stdev"
#define SETTING_DATAGEN_GEO_E           "dg_geo_e"
#define SETTING_DATAGEN_EXP_LAMBDA      "dg_exp_lambda"
#define SETTING_DATAGEN_POISSON_E       "dg_poisson_e"

enum mode_index_enum
{
    mode_distribution = 0,
    mode_raw_data = 1
};

enum distribution_index_enum
{
    dist_uniform = 0,
    dist_normal = 1,
    dist_geometric = 2,
    dist_exponential = 3,
    dist_poisson = 4
};

enum distribution_type_index_enum
{
    disttype_continuous = 0,
    disttype_discrete = 1
};

enum unit_type_index_enum
{
    unit_bytes = 0,
    unit_kilobytes = 1,
    unit_megabytes = 2
};


DataGenerator::DataGenerator(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::DataGenerator),
      m_cancel(false)
{
    ui->setupUi(this);

    // This connection will be queued because errors are emitted
    //  from the background thread
    connect(this, SIGNAL(NotifyError(std::shared_ptr<std::exception>)),
            this, SLOT(_handle_error(std::shared_ptr<std::exception>)));

    connect(this, SIGNAL(ProgressUpdated(int)), this, SLOT(_handle_progress(int)));
}

DataGenerator::~DataGenerator()
{
    delete ui;
    m_worker.waitForFinished();
}

void DataGenerator::_select_file()
{
    QString fn = QFileDialog::getSaveFileName(this, tr("Select output file"));
    if(fn.isEmpty())
        return;

    ui->le_outFile->setText(QFileInfo(fn).absoluteFilePath());
}

void DataGenerator::_mode_changed(int ind)
{
    switch(ind){
    case mode_distribution:
        ui->sw_mode->setCurrentWidget(ui->pg_distribution);
        break;
    case mode_raw_data:
        ui->sw_mode->setCurrentWidget(ui->pg_rawData);
        break;
    default:
        break;
    }
}

void DataGenerator::_distribution_changed(int ind)
{
    switch(ind){
    case dist_uniform:
        ui->sw_params->setCurrentWidget(ui->pg_uniform);
        ui->cmb_distributionType->setEnabled(true);
        break;
    case dist_normal:
        ui->sw_params->setCurrentWidget(ui->pg_normal);
        ui->cmb_distributionType->setEnabled(true);
        break;
    case dist_geometric:
        ui->sw_params->setCurrentWidget(ui->pg_geometric);
        ui->cmb_distributionType->setCurrentIndex((int)disttype_discrete);
        ui->cmb_distributionType->setEnabled(false);
        break;
    case dist_exponential:
        ui->sw_params->setCurrentWidget(ui->pg_exponential);
        ui->cmb_distributionType->setCurrentIndex((int)disttype_continuous);
        ui->cmb_distributionType->setEnabled(false);
        break;
    case dist_poisson:
        ui->sw_params->setCurrentWidget(ui->pg_poisson);
        ui->cmb_distributionType->setCurrentIndex((int)disttype_discrete);
        ui->cmb_distributionType->setEnabled(false);
        break;
    default:
        break;
    }
}

void DataGenerator::_distribution_type_changed(int ind)
{
    switch(ind){
    case disttype_continuous:
        ui->spn_u_min->setDecimals(DECIMAL_PRECISION);
        ui->spn_u_max->setDecimals(DECIMAL_PRECISION);
        break;
    case disttype_discrete:
        ui->spn_u_min->setDecimals(0);
        ui->spn_u_max->setDecimals(0);
        break;
    default:
        break;
    }
}

#define CREATE_AND_OPEN_OUTPUT_FILE(fn) \
    File outfile(fn.toUtf8().constData()); \
    outfile.Open(File::OpenReadWriteTruncate)

void DataGenerator::_export_raw_data(GUINT32 num_bytes,
                                     const QString &fn)
{
    try{
        CREATE_AND_OPEN_OUTPUT_FILE(fn);

        class rng_data_source : public GUtil::IInput{
            GUtil::CryptoPP::RNG &m_rng;
        public:
            rng_data_source(GUtil::CryptoPP::RNG &r) :m_rng(r) {}
            virtual GUINT32 ReadBytes(byte *b, GUINT32 len, GUINT32 to_read){
                to_read = Min(len, to_read);
                m_rng.Fill(b, to_read);
                return to_read;
            }
        } rng_source(m_rng);

        rng_source.Pump(&outfile, num_bytes, DEFAULT_CHUNK_SIZE,
            [&](int p) -> bool{
                emit ProgressUpdated(p);
                return m_cancel;
            });
    }
    catch(const Exception<> &ex){
        emit NotifyError(std::shared_ptr<std::exception>(
                             static_cast<std::exception*>((Exception<> *)ex.Clone()))
                         );
    }
}

#define SMALL_BUFFER_SIZE 100

void DataGenerator::_gen_dist_uniform(GUINT32 N, double min, double max, bool discrete, const QString &fn)
{
    char buf[SMALL_BUFFER_SIZE];
    GUINT32 progress_increment = N / 100;
    try{
        CREATE_AND_OPEN_OUTPUT_FILE(fn);

        for(GUINT32 i = 0; i < N; i++){
            if(0 == i % progress_increment)
                emit ProgressUpdated((double)i/N * 100);

            if(m_cancel)
                throw CancelledOperationException<>();

            int len;
            if(discrete){
                const int X = m_rng.U_Discrete(min, max);
                len = sprintf(buf, "%d\n", X);
            }
            else{
                const double X = m_rng.U(min, max);
                len = sprintf(buf, "%f\n", X);
            }
            outfile.Write(buf, len);
        }
        emit ProgressUpdated(100);
    }
    catch(const Exception<> &ex){
        emit NotifyError(std::shared_ptr<std::exception>(
                             static_cast<std::exception*>((Exception<> *)ex.Clone()))
                         );
    }
}

void DataGenerator::_gen_dist_normal(GUINT32 N, double mean, double sigma, bool discrete, const QString &fn)
{
    char buf[2*SMALL_BUFFER_SIZE];
    GUINT32 progress_increment = N / 100;
    GUINT32 progress_mem = progress_increment;
    try{
        CREATE_AND_OPEN_OUTPUT_FILE(fn);
        
        GUINT32 i;
        int len;
        for(i = 0; i < N; i+=2){
            if(i >= progress_mem){
                emit ProgressUpdated((double)i/N * 100);
                progress_mem += progress_increment;
            }

            if(m_cancel)
                throw CancelledOperationException<>();

            if(discrete){
                GUtil::Pair<int> X = m_rng.N_Discrete2(mean, sigma);
                len = sprintf(buf, "%d\n", X.First);
                if(i != N-1)
                    len += sprintf(buf + len, "%d\n", X.Second);
            }
            else{
                GUtil::Pair<GFLOAT64> X = m_rng.N2(mean, sigma);
                len = sprintf(buf, "%f\n", X.First);
                if(i != N-1)
                    len += sprintf(buf + len, "%f\n", X.Second);
            }
            outfile.Write(buf, len);
        }
        emit ProgressUpdated(100);
    }
    catch(const Exception<> &ex){
        emit NotifyError(std::shared_ptr<std::exception>(
                             static_cast<std::exception*>((Exception<> *)ex.Clone()))
                         );
    }
}

void DataGenerator::_gen_dist_geometric(GUINT32 N, double E, const QString &fn)
{
    char buf[SMALL_BUFFER_SIZE];
    GUINT32 progress_increment = N / 100;
    try{
        CREATE_AND_OPEN_OUTPUT_FILE(fn);

        for(GUINT32 i = 0; i < N; i++){
            if(0 == i % progress_increment)
                emit ProgressUpdated((double)i/N * 100);

            if(m_cancel)
                throw CancelledOperationException<>();

            const GUINT64 X = m_rng.Geometric(E);
            int len = sprintf(buf, "%llu\n", X);
            outfile.Write(buf, len);
        }
        emit ProgressUpdated(100);
    }
    catch(const Exception<> &ex){
        emit NotifyError(std::shared_ptr<std::exception>(
                             static_cast<std::exception*>((Exception<> *)ex.Clone()))
                         );
    }
}

void DataGenerator::_gen_dist_exponential(GUINT32 N, double lambda, const QString &fn)
{
    char buf[SMALL_BUFFER_SIZE];
    GUINT32 progress_increment = N / 100;
    try{
        CREATE_AND_OPEN_OUTPUT_FILE(fn);

        for(GUINT32 i = 0; i < N; i++){
            if(0 == i % progress_increment)
                emit ProgressUpdated((double)i/N * 100);

            if(m_cancel)
                throw CancelledOperationException<>();

            const double X = m_rng.Exponential(lambda);
            int len = sprintf(buf, "%f\n", X);
            outfile.Write(buf, len);
        }
        emit ProgressUpdated(100);
    }
    catch(const Exception<> &ex){
        emit NotifyError(std::shared_ptr<std::exception>(
                             static_cast<std::exception*>((Exception<> *)ex.Clone()))
                         );
    }
}

void DataGenerator::_gen_dist_poisson(GUINT32 N, double E, const QString &fn)
{
    char buf[SMALL_BUFFER_SIZE];
    GUINT32 progress_increment = N / 100;
    try{
        CREATE_AND_OPEN_OUTPUT_FILE(fn);

        for(GUINT32 i = 0; i < N; i++){
            if(0 == i % progress_increment)
                emit ProgressUpdated((double)i/N * 100);

            if(m_cancel)
                throw CancelledOperationException<>();

            const int X = m_rng.Poisson(E);
            int len = sprintf(buf, "%d\n", X);
            outfile.Write(buf, len);
        }
        emit ProgressUpdated(100);
    }
    catch(const Exception<> &ex){
        emit NotifyError(std::shared_ptr<std::exception>(
                             static_cast<std::exception*>((Exception<> *)ex.Clone()))
                         );
    }
}

void DataGenerator::_generate()
{
    if(!m_worker.isFinished())
        throw Exception<>(tr("Busy processing the last operation...").toUtf8().constData());

    QString fn = ui->le_outFile->text().trimmed();
    if(fn.isEmpty())
        throw Exception<>(tr("You must select an output file").toUtf8().constData());

    m_cancel = false;
    switch(ui->cmb_mode->currentIndex()){
    case mode_raw_data:
    {
        GUINT32 num_bytes = ui->spn_rawData_amount->value();
        switch(ui->cmb_rawData_units->currentIndex()){
        case unit_megabytes:
            num_bytes *= 1000;
        case unit_kilobytes:    // Note: fall-through is intentional
            num_bytes *= 1000;
        default:
            break;
        }

        m_progressDialog = new QProgressDialog(tr("Exporting raw data..."),
                                               tr("Cancel"), 0, 100, this);
        connect(m_progressDialog.data(), SIGNAL(canceled()), this, SLOT(_cancel()));
        connect(this, SIGNAL(ProgressUpdated(int)), m_progressDialog.data(), SLOT(setValue(int)));
        //m_progressDialog->setModal(true);
        m_progressDialog->show();

        m_worker = QtConcurrent::run(this, &DataGenerator::_export_raw_data,
                                     num_bytes, fn);
    }
        break;
    case mode_distribution:
    {
        QString disttype, msg;
        bool discrete = ui->cmb_distributionType->currentIndex() == disttype_discrete;
        const int N = ui->spn_n->value();

        m_progressDialog = new QProgressDialog(this);
        connect(m_progressDialog.data(), SIGNAL(canceled()), this, SLOT(_cancel()));
        connect(this, SIGNAL(ProgressUpdated(int)), m_progressDialog.data(), SLOT(setValue(int)));
        //m_progressDialog->setModal(true);
        m_progressDialog->show();

        switch(ui->cmb_distribution->currentIndex()){
        case dist_uniform:
        {
            const double min = ui->spn_u_min->value();
            const double max = ui->spn_u_max->value();
            if(min >= max)
                throw Exception<>(tr("Invalid range: The minimum must be less than the maximum").toUtf8().constData());

            disttype = QString("%1 uniform").arg(discrete ? "discrete" : "continuous");
            msg = QString(tr("in the range [%1, %2]")).arg(min).arg(max);
            m_worker = QtConcurrent::run(this, &DataGenerator::_gen_dist_uniform,
                                         N, min, max,
                                         discrete,
                                         fn);
        }
            break;
        case dist_normal:
        {
            const double mean = ui->spn_normal_mean->value();
            const double stdev = ui->spn_normal_sigma->value();
            disttype = "normal";
            msg = QString(tr("with a mean of %1 and standard deviation of %2")).arg(mean).arg(stdev);
            m_worker = QtConcurrent::run(this, &DataGenerator::_gen_dist_normal,
                                         N, mean, stdev,
                                         discrete,
                                         fn);
        }
            break;
        case dist_geometric:
        {
            const double E = ui->spn_geometric_e->value();
            disttype = "geometric";
            msg = QString(tr("with an expected value of %1")).arg(E);
            m_worker = QtConcurrent::run(this, &DataGenerator::_gen_dist_geometric,
                                         N, E, fn);
        }
            break;
        case dist_exponential:
        {
            const double lambda = ui->spn_exponential_lambda->value();
            disttype = "exponential";
            msg = QString(tr("with lambda value %1")).arg(lambda);
            m_worker = QtConcurrent::run(this, &DataGenerator::_gen_dist_exponential,
                                         N, lambda, fn);
        }
            break;
        case dist_poisson:
        {
            const double E = ui->spn_poisson_e->value();
            disttype = "poisson";
            msg = QString(tr("with an expected value of %1")).arg(E);
            m_worker = QtConcurrent::run(this, &DataGenerator::_gen_dist_poisson,
                                         N, E, fn);
        }
            break;
        default:
            throw NotImplementedException<>();
        }

        m_progressDialog->setLabelText(
                    QString(tr("Generating %1 values of a %2 distribution %3..."))
                                                                   .arg(N)
                                                                   .arg(disttype)
                                                                   .arg(msg));
    }
        break;
    default:
        break;
    }
}

void DataGenerator::_cancel()
{
    m_cancel = true;
}

void DataGenerator::_handle_progress(int p)
{
    if(100 == p){
        m_progressDialog->close();
        delete m_progressDialog.data();
    }
}

void DataGenerator::_handle_error(const std::shared_ptr<std::exception> &ex)
{
    _handle_progress(100);

    if(NULL != dynamic_cast<CancelledOperationException<>const*>(ex.get())){
        QMessageBox::information(this, tr("Cancelled"), tr("Operation cancelled"));
    }
    else{
        throw ex;
    }
}

void DataGenerator::SaveParameters(GUtil::Qt::Settings *s) const
{
    s->SetValue(SETTING_DATAGEN_MODE, ui->cmb_mode->currentIndex());
    s->SetValue(SETTING_DATAGEN_OUTPUT_FILE, ui->le_outFile->text().trimmed());
    
    // Get the raw data params
    s->SetValue(SETTING_DATAGEN_RAWDATA_AMOUNT, ui->spn_rawData_amount->value());
    s->SetValue(SETTING_DATAGEN_RAWDATA_UNIT, ui->cmb_rawData_units->currentIndex());
    
    // Get the parameters common to all distributions
    s->SetValue(SETTING_DATAGEN_DISTTYPE, ui->cmb_distributionType->currentIndex());
    s->SetValue(SETTING_DATAGEN_DISTRIBUTION, ui->cmb_distribution->currentIndex());
    s->SetValue(SETTING_DATAGEN_NSAMPLES, ui->spn_n->value());
    
    // Get the distribution-specific parameters
    s->SetValue(SETTING_DATAGEN_U_MIN, ui->spn_u_min->value());
    s->SetValue(SETTING_DATAGEN_U_MAX, ui->spn_u_max->value());
    
    s->SetValue(SETTING_DATAGEN_N_MEAN, ui->spn_normal_mean->value());
    s->SetValue(SETTING_DATAGEN_N_STDEV, ui->spn_normal_sigma->value());
    
    s->SetValue(SETTING_DATAGEN_GEO_E, ui->spn_geometric_e->value());
    
    s->SetValue(SETTING_DATAGEN_EXP_LAMBDA, ui->spn_exponential_lambda->value());
    
    s->SetValue(SETTING_DATAGEN_POISSON_E, ui->spn_poisson_e->value());
}

void DataGenerator::RestoreParameters(GUtil::Qt::Settings *s)
{
    if(s->Contains(SETTING_DATAGEN_MODE))
        ui->cmb_mode->setCurrentIndex(s->Value(SETTING_DATAGEN_MODE).toInt());
    if(s->Contains(SETTING_DATAGEN_OUTPUT_FILE))
        ui->le_outFile->setText(s->Value(SETTING_DATAGEN_OUTPUT_FILE).toString());
    
    // Raw data params
    if(s->Contains(SETTING_DATAGEN_RAWDATA_AMOUNT))
        ui->spn_rawData_amount->setValue(s->Value(SETTING_DATAGEN_RAWDATA_AMOUNT).toInt());
    if(s->Contains(SETTING_DATAGEN_RAWDATA_UNIT))
        ui->cmb_rawData_units->setCurrentIndex(s->Value(SETTING_DATAGEN_RAWDATA_UNIT).toInt());
    
    // Common distribution params
    if(s->Contains(SETTING_DATAGEN_DISTTYPE))
        ui->cmb_distributionType->setCurrentIndex(s->Value(SETTING_DATAGEN_DISTTYPE).toInt());
    if(s->Contains(SETTING_DATAGEN_DISTRIBUTION))
        ui->cmb_distribution->setCurrentIndex(s->Value(SETTING_DATAGEN_DISTRIBUTION).toInt());
    if(s->Contains(SETTING_DATAGEN_NSAMPLES))
        ui->spn_n->setValue(s->Value(SETTING_DATAGEN_NSAMPLES).toInt());
    
    // Distribution-specific params
    if(s->Contains(SETTING_DATAGEN_U_MIN))
        ui->spn_u_min->setValue(s->Value(SETTING_DATAGEN_U_MIN).toDouble());
    if(s->Contains(SETTING_DATAGEN_U_MAX))
        ui->spn_u_max->setValue(s->Value(SETTING_DATAGEN_U_MAX).toDouble());
    
    if(s->Contains(SETTING_DATAGEN_N_MEAN))
        ui->spn_normal_mean->setValue(s->Value(SETTING_DATAGEN_N_MEAN).toDouble());
    if(s->Contains(SETTING_DATAGEN_N_STDEV))
        ui->spn_normal_sigma->setValue(s->Value(SETTING_DATAGEN_N_STDEV).toDouble());
    
    if(s->Contains(SETTING_DATAGEN_GEO_E))
        ui->spn_geometric_e->setValue(s->Value(SETTING_DATAGEN_GEO_E).toDouble());
    
    if(s->Contains(SETTING_DATAGEN_EXP_LAMBDA))
        ui->spn_exponential_lambda->setValue(s->Value(SETTING_DATAGEN_EXP_LAMBDA).toDouble());
    
    if(s->Contains(SETTING_DATAGEN_POISSON_E))
        ui->spn_poisson_e->setValue(s->Value(SETTING_DATAGEN_POISSON_E).toDouble());
}
