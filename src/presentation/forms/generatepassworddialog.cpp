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

#include "generatepassworddialog.h"
#include "ui_generatepassworddialog.h"
#include "grypto_globals.h"
#include <gutil/cryptopp_rng.h>
#include <cmath>
USING_NAMESPACE_GUTIL;

NAMESPACE_GRYPTO;

static const char *lower_case = "abcdefghijklmnopqrstuvwxyz";
static const char *upper_case = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char *numbers = "0123456789";
static const char *default_special_chars = ".,<>/\\!@#$%^&*(){}[];:'\"+-_=";


GeneratePasswordDialog::GeneratePasswordDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GeneratePasswordDialog)
{
    ui->setupUi(this);

    ui->le_specialChars->setText(default_special_chars);

    generate();
}

GeneratePasswordDialog::~GeneratePasswordDialog()
{
    delete ui;
}

void GeneratePasswordDialog::accept()
{
    password = ui->lbl_value->text();
    QDialog::accept();
}

void GeneratePasswordDialog::generate()
{
    GUtil::CryptoPP::RNG rng;
    int n = ui->spn_numChars->value();
    ui->lbl_value->clear();

    // First pick numbers of the right kind:
    //  2/3 alphabet chars, 1/3 nums and special chars
    int n_3 = floor((float)n / 3.0);
    int n_lcase = 0;
    int n_ucase = 0;
    int n_num = 0;
    int n_spec = 0;

    if((ui->chk_specialChars->isChecked() && !ui->le_specialChars->text().isEmpty()) ||
            ui->chk_numbers->isChecked())
    {
        if((ui->chk_specialChars->isChecked() && !ui->le_specialChars->text().isEmpty()) &&
                ui->chk_numbers->isChecked())
        {
            n_num = n_3 >> 1;
            n_spec = n_3 - n_num;
        }
        else if(ui->chk_numbers->isChecked())
            n_num = n_3;
        else
            n_spec = n_3;
    }
    else
    {
        n_3 = n >> 1;
    }

    if(ui->chk_lcase->isChecked() || ui->chk_ucase->isChecked())
    {
        if(ui->chk_lcase->isChecked() && ui->chk_ucase->isChecked())
            n_lcase = n_ucase = n_3;
        else if(ui->chk_lcase->isChecked())
            n_lcase = n_3 << 1;
        else
            n_ucase = n_3 << 1;
    }

    // Distribute the remaining characters randomly
    int diff = n - (n_lcase + n_ucase + n_num + n_spec);
    if(diff > 0)
    {
        QList<int *> srcs;
        if(ui->chk_lcase->isChecked())
            srcs.append(&n_lcase);
        if(ui->chk_ucase->isChecked())
            srcs.append(&n_ucase);
        if(ui->chk_specialChars->isChecked() && !ui->le_specialChars->text().isEmpty())
            srcs.append(&n_spec);
        if(ui->chk_numbers->isChecked())
            srcs.append(&n_num);

        while(diff-- > 0)
            ++(*srcs[rng.U_Discrete(0, srcs.length() - 1)]);
    }


    QString s;
    for(int i = 0; i < n_lcase; ++i)
        s.append(QChar::fromLatin1(lower_case[rng.U_Discrete(0, 25)]));
    for(int i = 0; i < n_ucase; ++i)
        s.append(QChar::fromLatin1(upper_case[rng.U_Discrete(0, 25)]));
    for(int i = 0; i < n_num; ++i)
        s.append(QChar::fromLatin1(numbers[rng.U_Discrete(0, 9)]));
    for(int i = 0; i < n_spec; ++i)
        s.append(ui->le_specialChars->text()[rng.U_Discrete(0, ui->le_specialChars->text().length() - 1)]);

    if(!s.isEmpty())
    {
        // Shuffle the chars into the result string
        QString news;
        while(!s.isEmpty()){
            int idx = rng.U_Discrete(0, s.length() - 1);
            news.append(s[idx]);
            s.remove(idx, 1);
        }
        ui->lbl_value->setText(news);
    }
}


END_NAMESPACE_GRYPTO;
