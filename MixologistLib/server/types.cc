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
#include <interface/types.h>
#include <util/xml.h>
#include <util/dir.h>
#include <QFileInfo>
#include <QDateTime>
#include <time.h>

/**********************************************************************************
 * FriendLibraryMixerItem
 **********************************************************************************/

FriendLibraryMixerItem::FriendLibraryMixerItem(unsigned int item_id, unsigned int friend_id, const QString &title)
    :storedItemId(item_id), storedFriendId(friend_id), storedTitle(title) {}

QString FriendLibraryMixerItem::title() const {return storedTitle;}

bool FriendLibraryMixerItem::title(QString newTitle) {
    storedTitle = newTitle;
    return true;
}

unsigned int FriendLibraryMixerItem::item_id() const {return storedItemId;}

unsigned int FriendLibraryMixerItem::friend_id() const {return storedFriendId;}

/**********************************************************************************
 * OffLMShareItem
 **********************************************************************************/

OffLMShareItem::OffLMShareItem(QDomElement &domElement, int orderNumber, OffLMShareItem *parentItem)
    : linkedDomElement(domElement), orderNumber(orderNumber), parentItem(parentItem), friend_id(0) {
    if (parentItem) {
        connect(this, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), parentItem, SIGNAL(fileNoLongerAvailable(QString,qulonglong)));
        connect(this, SIGNAL(itemAboutToBeAdded(OffLMShareItem*)), parentItem, SIGNAL(itemAboutToBeAdded(OffLMShareItem*)), Qt::DirectConnection);
        connect(this, SIGNAL(itemAdded()), parentItem, SIGNAL(itemAdded()));
        connect(this, SIGNAL(itemChanged(OffLMShareItem*)), parentItem, SIGNAL(itemChanged(OffLMShareItem*)));
        connect(this, SIGNAL(itemAboutToBeRemoved(OffLMShareItem*)), parentItem, SIGNAL(itemAboutToBeRemoved(OffLMShareItem*)), Qt::DirectConnection);
        connect(this, SIGNAL(itemRemoved()), parentItem, SIGNAL(itemRemoved()));

    }
}

OffLMShareItem::~OffLMShareItem(){
    QHash<int,OffLMShareItem*>::iterator it;
    if (isFile()) emit fileNoLongerAvailable(hash(), size());
    for (it = childItems.begin(); it != childItems.end(); ++it) {
        delete it.value();
    }
}

bool OffLMShareItem::checkPathsOrUpdate() {

    bool unchanged = true;

    /* Check the represented item if it corresponds to a filesystem item (everything but the root item). */
    if (isFile() || isFolder()) {
        QFileInfo target(fullPath());

        /* If it has been removed, remove it from parent unless it is a base share, in which case set it missing, but in either case return. */
        if (target.exists() &&
            ((isFile() && target.isFile()) ||
            (isFolder() && target.isDir()))) {
            if (isShare()) {
                if (shareMethod() == STATE_TO_SEND_MISSING) shareMethod(STATE_TO_SEND);
                else if (shareMethod() == STATE_TO_LEND_MISSING) shareMethod(STATE_TO_LEND);
            }
        } else {
            if (isShare()) {
                if (shareMethod() == STATE_TO_SEND) shareMethod(STATE_TO_SEND_MISSING);
                else if (shareMethod() == STATE_TO_LEND) shareMethod(STATE_TO_LEND_MISSING);
            } else {
                parent()->removeChild(order());
            }
            return false;
        }

        /* If it has changed, remove all file info (we will wait for the fileWatcher to deliver us new info). */
        if (isFile()) {
            if (target.size() != size() || target.lastModified().toTime_t() != modified()) {
                unchanged = false;
                if (isShare()) {
                    linkedDomElement.firstChildElement("shareItem").removeChild(linkedDomElement.firstChildElement("shareItem").firstChildElement("size"));
                    linkedDomElement.firstChildElement("shareItem").removeChild(linkedDomElement.firstChildElement("shareItem").firstChildElement("modified"));
                    linkedDomElement.firstChildElement("shareItem").removeChild(linkedDomElement.firstChildElement("shareItem").firstChildElement("hash"));
                } else {
                    linkedDomElement.removeChild(linkedDomElement.firstChildElement("shareItem").firstChildElement("size"));
                    linkedDomElement.removeChild(linkedDomElement.firstChildElement("shareItem").firstChildElement("modified"));
                    linkedDomElement.removeChild(linkedDomElement.firstChildElement("shareItem").firstChildElement("hash"));
                }
                emit itemChanged(this);
            }
        }

        /* Make sure fileWatcher is watching this. */
        if (isFile()) fileWatcher->addFile(fullPath(), size(), modified());
        else if (isFolder()) fileWatcher->addDirectory(fullPath());
    }

    /* Now check the subitems, if any. */
    if (!checkSubitemsOrUpdate()) unchanged = false;

    return unchanged;
}

bool OffLMShareItem::isRootItem() const {
    return linkedDomElement.nodeName() == "offLM";
}

bool OffLMShareItem::isShare() const {
    //The rootItem is not a share
    return linkedDomElement.nodeName() == "share";
}

bool OffLMShareItem::isFile() const {
    if (isRootItem()) return false;
    /* We cannot use the subitemHoldingElement() convenience method here to avoid dealing with the difference in XML structure
       between shares and subfolders because it relies upon isFile() internally. */
    if (isShare()) return linkedDomElement.firstChildElement("shareItem").firstChildElement("subitems").isNull();
    return linkedDomElement.firstChildElement("subitems").isNull();
}

bool OffLMShareItem::isFolder() const {
    if (isRootItem()) return false;
    return !isFile();
}

int OffLMShareItem::childCount() const {
    //If root
    if (isRootItem()) return linkedDomElement.childNodes().count();
    if (isFile()) return 0;
    //Must be a folder
    return subitemHoldingElement().childNodes().count();
}

OffLMShareItem* OffLMShareItem::child(int order) {
    if (childItems.contains(order)) return childItems[order];

    if (order >= 0 && order < childCount()) {
        QDomElement childNode = subitemHoldingElement().childNodes().item(order).toElement();
        OffLMShareItem *childItem = new OffLMShareItem(childNode, order, this);

        childItem->friendId(friendId());
        childItems[order] = childItem;
        return childItem;
    }
    return NULL;
}

QList<OffLMShareItem*> OffLMShareItem::children() {
    QList<OffLMShareItem*> list;
    for (int i = 0; i < childCount(); i++) {
        list.append(child(i));
    }
    return list;
}

bool OffLMShareItem::removeChild(int order) {
    OffLMShareItem* child = childItems[order];

    if (child) {
        int cachedChildCount = childCount();

        emit itemAboutToBeRemoved(child);

        subitemHoldingElement().removeChild(child->domElement());
        childItems.remove(order);
        delete child;

        /* The cached order number and array position in all subsequent siblings must be reduced by one. */
        for(int i = order; i < cachedChildCount; i++) {
            if (childItems.contains(i)) {
                childItems[i]->order(i - 1);
                childItems[i - 1] = childItems[i];
                childItems.remove(i);
            }
        }

        emit itemRemoved();

        return true;
    }

    return false;
}

OffLMShareItem* OffLMShareItem::parent() {
    return parentItem;
}

int OffLMShareItem::order() const {return orderNumber;}

void OffLMShareItem::order(int newOrder) {orderNumber = newOrder;}

QString OffLMShareItem::path() const {
    if (isShare()) {
        /* There will be no rootPath for friend's items, as they have been sanitized. */
        QString rootPath = linkedDomElement.firstChildElement("rootPath").text();
        if (rootPath.isEmpty()) {
            return linkedDomElement.firstChildElement("shareItem").firstChildElement("path").text();
        } else {
            return QDir::toNativeSeparators(QDir::cleanPath(rootPath +
                                                            QDir::separator() +
                                                            linkedDomElement.firstChildElement("shareItem").firstChildElement("path").text()));
        }
    }
    return QDir::toNativeSeparators(linkedDomElement.firstChildElement("path").text());
}

QString OffLMShareItem::fullPath() const {
    /* cleanPath will fix back-to-back dir separators.
       This can occur if the parent path is at the root of a filesystem, in which case the QT path could be
       something like "/" or "D:/" even though normally QT doesn't have trailing slashes. */
    if (isRootItem()) return "";
    if (isShare()) return path();
    return QDir::toNativeSeparators(QDir::cleanPath(parentItem->fullPath() + QDir::separator() + path()));
}

QDomElement& OffLMShareItem::domElement() {return linkedDomElement;}

void OffLMShareItem::domElement(QDomElement &domElement) {linkedDomElement = domElement;}

OffLMShareItem::shareMethodState OffLMShareItem::shareMethod() {
    if (isShare()) return (OffLMShareItem::shareMethodState)linkedDomElement.firstChildElement("method").text().toInt();
    else return parent()->shareMethod();
}

bool OffLMShareItem::shareMethod(shareMethodState newState) {
    if (!isShare()) return false;
    if (newState == shareMethod()) return true;
    QDomElement oldElement = linkedDomElement.firstChildElement("method");
    bool success = XmlUtil::changeText(oldElement, QString::number(newState));
    emit itemChanged(this);
    return success;
}

void OffLMShareItem::addShare(QString path) {
    if (!isRootItem()) return;

    QFileInfo fullPath(path);

    /* First create the XML (for this item only, not including subitems if it is a folder share). */
    QDomDocument xml = linkedDomElement.ownerDocument();
    QDomElement newShare = XmlUtil::createElement(xml, "share");
    newShare.appendChild(XmlUtil::createElement(xml, "label", findUniqueLabel(fullPath.fileName())));
    newShare.appendChild(XmlUtil::createElement(xml, "rootPath", fullPath.absolutePath()));
    newShare.appendChild(XmlUtil::createElement(xml, "method", QString::number(OffLMShareItem::STATE_TO_SEND)));
    newShare.appendChild(XmlUtil::createElement(xml, "updated", QString::number(time(NULL))));
    newShare.appendChild(createOffLMShareItemXml(xml, fullPath.absolutePath(), fullPath.fileName()));

    emit itemAboutToBeAdded(this);
    linkedDomElement.appendChild(newShare);
    emit itemAdded();

    /* Add the subitems. */
    OffLMShareItem* newItem = child(childCount() - 1);
    newItem->checkSubitemsOrUpdate();
}

QDomElement OffLMShareItem::createOffLMShareItemXml(QDomDocument &xml, const QString &rootPath, const QString &path) {
    QString fullPath(QDir::cleanPath(rootPath + "/" + path));

    QDomElement node = xml.createElement("shareItem");
    QFileInfo file(fullPath);

    node.appendChild(XmlUtil::createElement(xml, "path", file.fileName()));
    if (file.isDir()) {
        node.appendChild(XmlUtil::createElement(xml, "subitems")).toElement();
        fileWatcher->addDirectory(file.canonicalFilePath());
    } else {
        fileWatcher->addFile(file.canonicalFilePath());
    }

    return node;
}

bool OffLMShareItem::checkSubitemsOrUpdate() {
    if (!isRootItem() && !isFolder()) return true;

    bool unchanged = true;

    /* Now make sure all existing subitems remain existent and up-to-date as well. */
    foreach (OffLMShareItem* currentChild, children()) {
        if (!currentChild->checkPathsOrUpdate()) unchanged = false;
    }

    /* Finally, make sure we aren't missing anything new if this is a folder. */
    if (!isRootItem()) {
        QDir targetDir(fullPath());
        QStringList entries = targetDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (entries.count() != childCount()) unchanged = false;

        foreach (OffLMShareItem* currentChild, children()) {
            entries.removeOne(QDir::fromNativeSeparators(currentChild->path()));
        }

        QDomDocument xml = linkedDomElement.ownerDocument();
        foreach (QString entry, entries) {
            emit itemAboutToBeAdded(this);
            subitemHoldingElement().appendChild(createOffLMShareItemXml(xml, fullPath(), entry));
            emit itemAdded();

            /* Add the subitems. */
            OffLMShareItem* newItem = child(childCount() - 1);
            newItem->checkSubitemsOrUpdate();
        }
    }

    return unchanged;
}

QString OffLMShareItem::label() const {
    if (!isShare()) return "";
    return linkedDomElement.firstChildElement("label").text();
}

void OffLMShareItem::label(QString newLabel) {
    if (!isShare()) return;
    if (newLabel == label()) return;
    QDomElement oldElement = linkedDomElement.firstChildElement("label");
    XmlUtil::changeText(oldElement, parent()->findUniqueLabel(newLabel));
    emit itemChanged(this);
}

unsigned int OffLMShareItem::updated() const {
    if (!isShare()) return 0;
    return linkedDomElement.firstChildElement("updated").text().toUInt();
}

unsigned int OffLMShareItem::lent() const {
    if (!isShare()) return 0;
    return linkedDomElement.firstChildElement("lent").text().toInt();
}

void OffLMShareItem::lent(unsigned int friend_id) {
    if (!isShare()) return;
    if (shareMethod() != OffLMShareItem::STATE_TO_LEND) return;

    QDomElement oldElement = linkedDomElement.firstChildElement("lent");
    if (friend_id == 0) linkedDomElement.removeChild(oldElement);
    else if (oldElement.isNull()) {
        QDomDocument xml = linkedDomElement.ownerDocument();
        linkedDomElement.appendChild(XmlUtil::createElement(xml, "lent", QString::number(friend_id)));
    } else XmlUtil::changeText(oldElement, QString::number(friend_id));

    if (friend_id != 0) recursiveDeleteFromDisk();

    emit itemChanged(this);
}

void OffLMShareItem::recursiveDeleteFromDisk() {
    if (isFile()) {
        emit fileNoLongerAvailable(hash(), size());
        fileWatcher->stopWatching(fullPath());
        QFile(fullPath()).remove();
    }
    else if (isFolder()) {
        fileWatcher->stopWatching(fullPath());
        foreach (OffLMShareItem* currentChild, children()) {
            currentChild->recursiveDeleteFromDisk();
        }
        QDir().rmdir(fullPath());
    }
}

QString OffLMShareItem::hash() const {
    if (!isFile()) return "";
    if (isShare()) return linkedDomElement.firstChildElement("shareItem").firstChildElement("hash").text();
    else return linkedDomElement.firstChildElement("hash").text();
}

qlonglong OffLMShareItem::size() const {
    if (!isFile()) return 0;
    if (isShare()) return linkedDomElement.firstChildElement("shareItem").firstChildElement("size").text().toLongLong();
    else return linkedDomElement.firstChildElement("size").text().toLongLong();
}

unsigned int OffLMShareItem::modified() const {
    if (!isFile()) return 0;
    if (isShare()) return linkedDomElement.firstChildElement("shareItem").firstChildElement("modified").text().toUInt();
    else return linkedDomElement.firstChildElement("modified").text().toUInt();
}

void OffLMShareItem::makeShareableWithFriends() {
    if (isShare()) {
        linkedDomElement.removeChild(linkedDomElement.firstChildElement("rootPath"));

        if (lent() != 0) {
            QDomElement lentElement = linkedDomElement.firstChildElement("lent");
            XmlUtil::changeText(lentElement, "1");
        }

        if (shareMethod() == STATE_TO_SEND_MISSING || shareMethod() == STATE_TO_LEND_MISSING) {
            linkedDomElement.removeChild(linkedDomElement.firstChildElement("shareItem"));
        }
    }

    if (isFile()){
        if (hash().isEmpty()) parent()->removeChild(order());
    } else if (isFolder() || isRootItem()) {
        foreach (OffLMShareItem* currentChild, children()) {
            currentChild->makeShareableWithFriends();
        }
        if (!isRootItem() && childCount() == 0) parent()->removeChild(order());
    }
}

bool OffLMShareItem::allFilesHashed() {
    if (isFile()) return !hash().isEmpty();
    else {
        foreach (OffLMShareItem* currentChild, children()) {
            if (!currentChild->allFilesHashed()) return false;
        }
        return true;
    }
}

void OffLMShareItem::setFileInfo(qlonglong newSize, unsigned int newModified, const QString &newHash) {
    if (!isFile()) return;
    if (isShare()) {
        if (linkedDomElement.firstChildElement("shareItem").firstChildElement("hash").isNull()) {
            QDomDocument xml = linkedDomElement.ownerDocument();
            linkedDomElement.firstChildElement("shareItem").appendChild(XmlUtil::createElement(xml, "size", QString::number(newSize)));
            linkedDomElement.firstChildElement("shareItem").appendChild(XmlUtil::createElement(xml, "modified", QString::number(newModified)));
            linkedDomElement.firstChildElement("shareItem").appendChild(XmlUtil::createElement(xml, "hash", newHash));
        } else {
            QDomElement sizeElement = linkedDomElement.firstChildElement("shareItem").firstChildElement("size");
            QDomElement modifiedElement = linkedDomElement.firstChildElement("shareItem").firstChildElement("modified");
            QDomElement hashElement = linkedDomElement.firstChildElement("shareItem").firstChildElement("hash");
            XmlUtil::changeText(sizeElement, QString::number(newSize));
            XmlUtil::changeText(modifiedElement, QString::number(newModified));
            XmlUtil::changeText(hashElement, newHash);
        }
    } else {
        if (linkedDomElement.firstChildElement("hash").isNull()) {
            QDomDocument xml = linkedDomElement.ownerDocument();
            linkedDomElement.appendChild(XmlUtil::createElement(xml, "size", QString::number(newSize)));
            linkedDomElement.appendChild(XmlUtil::createElement(xml, "modified", QString::number(newModified)));
            linkedDomElement.appendChild(XmlUtil::createElement(xml, "hash", newHash));
        } else {
            QDomElement sizeElement = linkedDomElement.firstChildElement("size");
            QDomElement modifiedElement = linkedDomElement.firstChildElement("modified");
            QDomElement hashElement = linkedDomElement.firstChildElement("hash");
            XmlUtil::changeText(sizeElement, QString::number(newSize));
            XmlUtil::changeText(modifiedElement, QString::number(newModified));
            XmlUtil::changeText(hashElement, newHash);
        }
    }
    emit itemChanged(this);
    shareUpdated(newModified);
}

int OffLMShareItem::friendId() const {return friend_id;}

void OffLMShareItem::friendId(int newFriendId) {friend_id = newFriendId;}

void OffLMShareItem::getRecursiveFileInfo(QStringList &paths, QStringList &hashes, QList<qlonglong> &filesizes) {
    if (isFile()) {
        paths.append(fullPath());
        hashes.append(hash());
        filesizes.append(size());
    } else if (isFolder()) {
        foreach (OffLMShareItem* currentChild, children()) {
            currentChild->getRecursiveFileInfo(paths, hashes, filesizes);
        }
    }
}

QString OffLMShareItem::findUniqueLabel(const QString &proposedLabel) {
    if (!isRootItem()) return "";
    QString currentLabel = proposedLabel;
    unsigned int endTag = 0;

    testLoop:
    foreach (OffLMShareItem* currentChild, children()) {
        if (currentChild->label() == currentLabel) {
            endTag++;
            currentLabel = proposedLabel + " (" + QString::number(endTag) + ")";
            goto testLoop;
        }
    }

    return currentLabel;
}

void OffLMShareItem::shareUpdated(unsigned int newModified) {
    if (isRootItem()) return;
    if (isShare()) {
        if (updated() == newModified) return;
        QDomElement oldElement = linkedDomElement.firstChildElement("updated");
        XmlUtil::changeText(oldElement, QString::number(newModified));
        emit itemChanged(this);
    } else {
        parent()->shareUpdated(newModified);
    }
}

QDomElement OffLMShareItem::subitemHoldingElement() const {
    if (isRootItem()) return linkedDomElement;
    else if (isShare()) return linkedDomElement.firstChildElement("shareItem").firstChildElement("subitems");
    else if (isFolder()) return linkedDomElement.firstChildElement("subitems");
    return QDomElement();
}

/**********************************************************************************
 * TempShareItem
 **********************************************************************************/

TempShareItem::TempShareItem(QDomNode &domNode) :linkedDomNode(domNode) {}

TempShareItem::TempShareItem(QDomDocument &xml, const QString &label, const QStringList &paths, unsigned int authorized_friend_id) {
    linkedDomNode = xml.createElement("item");
    xml.documentElement().appendChild(linkedDomNode);

    linkedDomNode.appendChild(XmlUtil::createElement(xml, "label", label));

    QDomElement filesElement = XmlUtil::createElement(xml, "files");
    linkedDomNode.appendChild(filesElement);
    for (int i = 0; i < paths.size(); i++) {
        QDomElement fileElement = XmlUtil::createElement(xml, "file");
        fileElement.appendChild(XmlUtil::createElement(xml, "path", QDir::fromNativeSeparators(paths[i])));
        filesElement.appendChild(fileElement);
    }

    linkedDomNode.appendChild(XmlUtil::createElement(xml, "expiration", "0"));
    expirationSetToTwoWeeks();

    linkedDomNode.appendChild(XmlUtil::createElement(xml, "friend", QString::number(authorized_friend_id)));
}

TempShareItem::~TempShareItem() {
    linkedDomNode.parentNode().removeChild(linkedDomNode);
}

QString TempShareItem::label() const {
    return linkedDomNode.firstChildElement("label").text();
}

QStringList TempShareItem::paths() const {
    QStringList list;
    QDomElement currentElement = linkedDomNode.firstChildElement("files").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(QDir::toNativeSeparators(currentElement.firstChildElement("path").text()));
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

QList<qlonglong> TempShareItem::filesizes() const {
    QList<qlonglong> list;
    QDomElement currentElement = linkedDomNode.firstChildElement("files").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(currentElement.firstChildElement("size").text().toLongLong());
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

QStringList TempShareItem::hashes() const {
    QStringList list;
    QDomElement currentElement = linkedDomNode.firstChildElement("files").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(currentElement.firstChildElement("hash").text());
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

QList<unsigned int> TempShareItem::modified() const {
    QList<unsigned int> list;
    QDomElement currentElement = linkedDomNode.firstChildElement("files").firstChildElement("file");
    while (!currentElement.isNull()){
        list.append(currentElement.firstChildElement("modified").text().toUInt());
        currentElement = currentElement.nextSiblingElement("file");
    }
    return list;
}

int TempShareItem::fileCount() const {
    return paths().count();
}

void TempShareItem::removeFile(int fileIndex) {
    if (fileIndex < 0 || fileIndex > (fileCount() - 1)) return;
    QDomElement currentElement = linkedDomNode.firstChildElement("files").firstChildElement("file");
    while (fileIndex > 0) {
        currentElement = currentElement.nextSiblingElement("file");
        fileIndex--;
    }
    if (!currentElement.firstChildElement("hash").isNull())
        emit fileNoLongerAvailable(currentElement.firstChildElement("hash").text(), currentElement.firstChildElement("size").text().toULongLong());
    linkedDomNode.firstChildElement("files").removeChild(currentElement);
}

bool TempShareItem::setFileInfo(const QString &path, qlonglong size, unsigned int modified, const QString &hash) {
    QString normalizedPath = QDir::fromNativeSeparators(path);
    QDomElement currentElement = linkedDomNode.firstChildElement("files").firstChildElement("file");
    while (!currentElement.isNull()){
        if (currentElement.firstChildElement("path").text() == normalizedPath){
            if (currentElement.firstChildElement("hash").isNull()){
                QDomDocument xml = linkedDomNode.ownerDocument();
                currentElement.appendChild(XmlUtil::createElement(xml, "size", QString::number(size)));
                currentElement.appendChild(XmlUtil::createElement(xml, "modified", QString::number(modified)));
                currentElement.appendChild(XmlUtil::createElement(xml, "hash", hash));
            } else {
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

bool TempShareItem::fullyHashed() const {
    return !hashes().contains("");
}

int TempShareItem::expiration() const {
    return linkedDomNode.firstChildElement("expiration").text().toInt();
}

void TempShareItem::expirationSetToTwoWeeks() {
    QDomElement expNode = linkedDomNode.firstChildElement("expiration");
    /* There will always be an expiration element, so no need to check if we need to create. */
    XmlUtil::changeText(expNode, QString::number(time(NULL) + (2 * 7 * 24 * 60 * 60)));
}

void TempShareItem::expirationDisable() {
    QDomElement expNode = linkedDomNode.firstChildElement("expiration");
    /* There will always be an expiration element, so no need to check if we need to create. */
    XmlUtil::changeText(expNode, "0");
}

unsigned int TempShareItem::friend_id() const {
    return linkedDomNode.firstChildElement("friend").text().toUInt();
}

QString TempShareItem::borrowKey() const {
    return linkedDomNode.firstChildElement("borrowKey").text();
}

void TempShareItem::borrowKey(const QString &newKey) {
    QDomElement borrowNode = linkedDomNode.firstChildElement("borrowKey");
    if (borrowNode.isNull()) {
        QDomDocument xml = linkedDomNode.ownerDocument();
        linkedDomNode.appendChild(XmlUtil::createElement(xml, "borrowKey", newKey));
    }
    else XmlUtil::changeText(borrowNode, newKey);
}
