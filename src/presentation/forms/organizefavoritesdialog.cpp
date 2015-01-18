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

#include "organizefavoritesdialog.h"
#include "ui_organizefavoritesdialog.h"
#include <QContextMenuEvent>
#include <QWhatsThis>
NAMESPACE_GRYPTO;


OrganizeFavoritesDialog::OrganizeFavoritesDialog(const QList<Entry> &favorites,
                                                 QWidget *parent)
    :QDialog(parent),
      ui(new Ui::OrganizeFavoritesDialog)
{
    ui->setupUi(this);

    for(const Entry &e : favorites){
        QListWidgetItem *lwi = new QListWidgetItem(e.GetName(), ui->listWidget);
        lwi->setData(Qt::ToolTipRole, e.GetDescription());
        lwi->setData(Qt::UserRole, QVariant::fromValue(e.GetId()));
        ui->listWidget->addItem(lwi);
    }

    m_contextMenu.addAction(QIcon(":/grypto/icons/redX.png"), tr("Remove from List"),
                            this, SLOT(_remove_favorite()));
    m_contextMenu.addSeparator();
    m_contextMenu.addAction(QWhatsThis::createAction(this));
    ui->listWidget->installEventFilter(this);
}

OrganizeFavoritesDialog::~OrganizeFavoritesDialog()
{
    delete ui;
}

bool OrganizeFavoritesDialog::eventFilter(QObject *o, QEvent *ev)
{
    (void)o;
    GASSERT(o == ui->listWidget);
    bool ret = false;
    if(ev->type() == QEvent::ContextMenu){
        QContextMenuEvent *cme = (QContextMenuEvent *)ev;
        m_contextMenu.move(cme->globalPos());
        m_contextMenu.show();
        ret = true;
    }
    else if(ev->type() == QEvent::KeyPress){
        QKeyEvent *ke = (QKeyEvent*)ev;
        if(ke->key() == Qt::Key_Delete){
            _remove_favorite();
        }
    }
    return ret;
}

void OrganizeFavoritesDialog::accept()
{
    for(int i = 0; i < ui->listWidget->count(); ++i)
        m_favorites.append(ui->listWidget->item(i)->data(Qt::UserRole).value<EntryId>());
    QDialog::accept();
}

void OrganizeFavoritesDialog::_remove_favorite()
{
    if(ui->listWidget->currentRow() >= 0)
        delete ui->listWidget->takeItem(ui->listWidget->currentRow());
}


END_NAMESPACE_GRYPTO;
