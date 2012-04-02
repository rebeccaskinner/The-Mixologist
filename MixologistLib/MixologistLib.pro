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

	INCLUDEPATH *= /usr/i586-mingw32msvc/include
}
################################# Windows ##########################################

win32 {
	QMAKE_CC = g++
	OBJECTS_DIR = temp/obj
	MOC_DIR = temp/moc
	DEFINES = WINDOWS_SYS WIN32 STATICLIB
	DESTDIR = lib
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

SSL_DIR= ../ThirdParty/src/openssl-1.0.0a
UPNPC_DIR= ../ThirdParty/src/miniupnpc-1.6
PJLIB_DIR = ../ThirdParty/src/pjproject-1.12/pjlib
PJLIBUTIL_DIR = ../ThirdParty/src/pjproject-1.12/pjlib-util
PJNATH_DIR = ../ThirdParty/src/pjproject-1.12/pjnath

INCLUDEPATH += . $${SSL_DIR}/include $${UPNPC_DIR} $${PJNATH_DIR}/include $${PJLIB_DIR}/include $${PJLIBUTIL_DIR}/include

# Input
HEADERS += ft/ftcontroller.h \
           ft/fttransfermodule.h \
           ft/ftdata.h \
           ft/ftdatademultiplex.h \
           ft/fttemplist.h \
           ft/ftofflmlist.h \
           ft/ftfilecreator.h \
           ft/ftfileprovider.h \
           ft/ftfilewatcher.h \
           ft/ftfilemethod.h \
           ft/ftserver.h \
           ft/ftborrower.h \
           pqi/authmgr.h \
           pqi/ownConnectivityManager.h \
           pqi/friendsConnectivityManager.h \
           pqi/pqi.h \
           pqi/pqi_base.h \
           pqi/pqihandler.h \
           pqi/pqilistener.h \
           pqi/pqiloopback.h \
           pqi/pqimonitor.h \
           pqi/pqinetwork.h \
           pqi/pqinotify.h \
           pqi/connectionToFriend.h \
           pqi/aggregatedConnections.h \
           pqi/pqiservice.h \
           pqi/pqissl.h \
           pqi/pqissllistener.h \
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
           server/server.h \
           server/librarymixer-library.h \
           server/librarymixer-friendlibrary.h \
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
           tcponudp/stunpacket.h \
           tcponudp/connectionrequestpacket.h \
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
           util/xml.h \

SOURCES = \
				server/librarymixer-connect.cc \
	   			server/librarymixer-library.cc \
	   			server/librarymixer-friendlibrary.cc \
				server/init.cc \
				server/server.cc \
				server/p3msgs.cc \
				server/p3peers.cc \
				server/types.cc \
				ft/ftcontroller.cc \
				ft/fttransfermodule.cc \
				ft/ftserver.cc \
				ft/ftdatademultiplex.cc \
				ft/fttemplist.cc \
				ft/ftofflmlist.cc \
				ft/ftfilecreator.cc \
				ft/ftfileprovider.cc \
				ft/ftfilewatcher.cc \
				ft/ftborrower.cc \
                                upnp/upnputil.cc \
				upnp/upnphandler.cc \
				services/mixologyservice.cc \
                                services/statusservice.cc \
				services/p3chatservice.cc \
				services/p3service.cc \
                                pqi/aggregatedConnections.cc \
				pqi/pqihandler.cc \
				pqi/pqinotify.cc \
				pqi/pqiservice.cc \
                                pqi/connectionToFriend.cc \
				pqi/pqissludp.cc \
                                pqi/authmgr.cc \
				pqi/pqissllistener.cc \
				pqi/pqissl.cc \
                                pqi/ownConnectivityManager.cc \
                                pqi/friendsConnectivityManager.cc \
				pqi/pqistreamer.cc \
				pqi/pqiloopback.cc \
				pqi/pqinetwork.cc \
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
                                tcponudp/stunpacket.cc \
                                tcponudp/connectionrequestpacket.cc \
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
				util/xml.cc 

