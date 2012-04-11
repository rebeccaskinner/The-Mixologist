/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-2006, Robert Fernie.
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

#ifndef LIBRARYMIXER_LIBRARY_ITEM
#define LIBRARYMIXER_LIBRARY_ITEM

#include <list>
#include <iostream>
#include <string>
#include <stdint.h>
#include <QStringList>
#include <QLinkedList>
#include <QHash>
#include <QDir>
#include <QDomNode>

/* Needed for the ItemState enum, which is shared with that. */
#include <interface/types.h>

/**********************************************************************************
 * LibraryMixerLibraryItem: An internal class used by LibraryMixerLibraryManager.
 * Collectively, the LibraryMixerLibraryItems are a wrapper to an underlying XML document that is the master database.
 * The management of the underlying XML is handled by the LibraryMixerLibraryManager.
 * Each LibraryMixerLibraryItem represents a single item in a user's LibraryMixer library,
 * which can potentially be matched to a file, a message, or opening a chat box.
 **********************************************************************************/

/* XML format.
Example:
<library>
 <item>
  <title>An unmatched item on LibraryMixer that has an ID of 1</title>
  <id>1</id>
  <itemstate>0</itemstate>
  <autoresponse/>
 </item>
 <item>
  <title>An item on LibraryMixer that has an ID of 2 and an author name matched to 2 files for lend</title>
  <id>2</id>
  <itemstate>5</itemstate>
  <autoresponse>
   <file>
    <path>C:/example1.txt</path>
    <size>9602</size>
    <modified>1120622632</modified>
    <hash>87194ae1fdf016f4d9bbb1abf7dbce3a</hash>
   </file>
   <file>
    <path>C:/example2.txt</path>
    <size>0</size>
    <modified>1320561241</modified>
    <hash>d41d8cd98f00b204e9800998ecf8427e</hash>
   </file>
   <file>
    <path>C:/NotYetHashedFile.txt</path>
   </file>
  </autoresponse>
 </item>
</library>
*/

class LibraryMixerLibraryItem: public QObject {
Q_OBJECT

public:
    LibraryMixerLibraryItem(QDomNode &domNode);
    /* Deleting a LibraryMixerLibraryItem will also remove its domNode from the xml tree. */
    ~LibraryMixerLibraryItem();

    /* General attribute accessors. The set methods will return true on success. */
    QString title() const;
    bool title(QString newTitle);

    unsigned int id() const; //item_id on the website
    bool id(unsigned int newId);

    /* If the set state is changed to a state where the old state data is no longer relevant,
       the old state data will be cleared out. */
    LibraryMixerItem::ItemState itemState() const;
    bool itemState(LibraryMixerItem::ItemState newState);

    /* If this has been lent out, this is the friend_id of the friend that borrowed it.
       The setter will fail if called when the itemState is not MATCHED_TO_LEND.
       The setter will change the itemState to MATCHED_TO_LENT using itemState(). */
    unsigned int lentTo() const;
    bool lentTo(unsigned int friend_id);

    /* The message for if the ItemState is MATCHED_TO_MESSAGE.
       Setting a value here will also change the itemState to MATCHED_TO_MESSAGE using itemState(). */
    QString message() const;
    bool message(QString newMessage);

    /* The files for if the ItemState is MATCHED_TO_FILE, MATCHED_TO_LEND, or MATCHED_TO_LENT.
       These lists are synchronized, in that element X of each refers to the same file.
       Will include an empty string or 0 in the lists for files that haven't had their file data set yet.
       Paths are returned with native directory separators. */
    QStringList paths() const;
    QList<qlonglong> filesizes() const;
    QStringList hashes() const;
    QList<unsigned int> modified() const;

    /* Returns the number of files that are contained in this item. 0 if none. */
    int fileCount() const;

    /* Sets a new set of paths for files. Can accept either native or QT directory separators.
       This actually sets not only paths, but reads and sets filesizes and modified as well.
       Will fail if ItemState is not either MATCHED_TO_FILE or MATCHED_TO_LEND. */
    bool paths(QStringList newPaths);

    /* Sets the file info for the file identified by path. Can accept either native or QT directory separators.
       Returns false is no such path exists. */
    bool setFileInfo(const QString &path, qlonglong size, unsigned int modified, const QString &hash);

    /* Returns true if this item's all have a hash inserted. */
    bool fullyHashed() const;

    /* Used by LibraryMixerLibraryManager to store friends' LibraryMixer IDs to send to on hash complete. */
    QList<unsigned int> sendToOnHashList;

signals:
    void fileNoLongerAvailable(QString hash, qulonglong size);

private:
    /* Sets the XML data for the specified base-level attribute to newValue.
       If newValue is an empty string, removes attribute. */
    bool modifyAttribute(QString attribute, QString newValue);

    /* Clears any XML data related to an existing file auto response, including lentto and sendToOnHashList. */
    void clearFiles();

    /* The QDomNode that contains all of the substantive contents. */
    QDomNode linkedDomNode;
};


#endif //LIBRARYMIXER_LIBRARY_ITEM
