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


#ifndef UTIL_DIRFNS_H
#define UTIL_DIRFNS_H

#include <string>
#include <list>
#include <QString>

namespace DirUtil {

/* Non-destructively removes the last part of a path and returns the result.
   i.e. C:\a\b.txt would return C:\a\ */
QString removeTopDir(const QString &dir);

QString findCommonParent(const QStringList &paths);

/* Returns paths, but with all of the common parent directories trimmed.
   A single file will be left with just the filename.
   A series of files will have at least some files that are just filenames, and others that may contain subdirectories.
   This is useful for when the paths list is to be transmitted to others, in order to sanitize the containing directory structure. */
QStringList getRelativePaths(const QStringList &paths);

/* Returns true if both lists are of identical paths, regardless of order.
   The lists can be passed in with either native or QT paths. */
bool allPathsMatch(const QStringList sourcePaths, const QStringList comparisonPaths);

/* Renames file from to file to. Files should be on the same file system.
   Returns true on success, false on failure */
bool renameFile(const QString &from, const QString &to);

/* Moves a file, first trying to rename, and if that fails, manually moving.
   If dest contains '\', appropriate directories are created.
   Any "~" or ".." are ignored in dest.
   Returns true on success, false on failure.
   Failure to remove the source is not considered a failure. */
bool moveFile(const QString &source, const QString &dest);

/* Copies a file.
   If dest contains '\', appropriate directories are created.
   Any "~" or ".." are ignored in dest.
   Returns true on success, false on failure */
bool copyFile(const QString &source, const QString &dest);

/* Returns 1 if files are same, 0 if they are different, -1 on error */
int compareFiles(const QString &filepath1, const QString &filepath2);

/* Called on the return of borrowed items to return the items to their original locations.
   Returns 1 on success, 0 if at least one file could not be returned,
   -1 if file count has changed, -2 if file contents have changed.
   For each file, attempts to move the file back to its original locations if moveFile is true, otherwise uses copy.
   Will notify GUI via AddSysMessage of any files could not be either moved or copied. */
int returnFilesToOriginalLocations(const QStringList &currentPaths, const QStringList &currentHashes, const QList<qlonglong> &currentFilesizes, const QList<bool> &moveFile,
                                   const QStringList &originalPaths, const QStringList &originalHashes, const QList<qlonglong> &originalFilesizes);

/* Checks if a directory exists and creates it if it does not. Returns true on success,
   false on failure. */
bool checkCreateDirectory(QString dir);

/* Attempts to create directory path, and if that fails due to a pre-existing directory, adds numbers to the end
   starting from 1 until one succeeds.
   Capped at 10,000 in case something other than pre-existing directories is causing the failure, in which case an empty QString is returned.
   Returns the directory name that was created.
   path must not end in a directory seperator. */
QString createUniqueDirectory(const QString &path);

/* Hashes the file and fills in hash. */
bool getFileHash(const QString &filepath, QString &hash);

}
#endif
