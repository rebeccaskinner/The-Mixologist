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

#ifndef DEVSERVER
  #define SERVER "librarymixer.heroku.com"
#else
  #define SERVER "192.168.1.114"
#endif
#define MIXOLOGY_CHECKOUT_LINK "mixology:userid==[[id]]¦name==[[full_formatted_name]]¦itemid==[[item_id]]¦"
#define MIXOLOGY_CONTACT_LINK "mixology:userid==[[id]]¦"
#define MIXOLOGY_LINK_TITLE "Mixologist"
#define STANDARD_LINK_TITLE "[[standard]]"

//Number of seconds that must pass between calls to a LibraryMixer API function
#define CONNECT_COOLDOWN 10

class QHttp;

class LibraryMixerConnect : public QObject {
    Q_OBJECT

public:
    LibraryMixerConnect();
    /*Sets the login info that will be used when needed by the download/upload functions*/
    void setLogin(const QString &_email,  const QString &_password);
    /*Retrieves the latest version information.*/
    int downloadVersion(qlonglong current);
    /*Retrieves id, name, all checkout_links, public_scratch1, and private_scratch1*/
    int downloadInfo();
    /*If checkout_link_to_set == -1, does not set, otherwise sets that number checkout_link and checkout_link_title to STANDARD.*/
    int uploadInfo(const int link_to_set, const QString &public_key);
    /*Downloads friends list from LibraryMixer.
      If blocking is true, then does not return until done. Can only call blocking from a QObject.
      Rate is limited by API, and calls before the cooldown period has passed will return -1.*/
    int downloadFriends(bool blocking = false);
    /*Downloads library from LibraryMixer.
      If blocking is true, then does not return until done. Can only call blocking from a QObject.
      Rate is limited by API, and calls before the cooldown period has passed will return -1.*/
    int downloadLibrary(bool blocking = false);

    enum errors {
        bad_login_error,
        version_download_error,
        ssl_error,
        info_download_error,
        info_upload_error,
        friend_download_error,
        library_download_error
    };

signals:
    void downloadedVersion(qlonglong version, QString description, QString importance);
    void downloadedInfo(QString name, int librarymixer_id,
                        QString checkout_link1, QString contact_link1, QString link_title1,
                        QString checkout_link2, QString contact_link2, QString link_title2,
                        QString checkout_link3, QString contact_link3, QString link_title3);
    void uploadedInfo();
    void downloadedLibrary();
    void downloadedFriends();
    void dataReadProgress(int read, int total);
    void errorReceived(int errorCode);

private slots:
    void httpRequestFinishedSlot(int requestId, bool error);
    void sslErrorsSlot( const QList<QSslError> & errors );
    void slotAuthenticationRequired();
    void blockingTimeOut();

private:
    int downloadXML(const QString &host, const QString &location, QIODevice *destination);
    int uploadXML(const QString &host, const QString &path, QIODevice *source, QIODevice *destination);
    void handleErrorReceived(int error);

    int httpGetId;
    int version_check_id;
    int info_download_id;
    int info_upload_id;
    int library_download_id;
    int friend_download_id;

    QHttp *http;
    QBuffer *buffer;
    QBuffer *uploadBuffer;
    QString email;
    QString password;
    bool doneTransfer; //Used for blocking transfers

    QDateTime lastFriendUpdate;
    QDateTime lastLibraryUpdate;
};

#endif
