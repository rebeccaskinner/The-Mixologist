/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006, crypton
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


#include "PopupChatDialog.h"
#include "MainWindow.h"
#include "LibraryDialog.h"
#include <gui/Util/GuiSettingsUtil.h>
#include <gui/Util/Helpers.h> //for the recursiveFileAdd on drop function

#include "interface/peers.h"
#include "interface/msgs.h"
#include "interface/files.h"

#include <time.h>

#include <QMessageBox>

/*****
 * #define CHAT_DEBUG 1
 *****/

/** Default constructor */
PopupChatDialog::PopupChatDialog(int _librarymixer_id, QWidget *parent, Qt::WFlags flags)
    : QMainWindow(parent, flags), librarymixer_id(_librarymixer_id), requestedItemId(0), online_status(true)

{
    friendName = peers->getPeerName(librarymixer_id);

    setAttribute(Qt::WA_DeleteOnClose);

    /* Invoke Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    GuiSettingsUtil::loadWidgetInformation(this);

    last_status_send_time = 0 ;

    /* Hide Avatar frame */
    showAvatarFrame(false);

    connect(ui.avatarFrameButton, SIGNAL(toggled(bool)), this, SLOT(showAvatarFrame(bool)));
    connect(ui.chattextEdit, SIGNAL(textChanged()), this, SLOT(checkChat()));
    connect(ui.sendButton, SIGNAL(clicked()), this, SLOT(sendChat()));
    connect(ui.sendFileButton, SIGNAL(clicked()), this, SLOT(selectFiles()));

    //Build drop down menu for requests
    QMenu *requestMenu = new QMenu(this);
    QAction *matchToMessage = new QAction(tr("In the future, auto send a message when requested"), requestMenu);
    connect(matchToMessage, SIGNAL(triggered()), this, SLOT(setMatchToMessage()));
    requestMenu->addAction(matchToMessage);

    QAction *matchToFiles = new QAction(tr("In the future, auto send one or more files when requested"), requestMenu);
    connect(matchToFiles, SIGNAL(triggered()), this, SLOT(setMatchToFiles()));
    requestMenu->addAction(matchToFiles);

    QAction *matchToLend = new QAction(tr("In the future, auto lend one or more files when requested"), requestMenu);
    connect(matchToLend, SIGNAL(triggered()), this, SLOT(setMatchToLend()));
    requestMenu->addAction(matchToLend);

    requestMenu->addSeparator();
    QAction *matchToChat = new QAction(tr("In the future, open a chat window and never ask again to set an auto response"), requestMenu);
    connect(matchToChat, SIGNAL(triggered()), this, SLOT(setMatchToChat()));
    requestMenu->addAction(matchToChat);

    requestMenu->addSeparator();

    QAction *clearRequestAct = new QAction(tr("Clear request"), requestMenu);
    connect(clearRequestAct, SIGNAL(triggered()), this, SLOT(clearRequest()));
    requestMenu->addAction(clearRequestAct);

    ui.requestButton->setMenu(requestMenu);
    connect(ui.requestButton, SIGNAL(clicked()), ui.requestButton, SLOT(showMenu()));

    time_stamp_timer = new QTimer(this);
    connect(time_stamp_timer, SIGNAL(timeout()), this, SLOT(addTimeStamp()));

    // Create the status bar
    resetStatusBar();

    setWindowIcon(QIcon(QString(":/Images/Chat.png")));
    setWindowTitle(friendName);

    //So web links can be opened
    ui.textBrowser->setOpenExternalLinks(true);
    //So that we can catch enter presses
    ui.chattextEdit->installEventFilter(this);

    ui.chattextEdit->setFocus(Qt::OtherFocusReason);

    //When there are no requests, make the request button hidden
    ui.requestButton->setVisible(false);

    //Avatar stuff not yet implemented
    ui.avatarFrameButton->hide();
    //updateAvatar();
    //updatePeerAvatar();

#if defined(Q_WS_WIN)
    /* Due to QT's horrible handling of cross-platform fonts, we as much as possible try to use the default font.
       This in general selects a font that works well on each platform.
       However, the default font for the QTextBrowser and QPlainTextEdit are only 8 point on Windows, which is too small.
       Default font on OS X and Linux look fine.
       Therefore, we make Windows a special-case and make the fonts bigger. */
    QFont chattextEditFont = ui.chattextEdit->font();
    chattextEditFont.setPointSize(10);
    ui.chattextEdit->setFont(chattextEditFont);
    QFont textBrowserFont = ui.textBrowser->font();
    textBrowserFont.setPointSize(10);
    ui.textBrowser->setFont(textBrowserFont);
#endif
}

void PopupChatDialog::resetStatusBar() {
    statusBar()->showMessage("Encrypted chat with " + friendName) ;
}

void PopupChatDialog::updateStatusTyping() {
    if (time(NULL) - last_status_send_time > 5) {   // limit 'peer is typing' packets to at most every 10 sec
        msgs->sendStatusString(peers->findCertByLibraryMixerId(librarymixer_id), peers->getOwnName() + " is typing...");
        last_status_send_time = time(NULL) ;
    }
}

void PopupChatDialog::updateStatusString(const QString &status_string) {
    statusBar()->showMessage(status_string,5000) ; // displays info for 5 secs.

    QTimer::singleShot(5000,this,SLOT(resetStatusBar())) ;
}

void PopupChatDialog::addMsgFromFriend(ChatInfo *ci) {
    addChatMsg(ci->msg, false);
    if (ci->chatflags & CHAT_AVATAR_AVAILABLE) {
        updatePeerAvatar() ;
    }
    resetStatusBar() ;
}

void PopupChatDialog::addSysMsg(const QString &text) {
    QString display = "<div style=\"color:#1D84C9\"><strong>";
    display = display + text;
    display = display + "</strong></div>";

    addText(display);
}

void PopupChatDialog::insertRequestEvent(int event, unsigned int item_id) {
    QString message;
    QString requestedItemName;
    LibraryMixerItem* item;

    switch(event) {
        case NOTIFY_TRANSFER_CHAT:
            requestedItemName = files->getLibraryMixerItem(item_id)->title();
            message = friendName + " is interested in getting " +
                      requestedItemName + " from you.";
            break;
        case NOTIFY_TRANSFER_MESSAGE:
            item = files->getLibraryMixerItem(item_id);
            requestedItemName = item->title();
            message = "Auto response for '" + requestedItemName + "': " + item->message();
            linkify(message);
            break;
        case NOTIFY_TRANSFER_LENT:
            requestedItemName = files->getLibraryMixerItem(item_id)->title();
            message = friendName + " is interested in getting " +
                      requestedItemName + " but it's currently lent to " +
                      peers->getPeerName(files->getLibraryMixerItem(item_id)->lentTo()) + ".";
            break;
        case NOTIFY_TRANSFER_UNMATCHED:
            requestedItemId = item_id;
            requestedItemName = files->getLibraryMixerItem(item_id)->title();
            ui.requestLabel->setText("'" + requestedItemName + "'' requested");
            ui.requestButton->setVisible(true);
            message = friendName + " is interested in getting " +
                      requestedItemName + " from you.";
            break;
        case NOTIFY_TRANSFER_BROKEN_MATCH:
            requestedItemId = item_id;
            requestedItemName = files->getLibraryMixerItem(item_id)->title();
            ui.requestLabel->setText("'" + requestedItemName + "'' requested");
            ui.requestButton->setVisible(true);
            message = friendName + " is interested in getting " +
                      requestedItemName + " from you. You marked to automatically send a file, but the marked file has gone missing.";
            break;
        case NOTIFY_TRANSFER_NO_SUCH_ITEM:
            message = friendName + " requested something from you that isn't in your library on LibraryMixer.";
            break;
        default:
            return;
    }
    addSysMsg(message);
}

void PopupChatDialog::insertTransferEvent(int event, const QString &transfer_name, const QString &extra_info) {
    QString message;
    switch(event) {
        case NOTIFY_TRANSFER_CHAT:
            message = friendName + " has requested friends to ask personally for " +
                      transfer_name + ".";
            break;
        case NOTIFY_TRANSFER_MESSAGE:
            message = "Auto response for '" + transfer_name +
                      "': " + extra_info;
            linkify(message);
            break;
        case NOTIFY_TRANSFER_LENT:
            message = friendName + " has currently lent " +
                      transfer_name + " out to somebody.";
            break;
        case NOTIFY_TRANSFER_UNMATCHED:
            message = "Sent " + friendName + " a message that you're interested in' " +
                      transfer_name + ", but you can type more here if you want.";
            break;
        case NOTIFY_TRANSFER_BROKEN_MATCH:
            message = friendName + " marked " +
                      transfer_name + " to automatically send a file, but the file marked has gone missing. You can still ask for it personally, or bother " +
                      friendName + " to fix it.";
            break;
        case NOTIFY_TRANSFER_NO_SUCH_ITEM:
            message = friendName + "'s Mixologist hasn't heard of an item called " +
                      transfer_name + ".  Did you click an outdated link?  You can still ask for it personally.";
            break;
        default:
            return;
    }
    addSysMsg(message);
}

void PopupChatDialog::insertUserOptional(int code, QString input) {
    QString message;
    switch(code) {
        case NotifyBase::NOTIFY_USER_REQUEST:
            message = "Requested '" + input +
                      "' from " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_FILE_REQUEST:
            message = "Auto response sending: '" + input +
                      "' to " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_FILE_RESPONSE:
            message = "Auto response receiving: '" + input +
                      "' from " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_LEND_OFFERED:
            message = "Auto response offer to lend: '" + input +
                      "' to " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_BORROW_DECLINED:
            message = "Decided not to borrow '" + input +
                      "' from " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_BORROW_ACCEPTED:
            message = "Borrowing '" + input +
                      "' from " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_SUGGEST_WAITING:
            message = "Reading file(s) to send to " + friendName +
                      ". Will send a download invitation automatically as soon as finished reading.";
            break;
        case NotifyBase::NOTIFY_USER_SUGGEST_SENT:
            message = "Sent an invitation to get '" + input +
                      "' to " + friendName + ".";
            break;
        case NotifyBase::NOTIFY_USER_SUGGEST_RECEIVED:
            message = "Received an invitation to get '" + input +
                      "' from " + friendName + ".";
            break;
        default:
            return;
    }
    addSysMsg(message);
}

void PopupChatDialog::setOnlineStatus(bool status) {
    if (status != online_status) {
        if (status) addSysMsg("Reconnected to " + friendName + ".");
        else addSysMsg("Connection to " + friendName + " has been lost.");
        online_status = status;
        ui.sendButton->setEnabled(status);
    }
}

void PopupChatDialog::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void PopupChatDialog::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QStringList paths;
    foreach(QUrl url, urls) {
        if (url.scheme() != "file") return;
        QString fileName = url.toLocalFile();
        if (fileName.isEmpty()) continue;
        paths << recursiveFileAdd(fileName);
    }

    show();
    raise();
    sendFiles(paths);
}

void PopupChatDialog::checkChat() {
    /* If length is greater than 10000, truncate the text.
       Not sure if this is necessary in the long run, but currently underlying functions will
       crash or not deliver if the chat message is too long. */
    if (ui.chattextEdit->toPlainText().length() > 10000) {
        ui.chattextEdit->setPlainText(ui.chattextEdit->toPlainText().left(10000));
        QTextCursor cursor = ui.chattextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui.chattextEdit->setTextCursor(cursor);
    }
    updateStatusTyping() ;
}


void PopupChatDialog::sendChat() {
    PeerDetails detail;
    QString message = ui.chattextEdit->toPlainText();
    if (message.isEmpty()) return;
    if (!peers->getPeerDetails(librarymixer_id, detail)) {
        addSysMsg("Catastrophic fail! Unable to load friend's details to send the message.");
        return;
    }
    if (detail.state == FCS_CONNECTED_TCP ||
        detail.state == FCS_CONNECTED_UDP) {
        addChatMsg(message, true);
        msgs->ChatSend(detail.id, message);
        ui.chattextEdit->clear();
    } else {
        addSysMsg("Friend offline, could not send");
    }
}

void PopupChatDialog::showAvatarFrame(bool show) {
    if (show) {
        ui.avatarframe->setVisible(true);
        ui.avatarFrameButton->setChecked(true);
        ui.avatarFrameButton->setToolTip(tr("Hide Avatar"));
        ui.avatarFrameButton->setIcon(QIcon(tr(":Images/HideFrame.png")));
    } else {
        ui.avatarframe->setVisible(false);
        ui.avatarFrameButton->setChecked(false);
        ui.avatarFrameButton->setToolTip(tr("Show Avatar"));
        ui.avatarFrameButton->setIcon(QIcon(tr(":Images/ShowFrame.png")));
    }
}

void PopupChatDialog::updatePeerAvatar() {
    unsigned char *data = NULL;
    int size = 0 ;

    msgs->getAvatarData(peers->findCertByLibraryMixerId(librarymixer_id),data,size);

    if (size == 0) {
        return ;
    }

    // set the image
    QPixmap pix ;
    pix.loadFromData(data,size,"JPG") ;
    ui.avatarlabel->setPixmap(pix); // writes image into ba in JPG format

    delete[] data ;
}

void PopupChatDialog::updateAvatar() {
    unsigned char *data = NULL;
    int size = 0 ;

    msgs->getOwnAvatarData(data,size);

    // set the image
    QPixmap pix ;
    pix.loadFromData(data,size,"JPG") ;
    ui.myavatarlabel->setPixmap(pix); // writes image into ba in JPG format

    delete[] data ;
}

void PopupChatDialog::getAvatar() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load File", QDir::homePath(), "Pictures (*.png *.xpm *.jpg)");

    if (!fileName.isEmpty()) {
        QPixmap picture = QPixmap(fileName).scaled(82,82, Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

        // send avatar down the pipe for other peers to get it.
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        picture.save(&buffer, "JPG"); // writes image into ba in JPG format

        msgs->setOwnAvatarData((unsigned char *)(ba.data()),ba.size()) ;    // last char 0 included.

        updateAvatar() ;
    }
}

bool PopupChatDialog::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            sendChat();
            return true;
        }
    }
    // standard event processing
    return QObject::eventFilter(obj, event);
}

void PopupChatDialog::closeEvent(QCloseEvent *) {
    emit(closeChat(librarymixer_id));
}

void PopupChatDialog::setMatchToMessage() {
    if (requestedItemId != 0) {
        bool ok;
        QString text = QInputDialog::getText(this, tr("Auto Response"), tr("Enter your auto response message:"), QLineEdit::Normal, "", &ok);
        if (ok) {
            files->setMatchMessage(requestedItemId, text);
            clearRequest();
        }
    }
}

void PopupChatDialog::setMatchToFiles() {
    if (requestedItemId != 0) {
        QStringList paths = QFileDialog::getOpenFileNames(this, "Select the files");
        if (!paths.empty()) {
            if (QMessageBox::question(this, "The Mixologist", "Send to " + friendName + " now?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                if (files->setMatchFile(requestedItemId, paths, LibraryMixerItem::MATCHED_TO_FILE, librarymixer_id)) {
                    if (paths.count() == 1){
                        addSysMsg("Reading file, will send as soon as ready");
                    } else {
                        addSysMsg("Reading files, will send as soon as ready");
                    }
                    clearRequest();
                } else {
                    addSysMsg("An error occurred trying to match and send =(");
                }
            } else {
                files->setMatchFile(requestedItemId, paths, LibraryMixerItem::MATCHED_TO_FILE);
                clearRequest();
            }
        }
    }
}

void PopupChatDialog::setMatchToLend() {
    if (requestedItemId != 0) {
        QStringList paths = QFileDialog::getOpenFileNames(this, "");
        if (!paths.empty()) {
            files->setMatchFile(requestedItemId, paths, LibraryMixerItem::MATCHED_TO_LEND);
            clearRequest();
        }
    }
}

void PopupChatDialog::setMatchToChat() {
    if (requestedItemId != 0) {
        files->setMatchChat(requestedItemId);
        clearRequest();
    }
}

void PopupChatDialog::clearRequest() {
    ui.requestButton->setVisible(false);
    ui.requestLabel->clear();
    requestedItemId = 0;
}

void PopupChatDialog::addTimeStamp(){
    time_stamp_timer->stop();
    QString timestamp = QDateTime::currentDateTime().toString("h:mm ap, dddd, MMMM d");
    timestamp = "<span style=\"color:grey;\">" + timestamp + "</span>";
    addText(timestamp);
}

void PopupChatDialog::selectFiles() {
    QStringList paths = QFileDialog::getOpenFileNames(this, "It's usually easier to just drag and drop, but you can also do it this way");
    if (!paths.empty()) sendFiles(paths);
}

void PopupChatDialog::sendFiles(QStringList paths) {
    bool offline = true;
    {
        PeerDetails detail;
        peers->getPeerDetails(librarymixer_id, detail);
        if (detail.state == FCS_CONNECTED_TCP ||
            detail.state == FCS_CONNECTED_UDP) {
            offline = false;
        }
    }
    if (offline) {
        addSysMsg("Friend offline, could not send.");
        return;
    }

    if (!paths.empty()) {
        /*we need to:
          (1) See if this is a match for something that was requested
          (2) See if this is a return being borrowed
          (3) See if paths are in library database
          (4) Add as a temp item and send an invitation
         */

        //QT file functions produce non-native directory separators, so we should convert them before we continue
        /*
        for(int i = 0; i < paths.size(); i++){
            paths[i] = QDir::toNativeSeparators(paths[i]);
        }*/

        //See if this is a match for something that was requested
        if (requestedItemId != 0) {
            QString transferName = files->getLibraryMixerItem(requestedItemId)->title();
            QString message;
            if (paths.count() > 1) message = "Automatically send these files on all future requests for " + transferName + "?";
            else message = "Automatically send this file on all future requests for " + transferName + "?";
            if (QMessageBox::question(this, transferName, message, QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                if (files->setMatchFile(requestedItemId, paths, LibraryMixerItem::MATCHED_TO_FILE, librarymixer_id)) {
                    if (paths.count() == 1){
                        addSysMsg("Reading file, will send as soon as ready");
                    } else {
                        addSysMsg("Reading files, will send as soon as ready");
                    }
                    clearRequest();
                } else {
                    addSysMsg("Unable to match " + transferName + " you either already matched it or removed it.");
                }
                return;
            }
        }

        //See if this is a return of something being borrowed
        QStringList borrowedTitles;
        QStringList borrowedItemKeys;
        files->getBorrowings(borrowedTitles, borrowedItemKeys, librarymixer_id);
        for(int i = 0; i < borrowedTitles.count(); i++) {
            int choice = (borrowedTitles.count() > 1) ?
                         QMessageBox::question(this, "The Mixologist", "Are you returning '" + borrowedTitles[i] + "'?", QMessageBox::Yes|QMessageBox::No|QMessageBox::NoToAll|QMessageBox::Cancel, QMessageBox::No) :
                         QMessageBox::question(this, "The Mixologist", "Are you returning '" + borrowedTitles[i] + "'?", QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::No);
            if (choice == QMessageBox::Cancel) return;
            else if (choice == QMessageBox::NoToAll) break;
            else if (choice == QMessageBox::Yes) {
                addSysMsg("Returning '" + borrowedTitles[i] + "' to " + friendName + ".");
                files->returnFiles(borrowedTitles[i], paths, librarymixer_id, borrowedItemKeys[i]);
                return;
            }
        }

        //See if paths are in library database
        LibraryMixerItem* item = files->getLibraryMixerItem(paths);
        if (item != NULL) {
            files->MixologySuggest(librarymixer_id, item->id());
            return;
        }

        //If we get to here, just add it as a temporary item and ask for a title
        //Use first file's name as the default invite text
        QFileInfo first_file(paths.first());
        QString title = QInputDialog::getText(this, "Description", "Please describe to your friend what you are sending:", QLineEdit::Normal, first_file.baseName());
        if (title == "") return;
        files->sendTemporary(title, paths, librarymixer_id);
    }
}

void PopupChatDialog::addChatMsg(const QString &message, bool self){
    //In case someone is sneakily trying to send us a poisoned string
    QString display_message = Qt::escape(message);

    linkify(display_message);

    QString name;
    if (self){
        name = "<span style=\"color:green;font-weight: bold;\">" + peers->getOwnName() + "</span>";
    } else {
        name = "<span style=\"color:green;font-weight: bold; background: #F0F0F0;\">" + friendName + "</span>";
    }
    display_message = name + ": " + display_message;

    addText(display_message);

    time_stamp_timer->start(TIMESTAMP_TIMEOUT_LENGTH);
}

void PopupChatDialog::linkify(QString &message) {
    QRegExp rx("http://[^ <>]*|https://[^ <>]*|www\\.[^ <>]*");
    int pos = 0;
    while ( (pos = rx.indexIn(message, pos)) != -1 ) {
        QString raw_link = rx.cap();
        QString link = raw_link;
        if (!link.startsWith("http")) link.prepend("http://");
        QString toInsert = "<a href=\"" +
                           link +
                           "\">" +
                           raw_link +
                           "</a>";
        message.remove(pos, raw_link.length());
        message.insert(pos, toInsert);
        pos += toInsert.length();
    }
}

void PopupChatDialog::addText(QString &text){
    /*We used to check to see if the cursor was moved so that we wouldn't disturb the user on inserting new text.
      Unfortunately, this method of restoring position is based on the cursor, not the view,
      which was counter to user expectations. If a user set the cursor at the beginning and then he scrolled the view back down,
      the cursor would remain at the beginning, causing the window to jump on every new message.
      I don't see a way with QT to save the view position, so this is all disabled for now.
    QTextCursor cursor = ui.textBrowser->textCursor();
    bool atEnd = cursor.atEnd();
    int position = cursor.position();*/

    QTextCursor cursor = ui.textBrowser->textCursor();
    ui.textBrowser->setHtml(ui.textBrowser->toHtml() + text);
    cursor.movePosition(QTextCursor::End);
    ui.textBrowser->setTextCursor(cursor);

    /*After inserting, we can now check if we were already at end, or if we should restore a position
    if (atEnd) cursor.movePosition(QTextCursor::End);
    else cursor.setPosition(position);
    ui.textBrowser->setTextCursor(cursor);*/
}
