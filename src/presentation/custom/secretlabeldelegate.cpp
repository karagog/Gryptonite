/*Copyright 2010-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "secretlabeldelegate.h"
#include <QComboBox>
#include <QLineEdit>

NAMESPACE_GRYPTO;

const char *SecretLabelDelegate::DefaultStrings[] =
{
    "Username",
    "Password",
    "URL",
    "E-mail",
    "Card #",
    "Name",
    "Exp.",
    "CVV2",
    "PIN",
    "Combo"
};


SecretLabelDelegate::SecretLabelDelegate(QObject *par)
    :QStyledItemDelegate(par)
{}

QWidget *SecretLabelDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    QComboBox *cb = new QComboBox(parent);
    QStringList items;
    for(uint i = 0; i < sizeof(DefaultStrings)/sizeof(const char *); ++i)
        items.append(DefaultStrings[i]);
    cb->addItems(items);
    cb->setEditable(true);
    return cb;
}

void SecretLabelDelegate::setEditorData(QWidget *editor, const QModelIndex & index ) const
{
    QString tmpstr = index.data(Qt::EditRole).toString();
    if(!tmpstr.isEmpty()){
        ((QComboBox *)editor)->setEditText(tmpstr);
        ((QComboBox *)editor)->lineEdit()->selectAll();
    }
}

void SecretLabelDelegate::setModelData(QWidget *editor,
                                       QAbstractItemModel *model,
                                       const QModelIndex &index ) const
{
    model->setData(index, ((QComboBox *)editor)->currentText(), Qt::EditRole);
}


END_NAMESPACE_GRYPTO;
