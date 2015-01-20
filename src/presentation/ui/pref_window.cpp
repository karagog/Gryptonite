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

#include "pref_window.h"
#include "Custom/myStringListModel.h"
#include "GUtil/Utils/widgethelpers.h"
#include "gutil_macros.h"
#include <QCloseEvent>
#include <QStringList>
GUTIL_USING_NAMESPACE(Utils);

USING_GRYPTO_NAMESPACE2(Common, DataObjects);
USING_GRYPTO_NAMESPACE2(GUI, DataObjects);
USING_GRYPTO_NAMESPACE2(GUI, Custom);

pref_window::pref_window(const QList<Entry *> &favs,
                         QWidget *p)
    :QWidget(p, Qt::Dialog | Qt::WindowContextHelpButtonHint),
      cancelled(true)
{
    widget.setupUi(this);
    setWindowModality(Qt::WindowModal);

    sett.SetAutoCommitChanges(false);

    // We do this so that the slider_changed functions are called to update the display
    // (but we don't care about these values)
    widget.delaySlider->setValue(1);
    widget.clipboard_slider->setValue(60);

    {
        myStringListModel *slm(new myStringListModel(favs, this));
        widget.listView->setModel(0);
        widget.listView->setModel(slm);
        connect(slm, SIGNAL(dirty()), this, SLOT(make_dirty()));
    }

    widget.autoLaunch->setChecked(sett.GetAutoLaunchURLs());
    widget.autoload->setChecked(sett.GetOpenLastFile());
    widget.minimize_on_close->setChecked(sett.GetMinimizeOnClose());
    widget.timeFrmt->setChecked(sett.GetFormat24Hours());

    widget.delaySlider->setValue(sett.GetLockoutTimeout() / 5);
    widget.clipboard_slider->setValue(sett.GetClipboardTimeout() / 10);

    widget.tabWidget->setCurrentIndex(0);
}

void pref_window::make_dirty()
{
    MakeDirty();
}

void pref_window::closeEvent(QCloseEvent *ev)
{
    if(!cancelled)
        sett.CommitChanges();

    myStringListModel *ref = (myStringListModel *)widget.listView->model();

    QList<Entry *> newlst;
    if(ref != 0 && IsDirty())
        newlst = ref->refList();
    emit edit_finished(!cancelled, newlst);
}

void pref_window::accept_changes()
{
    cancelled = false;
    close();
}

void pref_window::autoload(int val)
{
    sett.SetOpenLastFile(val == Qt::Checked);
}

void pref_window::autolaunch(int val)
{
    sett.SetAutoLaunchURLs(val == Qt::Checked);
}

void pref_window::minimize_close(int val)
{
    sett.SetMinimizeOnClose(val == Qt::Checked);
}

void pref_window::dateformat(int val)
{
    sett.SetFormat24Hours(val == Qt::Checked);
}

void pref_window::slider_changed(int val)
{
    widget.timeoutDisplay->setText((val == -1) ?
                                   "Disabled" : QVariant(val * 5).toString() + " minutes");

    sett.SetLockoutTimeout(val * 5);
}

void pref_window::clipboard_slider_changed(int val)
{
    int scaled_val(val * 10);

    widget.clip_timeoutDisplay->setText(
                (val == 0) ?
                    "Disabled" : QVariant(scaled_val).toString() + " seconds");
    sett.SetClipboardTimeout(scaled_val);
}

void pref_window::delete_favorite()
{
    myStringListModel *ref((myStringListModel *)widget.listView->model());
    ref->remove_row(widget.listView->currentIndex().row());
}

void pref_window::showEvent(QShowEvent *ev)
{
    ev->accept();
    WidgetHelpers::CenterOverWidget(parentWidget(), this);
}

void pref_window::move_up()
{
    int ind = widget.listView->currentIndex().row();
    if(ind <= 0)
        return;

    myStringListModel *ref = (myStringListModel *)widget.listView->model();
    ref->move(ind, ind - 1);

    widget.listView->setCurrentIndex(ref->index(ind - 1, 0));
}

void pref_window::move_down()
{
    int ind = widget.listView->currentIndex().row();


    myStringListModel *ref = (myStringListModel *)widget.listView->model();

    if(ind <= -1 || ind >= (ref->stringList().count() - 1))
        return;

    ref->move(ind, ind + 1);

    widget.listView->setCurrentIndex(ref->index(ind + 1, 0));
}
