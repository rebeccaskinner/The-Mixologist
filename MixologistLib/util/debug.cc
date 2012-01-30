/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#include "interface/iface.h"

#include "util/debug.h"

#include <map>
#include <stdio.h>

#include <QMutex>

/***
 * #define DEBUG_DEBUG
 **/

const int DEBUG_STDERR  = 1;  /* stuff goes to stderr */
const int DEBUG_LOGFILE     = 2;  /* stuff goes to logfile */
const int DEBUG_LOGCRASH    = 3;  /* minimal logfile stored after crashes */
const int DEBUG_LOGC_MAX    = 100000;  /* max length of crashfile log */
const int DEBUG_LOGC_MIN_SAVE = 100;    /* min length of crashfile log */

static std::map<int, int> zoneLevel;
static int defaultLevel = LOG_WARNING;
static FILE *ofd = stderr;

static int debugMode = DEBUG_STDERR;

static QMutex logMtx;

int locked_setDebugFile(const char *fname);
int locked_getZoneLevel(int zone);

#ifdef false
int setDebugCrashMode(const char *cfile) {
    QMutexLocker stack(&logMtx);
    crashfile = cfile;
    /* if the file exists - then we crashed, save it */
    FILE *tmpin = fopen(crashfile.c_str(), "r");
    if (tmpin) {
        /* see how long it is */
        fseek(tmpin, 0, SEEK_END);
        if (ftell(tmpin) > DEBUG_LOGC_MIN_SAVE) {
            std::string crashfile_save = crashfile + "-save";
#ifdef DEBUG_DEBUG
            fprintf(stderr, "Detected Old Log File: %s\n", crashfile.c_str());
            fprintf(stderr, "Copying to: %s\n", crashfile_save.c_str());
#endif
            /* go back to the start */
            fseek(tmpin, 0, SEEK_SET);

            FILE *tmpout = fopen(crashfile_save.c_str(), "w");
            int da_size = 10240;
            char dataarray[da_size]; /* 10k */
            unsigned int da_read = 0;

            if (!tmpout) {
#ifdef DEBUG_DEBUG
                fprintf(stderr, "Failed to open CrashSave\n");
#endif
                fclose(tmpin);
                return -1;
            }
            while (0 != (da_read = fread(dataarray, 1, da_size, tmpin))) {
                if (da_read != fwrite(dataarray, 1, da_read, tmpout)) {
#ifdef DEBUG_DEBUG
                    fprintf(stderr, "Failed writing to CrashSave\n");
#endif
                    fclose(tmpout);
                    fclose(tmpin);
                    return -1;
                }
            }
            fclose(tmpout);
            fclose(tmpin);
        } else {
#ifdef DEBUG_DEBUG
            fprintf(stderr, "Negligable Old CrashLog, ignoring\n");
#endif
            fclose(tmpin);
        }
    }

    if (0 < locked_setDebugFile(crashfile.c_str())) {
#ifdef DEBUG_DEBUG
        fprintf(stderr, "Switching To CrashLog Mode!\n");
#endif
        debugMode = DEBUG_LOGCRASH;
        lineCount = 0;
        debugTS = time(NULL);
    }
    return 1;
}


/* this is called when we exit normally */
int clearDebugCrashLog() {
    QMutexLocker stack(&logMtx);
    /* check we are in crashLog Mode */
    if (debugMode != DEBUG_LOGCRASH) {
#ifdef DEBUG_DEBUG
        fprintf(stderr, "Not in CrashLog Mode - nothing to clear!\n");
#endif
        return 1;
    }

#ifdef DEBUG_DEBUG
    fprintf(stderr, "clearDebugCrashLog() Cleaning up\n");
#endif
    /* shutdown crashLog Mode */
    fclose(ofd);
    ofd = stderr;
    debugMode = DEBUG_STDERR;

    /* just open the file, and then close */
    FILE *tmpin = fopen(crashfile.c_str(), "w");
    fclose(tmpin);

    return 1;
}



int setDebugFile(const char *fname) {
    QMutexLocker stack(&logMtx);
    return locked_setDebugFile(fname);
}
#endif

int locked_setDebugFile(const char *fname) {
    if (NULL != (ofd = fopen(fname, "w"))) {
#ifdef DEBUG_DEBUG
        fprintf(stderr, "Logging redirected to %s\n", fname);
#endif
        debugMode = DEBUG_LOGFILE;
        return 1;
    } else {
        ofd = stderr;
        debugMode = DEBUG_STDERR;
#ifdef DEBUG_DEBUG
        fprintf(stderr, "Logging redirect to %s FAILED\n", fname);
#endif
        return -1;
    }
}


int setOutputLevel(int lvl) {
    QMutexLocker stack(&logMtx);
    return defaultLevel = lvl;
}

int setZoneLevel(int lvl, int zone) {
    QMutexLocker stack(&logMtx);
    zoneLevel[zone] = lvl;
    return zone;
}


int getZoneLevel(int zone) {
    QMutexLocker stack(&logMtx);
    return locked_getZoneLevel(zone);
}

int locked_getZoneLevel(int zone) {
    std::map<int, int>::iterator it = zoneLevel.find(zone);
    if (it == zoneLevel.end()) {
        return defaultLevel;
    }
    return it -> second;
}

int log(unsigned int lvl, int zone, QString msg) {
    int zoneLevel;
    {
        QMutexLocker stack(&logMtx);
        zoneLevel = locked_getZoneLevel(zone);
    }
    if ((signed) lvl <= zoneLevel) {
        notifyBase->notifyLog(msg);
    }
    return 1;
}



