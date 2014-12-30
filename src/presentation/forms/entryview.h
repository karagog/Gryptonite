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

#ifndef ENTRYVIEW_H
#define ENTRYVIEW_H

#include <grypto_entry.h>
#include <QWidget>

namespace Ui {
class EntryView;
}

namespace Grypt{
class EntryModel;


class EntryView : public QWidget
{
    Q_OBJECT
public:
    explicit EntryView(QWidget *parent = 0);
    ~EntryView();

    void SetEntry(const Entry &);
    const Entry &GetEntry() const{ return m_entry; }

signals:

    /** This signal indicates that the row was activated (double-clicked or enter pressed). */
    void RowActivated(int row);

    /** This signal indicates that the user wants to export the file. */
    void ExportFileRequested();


protected:

    virtual bool eventFilter(QObject *, QEvent *);


private:
    Ui::EntryView *ui;
    Entry m_entry;

    EntryModel *_get_entry_model() const;
};


}

#endif // ENTRYVIEW_H
