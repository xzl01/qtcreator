// Copyright (c) 2018 Artur Shepilko
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fossilplugin.h"

#include "commiteditor.h"
#include "configuredialog.h"
#include "constants.h"
#include "fossilclient.h"
#include "fossilcommitwidget.h"
#include "fossileditor.h"
#include "fossiltr.h"
#include "pullorpushdialog.h"
#include "wizard/fossiljsextension.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/vcsmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/helpmanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/locator/commandlocator.h>
#include <coreplugin/jsexpander.h>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projecttree.h>
#include <projectexplorer/project.h>
#include <projectexplorer/jsonwizard/jsonwizardfactory.h>

#include <utils/commandline.h>
#include <utils/layoutbuilder.h>
#include <utils/parameteraction.h>
#include <utils/qtcassert.h>

#include <vcsbase/basevcseditorfactory.h>
#include <vcsbase/basevcssubmiteditorfactory.h>
#include <vcsbase/vcsbaseclient.h>
#include <vcsbase/vcsbaseconstants.h>
#include <vcsbase/vcsbaseeditor.h>
#include <vcsbase/vcsbasesubmiteditor.h>
#include <vcsbase/vcscommand.h>
#include <vcsbase/vcsoutputwindow.h>

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QtPlugin>

using namespace Core;
using namespace Utils;
using namespace VcsBase;
using namespace std::placeholders;

namespace Fossil {
namespace Internal {

class FossilTopicCache : public Core::IVersionControl::TopicCache
{
public:
    FossilTopicCache(FossilClient *client) :
        m_client(client)
    { }

protected:
    FilePath trackFile(const FilePath &repository) final
    {
        return repository.pathAppended(Constants::FOSSILREPO);
    }

    QString refreshTopic(const FilePath &repository) final
    {
        return m_client->synchronousTopic(repository);
    }

private:
    FossilClient *m_client;
};

const VcsBaseEditorParameters fileLogParameters {
    LogOutput,
    Constants::FILELOG_ID,
    Constants::FILELOG_DISPLAY_NAME,
    Constants::LOGAPP
};

const VcsBaseEditorParameters annotateLogParameters {
     AnnotateOutput,
     Constants::ANNOTATELOG_ID,
     Constants::ANNOTATELOG_DISPLAY_NAME,
     Constants::ANNOTATEAPP
};

const VcsBaseEditorParameters diffParameters {
    DiffOutput,
    Constants::DIFFLOG_ID,
    Constants::DIFFLOG_DISPLAY_NAME,
    Constants::DIFFAPP
};

const VcsBaseSubmitEditorParameters submitEditorParameters {
    Constants::COMMITMIMETYPE,
    Constants::COMMIT_ID,
    Constants::COMMIT_DISPLAY_NAME,
    VcsBaseSubmitEditorParameters::DiffFiles
};


class FossilPluginPrivate final : public VcsBase::VcsBasePluginPrivate
{
public:
    enum SyncMode {
        SyncPull,
        SyncPush
    };

    FossilPluginPrivate();

    // IVersionControl
    QString displayName() const final;
    Id id() const final;

    bool isVcsFileOrDirectory(const FilePath &filePath) const final;

    bool managesDirectory(const FilePath &directory, FilePath *topLevel) const final;
    bool managesFile(const FilePath &workingDirectory, const QString &fileName) const final;

    bool isConfigured() const final;
    bool supportsOperation(Operation operation) const final;
    bool vcsOpen(const FilePath &fileName) final;
    bool vcsAdd(const FilePath &fileName) final;
    bool vcsDelete(const FilePath &filename) final;
    bool vcsMove(const FilePath &from, const FilePath &to) final;
    bool vcsCreateRepository(const FilePath &directory) final;

    void vcsAnnotate(const FilePath &file, int line) final;
    void vcsDescribe(const FilePath &source, const QString &id) final;

    VcsCommand *createInitialCheckoutCommand(const QString &url,
                                             const Utils::FilePath &baseDirectory,
                                             const QString &localName,
                                             const QStringList &extraArgs) final;

    void updateActions(VcsBase::VcsBasePluginPrivate::ActionState) override;
    bool activateCommit() override;

    // File menu action slots
    void addCurrentFile();
    void deleteCurrentFile();
    void annotateCurrentFile();
    void diffCurrentFile();
    void logCurrentFile();
    void revertCurrentFile();
    void statusCurrentFile();

    // Directory menu action slots
    void diffRepository();
    void logRepository();
    void revertAll();
    void statusMulti();

    // Repository menu action slots
    void pull() { pullOrPush(SyncPull); }
    void push() { pullOrPush(SyncPush); }
    void update();
    void configureRepository();
    void commit();
    void showCommitWidget(const QList<VcsBase::VcsBaseClient::StatusItem> &status);
    void diffFromEditorSelected(const QStringList &files);
    void createRepository();

    // Methods
    void createMenu(const Core::Context &context);
    void createFileActions(const Core::Context &context);
    void createDirectoryActions(const Core::Context &context);
    void createRepositoryActions(const Core::Context &context);

    bool pullOrPush(SyncMode mode);

    // Variables
    FossilSettings m_settings;
    FossilClient m_client;

    VcsSubmitEditorFactory submitEditorFactory {
        submitEditorParameters,
        [] { return new CommitEditor; },
        this
    };

    VcsEditorFactory fileLogFactory {
        &fileLogParameters,
        [] { return new FossilEditorWidget; },
        std::bind(&FossilPluginPrivate::vcsDescribe, this, _1, _2)
    };

    VcsEditorFactory annotateLogFactory {
        &annotateLogParameters,
        [] { return new FossilEditorWidget; },
        std::bind(&FossilPluginPrivate::vcsDescribe, this, _1, _2)
    };

    VcsEditorFactory diffFactory {
        &diffParameters,
        [] { return new FossilEditorWidget; },
        std::bind(&FossilPluginPrivate::vcsDescribe, this, _1, _2)
    };

    Core::CommandLocator *m_commandLocator = nullptr;
    Core::ActionContainer *m_fossilContainer = nullptr;

    QList<QAction *> m_repositoryActionList;

    // Menu Items (file actions)
    Utils::ParameterAction *m_addAction = nullptr;
    Utils::ParameterAction *m_deleteAction = nullptr;
    Utils::ParameterAction *m_annotateFile = nullptr;
    Utils::ParameterAction *m_diffFile = nullptr;
    Utils::ParameterAction *m_logFile = nullptr;
    Utils::ParameterAction *m_revertFile = nullptr;
    Utils::ParameterAction *m_statusFile = nullptr;

    QAction *m_createRepositoryAction = nullptr;

    // Submit editor actions
    QAction *m_menuAction = nullptr;

    Utils::FilePath m_submitRepository;

    // To be connected to the VcsTask's success signal to emit the repository/
    // files changed signals according to the variant's type:
    // String -> repository, StringList -> files
    void changed(const QVariant &);
};

static FossilPluginPrivate *dd = nullptr;

class RevertDialog : public QDialog
{
public:
    RevertDialog(const QString &title, QWidget *parent = nullptr);

    QLineEdit *m_revisionLineEdit;
};

FossilPlugin::~FossilPlugin()
{
    delete dd;
    dd = nullptr;
}

bool FossilPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments);
    Q_UNUSED(errorMessage);

    dd = new FossilPluginPrivate;

    return true;
}

void FossilPlugin::extensionsInitialized()
{
    dd->extensionsInitialized();
}

FossilClient *FossilPlugin::client()
{
    return &dd->m_client;
}

FossilPluginPrivate::FossilPluginPrivate()
    : VcsBase::VcsBasePluginPrivate(Core::Context(Constants::FOSSIL_CONTEXT))
{
    Core::Context context(Constants::FOSSIL_CONTEXT);

    setTopicCache(new FossilTopicCache(&m_client));
    connect(&m_client, &VcsBase::VcsBaseClient::changed, this, &FossilPluginPrivate::changed);

    m_commandLocator = new Core::CommandLocator("Fossil", "fossil", "fossil", this);
    m_commandLocator->setDescription(Tr::tr("Triggers a Fossil version control operation."));

    ProjectExplorer::JsonWizardFactory::addWizardPath(Utils::FilePath::fromString(Constants::WIZARD_PATH));
    Core::JsExpander::registerGlobalObject("Fossil", [] { return new FossilJsExtension; });

    connect(&settings(), &AspectContainer::changed,
            this, &IVersionControl::configurationChanged);

    createMenu(context);
}

void FossilPluginPrivate::createMenu(const Core::Context &context)
{
    // Create menu item for Fossil
    m_fossilContainer = Core::ActionManager::createMenu("Fossil.FossilMenu");
    QMenu *menu = m_fossilContainer->menu();
    menu->setTitle(Tr::tr("&Fossil"));

    createFileActions(context);
    m_fossilContainer->addSeparator(context);
    createDirectoryActions(context);
    m_fossilContainer->addSeparator(context);
    createRepositoryActions(context);
    m_fossilContainer->addSeparator(context);

    // Request the Tools menu and add the Fossil menu to it
    Core::ActionContainer *toolsMenu = Core::ActionManager::actionContainer(Core::Constants::M_TOOLS);
    toolsMenu->addMenu(m_fossilContainer);
    m_menuAction = m_fossilContainer->menu()->menuAction();
}

void FossilPluginPrivate::createFileActions(const Core::Context &context)
{
    Core::Command *command;

    m_annotateFile = new Utils::ParameterAction(Tr::tr("Annotate Current File"), Tr::tr("Annotate \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_annotateFile, Constants::ANNOTATE, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_annotateFile, &QAction::triggered, this, &FossilPluginPrivate::annotateCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    m_diffFile = new Utils::ParameterAction(Tr::tr("Diff Current File"), Tr::tr("Diff \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_diffFile, Constants::DIFF, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+I,Meta+D")
                                                                      : Tr::tr("Alt+I,Alt+D")));
    connect(m_diffFile, &QAction::triggered, this, &FossilPluginPrivate::diffCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    m_logFile = new Utils::ParameterAction(Tr::tr("Timeline Current File"), Tr::tr("Timeline \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_logFile, Constants::LOG, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+I,Meta+L")
                                                                      : Tr::tr("Alt+I,Alt+L")));
    connect(m_logFile, &QAction::triggered, this, &FossilPluginPrivate::logCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    m_statusFile = new Utils::ParameterAction(Tr::tr("Status Current File"), Tr::tr("Status \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_statusFile, Constants::STATUS, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+I,Meta+S")
                                                                      : Tr::tr("Alt+I,Alt+S")));
    connect(m_statusFile, &QAction::triggered, this, &FossilPluginPrivate::statusCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    m_fossilContainer->addSeparator(context);

    m_addAction = new Utils::ParameterAction(Tr::tr("Add Current File"), Tr::tr("Add \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_addAction, Constants::ADD, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_addAction, &QAction::triggered, this, &FossilPluginPrivate::addCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    m_deleteAction = new Utils::ParameterAction(Tr::tr("Delete Current File..."), Tr::tr("Delete \"%1\"..."), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_deleteAction, Constants::DELETE, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_deleteAction, &QAction::triggered, this, &FossilPluginPrivate::deleteCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    m_revertFile = new Utils::ParameterAction(Tr::tr("Revert Current File..."), Tr::tr("Revert \"%1\"..."), Utils::ParameterAction::EnabledWithParameter, this);
    command = Core::ActionManager::registerAction(m_revertFile, Constants::REVERT, context);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_revertFile, &QAction::triggered, this, &FossilPluginPrivate::revertCurrentFile);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);
}

void FossilPluginPrivate::addCurrentFile()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);
    m_client.synchronousAdd(state.currentFileTopLevel(), state.relativeCurrentFile());
}

void FossilPluginPrivate::deleteCurrentFile()
{
    promptToDeleteCurrentFile();
}

void FossilPluginPrivate::annotateCurrentFile()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);
    const int lineNumber = VcsBase::VcsBaseEditor::lineNumberOfCurrentEditor(state.currentFile());
    m_client.annotate(state.currentFileTopLevel(), state.relativeCurrentFile(), lineNumber);
}

void FossilPluginPrivate::diffCurrentFile()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);
    m_client.diff(state.currentFileTopLevel(), QStringList(state.relativeCurrentFile()));
}

void FossilPluginPrivate::logCurrentFile()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);
    FossilClient::SupportedFeatures features = m_client.supportedFeatures();
    QStringList extraOptions;
    extraOptions << "-n" << QString::number(m_client.settings().logCount());

    if (features.testFlag(FossilClient::TimelineWidthFeature))
        extraOptions << "-W" << QString::number(m_client.settings().timelineWidth());

    // disable annotate context menu for older client versions, used to be supported for current revision only
    bool enableAnnotationContextMenu = features.testFlag(FossilClient::AnnotateRevisionFeature);

    m_client.logCurrentFile(state.currentFileTopLevel(), QStringList(state.relativeCurrentFile()),
                  extraOptions, enableAnnotationContextMenu);
}

void FossilPluginPrivate::revertCurrentFile()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);

    QDialog dialog(Core::ICore::dialogParent());

    auto revisionLineEdit = new QLineEdit;

    if (dialog.exec() != QDialog::Accepted)
        return;
    m_client.revertFile(state.currentFileTopLevel(),
                        state.relativeCurrentFile(),
                        revisionLineEdit->text());
}

void FossilPluginPrivate::statusCurrentFile()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);
    m_client.status(state.currentFileTopLevel(), state.relativeCurrentFile());
}

void FossilPluginPrivate::createDirectoryActions(const Core::Context &context)
{
    QAction *action;
    Core::Command *command;

    action = new QAction(Tr::tr("Diff"), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::DIFFMULTI, context);
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::diffRepository);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Timeline"), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::LOGMULTI, context);
    command->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+I,Meta+T")
                                                                      : Tr::tr("Alt+I,Alt+T")));
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::logRepository);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Revert..."), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::REVERTMULTI, context);
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::revertAll);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Status"), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::STATUSMULTI, context);
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::statusMulti);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);
}


void FossilPluginPrivate::diffRepository()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);
    m_client.diff(state.topLevel());
}

void FossilPluginPrivate::logRepository()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);
    FossilClient::SupportedFeatures features = m_client.supportedFeatures();
    QStringList extraOptions;
    extraOptions << "-n" << QString::number(m_client.settings().logCount());

    if (features.testFlag(FossilClient::TimelineWidthFeature))
        extraOptions << "-W" << QString::number(m_client.settings().timelineWidth());

    m_client.log(state.topLevel(), {}, extraOptions);
}

void FossilPluginPrivate::revertAll()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);

    RevertDialog dialog(Tr::tr("Revert"), Core::ICore::dialogParent());
    if (dialog.exec() == QDialog::Accepted)
        m_client.revertAll(state.topLevel(), dialog.m_revisionLineEdit->text());
}

void FossilPluginPrivate::statusMulti()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);
    m_client.status(state.topLevel());
}

void FossilPluginPrivate::createRepositoryActions(const Core::Context &context)
{
    QAction *action = 0;
    Core::Command *command = 0;

    action = new QAction(Tr::tr("Pull..."), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::PULL, context);
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::pull);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Push..."), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::PUSH, context);
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::push);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Update..."), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::UPDATE, context);
    command->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+I,Meta+U")
                                                                      : Tr::tr("Alt+I,Alt+U")));
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::update);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Commit..."), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::COMMIT, context);
    command->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+I,Meta+C")
                                                                      : Tr::tr("Alt+I,Alt+C")));
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::commit);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    action = new QAction(Tr::tr("Settings..."), this);
    m_repositoryActionList.append(action);
    command = Core::ActionManager::registerAction(action, Constants::CONFIGURE_REPOSITORY, context);
    connect(action, &QAction::triggered, this, &FossilPluginPrivate::configureRepository);
    m_fossilContainer->addAction(command);
    m_commandLocator->appendCommand(command);

    // Register "Create Repository..." action in global context, so that it's visible
    // without active repository to allow creating a new one.
    m_createRepositoryAction = new QAction(Tr::tr("Create Repository..."), this);
    command = Core::ActionManager::registerAction(m_createRepositoryAction, Constants::CREATE_REPOSITORY);
    connect(m_createRepositoryAction, &QAction::triggered, this, &FossilPluginPrivate::createRepository);
    m_fossilContainer->addAction(command);
}

bool FossilPluginPrivate::pullOrPush(FossilPluginPrivate::SyncMode mode)
{
    PullOrPushDialog::Mode pullOrPushMode;
    switch (mode) {
    case SyncPull:
        pullOrPushMode = PullOrPushDialog::PullMode;
        break;
    case SyncPush:
        pullOrPushMode = PullOrPushDialog::PushMode;
        break;
    default:
        return false;
    }

    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return false);

    PullOrPushDialog dialog(pullOrPushMode, Core::ICore::dialogParent());
    dialog.setLocalBaseDirectory(m_client.settings().defaultRepoPath());
    const QString defaultURL(m_client.synchronousGetRepositoryURL(state.topLevel()));
    dialog.setDefaultRemoteLocation(defaultURL);
    if (dialog.exec() != QDialog::Accepted)
        return true;

    QString remoteLocation(dialog.remoteLocation());
    if (remoteLocation.isEmpty() && defaultURL.isEmpty()) {
        VcsBase::VcsOutputWindow::appendError(Tr::tr("Remote repository is not defined."));
        return false;
    } else if (remoteLocation == defaultURL) {
        remoteLocation.clear();
    }

    QStringList extraOptions;
    if (!remoteLocation.isEmpty() && !dialog.isRememberOptionEnabled())
        extraOptions << "--once";
    if (dialog.isPrivateOptionEnabled())
        extraOptions << "--private";
    switch (mode) {
    case SyncPull:
        return m_client.synchronousPull(state.topLevel(), remoteLocation, extraOptions);
    case SyncPush:
        return m_client.synchronousPush(state.topLevel(), remoteLocation, extraOptions);
    default:
        return false;
    }
}

void FossilPluginPrivate::update()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);

    RevertDialog dialog(Tr::tr("Update"), Core::ICore::dialogParent());
    if (dialog.exec() == QDialog::Accepted)
        m_client.update(state.topLevel(), dialog.m_revisionLineEdit->text());
}

void FossilPluginPrivate::configureRepository()
{
    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);

    ConfigureDialog dialog;

    // retrieve current settings from the repository
    RepositorySettings currentSettings = m_client.synchronousSettingsQuery(state.topLevel());

    dialog.setSettings(currentSettings);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const RepositorySettings newSettings = dialog.settings();

    m_client.synchronousConfigureRepository(state.topLevel(), newSettings, currentSettings);
}

void FossilPluginPrivate::commit()
{
    if (!promptBeforeCommit())
        return;

    if (raiseSubmitEditor())
        return;

    const VcsBase::VcsBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);

    m_submitRepository = state.topLevel();
    connect(&m_client, &VcsBaseClient::parsedStatus, this, &FossilPluginPrivate::showCommitWidget);
    m_client.emitParsedStatus(m_submitRepository, {});
}

void FossilPluginPrivate::showCommitWidget(const QList<VcsBase::VcsBaseClient::StatusItem> &status)
{
    //Once we receive our data release the connection so it can be reused elsewhere
    disconnect(&m_client, &VcsBaseClient::parsedStatus,
               this, &FossilPluginPrivate::showCommitWidget);

    if (status.isEmpty()) {
        VcsBase::VcsOutputWindow::appendError(Tr::tr("There are no changes to commit."));
        return;
    }

    // Start new temp file for commit message
    Utils::TempFileSaver saver;
    // Keep the file alive, else it removes self and forgets its name
    saver.setAutoRemove(false);
    if (!saver.finalize()) {
        VcsBase::VcsOutputWindow::appendError(saver.errorString());
        return;
    }

    Core::IEditor *editor = Core::EditorManager::openEditor(saver.filePath(), Constants::COMMIT_ID);
    if (!editor) {
        VcsBase::VcsOutputWindow::appendError(Tr::tr("Unable to create an editor for the commit."));
        return;
    }

    CommitEditor *commitEditor = qobject_cast<CommitEditor *>(editor);

    if (!commitEditor) {
        VcsBase::VcsOutputWindow::appendError(Tr::tr("Unable to create a commit editor."));
        return;
    }
    setSubmitEditor(commitEditor);

    const QString msg = Tr::tr("Commit changes for \"%1\".").arg(m_submitRepository.toUserOutput());
    commitEditor->document()->setPreferredDisplayName(msg);

    const RevisionInfo currentRevision = m_client.synchronousRevisionQuery(m_submitRepository);
    const BranchInfo currentBranch = m_client.synchronousCurrentBranch(m_submitRepository);
    const QString currentUser = m_client.synchronousUserDefaultQuery(m_submitRepository);
    QStringList tags = m_client.synchronousTagQuery(m_submitRepository, currentRevision.id);
    // Fossil includes branch name in tag list -- remove.
    tags.removeAll(currentBranch.name);
    commitEditor->setFields(m_submitRepository, currentBranch, tags, currentUser, status);

    connect(commitEditor, &VcsBase::VcsBaseSubmitEditor::diffSelectedFiles,
            this, &FossilPluginPrivate::diffFromEditorSelected);
    commitEditor->setCheckScriptWorkingDirectory(m_submitRepository);
}

void FossilPluginPrivate::diffFromEditorSelected(const QStringList &files)
{
    m_client.diff(m_submitRepository, files);
}

static inline bool ask(QWidget *parent, const QString &title, const QString &question, bool defaultValue = true)

{
    const QMessageBox::StandardButton defaultButton = defaultValue ? QMessageBox::Yes : QMessageBox::No;
    return QMessageBox::question(parent, title, question, QMessageBox::Yes|QMessageBox::No, defaultButton) == QMessageBox::Yes;
}

void FossilPluginPrivate::createRepository()
{
    // re-implemented from void VcsBasePlugin::createRepository()

    // Find current starting directory
    Utils::FilePath directory;
    if (const ProjectExplorer::Project *currentProject = ProjectExplorer::ProjectTree::currentProject())
        directory = currentProject->projectDirectory();
    // Prompt for a directory that is not under version control yet
    QWidget *mw = Core::ICore::dialogParent();
    do {
        directory = FileUtils::getExistingDirectory(nullptr, Tr::tr("Choose Checkout Directory"), directory);
        if (directory.isEmpty())
            return;
        const Core::IVersionControl *managingControl = Core::VcsManager::findVersionControlForDirectory(directory);
        if (managingControl == 0)
            break;
        const QString question = Tr::tr("The directory \"%1\" is already managed by a version control system (%2)."
                                    " Would you like to specify another directory?").arg(directory.toUserOutput(), managingControl->displayName());

        if (!ask(mw, Tr::tr("Repository already under version control"), question))
            return;
    } while (true);
    // Create
    const bool rc = vcsCreateRepository(directory);
    const QString nativeDir = directory.toUserOutput();
    if (rc) {
        QMessageBox::information(mw, Tr::tr("Repository Created"),
                                 Tr::tr("A version control repository has been created in %1.").
                                 arg(nativeDir));
    } else {
        QMessageBox::warning(mw, Tr::tr("Repository Creation Failed"),
                                 Tr::tr("A version control repository could not be created in %1.").
                                 arg(nativeDir));
    }
}

bool FossilPluginPrivate::activateCommit()
{
    CommitEditor *commitEditor = qobject_cast<CommitEditor *>(submitEditor());
    QTC_ASSERT(commitEditor, return true);
    Core::IDocument *editorDocument = commitEditor->document();
    QTC_ASSERT(editorDocument, return true);

    QStringList files = commitEditor->checkedFiles();
    if (!files.empty()) {
        //save the commit message
        if (!Core::DocumentManager::saveDocument(editorDocument))
            return false;

        //rewrite entries of the form 'file => newfile' to 'newfile' because
        //this would mess the commit command
        for (QStringList::iterator iFile = files.begin(); iFile != files.end(); ++iFile) {
            const QStringList parts = iFile->split(" => ", Qt::SkipEmptyParts);
            if (!parts.isEmpty())
                *iFile = parts.last();
        }

        FossilCommitWidget *commitWidget = commitEditor->commitWidget();
        QStringList extraOptions;
        // Author -- override the repository-default user
        if (!commitWidget->committer().isEmpty())
            extraOptions << "--user" << commitWidget->committer();
        // Branch
        QString branch = commitWidget->newBranch();
        if (!branch.isEmpty()) {
            // @TODO: make enquote utility function
            QString enquotedBranch = branch;
            if (branch.contains(QRegularExpression("\\s")))
                enquotedBranch = QString("\"") + branch + "\"";
            extraOptions << "--branch" << enquotedBranch;
        }
        // Tags
        const QStringList tags = commitWidget->tags();
        for (const QString &tag : tags) {
            extraOptions << "--tag" << tag;
        }

        // Whether local commit or not
        if (commitWidget->isPrivateOptionEnabled())
            extraOptions += "--private";
        m_client.commit(m_submitRepository, files, editorDocument->filePath().toString(), extraOptions);
    }
    return true;
}


void FossilPluginPrivate::updateActions(VcsBase::VcsBasePluginPrivate::ActionState as)
{
    m_createRepositoryAction->setEnabled(true);

    if (!enableMenuAction(as, m_menuAction)) {
        m_commandLocator->setEnabled(false);
        return;
    }
    const QString filename = currentState().currentFileName();
    const bool repoEnabled = currentState().hasTopLevel();
    m_commandLocator->setEnabled(repoEnabled);

    m_annotateFile->setParameter(filename);
    m_diffFile->setParameter(filename);
    m_logFile->setParameter(filename);
    m_addAction->setParameter(filename);
    m_deleteAction->setParameter(filename);
    m_revertFile->setParameter(filename);
    m_statusFile->setParameter(filename);

    for (QAction *repoAction : std::as_const(m_repositoryActionList))
        repoAction->setEnabled(repoEnabled);
}

QString FossilPluginPrivate::displayName() const
{
    return Tr::tr("Fossil");
}

Id FossilPluginPrivate::id() const
{
    return Id(Constants::VCS_ID_FOSSIL);
}

bool FossilPluginPrivate::isVcsFileOrDirectory(const FilePath &filePath) const
{
    return m_client.isVcsFileOrDirectory(filePath);
}

bool FossilPluginPrivate::managesDirectory(const FilePath &directory, FilePath *topLevel) const
{
    const FilePath topLevelFound = m_client.findTopLevelForFile(directory);
    if (topLevel)
        *topLevel = topLevelFound;
    return !topLevelFound.isEmpty();
}

bool FossilPluginPrivate::managesFile(const FilePath &workingDirectory, const QString &fileName) const
{
    return m_client.managesFile(workingDirectory, fileName);
}

bool FossilPluginPrivate::isConfigured() const
{
    const FilePath binary = m_client.vcsBinary();
    if (binary.isEmpty())
        return false;

    if (!binary.isExecutableFile())
        return false;

    // Local repositories default path must be set and exist
    const FilePath repoPath = m_client.settings().defaultRepoPath();
    if (repoPath.isEmpty())
        return false;

    return repoPath.isReadableDir();
}

bool FossilPluginPrivate::supportsOperation(Operation operation) const
{
    bool supported = isConfigured();

    switch (operation) {
    case Core::IVersionControl::AddOperation:
    case Core::IVersionControl::DeleteOperation:
    case Core::IVersionControl::MoveOperation:
    case Core::IVersionControl::CreateRepositoryOperation:
    case Core::IVersionControl::AnnotateOperation:
    case Core::IVersionControl::InitialCheckoutOperation:
        break;
    case Core::IVersionControl::SnapshotOperations:
        supported = false;
        break;
    }
    return supported;
}

bool FossilPluginPrivate::vcsOpen(const FilePath &filePath)
{
    Q_UNUSED(filePath)
    return true;
}

bool FossilPluginPrivate::vcsAdd(const FilePath &filePath)
{
    return m_client.synchronousAdd(filePath.absolutePath(), filePath.fileName());
}

bool FossilPluginPrivate::vcsDelete(const FilePath &filePath)
{
    return m_client.synchronousRemove(filePath.absolutePath(), filePath.fileName());
}

bool FossilPluginPrivate::vcsMove(const FilePath &from, const FilePath &to)
{
    const QFileInfo fromInfo = from.toFileInfo();
    const QFileInfo toInfo = to.toFileInfo();
    return m_client.synchronousMove(from.absolutePath(), fromInfo.absoluteFilePath(),
                                    toInfo.absoluteFilePath());
}

bool FossilPluginPrivate::vcsCreateRepository(const FilePath &directory)
{
    return m_client.synchronousCreateRepository(directory);
}

void FossilPluginPrivate::vcsAnnotate(const FilePath &filePath, int line)
{
    m_client.annotate(filePath.absolutePath(), filePath.fileName(), line);
}

void FossilPluginPrivate::vcsDescribe(const FilePath &source, const QString &id)
{
    m_client.view(source, id);
}

VcsCommand *FossilPluginPrivate::createInitialCheckoutCommand(const QString &sourceUrl,
                                                              const FilePath &baseDirectory,
                                                              const QString &localName,
                                                              const QStringList &extraArgs)
{
    const QMap<QString, QString> options = FossilJsExtension::parseArgOptions(extraArgs);

    // Two operating modes:
    //  1) CloneCheckout:
    //  -- clone from remote-URL or a local-fossil a repository  into a local-clone fossil.
    //  -- open/checkout the local-clone fossil
    //  The local-clone fossil must not point to an existing repository.
    //  Clone URL may be either schema-based (http, ssh, file) or an absolute local path.
    //
    //  2) LocalCheckout:
    //  -- open/checkout an existing local fossil
    //  Clone URL is an absolute local path and is the same as the local fossil.

    const Utils::FilePath checkoutPath = baseDirectory.pathAppended(localName);
    const QString fossilFile = options.value("fossil-file");
    const Utils::FilePath fossilFilePath = Utils::FilePath::fromUserInput(QDir::fromNativeSeparators(fossilFile));
    const QString fossilFileNative = fossilFilePath.toUserOutput();
    const QFileInfo cloneRepository(fossilFilePath.toString());

    // Check when requested to clone a local repository and clone-into repository file is the same
    // or not specified.
    // In this case handle it as local fossil checkout request.
    const QUrl url(sourceUrl);
    bool isLocalRepository = (options.value("repository-type") == "localRepo");

    if (url.isLocalFile() || url.isRelative()) {
        const QFileInfo sourcePath(url.path());
        isLocalRepository = (sourcePath.canonicalFilePath() == cloneRepository.canonicalFilePath());
    }

    // set clone repository admin user to configured user name
    // OR override it with the specified user from clone panel
    const QString adminUser = options.value("admin-user");
    const bool disableAutosync = (options.value("settings-autosync") == "off");
    const QString checkoutBranch = options.value("branch-tag");

    // first create the checkout directory,
    // as it needs to become a working directory for wizard command jobs
    checkoutPath.createDir();

    // Setup the wizard page command job
    auto command = VcsBaseClient::createVcsCommand(checkoutPath, m_client.processEnvironment());

    if (!isLocalRepository
        && !cloneRepository.exists()) {

        const QString sslIdentityFile = options.value("ssl-identity");
        const Utils::FilePath sslIdentityFilePath = Utils::FilePath::fromUserInput(QDir::fromNativeSeparators(sslIdentityFile));
        const bool includePrivate = (options.value("include-private") == "true");

        QStringList extraOptions;
        if (includePrivate)
            extraOptions << "--private";
        if (!sslIdentityFile.isEmpty())
            extraOptions << "--ssl-identity" << sslIdentityFilePath.toUserOutput();
        if (!adminUser.isEmpty())
            extraOptions << "--admin-user" << adminUser;

        // Fossil allows saving the remote address and login. This is used to
        // facilitate autosync (commit/update) functionality.
        // When no password is given, it prompts for that.
        // When both username and password are specified, it prompts whether to
        // save them.
        // NOTE: In non-interactive context, these prompts won't work.
        // Fossil currently does not support SSH_ASKPASS way for login query.
        //
        // Alternatively, "--once" option does not save the remote details.
        // In such case remote details must be provided on the command-line every
        // time. This also precludes autosync.
        //
        // So here we want Fossil to save the remote details when specified.

        QStringList args;
        args << m_client.vcsCommandString(FossilClient::CloneCommand)
             << extraOptions
             << sourceUrl
             << fossilFileNative;
        command->addJob({m_client.vcsBinary(), args}, -1);
    }

    // check out the cloned repository file into the working copy directory;
    // by default the latest revision is checked out

    QStringList args({"open", fossilFileNative});
    if (!checkoutBranch.isEmpty())
        args << checkoutBranch;
    command->addJob({m_client.vcsBinary(), args}, -1);

    // set user default to admin user if specified
    if (!isLocalRepository
        && !adminUser.isEmpty()) {
        const QStringList args({ "user", "default", adminUser, "--user", adminUser});
        command->addJob({m_client.vcsBinary(), args}, -1);
    }

    // turn-off autosync if requested
    if (!isLocalRepository
        && disableAutosync) {
        const QStringList args({"settings", "autosync", "off"});
        command->addJob({m_client.vcsBinary(), args}, -1);
    }

    return command;
}

void FossilPluginPrivate::changed(const QVariant &v)
{
    switch (v.type()) {
    case QVariant::String:
        emit repositoryChanged(Utils::FilePath::fromVariant(v));
        break;
    case QVariant::StringList:
        emit filesChanged(v.toStringList());
        break;
    default:
        break;
    }
}

RevertDialog::RevertDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    resize(600, 0);
    setWindowTitle(title);

    auto *groupBox = new QGroupBox(Tr::tr("Specify a revision other than the default?"));
    groupBox->setCheckable(true);
    groupBox->setChecked(false);
    groupBox->setToolTip(Tr::tr("Checkout revision, can also be a branch or a tag name."));

    m_revisionLineEdit = new QLineEdit;

    auto buttonBox = new QDialogButtonBox;
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    using namespace Layouting;
    Form {
        Tr::tr("Revision"), m_revisionLineEdit, br,
    }.attachTo(groupBox);

    Column {
        groupBox,
        buttonBox,
    }.attachTo(this);
}

} // namespace Internal
} // namespace Fossil

#ifdef WITH_TESTS
#include <QTest>

void Fossil::Internal::FossilPlugin::testDiffFileResolving_data()
{
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<QByteArray>("fileName");

    QTest::newRow("New") << QByteArray(
            "ADDED   src/plugins/fossil/fossilclient.cpp\n"
            "Index: src/plugins/fossil/fossilclient.cpp\n"
            "==================================================================\n"
            "--- src/plugins/fossil/fossilclient.cpp\n"
            "+++ src/plugins/fossil/fossilclient.cpp\n"
            "@@ -0,0 +1,295 @@\n"
        )
        << QByteArray("src/plugins/fossil/fossilclient.cpp");
    QTest::newRow("Deleted") << QByteArray(
            "DELETED src/plugins/fossil/fossilclient.cpp\n"
            "Index: src/plugins/fossil/fossilclient.cpp\n"
            "==================================================================\n"
            "--- src/plugins/fossil/fossilclient.cpp\n"
            "+++ src/plugins/fossil/fossilclient.cpp\n"
            "@@ -1,266 +0,0 @@\n"
        )
        << QByteArray("src/plugins/fossil/fossilclient.cpp");
    QTest::newRow("Modified") << QByteArray(
            "Index: src/plugins/fossil/fossilclient.cpp\n"
            "==================================================================\n"
            "--- src/plugins/fossil/fossilclient.cpp\n"
            "+++ src/plugins/fossil/fossilclient.cpp\n"
            "@@ -112,22 +112,37 @@\n"
        )
        << QByteArray("src/plugins/fossil/fossilclient.cpp");
}

void Fossil::Internal::FossilPlugin::testDiffFileResolving()
{
    VcsBase::VcsBaseEditorWidget::testDiffFileResolving(dd->diffFactory);
}

void Fossil::Internal::FossilPlugin::testLogResolving()
{
    QByteArray data(
        "=== 2014-03-08 ===\n"
        "22:14:02 [ac6d1129b8] Change scaling algorithm. (user: ninja tags: ninja-fixes-5.1)\n"
        "   EDITED src/core/scaler.cpp\n"
        "20:23:51 [56d6917c3b] *BRANCH* Add width option (conditional). (user: ninja tags: ninja-fixes-5.1)\n"
        "   EDITED src/core/scaler.cpp\n"
        "   EDITED src/core/scaler.h\n"
    );
    VcsBase::VcsBaseEditorWidget::testLogResolving(dd->fileLogFactory, data, "ac6d1129b8", "56d6917c3b");
}
#endif
