/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#include <ft/ftfilewatcher.h>
#include <server/librarymixer-libraryitem.h>
#include <util/xml.h>
#include <util/dir.h>
#include <QFileInfo>
#include <QDateTime>
#include <time.h>

/**********************************************************************************
 * LibraryMixerLibraryItem
 **********************************************************************************/

LibraryMixerLibraryItem::LibraryMixerLibraryItem(QDomNode &domNode)
    : linkedDomNode(domNode) {}

LibraryMixerLibraryItem::~LibraryMixerLibraryItem() {
    linkedDomNode.parentNode().removeChild(linkedDomNode);
}

QString LibraryMixerLibraryItem::title() const {
    return linkedDomNode.firstChildElement("title").text();
}

bool LibraryMixerLibraryItem::title(QString newTitle) {
    return modifyAttribute("title", newTitle);
}

unsigned int LibraryMixerLibraryItem::id() const {
    return linkedDomNode.firstChildElement("id").text().toInt();
}

bool LibraryMixerLibraryItem::id(unsigned int newId) {
    return modifyAttribute("id", QString::number(newId));
}

LibraryMixerItem::ItemState LibraryMixerLibraryItem::itemState() const {
    return (LibraryMixerItem::ItemState)linkedDomNode.firstChildElement("itemstate").text().toInt();
}

bool LibraryMixerLibraryItem::itemState(LibraryMixerItem::ItemState newState) {
    if (newState != LibraryMixerItem::MATCHED_TO_LENT) {
        modifyAttribute("lentto", "");
    }
    if (newState != LibraryMixerItem::MATCHED_TO_FILE &&
        newState != LibraryMixerItem::MATCH_NOT_FOUND &&
        newState != LibraryMixerItem::MATCHED_TO_LEND &&
        newState != LibraryMixerItem::MATCHED_TO_LENT) {
        clearFiles();
    }
    if (newState != LibraryMixerItem::MATCHED_TO_MESSAGE){
        linkedDomNode.firstChildElement("autoresponse").removeChild(linkedDomNode.firstChildElement("autoresponse").firstChildElement("message"));
    }
    return modifyAttribute("itemstate", QString::number(newState));
}

unsigned int LibraryMixerLibraryItem::lentTo() const {
    return linkedDomNode.firstChildElement("lentto").text().toInt();
}

bool LibraryMixerLibraryItem::lentTo(unsigned int friend_id) {
    if (itemState() != LibraryMixerItem::MATCHED_TO_LEND) return false;

    itemState(LibraryMixerItem::MATCHED_TO_LENT);

    /* Emit fileNoLongerAvailable for all files when they're lent out. */
    QDomElement currentFile = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while(!currentFile.isNull()) {
        if (!currentFile.firstChildElement("hash").isNull())
            emit fileNoLongerAvailable(currentFile.firstChildElement("hash").text(), currentFile.firstChildElement("size").text().toLongLong());
        currentFile = currentFile.nextSiblingElement("file");
    }

    return modifyAttribute("lentto", QString::number(friend_id));
}

QString LibraryMixerLibraryItem::message() const {
    return linkedDomNode.firstChildElement("autoresponse").firstChildElement("message").text();
}

bool LibraryMixerLibraryItem::message(QString newMessage) {
    itemState(LibraryMixerItem::MATCHED_TO_MESSAGE);
    QDomDocument xml = linkedDomNode.ownerDocument();
    if (linkedDomNode.firstChildElement("autoresponse").firstChildElement("message").isNull()) {
        QDomElement messageElement = XmlUtil::createElement(xml, "message", newMessage);
        return (!linkedDomNode.firstChildElement("autoresponse").appendChild(messageElement).isNull());
    } else {
        QDomElement targetElement = linkedDomNode.firstChildElement("autoresponse").firstChildElement("message");
        return XmlUtil::changeText(targetElement, newMessage);
    }
}

QStringList LibraryMixerLibraryItem::paths() const {
    QStringList list;
    QDomElement currentElement = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(QDir::toNativeSeparators(currentElement.firstChildElement("path").text()));
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

QList<qlonglong> LibraryMixerLibraryItem::filesizes() const {
    QList<qlonglong> list;
    QDomElement currentElement = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(currentElement.firstChildElement("size").text().toLongLong());
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

QStringList LibraryMixerLibraryItem::hashes() const {
    QStringList list;
    QDomElement currentElement = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(currentElement.firstChildElement("hash").text());
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

QList<unsigned int> LibraryMixerLibraryItem::modified() const {
    QList<unsigned int> list;
    QDomElement currentElement = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(currentElement.firstChildElement("modified").text().toUInt());
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

int LibraryMixerLibraryItem::fileCount() const {
    return paths().count();
}

bool LibraryMixerLibraryItem::paths(QStringList newPaths) {
    if (itemState() != LibraryMixerItem::MATCHED_TO_FILE &&
        itemState() != LibraryMixerItem::MATCHED_TO_LEND) return false;
    clearFiles();
    for (int i = 0; i < newPaths.size(); i++) {
        QDomDocument xml = linkedDomNode.ownerDocument();
        QDomElement fileElement = XmlUtil::createElement(xml, "file");
        fileElement.appendChild(XmlUtil::createElement(xml, "path", QDir::fromNativeSeparators(newPaths[i])));
        linkedDomNode.firstChildElement("autoresponse").appendChild(fileElement);
    }
    return true;
}

bool LibraryMixerLibraryItem::setFileInfo(const QString &path, qlonglong size, unsigned int modified, const QString &hash) {
    QString normalizedPath = QDir::fromNativeSeparators(path);
    QDomElement currentElement = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while (!currentElement.isNull()){
        if (currentElement.firstChildElement("path").text() == normalizedPath){
            if (currentElement.firstChildElement("hash").isNull()){
                QDomDocument xml = linkedDomNode.ownerDocument();
                currentElement.appendChild(XmlUtil::createElement(xml, "size", QString::number(size)));
                currentElement.appendChild(XmlUtil::createElement(xml, "modified", QString::number(modified)));
                currentElement.appendChild(XmlUtil::createElement(xml, "hash", hash));
            } else {
                emit fileNoLongerAvailable(currentElement.firstChildElement("hash").text(), currentElement.firstChildElement("size").text().toLongLong());
                QDomElement sizeElement = currentElement.firstChildElement("size");
                QDomElement modifiedElement = currentElement.firstChildElement("modified");
                QDomElement hashElement = currentElement.firstChildElement("hash");
                XmlUtil::changeText(sizeElement, QString::number(size));
                XmlUtil::changeText(modifiedElement, QString::number(modified));
                XmlUtil::changeText(hashElement, hash);
            }
            return true;
        }
        currentElement = currentElement.nextSiblingElement("file");
    }
    return false;
}

bool LibraryMixerLibraryItem::fullyHashed() const {
    return !hashes().contains("");
}

bool LibraryMixerLibraryItem::modifyAttribute(QString attribute, QString newValue) {
    QDomElement target = linkedDomNode.firstChildElement(attribute);
    if (newValue.isEmpty()){
        return !linkedDomNode.removeChild(target).isNull();
    } else if (target.isNull()) {
        QDomDocument xml = linkedDomNode.ownerDocument();
        linkedDomNode.appendChild(XmlUtil::createElement(xml, attribute, newValue));
        return true;
    } else return XmlUtil::changeText(target, newValue);
}

void LibraryMixerLibraryItem::clearFiles() {
    QDomElement currentFile = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    while(!currentFile.isNull()) {
        if (!currentFile.firstChildElement("hash").isNull())
            emit fileNoLongerAvailable(currentFile.firstChildElement("hash").text(), currentFile.firstChildElement("size").text().toLongLong());
        linkedDomNode.firstChildElement("autoresponse").removeChild(currentFile);
        currentFile = linkedDomNode.firstChildElement("autoresponse").firstChildElement("file");
    }
    linkedDomNode.removeChild(linkedDomNode.firstChildElement("lentto"));
    sendToOnHashList.clear();
}
