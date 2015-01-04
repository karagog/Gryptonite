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

#include "xmlconverter.h"
#include "grypto_entry.h"
#include <gutil/databaseutils.h>
#include <QDomDocument>
USING_NAMESPACE_GUTIL1(Qt);
USING_NAMESPACE_GUTIL;

#define ENTRY_XML_NAME "entry"
#define VALUE_XML_NAME "value"

NAMESPACE_GRYPTO;


template<>QByteArray XmlConverter::ToXmlString(const Entry &e, bool everything, bool pretty)
{
    QDomDocument xdoc;
    AppendToXmlNode(e, xdoc, xdoc, everything);
    return xdoc.toByteArray(pretty ? 1 : -1);
}

template<>QDomNode XmlConverter::AppendToXmlNode(const Entry &e, QDomNode &root, QDomDocument &xdoc, bool everything)
{
    QDomNode ret;
    QDomElement entry_root = xdoc.createElement(ENTRY_XML_NAME);
    ret = root.appendChild(entry_root);

    entry_root.setAttribute("name", e.GetName().toUtf8().toBase64().constData());
    entry_root.setAttribute("desc", e.GetDescription().toUtf8().toBase64().constData());
    entry_root.setAttribute("modified", DatabaseUtils::ConvertDateToString(e.GetModifyDate()));
    if(!e.GetFileId().IsNull()){
        entry_root.setAttribute("file_name", e.GetFileName().toUtf8().toBase64().constData());
        if(everything)
            entry_root.setAttribute("file_id", e.GetFileId().ToQByteArray().toBase64().constData());
    }

    foreach(const SecretValue &sv, e.Values())
    {
        QDomElement val_elt = xdoc.createElement(VALUE_XML_NAME);
        entry_root.appendChild(val_elt);
        val_elt.setAttribute("name", sv.GetName().toUtf8().toBase64().constData());
        if(!sv.GetNotes().isEmpty())
            val_elt.setAttribute("note", sv.GetNotes().toUtf8().toBase64().constData());
        if(sv.GetIsHidden())
            val_elt.setAttribute("hide", 1);
        val_elt.setAttribute("value", sv.GetValue().toBase64().constData());
    }
    return ret;
}

template<>Entry XmlConverter::FromXmlNode(const QDomElement &elt)
{
    if(elt.isNull())
        throw XmlException<>("Invalid entry XML");

    Entry ret;
    bool ok = true;
    ret.SetName(QByteArray::fromBase64(elt.attribute("name").toUtf8()));
    ret.SetDescription(QByteArray::fromBase64(elt.attribute("desc").toUtf8()));
    ret.SetModifyDate(DatabaseUtils::ConvertStringToDate(elt.attribute("modified")));
    if(elt.attributes().contains("file_name"))
        ret.SetFileName(QByteArray::fromBase64(elt.attribute("file_name").toUtf8()));
    if(elt.attributes().contains("file_id"))
        ret.SetFileId(QByteArray::fromBase64(elt.attribute("file_id").toUtf8()));

    QDomNodeList nl = elt.childNodes();
    for(int i = 0; i < nl.count(); ++i)
    {
        SecretValue v;
        QDomElement c_elt = nl.at(i).toElement();
        if(c_elt.isNull() || c_elt.tagName() != VALUE_XML_NAME)
            continue;

        v.SetName(QByteArray::fromBase64(c_elt.attribute("name").toUtf8()));
        QString s = c_elt.attribute("note");
        if(!s.isEmpty())
            v.SetNotes(QByteArray::fromBase64(s.toUtf8()));

        s = c_elt.attribute("hide");
        if(!s.isEmpty())
            v.SetIsHidden(s.toInt(&ok) != 0);

        s = c_elt.attribute("value");
        if(!s.isEmpty())
            v.SetValue(QByteArray::fromBase64(s.toUtf8()));

        ret.Values().append(v);
    }
    return ret;
}

template<>Entry XmlConverter::FromXmlString(const QByteArray &xml)
{
    QDomDocument xdoc;
    QString err_msg;
    int err_line = -1, err_col = -1;
    if(!xdoc.setContent(xml, &err_msg, &err_line, &err_col)){
        throw XmlException<>(QString("Error on line %1 column %2: %3")
                             .arg(err_line)
                             .arg(err_col)
                             .arg(err_msg).toUtf8());
    }
    return FromXmlNode<Entry>(xdoc.documentElement());
}


END_NAMESPACE_GRYPTO;
