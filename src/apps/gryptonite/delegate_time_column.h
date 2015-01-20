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

#ifndef DELEGATE_TIME_COLUMN_H
#define DELEGATE_TIME_COLUMN_H

#include <gutil/macros.h>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>

#define GRYPTONITE_DATE_FORMAT_STRING "d MMM yyyy h:m"


/** Draws the correct date/time format in views. */
class TimeColumnDelegate :
        public QStyledItemDelegate
{
public:

    TimeColumnDelegate(QObject *p)
        :QStyledItemDelegate(p),
          _p_Format24Hours(false)
    {}

    PROPERTY(Format24Hours, bool);

protected:
    virtual void paint(QPainter *painter,
                       const QStyleOptionViewItem &option,
                       const QModelIndex &index) const
    {
        QDateTime dt = index.data().toDateTime();
        if(dt.isNull())
            QStyledItemDelegate::paint(painter, option, index);
        else{
            QBrush background_color = option.backgroundBrush;
            bool selected = option.state & QStyle::State_Selected;
            if(selected){
                painter->save();
                painter->setPen(option.palette.highlightedText().color());
                background_color = option.palette.highlight();
            }

            painter->fillRect(option.rect, background_color);

            painter->drawText(option.rect,
                              GetFormat24Hours() ?
                                  dt.toString(GRYPTONITE_DATE_FORMAT_STRING) :
                                  dt.toString(GRYPTONITE_DATE_FORMAT_STRING " ap"),
                              QTextOption((Qt::AlignmentFlag)index.data(Qt::TextAlignmentRole).toInt()));

            if(selected)
                painter->restore();
        }
    }


};


#endif // DELEGATE_TIME_COLUMN_H

