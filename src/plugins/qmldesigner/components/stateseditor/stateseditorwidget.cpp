// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "stateseditorwidget.h"
#include "stateseditorimageprovider.h"
#include "stateseditormodel.h"
#include "stateseditorview.h"

#include <designersettings.h>
#include <theme.h>
#include <qmldesignerconstants.h>
#include <qmldesignerplugin.h>

#include <invalidqmlsourceexception.h>

#include <coreplugin/icore.h>
#include <coreplugin/messagebox.h>

#include <utils/environment.h>
#include <utils/qtcassert.h>
#include <utils/stylehelper.h>

#include <QApplication>

#include <QBoxLayout>
#include <QFileInfo>
#include <QKeySequence>
#include <QShortcut>

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

enum { debug = false };

namespace QmlDesigner {

static QString propertyEditorResourcesPath()
{
#ifdef SHARE_QML_PATH
    if (Utils::qtcEnvironmentVariableIsSet("LOAD_QML_FROM_SOURCE"))
        return QLatin1String(SHARE_QML_PATH) + "/propertyEditorQmlSources";
#endif
    return Core::ICore::resourcePath("qmldesigner/propertyEditorQmlSources").toString();
}

int StatesEditorWidget::currentStateInternalId() const
{
    QTC_ASSERT(rootObject(), return -1);
    QTC_ASSERT(rootObject()->property("currentStateInternalId").isValid(), return -1);

    return rootObject()->property("currentStateInternalId").toInt();
}

void StatesEditorWidget::setCurrentStateInternalId(int internalId)
{
    QTC_ASSERT(rootObject(), return);
    rootObject()->setProperty("currentStateInternalId", internalId);
}

void StatesEditorWidget::setNodeInstanceView(const NodeInstanceView *nodeInstanceView)
{
    m_imageProvider->setNodeInstanceView(nodeInstanceView);
}

void StatesEditorWidget::showAddNewStatesButton(bool showAddNewStatesButton)
{
    rootContext()->setContextProperty(QLatin1String("canAddNewStates"), showAddNewStatesButton);
}

StatesEditorWidget::StatesEditorWidget(StatesEditorView *statesEditorView,
                                       StatesEditorModel *statesEditorModel)
    : m_statesEditorView(statesEditorView)
    , m_imageProvider(nullptr)
    , m_qmlSourceUpdateShortcut(nullptr)
{
    m_imageProvider = new Internal::StatesEditorImageProvider;
    m_imageProvider->setNodeInstanceView(statesEditorView->nodeInstanceView());

    engine()->addImageProvider(QStringLiteral("qmldesigner_stateseditor"), m_imageProvider);
    engine()->addImportPath(qmlSourcesPath());
    engine()->addImportPath(propertyEditorResourcesPath() + "/imports");

    m_qmlSourceUpdateShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F4), this);
    connect(m_qmlSourceUpdateShortcut, &QShortcut::activated, this, &StatesEditorWidget::reloadQmlSource);

    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    rootContext()->setContextProperties(
        QVector<QQmlContext::PropertyPair>{{{"statesEditorModel"},
                                            QVariant::fromValue(statesEditorModel)},
                                           {{"canAddNewStates"}, true}});

    Theme::setupTheme(engine());

    setWindowTitle(tr("States", "Title of Editor widget"));

    // init the first load of the QML UI elements
    reloadQmlSource();
}

StatesEditorWidget::~StatesEditorWidget() = default;

QString StatesEditorWidget::qmlSourcesPath()
{
#ifdef SHARE_QML_PATH
    if (Utils::qtcEnvironmentVariableIsSet("LOAD_QML_FROM_SOURCE"))
        return QLatin1String(SHARE_QML_PATH) + "/statesEditorQmlSources";
#endif
    return Core::ICore::resourcePath("qmldesigner/statesEditorQmlSources").toString();
}

void StatesEditorWidget::showEvent(QShowEvent *event)
{
    StudioQuickWidget::showEvent(event);
    update();
}

void StatesEditorWidget::focusOutEvent(QFocusEvent *focusEvent)
{
    QmlDesignerPlugin::emitUsageStatisticsTime(Constants::EVENT_STATESEDITOR_TIME,
                                               m_usageTimer.elapsed());
    StudioQuickWidget::focusOutEvent(focusEvent);
}

void StatesEditorWidget::focusInEvent(QFocusEvent *focusEvent)
{
    m_usageTimer.restart();
    StudioQuickWidget::focusInEvent(focusEvent);
}

void StatesEditorWidget::reloadQmlSource()
{
    QString statesListQmlFilePath = qmlSourcesPath() + QStringLiteral("/StatesList.qml");
    QTC_ASSERT(QFileInfo::exists(statesListQmlFilePath), return);
    engine()->clearComponentCache();
    setSource(QUrl::fromLocalFile(statesListQmlFilePath));

    if (!rootObject()) {
        QString errorString;
        for (const QQmlError &error : errors())
            errorString += "\n" + error.toString();

        Core::AsynchronousMessageBox::warning(tr("Cannot Create QtQuick View"),
                                              tr("StatesEditorWidget: %1 cannot be created.%2")
                                                  .arg(qmlSourcesPath(), errorString));
        return;
    }

    connect(rootObject(),
            SIGNAL(currentStateInternalIdChanged()),
            m_statesEditorView.data(),
            SLOT(synchonizeCurrentStateFromWidget()));
    connect(rootObject(), SIGNAL(createNewState()), m_statesEditorView.data(), SLOT(createNewState()));
    connect(rootObject(), SIGNAL(deleteState(int)), m_statesEditorView.data(), SLOT(removeState(int)));
    m_statesEditorView.data()->synchonizeCurrentStateFromWidget();
}

} // namespace QmlDesigner
