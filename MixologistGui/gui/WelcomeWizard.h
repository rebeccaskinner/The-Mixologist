/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *
 *  This file is part of The Mixologist.
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

#ifndef _WELCOME_WIZ_H
#define _WELCOME_WIZ_H

#include <QWizard>
#include <QRadioButton>

class QTreeView;

class WelcomeWizard :public QWizard {
public:
    WelcomeWizard(QWidget *parent = 0);

    void accept();
    void reject();
};

class IntroPage :public QWizardPage {
public:
    IntroPage(QWidget *parent = 0);
};

class MiddlePage :public QWizardPage {
public:
    MiddlePage(QWidget *parent = 0);
};

class ConclusionPage :public QWizardPage {
public:
    ConclusionPage(QWidget *parent = 0);
};

#endif


