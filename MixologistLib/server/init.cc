#include "ft/ftserver.h"
#include "ft/ftcontroller.h"
#include "ft/ftofflmlist.h"
#include "ft/fttemplist.h"
#include "ft/ftfilewatcher.h"
#include "ft/ftborrower.h"

#include "pqi/authmgr.h"
#include "pqi/p3notify.h"
#include "pqi/pqisslpersongrp.h"
#include "pqi/pqiloopback.h"

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

#include "upnp/upnphandler.h"
#include "dht/opendhtmgr.h"

#include "util/debug.h"
#include "util/dir.h"
#include "util/net.h"

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

/* Directories and Files*/
QString Init::basedir;
QString Init::userdir;

/* Listening Port */
bool Init::forceExtPort;
bool Init::forceLocalAddr;
unsigned short Init::port;
char Init::inet[256];

/* Logging */
bool Init::haveDebugLevel;
int  Init::debugLevel;
char Init::logfname[1024];

bool Init::udpListenerOnly;

//Definition for the extern variables
Files *files = NULL;
ftServer *ftserver = NULL;
ftController *fileDownloadController = NULL;
Control *control = NULL;
p3ConnectMgr *connMgr = NULL;
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

void Init::InitNetConfig() {
    port = 1680; // default port.
    forceLocalAddr = false;
    forceExtPort   = false;

    strcpy(inet, "127.0.0.1");
    strcpy(logfname, "");

    haveDebugLevel = false;
    debugLevel  = PQL_WARNING;
    udpListenerOnly = false;

    /* Setup logging */
    setOutputLevel(LOG_WARNING); // default to Warnings.

    // Setup more detailed logging for desired zones.
    // For Testing purposes, can set any individual section to have greater logfile output.
    //setZoneLevel(PQL_DEBUG_BASIC, PQIHANDLERZONE);
}

/******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS
void Init::processCmdLine(int argc, char **argv) {
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
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
    /* getopt info: every availiable option is listet here. if it is followed by a ':' it
       needs an argument. If it is followed by a '::' the argument is optional.
    */
    while ((c = getopt(argc, argv,"hesamui:p:c:l:d:")) != -1) {
        switch (c) {
            case 'l':
                strncpy(logfname, optarg, 1024);
                std::cerr << "LogFile (" << logfname;
                std::cerr << ") Selected" << std::endl;
                break;
                break;
            case 'i':
                strncpy(inet, optarg, 256);
                std::cerr << "New Inet Addr(" << inet;
                std::cerr << ") Selected" << std::endl;
                forceLocalAddr = true;
                break;
            case 'p':
                port = atoi(optarg);
                std::cerr << "New Listening Port(" << port;
                std::cerr << ") Selected" << std::endl;
                break;
            case 'c':
                userdir = optarg;
                std::cerr << "New Base Config Dir(";
                std::cerr << userdir.toStdString();
                std::cerr << ") Selected" << std::endl;
                break;
            case 's':
                std::cerr << "Output to Stderr";
                std::cerr << std::endl;
                break;
            case 'd':
                haveDebugLevel = true;
                debugLevel = atoi(optarg);
                std::cerr << "Opt for new Debug Level";
                std::cerr << std::endl;
                break;
            case 'u':
                udpListenerOnly = true;
                std::cerr << "Opt for only udpListener";
                std::cerr << std::endl;
                break;
            case 'e':
                forceExtPort = true;
                std::cerr << "Opt for External Port Mode";
                std::cerr << std::endl;
                break;
            case 'h':
                std::cerr << "Help: " << std::endl;
                std::cerr << "-l [logfile]      Set the logfilename" << std::endl;
                std::cerr << "-w [password]     Set the password" << std::endl;
                std::cerr << "-i [ip_adress]    Set IP Adress to use" << std::endl;
                std::cerr << "-p [port]         Set the Port to listen on" << std::endl;
                std::cerr << "-c [userdir]      Set the config basdir" << std::endl;
                std::cerr << "-s                Output to Stderr" << std::endl;
                std::cerr << "-d [debuglevel]   Set the debuglevel" << std::endl;
                std::cerr << "-u                Only listen to UDP" << std::endl;
                std::cerr << "-e                Use a forwarded external Port" << std::endl << std::endl;
                exit(1);
                break;
            default:
                std::cerr << "Unknown Option!" << std::endl;
                std::cerr << "Use '-h' for help." << std::endl;
                exit(1);
        }
    }


    // set the default Debug Level...
    if (haveDebugLevel) {
        if ((debugLevel > 0) &&
                (debugLevel <= PQL_DEBUG_ALL)) {
            std::cerr << "Setting Debug Level to: ";
            std::cerr << debugLevel;
            std::cerr << std::endl;
            setOutputLevel(debugLevel);
        } else {
            std::cerr << "Ignoring Invalid Debug Level: ";
            std::cerr << debugLevel;
            std::cerr << std::endl;
        }
    }

    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifdef WINDOWS_SYS
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
    // Windows Networking Init.
    WORD wVerReq = MAKEWORD(2,2);
    WSADATA wsaData;
    if (0 != WSAStartup(wVerReq, &wsaData)) {
#ifdef DEBUGSTARTUP
        std::cerr << "Failed to Startup Windows Networking";
        std::cerr << std::endl;
#endif
    } else {
#ifdef DEBUGSTARTUP
        std::cerr << "Started Windows Networking";
        std::cerr << std::endl;
#endif
    }
#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
}

void Init::loadBaseDir() {
    // get the default configuration location
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS
    char *h = getenv("HOME");
#ifdef DEBUGSTARTUP
    std::cerr << "basedir() -> $HOME = ";
    std::cerr << h << std::endl;
#endif
    if (h == NULL) {
#ifdef DEBUGSTARTUP
        std::cerr << "loadCheckBasedir() Fatal Error --";
        std::cerr << std::endl;
        std::cerr << "\tcannot determine $HOME dir" <<std::endl;
#endif
        exit(1);
    }
    basedir = h;
    basedir += "/.Mixologist";
    basedir = QDir::toNativeSeparators(basedir);
#else //Windows
    char *h = getenv("APPDATA");
#ifdef DEBUGSTARTUP
    std::cerr << "basedir() -> $APPDATA = ";
    std::cerr << h << std::endl;
    char *h2 = getenv("HOMEDRIVE");
    std::cerr << "basedir() -> $HOMEDRIVE = ";
    std::cerr << h2 << std::endl;
    char *h3 = getenv("HOMEPATH");
    std::cerr << "basedir() -> $HOMEPATH = ";
    std::cerr << h3 << std::endl;
#endif
    if (h == NULL) {
        // generating default
#ifdef DEBUGSTARTUP
        std::cerr << "loadCheckBasedir() getEnv Error --Win95/98?";
        std::cerr << std::endl;
#endif
        basedir="C:\\Mixologist";

    } else {
        basedir = h;
    }

    basedir += "\\Mixologist";
#endif
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
    if (!DirUtil::checkCreateDirectory(basedir)) {
#ifdef DEBUGSTARTUP
        std::cerr << "Cannot Create BaseConfig Dir" << std::endl;
#endif
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

    //First time using SSL system is here
    QString cert;
    if (!authMgr->InitAuth(_librarymixer_id, cert)) return "";

    return cert;
}



Control *Init::createControl(QString name) {
    Server *server = new Server();
    control = server;

    /**************************************************************************/
    /* STARTUP procedure */
    /**************************************************************************/
    /**************************************************************************/
    /* (1) Init variables */
    /**************************************************************************/

    // SWITCH off the SIGPIPE - kills process on Linux.
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS
    struct sigaction sigact;
    sigact.sa_handler = SIG_IGN;
    sigact.sa_flags = 0;
#endif
    /**************************** END WINDOWS/UNIX SPECIFIC PART ******************/

    std::string ownId = authMgr->OwnCertId();
    unsigned int librarymixer_id = authMgr->OwnLibraryMixerId();

    /**************************************************************************/
    /* Any Initial Configuration (Commandline Options)  */
    /**************************************************************************/

    unsigned long flags = 0;
    if (udpListenerOnly) {
        flags |= PQIPERSON_NO_LISTENER;
    }

    /**************************************************************************/

    QString saveDir;
    QString partialsDir;
#ifndef WINDOWS_SYS
    char *envSaveDir = getenv("HOME");
    char *envTmpDir = getenv("TMPDIR");
    saveDir = envSaveDir;
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

    notify = new p3Notify();
    connMgr = new p3ConnectMgr(name);
    connMgr->addNetAssistFirewall(new upnphandler());
    server->mDhtMgr  = new OpenDHTMgr(ownId, connMgr, userdir);
    connMgr->addNetAssistConnect(server->mDhtMgr);

    server->pqih = new pqisslpersongrp(flags);
    connMgr->addMonitor(server->pqih);

    /* This is used by the LibraryMixerLibraryManager to begin hashing files that need hashing. */
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
    ftserver->setP3Interface(server->pqih);

    ftserver->SetupFtServer();

    /* setup default paths) */
    fileDownloadController->loadOrSetDownloadDirectory(saveDir);
    fileDownloadController->loadOrSetPartialsDirectory(partialsDir);

    /* create Services */
    statusService = new StatusService();
    server->pqih->addService(statusService);
    connMgr->addMonitor(statusService);

    p3ChatService *chatservice = new p3ChatService();
    server->pqih->addService(chatservice);

    mixologyService = new MixologyService();
    server->pqih->addService(mixologyService);
    ftserver->setupMixologyService();

    /**************************************************************************/
    /* (2) Load configuration files */
    /**************************************************************************/

    /**************************************************************************/
    /* trigger generalConfig loading for classes that require it */
    /**************************************************************************/

    server->pqih->load_transfer_rates();

    /**************************************************************************/
    /* Force Any Configuration before Startup (After Load) */
    /**************************************************************************/

    if (forceLocalAddr) {
        struct sockaddr_in laddr;

        /* clean sockaddr before setting values (MaxOSX) */
        sockaddr_clear(&laddr);

        laddr.sin_family = AF_INET;
        laddr.sin_port = htons(port);

        // universal
        laddr.sin_addr.s_addr = inet_addr(inet);

        connMgr->setLocalAddress(librarymixer_id, laddr);
    }

    if (forceExtPort) {
        connMgr->setOwnNetConfig(NET_MODE_EXT, VIS_STATE_STD);
    }

    connMgr -> checkNetAddress();

    /**************************************************************************/
    /* startup (stuff dependent on Ids/peers is after this point) */
    /**************************************************************************/

    server->pqih->init_listener();

    /**************************************************************************/
    /* Force Any Last Configuration Options */
    /**************************************************************************/

    pqiloopback *ploop = new pqiloopback(ownId, librarymixer_id);
    server->pqih->AddPQI(ploop);

    /* Setup GUI Interfaces. */

    peers = new p3Peers();
    msgs = new p3Msgs(chatservice); //not actually for messages, used to be for both chat and messages, now only chat

    return server;
}

QString Init::getBaseDirectory(bool withDirSeperator) {
    if (withDirSeperator) return (basedir + QDir::separator());
    return basedir;
}

QString Init::getUserDirectory(bool withDirSeperator) {
    if (withDirSeperator) return (userdir + QDir::separator());
    return userdir;
}
