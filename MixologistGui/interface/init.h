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

/* Initialisation Class (not publicly disclosed to Iface) */
    #include <QString>

    class NotifyBase;
    class Control;

    class Init {
    public:
            //Startup Process, in order of usage
            //Sets default values for variables
            static void	InitNetConfig() ;
            /* Commandline/Directory options */
            static void processCmdLine(int argc, char **argv);
            //Checks for or creates the basic config dir, which in Unix reside at ~/.mixologist
            static void	loadBaseDir() ;
            //Sets up the user directory and all subdirectories
            static void loadUserDir(int librarymixer_id);
            //Initializes SSL for the entire application
            static QString InitEncryption(int librarymixer_id);
            //Creates, initializes and returns a new Control object.
            static Control* createControl(QString name);

            //Returns the basedir, without or without a separator
            static QString getBaseDirectory(bool withDirSeperator);
            //Returns the userdir
            static QString getUserDirectory(bool withDirSeperator);

            /* Key Parameters that must be set before the Mixologist will start up: */

            /* Win/Unix Differences */
            static char dirSeperator;

            /* Directories */
            static QString basedir;
            static QString userdir;

            /* Listening Port */
            //These configuration variables are probably not working at this time, and need to be looked into more
            static bool forceExtPort;
            static bool forceLocalAddr;
            static unsigned short port;
            static char inet[256];
            static bool udpListenerOnly;

            /* Logging */
            static bool haveDebugLevel;
            static int  debugLevel;
            static char logfname[1024];

    };

