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


#ifndef FT_FILE_WATCHER_HEADER
#define FT_FILE_WATCHER_HEADER

#include <QStringList>
#include <QThread>
#include <QMutex>
#include <QFileSystemWatcher>

/*
 * This class handles all file hashing as a support class for the other file classes.
 * In addition, it also watches for changes to files on disk for other classes.
 * This class has its own thread, so that it can handle the long-running file operations for other threads.
 */

class ftFileWatcher;
extern ftFileWatcher *fileWatcher;

class ftFileWatcher: public QObject {
    Q_OBJECT

public:
    ftFileWatcher();

    /* Adds a new file to the watch list.
       If no existingSize and existingModified information is provided (or they are provided both as 0), will queue a request to hash the file.
       If existingSize and existingModified information is provided, will check if file has changed and only queue a request to hash if changed. */
    void addFile(const QString &path, qlonglong existingSize = 0, unsigned int existingModified = 0);

    /* Adds a new directory to the watch list. */
    void addDirectory(const QString &path);

    /* Adds a request that a file be hashed to the queue without adding it to the watch list.
       Path may be supplied with either native or QT directory separators.
       Duplicates of an existing request will be ignored.
       If priority is set to true, will add to the front of the queue. */
    void addHashJob(const QString &path, bool priority = false);

    /* Removes either a file or folder from the watch list.
       NOTE: As of QT 4.7.0 there is a bug that prevents a watched folder from being removed if its files were once watched.
       https://bugreports.qt.nokia.com/browse/QTBUG-10846
       FOr the Mixologist, this basically means we can't unwatch folders. */
    void stopWatching(const QString &path);

signals:
    /* Emitted whenever a file has changed on disk.
       A hash will be simultaneously scheduled so updated information will be sent when available.
       Emitted path will be with native directory separators. */
    void oldHashInvalidated(QString path, qlonglong size, unsigned int modified);

    /* Emitted whenever there is a new hash that has been completed.
       Emitted path will be with native directory separators. */
    void newFileHash(QString path, qlonglong size, unsigned int modified, QString newHash);

    /* Emitted whenever there is a watched file that has been removed.
       Emitted path will be with native directory separators. */
    void fileRemoved(QString path);

    /* Emitted whenever a hash job is added, so that the file watcher thread can wake up and handle a job. */
    void hashJobAdded();

    /* Emitted whenever a watched directory has its contents change, whether by file rename, or addition or removal of file.
       This will not be fired when a contained file's contents are changed, so file contents must be monitored separately. */
    void directoryChanged(QString path);

private slots:
    /* Connected to fileSysWatcher for when a watched file changes.
       Emits oldHashInvalidated and adds a new hash job. */
    void watchedFileChanged(QString path);

    /* Connected to own signal hashJobAdded, so that the thread event loop is activated to process the hash job once for each that is added. */
    void processHashJob();

private:
    /* Hashes the given path, which may be supplied with either native or QT directory separators.
       Updating the GUI as to its progress, and notifies ftserver on successful completion.
       Returns the hash, or an empty string on failure. */
    QString performHash(const QString &path);

    /* mToHash is the list of files to be hashed.
       We need to be able to access this quickly from external threads to add new jobs, so it can never remain locked for long. */
    QStringList mToHash;
    mutable QMutex fileWatcherMutex;

    /* This mutex protects the oldHashInvalidated signal and the newFileHash signal to ensure that while hashing is occurring no oldHashInvalidated signals are sent.
       This is important to prevent the following sequence:
       1) begin hashing file
       2) file changes on disk, signal oldHashInvalidated
       3) finish hashing file, signal newFileHash
       This would leave the final state invalid, as the hash that was meant to be invalidated was newFileHash.
       This mutex can remain locked for a long time, as hashes can take ages, which is why we need multiple mutexes. */
    mutable QMutex fileSignalMutex;

    QFileSystemWatcher* fileSysWatcher;
};
#endif
