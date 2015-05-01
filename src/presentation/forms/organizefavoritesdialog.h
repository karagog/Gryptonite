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

#ifndef ORGANIZEFAVORITESDIALOG_H
#define ORGANIZEFAVORITESDIALOG_H

#include <grypto/entry.h>
#include <QDialog>
#include <QMenu>

namespace Ui {
class OrganizeFavoritesDialog;
}

namespace Grypt{


class OrganizeFavoritesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OrganizeFavoritesDialog(const QList<Grypt::Entry> &favorites,
                                     QWidget *parent = 0);
    ~OrganizeFavoritesDialog();

    /** Returns the ordered set of favorites after the user accepts the dialog. */
    QList<EntryId> GetFavorites() const{ return m_favorites; }

    virtual void accept();

protected:
    virtual bool eventFilter(QObject *, QEvent *);

private slots:
    void _remove_favorite();

private:
    Ui::OrganizeFavoritesDialog *ui;
    QList<EntryId> m_favorites;
    QMenu m_contextMenu;
};


}

#endif // ORGANIZEFAVORITESDIALOG_H
