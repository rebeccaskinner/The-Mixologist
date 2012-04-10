#include "ft/ftserver.h"
#include "ft/ftcontroller.h"
#include "ft/ftofflmlist.h"
#include "ft/fttemplist.h"
#include "ft/ftfilewatcher.h"
#include "ft/ftborrower.h"

#include "pqi/authmgr.h"
#include "pqi/friendsConnectivityManager.h"
#include "pqi/ownConnectivityManager.h"
#include "pqi/pqinetwork.h"
#include "pqi/pqinotify.h"
#include "pqi/pqiloopback.h"
#include "pqi/pqissllistener.h"
#include "pqi/aggregatedConnections.h"

#include "server/librarymixer-library.h"
#include "server/librarymixer-friendlibrary.h"
#include "server/server.h"
#include "server/p3peers.h"
#include "server/p3msgs.h"

#include "services/statusservice.h"
#include "services/p3chatservice.h"
#include "services/mixologyservice.h"
#include "services/p3chatservice.h"

#include "interface/init.h"
#include "interface/peers.h" //for peers variable

#include "util/debug.h"
#include "util/dir.h"

#include <list>
#include <string>
#include <sstream>
#include <fstream>
#include <QFile>
#include <QTextStream>

#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#ifdef WINDOWS_SYS
#include <shlobj.h>
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0 //not really sure why, but this isn't defined by our shlobj.h
#endif
#endif

// initial configuration bootstrapping...

QString Init::basedir;
QString Init::userdir;

//Definition for the extern variables
Files *files = NULL;
ftServer *ftserver = NULL;
ftController *fileDownloadController = NULL;
Control *control = NULL;
FriendsConnectivityManager *friendsConnectivityManager = NULL;
OwnConnectivityManager *ownConnectivityManager = NULL;
UdpSorter *udpMainSocket = NULL;
AuthMgr *authMgr = NULL;
Notify *notify = NULL;
Msgs *msgs = NULL;
Peers *peers = NULL;
LibraryMixerLibraryManager *librarymixermanager = NULL;
LibraryMixerFriendLibrary *libraryMixerFriendLibrary = NULL;
ftFileWatcher *fileWatcher = NULL;
ftBorrowManager *borrowManager = NULL;
ftOffLMList *offLMList = NULL;
ftTempList *tempList = NULL;
MixologyService *mixologyService = NULL;
StatusService *statusService = NULL;
AggregatedConnectionsToFriends *aggregatedConnectionsToFriends = NULL;
pqissllistener *sslListener = NULL;

void Init::InitNetConfig() {
    /* Setup logging */
    setOutputLevel(LOG_WARNING);

    /* Setup more detailed logging for desired zones.
       For Testing purposes, can set any individual section to have greater logfile output. */
    //setZoneLevel(LOG_DEBUG_ALERT, TCP_STREAM_ZONE);
}

/******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS
void Init::processCmdLine(int argc, char **argv) {
#else
void Init::processCmdLine(int argcIgnored, char **argvIgnored) {

    /* THIS IS A HACK TO ALLOW WINDOWS TO ACCEPT COMMANDLINE ARGUMENTS */

    const int MAX_ARGS = 32;
    int i,j;

    int argc;
    char *argv[MAX_ARGS];
    char *wholeline = (char *)GetCommandLine();
    int cmdlen = strlen(wholeline);
    // duplicate line, so we can put in spaces..
    char dupline[cmdlen+1];
    strcpy(dupline, wholeline);

    /* break wholeline down ....
     * NB. This is very simplistic, and will not
     * handle multiple spaces, or quotations etc, only for debugging purposes
     */
    argv[0] = dupline;
    for (i = 1, j = 0; (j + 1 < cmdlen) && (i < MAX_ARGS);) {
        /* find next space. */
        for (; (j + 1 < cmdlen) && (dupline[j] != ' '); j++);
        if (j + 1 < cmdlen) {
            dupline[j] = '\0';
            argv[i++] = &(dupline[j+1]);
        }
    }
    argc = i;
    for ( i=0; i<argc; i++) {
        printf("%d: %s\n", i, argv[i]);
    }
#endif
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/

    int c;
    int debugLevel;
    /* getopt info: every available option is listed here. if it is followed by a ':' it
       needs an argument. If it is followed by a '::' the argument is optional. */
    while ((c = getopt(argc, argv,"hd:")) != -1) {
        switch (c) {
            case 'd':
                debugLevel = atoi(optarg);
                if ((debugLevel > LOG_NONE) && (debugLevel <= LOG_DEBUG_ALL)) {
                    setOutputLevel(debugLevel);
                } else {
                    std::cerr << "Ignoring Invalid Debug Level: " << debugLevel << std::endl;
                }
                std::cerr << "Opt for new Debug Level\n";
                break;
            case 'h':
                std::cerr << "Help: " << std::endl;
                std::cerr << "-d [debuglevel]   Set the debuglevel" << std::endl;
                exit(1);
                break;
            default:
                std::cerr << "Unknown Option: " << c << std::endl;
                std::cerr << "Use '-h' for help." << std::endl;
        }
    }

    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifdef WINDOWS_SYS
    // Windows Networking Init.
    WORD wVerReq = MAKEWORD(2,2);
    WSADATA wsaData;
    if (0 != WSAStartup(wVerReq, &wsaData)) {
        exit(0);
    }
#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
}

void Init::loadBaseDir() {
    // get the default configuration location
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS
    char *h = getenv("HOME");
    if (h == NULL) exit(1);

    basedir = h;
    basedir += "/.Mixologist";
    basedir = QDir::toNativeSeparators(basedir);
#else //Windows
    char *h = getenv("APPDATA");
    if (h == NULL) {
        // generating default
        basedir="C:\\Mixologist";

    } else {
        basedir = h;
    }

    basedir += "\\Mixologist";
#endif
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
    if (!DirUtil::checkCreateDirectory(basedir)) {
        exit(1);
    }
}

void Init::loadUserDir(unsigned int librarymixer_id) {
    userdir = basedir + QDir::separator() + QString::number(librarymixer_id);
    if (!DirUtil::checkCreateDirectory(userdir)) exit(1);

    return;
}

QString Init::InitEncryption(unsigned int _librarymixer_id) {
    authMgr = new AuthMgr();

    QString cert;
    if (!authMgr->InitAuth(_librarymixer_id, cert)) return "";

    return cert;
}



Control *Init::createControl(QString ownName) {
    QThread* serverThread = new QThread();
    Server *server = new Server();
    server->moveToThread(serverThread);
    serverThread->start();

    control = server;

#ifndef WINDOWS_SYS
    /* Switch off process termination on a broken network connection for Linux and OS X. */
    signal(SIGPIPE, SIG_IGN);
#endif

    QString saveDir;
    QString partialsDir;
#ifndef WINDOWS_SYS
    char *envSaveDir = getenv("HOME");
    char *envTmpDir = getenv("TMPDIR");
    saveDir = envSaveDir;
    saveDir.append(QDir::separator() + "/Desktop");
    if (envTmpDir != NULL) partialsDir = envTmpDir;
    else partialsDir = "/tmp";
    partialsDir = partialsDir + QDir::separator() + "MixologistPartialDownloads";
#else //Windows
    char envSaveDir[MAX_PATH];
    if (SHGetFolderPath(0, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, envSaveDir) == S_OK) {
        saveDir = envSaveDir;
    } else saveDir = "C:\\MixologistDownloads";
    char *envTmpDir = getenv("TMP");
    if (envTmpDir != NULL) partialsDir = QString(envTmpDir) + QDir::separator() + "MixologistPartialDownloads";
    else partialsDir = "C:\\MixologistPartialDownloads";
#endif

    /**************************************************************************/
    /* setup classes / structures */
    /**************************************************************************/

    notify = new pqiNotify();

    ownConnectivityManager = new OwnConnectivityManager();
    friendsConnectivityManager = new FriendsConnectivityManager();

    aggregatedConnectionsToFriends = new AggregatedConnectionsToFriends();
    friendsConnectivityManager->addMonitor(aggregatedConnectionsToFriends);

    /* Create this before LibraryMixerLibraryManager so it can use it to begin hashing files that need hashing. */
    QThread* fileWatcherThread = new QThread();
    fileWatcher = new ftFileWatcher();
    fileWatcher->moveToThread(fileWatcherThread);
    fileWatcherThread->start();

    /* This must take place before the ftserver so it can use it in its own setup. */
    librarymixermanager = new LibraryMixerLibraryManager();
    libraryMixerFriendLibrary = new LibraryMixerFriendLibrary();

    borrowManager = new ftBorrowManager();

    /****** New Ft Server *****/
    files = ftserver = new ftServer();
    ftserver->setP3Interface(aggregatedConnectionsToFriends);

    ftserver->SetupFtServer();

    /* setup default paths) */
    fileDownloadController->loadOrSetDownloadDirectory(saveDir);
    fileDownloadController->loadOrSetPartialsDirectory(partialsDir);

    /* create Services */
    statusService = new StatusService();
    aggregatedConnectionsToFriends->addService(statusService);

    p3ChatService *chatservice = new p3ChatService();
    aggregatedConnectionsToFriends->addService(chatservice);

    mixologyService = new MixologyService();
    aggregatedConnectionsToFriends->addService(mixologyService);
    ftserver->setupMixologyService();

    aggregatedConnectionsToFriends->load_transfer_rates();

    ownConnectivityManager->select_NetInterface_OpenPorts();

    pqiloopback *loopback = new pqiloopback(authMgr->OwnCertId(), authMgr->OwnLibraryMixerId());
    aggregatedConnectionsToFriends->AddPQI(loopback);

    peers = new p3Peers(ownName);
    msgs = new p3Msgs(chatservice); //not actually for messages, used to be for both chat and messages, now only chat

    return server;
}

void Init::loadLibrary(const QDomElement &libraryElement) {
    librarymixermanager->mergeLibrary(libraryElement);
}

QString Init::getBaseDirectory(bool withDirSeperator) {
    if (withDirSeperator) return (basedir + QDir::separator());
    return basedir;
}

QString Init::getUserDirectory(bool withDirSeperator) {
    if (withDirSeperator) return (userdir + QDir::separator());
    return userdir;
}
