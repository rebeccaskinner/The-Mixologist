CONFIG += qt \
          gui \
          uic \
          qrc \
          resources \
          uitools
QT += network \
      xml \
      script
TEMPLATE = app
TARGET = Mixologist
RCC_DIR = temp/qrc
UI_DIR = temp/ui
MOC_DIR = temp/moc

# ################################ Linux ##########################################
linux-g++ {
    OBJECTS_DIR = temp/linux-g++/obj
    LIBS += ../MixologistLib/lib.linux-g++/libMixologist.a
    LIBS += ../ThirdParty/lib/libminiupnpc.a
    LIBS += ../ThirdParty/lib/libssl.a
    LIBS += ../ThirdParty/lib/libcrypto.a
    LIBS += -lQtUiTools
    LIBS += -lz
}
linux-g++-64 {
    OBJECTS_DIR = temp/linux-g++-64/obj
    LIBS += ../MixologistLib/lib.linux-g++-64/libMixologist.a
    LIBS += ../ThirdParty/lib/libminiupnpc.a
    LIBS += ../ThirdParty/liblibssl.a
    LIBS += ../ThirdParty/lib/libcrypto.a
    LIBS += -lz
}

# ################### Cross compilation for windows under Linux ###################
#win32-x-g++ {
#    OBJECTS_DIR = temp/win32-x-g++/obj
#    LIBS += ../MixologistLib/win32-x-g++/libMixologist.a
#    LIBS += ../ThirdParty/lib/libssl.a
#    LIBS += ../ThirdParty/lib/libcrypto.a
#    LIBS += ../ThirdParty/lib/libminiupnpc.a
#    LIBS += ../ThirdParty/lib/libz.a
#    LIBS += -lws2_32 \
#        -luuid \
#        -lole32 \
#        -liphlpapi \
#        -lcrypt32 \
#        -gdi32
#    LIBS += -lole32 \
#        -lwinmm
#    DEFINES *= WIN32
#    RC_FILE = gui/Images/win.rc
#}
#
# ################################### Windows #####################################
win32 {
    OBJECTS_DIR = temp/obj
    LIBS += ../MixologistLib/lib/libMixologist.a
    LIBS += -L"../ThirdParty/lib" \
        -lminiupnpc \
        -lz \
        -lssl \
        -lcrypto
    LIBS += -lws2_32 \
        -luuid \
        -lole32 \
        -liphlpapi \
        -lcrypt32-cygwin \
        -lgdi32
    LIBS += -lole32 \
        -lwinmm
    RC_FILE = platformspecific/win.rc

#Remove if not static compile
    QTPLUGIN += qgif
    DEFINES += STATIC
}

# #################################### MacOS ######################################
macx {
    CONFIG += x86 ppc
    OBJECTS_DIR = temp/obj
    LIBS += -Wl,-search_paths_first
    LIBS += ../MixologistLib/lib/libMixologist.a
    LIBS += -L"../ThirdParty/lib" \
        -lssl \
        -lcrypto \
        -lminiupnpc \
        -lz
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4
    QMAKE_INFO_PLIST = platformspecific/Info.plist
    ICON = platformspecific/mac.icns

#Remove if not static compile
    QTPLUGIN += qgif
    DEFINES += STATIC
}

# ############################# Common stuff ######################################
include(qtsingleapplication\qtsingleapplication.pri)
DEPENDPATH += . \
    interface \
    gui \
    gui\common \
    gui\images \
    gui\Preferences \
    gui\Toaster
INCLUDEPATH += .
HEADERS += version.h \
    qtsingleapplication\mixologistapplication.h \
    interface/librarymixer-connect.h \
    interface/files.h \
    interface/init.h \
    interface/msgs.h \
    interface/peers.h \
    interface/iface.h \
    interface/types.h \
    interface/notify.h \
    interface/notifyqt.h \
    interface/settings.h \
    gui/StartDialog.h \
    gui/WelcomeWizard.h \
    gui/NetworkDialog.h \
    gui/TransfersDialog.h \
    gui/MainWindow.h \
    gui/PeersDialog.h \
    gui/LibraryDialog.h \
    gui/LibraryModel.h \
    gui/LibraryFriendModel.h \
    gui/FriendsLibraryDialog.h \
    gui/OffLMOwnModel.h \
    gui/OffLMFriendModel.h \
    gui/PopupChatDialog.h \
    gui/Preferences/configpage.h \
    gui/Preferences/GeneralDialog.h \
    gui/Preferences/TransfersPrefDialog.h \
    gui/Preferences/PreferencesWindow.h \
    gui/Preferences/ServerDialog.h \
    gui/Preferences/NotifyDialog.h \
    gui/Toaster/OnlineToaster.h \
    gui/Toaster/QtToaster.h \
    gui/Toaster/IQtToaster.h \
    gui/Statusbar/ratesstatus.h \
    gui/Util/GuiSettingsUtil.h \
    gui/Util/Helpers.h \
    gui/Util/OSHelpers.h
FORMS += gui/StartDialog.ui \
    gui/NetworkDialog.ui \
    gui/TransfersDialog.ui \
    gui/MainWindow.ui \
    gui/PeersDialog.ui \
    gui/LibraryDialog.ui \
    gui/FriendsLibraryDialog.ui \
    gui/PopupChatDialog.ui \
    gui/Preferences/GeneralDialog.ui \
    gui/Preferences/TransfersPrefDialog.ui \
    gui/Preferences/PreferencesWindow.ui \
    gui/Preferences/ServerDialog.ui \
    gui/Preferences/NotifyDialog.ui \
    gui/Toaster/OnlineToaster.ui
SOURCES += main.cpp \
    qtsingleapplication\mixologistapplication.cpp \
    interface/notifyqt.cpp \
    gui/StartDialog.cpp \
    gui/WelcomeWizard.cpp \
    gui/NetworkDialog.cpp \
    gui/TransfersDialog.cpp \
    gui/MainWindow.cpp \
    gui/PeersDialog.cpp \
    gui/LibraryDialog.cpp \
    gui/LibraryModel.cpp \
    gui/LibraryFriendModel.cpp \
    gui/FriendsLibraryDialog.cpp \
    gui/OffLMOwnModel.cpp \
    gui/OffLMFriendModel.cpp \
    gui/PopupChatDialog.cpp \
    gui/Preferences/GeneralDialog.cpp \
    gui/Preferences/TransfersPrefDialog.cpp \
    gui/Preferences/PreferencesWindow.cpp \
    gui/Preferences/ServerDialog.cpp \
    gui/Preferences/NotifyDialog.cpp \
    gui/Statusbar/ratesstatus.cpp \
    gui/Toaster/OnlineToaster.cpp \
    gui/Toaster/QtToaster.cpp \
    gui/Util/GuiSettingsUtil.cpp \
    gui/Util/Helpers.cpp \
    gui/Util/OSHelpers.cpp
RESOURCES += gui/images.qrc
