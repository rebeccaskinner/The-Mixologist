#include <signal.h>

#include <unistd.h>

#include "ft/ftserver.h"
#include "ft/ftcontroller.h"

#include "util/debug.h"
#include "util/dir.h"
#include "util/net.h"

#include "interface/init.h"
#include "interface/peers.h" //for peers variable

#include "services/statusservice.h"
#include "services/p3chatservice.h"
#include "services/mixologyservice.h"

#include <list>
#include <string>
#include <sstream>
#include <fstream>
#include <QFile>
#include <QTextStream>

#include <dirent.h>

/* Implemented Interfaces */
#include "server/server.h"
#include "server/p3peers.h"
#include "server/p3msgs.h"

#include "pqi/authmgr.h"

// for blocking signals
#include <signal.h>
#include "util/debug.h"
#include "pqi/p3notify.h"
#include "upnp/upnphandler.h"
#include "dht/opendhtmgr.h"
#include "pqi/pqisslpersongrp.h"
#include "pqi/pqiloopback.h"
#include "ft/ftcontroller.h"
/* Implemented Interfaces */
#include "server/p3peers.h"
#include "server/p3msgs.h"
#include "ft/ftserver.h"
#include "services/p3chatservice.h"

#include <sys/time.h>

#ifdef WINDOWS_SYS
#include <shlobj.h>
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0 //not really sure why, but this isn't defined by our shlobj.h
#endif
#endif

const int p3facestartupzone = 47238;

// initial configuration bootstrapping...

/* Win/Unix Differences */
char Init::dirSeperator;

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

/* global variable now points straight to
 * ft/ code so variable defined here.
 */

Files *files = NULL;
ftServer *ftserver = NULL;

//Definition for the extern variables
Iface   *iface    = NULL;
Control *control = NULL;
p3ConnectMgr *conMgr = NULL;


void Init::InitNetConfig() {
    port = 1680; // default port.
    forceLocalAddr = false;
    forceExtPort   = false;

    strcpy(inet, "127.0.0.1");
    strcpy(logfname, "");

    haveDebugLevel = false;
    debugLevel  = PQL_WARNING;
    udpListenerOnly = false;

#ifndef WINDOWS_SYS
    dirSeperator = '/'; // For unix.
#else
    dirSeperator = '\\'; // For windows.
#endif

    /* Setup logging */
    setOutputLevel(PQL_WARNING); // default to Warnings.

    // Setup more detailed logging for desired zones.
    // For Testing purposes, can set any individual section to have greater logfile output.
    //setZoneLevel(PQL_DEBUG_BASIC, 38422); // pqipacket.
    //setZoneLevel(PQL_DEBUG_BASIC, 96184); // pqinetwork;
    //setZoneLevel(PQL_DEBUG_BASIC, 82371); // pqiperson.
    //setZoneLevel(PQL_DEBUG_BASIC, 60478); // pqitunnel.
    //setZoneLevel(PQL_DEBUG_BASIC, 34283); // pqihandler.
    //setZoneLevel(PQL_DEBUG_BASIC, 44863); // discItems.
    //setZoneLevel(PQL_DEBUG_BASIC, 2482); // p3disc
    //setZoneLevel(PQL_DEBUG_BASIC, 1728); // pqi/p3proxy
    //setZoneLevel(PQL_DEBUG_BASIC, 1211); // sslroot.
    //setZoneLevel(PQL_DEBUG_BASIC, 37714); // pqissl.
    //setZoneLevel(PQL_DEBUG_BASIC, 8221); // pqistreamer.
    //setZoneLevel(PQL_DEBUG_BASIC,  9326); // pqiarchive
    //setZoneLevel(PQL_DEBUG_BASIC, 3334); // p3channel.
    //setZoneLevel(PQL_DEBUG_BASIC, 354); // pqipersongrp.
    //setZoneLevel(PQL_DEBUG_BASIC, 6846); // pqiudpproxy
    //setZoneLevel(PQL_DEBUG_BASIC, 3144); // pqissludp;
    //setZoneLevel(PQL_DEBUG_BASIC, 86539); // pqifiler.
    //setZoneLevel(PQL_DEBUG_BASIC, 91393); // Funky_Browser.
    //setZoneLevel(PQL_DEBUG_BASIC, 25915); // fltkserver
    //setZoneLevel(PQL_DEBUG_BASIC, 47659); // fldxsrvr
    //setZoneLevel(PQL_DEBUG_BASIC, 49787); // pqissllistener
    //setZoneLevel(PQL_DEBUG_BASIC, 3431); // p3connmgr
    //setZoneLevel(PQL_DEBUG_BASIC, 29539); // ftserver
    //setZoneLevel(PQL_DEBUG_BASIC, FTCONTROLLERZONE);
    //setZoneLevel(PQL_DEBUG_BASIC, 29592); // ftdatamultiplex
    //setZoneLevel(PQL_DEBUG_BASIC, UPNPHANDLERZONE);
}

/******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS
void Init::processCmdLine(int argc, char **argv) {
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#else

/* for static PThreads under windows... we need to init the library...
 */
#ifdef PTW32_STATIC_LIB
#include <pthread.h>
#endif

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

    /* for static PThreads under windows... we need to init the library...
     */
#ifdef PTW32_STATIC_LIB
    pthread_win32_process_attach_np();
#endif

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

void Init::loadUserDir(int librarymixer_id) {

    userdir = basedir + dirSeperator + QString::number(librarymixer_id);
    if (!DirUtil::checkCreateDirectory(userdir)) exit(1);

    return;
}

QString Init::InitEncryption(int _librarymixer_id) {
    getAuthMgr()->initSSL(); //first time using SSL system is here

    QString cert;
    if (!getAuthMgr()->InitAuth(_librarymixer_id, cert)) return "";

    return cert;
}



Control *Init::createControl(QString name, NotifyBase &notifybase) {
    iface = new Iface(notifybase);
    Server *server = new Server(*iface, notifybase);
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
#ifdef DEBUGSTARTUP
    if (0 == sigaction(SIGPIPE, &sigact, NULL)) {
        std::cerr << "Successfully Installed";
        std::cerr << "the SIGPIPE Block" << std::endl;
    } else {
        std::cerr << "Failed to Install";
        std::cerr << "the SIGPIPE Block" << std::endl;
    }
#endif
#endif
    /******************************** WINDOWS/UNIX SPECIFIC PART ******************/
    server->mAuthMgr = getAuthMgr();

    std::string ownId = server->mAuthMgr->OwnCertId();
    int librarymixer_id = server->mAuthMgr->OwnLibraryMixerId();

    /**************************************************************************/
    /* Any Initial Configuration (Commandline Options)  */
    /**************************************************************************/

    unsigned long flags = 0;
    if (udpListenerOnly) {
        flags |= PQIPERSON_NO_LISTENER;
    }

    /**************************************************************************/

#ifndef WINDOWS_SYS
    char *saveDir = getenv("HOME");
    char *tmpDir = getenv("TMPDIR");
    QString emergencySaveDir = saveDir;
    QString emergencyPartialsDir;
    if (tmpDir != NULL) emergencyPartialsDir = tmpDir;
    else emergencyPartialsDir = "/tmp";
    emergencyPartialsDir = emergencyPartialsDir + dirSeperator + "LibraryMixerPartialDownloads";
#else //Windows
    QString emergencySaveDir;
    QString emergencyPartialsDir;
    //wchar_t wcharFolder[MAX_PATH];
    char charFolder[MAX_PATH];
    if (SHGetFolderPath( 0, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, charFolder) == S_OK) {
        emergencySaveDir = charFolder;
    } else emergencySaveDir = "C:\\LibraryMixerDownloads";
    char *tmpDir = getenv("TMP");
    if (tmpDir != NULL) emergencyPartialsDir = tmpDir;
    else emergencyPartialsDir = "C:\\LibraryMixerPartialDownloads";
#endif

    /**************************************************************************/
    /* setup classes / structures */
    /**************************************************************************/

    /* Setup Notify Early - So we can use it. */
    notify = new p3Notify();

    conMgr = new p3ConnectMgr(name, server->mAuthMgr);
    server->mConnMgr = conMgr;
    server->mConnMgr->addNetAssistFirewall(new upnphandler());
    server->mDhtMgr  = new OpenDHTMgr(ownId, server->mConnMgr, userdir);
    server->mConnMgr->addNetAssistConnect(server->mDhtMgr);

    SecurityPolicy *none = secpolicy_create();
    server->pqih = new pqisslpersongrp(none, flags);

    /****** New Ft Server **** !!! */
    files = server->ftserver = ftserver = new ftServer(server->mAuthMgr, server->mConnMgr);
    server->ftserver->setP3Interface(server->pqih);

    server->ftserver->SetupFtServer();

    /* setup any extra bits (Default Paths) */
    server->ftserver->setDownloadDirectory(emergencySaveDir, true);
    server->ftserver->setPartialsDirectory(emergencyPartialsDir, true);

    /* create Services */
    server->pqih->addService(new StatusService());

    p3ChatService *chatservice = new p3ChatService(server->mConnMgr);
    server->pqih-> addService(chatservice);

    MixologyService *mixologyservice = new MixologyService();
    server->pqih->addService(mixologyservice);
    server->ftserver->setMixologyService(mixologyservice);

    /**************************************************************************/
    /* need to Monitor too! */
    //the mCacheStrapper and mCacheTransfer are already added by ftServer::SetupFtServer

    server->mConnMgr->addMonitor(server->pqih);

    /**************************************************************************/


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

        server->mConnMgr->setLocalAddress(librarymixer_id, laddr);
    }

    if (forceExtPort) {
        server->mConnMgr->setOwnNetConfig(NET_MODE_EXT, VIS_STATE_STD);
    }

    server->mConnMgr -> checkNetAddress();

    /**************************************************************************/
    /* startup (stuff dependent on Ids/peers is after this point) */
    /**************************************************************************/

    server->pqih->init_listener();




    /**************************************************************************/
    /* load caches and secondary data */
    /**************************************************************************/


    /**************************************************************************/
    /* Force Any Last Configuration Options */
    /**************************************************************************/

    /**************************************************************************/
    /* Start up Threads */
    /**************************************************************************/
    // create loopback device, and add to pqisslgrp.

    SearchModule *mod = new SearchModule();
    pqiloopback *ploop = new pqiloopback(ownId, librarymixer_id);

    mod -> cert_id = ownId;
    mod -> pqi = ploop;
    mod -> sp = secpolicy_create();

    server->pqih->AddSearchModule(mod);

    /* Setup GUI Interfaces. */

    peers = new p3Peers(server->mConnMgr, server->mAuthMgr);
    msgs  = new p3Msgs(server->mConnMgr, chatservice); //not actually for messages, used to be for both chat and messages, now only chat

    return server;
}

QString Init::getBaseDirectory(bool withDirSeperator) {
    if (withDirSeperator) return (basedir + dirSeperator);
    return basedir;
}

QString Init::getUserDirectory(bool withDirSeperator) {
    if (withDirSeperator) return (userdir + dirSeperator);
    return userdir;
}
