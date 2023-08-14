// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <utils/filepath.h>
#include <qtsupport/examplesparser.h>

#include <QtTest>

using namespace Core;
using namespace Utils;
using namespace QtSupport::Internal;

class tst_Examples : public QObject
{
    Q_OBJECT

public:
    tst_Examples();
    ~tst_Examples();

private slots:
    void parsing_data();
    void parsing();
};

tst_Examples::tst_Examples() = default;
tst_Examples::~tst_Examples() = default;

using MetaData = QHash<QString, QStringList>;

static ExampleItem fetchItem()
{
    QFETCH(QString, name);
    QFETCH(QString, description);
    QFETCH(QString, imageUrl);
    QFETCH(QStringList, tags);
    QFETCH(FilePath, projectPath);
    QFETCH(QString, docUrl);
    QFETCH(FilePaths, filesToOpen);
    QFETCH(FilePath, mainFile);
    QFETCH(FilePaths, dependencies);
    QFETCH(InstructionalType, type);
    QFETCH(bool, hasSourceCode);
    QFETCH(bool, isVideo);
    QFETCH(bool, isHighlighted);
    QFETCH(QString, videoUrl);
    QFETCH(QString, videoLength);
    QFETCH(QStringList, platforms);
    QFETCH(MetaData, metaData);
    ExampleItem item;
    item.name = name;
    item.description = description;
    item.imageUrl = imageUrl;
    item.tags = tags;
    item.projectPath = projectPath;
    item.docUrl = docUrl;
    item.filesToOpen = filesToOpen;
    item.mainFile = mainFile;
    item.dependencies = dependencies;
    item.type = type;
    item.hasSourceCode = hasSourceCode;
    item.isVideo = isVideo;
    item.isHighlighted = isHighlighted;
    item.videoUrl = videoUrl;
    item.videoLength = videoLength;
    item.platforms = platforms;
    item.metaData = metaData;
    return item;
}

void tst_Examples::parsing_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<bool>("isExamples");
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("imageUrl");
    QTest::addColumn<QStringList>("tags");
    QTest::addColumn<FilePath>("projectPath");
    QTest::addColumn<QString>("docUrl");
    QTest::addColumn<FilePaths>("filesToOpen");
    QTest::addColumn<FilePath>("mainFile");
    QTest::addColumn<FilePaths>("dependencies");
    QTest::addColumn<InstructionalType>("type");
    QTest::addColumn<bool>("hasSourceCode");
    QTest::addColumn<bool>("isVideo");
    QTest::addColumn<bool>("isHighlighted");
    QTest::addColumn<QString>("videoUrl");
    QTest::addColumn<QString>("videoLength");
    QTest::addColumn<QStringList>("platforms");
    QTest::addColumn<MetaData>("metaData");
    QTest::addColumn<QStringList>("categories");

    QTest::addRow("example")
        << QByteArray(R"raw(
    <examples>
        <example docUrl="qthelp://org.qt-project.qtwidgets.660/qtwidgets/qtwidgets-widgets-analogclock-example.html"
                 imageUrl="qthelp://org.qt-project.qtwidgets.660/qtwidgets/images/analogclock-example.png"
                 name="Analog Clock" projectPath="widgets/widgets/analogclock/CMakeLists.txt">
            <description><![CDATA[The Analog Clock example shows how to draw the contents of a custom widget.]]></description>
            <tags>ios,widgets</tags>
            <fileToOpen>widgets/widgets/analogclock/main.cpp</fileToOpen>
            <fileToOpen>widgets/widgets/analogclock/analogclock.h</fileToOpen>
            <fileToOpen mainFile="true">widgets/widgets/analogclock/analogclock.cpp</fileToOpen>
            <meta>
                <entry name="category">Graphics</entry>
                <entry name="category">Graphics</entry>
                <entry name="category">Foobar</entry>
                <entry name="tags">widgets</entry>
            </meta>
        </example>
    </examples>
 )raw") << /*isExamples=*/true
        << "Analog Clock"
        << "The Analog Clock example shows how to draw the contents of a custom widget."
        << "qthelp://org.qt-project.qtwidgets.660/qtwidgets/images/analogclock-example.png"
        << QStringList{"ios", "widgets"}
        << FilePath::fromUserInput("examples/widgets/widgets/analogclock/CMakeLists.txt")
        << "qthelp://org.qt-project.qtwidgets.660/qtwidgets/"
           "qtwidgets-widgets-analogclock-example.html"
        << FilePaths{FilePath::fromUserInput("examples/widgets/widgets/analogclock/main.cpp"),
                     FilePath::fromUserInput("examples/widgets/widgets/analogclock/analogclock.h"),
                     FilePath::fromUserInput(
                         "examples/widgets/widgets/analogclock/analogclock.cpp")}
        << FilePath::fromUserInput("examples/widgets/widgets/analogclock/analogclock.cpp")
        << FilePaths() << Example << true << false << false << ""
        << "" << QStringList()
        << MetaData({{"category", {"Graphics", "Graphics", "Foobar"}}, {"tags", {"widgets"}}})
        << QStringList{"Foobar", "Graphics"};

    QTest::addRow("no category, highlighted")
        << QByteArray(R"raw(
    <examples>
        <example name="No Category, highlighted"
                 isHighlighted="true">
        </example>
    </examples>
 )raw") << /*isExamples=*/true
        << "No Category, highlighted" << QString() << QString() << QStringList()
        << FilePath("examples") << QString() << FilePaths() << FilePath() << FilePaths() << Example
        << /*hasSourceCode=*/false << false << /*isHighlighted=*/true << ""
        << "" << QStringList() << MetaData() << QStringList{"Featured"};
}

void tst_Examples::parsing()
{
    QFETCH(QByteArray, data);
    QFETCH(bool, isExamples);
    QFETCH(QStringList, categories);
    const ExampleItem expected = fetchItem();
    const expected_str<QList<ExampleItem *>> result
        = parseExamples(data,
                        FilePath("manifest/examples-manifest.xml"),
                        FilePath("examples"),
                        FilePath("demos"),
                        isExamples);
    QVERIFY(result);
    QCOMPARE(result->size(), 1);
    const ExampleItem item = *result->at(0);
    QCOMPARE(item.name, expected.name);
    QCOMPARE(item.description, expected.description);
    QCOMPARE(item.imageUrl, expected.imageUrl);
    QCOMPARE(item.tags, expected.tags);
    QCOMPARE(item.projectPath, expected.projectPath);
    QCOMPARE(item.docUrl, expected.docUrl);
    QCOMPARE(item.filesToOpen, expected.filesToOpen);
    QCOMPARE(item.mainFile, expected.mainFile);
    QCOMPARE(item.dependencies, expected.dependencies);
    QCOMPARE(item.type, expected.type);
    QCOMPARE(item.hasSourceCode, expected.hasSourceCode);
    QCOMPARE(item.isVideo, expected.isVideo);
    QCOMPARE(item.isHighlighted, expected.isHighlighted);
    QCOMPARE(item.videoUrl, expected.videoUrl);
    QCOMPARE(item.videoLength, expected.videoLength);
    QCOMPARE(item.platforms, expected.platforms);
    QCOMPARE(item.metaData, expected.metaData);

    const QList<std::pair<Section, QList<ExampleItem *>>> resultCategories = getCategories(*result,
                                                                                           true);
    QCOMPARE(resultCategories.size(), categories.size());
    for (int i = 0; i < resultCategories.size(); ++i) {
        QCOMPARE(resultCategories.at(i).first.name, categories.at(i));
        QCOMPARE(resultCategories.at(i).second.size(), 1);
    }

    for (const auto &category : resultCategories)
        qDeleteAll(category.second);
}

QTEST_APPLESS_MAIN(tst_Examples)

#include "tst_examples.moc"
