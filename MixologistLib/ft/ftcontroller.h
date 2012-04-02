/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie
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

#ifndef FT_CONTROLLER_HEADER
#define FT_CONTROLLER_HEADER

#include "pqi/pqimonitor.h"
#include "interface/files.h"

#include <QThread>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QDir>

class ftTransferModule;

class ftController;
extern ftController *fileDownloadController;

/* Internal class used to represent a one or more files that are transferred together as a single download.
   For example:
   Each downloaded LibraryMixerItem would have a downloadGroup, regardless of the number of files in it.
   Each downloaded TempShareItem would have a downloadGroup.
   Each single request for any number of files from ftOffLM would be in a single download group. */
struct downloadGroup {
    unsigned int friend_id;
    QString title;

    enum DownloadType {
        DOWNLOAD_NORMAL = 0,
        DOWNLOAD_BORROW = 1,
        DOWNLOAD_RETURN = 2
    };

    downloadGroup() :downloadFinished(false) {}

    /* These fields are "" and 0 for non-borrow downloads.
       For borrows, they are information supplied by the sender we will pass back on transfer completion so he knows which item we just borrowed. */
    DownloadType downloadType;
    unsigned int source_type;
    QString source_id;

    /* These two lists are synchronized, such that filenames[0] contains the final name for files[0] */
    QStringList filenames;
    QList<ftTransferModule*> filesInGroup;

    bool downloadFinished;

    /* The path of the final resting place of the files. */
    QString finalDestination;

    /* Fills in appropriate statistics about this group's current situation. */
    void getStatus(int *waiting_to_download, int *downloading, int *completed, int *total) const;

    /* Starts a transfer on a file that is waiting to download. */
    void startOneTransfer();
};

/*
This class is the master file downloads controller, and its loop steps through the downloads sending requests for
further data, and handles items that have finished download.

On receiving requested ata ftDataDemultiplex calls ftController's handleReceiveData to handle it.

Downloads belong to downloadGroups, which contain information about a batch of file(s) being downloaded.
Each file additionally is represented by an ftTransferModule.
*/
class ftController: public QThread, public pqiMonitor {

public:
    ftController();

    //Thread loop for file downloads
    virtual void run();

    //Called by ftServer to indicate transfers may begin to download
    void activate();

    /***************************************************************/
    /********************** Control Interface **********************/
    /***************************************************************/

    /* Creates a new download of a batch of files. */
    bool downloadFiles(unsigned int friend_id, const QString &title, const QStringList &paths,
                       const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Creates a new download to borrow a batch of files.
       source_type is one of the file_hint constants defined above to indicate the type of share we are borrowing from.
       source_id's meaning changes depending on the source_type, but always uniquely identifies the item. */
    bool borrowFiles(unsigned int friend_id, const QString &title, const QStringList &paths,
                     const QStringList &hashes, const QList<qlonglong> &filesizes,
                     uint32_t source_type, QString source_id);

    /* Creates a new download to get back a batch of files that were previously borrowed.
       Returns false on failure, including if records of this batch of files are missing or indicate it was not lent to this friend.
       source_type is one of the file_hint constants defined above to indicate the type of share we are borrowing from.
       source_id's meaning changes depending on the source_type, but always uniquely identifies the item. */
    bool downloadBorrowedFiles(unsigned int friend_id, uint32_t source_type, QString source_id,
                               const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Cancels all files that have that librarymixer item ID */
    void cancelDownloadGroup(int groupId);

    /* Cancels a file.
       However, if there is more than one set of transfer information, that means it is being
       requested by more than one transfer, and it only removes information on the one marked.
       (i.e. it cancels the download from that transfer, but not the rest.)
       If checkComplete is not false, then after cancelling the file, it will be check that item
       to see if this was the last file that was holding up its completion and it should be completed.*/
    void cancelFile(int groupId, const QString &hash);

    /* Enables pausing and starting of transfers
       TEMPORARILY DISABLED */
    bool controlFile(int orig_item_id, QString hash, uint32_t flags);

    /* Clears out mCompleted */
    void clearCompletedFiles();

    /* Get information on all current downloads. */
    void FileDownloads(QList<downloadGroupInfo> &downloads);

    /* Directory Handling */
    /* Set the directories as specified, saving changes to settings file. */
    bool setDownloadDirectory(QString path);
    bool setPartialsDirectory(QString path);

    /* First attempt to load from saved settings, and use path as a fallback. */
    bool loadOrSetDownloadDirectory(QString path);
    bool loadOrSetPartialsDirectory(QString path);

    /* Return the directories currently being used */
    QString getDownloadDirectory() const;
    QString getPartialsDirectory() const;

    /* Called from ftDataDemultiplex when we receive new data to pass it to the appropriate transferModule. */
    bool handleReceiveData(const std::string &peerId, const QString &hash, uint64_t offset, uint32_t chunksize, void *data);

    /***************************************************************/
    /********************** pqiMonitor Functions********************/
    /***************************************************************/

    //Called by the FriendsConnectivityManager to inform about changes in friends' online statuses
    virtual void statusChange(const std::list<pqipeer> &changeList);

private:
    /* Called from tick() to handle moving files to the appropriate places, notifying the GUI, etc. when an entire downloadGroup completes.
       As it is called from within tick(), it has no mutex protection for itself. */
    void finishGroup(int groupKey);

    /* If no other transfers are using these files, moves the files to the downloadPath, and removes the transferModules from mDownloads.
       If other transfers are using a file, copies the file to the downloadPath, and leaves the transferModule. */
    bool moveFilesToDownloadPath(int groupKey);

    /* Delegates to each of the different lending providers on how to return the files to their original locations.
       If no other transfers are using these files, moves the files, and remove the transferModules from mDownloads.
       If other transfers are using a file, copies the file, and leaves the transferModule. */
    bool moveLentFilesToOriginalPaths(int groupKey);

    /* Does the actual work in creating a new download of a batch of files and saving it to saved transfers.
       If a specificKey other than -1, that key will be assigned to the group or else it will fail if unavailable.
       Setting a specificKey is used for restoring saved transfers on startup (retaining the same key is needed to keep the save in sync).
       Not mutex protected so we can call from inside internal mutexes. */
    virtual bool internalDownloadFiles(unsigned int friend_id, const QString &title, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes,
                                       int specificKey, downloadGroup::DownloadType download_type, unsigned int source_type, const QString &source_id);

    /* Requests the file.
       If a file is already being downloaded, returns it.
       Otherwise, creates a new ftTransferModule and adds it to mDownloads.
       No mutex protection. */
    ftTransferModule* internalRequestFile(unsigned int friend_id, const QString &hash, uint64_t size);

    /* Internal function that does the lifting for both of the public cancel functions.
       No mutex protection because both of the cancel functions are mutex protected. */
    void internalCancelFile(int groupId, ftTransferModule* file);

    /* Returns true if more than one downloadGroup contains the given file.
       This information is useful for finishGroup(), so we know whether to leave the transferModule in mDownloads and the file behind.
       As it is called from within finishGroup(), it has no mutex protection for itself. */
    bool inMultipleDownloadGroups(ftTransferModule* file) const;

    /* Returns true if it is any non-completed downloadGroup. */
    bool inActiveDownloadGroup(ftTransferModule* file) const;

    /** Called by statusChange monitor */
    /* Update each ftTransferModule as to changes in friend connectivity. */
    bool setPeerState(ftTransferModule *tm, unsigned int librarymixer_id,  bool online);

    /* Called by FileDownloads() to fill in info on information from file.
       Not mutex protected, as FileDownloads() is protected. */
    void fileDetails(ftTransferModule* file, downloadFileInfo &info);

    /* Reads in the saved transfers from the save file on disk. After it finishes it sets the mInitialLoadDone variable to true. */
    void loadSavedTransfers();

    /* Adds the group to the save file on disk of current transfers.
       If there is a pre-existing group with that groupKey, it will be removed. */
    void addGroupToSavedTransfers(int groupKey, const downloadGroup &group);

    /* Removes the given group from the save file on disk of current transfers. */
    void removeGroupFromSavedTransfers(int groupKey);

    mutable QMutex ctrlMutex;

    //Starts out false, and becomes true when activate() is called
    bool mFtActive;
    //Starts out false, and becomes true after first time tick() calls
    bool mInitialLoadDone;

    /* mDownloadGroups and mDownloads are the related QMaps that store information on all of our downloads.

       mDownloads is a map from the file hash to the ftTransferModule.
       It enables us to step through all files being downloaded conveniently.
       Normally an ftTransferModule represents a file being downloaded.
       However, mDownloads contains only ftTransferModules for files that are part of groups that have not yet completed.
       Another way of looking at it, is that mDownloads contains all files that either are or will be in the temp directory,
       but not files that are only part of downloadGroups that fully completed and thus have been moved to their final destinations.

       mDownloadGroups is a map from an arbitrary int that is used only to uniquely identify the group to the downloadGroup,
       which represents a group of files that were requested as one unit by the user.
       It enables us to control which files are being downloaded with limits by group, and to store data about the group.
       Within each downloadGroup is a list of the ftTransferModules contains in that downloadGroup.

       This overlap of pointers to the ftTransferModules is a possible source of confusion.
       The life cycle of a ftTransferModule is as follows:
       (1) Created when a new file not already being downloaded is downloaded.
       (2) Added to both mDownloads and the file list in its downloadGroup.
       (2) Upon completion of that file, the ftTransferModule is kept around until the last downloadGroup downloading it fully completes.
       (3) At such time, the actual file on disk is moved out of the temp folder to that last downloadGroup's destinaiton.
       (4) Additionally, the ftTransferModule is removed from mDownloads, but not deleted or removed from its downloadGroups.
           It is not deleted so we can continue to call it to get display information for the GUI.
           However, it must be removed from mDownloads so that if a new transfer begins that includes the same file, it can create
           a new ftTransferModule to download the file with the same hash.
           Therefore, it is actually possible to have more than one ftTransferModule for the same file, as long as only one is active in mDownloads.
       (5) Only when the user calls clearCompleted or cancel, will an ftTransferModule be deleted.
           This is because both of these operations indicate it is no longer needed for display by the GUI. */
    QMap<QString, ftTransferModule*> mDownloads;
    QMap<int, downloadGroup> mDownloadGroups;

    //The path completed files are moved to
    QString mDownloadPath;
    //The path that incoming files are temporarily stored in
    QString mPartialsPath;
};

#endif
