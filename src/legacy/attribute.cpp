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

#include "attribute.h"
#include "default_attribute_info.h"
#include "encryption_utils.h"
#include <gutil/exception.h>
#include <gutil/string.h>
#include <QXmlStreamReader>
#include <QString>
#include <QVariant>
USING_NAMESPACE_GUTIL;
using namespace std;

namespace Grypt{ namespace Legacy{ namespace V3{


Attribute::Attribute()
{
    //label = "No Label";
    //content = "No Content";
    secret = false;
    binary = false;
    broken_binary_data_reference = false;
}

Attribute::Attribute(const Attribute& orig)
{
    label = orig.label;
    content = orig.content;
    secret = orig.secret;
    notes = orig.notes;
    binary = orig.binary;
    broken_binary_data_reference = orig.broken_binary_data_reference;
}

const Attribute& Attribute::operator = (const Attribute & rhs)
{
    label = rhs.label;
    content = rhs.content;
    secret = rhs.secret;
    notes = rhs.notes;
    binary = rhs.binary;
    broken_binary_data_reference = rhs.broken_binary_data_reference;
    return *this;
}

bool Attribute::operator == (const Attribute & rhs) const
{
    if(label != rhs.label)
        return false;
    if(content != rhs.content)
        return false;
    return true;
}


QString Attribute::getLabel() const
{
    return label;
}

void Attribute::setLabel(const QString &Label)
{
    label = Label;
}

QString Attribute::getContent() const
{
    return content;
}

void Attribute::setContent(const QString &Content)
{
    content = Content;
}

void Attribute::readXml(QXmlStreamReader *x)
{
    if(x->name() == "shh")
    {
        secret = true;
    }
    else
    {
        secret = false;
    }

    if(x->attributes().hasAttribute(S_LABEL))
    {
        QString t = x->attributes().value(S_LABEL).toString();
        setLabel(t);
    }

    if(x->attributes().hasAttribute(S_BINARY))
    {
        binary = "1" == x->attributes().value(S_BINARY).toString();
    }

    if(x->attributes().hasAttribute(S_VALUE))
    {
        setContent(x->attributes().value(S_VALUE).toString());
    }

    if(x->attributes().hasAttribute(S_NOTES))
    {
        notes = x->attributes().value(S_NOTES).toString();
    }

    if(x->attributes().hasAttribute(S_BROKEN_ID))
    {
        broken_binary_data_reference =
                "1" == x->attributes().value(S_BROKEN_ID).toString();
    }

    // Read in the end of the attribute element
    int p = x->readNext();
    while(p != QXmlStreamReader::EndElement)
    {
        p = x->readNext();
    }
}

void Attribute::writeXml(QXmlStreamWriter *w) const
{
    if(secret)
        w->writeStartElement("shh");
    else
        w->writeStartElement("attr");

    w->writeAttribute(S_LABEL, label);
    w->writeAttribute(S_VALUE, content);

    if(notes.length() > 0)
        w->writeAttribute(S_NOTES, notes);

    if(binary)
    {
        w->writeAttribute(S_BINARY, "1");

        if(broken_binary_data_reference)
            w->writeAttribute(S_BROKEN_ID, "1");
    }

    w->writeEndElement();
}


}}}
