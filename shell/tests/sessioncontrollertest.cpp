/***************************************************************************
 *   Copyright 2008 Manuel Breugelmans <mbr.nxi@gmail.com>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "sessioncontrollertest.h"

#include <QtGui>
#define QT_GUI_LIB 1
#include <QtTest/QtTest>

#include <qtest_kde.h>
#include <tests/common/autotestshell.h>

#include "../core.h"
#include "../sessioncontroller.h"
#include "../session.h"

Q_DECLARE_METATYPE( KDevelop::ISession* )

using KDevelop::SessionController;
using KDevelop::ISession;
using KDevelop::Core;

using QTest::kWaitForSignal;

////////////////////// Fixture ///////////////////////////////////////////////

void SessionControllerTest::initTestCase()
{
    AutoTestShell::init();
    Core::initialize( KDevelop::Core::NoUi );
    m_core = Core::self();
    qRegisterMetaType<KDevelop::ISession*>();
}

void SessionControllerTest::init()
{
    m_sessionCtrl = m_core->sessionControllerInternal();
}

void SessionControllerTest::cleanup()
{
}

void SessionControllerTest::createSession()
{
    const QString sessionName = "TestSession";
    int sessionCount = m_sessionCtrl->sessions().count();
    ISession* s = m_sessionCtrl->createSession( sessionName );
    QCOMPARE( sessionName, s->name() );
    QCOMPARE( sessionCount+1, m_sessionCtrl->sessions().count() );
}

void SessionControllerTest::loadSession()
{
    const QString sessionName = "TestSession2";
    ISession* s = m_sessionCtrl->createSession( sessionName );
    QSignalSpy spy(m_sessionCtrl, SIGNAL(sessionLoaded(KDevelop::ISession*)));
    m_sessionCtrl->loadSession( s );

    QCOMPARE(spy.size(), 1);
    QList<QVariant> arguments = spy.takeFirst();

    ISession* emittedSession = arguments.at(0).value<ISession*>();
    QCOMPARE(s, emittedSession);
}

QTEST_KDEMAIN( SessionControllerTest, GUI)
#include "sessioncontrollertest.moc"
