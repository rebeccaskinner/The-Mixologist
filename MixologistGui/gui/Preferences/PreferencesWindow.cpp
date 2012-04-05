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



#include <QMessageBox>
#include <QSettings>
#include <gui/Preferences/PreferencesWindow.h>
#include <gui/MainWindow.h>
#include <interface/iface.h>
#include <version.h>

/* Constructor */
PreferencesWindow::PreferencesWindow(QWidget *parent, Qt::WFlags flags)
    : QMainWindow(parent, flags) {
    /* Invoke the Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    /* Toolbar */
    QActionGroup *grp = new QActionGroup(this);

    GeneralDialog *generalDialog = new GeneralDialog(this);
    _pages.insert(ui.actionGeneral, generalDialog);
    grp->addAction(ui.actionGeneral);
    ui.stackPages->insertWidget(0, generalDialog);

    TransfersPrefDialog *transfersPrefDialog = new TransfersPrefDialog(this);
    _pages.insert(ui.actionTransfersPref, transfersPrefDialog);
    grp->addAction(ui.actionTransfersPref);
    ui.stackPages->insertWidget(1, transfersPrefDialog);

    NotifyDialog *notifyDialog = new NotifyDialog(this);
    _pages.insert(ui.actionNotifications, notifyDialog);
    grp->addAction(ui.actionNotifications);
    ui.stackPages->insertWidget(2, notifyDialog);

    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    grp->addAction(ui.actionConnection);
    connectionDialog = new ServerDialog(this);
    _pages.insert(ui.actionConnection, connectionDialog);
    ui.stackPages->insertWidget(3, connectionDialog);

    connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(showPage(QAction *)));
    ui.stackPages->setCurrentWidget(generalDialog);

    ui.versionLabel->setText("Version " + VersionUtil::display_version());

    connect(ui.okButton, SIGNAL(clicked()), this, SLOT(saveChanges()));
    connect(ui.cancelprefButton, SIGNAL(clicked()), this, SLOT(cancelpreferences()));
}

/* Shows the page associated with the activated action. */
void PreferencesWindow::showPage(QAction *pageAction) {
    ui.stackPages->setCurrentWidget(_pages.value(pageAction));
}

/* Saves changes made to settings. */
void PreferencesWindow::saveChanges() {
    /* Call each config page's save() method to save its data */
    foreach (ConfigPage *page, _pages.values()) {
        this->setWindowTitle(QString("Saving ").append(page->objectName()));
        page->save();
    }
    QMainWindow::close();
}

/** Cancel and close the Preferences Window. */
void PreferencesWindow::cancelpreferences() {
    QMainWindow::close();
}

void PreferencesWindow::closeEvent (QCloseEvent *) {
    mainwindow->preferencesWindow = NULL;
    this->deleteLater();
}
