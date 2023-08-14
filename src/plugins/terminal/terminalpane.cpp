// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "terminalpane.h"

#include "shellmodel.h"
#include "shortcutmap.h"
#include "terminalconstants.h"
#include "terminalicons.h"
#include "terminalsettings.h"
#include "terminaltr.h"
#include "terminalwidget.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <coreplugin/locator/locatorconstants.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <utils/algorithm.h>
#include <utils/environment.h>
#include <utils/terminalhooks.h>
#include <utils/utilsicons.h>

#include <QMenu>
#include <QStandardPaths>
#include <QToolButton>

namespace Terminal {

using namespace Utils;
using namespace Utils::Terminal;
using namespace Core;

TerminalPane::TerminalPane(QObject *parent)
    : IOutputPane(parent)
    , m_selfContext("Terminal.Pane")
{
    setupContext(m_selfContext, &m_tabWidget);
    setZoomButtonsEnabled(true);

    connect(this, &IOutputPane::zoomInRequested, this, [this] {
        if (currentTerminal())
            currentTerminal()->zoomIn();
    });
    connect(this, &IOutputPane::zoomOutRequested, this, [this] {
        if (currentTerminal())
            currentTerminal()->zoomOut();
    });

    initActions();

    m_lockKeyboardButton = new QToolButton();
    m_lockKeyboardButton->setDefaultAction(&lockKeyboard);

    m_newTerminalButton = new QToolButton();
    m_newTerminalButton->setDefaultAction(&newTerminal);

    m_closeTerminalButton = new QToolButton();
    m_closeTerminalButton->setDefaultAction(&closeTerminal);

    m_openSettingsButton = new QToolButton();
    m_openSettingsButton->setToolTip(Tr::tr("Configure..."));
    m_openSettingsButton->setIcon(Icons::SETTINGS_TOOLBAR.icon());

    connect(m_openSettingsButton, &QToolButton::clicked, m_openSettingsButton, []() {
        ICore::showOptionsDialog("Terminal.General");
    });

    const auto updateEscButton = [this] {
        m_escSettingButton->setChecked(TerminalSettings::instance().sendEscapeToTerminal());
        static const QString escKey
            = QKeySequence(Qt::Key_Escape).toString(QKeySequence::NativeText);
        static const QString shiftEsc = QKeySequence(
                                            QKeyCombination(Qt::ShiftModifier, Qt::Key_Escape))
                                            .toString(QKeySequence::NativeText);
        if (TerminalSettings::instance().sendEscapeToTerminal.value()) {
            m_escSettingButton->setText(escKey);
            m_escSettingButton->setToolTip(Tr::tr("Sends Esc to terminal instead of Qt Creator."));
        } else {
            m_escSettingButton->setText(shiftEsc);
            m_escSettingButton->setToolTip(Tr::tr("Press %1 to send Esc to terminal.").arg(shiftEsc));
        }
    };

    m_escSettingButton = new QToolButton();
    m_escSettingButton->setCheckable(true);

    updateEscButton();

    connect(m_escSettingButton, &QToolButton::toggled, this, [this, updateEscButton] {
        TerminalSettings::instance().sendEscapeToTerminal.setValue(m_escSettingButton->isChecked());
        TerminalSettings::instance().writeSettings(ICore::settings());
        updateEscButton();
    });

    connect(&TerminalSettings::instance(), &TerminalSettings::applied, this, updateEscButton);
}

TerminalPane::~TerminalPane() {}

static std::optional<FilePath> startupProjectDirectory()
{
    const ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project)
        return std::nullopt;

    return project->projectDirectory();
}

void TerminalPane::openTerminal(const OpenTerminalParameters &parameters)
{
    OpenTerminalParameters parametersCopy{parameters};

    if (!parametersCopy.workingDirectory) {
        const std::optional<FilePath> projectDir = startupProjectDirectory();
        if (projectDir) {
            if (!parametersCopy.shellCommand
                || parametersCopy.shellCommand->executable().ensureReachable(*projectDir)) {
                parametersCopy.workingDirectory = *projectDir;
            }
        }
    }

    const auto terminalWidget = new TerminalWidget(&m_tabWidget, parametersCopy);

    using namespace Constants;
    terminalWidget->unlockGlobalAction("Coreplugin.OutputPane.minmax");
    terminalWidget->unlockGlobalAction(Core::Constants::LOCATE);
    terminalWidget->unlockGlobalAction(NEWTERMINAL);
    terminalWidget->unlockGlobalAction(NEXTTERMINAL);
    terminalWidget->unlockGlobalAction(PREVTERMINAL);

    m_tabWidget.setCurrentIndex(m_tabWidget.addTab(terminalWidget, Tr::tr("Terminal")));
    setupTerminalWidget(terminalWidget);

    if (!m_isVisible)
        emit showPage(IOutputPane::ModeSwitch);

    m_tabWidget.currentWidget()->setFocus();

    emit navigateStateUpdate();
}

void TerminalPane::addTerminal(TerminalWidget *terminal, const QString &title)
{
    m_tabWidget.setCurrentIndex(m_tabWidget.addTab(terminal, title));
    setupTerminalWidget(terminal);

    if (!m_isVisible)
        emit showPage(IOutputPane::ModeSwitch);

    terminal->setFocus();

    emit navigateStateUpdate();
}

void TerminalPane::ensureVisible(TerminalWidget *terminal)
{
    if (!m_isVisible)
        emit showPage(IOutputPane::ModeSwitch);
    m_tabWidget.setCurrentWidget(terminal);
    terminal->setFocus();
}

TerminalWidget *TerminalPane::stoppedTerminalWithId(Id identifier) const
{
    for (int i = 0; i < m_tabWidget.count(); ++i) {
        const auto terminal = qobject_cast<TerminalWidget *>(m_tabWidget.widget(i));
        if (terminal && terminal->processState() == QProcess::NotRunning
            && terminal->identifier() == identifier)
            return terminal;
    }

    return nullptr;
}

QWidget *TerminalPane::outputWidget(QWidget *parent)
{
    Q_UNUSED(parent)
    if (!m_widgetInitialized) {
        m_widgetInitialized = true;
        m_tabWidget.setTabBarAutoHide(false);
        m_tabWidget.setDocumentMode(true);
        m_tabWidget.setTabsClosable(true);
        m_tabWidget.setMovable(true);

        connect(&m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
            removeTab(index);
        });

        connect(&m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
            if (auto widget = m_tabWidget.widget(index))
                widget->setFocus();
            else
                emit hidePage();
        });
    }

    return &m_tabWidget;
}

TerminalWidget *TerminalPane::currentTerminal() const
{
    return static_cast<TerminalWidget *>(m_tabWidget.currentWidget());
}

void TerminalPane::removeTab(int index)
{
    delete m_tabWidget.widget(index);
    emit navigateStateUpdate();
}

void TerminalPane::setupTerminalWidget(TerminalWidget *terminal)
{
    if (!terminal)
        return;

    const auto setTabText = [this, terminal]() {
        const int index = m_tabWidget.indexOf(terminal);
        const FilePath cwd = terminal->cwd();

        const QString exe = terminal->currentCommand().isEmpty()
                                ? terminal->shellName()
                                : terminal->currentCommand().executable().fileName();

        if (cwd.isEmpty())
            m_tabWidget.setTabText(index, exe);
        else
            m_tabWidget.setTabText(index, exe + " - " + cwd.fileName());
    };

    connect(terminal, &TerminalWidget::started, this, setTabText);
    connect(terminal, &TerminalWidget::cwdChanged, this, setTabText);
    connect(terminal, &TerminalWidget::commandChanged, this, setTabText);

    if (!terminal->shellName().isEmpty())
        setTabText();
}

void TerminalPane::initActions()
{
    createShellMenu();

    lockKeyboard.setCheckable(true);
    lockKeyboard.setChecked(TerminalSettings::instance().lockKeyboard());

    auto updateLockKeyboard = [this](bool locked) {
        TerminalSettings::instance().lockKeyboard.setValue(locked);
        if (locked) {
            lockKeyboard.setIcon(LOCK_KEYBOARD_ICON.icon());
            lockKeyboard.setToolTip(Tr::tr("Sends keyboard shortcuts to Terminal."));
        } else {
            lockKeyboard.setIcon(UNLOCK_KEYBOARD_ICON.icon());
            lockKeyboard.setToolTip(Tr::tr("Sends keyboard shortcuts to Qt Creator."));
        }
    };

    updateLockKeyboard(TerminalSettings::instance().lockKeyboard());
    connect(&lockKeyboard, &QAction::toggled, this, updateLockKeyboard);

    newTerminal.setText(Tr::tr("New Terminal"));
    newTerminal.setIcon(NEW_TERMINAL_ICON.icon());
    newTerminal.setToolTip(Tr::tr("Create a new Terminal."));
    newTerminal.setMenu(&m_shellMenu);

    nextTerminal.setText(Tr::tr("Next Terminal"));
    prevTerminal.setText(Tr::tr("Previous Terminal"));

    closeTerminal.setIcon(CLOSE_TERMINAL_ICON.icon());
    closeTerminal.setToolTip(Tr::tr("Close the current Terminal."));

    using namespace Constants;

    Command *cmd = ActionManager::registerAction(&newTerminal, NEWTERMINAL, m_selfContext);
    cmd->setDefaultKeySequences({QKeySequence(
        HostOsInfo::isMacHost() ? QLatin1String("Ctrl+T") : QLatin1String("Ctrl+Shift+T"))});

    ActionManager::registerAction(&nextTerminal, NEXTTERMINAL, m_selfContext)
        ->setDefaultKeySequences(
            {QKeySequence("Alt+Tab"),
             QKeySequence(HostOsInfo::isMacHost() ? QLatin1String("Ctrl+Shift+[")
                                                  : QLatin1String("Ctrl+PgUp"))});

    ActionManager::registerAction(&prevTerminal, PREVTERMINAL, m_selfContext)
        ->setDefaultKeySequences(
            {QKeySequence("Alt+Shift+Tab"),
             QKeySequence(HostOsInfo::isMacHost() ? QLatin1String("Ctrl+Shift+]")
                                                  : QLatin1String("Ctrl+PgDown"))});

    connect(&newTerminal, &QAction::triggered, this, [this] { openTerminal({}); });
    connect(&closeTerminal, &QAction::triggered, this, [this] {
        removeTab(m_tabWidget.currentIndex());
    });
    connect(&nextTerminal, &QAction::triggered, this, [this] {
        if (canNavigate())
            goToNext();
    });
    connect(&prevTerminal, &QAction::triggered, this, [this] {
        if (canPrevious())
            goToPrev();
    });
}

void TerminalPane::createShellMenu()
{
    const Internal::ShellModel *shellModel = new Internal::ShellModel(&m_shellMenu);

    connect(&m_shellMenu, &QMenu::aboutToShow, &m_shellMenu, [shellModel, this] {
        m_shellMenu.clear();

        const auto addItems = [this](const QList<Internal::ShellModelItem> &items) {
            for (const Internal::ShellModelItem &item : items) {
                QAction *action = new QAction(item.icon, item.name, &m_shellMenu);

                connect(action, &QAction::triggered, action, [item, this]() {
                    openTerminal(item.openParameters);
                });

                m_shellMenu.addAction(action);
            }
        };

        addItems(shellModel->local());
        m_shellMenu.addSection(Tr::tr("Devices"));
        addItems(shellModel->remote());
    });
}

QList<QWidget *> TerminalPane::toolBarWidgets() const
{
    QList<QWidget *> widgets = IOutputPane::toolBarWidgets();

    widgets.prepend(m_newTerminalButton);
    widgets.prepend(m_closeTerminalButton);

    return widgets << m_openSettingsButton << m_lockKeyboardButton << m_escSettingButton;
}

QString TerminalPane::displayName() const
{
    return Tr::tr("Terminal");
}

int TerminalPane::priorityInStatusBar() const
{
    return 50;
}

void TerminalPane::clearContents()
{
    if (const auto t = currentTerminal())
        t->clearContents();
}

void TerminalPane::visibilityChanged(bool visible)
{
    if (m_isVisible == visible)
        return;

    m_isVisible = visible;

    if (visible && m_tabWidget.count() == 0)
        openTerminal({});

    IOutputPane::visibilityChanged(visible);
}

void TerminalPane::setFocus()
{
    if (const auto t = currentTerminal())
        t->setFocus();
}

bool TerminalPane::hasFocus() const
{
    if (const auto t = currentTerminal())
        return t->hasFocus();

    return false;
}

bool TerminalPane::canFocus() const
{
    return true;
}

bool TerminalPane::canNavigate() const
{
    return true;
}

bool TerminalPane::canNext() const
{
    return m_tabWidget.count() > 1;
}

bool TerminalPane::canPrevious() const
{
    return m_tabWidget.count() > 1;
}

void TerminalPane::goToNext()
{
    int nextIndex = m_tabWidget.currentIndex() + 1;
    if (nextIndex >= m_tabWidget.count())
        nextIndex = 0;

    m_tabWidget.setCurrentIndex(nextIndex);
    emit navigateStateUpdate();
}

void TerminalPane::goToPrev()
{
    int prevIndex = m_tabWidget.currentIndex() - 1;
    if (prevIndex < 0)
        prevIndex = m_tabWidget.count() - 1;

    m_tabWidget.setCurrentIndex(prevIndex);
    emit navigateStateUpdate();
}

} // namespace Terminal
