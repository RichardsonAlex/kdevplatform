/*
 * This file is part of KDevelop
 *
 * Copyright 2016 Milian Wolff <mail@milianw.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "test_stringhelpers.h"

#include <QTest>

#include <language/duchain/stringhelpers.h>

QTEST_MAIN(TestDUChain)

using namespace KDevelop;

void TestDUChain::testFormatComment_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QByteArray>("output");

    QTest::newRow("c-style") << QByteArrayLiteral("// foo\n// bar") << QByteArrayLiteral("foo\n bar");
    QTest::newRow("doxy-c-style") << QByteArrayLiteral("/// foo\n/// bar") << QByteArrayLiteral("foo\n bar");
    QTest::newRow("cpp-style") << QByteArrayLiteral("/*\n foo\n bar\n*/") << QByteArrayLiteral("foo\n bar");
    QTest::newRow("doxy-cpp-style") << QByteArrayLiteral("/**\n * foo\n * bar\n */") << QByteArrayLiteral("foo\n bar");
}

void TestDUChain::testFormatComment()
{
    QFETCH(QByteArray, input);
    QFETCH(QByteArray, output);

    QCOMPARE(formatComment(input), output);
}

void TestDUChain::benchFormatComment()
{
    QBENCHMARK {
        formatComment(QByteArrayLiteral(
            "/**\n"
            " * This is a real comment of some imaginary code.\n"
            " *\n"
            " * @param foo bar\n"
            " * @return meh\n"
            " */\n"
        ));
    }
}
