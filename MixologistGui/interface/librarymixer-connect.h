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

#ifndef LibraryMixerConnect_H
#define LibraryMixerConnect_H

#include <QObject>
#include <QBuffer>
#include <QtNetwork>
#include <QDomElement>

#define MIXOLOGY_CHECKOUT_LINK "mixology:userid==[[id]]¦name==[[full_formatted_name]]¦itemid==[[item_id]]¦"
#define MIXOLOGY_CONTACT_LINK "mixology:userid==[[id]]¦"
#define MIXOLOGY_LINK_TITLE "Mixologist"
#define STANDARD_LINK_TITLE "[[standard]]"

//Number of seconds that must pass between calls to a LibraryMixer API function
#define CONNECT_COOLDOWN 10

class LibraryMixerConnect;
extern LibraryMixerConnect *librarymixerconnect;

class QHttp;

class LibraryMixerConnect : public QObject {
    Q_OBJECT

public:
    LibraryMixerConnect();

    /* Sets the login info that will be used when needed by the download/upload functions */
    void setLogin(const QString &_email,  const QString &_password);

    /* Retrieves the latest version information.
       Returns -1 on all failures. */
    int downloadVersion(qlonglong current);

    /* Retrieves id, name, all checkout_links, public_scratch1, and private_scratch1.
       Returns -1 on all failures. */
    int downloadInfo();

    /* If checkout_link_to_set == -1, does not set, otherwise sets that number checkout_link and checkout_link_title to STANDARD. */
    int uploadInfo(const int link_to_set, const QString &public_key);

    /* Downloads friends list from LibraryMixer.
      Returns -1 on all failures.
      If blocking is true, then does not return until done. Can only call blocking from a QObject.
      Rate is limited by API, and calls before the cooldown period has passed will return -1. */
    int downloadFriends(bool blocking = false);

    /* Downloads friends library list from LibraryMixer
       Returns -1 on all failures.
       Rate is limited by API, and calls before the cooldown period has passed will return -1. */
    int downloadFriendsLibrary();

    /* Downloads library from LibraryMixer.
      Returns -1 on all failures.
      If blocking is true, then does not return until done. Can only call blocking from a QObject.
      Rate is limited by API, and calls before the cooldown period has passed will return -1. */
    int downloadLibrary(bool blocking = false);

    /* Updates our address info on LibraryMixer. */
    int uploadAddress(const QString &localIP, ushort localPort, const QString &externalIP, ushort externalPort);

    enum errors {
        bad_login_error = 0,
        version_download_error = 1,
        ssl_error  = 2,
        info_download_error = 3,
        info_upload_error = 4,
        friend_download_error = 5,
        friend_library_download_error = 6,
        library_download_error = 7,
        address_upload_error = 8
    };

signals:
    void downloadedVersion(qulonglong version, QString description, QString importance);
    void downloadedInfo(QString name, unsigned int librarymixer_id,
                        QString checkout_link1, QString contact_link1, QString link_title1,
                        QString checkout_link2, QString contact_link2, QString link_title2,
                        QString checkout_link3, QString contact_link3, QString link_title3,
                        QDomElement libraryNode);
    void uploadedInfo();
    void downloadedFriends();
    void downloadedFriendsLibrary();
    void downloadedLibrary();
    void uploadedAddress();
    void dataReadProgress(int read, int total);
    void errorReceived(int errorCode);

private slots:
    void httpRequestFinishedSlot(int requestId, bool error);
    void sslErrorsSlot(const QList<QSslError> & errors );
    void slotAuthenticationRequired();
    void blockingTimeOut();

private:
    /* Downloads an XML file from the user set server. */
    int downloadXML(const QString &location, QIODevice *destination);

    /* Downloads an XML file to the user set server. */
    int uploadXML(const QString &path, QIODevice *source, QIODevice *destination);

    /* Utility function used by downloadXML and uploadXML to setup the QT connection. */
    void setupModeAndHost(QString *host, QHttp::ConnectionMode *mode);

    /* Cleans up on receiving an error, and emits the errorReceived signal. */
    void handleErrorReceived(int error);

    mutable QMutex lmMutex;

    int httpGetId;
    int version_check_id;
    int info_download_id;
    int info_upload_id;
    int friend_download_id;
    int friend_library_download_id;
    int library_download_id;
    int address_upload_id;

    QHttp *http;
    QBuffer *buffer;
    QBuffer *uploadBuffer;
    QString email;
    QString password;
    bool doneTransfer; //Used for blocking transfers

    QDateTime lastFriendUpdate;
    QDateTime lastFriendLibraryUpdate;
    QDateTime lastLibraryUpdate;
};

#endif
