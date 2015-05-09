/*Copyright 2014-2015 George Karagoulis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#ifndef GRYPTO_XMLCONVERTER_H
#define	GRYPTO_XMLCONVERTER_H

class QByteArray;
class QDomNode;
class QDomElement;
class QDomDocument;

namespace Grypt{
class Entry;


/** A static class that has functions to serialize certain objects to XML.
    If the XML conversion fails you get an XmlException.
*/
class XmlConverter
{
public:

    /** A generic function to convert to a specific type from XML.
        There is no generic implementation; there must be a specialization
        for the type you want.
    */
    template<class T>static T FromXmlString(const QByteArray &);

    /** A generic function to convert to a specific type from XML.
        There is no generic implementation; there must be a specialization
        for the type you want.
    */
    template<class T>static T FromXmlNode(const QDomElement &);

    /** A generic function to serialize the object to XML.
     *  \param everything If this is true, then some extra fields are included in the XML,
     *              which are normally only in the database. This makes it so you can completely
     *              reconstruct an entry from the XML, whereas without it you need those extra
     *              fields which are stored in the database.
     *  \param pretty If it's true the XML will be nicely indented so humans can read it.
     *                  The default is false.
    */
    template<class T>static QByteArray ToXmlString(const T &, bool everything = false, bool pretty = false);

    /** A generic function to serialize the object to XML and append it
     *  to the children of the given node.
     *  \returns The node it just appended.
    */
    template<class T>static QDomNode AppendToXmlNode(const T &, QDomNode &, QDomDocument &, bool everything = false);

};


/** \name Entry conversion functions
    \{
*/
template<>Entry XmlConverter::FromXmlString(const QByteArray &xml);
template<>Entry XmlConverter::FromXmlNode(const QDomElement &);
template<>QByteArray XmlConverter::ToXmlString(const Entry &, bool, bool);
template<>QDomNode XmlConverter::AppendToXmlNode(const Entry &, QDomNode &, QDomDocument &, bool);
/** \} */

}

#endif	/* GRYPTO_XMLCONVERTER_H */

