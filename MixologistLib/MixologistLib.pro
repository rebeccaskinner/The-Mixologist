TEMPLATE = lib
CONFIG += static
TARGET = Mixologist
QT += xml network

################################# Linux ##########################################

debug {
	DEFINES *= DEBUG
	QMAKE_CXXFLAGS *= -g
}

linux-g++ {
	OBJECTS_DIR = temp/linux-g++/obj
	DESTDIR = lib.linux-g++
	QMAKE_CXXFLAGS *= -fomit-frame-pointer -Wall 
	QMAKE_CC = g++
}
linux-g++-64 {
	OBJECTS_DIR = temp/linux-g++-64/obj
	DESTDIR = lib.linux-g++-64
	QMAKE_CXXFLAGS *= -fomit-frame-pointer -Wall 
	QMAKE_CC = g++
}
#################### Cross compilation for windows under Linux ####################

win32-x-g++ {	
	OBJECTS_DIR = temp/win32xgcc/obj
	DESTDIR = lib.win32xgcc
	DEFINES *= WINDOWS_SYS WIN32 WIN_CROSS_UBUNTU
	QMAKE_CXXFLAGS *= -Wmissing-include-dirs
	QMAKE_CC = i586-mingw32msvc-g++
	QMAKE_LIB = i586-mingw32msvc-ar
	DEFINES *= STATICLIB WIN32

	INCLUDEPATH *= /usr/i586-mingw32msvc/include ${HOME}/.wine/drive_c/pthreads/include/
}
################################# Windows ##########################################

win32 {
	QMAKE_CC = g++
  OBJECTS_DIR = temp/obj
	MOC_DIR = temp/moc
  DEFINES = WINDOWS_SYS WIN32 STATICLIB MINGW PTW32_STATIC_LIB
	DESTDIR = lib
	  
	PTHREADS_DIR = ../ThirdParty/src/pthreads-w32-2-8-0-release
	ZLIB_DIR = ../ThirdParty/src/zlib-1.2.3
        
  INCLUDEPATH += . $${PTHREADS_DIR} $${ZLIB_DIR}
}

# #################################### MacOS ######################################
macx {
        OBJECTS_DIR = temp/obj
        DESTDIR = lib
        QMAKE_CXXFLAGS *= -fomit-frame-pointer -Wall
        QMAKE_CC = g++
        CONFIG += x86 ppc
        LIBS += -dead_strip
        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4
}
################################### COMMON stuff ##################################

DEFINES *= MINIUPNPC_VERSION=14

SSL_DIR= ../ThirdParty/src/openssl-1.0.0a
UPNPC_DIR= ../ThirdParty/src/miniupnpc-1.4.20100202

INCLUDEPATH += . $${SSL_DIR}/include $${UPNPC_DIR}

# Input
HEADERS += dht/b64.h \
           dht/dhtclient.h \
           dht/opendht.h \
           dht/opendhtmgr.h \
           dht/opendhtstr.h \
           ft/ftcontroller.h \
           ft/ftdata.h \
           ft/ftdatamultiplex.h \
           ft/ftitemlist.h \
           ft/ftfilecreator.h \
           ft/ftfileprovider.h \
           ft/ftfilesearch.h \
           ft/ftsearch.h \
           ft/ftserver.h \
           ft/fttransfermodule.h \
           ft/mixologyborrower.h \
           pqi/authmgr.h \
           pqi/p3connmgr.h \
           pqi/p3dhtmgr.h \
           pqi/p3notify.h \
           pqi/p3upnpmgr.h \
           pqi/pqi.h \
           pqi/pqi_base.h \
           pqi/pqiassist.h \
           pqi/pqibin.h \
           pqi/pqihandler.h \
           pqi/pqihash.h \
           pqi/pqilistener.h \
           pqi/pqiloopback.h \
           pqi/pqimonitor.h \
           pqi/pqinetwork.h \
           pqi/pqinotify.h \
           pqi/pqiperson.h \
           pqi/pqipersongrp.h \
           pqi/pqisecurity.h \
           pqi/pqiservice.h \
           pqi/pqissl.h \
           pqi/pqissllistener.h \
           pqi/pqisslpersongrp.h \
           pqi/pqissludp.h \
           pqi/pqistreamer.h \
           interface/files.h \
           interface/iface.h \
           interface/init.h \
           interface/msgs.h \
           interface/notify.h \
           interface/peers.h \
           interface/types.h \
           interface/settings.h \
           interface/librarymixer-connect.h \
           interface/librarymixer-library.h \
           server/server.h \
           server/p3msgs.h \
           server/p3peers.h \
           server/pqistrings.h \
           serialiser/baseitems.h \
           serialiser/baseserial.h \
           serialiser/mixologyitems.h \
           serialiser/statusitems.h \
           serialiser/msgitems.h \
           serialiser/serial.h \
           serialiser/serviceids.h \
           serialiser/tlvbase.h \
           serialiser/tlvtypes.h \
           services/mixologyservice.h \
           services/statusservice.h \
           services/p3chatservice.h \
           services/p3service.h \
           tcponudp/bio_tou.h \
           tcponudp/tcppacket.h \
           tcponudp/tcpstream.h \
           tcponudp/tou.h \
           tcponudp/tou_errno.h \
           tcponudp/tou_net.h \
           tcponudp/udplayer.h \
           tcponudp/udpsorter.h \
           upnp/upnphandler.h \
           upnp/upnputil.h \
           util/debug.h \
           util/dir.h \
           util/net.h \
           util/print.h \
           util/threads.h \

SOURCES = \
				dht/dht_check_peers.cc \
				dht/dht_bootstrap.cc \
	   			server/librarymixer-connect.cc \
	   			server/librarymixer-library.cc \
				server/p3face-msgs.cc \
				server/interface.cc \
				server/types.cc \
				server/init.cc \
				server/p3face-config.cc \
				server/server.cc \
				server/p3msgs.cc \
				server/p3peers.cc \
				ft/ftcontroller.cc \
				ft/ftserver.cc \
				ft/fttransfermodule.cc \
				ft/ftdatamultiplex.cc \
				ft/ftfilesearch.cc \
				ft/ftitemlist.cc \
				ft/ftfilecreator.cc \
				ft/ftdata.cc \
				ft/ftfileprovider.cc \
				ft/mixologyborrower.cc \
                                upnp/upnputil.cc \
				dht/opendhtmgr.cc \
				upnp/upnphandler.cc \
				dht/opendht.cc \
				dht/opendhtstr.cc \
                                dht/b64.cc \
				services/mixologyservice.cc \
                                services/statusservice.cc \
				services/p3chatservice.cc \
				services/p3service.cc \
				pqi/p3notify.cc \
				pqi/pqipersongrp.cc \
				pqi/pqihandler.cc \
				pqi/pqiservice.cc \
				pqi/pqiperson.cc \
				pqi/pqissludp.cc \
                                pqi/authmgr.cc \
				pqi/pqisslpersongrp.cc \
				pqi/pqissllistener.cc \
				pqi/pqissl.cc \
				pqi/p3connmgr.cc \
				pqi/p3dhtmgr.cc \
				pqi/pqibin.cc \
				pqi/pqistreamer.cc \
				pqi/pqiloopback.cc \
				pqi/pqinetwork.cc \
				pqi/pqisecurity.cc \
				serialiser/mixologyitems.cc \
                                serialiser/statusitems.cc \
				serialiser/msgitems.cc \
				serialiser/baseitems.cc \
				serialiser/baseserial.cc \
				serialiser/tlvbase.cc \
				serialiser/tlvtypes.cc \
                                serialiser/tlvfileitem.cc \
				serialiser/serial.cc \
                                tcponudp/bss_tou.cc \
				tcponudp/tcpstream.cc \
				tcponudp/tou.cc \
				tcponudp/tcppacket.cc \
				tcponudp/udpsorter.cc \
				tcponudp/tou_net.cc \
				tcponudp/udplayer.cc \
				util/debug.cc \
				util/dir.cc \
				util/net.cc \
				util/print.cc \
				util/threads.cc 

