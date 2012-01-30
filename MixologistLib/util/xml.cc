/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-7, Robert Fernie
 *
 *  This file is part of the Mixologist.
 *
 *  The Mixologist is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  The Mixologist is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the Mixologist; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#include <util/xml.h>
#include <util/debug.h>
#include <interface/init.h>
#include <QFile>
#include <QTextStream>

bool XmlUtil::openXml(const QString &file, QDomDocument &xml, QDomElement &rootNode, const QString &expectedRootName, QIODevice::OpenModeFlag readOrWrite) {
    QString pathToUse = file;
    if (!pathToUse.startsWith(Init::getUserDirectory(true))) pathToUse = Init::getUserDirectory(true).append(pathToUse);
    QFile source(pathToUse);
    return XmlUtil::openXml(source, xml, rootNode, expectedRootName, readOrWrite);
}

bool XmlUtil::openXml(QIODevice &source, QDomDocument &xml, QDomElement &rootNode, const QString &expectedRootName, QIODevice::OpenModeFlag readOrWrite) {
    if (!source.open(readOrWrite | QIODevice::Text)) return false;

    QString errorMsg;
    if (xml.setContent(&source, &errorMsg)) {
        source.close();
        rootNode = xml.documentElement();
        if (rootNode.isNull()) return false;
        if (rootNode.nodeName() != expectedRootName) return false;
        return true;
    } else {
        source.close();
        /* If we have an unexpected end of file, this means we most likely have an empty file.
           In that case, if we have write permission, we start by initializing the xml.
           Note that QIODevice::WriteOnly is a bit of misnomer, and actually matches ReadWrite as well. */
        if (errorMsg == "unexpected end of file" &&
            (readOrWrite & QIODevice::WriteOnly)) {
            rootNode = xml.createElement(expectedRootName);
            xml.appendChild(rootNode);
            return true;
        } else {
            log(LOG_ERROR, XMLUTILZONE, "Error reading xml from input");
            return false;
        }
    }
}

bool XmlUtil::writeXml(const QString &file, QDomDocument &xml) {
    QString pathToUse = file;
    if (!pathToUse.startsWith(Init::getUserDirectory(true))) pathToUse = Init::getUserDirectory(true).append(pathToUse);
    QFile source(pathToUse);
    if (!source.open(QIODevice::WriteOnly)) {
        log(LOG_ERROR, XMLUTILZONE, "Error opening " + file + " for writing");
        return false;
    }
    source.write(xml.toByteArray());
    source.close();
    return true;
}

QDomElement XmlUtil::createElement(QDomDocument &xml, const QString &elementName, const QString &elementText){
    QDomElement childElement = xml.createElement(elementName);
    if (!elementText.isEmpty()) {
        QDomText textNode = xml.createTextNode(elementText);
        childElement.appendChild(textNode);
    }
    return childElement;
}

bool XmlUtil::changeText(QDomNode &targetNode, const QString &newText){
    QDomElement targetElement = targetNode.toElement();
    if (!targetElement.isNull()){
        QDomDocument xml = targetNode.ownerDocument();
        QDomNode newNode = createElement(xml, targetElement.tagName(), newText);
        bool success = !targetElement.parentNode().replaceChild(newNode, targetNode).isNull();
        return success;
    }
    return false;
}
