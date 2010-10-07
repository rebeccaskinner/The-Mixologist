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

QString DirUtil::getTopDir(const QString dir) {
    QString top("");

    /* find the subdir: [/][dir1.../]<top>[/]
     */
    int i,j;
    int len = dir.length();
    for (j = len - 1; (j > 0) && (dir[j] == '/'); j--) {}
    for (i = j; (i > 0) && (dir[i] != '/'); i--) {}

    if (dir[i] == '/')
        i++;

    for (; i <= j; i++) {
        top += dir[i];
    }

    return top;
}

QString DirUtil::removeTopDir(const QString dir) {
    QString rest("");

    /* remove the subdir: [/][dir1.../]<top>[/]
     */
    int i,j;
    int len = dir.length();
    for (j = len - 1; (j > 0) && (dir[j] == '/'); j--) {}
    for (i = j; (i >= 0) && (dir[i] != '/'); i--) {}

    /* remove any more slashes */
    for (; (i >= 0) && (dir[i] == '/'); i--) {}

    for (j = 0; j <= i; j++) {
        rest += dir[j];
    }

    return rest;
}

bool    DirUtil::checkCreateDirectory(QString dir) {
    QDir qdir;
    return qdir.mkpath(dir);
}

QString DirUtil::createUniqueDirectory(const QString &path){
    QDir dir(path);
    QFile file(path);
    if (!dir.exists() && !file.exists()){
        dir.mkpath(path);
        return path + QDir::separator();
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
        return new_path.append(QDir::separator());
    }
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
    if (QFile::exists(dest)) {
        return (compareFiles(source, dest) == 1);
    }

    // First make sure that if the dest includes directories, that all directories exist.
    QString path = removeTopDir(dest);
    if (path != "") checkCreateDirectory(path);

    return QFile::copy(source, dest);
}

bool DirUtil::createEmptyFile(const QString &dest){
    QFile emptyFile(dest);
    if (!emptyFile.open(QIODevice::WriteOnly)) return false;
    emptyFile.close();
    return true;
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

bool DirUtil::getFileHash(QString filepath,
                          std::string &hash, uint64_t &size) {
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    size = file.size();

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
    hash = QString(result.toHex()).toStdString();

    return true;
}
