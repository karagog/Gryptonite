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

#include "coinflipper.h"
#include "ui_coinflipper.h"
#include "coinmodel.h"
#include "about.h"

#define SETTING_COINFLIPPER_N    "cf_n"

CoinFlipper::CoinFlipper(QWidget *parent)
    :QWidget(parent),
      ui(new Ui::coinflipper),
      m_model(new CoinModel(this)),
      m_pd(this)
{
    ui->setupUi(this);
    ui->tbl_results->setModel(m_model);
    connect(m_model, SIGNAL(NotifyProgressUpdated(int)), this, SLOT(_progress_updated(int)));
    _update();
    m_pd.setWindowTitle(GRYPTO_RNG_APP_NAME);
    m_pd.setCancelButton(0);
}

CoinFlipper::~CoinFlipper()
{
    delete ui;
    delete m_model;
}

void CoinFlipper::_progress_updated(int progress)
{
    m_pd.setValue(progress);

    if(progress == 100){
        // This code section executes when the coin model's background worker is finished
        _update();

        // This doesn't really resize it, but it causes a resize event on the vertical axis,
        //  which forces the view to fetch more if it needs to
        int old_height = ui->tbl_results->height();
        ui->tbl_results->resize(ui->tbl_results->width(), ui->tbl_results->height() + 1);
        ui->tbl_results->resize(ui->tbl_results->width(), old_height);
    }
    else if(progress == 0){
        m_pd.show();
    }
}

void CoinFlipper::_update()
{
    uint total = m_model->Heads() + m_model->Tails();
    double head_percent = ((double)m_model->Heads())/total * 100;
    double tail_percent = ((double)m_model->Tails())/total * 100;
    ui->lbl_headCount->setText(QString("%L1").arg(m_model->Heads()));
    ui->lbl_tailCount->setText(QString("%L1").arg(m_model->Tails()));
    ui->lbl_headPercent->setText(QString("%1").arg(total == 0 ? 0 : head_percent, 0, 'g', 6));
    ui->lbl_tailPercent->setText(QString("%1").arg(total == 0 ? 0 : tail_percent, 0, 'g', 6));
}

void CoinFlipper::_flip()
{
    // Show a progress dialog while the worker is busy
    int count = ui->spn_flip->value();
    m_pd.setLabelText(QString(tr("Flipping %L1 coins...")).arg(count));

    // Tell the model to start flipping
    m_model->Flip(count);
}

void CoinFlipper::_clear()
{
    m_model->Clear();
    _update();
}

void CoinFlipper::SaveParameters(GUtil::Qt::Settings *s) const
{
    s->SetValue(SETTING_COINFLIPPER_N, ui->spn_flip->value());
}

void CoinFlipper::RestoreParameters(GUtil::Qt::Settings *s)
{
    if(s->Contains(SETTING_COINFLIPPER_N))
        ui->spn_flip->setValue(s->Value(SETTING_COINFLIPPER_N).toInt());
}
