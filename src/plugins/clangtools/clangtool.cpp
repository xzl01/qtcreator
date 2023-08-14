// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangtool.h"

#include "clangfixitsrefactoringchanges.h"
#include "clangselectablefilesdialog.h"
#include "clangtoolruncontrol.h"
#include "clangtoolsconstants.h"
#include "clangtoolsdiagnostic.h"
#include "clangtoolsdiagnosticmodel.h"
#include "clangtoolsdiagnosticview.h"
#include "clangtoolslogfilereader.h"
#include "clangtoolsprojectsettings.h"
#include "clangtoolssettings.h"
#include "clangtoolstr.h"
#include "clangtoolsutils.h"
#include "filterdialog.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagebox.h>

#include <cppeditor/clangdiagnosticconfigsmodel.h>
#include <cppeditor/cppmodelmanager.h>

#include <debugger/analyzer/analyzermanager.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectexplorericons.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>
#include <projectexplorer/taskhub.h>

#include <texteditor/textdocument.h>

#include <utils/algorithm.h>
#include <utils/checkablemessagebox.h>
#include <utils/fancylineedit.h>
#include <utils/fancymainwindow.h>
#include <utils/infolabel.h>
#include <utils/progressindicator.h>
#include <utils/proxyaction.h>
#include <utils/utilsicons.h>

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

using namespace Core;
using namespace CppEditor;
using namespace Debugger;
using namespace ProjectExplorer;
using namespace Utils;

namespace ClangTools {
namespace Internal {

static QString makeLink(const QString &text)
{
    return QString("<a href=t>%1</a>").arg(text);
}

class InfoBarWidget : public QFrame
{
    Q_OBJECT

public:
    InfoBarWidget()
        : m_progressIndicator(new Utils::ProgressIndicator(ProgressIndicatorSize::Small))
        , m_info(new InfoLabel({}, InfoLabel::Information))
        , m_error(new InfoLabel({}, InfoLabel::Warning))
        , m_diagStats(new QLabel)
    {
        m_info->setElideMode(Qt::ElideNone);
        m_error->setElideMode(Qt::ElideNone);

        m_diagStats->setTextInteractionFlags(Qt::TextBrowserInteraction);

        QHBoxLayout *layout = new QHBoxLayout;
        layout->setContentsMargins(5, 5, 5, 5);
        layout->addWidget(m_progressIndicator);
        layout->addWidget(m_info);
        layout->addWidget(m_error);
        layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
        layout->addWidget(m_diagStats);
        setLayout(layout);

        QPalette pal;
        pal.setColor(QPalette::Window, Utils::creatorTheme()->color(Utils::Theme::InfoBarBackground));
        pal.setColor(QPalette::WindowText, Utils::creatorTheme()->color(Utils::Theme::InfoBarText));
        setPalette(pal);

        setAutoFillBackground(true);
    }

    // Info
    enum InfoIconType { ProgressIcon, InfoIcon };
    void setInfoIcon(InfoIconType type)
    {
        const bool showProgress = type == ProgressIcon;
        m_progressIndicator->setVisible(showProgress);
        m_info->setType(showProgress ? InfoLabel::None : InfoLabel::Information);
    }
    QString infoText() const { return m_info->text(); }
    void setInfoText(const QString &text)
    {
        m_info->setVisible(!text.isEmpty());
        m_info->setText(text);
        evaluateVisibility();
    }

    // Error
    using OnLinkActivated = std::function<void()>;
    enum IssueType { Warning, Error };

    QString errorText() const { return m_error->text(); }
    void setError(IssueType type,
                  const QString &text,
                  const OnLinkActivated &linkAction = OnLinkActivated())
    {
        m_error->setVisible(!text.isEmpty());
        m_error->setText(text);
        m_error->setType(type == Warning ? InfoLabel::Warning : InfoLabel::Error);
        m_error->disconnect();
        if (linkAction)
            connect(m_error, &QLabel::linkActivated, this, linkAction);
        evaluateVisibility();
    }

    // Diag stats
    void setDiagText(const QString &text) { m_diagStats->setText(text); }

    void reset()
    {
        setInfoIcon(InfoIcon);
        setInfoText({});
        setError(Warning, {}, {});
        setDiagText({});
    }

    void evaluateVisibility()
    {
        setVisible(!infoText().isEmpty() || !errorText().isEmpty());
    }

private:
    Utils::ProgressIndicator *m_progressIndicator;
    InfoLabel *m_info;
    InfoLabel *m_error;
    QLabel *m_diagStats;
};

class SelectFixitsCheckBox : public QCheckBox
{
    Q_OBJECT

private:
    void nextCheckState() final
    {
        setCheckState(checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
};

class ApplyFixIts
{
public:
    class RefactoringFileInfo
    {
    public:
        FixitsRefactoringFile file;
        QVector<DiagnosticItem *> diagnosticItems;
        bool hasScheduledFixits = false;
    };

    ApplyFixIts(const QVector<DiagnosticItem *> &diagnosticItems)
    {
        for (DiagnosticItem *diagnosticItem : diagnosticItems) {
            const Utils::FilePath &filePath = diagnosticItem->diagnostic().location.filePath;
            QTC_ASSERT(!filePath.isEmpty(), continue);

            // Get or create refactoring file
            RefactoringFileInfo &fileInfo = m_refactoringFileInfos[filePath];

            // Append item
            fileInfo.diagnosticItems += diagnosticItem;
            if (diagnosticItem->fixItStatus() == FixitStatus::Scheduled)
                fileInfo.hasScheduledFixits = true;
        }
    }

    static void addFixitOperations(DiagnosticItem *diagnosticItem,
                                   const FixitsRefactoringFile &file, bool apply)
    {
        if (!diagnosticItem->hasNewFixIts())
            return;

        // Did we already created the fixit operations?
        ReplacementOperations currentOps = diagnosticItem->fixitOperations();
        if (!currentOps.isEmpty()) {
            for (ReplacementOperation *op : currentOps)
                op->apply = apply;
            return;
        }

        // Collect/construct the fixit operations
        ReplacementOperations replacements;

        for (const ExplainingStep &step : diagnosticItem->diagnostic().explainingSteps) {
            if (!step.isFixIt)
                continue;

            const Debugger::DiagnosticLocation start = step.ranges.first();
            const Debugger::DiagnosticLocation end = step.ranges.last();
            const int startPos = file.position(start.filePath.toString(), start.line, start.column);
            const int endPos = file.position(start.filePath.toString(), end.line, end.column);

            auto op = new ReplacementOperation;
            op->pos = startPos;
            op->length = endPos - startPos;
            op->text = step.message;
            op->fileName = start.filePath.toString();
            op->apply = apply;

            replacements += op;
        }

        diagnosticItem->setFixitOperations(replacements);
    }

    void apply(ClangToolsDiagnosticModel *model)
    {
        for (auto it = m_refactoringFileInfos.begin(); it != m_refactoringFileInfos.end(); ++it) {
            RefactoringFileInfo &fileInfo = it.value();

            QVector<DiagnosticItem *> itemsScheduledOrSchedulable;
            QVector<DiagnosticItem *> itemsScheduled;
            QVector<DiagnosticItem *> itemsSchedulable;

            // Construct refactoring operations
            for (DiagnosticItem *diagnosticItem : std::as_const(fileInfo.diagnosticItems)) {
                const FixitStatus fixItStatus = diagnosticItem->fixItStatus();

                const bool isScheduled = fixItStatus == FixitStatus::Scheduled;
                const bool isSchedulable = fileInfo.hasScheduledFixits
                                           && fixItStatus == FixitStatus::NotScheduled;

                if (isScheduled || isSchedulable) {
                    addFixitOperations(diagnosticItem, fileInfo.file, isScheduled);
                    itemsScheduledOrSchedulable += diagnosticItem;
                    if (isScheduled)
                        itemsScheduled += diagnosticItem;
                    else
                        itemsSchedulable += diagnosticItem;
                }
            }

            // Collect replacements
            ReplacementOperations ops;
            for (DiagnosticItem *item : std::as_const(itemsScheduledOrSchedulable))
                ops += item->fixitOperations();

            if (ops.empty())
                continue;

            // Apply file
            QVector<DiagnosticItem *> itemsApplied;
            QVector<DiagnosticItem *> itemsFailedToApply;
            QVector<DiagnosticItem *> itemsInvalidated;

            fileInfo.file.setReplacements(ops);
            model->removeWatchedPath(ops.first()->fileName);
            if (fileInfo.file.apply()) {
                itemsApplied = itemsScheduled;
            } else {
                itemsFailedToApply = itemsScheduled;
                itemsInvalidated = itemsSchedulable;
            }
            model->addWatchedPath(ops.first()->fileName);

            // Update DiagnosticItem state
            for (DiagnosticItem *diagnosticItem : std::as_const(itemsScheduled))
                diagnosticItem->setFixItStatus(FixitStatus::Applied);
            for (DiagnosticItem *diagnosticItem : std::as_const(itemsFailedToApply))
                diagnosticItem->setFixItStatus(FixitStatus::FailedToApply);
            for (DiagnosticItem *diagnosticItem : std::as_const(itemsInvalidated))
                diagnosticItem->setFixItStatus(FixitStatus::Invalidated);
        }
    }

private:
    QMap<Utils::FilePath, RefactoringFileInfo> m_refactoringFileInfos;
};

static FileInfos sortedFileInfos(const QVector<CppEditor::ProjectPart::ConstPtr> &projectParts)
{
    FileInfos fileInfos;

    for (const CppEditor::ProjectPart::ConstPtr &projectPart : projectParts) {
        QTC_ASSERT(projectPart, continue);
        if (!projectPart->selectedForBuilding)
            continue;

        for (const CppEditor::ProjectFile &file : std::as_const(projectPart->files)) {
            QTC_ASSERT(file.kind != CppEditor::ProjectFile::Unclassified, continue);
            QTC_ASSERT(file.kind != CppEditor::ProjectFile::Unsupported, continue);
            if (file.path == CppEditor::CppModelManager::configurationFileName())
                continue;

            if (file.active
                && (CppEditor::ProjectFile::isSource(file.kind)
                    || CppEditor::ProjectFile::isHeader(file.kind))) {
                ProjectFile::Kind sourceKind = CppEditor::ProjectFile::sourceKind(file.kind);
                fileInfos.emplace_back(file.path, sourceKind, projectPart);
            }
        }
    }

    Utils::sort(fileInfos, [](const FileInfo &fi1, const FileInfo &fi2) {
        if (fi1.file == fi2.file) {
            // If the same file appears more than once, prefer contexts where the file is
            // built as part of an application or library to those where it may not be,
            // e.g. because it is just listed as some sort of resource.
            return fi1.projectPart->buildTargetType != BuildTargetType::Unknown
                    && fi2.projectPart->buildTargetType == BuildTargetType::Unknown;
        }
        return fi1.file < fi2.file;
    });
    fileInfos.erase(std::unique(fileInfos.begin(), fileInfos.end()), fileInfos.end());

    return fileInfos;
}

static RunSettings runSettings()
{
    if (Project *project = ProjectManager::startupProject()) {
        const auto projectSettings = ClangToolsProjectSettings::getSettings(project);
        if (!projectSettings->useGlobalSettings())
            return projectSettings->runSettings();
    }
    return ClangToolsSettings::instance()->runSettings();
}

ClangTool::ClangTool(const QString &name, Utils::Id id, ClangToolType type)
    : m_name(name), m_perspective{id.toString(), name}, m_type(type)
{
    setObjectName(name);
    m_diagnosticModel = new ClangToolsDiagnosticModel(this);

    auto action = new QAction(Tr::tr("Analyze Project with %1...").arg(name), this);
    action->setIcon(Utils::Icons::RUN_SELECTED_TOOLBAR.icon());
    m_startAction = action;

    action = new QAction(Tr::tr("Analyze Current File with %1").arg(name), this);
    action->setIcon(Utils::Icons::RUN_FILE.icon());
    m_startOnCurrentFileAction = action;

    m_stopAction = Debugger::createStopAction();

    m_diagnosticFilterModel = new DiagnosticFilterModel(this);
    m_diagnosticFilterModel->setSourceModel(m_diagnosticModel);
    m_diagnosticFilterModel->setDynamicSortFilter(true);

    m_infoBarWidget = new InfoBarWidget;

    m_diagnosticView = new DiagnosticView;
    initDiagnosticView();
    m_diagnosticView->setModel(m_diagnosticFilterModel);
    m_diagnosticView->setSortingEnabled(true);
    m_diagnosticView->sortByColumn(Debugger::DetailedErrorView::DiagnosticColumn,
                                   Qt::AscendingOrder);
    connect(m_diagnosticView, &DiagnosticView::showHelp,
            this, &ClangTool::help);
    connect(m_diagnosticView, &DiagnosticView::showFilter,
            this, &ClangTool::filter);
    connect(m_diagnosticView, &DiagnosticView::clearFilter,
            this, &ClangTool::clearFilter);
    connect(m_diagnosticView, &DiagnosticView::filterForCurrentKind,
            this, &ClangTool::filterForCurrentKind);
    connect(m_diagnosticView, &DiagnosticView::filterOutCurrentKind,
            this, &ClangTool::filterOutCurrentKind);

    for (QAbstractItemModel *const model :
         QList<QAbstractItemModel *>({m_diagnosticModel, m_diagnosticFilterModel})) {
        connect(model, &QAbstractItemModel::rowsInserted,
                this, &ClangTool::updateForCurrentState);
        connect(model, &QAbstractItemModel::rowsRemoved,
                this, &ClangTool::updateForCurrentState);
        connect(model, &QAbstractItemModel::modelReset,
                this, &ClangTool::updateForCurrentState);
        connect(model, &QAbstractItemModel::layoutChanged, // For QSortFilterProxyModel::invalidate()
                this, &ClangTool::updateForCurrentState);
    }

    // Go to previous diagnostic
    action = new QAction(this);
    action->setDisabled(true);
    action->setIcon(Utils::Icons::PREV_TOOLBAR.icon());
    action->setToolTip(Tr::tr("Go to previous diagnostic."));
    connect(action, &QAction::triggered, m_diagnosticView, &DetailedErrorView::goBack);
    m_goBack = action;

    // Go to next diagnostic
    action = new QAction(this);
    action->setDisabled(true);
    action->setIcon(Utils::Icons::NEXT_TOOLBAR.icon());
    action->setToolTip(Tr::tr("Go to next diagnostic."));
    connect(action, &QAction::triggered, m_diagnosticView, &DetailedErrorView::goNext);
    m_goNext = action;

    // Load diagnostics from file
    action = new QAction(this);
    action->setIcon(Utils::Icons::OPENFILE_TOOLBAR.icon());
    action->setToolTip(Tr::tr("Load diagnostics from YAML files exported with \"-export-fixes\"."));
    connect(action, &QAction::triggered, this, &ClangTool::loadDiagnosticsFromFiles);
    m_loadExported = action;

    // Clear data
    action = new QAction(this);
    action->setDisabled(true);
    action->setIcon(Utils::Icons::CLEAN_TOOLBAR.icon());
    action->setToolTip(Tr::tr("Clear"));
    connect(action, &QAction::triggered, this, [this] {
        reset();
        update();
    });
    m_clear = action;

    // Expand/Collapse
    action = new QAction(this);
    action->setDisabled(true);
    action->setCheckable(true);
    action->setIcon(Utils::Icons::EXPAND_ALL_TOOLBAR.icon());
    action->setToolTip(Tr::tr("Expand All"));
    connect(action, &QAction::toggled, this, [this](bool checked){
        if (checked) {
            m_expandCollapse->setToolTip(Tr::tr("Collapse All"));
            m_diagnosticView->expandAll();
        } else {
            m_expandCollapse->setToolTip(Tr::tr("Expand All"));
            m_diagnosticView->collapseAll();
        }
    });
    m_expandCollapse = action;

    // Filter button
    action = m_showFilter = new QAction(this);
    action->setIcon(
        Utils::Icon({{":/utils/images/filtericon.png", Utils::Theme::IconsBaseColor}}).icon());
    action->setToolTip(Tr::tr("Filter Diagnostics"));
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &ClangTool::filter);

    // Schedule/Unschedule all fixits
    m_selectFixitsCheckBox = new SelectFixitsCheckBox;
    m_selectFixitsCheckBox->setText("Select Fixits");
    m_selectFixitsCheckBox->setEnabled(false);
    m_selectFixitsCheckBox->setTristate(true);
    connect(m_selectFixitsCheckBox, &QCheckBox::clicked, this, [this] {
        m_diagnosticView->scheduleAllFixits(m_selectFixitsCheckBox->isChecked());
    });

    // Apply fixits button
    m_applyFixitsButton = new QToolButton;
    m_applyFixitsButton->setText(Tr::tr("Apply Fixits"));
    m_applyFixitsButton->setEnabled(false);

    connect(m_diagnosticModel, &ClangToolsDiagnosticModel::fixitStatusChanged,
            m_diagnosticFilterModel, &DiagnosticFilterModel::onFixitStatusChanged);
    connect(m_diagnosticFilterModel, &DiagnosticFilterModel::fixitCountersChanged,
            this,
            [this](int scheduled, int scheduable){
                m_selectFixitsCheckBox->setEnabled(scheduable > 0);
                m_applyFixitsButton->setEnabled(scheduled > 0);

                if (scheduled == 0)
                    m_selectFixitsCheckBox->setCheckState(Qt::Unchecked);
                else if (scheduled == scheduable)
                    m_selectFixitsCheckBox->setCheckState(Qt::Checked);
                else
                    m_selectFixitsCheckBox->setCheckState(Qt::PartiallyChecked);

                updateForCurrentState();
            });
    connect(m_applyFixitsButton, &QToolButton::clicked, this, [this] {
        QVector<DiagnosticItem *> diagnosticItems;
        m_diagnosticModel->forItemsAtLevel<2>([&](DiagnosticItem *item){
            diagnosticItems += item;
        });

        ApplyFixIts(diagnosticItems).apply(m_diagnosticModel);
    });

    // Open Project Settings
    action = new QAction(this);
    action->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
    //action->setToolTip(Tr::tr("Open Project Settings")); // TODO: Uncomment in master.
    connect(action, &QAction::triggered, [] {
        ProjectExplorerPlugin::activateProjectPanel(Constants::PROJECT_PANEL_ID);
    });
    m_openProjectSettings = action;

    ActionContainer *menu = ActionManager::actionContainer(Debugger::Constants::M_DEBUG_ANALYZER);
    const QString toolTip = Tr::tr("Clang-Tidy and Clazy use a customized Clang executable from the "
                                   "Clang project to search for diagnostics.");

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(1);
    mainLayout->addWidget(m_infoBarWidget);
    mainLayout->addWidget(m_diagnosticView);
    auto mainWidget = new QWidget;
    mainWidget->setObjectName(id.toString() + "IssuesView");
    mainWidget->setWindowTitle(name);
    mainWidget->setLayout(mainLayout);

    m_perspective.addWindow(mainWidget, Perspective::SplitVertical, nullptr);

    action = new QAction(name, this);
    action->setToolTip(toolTip);
    menu->addAction(ActionManager::registerAction(action, id),
                    Debugger::Constants::G_ANALYZER_TOOLS);
    QObject::connect(action, &QAction::triggered, this, [this] {
        startTool(FileSelectionType::AskUser);
    });
    QObject::connect(m_startAction, &QAction::triggered, action, &QAction::triggered);
    QObject::connect(m_startAction, &QAction::changed, action, [action, this] {
        action->setEnabled(m_startAction->isEnabled());
    });

    QObject::connect(m_startOnCurrentFileAction, &QAction::triggered, this, [this] {
        startTool(FileSelectionType::CurrentFile);
    });

    m_perspective.addToolBarAction(m_startAction);
    m_perspective.addToolBarAction(ProxyAction::proxyActionWithIcon(
                                       m_startOnCurrentFileAction,
                                       Utils::Icons::RUN_FILE_TOOLBAR.icon()));
    m_perspective.addToolBarAction(m_stopAction);
    m_perspective.addToolBarAction(m_openProjectSettings);
    m_perspective.addToolbarSeparator();
    m_perspective.addToolBarAction(m_loadExported);
    m_perspective.addToolBarAction(m_clear);
    m_perspective.addToolbarSeparator();
    m_perspective.addToolBarAction(m_expandCollapse);
    m_perspective.addToolBarAction(m_goBack);
    m_perspective.addToolBarAction(m_goNext);
    m_perspective.addToolbarSeparator();
    m_perspective.addToolBarAction(m_showFilter);
    m_perspective.addToolBarWidget(m_selectFixitsCheckBox);
    m_perspective.addToolBarWidget(m_applyFixitsButton);
    m_perspective.registerNextPrevShortcuts(m_goNext, m_goBack);

    update();

    connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::runActionsUpdated,
            this, &ClangTool::update);
    connect(CppModelManager::instance(), &CppModelManager::projectPartsUpdated,
            this, &ClangTool::update);
    connect(ClangToolsSettings::instance(), &ClangToolsSettings::changed,
            this, &ClangTool::update);
}

void ClangTool::selectPerspective()
{
    m_perspective.select();
}

void ClangTool::startTool(ClangTool::FileSelection fileSelection)
{
    const RunSettings theRunSettings = runSettings();
    startTool(fileSelection, theRunSettings, diagnosticConfig(theRunSettings.diagnosticConfigId()));
}

static bool continueDespiteReleaseBuild(const QString &toolName)
{
    const QString wrongMode = Tr::tr("Release");
    const QString title = Tr::tr("Run %1 in %2 Mode?").arg(toolName, wrongMode);
    const QString problem
        = Tr::tr(
              "You are trying to run the tool \"%1\" on an application in %2 mode. The tool is "
              "designed to be used in Debug mode since enabled assertions can reduce the number of "
              "false positives.")
              .arg(toolName, wrongMode);
    const QString question = Tr::tr(
                                 "Do you want to continue and run the tool in %1 mode?")
                                 .arg(wrongMode);
    const QString message = QString("<html><head/><body>"
                                    "<p>%1</p>"
                                    "<p>%2</p>"
                                    "</body></html>")
                                .arg(problem, question);
    return CheckableMessageBox::question(ICore::dialogParent(),
                                         title,
                                         message,
                                         QString("ClangToolsCorrectModeWarning"))
           == QMessageBox::Yes;
}

void ClangTool::startTool(ClangTool::FileSelection fileSelection,
                          const RunSettings &runSettings,
                          const CppEditor::ClangDiagnosticConfig &diagnosticConfig)
{
    Project *project = ProjectManager::startupProject();
    QTC_ASSERT(project, return);
    QTC_ASSERT(project->activeTarget(), return);

    // Continue despite release mode?
    if (BuildConfiguration *bc = project->activeTarget()->activeBuildConfiguration()) {
        if (bc->buildType() == BuildConfiguration::Release)
            if (!continueDespiteReleaseBuild(m_name))
                return;
    }

    TaskHub::clearTasks(taskCategory());

    // Collect files to analyze
    const FileInfos fileInfos = collectFileInfos(project, fileSelection);
    if (fileInfos.empty())
        return;

    // Reset
    reset();

    // Run control
    m_runControl = new RunControl(Constants::CLANGTIDYCLAZY_RUN_MODE);
    m_runControl->setDisplayName(m_name);
    m_runControl->setIcon(ProjectExplorer::Icons::ANALYZER_START_SMALL_TOOLBAR);
    m_runControl->setTarget(project->activeTarget());
    m_stopAction->disconnect();
    connect(m_stopAction, &QAction::triggered, m_runControl, [this] {
        emit m_runControl->appendMessage(Tr::tr("%1 tool stopped by user.").arg(m_name),
                                         NormalMessageFormat);
        m_runControl->initiateStop();
        setState(State::StoppedByUser);
    });
    connect(m_runControl, &RunControl::stopped, this, &ClangTool::onRunControlStopped);

    // Run worker
    const bool preventBuild = std::holds_alternative<FilePath>(fileSelection)
                              || std::get<FileSelectionType>(fileSelection)
                                     == FileSelectionType::CurrentFile;
    const bool buildBeforeAnalysis = !preventBuild && runSettings.buildBeforeAnalysis();
    m_runWorker = new ClangToolRunWorker(this,
                                         m_runControl,
                                         runSettings,
                                         diagnosticConfig,
                                         fileInfos,
                                         buildBeforeAnalysis);
    connect(m_runWorker, &ClangToolRunWorker::buildFailed,this, &ClangTool::onBuildFailed);
    connect(m_runWorker, &ClangToolRunWorker::startFailed, this, &ClangTool::onStartFailed);
    connect(m_runWorker, &ClangToolRunWorker::started, this, &ClangTool::onStarted);
    connect(m_runWorker, &ClangToolRunWorker::runnerFinished, this, [this] {
        m_filesCount = m_runWorker->totalFilesToAnalyze();
        m_filesSucceeded = m_runWorker->filesAnalyzed();
        m_filesFailed = m_runWorker->filesNotAnalyzed();
        updateForCurrentState();
    });

    // More init and UI update
    m_diagnosticFilterModel->setProject(project);
    m_perspective.select();
    if (buildBeforeAnalysis)
        m_infoBarWidget->setInfoText("Waiting for build to finish...");
    setState(State::PreparationStarted);

    // Start
    ProjectExplorerPlugin::startRunControl(m_runControl);
}

FileInfos ClangTool::collectFileInfos(Project *project, FileSelection fileSelection)
{
    FileSelectionType *selectionType = std::get_if<FileSelectionType>(&fileSelection);
    // early bailout
    if (selectionType && *selectionType == FileSelectionType::CurrentFile
        && !EditorManager::currentDocument()) {
        TaskHub::addTask(Task::Error, Tr::tr("Cannot analyze current file: No files open."),
                                         taskCategory());
        TaskHub::requestPopup();
        return {};
    }

    const auto projectInfo = CppEditor::CppModelManager::instance()->projectInfo(project);
    QTC_ASSERT(projectInfo, return FileInfos());

    const FileInfos allFileInfos = sortedFileInfos(projectInfo->projectParts());

    if (selectionType && *selectionType == FileSelectionType::AllFiles)
        return allFileInfos;

    if (selectionType && *selectionType == FileSelectionType::AskUser) {
        static int initialProviderIndex = 0;
        SelectableFilesDialog dialog(project,
                                     fileInfoProviders(project, allFileInfos),
                                     initialProviderIndex);
        if (dialog.exec() == QDialog::Rejected)
            return FileInfos();
        initialProviderIndex = dialog.currentProviderIndex();
        return dialog.fileInfos();
    }

    const FilePath filePath = std::holds_alternative<FilePath>(fileSelection)
                                  ? std::get<FilePath>(fileSelection)
                                  : EditorManager::currentDocument()->filePath(); // see early bailout
    if (!filePath.isEmpty()) {
        const FileInfo fileInfo = Utils::findOrDefault(allFileInfos, [&](const FileInfo &fi) {
            return fi.file == filePath;
        });
        if (!fileInfo.file.isEmpty())
            return {fileInfo};
        TaskHub::addTask(Task::Error,
                         Tr::tr("Cannot analyze current file: \"%1\" is not a known source file.")
                         .arg(filePath.toUserOutput()),
                         taskCategory());
        TaskHub::requestPopup();
    }

    return {};
}

const QString &ClangTool::name() const
{
    return m_name;
}

void ClangTool::initDiagnosticView()
{
    m_diagnosticView->setFrameStyle(QFrame::NoFrame);
    m_diagnosticView->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_diagnosticView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_diagnosticView->setAutoScroll(false);
}

void ClangTool::loadDiagnosticsFromFiles()
{
    // Ask user for files
    const FilePaths filePaths
        = FileUtils::getOpenFilePaths(nullptr,
                                      Tr::tr("Select YAML Files with Diagnostics"),
                                      FileUtils::homePath(),
                                      Tr::tr("YAML Files (*.yml *.yaml);;All Files (*)"));
    if (filePaths.isEmpty())
        return;

    // Load files
    Diagnostics diagnostics;
    QStringList errors;
    for (const FilePath &filePath : filePaths) {
        if (expected_str<Diagnostics> expectedDiagnostics = readExportedDiagnostics(filePath))
            diagnostics << *expectedDiagnostics;
        else
            errors.append(expectedDiagnostics.error());
    }

    // Show errors
    if (!errors.isEmpty()) {
        AsynchronousMessageBox::critical(Tr::tr("Error Loading Diagnostics"), errors.join('\n'));
        return;
    }

    // Show imported
    reset();
    onNewDiagnosticsAvailable(diagnostics, /*generateMarks =*/ true);
    setState(State::ImportFinished);
}

DiagnosticItem *ClangTool::diagnosticItem(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    TreeItem *item = m_diagnosticModel->itemForIndex(m_diagnosticFilterModel->mapToSource(index));
    if (item->level() == 3)
        item = item->parent();
    if (item->level() == 2)
        return static_cast<DiagnosticItem *>(item);

    return {};
}

void ClangTool::showOutputPane()
{
    ProjectExplorerPlugin::showOutputPaneForRunControl(m_runControl);
}

void ClangTool::reset()
{
    m_clear->setEnabled(false);
    m_showFilter->setEnabled(false);
    m_showFilter->setChecked(false);
    m_selectFixitsCheckBox->setEnabled(false);
    m_applyFixitsButton->setEnabled(false);

    m_diagnosticModel->clear();
    m_diagnosticFilterModel->reset();

    m_infoBarWidget->reset();

    m_state = State::Initial;
    m_runControl = nullptr;
    m_runWorker = nullptr;

    m_filesCount = 0;
    m_filesSucceeded = 0;
    m_filesFailed = 0;
}

static bool canAnalyzeProject(Project *project)
{
    if (const Target *target = project->activeTarget()) {
        const Utils::Id c = ProjectExplorer::Constants::C_LANGUAGE_ID;
        const Utils::Id cxx = ProjectExplorer::Constants::CXX_LANGUAGE_ID;
        const bool projectSupportsLanguage = project->projectLanguages().contains(c)
                                             || project->projectLanguages().contains(cxx);
        return projectSupportsLanguage
               && CppModelManager::instance()->projectInfo(project)
               && ToolChainKitAspect::cxxToolChain(target->kit());
    }
    return false;
}

struct CheckResult {
    enum {
        InvalidExecutable,
        ProjectNotOpen,
        ProjectNotSuitable,
        ReadyToAnalyze,
    } kind;
    QString errorText;
};

static CheckResult canAnalyze(ClangToolType type, const QString &name)
{
    const ClangDiagnosticConfig config = diagnosticConfig(runSettings().diagnosticConfigId());

    if (toolEnabled(type, config, runSettings())
        && !toolExecutable(type).isExecutableFile()) {
        return {CheckResult::InvalidExecutable, Tr::tr("Set a valid %1 executable.").arg(name)};
    }

    if (Project *project = ProjectManager::startupProject()) {
        if (!canAnalyzeProject(project)) {
            return {CheckResult::ProjectNotSuitable,
                    Tr::tr("Project \"%1\" is not a C/C++ project.")
                        .arg(project->displayName())};
        }
    } else {
        return {CheckResult::ProjectNotOpen,
                Tr::tr("Open a C/C++ project to start analyzing.")};
    }

    return {CheckResult::ReadyToAnalyze, {}};
}

void ClangTool::updateForInitialState()
{
    if (m_state != State::Initial)
        return;

    m_infoBarWidget->reset();

    const CheckResult result = canAnalyze(m_type, m_name);
    switch (result.kind)
    case CheckResult::InvalidExecutable: {
        m_infoBarWidget->setError(InfoBarWidget::Warning, makeLink(result.errorText),
                                  [] { ICore::showOptionsDialog(Constants::SETTINGS_PAGE_ID); });
        break;
    case CheckResult::ProjectNotSuitable:
    case CheckResult::ProjectNotOpen:
    case CheckResult::ReadyToAnalyze:
        break;
        }
}

void ClangTool::help()
{
    if (DiagnosticItem *item = diagnosticItem(m_diagnosticView->currentIndex())) {
        const QString url = documentationUrl(item->diagnostic().name);
        if (!url.isEmpty())
            QDesktopServices::openUrl(url);
    }
}

void ClangTool::setFilterOptions(const OptionalFilterOptions &filterOptions)
{
    m_diagnosticFilterModel->setFilterOptions(filterOptions);
    const bool isFilterActive = filterOptions
                                    ? (filterOptions->checks != m_diagnosticModel->allChecks())
                                    : false;
    m_showFilter->setChecked(isFilterActive);
}

void ClangTool::filter()
{
    const OptionalFilterOptions filterOptions = m_diagnosticFilterModel->filterOptions();

    // Collect available and currently shown checks
    QHash<QString, Check> checks;
    m_diagnosticModel->forItemsAtLevel<2>([&](DiagnosticItem *item) {
        const QString checkName = item->diagnostic().name;
        Check &check = checks[checkName];
        if (check.name.isEmpty()) {
            check.name = checkName;
            check.displayName = checkName;
            check.count = 1;
            check.isShown = filterOptions ? filterOptions->checks.contains(checkName) : true;
            check.hasFixit = check.hasFixit || item->diagnostic().hasFixits;
            checks.insert(check.name, check);
        } else {
            ++check.count;
        }
    });

    // Show dialog
    FilterDialog dialog(checks.values());
    if (dialog.exec() == QDialog::Rejected)
        return;

    // Apply filter
    setFilterOptions(FilterOptions{dialog.selectedChecks()});
}

void ClangTool::clearFilter()
{
    m_diagnosticFilterModel->setFilterOptions({});
    m_showFilter->setChecked(false);
}

void ClangTool::filterForCurrentKind()
{
    if (DiagnosticItem *item = diagnosticItem(m_diagnosticView->currentIndex()))
        setFilterOptions(FilterOptions{{item->diagnostic().name}});
}

void ClangTool::filterOutCurrentKind()
{
    if (DiagnosticItem *item = diagnosticItem(m_diagnosticView->currentIndex())) {
        const OptionalFilterOptions filterOpts = m_diagnosticFilterModel->filterOptions();
        QSet<QString> checks = filterOpts ? filterOpts->checks : m_diagnosticModel->allChecks();
        checks.remove(item->diagnostic().name);

        setFilterOptions(FilterOptions{checks});
    }
}

void ClangTool::onBuildFailed()
{
    m_infoBarWidget->setError(InfoBarWidget::Error, Tr::tr("Failed to build the project."),
                              [this] { showOutputPane(); });
    setState(State::PreparationFailed);
}

void ClangTool::onStartFailed()
{
    m_infoBarWidget->setError(InfoBarWidget::Error, makeLink(Tr::tr("Failed to start the analyzer.")),
                              [this] { showOutputPane(); });
    setState(State::PreparationFailed);
}

void ClangTool::onStarted()
{
    setState(State::AnalyzerRunning);
}

void ClangTool::onRunControlStopped()
{
    if (m_state != State::StoppedByUser && m_state != State::PreparationFailed)
        setState(State::AnalyzerFinished);
    emit finished(m_infoBarWidget->errorText());
}

void ClangTool::update()
{
    updateForInitialState();
    updateForCurrentState();
}

using DocumentPredicate = std::function<bool(Core::IDocument *)>;

static FileInfos fileInfosMatchingDocuments(const FileInfos &fileInfos,
                                            const DocumentPredicate &predicate)
{
    QSet<Utils::FilePath> documentPaths;
    for (const Core::DocumentModel::Entry *e : Core::DocumentModel::entries()) {
        if (predicate(e->document))
            documentPaths.insert(e->filePath());
    }

    return Utils::filtered(fileInfos, [documentPaths](const FileInfo &fileInfo) {
        return documentPaths.contains(fileInfo.file);
    });
}

static FileInfos fileInfosMatchingOpenedDocuments(const FileInfos &fileInfos)
{
    // Note that (initially) suspended text documents are still IDocuments, not yet TextDocuments.
    return fileInfosMatchingDocuments(fileInfos, [](Core::IDocument *) { return true; });
}

static FileInfos fileInfosMatchingEditedDocuments(const FileInfos &fileInfos)
{
    return fileInfosMatchingDocuments(fileInfos, [](Core::IDocument *document) {
        if (auto textDocument = qobject_cast<TextEditor::TextDocument*>(document))
            return textDocument->document()->revision() > 1;
        return false;
    });
}

FileInfoProviders ClangTool::fileInfoProviders(ProjectExplorer::Project *project,
                                               const FileInfos &allFileInfos)
{
    const QSharedPointer<ClangToolsProjectSettings> s = ClangToolsProjectSettings::getSettings(project);
    static FileInfoSelection openedFilesSelection;
    static FileInfoSelection editeddFilesSelection;

    return {
        {Tr::tr("All Files"),
         allFileInfos,
         FileInfoSelection{s->selectedDirs(), s->selectedFiles()},
         FileInfoProvider::Limited,
         [s](const FileInfoSelection &selection) {
             s->setSelectedDirs(selection.dirs);
             s->setSelectedFiles(selection.files);
         }},

        {Tr::tr("Opened Files"),
         fileInfosMatchingOpenedDocuments(allFileInfos),
         openedFilesSelection,
         FileInfoProvider::All,
         [](const FileInfoSelection &selection) { openedFilesSelection = selection; }},

        {Tr::tr("Edited Files"),
         fileInfosMatchingEditedDocuments(allFileInfos),
         editeddFilesSelection,
         FileInfoProvider::All,
         [](const FileInfoSelection &selection) { editeddFilesSelection = selection; }},
    };
}

void ClangTool::setState(ClangTool::State state)
{
    m_state = state;
    updateForCurrentState();
}

QSet<Diagnostic> ClangTool::diagnostics() const
{
    return Utils::filtered(m_diagnosticModel->diagnostics(), [](const Diagnostic &diagnostic) {
        using CppEditor::ProjectFile;
        return ProjectFile::isSource(ProjectFile::classify(diagnostic.location.filePath.toString()));
    });
}

void ClangTool::onNewDiagnosticsAvailable(const Diagnostics &diagnostics, bool generateMarks)
{
    QTC_ASSERT(m_diagnosticModel, return);
    m_diagnosticModel->addDiagnostics(diagnostics, generateMarks);
}

void ClangTool::updateForCurrentState()
{
    // Actions
    bool canStart = false;
    const bool isPreparing = m_state == State::PreparationStarted;
    const bool isRunning = m_state == State::AnalyzerRunning;
    QString startActionToolTip = m_startAction->text();
    QString startOnCurrentToolTip = m_startOnCurrentFileAction->text();
    if (!isRunning) {
        const CheckResult result = canAnalyze(m_type, m_name);
        canStart = result.kind == CheckResult::ReadyToAnalyze;
        if (!canStart) {
            startActionToolTip = result.errorText;
            startOnCurrentToolTip = result.errorText;
        }
    }
    m_startAction->setEnabled(canStart);
    m_startAction->setToolTip(startActionToolTip);
    m_startOnCurrentFileAction->setEnabled(canStart);
    m_startOnCurrentFileAction->setToolTip(startOnCurrentToolTip);
    m_stopAction->setEnabled(isPreparing || isRunning);

    const int issuesFound = m_diagnosticModel->diagnostics().count();
    const int issuesVisible = m_diagnosticFilterModel->diagnostics();
    m_goBack->setEnabled(issuesVisible > 0);
    m_goNext->setEnabled(issuesVisible > 0);
    m_clear->setEnabled(!isRunning);
    m_expandCollapse->setEnabled(issuesVisible);
    m_loadExported->setEnabled(!isRunning);
    m_showFilter->setEnabled(issuesFound > 1);

    // Diagnostic view
    m_diagnosticView->setCursor(isRunning ? Qt::BusyCursor : Qt::ArrowCursor);

    // Info bar: errors
    if (m_filesFailed > 0) {
        const QString currentErrorText = m_infoBarWidget->errorText();
        const QString newErrorText = makeLink(Tr::tr("Failed to analyze %n file(s).", nullptr,
                                                 m_filesFailed));
        if (newErrorText != currentErrorText)
            m_infoBarWidget->setError(InfoBarWidget::Warning, newErrorText,
                                      [this] { showOutputPane(); });
    }

    // Info bar: info
    bool showProgressIcon = false;
    QString infoText;
    switch (m_state) {
    case State::Initial:
        infoText = m_infoBarWidget->infoText();
        break;
    case State::AnalyzerRunning:
        showProgressIcon = true;
        if (m_filesCount == 0) {
            infoText = Tr::tr("Analyzing..."); // Not yet fully started/initialized
        } else {
            infoText = Tr::tr("Analyzing... %1 of %n file(s) processed.", nullptr, m_filesCount)
                           .arg(m_filesSucceeded + m_filesFailed);
        }
        break;
    case State::PreparationStarted:
        showProgressIcon = true;
        infoText = m_infoBarWidget->infoText();
        break;
    case State::PreparationFailed:
        break; // OK, we just show an error.
    case State::StoppedByUser:
        infoText = Tr::tr("Analysis stopped by user.");
        break;
    case State::AnalyzerFinished:
        infoText = Tr::tr("Finished processing %n file(s).", nullptr, m_filesCount);
        break;
    case State::ImportFinished:
        infoText = Tr::tr("Diagnostics imported.");
        break;
    }
    m_infoBarWidget->setInfoText(infoText);
    m_infoBarWidget->setInfoIcon(showProgressIcon ? InfoBarWidget::ProgressIcon
                                                  : InfoBarWidget::InfoIcon);

    // Info bar: diagnostic stats
    QString diagText;
    if (issuesFound) {
        diagText = Tr::tr("%1 diagnostics. %2 fixits, %3 selected.")
                   .arg(issuesVisible)
                   .arg(m_diagnosticFilterModel->fixitsScheduable())
                   .arg(m_diagnosticFilterModel->fixitsScheduled());
    } else if (m_state != State::AnalyzerRunning
               && m_state != State::Initial
               && m_state != State::PreparationStarted
               && m_state != State::PreparationFailed) {
        diagText = Tr::tr("No diagnostics.");
    }
    m_infoBarWidget->setDiagText(diagText);
}

ClangTidyTool::ClangTidyTool() : ClangTool(Tr::tr("Clang-Tidy"), "ClangTidy.Perspective",
                                           ClangToolType::Tidy)
{
    m_instance = this;
}
ClazyTool::ClazyTool() : ClangTool(Tr::tr("Clazy"), "Clazy.Perspective", ClangToolType::Clazy)
{
    m_instance = this;
}

} // namespace Internal
} // namespace ClangTools

#include "clangtool.moc"
