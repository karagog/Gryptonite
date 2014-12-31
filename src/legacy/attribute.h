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

#ifndef _ATTRIBUTE_H
#define	_ATTRIBUTE_H

#include <QString>
class QXmlStreamReader;
class QXmlStreamWriter;

// A very generic representation of an attribute for a password.

namespace Grypt{ namespace Legacy{ namespace V3{


class Attribute
{
    friend class Password_File;
    friend class File_Entry;
public:
    Attribute();
    Attribute(const Attribute& orig);

    const Attribute& operator = (const Attribute &);
    bool operator == (const Attribute &) const;

    QString getLabel() const;
    void setLabel(const QString &);
    QString getContent() const;
    void setContent(const QString &);

    void readXml(QXmlStreamReader *);
    void writeXml(QXmlStreamWriter *) const;

    bool secret;
    QString notes;

private:
    QString label;       //i.e.  Description, username, password, url, etc...
    QString content;     // the actual stuff to remember
    bool binary;
    bool broken_binary_data_reference;
};


}}}

#endif	/* _ATTRIBUTE_H */

