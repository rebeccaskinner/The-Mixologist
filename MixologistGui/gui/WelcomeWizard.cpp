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
    addPage(new MiddlePage(this));
    ConclusionPage* conclusionPage = new ConclusionPage(this);
    addPage(conclusionPage);

    /* This is currently hard-coded for simplicity, but the conclusion page is the longest.
       Therefore, we set the wizard size to be the size of the conclusion page. */
    conclusionPage->adjustSize();
    setFixedHeight(conclusionPage->height());

    setWindowTitle("Welcome to the Mixologist!");
    setOption(QWizard::IndependentPages, true);
    setOption(QWizard::NoCancelButton, true);
}

void WelcomeWizard::accept() {
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    settings.setValue("Tutorial/Initial", true);

    if (field("DisableOffLM").toBool()) settings.setValue("Transfers/EnableOffLibraryMixer", false);
    else settings.setValue("Transfers/EnableOffLibraryMixer", true);

    if (field("manualConnection").toBool()) {
        settings.setValue("Network/RandomizePorts", false);
        QMessageBox::StandardButton result = QMessageBox::information(this,
                                                                      "Allowing the Mixologist to Connect Directly to the Internet",
                                                                      "<p>If you don't know how to configure your router to give the Mixologist a direct connection, "
                                                                      "<a href='http://www.pcwintech.com/port-forwarding-guides'>click here for a guide</a>.</p>"
                                                                      "<p>First, find the brand of your router, and then choose the model that best matches the model number written on your router.</p>"
                                                                      "<p>Set both the external and internal port to be forwarded to 1680 (the Mixologist's port), with at least TCP traffic forwarded. "
                                                                      "You'll also need to set your internal IP address. "
                                                                      "If you need help finding your computer's IP address to enter on the form, "
                                                                      "<a href='http://en.wikibooks.org/wiki/Finding_Your_IP_Address'>click here for a guide</a>.</p>"
                                                                      "<p><i>Neither of these guides is affiliated with the Mixologist.</i></p>"
                                                                      "Select okay when you're done, or hit cancel to quit for now (and hopefully come back later).",
                                                                      QMessageBox::Ok, QMessageBox::Cancel);
        if (result == QMessageBox::Cancel) exit(1);
    }

    QDialog::accept();
}

void WelcomeWizard::reject() {
    exit(1);
}

IntroPage::IntroPage(QWidget *parent) :QWizardPage(parent) {
    setTitle(tr("Introduction"));

    QLabel *label = new QLabel("<p>The Mixologist is a chat and file transfer program that works with LibraryMixer. "
                               "The chat part is just like any instant messenger, you can type and drag files directly into the chat box.</p>"
                               "<p>The Mixologist integrates with LibraryMixer, downloading your friends list from the site, and also listing any items you marked on LibraryMixer as in your library and available for checkout.</p>"
                               "<p>When a friend clicks on a link in LibraryMixer to ask you for something, you can set up an automatic response such as lending or sending a file or message that the Mixologist will automatically use for all future requests.</p>");
    label->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addSpacerItem(new QSpacerItem(0, 0));
    setLayout(layout);
}

MiddlePage::MiddlePage(QWidget *parent) :QWizardPage(parent) {
    setTitle(tr("Additional File Transfer Method"));

    QLabel *label = new QLabel("<p>Besides the stuff on the previous page, the Mixologist also has an extra file transfer method. "
                               "This lets you drag and drop whole folders that your friends can copy or borrow. "
                               "You can browse or search the list of files your friends have shared with you in this way whether they're on or offline.</p>"
                               "<p>If this seems really confusing to you, you can disable this additional file transfer method. "
                               "If you disable it, you can basically just leave the Mixologist main window minimized all the time, and interact with the Mixologist entirely through the LibraryMixer website.</p>"
                               "<p><b>Do you want to disable the optional Mixologist-based file transfer method now?</b></p>");
    label->setWordWrap(true);

    QRadioButton* yesButton = new QRadioButton("Yes", this);
    registerField("DisableOffLM", yesButton);
    QRadioButton* noButton = new QRadioButton("No", this);
    yesButton->setChecked(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(yesButton);
    layout->addWidget(noButton);
    setLayout(layout);
}

ConclusionPage::ConclusionPage(QWidget *parent) :QWizardPage(parent) {
    setTitle("Connecting to Friends");

    QLabel *topLabel = new QLabel("<p>Like all P2P programs, the Mixologist relies on a direct connection to the Internet to be able to connect directly to your friends. "
                                  "Which of these describes your Internet: </p>");
    topLabel->setWordWrap(true);


    QRadioButton* autoButton = new QRadioButton(this);
    QLabel *autoLabel = new QLabel("You have a direct connection to the Internet that is not behind a router/firewall, OR you have a modern router that supports Universal Plug and Play.");
    QSizePolicy size;
    size.setHorizontalPolicy(QSizePolicy::Expanding);
    size.setVerticalPolicy(QSizePolicy::Preferred);
    autoLabel->setSizePolicy(size);
    autoLabel->setWordWrap(true);
    QHBoxLayout *autoLayout = new QHBoxLayout;
    autoLayout->addWidget(autoButton);
    autoLayout->addWidget(autoLabel);

    QRadioButton* manualButton = new QRadioButton(this);
    QLabel *manualLabel = new QLabel("You're a regular home or office user behind a router/firewall that doesn't support Universal Plug and Play. "
                                     "You will need to manually allow the Mixologist direct Internet access on your router/firewall, and after selecting this, instructions will pop-up to help you do this. "
                                     "<b>If you don't understand any of this, chances are this is you.</b>");
    manualLabel->setSizePolicy(size);
    manualLabel->setWordWrap(true);
    QHBoxLayout *manualLayout = new QHBoxLayout;
    manualLayout->addWidget(manualButton);
    manualLayout->addWidget(manualLabel);

    registerField("manualConnection", manualButton);
    manualButton->setChecked(true);

    QLabel *bottomLabel = new QLabel("<p>If you don't have a direct connection to the Internet and can't do either of these, then your experience will not be optimal, and you will only be able to connect to friends that do have a direct connection to the Internet. "
                                     "<i>In the future, more automatic configuration is planned to be added, but for now, that's the unfortunate situation.</i></p>");
    bottomLabel->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    layout->addLayout(autoLayout);
    layout->addLayout(manualLayout);
    layout->addWidget(bottomLabel);
    setLayout(layout);
}
