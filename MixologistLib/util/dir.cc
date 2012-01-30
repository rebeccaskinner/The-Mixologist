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

// Includes for directory creation.
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pqi/pqinotify.h>

#include "util/dir.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <openssl/md5.h>
#include <sstream>
#include <iomanip>

#include <QCryptographicHash>
#include <QFileInfo>

#if defined(WIN32) || defined(__CYGWIN__)
#include "wtypes.h"
#include <winioctl.h>
#else
#include <errno.h>
#endif

#include <QFile>
#include <QDir>

/****
 * #define DIR_DEBUG 1
 ****/

//multiples of 128 compute MD5 more efficiently I hear
#define READ_BUFFER_SIZE 8192

QString DirUtil::removeTopDir(const QString &dir) {
    /* Originally tried just using QFileInfo and getting absolutePath(), but its behavior was not consistently correct. */
    QString normalizedDir = QDir::fromNativeSeparators(dir);
    int finalSlash = normalizedDir.lastIndexOf("/");
    normalizedDir.truncate(finalSlash);
    return normalizedDir;
}

QString DirUtil::findCommonParent(const QStringList &paths) {
    bool countingDone = false;
    int commonCharCount = 0;

    /* We can pick to work with an arbitrary member of paths, because if we are able to find common characters,
       by definition it will be present in all of the members. Therefore, we simply use the first. */
    while(!countingDone) {
        int currentSubDirEnd = paths.first().indexOf(QDir::separator(), commonCharCount);
        if (currentSubDirEnd == -1) {
            countingDone = true;
        } else {
            QString currentSubDir = paths.first().left(currentSubDirEnd + 1);
            for (int i = 0; i < paths.count(); i++) {
                if (!paths[i].startsWith(currentSubDir)) {
                    countingDone = true;
                    break;
                }
            }
            if (!countingDone) commonCharCount = currentSubDir.length();
        }
    }

    return paths.first().left(commonCharCount);
}

QStringList DirUtil::getRelativePaths(const QStringList &paths) {
    QStringList list = paths;

    int toTrim = findCommonParent(paths).count();


    for (int i = 0; i < list.count(); i++) {
        list[i] = list[i].mid(toTrim);
    }

    return list;
}

bool DirUtil::checkCreateDirectory(QString dir) {
    QDir qdir;
    return qdir.mkpath(dir);
}

QString DirUtil::createUniqueDirectory(const QString &path){
    QDir dir(path);
    QFile file(path);
    if (!dir.exists() && !file.exists()){
        dir.mkpath(path);
        return path;
    } else {
        int i = 1;
        QString new_path;
        while(dir.exists() || file.exists()){
            if (i > 10000) return "";
            new_path = path + QString::number(i);
            dir.setPath(new_path);
            file.setFileName(new_path);
            i++;
        }
        dir.mkpath(new_path);
        return new_path;
    }
}

bool DirUtil::allPathsMatch(QStringList sourcePaths, QStringList comparisonPaths) {
    if (sourcePaths.count() != comparisonPaths.count()) return false;

    foreach (QString sourcePath, sourcePaths) {
        bool found = false;
        foreach (QString comparisonPath, comparisonPaths) {
            if (QDir::fromNativeSeparators(sourcePath) == QDir::fromNativeSeparators(comparisonPath)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

bool DirUtil::renameFile(const QString &from, const QString &to) {
    int loops = 0;
    while (QFile::rename(from, to) != true) {
#ifdef WIN32
        Sleep(100); /* milliseconds */
#else
        usleep(100000); /* microseconds */
#endif

        if (loops >= 30) return false;
        loops++;
    }

    return true ;
}

bool DirUtil::moveFile(const QString &source, const QString &dest) {
    QString destination = dest;

    if (QFile::exists(destination)) {
        if (compareFiles(source, destination) == 1) {
            QFile::remove(source);
            return true;
        } else return false;
    }

    // For security, remove dangerous characters
    destination.remove("..");
    destination.remove("~");

    // First make sure that if the dest includes directories, that all directories exist.
    QString path = removeTopDir(destination);
    if (path != "") checkCreateDirectory(path);

    // Try to move via rename before resorting to copying and erasing.
    if (renameFile(source, destination)) {
        return true;
    } else {
        if (QFile::copy(source, destination)) {
            QFile::remove(source);
            return true;
        } else {
            return false;
        }
    }
}

bool DirUtil::copyFile(const QString &source, const QString &dest) {
    QString destination = dest;
    if (QFile::exists(destination)) {
        return (compareFiles(source, destination) == 1);
    }

    // For security, remove dangerous characters
    destination.remove("..");
    destination.remove("~");

    // First make sure that if the dest includes directories, that all directories exist.
    QString path = removeTopDir(destination);
    if (path != "") checkCreateDirectory(path);

    return QFile::copy(source, destination);
}

int DirUtil::compareFiles(const QString &filepath1, const QString &filepath2) {
    QFile file1(filepath1);
    QFile file2(filepath2);
    if (!file1.open(QIODevice::ReadOnly)) return -1;
    if (!file2.open(QIODevice::ReadOnly)) return -1;
    if (file1.size() != file2.size()) return 0;
    QByteArray readArray1;
    QByteArray readArray2;

    readArray1 = file1.read(READ_BUFFER_SIZE);
    readArray2 = file2.read(READ_BUFFER_SIZE);
    while (readArray1.size() != 0 &&
            readArray2.size() != 0) {
        if (readArray1 != readArray2) return 0;
        readArray1 = file1.read(READ_BUFFER_SIZE);
        readArray2 = file2.read(READ_BUFFER_SIZE);
    }
    return 1;
}

int DirUtil::returnFilesToOriginalLocations(const QStringList &currentPaths, const QStringList &currentHashes, const QList<qlonglong> &currentFilesizes, const QList<bool> &moveFile,
                                            const QStringList &originalPaths, const QStringList &originalHashes, const QList<qlonglong> &originalFilesizes) {
    int numberOfItems = currentPaths.count();

    /* Make sure the number of files is still the same. */
    if (numberOfItems != originalPaths.count()) return -1;

    /* Make sure all files are exactly the same still.
       We will duplicate the hash list to avoid modifying the argument, and then go through the current lists one at a time, and removing them from the hashesToCheck list if found.
       The hashesToCheck list is thus ever dwindling, so its indexes don't line up with the original list indexes, therefore we will need to get its index separately. */
    QStringList hashesToCheck(originalHashes);

    for(int currentItemIndex = 0; currentItemIndex < numberOfItems; currentItemIndex++) {
        int checkIndex = hashesToCheck.indexOf(currentHashes[currentItemIndex]);
        if (checkIndex == -1) return -2;
        int originalItemIndex = originalHashes.indexOf(currentHashes[currentItemIndex]);
        if (originalFilesizes[originalItemIndex] != currentFilesizes[currentItemIndex]) return -2;
        hashesToCheck.removeAt(checkIndex);
    }

    /* If we've gotten here, that means the files are exactly the same still, so try to move them back. */
    bool allSuccess = true;
    for(int currentItemIndex = 0; currentItemIndex < numberOfItems; currentItemIndex++) {
        /* In case the order of the lists has somehow changed, we translate the order from the supplied lists to the stored LibraryMixer item lists. */
        int originalItemIndex = originalHashes.indexOf(currentHashes[currentItemIndex]);
        bool success = true;
        if (moveFile[currentItemIndex]) {
            if (!DirUtil::moveFile(currentPaths[currentItemIndex], originalPaths[originalItemIndex])) success = false;
        } else {
            if (!DirUtil::copyFile(currentPaths[currentItemIndex], originalPaths[originalItemIndex])) success = false;
        }
        if (!success) {
            allSuccess = false;
            getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                          "The Mixologist",
                                          "Error while returning file " + originalPaths[originalItemIndex] + " from temporary location " + currentPaths[currentItemIndex]);
        }
    }

    if (allSuccess) return 1;
    else return 0;
 }

bool DirUtil::getFileHash(const QString &filepath, QString &hash) {
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    MD5_CTX *md5_ctx = new MD5_CTX;
    MD5_Init(md5_ctx);

    char data[READ_BUFFER_SIZE];
    int amount_read;

    while ((amount_read = file.read(data, READ_BUFFER_SIZE)) > 0) {
        MD5_Update(md5_ctx, data, amount_read);
    }

    unsigned char md5_buf[MD5_DIGEST_LENGTH];
    MD5_Final(&md5_buf[0], md5_ctx);

    delete md5_ctx;
    file.close();

    QByteArray result((char *)md5_buf, MD5_DIGEST_LENGTH);
    hash = QString(result.toHex());

    return true;
}
