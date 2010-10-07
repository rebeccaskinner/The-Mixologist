/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
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

#include <interface/init.h>
#include <interface/iface.h>
#include <interface/peers.h>
#include <gui/Util/Helpers.h> //for rot13
#include <gui/Util/SettingsUtil.h>
#include <gui/Util/OSHelpers.h>
#include <gui/StartDialog.h>
#include <version.h>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <iostream>


//Setting up global extern for iface.h
LibraryMixerConnect *librarymixerconnect = NULL;

//Setting up global extern for settings.h
QString *startupSettings;
QString *mainSettings;
QString *savedTransfers;

StartDialog::StartDialog(NotifyQt *_notify, QWidget *parent, Qt::WFlags flags)
    : QMainWindow(parent, flags), notify(_notify) {
    loadedOk = false;
    /* Invoke Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    startupSettings = new QString(Init::getBaseDirectory(true).append("startup.ini"));
#if defined(Q_WS_WIN)
    QFile file(*startupSettings);
    if (!file.exists()) {
        QMessageBox::information(this, "The Mixologist", "<p>It looks like this is the first time you've started the Mixologist!</p><p>A box is probably going to popup after you login telling you Windows Firewall has blocked the Mixologist.</p><p><b>On the box that pops up, be absolutely sure to allow the Mixologist to connect through Windows Firewall, or else it will not work.</b></p>");
    }
#endif
    SettingsUtil::loadWidgetInformation(this, *startupSettings);

    ui.loadEmail->setFocus();

    connect(ui.loadButton, SIGNAL(clicked()), this, SLOT(checkVersion()));
    connect(ui.loadPassword, SIGNAL(returnPressed()), ui.loadButton, SLOT(click()));
    connect(ui.loadEmail, SIGNAL(returnPressed()), ui.loadButton, SLOT(click()));

    connect(ui.loadEmail, SIGNAL(textChanged(QString)), this, SLOT(edited()));
    connect(ui.loadPassword, SIGNAL(textChanged(QString)), this, SLOT(edited()));

    librarymixerconnect = new LibraryMixerConnect();
    connect(librarymixerconnect, SIGNAL(downloadedVersion(qlonglong,QString, QString)), this, SLOT(downloadedVersion(qlonglong,QString,QString)));
    connect(librarymixerconnect, SIGNAL(downloadedInfo(QString,int,QString,QString,QString,QString,QString,QString,QString,QString,QString)),
            this, SLOT(downloadedInfo(QString,int,QString,QString,QString,QString,QString,QString,QString,QString,QString)));
    connect(librarymixerconnect, SIGNAL(uploadedInfo()), this, SLOT(downloadLibrary()));
    connect(librarymixerconnect, SIGNAL(downloadedLibrary()), this, SLOT(downloadFriends()));
    connect(librarymixerconnect, SIGNAL(downloadedFriends()), this, SLOT(finishLoading()));
    connect(librarymixerconnect, SIGNAL(dataReadProgress(int,int)), this, SLOT(updateDataReadProgress(int,int)));
    connect(librarymixerconnect, SIGNAL(errorReceived(int)), this, SLOT(errorReceived(int)));

    ui.progressBar->setVisible(false);
    ui.loadButton->setEnabled(false);

    setWindowTitle("Login");

    QSettings settings(*startupSettings, QSettings::IniFormat, this);
    QString email(rot13(settings.value("DefaultEmail", "").toString()));
    QString password(rot13(settings.value("DefaultPassword", "").toString()));
    skip_to_version = settings.value("SkipToVersion", VERSION).toLongLong();

    if (!email.isEmpty()) {
        ui.loadEmail->setText(email);
        if (!password.isEmpty()) {
            ui.loadPassword->setText(password);
            ui.autoBox->setChecked(true);
        }
    }

    //Disable box until fully ready for user interaction
    ui.groupBox->setEnabled(false);
    ui.loadButton->setEnabled(false);
    show();

    if (!getMixologyLinksAssociated()) {
        if (QMessageBox::question(this,
                                  "Associate Links",
                                  "Mixology links are currently not set up to be opened by the Mixologist.\nSet up now? (Highly recommended)\n\nClicking Mixology links on LibraryMixer will NOT work until they are set up.",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                == QMessageBox::Yes) {
            if (!setMixologyLinksAssociated()) {
                QMessageBox::warning (this,
                                      "Problem writing to registry",
                                      "Unable to set Mixology links to open with the Mixologist because you do not have administrative rights.\n\nStart the Mixologist by right clicking and choosing \"Run as administrator\" to set up the links.",
                                      QMessageBox::Ok);
                exit(1);
            }

        }

    }

    //If we set the auto login box earlier in the constructor, auto login now.
    if (ui.autoBox->isChecked()) checkVersion();
    else {
        ui.groupBox->setEnabled(true);
        ui.loadButton->setEnabled(true);
        if (!email.isEmpty()) {
            if (password.isEmpty()) {
                ui.loadPassword->setFocus();
            }
        }
    }
}

void StartDialog::edited() {
    if (ui.loadEmail->text() != "" && ui.loadPassword->text() != "") {
        ui.loadButton->setEnabled(true);
    } else {
        ui.loadButton->setEnabled(false);
    }
}

//First step after clicking Log In button is to check client version
void StartDialog::checkVersion() {
    ui.groupBox->setEnabled(false);
    ui.loadButton->setEnabled(false);

    ui.loadStatus->setText("Connecting");
    ui.progressBar->setVisible(true);
    librarymixerconnect->downloadVersion(skip_to_version);
}

void StartDialog::downloadedVersion(qlonglong _version, QString description, QString importance) {
    if (skip_to_version < _version) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("The Mixologist - Good news!");
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("A new version of the the Mixologist is available. Download now?");
        QPushButton* Yes = msgBox.addButton(QMessageBox::Yes);
        QPushButton* No = msgBox.addButton(QMessageBox::No);
        QPushButton* Never = NULL;
        if (importance == "Essential"){
            msgBox.setInformativeText("This is an essential update.\nYou will not be able to connect until you have upgraded.");
        } else {
            Never = msgBox.addButton("Skip Version", QMessageBox::NoRole);
        }
        msgBox.setDefaultButton(Yes);
        QString details("Changes made since your version:\n");
        msgBox.setDetailedText(details.append(description));
        msgBox.exec();
        if (msgBox.clickedButton() == Yes) {
            QDesktopServices::openUrl(QUrl(QString("http://librarymixer.com/download/mixologist/")));
            exit(1);
        } else if (msgBox.clickedButton() == No && importance == "Essential"){
            exit(1);
        } else if (Never != NULL && msgBox.clickedButton() == Never) {
            QSettings settings(*startupSettings, QSettings::IniFormat, this);
            settings.setValue("SkipToVersion", _version);
            SettingsUtil::saveWidgetInformation(this, *startupSettings);
        }

    }
    downloadInfo();
}

/*Second step after clicking Log In button is to query server for:
  id
  name
  communications links
*/

void StartDialog::downloadInfo() {
    QString email = ui.loadEmail->text();
    QString password = ui.loadPassword->text();
    librarymixerconnect->setLogin(email, password);

    ui.loadStatus->setText("Downloading user info");
    ui.progressBar->setValue(20);
    librarymixerconnect->downloadInfo();
}

void StartDialog::downloadedInfo(QString name, int librarymixer_id,
                                 QString checkout_link1, QString contact_link1, QString link_title1,
                                 QString checkout_link2, QString contact_link2, QString link_title2,
                                 QString checkout_link3, QString contact_link3, QString link_title3) {
    bool savePassword = ui.autoBox -> checkState();

    //First time we have the librarymixer_id and can initialize user directory
    Init::loadUserDir(librarymixer_id);
    //First time we have verified that the credentials are good, and can save them to the autoload
    QSettings settings(*startupSettings, QSettings::IniFormat, this);
    settings.setValue("DefaultEmail", rot13(ui.loadEmail->text()));
    if (savePassword) settings.setValue("DefaultPassword", rot13(ui.loadPassword->text()));

    //Now that user dir, we can setup the other settings object
    mainSettings = new QString(Init::getUserDirectory(true).append("settings.ini"));
    savedTransfers = new QString(Init::getUserDirectory(true).append("transfers.ini"));

    //Check communications links and make sure at least one is set to librarymixer. If not, prompt to set.
    int link_to_set = -1;
    if ((checkout_link1 != MIXOLOGY_CHECKOUT_LINK || contact_link1 != MIXOLOGY_CONTACT_LINK) &&
        (checkout_link2 != MIXOLOGY_CHECKOUT_LINK || contact_link2 != MIXOLOGY_CONTACT_LINK) &&
        (checkout_link3 != MIXOLOGY_CHECKOUT_LINK || contact_link3 != MIXOLOGY_CONTACT_LINK)) {
        if (QMessageBox::question(this,
                                  "Setup your account",
                                  "Your LibraryMixer account has not yet been setup for sharing with the Mixologist. Your friends will not be able to check out your stuff using the Mixologist until it is set up.\nSet it up now? (highly recommended)",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                == QMessageBox::Yes) {
            if (checkout_link1 == "") link_to_set = 1;
            else if (checkout_link2 == "") link_to_set = 2;
            else if (checkout_link3 == "") link_to_set = 3;
            else {
                QMessageBox msgBox;
                msgBox.setWindowTitle("Choose a slot");
                msgBox.setIcon(QMessageBox::Question);
                if (link_title1 == STANDARD_LINK_TITLE) link_title1 = "Message";
                if (link_title2 == STANDARD_LINK_TITLE) link_title2 = "Message";
                if (link_title3 == STANDARD_LINK_TITLE) link_title3 = "Message";
                msgBox.setText("All 3 of your sharing methods are already in use on your LibraryMixer account. Choose one to replace:\n" +
                               link_title1 +
                               "\n" +
                               link_title2 +
                               "\n" +
                               link_title3);
                QAbstractButton *button1 = msgBox.addButton(link_title1, QMessageBox::AcceptRole);
                QAbstractButton *button2 = msgBox.addButton(link_title2, QMessageBox::AcceptRole);
                QAbstractButton *button3 = msgBox.addButton(link_title3, QMessageBox::AcceptRole);
                msgBox.addButton(QMessageBox::Cancel);
                msgBox.exec();
                if (msgBox.clickedButton() == button1) link_to_set = 1;
                else if (msgBox.clickedButton() == button2) link_to_set = 2;
                else if (msgBox.clickedButton() == button3) link_to_set = 3;
            }
        }
    }

    ui.loadStatus->setText("Initializing encryption");
    ui.progressBar->setValue(30);

    QString public_key = Init::InitEncryption(librarymixer_id);
    if (public_key.isEmpty()) {
        QMessageBox::warning (this,
                              "Something has gone wrong",
                              "Unable to start encryption",
                              QMessageBox::Ok);
        exit(1);
    }

    //Must start server now so that we can get the IP address to upload in the next step
    Init::createControl(name, *notify);
    ui.loadStatus->setText("Updating info on server");
    ui.progressBar->setValue(50);
    librarymixerconnect->uploadInfo(link_to_set, public_key);
}

//Fourth step after clicking Log In button is to download library
void StartDialog::downloadLibrary() {
    ui.loadStatus->setText("Downloading your library");
    ui.progressBar->setValue(60);

    librarymixerconnect->downloadLibrary();
}

//Fifth step after clicking Log In button is to download friend list
void StartDialog::downloadFriends() {
    ui.loadStatus->setText("Downloading friend list");
    ui.progressBar->setValue(80);

    librarymixerconnect->downloadFriends();
}


void StartDialog::finishLoading() {
    control->StartupMixologist();
    loadedOk = true;
    close();
}

void StartDialog::updateDataReadProgress(int bytesRead, int totalBytes) {
    if (ui.progressBar->value() < 20) { //Step 1
        ui.progressBar->setValue(bytesRead/totalBytes);
    } else if (20 <= ui.progressBar->value() && ui.progressBar->value() < 30) { //Step 2.0
        ui.progressBar->setValue(20 + (bytesRead/totalBytes));
    } else if (30 <= ui.progressBar->value() && ui.progressBar->value() < 50) { //Step 2.5
        ui.progressBar->setValue(30 + (bytesRead/totalBytes));
    } else if (50 <= ui.progressBar->value() && ui.progressBar->value() < 60) { //Step 3
        ui.progressBar->setValue(50 + (bytesRead/totalBytes));
    } else if (60 <= ui.progressBar->value() && ui.progressBar->value() < 80) { //Step 4
        ui.progressBar->setValue(60 + (bytesRead/totalBytes));
    } else if (80 <= ui.progressBar->value() && ui.progressBar->value() < 100) { //Step 5
        ui.progressBar->setValue(80 + (bytesRead/totalBytes));
    }
}

void StartDialog::errorReceived(int errorCode) {
    if (errorCode == LibraryMixerConnect::version_download_error) {
        QMessageBox::warning (this,
                              "Something has gone wrong",
                              "Unable to download version update info!",
                              QMessageBox::Ok);
        downloadInfo();
    } else if (errorCode == LibraryMixerConnect::ssl_error) {
        ui.loadStatus->setText("Unable to connect");
    } else if (errorCode == LibraryMixerConnect::bad_login_error || errorCode == LibraryMixerConnect::info_download_error) {
        ui.loadStatus->setText("There was a problem\nwith your login");
    } else {
        QMessageBox::warning (this,
                              "The Mixologist",
                              "Ran into a problem while starting up, give it another try.",
                              QMessageBox::Ok);
        exit(1);
    }

    ui.groupBox->setEnabled(true);
    ui.loadButton->setEnabled(true);

    ui.progressBar->setValue(0);
    ui.progressBar->setVisible(false);

    this->show();
}

void StartDialog::closeEvent (QCloseEvent *event) {
    {
        QSettings settings(*startupSettings, QSettings::IniFormat, this);
        settings.setValue("Version", VERSION);
        SettingsUtil::saveWidgetInformation(this, *startupSettings);
    }//flush the QSettings object so it saves

    if (loadedOk) {
        this->deleteLater();
        QWidget::closeEvent(event);
    } else {
        exit(1);
    }
}
