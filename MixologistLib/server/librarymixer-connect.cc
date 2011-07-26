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

#include <QtGui>
#include <QtNetwork>
#include <QDomDocument>
#include <QSettings>
#include <interface/settings.h>
#include "interface/peers.h" //for peers variable
#include "interface/iface.h" //for Control variable and librarymixerconnect

#include "interface/librarymixer-connect.h"
#include "interface/librarymixer-library.h"

#if defined(WIN32) || defined(__CYGWIN__)
#include "wtypes.h"
#include <winioctl.h> //For sleep
#endif

const int BLOCKING_TRANSFER_TIMEOUT = 10000; //10 seconds

LibraryMixerConnect::LibraryMixerConnect()
    :email(""), password("") {
    http = new QHttp(this);

    connect(http, SIGNAL(requestFinished(int, bool)),
            this, SLOT(httpRequestFinishedSlot(int, bool)));
    connect(http, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(sslErrorsSlot(QList<QSslError>)));
    connect(http, SIGNAL(dataReadProgress(int,int)),
            this, SIGNAL(dataReadProgress(int,int)));
    connect(http, SIGNAL(authenticationRequired(const QString &, quint16, QAuthenticator *)),
            this, SLOT(slotAuthenticationRequired()));
}

void LibraryMixerConnect::setLogin(const QString &_email, const QString &_password) {
    email = _email;
    password = _password;
}

void LibraryMixerConnect::setupModeAndHost(QString *host, QHttp::ConnectionMode *mode){
    QSettings settings(*startupSettings, QSettings::IniFormat);
    *host = settings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString();

    if (email != "" || password != "") {
        http->setUser(email, password);
        *mode = QHttp::ConnectionModeHttps;
    } else *mode = QHttp::ConnectionModeHttp;
    if (host->startsWith("http://")) {
        *mode = QHttp::ConnectionModeHttp;
        host->remove(0, 7);
    }
    if (host->startsWith("https://")){
        *mode = QHttp::ConnectionModeHttps;
        host->remove(0, 8);
    }
    if (host->compare(DEFAULT_MIXOLOGY_SERVER, Qt::CaseInsensitive) == 0){
        *host = DEFAULT_MIXOLOGY_SERVER_VALUE;
    }
}

int LibraryMixerConnect::downloadXML(const QString &path,
                                     QIODevice *destination) {
    QString host;
    QHttp::ConnectionMode mode;
    setupModeAndHost(&host, &mode);

    if (email != "" || password != "") {
        http->setUser(email, password);
    }

    http->setHost(host, mode);

    httpGetId = http->get(path, destination);
    return httpGetId;
}

int LibraryMixerConnect::downloadVersion(qlonglong current) {
    buffer = new QBuffer();
    if (!buffer->open(QIODevice::ReadWrite)) return -1;
    QString url = "/api/mixologist_version?current=";
    url.append(QString::number(current));
    version_check_id = downloadXML(url, buffer);
    return version_check_id;
}

int LibraryMixerConnect::downloadInfo() {
    buffer = new QBuffer();
    if (!buffer->open(QIODevice::ReadWrite)) return -1;
    info_download_id = downloadXML("/api/user?id=&name=&checkout_link1=&contact_link1=&link_title1=&checkout_link2=&contact_link2=&link_title2=&checkout_link3=&contact_link3=&link_title3=",
                                   buffer);
    return info_download_id;
}

int LibraryMixerConnect::downloadLibrary(bool blocking) {
    if (lastLibraryUpdate.isNull() || lastLibraryUpdate.secsTo(QDateTime::currentDateTime()) > CONNECT_COOLDOWN) {
        lastLibraryUpdate = QDateTime::currentDateTime();
        buffer = new QBuffer();
        if (!buffer->open(QIODevice::ReadWrite)) return -1;
        library_download_id = downloadXML("/api/library?paginate=false&onlycheckout=true", buffer);
        if (blocking) {
            doneTransfer = false;
            QTimer::singleShot(BLOCKING_TRANSFER_TIMEOUT, this, SLOT(blockingTimeOut()));
            while (!doneTransfer) {
#ifdef WIN32
                Sleep(1000); /* milliseconds */
#else
                usleep(1000000); /* microseconds */
#endif
            }
            return 0;
        } else return library_download_id;
    }
    return -1;
}

int LibraryMixerConnect::downloadFriends(bool blocking) {
    if (lastFriendUpdate.isNull() || lastFriendUpdate.secsTo(QDateTime::currentDateTime()) > CONNECT_COOLDOWN) {
        lastFriendUpdate = QDateTime::currentDateTime();
        buffer = new QBuffer();
        if (!buffer->open(QIODevice::ReadWrite)) return -1;
        friend_download_id = downloadXML("/api/friends?name=&id=&scratch[Mixology_Public_Key]=&scratch[Mixology_Local_IP]=&scratch[Mixology_Local_Port]=&scratch[Mixology_External_IP]=&scratch[Mixology_External_Port]=",
                                         buffer);
        if (blocking) {
            doneTransfer = false;
            QTimer::singleShot(BLOCKING_TRANSFER_TIMEOUT, this, SLOT(blockingTimeOut()));
            while (!doneTransfer) {
                qApp->processEvents(QEventLoop::WaitForMoreEvents);
            }
            return 0;
        } else return friend_download_id;
    }
    return -1;
}

int LibraryMixerConnect::uploadXML(const QString &path,
                                   QIODevice *source,
                                   QIODevice *destination) {

    QHttpRequestHeader header("POST", path);
    header.setValue("Accept", "application/xml");
    header.setValue("content-type", "application/xml");

    QString host;
    QHttp::ConnectionMode mode;
    setupModeAndHost(&host, &mode);
    header.setValue("Host", host);
    if (email != "" && password != "") {
        QString credentials;
        credentials = email + ":" + password;
        header.setValue( "Authorization", QString("Basic ").append(credentials.toLatin1().toBase64()));
    }

    http->setHost(host, mode);

    httpGetId = http->request(header, source, destination);

    return httpGetId;
}

int LibraryMixerConnect::uploadInfo(const int link_to_set, const QString &public_key) {
    PeerDetails ownDetails;
    peers->getPeerDetails(peers->getOwnLibraryMixerId(),ownDetails);

    buffer = new QBuffer();
    if (!buffer->open(QIODevice::ReadWrite)) return -1;
    uploadBuffer = new QBuffer();
    if (!uploadBuffer->open(QIODevice::ReadWrite)) return -1;

    /* We cannot simply perform an ordinary post using &key=value syntax because
       the public key may contain + characters. */
    uploadBuffer->write("<user>");
    if (link_to_set != -1) {
        uploadBuffer->write("<standard_link" + QByteArray::number(link_to_set) + ">");
        uploadBuffer->write(MIXOLOGY_LINK_TITLE);
        uploadBuffer->write("</standard_link" + QByteArray::number(link_to_set) + ">");
    }
    uploadBuffer->write("<scratch>");
    uploadBuffer->write("<Mixology_Local_IP>");
    uploadBuffer->write(ownDetails.localAddr.c_str());
    uploadBuffer->write("</Mixology_Local_IP>");
    uploadBuffer->write("<Mixology_Local_Port>");
    uploadBuffer->write(QByteArray::number(ownDetails.localPort));
    uploadBuffer->write("</Mixology_Local_Port>");
    uploadBuffer->write("<Mixology_External_IP>");
    uploadBuffer->write("[[set_ip]]");
    uploadBuffer->write("</Mixology_External_IP>");
    uploadBuffer->write("<Mixology_External_Port>");
    //Setting the external port to be the same as the internal port for now.
    //External Port is not yet set up here
    uploadBuffer->write(QByteArray::number(ownDetails.localPort));
    uploadBuffer->write("</Mixology_External_Port>");
    uploadBuffer->write("<Mixology_Public_Key>");
    uploadBuffer->write(public_key.toLatin1());
    uploadBuffer->write("</Mixology_Public_Key>");
    uploadBuffer->write("</scratch>");
    uploadBuffer->write("</user>");

    uploadBuffer->seek(0);

    info_upload_id = uploadXML("/api/edit_user", uploadBuffer, buffer);
    return info_upload_id;
}

void LibraryMixerConnect::httpRequestFinishedSlot(int requestId, bool error) {
    if (requestId != httpGetId) return; //skip request completions that aren't related to actual http requests
    if (requestId == version_check_id) {
        if (error) goto versionDownloadError;

        qlonglong version = 0;
        QString description("");
        QString importance("Recommended");

        QDomDocument xml;
        buffer->seek(0);
        if (!xml.setContent(buffer)) goto versionDownloadError;
        buffer->deleteLater();
        QDomElement rootNode = xml.documentElement();
        if (rootNode.tagName() != "versions" ) goto versionDownloadError;
        if (rootNode.firstChildElement("version").firstChildElement("number").isNull()) {
            emit downloadedVersion(1, "", ""); //Version is up to date if there are no version elements
            return;
        }
        version = rootNode.firstChildElement("version").firstChildElement("number").text().toLongLong();
        for (QDomElement versionNode = rootNode.firstChildElement();
                !versionNode.isNull();
                versionNode = versionNode.nextSiblingElement()) {

            if (versionNode.firstChildElement("number").isNull()) goto versionDownloadError;
            if (versionNode.firstChildElement("description").isNull()) goto versionDownloadError;
            if (versionNode.firstChildElement("importance").isNull()) goto versionDownloadError;
            description.append(versionNode.firstChildElement("number").text());
            description.append(":\n");
            description.append(versionNode.firstChildElement("description").text());
            description.append("\n\n");
            if (versionNode.firstChildElement("importance").text() == "Essential") importance = "Essential";
        }

        emit downloadedVersion(version, description, importance);
        return;
    } else if (requestId == info_download_id) {
        if (error) goto infoDownloadError;

        QDomDocument xml;
        buffer->seek(0);
        if (!xml.setContent(buffer)) goto infoDownloadError;
        buffer->deleteLater();
        QDomElement rootNode = xml.documentElement();
        if (rootNode.tagName() != "user" ) goto infoDownloadError;
        QDomElement idNode = rootNode.firstChildElement("id");
        if (idNode.isNull()) goto infoDownloadError;
        QDomElement nameNode = rootNode.firstChildElement("name");
        if (nameNode.isNull()) goto infoDownloadError;
        QDomElement checkout1Node = rootNode.firstChildElement("checkout_link1");
        if (checkout1Node.isNull()) goto infoDownloadError;
        QDomElement contact1Node = rootNode.firstChildElement("contact_link1");
        if (contact1Node.isNull()) goto infoDownloadError;
        QDomElement title1Node = rootNode.firstChildElement("link_title1");
        if (title1Node.isNull()) goto infoDownloadError;
        QDomElement checkout2Node = rootNode.firstChildElement("checkout_link2");
        if (checkout2Node.isNull()) goto infoDownloadError;
        QDomElement contact2Node = rootNode.firstChildElement("contact_link2");
        if (contact2Node.isNull()) goto infoDownloadError;
        QDomElement title2Node = rootNode.firstChildElement("link_title2");
        if (title2Node.isNull()) goto infoDownloadError;
        QDomElement checkout3Node = rootNode.firstChildElement("checkout_link3");
        if (checkout3Node.isNull()) goto infoDownloadError;
        QDomElement contact3Node = rootNode.firstChildElement("contact_link3");
        if (contact3Node.isNull()) goto infoDownloadError;
        QDomElement title3Node = rootNode.firstChildElement("link_title3");
        if (title3Node.isNull()) goto infoDownloadError;

        emit downloadedInfo(nameNode.text(), idNode.text().toInt(),
                            checkout1Node.text(), contact1Node.text(), title1Node.text(),
                            checkout2Node.text(), contact2Node.text(), title2Node.text(),
                            checkout3Node.text(), contact3Node.text(), title3Node.text());
        return;
    } else if (requestId == info_upload_id) {
        if (error) goto infoUploadError;

        QDomDocument xml;
        QDomDocument uploaded_xml;
        buffer->seek(0);
        uploadBuffer->seek(0);
        if (!xml.setContent(buffer)) goto infoUploadError;
        if (!uploaded_xml.setContent(uploadBuffer)) goto infoUploadError;
        buffer->deleteLater();
        uploadBuffer->deleteLater();
        QDomElement rootNode = xml.documentElement();
        QDomElement uploaded_rootNode = uploaded_xml.documentElement();
        if (rootNode.tagName() != "user" ) goto infoUploadError;
        if (uploaded_rootNode.tagName() != "user" ) goto infoUploadError;

        QDomElement scratchNode = rootNode.firstChildElement("scratch");
        if (scratchNode.isNull()) goto infoUploadError;
        QDomElement uploaded_scratchNode = uploaded_rootNode.firstChildElement("scratch");
        if (uploaded_scratchNode.isNull()) goto infoUploadError;

        QDomElement localIPNode = scratchNode.firstChildElement("Mixology_Local_IP");
        QDomElement uploaded_localIPNode = uploaded_scratchNode.firstChildElement("Mixology_Local_IP");
        if (localIPNode.isNull()) goto infoUploadError;
        if (uploaded_localIPNode.isNull()) goto infoUploadError;
        if (localIPNode.text() != uploaded_localIPNode.text()) goto infoUploadError;

        QDomElement localPortNode = scratchNode.firstChildElement("Mixology_Local_Port");
        QDomElement uploaded_localPortNode = uploaded_scratchNode.firstChildElement("Mixology_Local_Port");
        if (localPortNode.isNull()) goto infoUploadError;
        if (uploaded_localPortNode.isNull()) goto infoUploadError;
        if (localPortNode.text() != uploaded_localPortNode.text()) goto infoUploadError;

        //No need to validate, because we asked the server to set it for us.
        QDomElement externalIPNode = scratchNode.firstChildElement("Mixology_External_IP");
        if (externalIPNode.isNull()) goto infoUploadError;

        QDomElement externalPortNode = scratchNode.firstChildElement("Mixology_External_Port");
        QDomElement uploaded_externalPortNode = uploaded_scratchNode.firstChildElement("Mixology_External_Port");
        if (externalPortNode.isNull()) goto infoUploadError;
        if (uploaded_externalPortNode.isNull()) goto infoUploadError;
        if (externalPortNode.text() != uploaded_externalPortNode.text()) goto infoUploadError;

        QDomElement publicNode = scratchNode.firstChildElement("Mixology_Public_Key");
        QDomElement uploaded_publicNode = uploaded_scratchNode.firstChildElement("Mixology_Public_Key");
        if (publicNode.isNull()) goto infoUploadError;
        if (uploaded_publicNode.isNull()) goto infoUploadError;
        if (publicNode.text() != uploaded_publicNode.text()) goto infoUploadError;

        //Setting the external port to be the same as the internal port for now.
        peers->setExtAddress(peers->getOwnLibraryMixerId(), externalIPNode.text().toStdString(), externalPortNode.text().toInt());

        emit uploadedInfo();
        return;
    } else if (requestId == library_download_id) {
        if (error) goto libraryDownloadError;
        buffer->seek(0);
        LibraryMixerLibraryManager::mergeLibrary(*buffer);
        buffer->deleteLater();
        emit downloadedLibrary();
        doneTransfer = true;
        return;
    } else if (requestId == friend_download_id) {
        if (error) goto friendDownloadError;

        QDomDocument xml;
        buffer->seek(0);
        if (!xml.setContent(buffer)) goto friendDownloadError;
        buffer->deleteLater();
        QDomElement rootNode = xml.documentElement();
        if (rootNode.tagName() != "friends" ) goto friendDownloadError;
        for (QDomElement friendNode = rootNode.firstChildElement("user");
                !friendNode.isNull();
                friendNode = friendNode.nextSiblingElement("user")) {

            std::string local_ip, external_ip;
            QString name, certificate;
            int librarymixer_id, local_port, external_port;

            if (!friendNode.firstChildElement("name").isNull()) {
                name = friendNode.firstChildElement("name").text();
            } else goto friendDownloadError;
            if (!friendNode.firstChildElement("id").isNull()) {
                librarymixer_id = friendNode.firstChildElement("id").text().toInt();
            } else goto friendDownloadError;
            if (!friendNode.firstChildElement("scratch").firstChildElement("Mixology_Local_IP").isNull()) {
                local_ip = friendNode.firstChildElement("scratch").firstChildElement("Mixology_Local_IP").text().toStdString();
            } else goto friendDownloadError;
            if (!friendNode.firstChildElement("scratch").firstChildElement("Mixology_Local_Port").isNull()) {
                local_port = friendNode.firstChildElement("scratch").firstChildElement("Mixology_Local_Port").text().toInt();
            } else goto friendDownloadError;
            if (!friendNode.firstChildElement("scratch").firstChildElement("Mixology_External_IP").isNull()) {
                external_ip = friendNode.firstChildElement("scratch").firstChildElement("Mixology_External_IP").text().toStdString();
            } else goto friendDownloadError;
            if (!friendNode.firstChildElement("scratch").firstChildElement("Mixology_External_Port").isNull()) {
                external_port = friendNode.firstChildElement("scratch").firstChildElement("Mixology_External_Port").text().toInt();
            } else goto friendDownloadError;
            if (!friendNode.firstChildElement("scratch").firstChildElement("Mixology_Public_Key").isNull()) {
                certificate = friendNode.firstChildElement("scratch").firstChildElement("Mixology_Public_Key").text();
            } else goto friendDownloadError;

            peers->addUpdateFriend(librarymixer_id, certificate, name);
            peers->setExtAddress(librarymixer_id, external_ip, external_port);
            peers->setLocalAddress(librarymixer_id, local_ip, local_port);
            //For now, we are doing nothing to remove already connected friends that have been removed on LibraryMixexr
        }
        emit downloadedFriends();
        doneTransfer = true;
        return;
    }
versionDownloadError:
    handleErrorReceived(version_download_error);
    return;
infoDownloadError:
    std::cerr << http->errorString().toStdString();
    handleErrorReceived(info_download_error);
    return;
infoUploadError:
    handleErrorReceived(info_upload_error);
    return;
libraryDownloadError:
    handleErrorReceived(library_download_error);
    return;
friendDownloadError:
    handleErrorReceived(friend_download_error);
    return;
}

void LibraryMixerConnect::handleErrorReceived(int error) {
    buffer->deleteLater();
    if (httpGetId == info_upload_id) uploadBuffer->deleteLater(); //it seems like there should be a better way
    emit(errorReceived(error));
}

void LibraryMixerConnect::sslErrorsSlot(const QList<QSslError> &errors) {
    /* Fix for the weird erroneous QSslErrors set to NoError that showed up after upgrading to OpenSSL 1.0.0a
       from the old custom Retroshare version */
    bool realError = false;
    foreach (const QSslError &error, errors) {
        if (error.error() != QSslError::NoError) {
            realError = true;
            break;
        }
    }
    if (realError) {
        buffer->deleteLater();
        if (httpGetId == info_upload_id) uploadBuffer->deleteLater(); //it seems like there should be a better way
        emit(errorReceived(ssl_error));
    } else {
        http->ignoreSslErrors();
    }
}

void LibraryMixerConnect::slotAuthenticationRequired() {
    http->abort();
    handleErrorReceived(bad_login_error);
}

void LibraryMixerConnect::blockingTimeOut() {
    doneTransfer = true;
}
