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

#ifndef _LABEL_DEL_H
#define	_LABEL_DEL_H
#include <grypto/common.h>
#include <QStyledItemDelegate>

namespace Grypt{


class SecretLabelDelegate :
        public QStyledItemDelegate
{
    Q_OBJECT
public:

    static const char *DefaultStrings[10];

    SecretLabelDelegate(QObject *);

    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const;
    void setEditorData(QWidget *, const QModelIndex &) const;
    void setModelData(QWidget *, QAbstractItemModel *, const QModelIndex &) const;

};


}

#endif

