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

#include <gui/WelcomeWizard.h>
#include <interface/settings.h>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMessageBox>

WelcomeWizard::WelcomeWizard(QWidget *parent) :QWizard(parent) {
    setWindowFlags(windowFlags() & !Qt::WindowContextHelpButtonHint);

    IntroPage* introPage = new IntroPage(this);
    addPage(introPage);
    ConclusionPage* conclusionPage = new ConclusionPage(this);
    addPage(conclusionPage);

    /* This is currently hard-coded for simplicity, but the conclusion page is the longest.
       Therefore, we set the wizard size to be the size of the conclusion page. */
    conclusionPage->adjustSize();
    introPage->setMinimumHeight(conclusionPage->height());

    setWindowTitle("Welcome to the Mixologist!");
    setOption(QWizard::IndependentPages, true);
    setOption(QWizard::NoCancelButton, true);
}

void WelcomeWizard::accept() {
    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    settings.setValue("Gui/ShowAdvanced", field("EnableAdvanced").toBool());

    settings.setValue("Tutorial/Initial", true);

    QDialog::accept();
}

void WelcomeWizard::reject() {
    exit(1);
}

IntroPage::IntroPage(QWidget *parent) :QWizardPage(parent) {
    setTitle(tr("Introduction"));

    QLabel *label = new QLabel("<p>The Mixologist is a chat and file transfer program that works with LibraryMixer. "
                               "The chat part is just like any instant messenger, you can type and drag files directly into the chat box.</p>"
                               "<p>The Mixologist integrates with LibraryMixer, downloading your friends list from the site, and also listing any items from LibraryMixer you marked as in your library and available for checkout.</p>"
                               "<p>When a friend clicks on a 'Get It' link on LibraryMixer to ask you for something, you can set up an automatic response such as lending or sending a file or message that the Mixologist will automatically use for all future requests.</p>");
    label->setWordWrap(true);
    /* This is needed because when we resize the IntroPage in the constructor, the text floats in the middle without it. */
    label->setAlignment(Qt::AlignTop);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addSpacerItem(new QSpacerItem(0, 0));
    setLayout(layout);
}

ConclusionPage::ConclusionPage(QWidget *parent) :QWizardPage(parent) {
    setTitle(tr("Advanced Features"));

    QLabel *label = new QLabel("<p>Besides the features on the previous page, the Mixologist also has a number of advanced features.</p>"
                               "<p>The biggest among these is the ability to list files and browse friends' files not on LibraryMixer. "
                               "You can drag and drop whole folders that your friends can copy or borrow, without ever listing them on LibraryMixer's website.</p>"
                               "<p>Additionally, more information will be available to you on both the inner-workings of the Mixologist and of your connections with friends.</p>"
                               "<p>If this seems really confusing to you, you can leave the advanced features disabled to keep things simple.</p>"
                               "<p>With the advanced features disabled, you can basically just leave the Mixologist main window minimized all the time, and interact with the Mixologist entirely through the LibraryMixer website.</p>"
                               "<p><b>Do you want to enable the advanced features now?</b></p>");
    label->setWordWrap(true);

    QRadioButton* yesButton = new QRadioButton("Yes", this);
    registerField("EnableAdvanced", yesButton);
    QRadioButton* noButton = new QRadioButton("No", this);
    noButton->setChecked(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(yesButton);
    layout->addWidget(noButton);
    setLayout(layout);
}
