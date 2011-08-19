/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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

#include <interface/librarymixer-library.h>
#include <interface/iface.h> //For the libraryconnect variable
#include <interface/librarymixer-connect.h>
#include <interface/init.h>
#include <interface/peers.h> //For the peers variable
#include <ft/ftitemlist.h>
#include <ft/ftserver.h>
#include <util/dir.h>
#include <pqi/pqinotify.h>
#include <QFile>
#include <QDomDocument>
#include <iostream>

#ifdef false
#define DEBUG_LIBRARY_MIXER_LIBRARY
#endif

#define LIBRARYFILE "library.xml"
//TODO add thread safety

bool LibraryMixerLibraryManager::mergeLibrary(QIODevice &newLibraryList) {
    QFile libraryStore(Init::getUserDirectory(true).append(LIBRARYFILE));
    if (!libraryStore.open(QIODevice::ReadWrite | QIODevice::Text)) return false; //ReadWrite so it creates a file if not present
    QDomDocument storedxml;
    QString errorMsg;
    QDomNode storedRootNode;
    if (!storedxml.setContent(&libraryStore, &errorMsg)) {
        if (errorMsg == "unexpected end of file") {
            storedRootNode = storedxml.createElement("library");
            storedxml.appendChild(storedRootNode);
        } else return false;
    }
    libraryStore.close();
    storedRootNode = storedxml.documentElement();
    if (storedRootNode.isNull()) return false;
    if (storedRootNode.nodeName() != "library" ) return false;

    if (!newLibraryList.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QDomDocument newxml;
    int errorLine, errorColumn;
    if (!newxml.setContent(&newLibraryList, &errorMsg, &errorLine, &errorColumn)) std::cerr << errorMsg.toStdString();
    QDomNode newRootNode = newxml.documentElement();
    if (newRootNode.nodeName() != "library" ) return false;

    QDomNode tempNode;
    QDomNode storedItemNode = storedRootNode.lastChildElement("item");
    QDomNode newItemNode = newRootNode.lastChildElement("item");

    //New additions are guaranteed to come newest first, so it is more efficient to start from the back
    //so that we only have to make one pass.
    //if both newItemNode and storedItemNode are null, we're done
    //if storedItemNode is null that means we've finished traversing it, so add newItemNode, add the local elements, and increment newItemNode
    //if newItemNode == storedItemNode make sure author and title are correct and increment both
    //if newItemNode != storedItemNode then that means something was deleted online, so we should delete it here as well
    while(!(newItemNode.isNull() && storedItemNode.isNull())) {
        if (storedItemNode.isNull()) {
            tempNode = newItemNode.cloneNode();
            addChildElement(storedxml, tempNode, "itemstate", QString::number(ITEM_UNMATCHED));
            addChildElement(storedxml, tempNode, "autoresponse");
            storedRootNode.insertBefore(tempNode, storedItemNode); //this insert before a null child simply inserts in the beginning
            newItemNode = newItemNode.previousSibling();
        } else if (equalItem(storedItemNode, newItemNode)) {
            setChildElementValue(storedItemNode, "author", newItemNode.firstChildElement("author"));
            setChildElementValue(storedItemNode, "title", newItemNode.firstChildElement("title"));
            //Remove this in next version. Updating old config files.
            if (!storedItemNode.firstChildElement("pathstatus").isNull()) {
                storedItemNode.firstChildElement("pathstatus").setTagName("itemstate");
            }
            storedItemNode = storedItemNode.previousSibling();
            newItemNode = newItemNode.previousSibling();
        } else {
            tempNode = storedItemNode;
            storedItemNode = storedItemNode.previousSibling();
            storedRootNode.removeChild(tempNode);
        }
    }
    libraryStore.open(QIODevice::WriteOnly);
    libraryStore.write(storedxml.toByteArray());
    libraryStore.close();

    if (!getUnmatched().empty()) {
        pqiNotify *notify = getPqiNotify();
        if (notify) {
            notify->AddPopupMessage(POPUP_UNMATCHED, "", "Items without automatic responses exist");
        }
    }

    ftserver->setItems();

    return true;
}

std::list<LibraryMixerItem> LibraryMixerLibraryManager::getUnmatched() {
    std::list<LibraryMixerItem> items;
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return items;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (itemNode.firstChildElement("itemstate").text().toInt() == ITEM_UNMATCHED ||
                itemNode.firstChildElement("itemstate").text().toInt() == ITEM_MATCH_NOT_FOUND) {
            items.push_back(itemNodeToLibraryMixerItem(itemNode));
        }
    }
    return items;
}

std::list<LibraryMixerItem> LibraryMixerLibraryManager::getMatched() {
    std::list<LibraryMixerItem> items;
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return items;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (itemNode.firstChildElement("itemstate").text().toInt() != ITEM_UNMATCHED &&
                itemNode.firstChildElement("itemstate").text().toInt() != ITEM_MATCH_NOT_FOUND) {
            items.push_back(itemNodeToLibraryMixerItem(itemNode));
        }
    }
    return items;
}

/*
bool LibraryMixerLibraryManager::setAllUnmatchedChat(){
    std::list<LibraryMixerItem> items;
    QFile source((Init::getUserDirectory(true)).append(LIBRARYFILE));
    if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QDomDocument xml;
    if (!xml.setContent(&source)) return false;
    source.close();

    QDomNode rootNode = xml.documentElement();
    if (rootNode.isNull()) return false;
    if (rootNode.nodeName() != "library" ) return false;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (itemNode.firstChildElement("itemstate").text().toInt() == ITEM_UNMATCHED ||
            itemNode.firstChildElement("itemstate").text().toInt() == ITEM_MATCH_NOT_FOUND){
            while(itemNode.firstChildElement("autoresponse").hasChildNodes()){
                itemNode.firstChildElement("autoresponse").removeChild(itemNode.firstChildElement("autoresponse").firstChildElement());
            }

            setChildElementValue(xml, itemNode, "itemstate", QString::number(ITEM_MATCHED_TO_CHAT));
            //Still needed to eliminate broken links
            ftserver->removeItem(itemNode.firstChildElement("id").text().toInt());
        }
    }
    source.open(QIODevice::WriteOnly);
    source.write(xml.toByteArray());
    source.close();
    return true;
}*/

bool LibraryMixerLibraryManager::setMatchChat(int item_id) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return false;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, item_id)) {
            //Clear existing autoresponse
            while(itemNode.firstChildElement("autoresponse").hasChildNodes()) {
                itemNode.firstChildElement("autoresponse").removeChild(itemNode.firstChildElement("autoresponse").firstChildElement());
            }
            itemNode.removeChild(itemNode.firstChildElement("lentto"));

            setChildElementValue(xml, itemNode, "itemstate", QString::number(ITEM_MATCHED_TO_CHAT));
            ftserver->removeItem(item_id);

            if (!writeXml(xml)) return false;
            notifyBase->notifyLibraryUpdated();

            return true;
        }
    }
    return false;
}

bool LibraryMixerLibraryManager::setMatchFile(int item_id, QStringList paths, ItemState itemstate, int recipient) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return false;

    if (itemstate != ITEM_MATCHED_TO_FILE && itemstate != ITEM_MATCHED_TO_LEND) return false;
    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, item_id)) {
            setChildElementValue(xml, itemNode, "itemstate", QString::number(itemstate));
            if (!paths.empty()) {
                //Clear existing autoresponse
                while(itemNode.firstChildElement("autoresponse").hasChildNodes()) {
                    itemNode.firstChildElement("autoresponse").removeChild(itemNode.firstChildElement("autoresponse").firstChildElement());
                }
                itemNode.removeChild(itemNode.firstChildElement("lentto"));

                //Save in new autoresponse and request hashing.
                for(int i = 0; i < paths.count(); i++) {
                    QDomElement fileElement = xml.createElement("file");
                    addChildElement(xml, fileElement, "path", QDir::fromNativeSeparators(paths[i]));
                    itemNode.firstChildElement("autoresponse").appendChild(fileElement);
                }
                LibraryMixerItem item = itemNodeToLibraryMixerItem(itemNode);
                if (recipient != 0) item.sendToOnHash.append(recipient);
                ftserver->addItem(item);
            }
            if (!writeXml(xml)) return false;

            notifyBase->notifyLibraryUpdated();
            return true;
        }
    }
    return false;
}

bool LibraryMixerLibraryManager::setMatchMessage(int item_id, QString message) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return false;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, item_id)) {
            //Clear existing autoresponse
            while(itemNode.firstChildElement("autoresponse").hasChildNodes()) {
                itemNode.firstChildElement("autoresponse").removeChild(itemNode.firstChildElement("autoresponse").firstChildElement());
            }
            itemNode.removeChild(itemNode.firstChildElement("lentto"));

            setChildElementValue(xml, itemNode, "itemstate", QString::number(ITEM_MATCHED_TO_MESSAGE));
            ftserver->removeItem(item_id);

            QDomElement messageElement = xml.createElement("message");
            QDomText textNode = xml.createTextNode(message);
            messageElement.appendChild(textNode);
            itemNode.firstChildElement("autoresponse").appendChild(messageElement);

            if (!writeXml(xml)) return false;
            notifyBase->notifyLibraryUpdated();
            return true;
        }
    }
    return false;
}

bool LibraryMixerLibraryManager::setLent(int librarymixer_id, int item_id) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return false;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, item_id)) {
            if (itemNode.firstChildElement("itemstate").text().toInt() != ITEM_MATCHED_TO_LEND) {
                return false;
            }
            setChildElementValue(xml, itemNode, "itemstate", QString::number(ITEM_MATCHED_TO_LENT));
            ftserver->removeItem(item_id);
            setChildElementValue(xml, itemNode, "lentto", QString::number(librarymixer_id));

            if (!writeXml(xml)) return false;

            LibraryMixerItem item = itemNodeToLibraryMixerItem(itemNode);
            for (int i = 0; i < item.paths.count(); i++) {
                if (!QFile::remove(item.paths[i]))
                    getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File remove error", "Unable to remove borrowed file " + item.paths[i]);
            }

            if (!writeXml(xml)) return false;
            notifyBase->notifyLibraryUpdated();
            return true;
        }
    }
    return false;
}

void LibraryMixerLibraryManager::completedDownloadLendCheck(int item_id, QStringList paths, QStringList hashes, QList<unsigned long>filesizes, QString createdDirectory) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, item_id)) {
            if (itemNode.firstChildElement("itemstate").text().toInt() != ITEM_MATCHED_TO_LENT) {
                return;
            }

            //Mark as returned
            setChildElementValue(xml, itemNode, "itemstate", QString::number(ITEM_MATCHED_TO_LEND));
            int lentto = itemNode.firstChildElement("lentto").text().toInt();
            itemNode.removeChild(itemNode.firstChildElement("lentto"));
            writeXml(xml); //Even if there is a problem saving the result to xml, keep going to process the return
            notifyBase->notifyLibraryUpdated();

            //Let friend know we got the return
            ftserver->LibraryMixerBorrowReturned(lentto, item_id);

            LibraryMixerItem item = itemNodeToLibraryMixerItem(itemNode);
            //Make sure the number of files is still the same
            if (paths.count() != item.paths.count()) {
                getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                              "The Mixologist",
                                              "The number of files you were returned by " +
                                              peers->getPeerName(lentto) +
                                              " for " +
                                              item.title +
                                              " is not the same as the number of files you lent.\n" +
                                              "All files have been left in your downloads folder.");
                return;
            }
            QStringList hashesToCheck(hashes);
            //Make sure all files are exactly the same still
            for(int i = 0; i < item.hashes.count(); i++) {
                int index = hashesToCheck.indexOf(item.hashes[i]);
                if (index == -1 || filesizes[index] != item.filesizes[i]) {
                    if (hashes.count() > 1)
                        getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                                      "The Mixologist",
                                                      "The files you were returned by " +
                                                      peers->getPeerName(lentto) +
                                                      " for " +
                                                      item.title +
                                                      " have been altered and are no longer exactly the same.\n" +
                                                      "All files have been left in your downloads folder.");
                    else
                        getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                                      "The Mixologist",
                                                      "The file you were returned by " +
                                                      peers->getPeerName(lentto) +
                                                      " for " +
                                                      item.title +
                                                      " has been altered and is no longer exactly the same.\n" +
                                                      "The file has been left in your downloads folder.");
                    return;
                } else {
                    hashesToCheck.removeAt(index);
                    filesizes.removeAt(index);
                }
            }
            //If we've gotten here, that means the files are exactly the same still, so move them back
            for(int i = 0; i < item.paths.count(); i++) {
                int index = hashes.indexOf(item.hashes[i]);
                if (!DirUtil::moveFile(paths[index], item.paths[i])) {
                    getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                                  "The Mixologist",
                                                  "Error while returning file " + item.paths[i] + " from temporary location " + paths[index]);
                }
            }
            QDir created(createdDirectory);
            if (!created.rmdir(createdDirectory)){
                getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                              "The Mixologist",
                                              "Could not remove temporary directory " + createdDirectory);
            }

            ftserver->addItem(item);

            return;
        }
    }
    return;
}

int LibraryMixerLibraryManager::getLibraryMixerItemStatus(int id, bool retry) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return -2;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, id)) {
            if (!itemNode.firstChildElement("itemstate").text().isEmpty()) {
                return itemNode.firstChildElement("itemstate").text().toInt();
            }
        }
    }
    if (retry) {
        //Note that this blocks, ensuring the update completes before we run our second scan
        librarymixerconnect->downloadLibrary(true);
        return getLibraryMixerItemStatus(id, false);
    } else return -1;
}

LibraryMixerItem LibraryMixerLibraryManager::getLibraryMixerItem(int id) {
    LibraryMixerItem item;
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return item;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, id)) {
            return itemNodeToLibraryMixerItem(itemNode);
        }
    }

    return item;
}

LibraryMixerItem *LibraryMixerLibraryManager::recheckItemNode(int id) {
    bool changed;
    LibraryMixerItem *item = ftserver->recheckItem(id, &changed);
    if (changed) updateItemNode(*item);
    return item;
}

//private utility functions
bool LibraryMixerLibraryManager::equalItem(const QDomNode a, const QDomNode b) {
    return !a.firstChildElement("id").isNull() && !b.firstChildElement("id").isNull() &&
           a.firstChildElement("id").text() == b.firstChildElement("id").text();
}

bool LibraryMixerLibraryManager::equalItem(const QDomNode a, const int item_id) {
    return !a.firstChildElement("id").isNull() &&
           a.firstChildElement("id").text().toInt() == item_id;
}

void LibraryMixerLibraryManager::addChildElement(QDomDocument xml, QDomNode &parentNode, const QString elementName, const QString elementText) {
    QDomElement childElement = xml.createElement(elementName);
    if (!elementText.isEmpty()) {
        QDomText textNode = xml.createTextNode(elementText);
        childElement.appendChild(textNode);
    }
    parentNode.appendChild(childElement);
}

void LibraryMixerLibraryManager::setChildElementValue(QDomNode &parentNode, const QString elementName, const QDomElement sourceElement) {
    parentNode.removeChild(parentNode.firstChildElement(elementName));
    parentNode.appendChild(sourceElement.cloneNode());
}

void LibraryMixerLibraryManager::setChildElementValue(QDomDocument xml, QDomNode &parentNode, const QString elementName, const QString elementText) {
    if (elementText.isEmpty()) {
        parentNode.removeChild(parentNode.firstChildElement(elementName));
    } else {
        if (parentNode.firstChildElement(elementName).isNull()) {
            QDomElement childElement = xml.createElement(elementName);
            parentNode.appendChild(childElement);
        }
        QDomText textNode = xml.createTextNode(elementText);
        parentNode.firstChildElement(elementName).removeChild(parentNode.firstChildElement(elementName).firstChild());
        parentNode.firstChildElement(elementName).appendChild(textNode);
    }
}

LibraryMixerItem LibraryMixerLibraryManager::itemNodeToLibraryMixerItem(const QDomNode itemNode) {
    LibraryMixerItem item;
    item.author = itemNode.firstChildElement("author").text();
    item.title = itemNode.firstChildElement("title").text();
    item.id = itemNode.firstChildElement("id").text().toInt();
    bool ok;
    int rawItemState = itemNode.firstChildElement("itemstate").text().toInt(&ok);
    if (ok) item.itemState = (ItemState) rawItemState;
    else item.itemState = ITEM_UNMATCHED;

    if (item.itemState == ITEM_MATCHED_TO_FILE ||
            item.itemState == ITEM_MATCHED_TO_LEND ||
            item.itemState == ITEM_MATCHED_TO_LENT) {
        for(QDomNode fileNode = itemNode.firstChildElement("autoresponse").firstChildElement("file"); !fileNode.isNull(); fileNode = fileNode.nextSibling()) {
            //optional elements
            item.paths.append(QDir::toNativeSeparators(fileNode.firstChildElement("path").text()));
            item.hashes.append(fileNode.firstChildElement("hash").text());
            item.filesizes.append(fileNode.firstChildElement("filesize").text().toInt());
        }
    }
    if (item.itemState == ITEM_MATCHED_TO_MESSAGE) {
        item.message = itemNode.firstChildElement("autoresponse").firstChildElement("message").text();
    }
    if (item.itemState == ITEM_MATCHED_TO_LENT) {
        item.lentTo = itemNode.firstChildElement("lentto").text().toInt();
    }
    return item;
}

bool LibraryMixerLibraryManager::updateItemNode(const LibraryMixerItem librarymixeritem) {
    QDomDocument xml;
    QDomNode rootNode;
    if (!openXml(xml, rootNode)) return false;

    for (QDomNode itemNode = rootNode.firstChildElement("item"); !itemNode.isNull(); itemNode = itemNode.nextSibling()) {
        if (equalItem(itemNode, librarymixeritem.id)) {
            setChildElementValue(xml, itemNode, "itemstate", QString::number(librarymixeritem.itemState));
            //Clear existing autoresponse
            while(itemNode.firstChildElement("autoresponse").hasChildNodes()) {
                itemNode.firstChildElement("autoresponse").removeChild(itemNode.firstChildElement("autoresponse").firstChildElement());
            }
            //Save in new file info
            for(int i = 0; i < librarymixeritem.paths.count(); i++) {
                QDomElement fileElement = xml.createElement("file");
                addChildElement(xml, fileElement, "path", QDir::fromNativeSeparators(librarymixeritem.paths[i]));
                addChildElement(xml, fileElement, "hash", librarymixeritem.hashes[i]);
                addChildElement(xml, fileElement, "filesize", QString::number(librarymixeritem.filesizes[i]));
                itemNode.firstChildElement("autoresponse").appendChild(fileElement);
            }
            if (!writeXml(xml)) return false;
            notifyBase->notifyLibraryUpdated();
            return true;
        }
    }
    return false;
}

bool LibraryMixerLibraryManager::openXml(QDomDocument &xml, QDomNode &rootNode) {
    QFile source((Init::getUserDirectory(true)).append(LIBRARYFILE));
    if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    if (!xml.setContent(&source)) return false;
    source.close();

    rootNode = xml.documentElement();
    if (rootNode.isNull()) return false;
    if (rootNode.nodeName() != "library" ) return false;
    return true;
}

bool LibraryMixerLibraryManager::writeXml(QDomDocument &xml) {
    QFile source((Init::getUserDirectory(true)).append(LIBRARYFILE));
    if (!source.open(QIODevice::WriteOnly)) return false;
    source.write(xml.toByteArray());
    source.close();
    return true;
}
