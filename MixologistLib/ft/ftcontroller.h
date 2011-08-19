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

#include "util/threads.h"
#include "pqi/pqimonitor.h"

#include "interface/files.h"

#include <QMap>
#include <QString>
#include <QDir>

class ftFileCreator;
class ftTransferModule;
class ftFileProvider;
class ftSearch;
class ftItemList;
class ftDataDemultiplex;
class ftFileControl;
class ftPendingRequest;

/*
This class is the master file downloads controller, and its loop steps through the downloads sending requests for
further data, and handles items that have finished download
*/
class ftController: public MixThread, public pqiMonitor {

public:
    ftController(ftDataDemultiplex *dm, ftSearch *fs, ftItemList *fil);

    //Thread loop for file downloads
    virtual void run();
    //Called by ftServer to indicate transfers may begin to download
    bool activate();

    /***************************************************************/
    /********************** Control Interface **********************/
    /***************************************************************/

    /* Requests the file.
       If activate() has not yet been called, adds it to the mPendingRequest queue.
       If a file is already being downloaded, adds in another set of librarymixer_names,
       orig_item_ids and fnames to mark that it is being requested by more than one transfer. */
    bool requestFile(QString librarymixer_name, int orig_item_id, QString fname,
                     std::string hash, uint64_t size, uint32_t flags, QList<int> &sourceIds);
    /* Overloaded. Requests a single file for multiple librarymixer transfers. */
    bool requestFile(QStringList librarymixer_names, QList<int> orig_item_ids, QStringList filenames,
                     std::string hash, uint64_t size, uint32_t flags, QList<int> &sourceIds);
    /* Cancels all files that have that librarymixer item ID */
    bool LibraryMixerTransferCancel(int orig_item_id);
    /* Cancels a file.
       However, if there is more than one set of transfer information, that means it is being
       requested by more than one transfer, and it only removes information on the one marked.
       (i.e. it cancels the download from that transfer, but not the rest.)
       If checkComplete is not false, then after cancelling the file, it will be check that item
       to see if this was the last file that was holding up its completion and it should be completed.*/
    bool cancelFile(int orig_item_id, QString filename, std::string hash, bool checkComplete = true);
    /* Enables pausing and starting of transfers
       TEMPORARILY DISABLED */
    bool controlFile(int orig_item_id, std::string hash, uint32_t flags);
    /* Clears out mCompleted */
    void clearCompletedFiles();
    /* Called by a ftTransferModule for a file when it is done.
       Not mutex protected, since this is only called by ftTransferModule.tick() which is called
       from within ftcontroller's own mutex by run. */
    void FlagFileComplete(std::string hash);

    /* Get information on all current downloads. */
    bool FileDownloads(QList<FileInfo> &downloads);

    /* Directory Handling */
    /* Set the directories as specified, saving changes to settings file. */
    bool setDownloadDirectory(QString path);
    bool setPartialsDirectory(QString path);
    /* First attempt to load from saved settings, and use path as a fallback. */
    bool loadOrSetDownloadDirectory(QString path);
    bool loadOrSetPartialsDirectory(QString path);
    /* Return the directories currently being used */
    QString getDownloadDirectory(){return mDownloadPath;}
    QString getPartialsDirectory(){return mPartialsPath;}

    /***************************************************************/
    /********************** pqiMonitor Functions********************/
    /***************************************************************/

    //Called by the p3ConnectMgr to inform about changes in friends' online statuses
    virtual void statusChange(const std::list<pqipeer> &changeList);

private:

    /** RunTime Functions called by the run loop **/
    /* Takes an element of mDownload and frees all the transfer elements.
       No mutex protection, call from within mutex. */
    void finishFile(ftFileControl *fc);
    /* Returns true if all items for that transfer are done, else false.
       Also returns false if no items are found at all with that orig_item_id.
       No mutex protection, call from within mutex. */
    bool checkTransferComplete(int orig_item_id);
    /* For all members of mDownloads that match the given LibraryMixer item id, moves the files
       to final destination and moves the ftFileControl from mDownloads to mCompleted.
       However, if a given ftFileControl has more than one set of identifying information
       (i.e. more than one orig_item_id) then only removes one set of information from the
       ftFileControl and copies that set to mCompleted.
       No mutex protection, call from within mutex. */
    void completeTransfer(int orig_item_id);
    /* Pulls a file off mInitialToLoad and calls FileRequest on it. */
    bool handleAPendingRequest();

    /** Called by statusChange monitor */
    /* Update each ftTransferModule as to changes in friend connectivity. */
    bool setPeerState(ftTransferModule *tm, int librarymixer_id,  bool online);

    /* Returns information in info about file identified by hash.
       Called by FileDownloads.
       Not mutex protected, so only call from within mutex. */
    bool FileDetails(QMap<std::string, ftFileControl>::const_iterator file, FileInfo &info);

    /* Utility function that takes an input string, and encodes it so that it can be used
       as a key in an ini QSettings file.
       In particular, '/' and '\' are encoded to '|'. */
    QString iniEncode(QString input){return input.replace("/", "|").replace("\\", "|");}

    /* pointers to other components */
    ftSearch *mSearch;
    ftDataDemultiplex *mDataplex;
    ftItemList *mItemList;

    MixMutex ctrlMutex;

    /* List to Pause File transfers until Caches are properly loaded */
    //Starts out false, and becomes true when activate() is called
    bool mFtActive;
    //Starts out true, and becomes false when all of mInitialToLoad is called
    bool mFtInitialLoadDone;
    //Items are added to this when FileRequest is called before mFtActive is true.
    std::list<ftPendingRequest> mInitialToLoad;

    //List of files being downloaded
    //Map is by the file hash
    QMap<std::string, ftFileControl> mDownloads;
    //List of files done being downloaded
    //Map is by the file hash
    QMap<std::string, ftFileControl> mCompleted;

    //The path completed files are moved to
    QString mDownloadPath;
    //The path that incoming files are temporarily stored in
    QString mPartialsPath;
};

/*Internal class used to represent files being downloaded to be stored by ftController.
  Each file has one ftFileControl, but may be used by multiple transfers (have multiple names and
  other identifying info) if the same file is found in more than one download simultaneously. */
class ftFileControl {
public:

    enum downloadStatus {
        DOWNLOADING, //Default status
        COMPLETED_CHECK, //Done downloading, check to see if all associated downloads done
        COMPLETED_WAIT, //Done downloading, waiting for associated downloads to finish
        /* The ones above here are for the mDownloads list, below here for mCompleted list */
        COMPLETED_ERROR, //Error when trying to complete on file copy/move operations
        COMPLETED //Done
    };

    ftFileControl() :mTransfer(NULL), mCreator(NULL), mState(DOWNLOADING), mSize(0), mFlags(0) {return;}
    ftFileControl(uint64_t size, std::string hash, uint32_t flags)
        :mState(DOWNLOADING), mHash(hash), mSize(size), mFlags(flags) {return;}

    //Returns the destination in the temp directory while downloading
    QString tempDestination(){
        return files->getPartialsDirectory() + QDir::separator() + mHash.c_str();
    }

    //There can be more than one if two separate downloads are downloading the same file
    //These items are synchronized, in that the first item of each refer to one unified set, the 2nd another, etc.
    QStringList librarymixer_names;
    QStringList filenames;
    QList<int> orig_item_ids;

    ftTransferModule *mTransfer;
    ftFileCreator *mCreator;
    downloadStatus mState;
    std::string mHash; //Also used as the name of the temporary file
    uint64_t mSize;
    uint32_t mFlags;
};

/* Used to store things in ftController's mInitialToLoad list
   Contains all necessary information to make a request
   Unlike ftFileControl, each ftPendingRequest only represents a single file for a single transfer. */

class ftPendingRequest {
public:
    ftPendingRequest(QString _librarymixer_name, int _orig_item_id, QString fname, std::string hash,
                     uint64_t size, uint32_t flags, QList<int> &_sourceIds)
        : librarymixer_name(_librarymixer_name), orig_item_id(_orig_item_id), filename(fname),
          mHash(hash), mSize(size), mFlags(flags), sourceIds(_sourceIds) {return;}

    ftPendingRequest() : mSize(0), mFlags(0) {return;}

    //These items are synchronized, in that the first item of each refer to one unified set, the 2nd another, etc.
    QString librarymixer_name;
    int orig_item_id;
    QString filename;

    std::string mHash;
    uint64_t mSize;
    uint32_t mFlags;
    QList<int> sourceIds;
};

#endif
