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

#include <time.h>
#include <ft/ftfilewatcher.h>
#include <ft/ftserver.h>
#include <interface/notify.h>
#include <util/dir.h>
#include <QFileInfo>
#include <QDateTime>

ftFileWatcher::ftFileWatcher() {
    fileSysWatcher = new QFileSystemWatcher(this);
    connect(fileSysWatcher, SIGNAL(fileChanged(QString)), this, SLOT(watchedFileChanged(QString)), Qt::QueuedConnection);
    connect(fileSysWatcher, SIGNAL(directoryChanged(QString)), this, SIGNAL(directoryChanged(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(hashJobAdded()), this, SLOT(processHashJob()), Qt::QueuedConnection);
}

void ftFileWatcher::addFile(const QString &path, qlonglong existingSize, unsigned int existingModified) {
    if (existingSize == 0 && existingModified == 0) {
        addHashJob(path);
    } else {
        QFileInfo targetFile(path);
        if (targetFile.size() != existingSize || targetFile.lastModified().toTime_t() != existingModified) {
            addHashJob(path);
        }
    }

    QMutexLocker stack(&fileWatcherMutex);
    fileSysWatcher->addPath(path);
}

void ftFileWatcher::addDirectory(const QString &path) {
    QMutexLocker stack(&fileWatcherMutex);
    fileSysWatcher->addPath(path);
}

void ftFileWatcher::addHashJob(const QString &path, bool priority) {
    QMutexLocker stack(&fileWatcherMutex);
    QString normalizedPath = QDir::toNativeSeparators(path);

    if (mToHash.contains(normalizedPath)) return;

    if (priority) mToHash.prepend(normalizedPath);
    else mToHash.append(normalizedPath);

    emit hashJobAdded();
}

void ftFileWatcher::stopWatching(const QString &path) {
    QMutexLocker stack(&fileWatcherMutex);
    QString normalizedPath = QDir::toNativeSeparators(path);

    fileSysWatcher->removePath(normalizedPath);
}

void ftFileWatcher::watchedFileChanged(QString path) {
    QFileInfo file(path);
    if (file.exists()) {
        {
            QMutexLocker stack(&fileSignalMutex);
            QFileInfo targetFile(path);
            emit oldHashInvalidated(QDir::toNativeSeparators(path), targetFile.size(), targetFile.lastModified().toTime_t());
        }
        addHashJob(path);
    } else {
        emit fileRemoved(QDir::toNativeSeparators(path));
    }
}

void ftFileWatcher::processHashJob() {
    /* When we pull an item to hash off the queue, we temporarily leave it on the queue.
       This way, we can release the mutex during the long-running hash job, while still being sure
       that another process can't add the current path back onto the queue while the mutex is open. */
    QString path;
    {
        QMutexLocker stack(&fileWatcherMutex);
        if (mToHash.isEmpty()) return;
        path = mToHash.first();
    }

    performHash(path);

    {
        QMutexLocker stack(&fileWatcherMutex);
        mToHash.removeOne(path);
    }

}

QString ftFileWatcher::performHash(const QString &path) {
    QMutexLocker stack(&fileSignalMutex);
    notifyBase->notifyHashingInfo(path);

    QString hash("");
    bool success = DirUtil::getFileHash(path, hash);

    notifyBase->notifyHashingInfo("");

    /* Send result to receipient*/
    if (success) {
        QFileInfo targetFile(path);
        emit newFileHash(QDir::toNativeSeparators(path), targetFile.size(), targetFile.lastModified().toTime_t(), hash);
    }

    return hash;
}
