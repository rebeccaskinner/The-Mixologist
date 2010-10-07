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

#ifndef LibraryMixerLibraryManager_H
#define LibraryMixerLibraryManager_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QIODevice>
#include <QDomElement>

//Possible modifiers that provide more information about an item's state
enum ItemState {
    ITEM_UNMATCHED = 0, //if it is unmatched
    ITEM_MATCHED_TO_CHAT = 1, //open a chat window on request
    ITEM_MATCHED_TO_FILE = 2, //automatically send a file on request
    ITEM_MATCH_NOT_FOUND = 3, //previously matched to file, but now a broken link
    ITEM_MATCHED_TO_MESSAGE = 4, //automatically send a message on request
    ITEM_MATCHED_TO_LEND = 5, //automatically lend a file on request
    ITEM_MATCHED_TO_LENT = 6 //matched to lend, but currently lent out
};

class LibraryMixerItem;

class LibraryMixerLibraryManager {

public:
    //Called whenever the library list is downloaded from LibraryMixer.
    //Merges an xml list of item to the existing library.xml.  Assumes that they are in the same order,
    //with newest first.
    static bool mergeLibrary(QIODevice &newLibraryList);
    //Returns a list of all item that are not matched to a path, as well as those marked as ITEM_MATCH_NOT_FOUND,
    //except those with empty paths are marked as ITEM_MATCHED_TO_CHAT.
    static std::list<LibraryMixerItem> getUnmatched();
    //Returns a list of all item that are not matched to a path, as well as those marked as ITEM_MATCHED_TO_CHAT,
    //except those with paths marked as ITEM_MATCH_NOT_FOUND.
    static std::list<LibraryMixerItem> getMatched();
    //Sets the library.xml for the given item item, clearing out all pre-existing path info.
    //Sets ItemState ITEM_MATCHED_TO_CHAT.
    static bool setMatchChat(int item_id);
    /*Sets the library.xml for the given item item.
      If paths is not empty, clears out all pre-existing path info and requests that they be hashed by Files.
      If paths is empty, then this can be used to toggle the ItemState.
      Sets ItemState ITEM_MATCHED_TO_FILE or ITEM_MATCHED_TO_LEND based on ItemState.
      If recipient is added with librarymixer id, on completion of hash, a download invitation will be sent.*/
    static bool setMatchFile(int item_id, QStringList paths, ItemState itemState, int recipient = 0);
    //Sets the library.xml for the given item item, clearing out all pre-existing path info, and setting the autorespond message.
    //Sets ItemState ITEM_MATCHED_TO_MESSAGE.
    static bool setMatchMessage(int item_id, QString message);
    /*Sets the given item that is currently ITEM_MATCHED_TO_LEND to ITEM_MATCHED_TO_LENT
      and deletes all files that it matches.*/
    static bool setLent(int librarymixer_id, int item_id);
    /*Checks if the item was a lent out item, and if so marks it as returned.
      Attempts to move the files back to their original locations, or if they do not match, raises a sysmessage.*/
    static void completedDownloadLendCheck(int item_id, QStringList paths, QStringList hashes, QList<unsigned long> filesizes, QString createdDirectory);
    /*Note that there are another set of similarly named functions in files.h
      Someday, librarymixer-library and the ftserver ftextrralist should be merged, but for now
      these functions search all item.*/
    //Finds the LibraryMixerItem that corresponds to the item id, and returns its status.  Returns -2 on error, -1 on not found.
    //Retry if true will update the item if the item is not immediately found and try again once.
    static int getLibraryMixerItemStatus(int id, bool retry=true);
    //Finds the LibraryMixerItem that corresponds to the item id, and returns it.
    //Returns a blank LibraryMixerItem on failure.
    static LibraryMixerItem getLibraryMixerItem(int id);
    //Calls the ftserver to rehash the identified file in order to check it.  If there has been a change, it saves it to xml.
    //Returns a pointer to the LibraryMixerItem from the ftserver, NULL if there is either a hashing error or unable to find the itemNode.
    static LibraryMixerItem *recheckItemNode(int id);
    //Searches library.xml for the node that corresponds to item, clears out its file info and replaces it with that from item and returns true.
    //If unable to find a corresponding node, returns false;
    static bool updateItemNode(const LibraryMixerItem item);

private:
    //returns true if both a and b have item_id and they are equal
    static bool equalItem(const QDomNode a, const QDomNode b);
    static bool equalItem(const QDomNode a, const int item_id);
    //Adds a child element elementName to parentNode in xml.  If elementText is supplied, that will be added, othewrise it will be an empty element.
    static void addChildElement(QDomDocument xml, QDomNode &parentNode, const QString elementName, const QString elementText = QString());
    //Sets the values of the child element elementName of parentNode in xml to a clone of sourceElement.
    static void setChildElementValue(QDomNode &parentNode, const QString elementName, const QDomElement sourceElement);
    //Sets the values of the child element elementName of parentNode in xml to elementText.
    //If elementText is empty, removes that element
    static void setChildElementValue(QDomDocument xml, QDomNode &parentNode, const QString elementName, const QString elementText);
    //Converts a itemNode in the xml to a LibraryMixerItem and returns it.
    static LibraryMixerItem itemNodeToLibraryMixerItem(const QDomNode itemNode);
    //Opens the library.xml file, and populates xml and rootNode.
    static bool openXml(QDomDocument &xml, QDomNode &rootNode);
    //Writes xml to the library.xml file.
    static bool writeXml(QDomDocument &xml);
};

class LibraryMixerItem {
public:
    QString author, title;
    int id; //item_id on the website, or a negative number for temporary item
    /* Describes the state of an item's setup. */
    ItemState itemState;
    //If this has been lent out, this is the librarymixer_id of the friend that borrowed it.
    int lentTo;
    QString message;
    //These three elements are synchronized, in that element X of each refers to the same file
    QStringList paths;
    QList<unsigned long> filesizes;
    QStringList hashes;
    QList<unsigned int> sendToOnHash; //Usually empty, but if set, an invitation to download will be sent to each on completion of hashing.
    //Used to determine if this is an empty item.
    bool empty() {
        //a valid item should never have an empty title
        return title.isEmpty();
    }
    QString name() {
        if (author.isEmpty()) return title;
        else return author + " - " + title;
    }
};

#endif
