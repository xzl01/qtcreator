// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "vcpkg_test.h"

#include "vcpkgsearch.h"

#include <QTest>

namespace Vcpkg::Internal {

VcpkgSearchTest::VcpkgSearchTest(QObject *parent)
    : QObject(parent)
{ }

VcpkgSearchTest::~VcpkgSearchTest() = default;

void VcpkgSearchTest::testVcpkgJsonParser_data()
{
    QTest::addColumn<QString>("vcpkgManifestJsonData");
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("version");
    QTest::addColumn<QString>("license");
    QTest::addColumn<QString>("shortDescription");
    QTest::addColumn<QStringList>("description");
    QTest::addColumn<QUrl>("homepage");
    QTest::addColumn<bool>("success");

    QTest::newRow("cimg, version, short description")
            << R"({
                    "name": "cimg",
                    "version": "2.9.9",
                    "description": "The CImg Library is a small, open-source, and modern C++ toolkit for image processing",
                    "homepage": "https://github.com/dtschump/CImg",
                    "dependencies": [
                      {
                        "name": "vcpkg-cmake",
                        "host": true
                      }
                    ]
                   })"
            << "cimg"
            << "2.9.9"
            << ""
            << "The CImg Library is a small, open-source, and modern C++ toolkit for image processing"
            << QStringList()
            << QUrl::fromUserInput("https://github.com/dtschump/CImg")
            << true;

    QTest::newRow("catch-classic, version-string, complete description")
            << R"({
                    "name": "catch-classic",
                    "version-string": "1.12.2",
                    "port-version": 1,
                    "description": [
                      "A modern, header-only test framework for unit tests",
                      "This is specifically the legacy 1.x branch provided for compatibility",
                      "with older compilers."
                    ],
                    "homepage": "https://github.com/catchorg/Catch2"
                  })"
            << "catch-classic"
            << "1.12.2"
            << ""
            << "A modern, header-only test framework for unit tests"
            << QStringList({"This is specifically the legacy 1.x branch provided for compatibility",
                            "with older compilers."})
            << QUrl::fromUserInput("https://github.com/catchorg/Catch2")
            << true;

    QTest::newRow("Incomplete")
            << R"({
                    "version-semver": "1.0",
                    "description": "foo",
                    "license": "WTFPL"
                  })"
            << ""
            << "1.0"
            << "WTFPL"
            << "foo"
            << QStringList()
            << QUrl()
            << false;
}

void VcpkgSearchTest::testVcpkgJsonParser()
{
    QFETCH(QString, vcpkgManifestJsonData);
    QFETCH(QString, name);
    QFETCH(QString, version);
    QFETCH(QString, license);
    QFETCH(QString, shortDescription);
    QFETCH(QStringList, description);
    QFETCH(QUrl, homepage);
    QFETCH(bool, success);

    bool ok = false;
    const Search::VcpkgManifest mf =
            Search::parseVcpkgManifest(vcpkgManifestJsonData.toUtf8(), &ok);

    QCOMPARE(mf.name, name);
    QCOMPARE(mf.version, version);
    QCOMPARE(mf.license, license);
    QCOMPARE(mf.shortDescription, shortDescription);
    QCOMPARE(mf.description, description);
    QCOMPARE(mf.homepage, homepage);
    QCOMPARE(ok, success);
}

} // namespace Vcpkg::Internal
