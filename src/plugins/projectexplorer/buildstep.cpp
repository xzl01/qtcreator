// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildstep.h"

#include "buildconfiguration.h"
#include "buildsteplist.h"
#include "customparser.h"
#include "deployconfiguration.h"
#include "kitinformation.h"
#include "project.h"
#include "projectexplorerconstants.h"
#include "sanitizerparser.h"
#include "target.h"

#include <utils/algorithm.h>
#include <utils/fileinprojectfinder.h>
#include <utils/layoutbuilder.h>
#include <utils/outputformatter.h>
#include <utils/qtcassert.h>
#include <utils/variablechooser.h>

/*!
    \class ProjectExplorer::BuildStep

    \brief The BuildStep class provides build steps for projects.

    Build steps are the primary way plugin developers can customize
    how their projects (or projects from other plugins) are built.

    Projects are built by taking the list of build steps
    from the project and calling first \c init() and then \c run() on them.

    To change the way your project is built, reimplement
    this class and add your build step to the build step list of the project.

    \note The projects own the build step. Do not delete them yourself.

    \c init() is called in the GUI thread and can be used to query the
    project for any information you need.

    \c run() is run via Utils::runAsync in a separate thread. If you need an
    event loop, you need to create it yourself.
*/

/*!
    \fn bool ProjectExplorer::BuildStep::init()

    This function is run in the GUI thread. Use it to retrieve any information
    that you need in the run() function.
*/

/*!
    \fn void ProjectExplorer::BuildStep::run(QFutureInterface<bool> &fi)

    Reimplement this function. It is called when the target is built.
    By default, this function is NOT run in the GUI thread, but runs in its
    own thread. If you need an event loop, you need to create one.
    This function should block until the task is done

    The absolute minimal implementation is:
    \code
    fi.reportResult(true);
    \endcode

    By returning \c true from runInGuiThread(), this function is called in
    the GUI thread. Then the function should not block and instead the
    finished() signal should be emitted.

    \sa runInGuiThread()
*/

/*!
    \fn BuildStepConfigWidget *ProjectExplorer::BuildStep::createConfigWidget()

    Returns the Widget shown in the target settings dialog for this build step.
    Ownership is transferred to the caller.
*/

/*!
    \fn  void ProjectExplorer::BuildStep::addTask(const ProjectExplorer::Task &task)
    Adds \a task.
*/

/*!
    \fn  void ProjectExplorer::BuildStep::addOutput(const QString &string, ProjectExplorer::BuildStep::OutputFormat format,
              ProjectExplorer::BuildStep::OutputNewlineSetting newlineSetting = DoAppendNewline) const

    The \a string is added to the generated output, usually in the output.
    It should be in plain text, with the format in the parameter.
*/

/*!
    \fn  void ProjectExplorer::BuildStep::finished()
    This signal needs to be emitted if the build step runs in the GUI thread.
*/

/*!
    \fn bool ProjectExplorer::BuildStep::isImmutable()

    If this function returns \c true, the user cannot delete this build step for
    this target and the user is prevented from changing the order in which
    immutable steps are run. The default implementation returns \c false.
*/

using namespace Utils;

static const char buildStepEnabledKey[] = "ProjectExplorer.BuildStep.Enabled";

namespace ProjectExplorer {

static QList<BuildStepFactory *> g_buildStepFactories;

BuildStep::BuildStep(BuildStepList *bsl, Id id) :
    ProjectConfiguration(bsl, id)
{
    QTC_CHECK(bsl->target() && bsl->target() == this->target());
    connect(this, &ProjectConfiguration::displayNameChanged,
            this, &BuildStep::updateSummary);
//    m_displayName = step->displayName();
//    m_summaryText = "<b>" + m_displayName + "</b>";
}

BuildStep::~BuildStep()
{
    emit finished(false);
}

void BuildStep::run()
{
    m_cancelFlag = false;
    doRun();
}

void BuildStep::cancel()
{
    m_cancelFlag = true;
    doCancel();
}

QWidget *BuildStep::doCreateConfigWidget()
{
    QWidget *widget = createConfigWidget();

    const auto recreateSummary = [this] {
        if (m_summaryUpdater)
            setSummaryText(m_summaryUpdater());
    };

    for (BaseAspect *aspect : std::as_const(*this))
        connect(aspect, &BaseAspect::changed, widget, recreateSummary);

    if (buildConfiguration()) {
        connect(buildConfiguration(), &BuildConfiguration::buildDirectoryChanged,
                widget, recreateSummary);
    }

    recreateSummary();

    return widget;
}

QWidget *BuildStep::createConfigWidget()
{
    Layouting::Form form;
    for (BaseAspect *aspect : std::as_const(*this)) {
        if (aspect->isVisible())
            form.addItems({aspect, Layouting::br()});
    }
    form.addItem(Layouting::noMargin);
    auto widget = form.emerge();

    if (m_addMacroExpander)
        VariableChooser::addSupportForChildWidgets(widget, macroExpander());

    return widget;
}

bool BuildStep::fromMap(const QVariantMap &map)
{
    m_enabled = map.value(buildStepEnabledKey, true).toBool();
    return ProjectConfiguration::fromMap(map);
}

QVariantMap BuildStep::toMap() const
{
    QVariantMap map = ProjectConfiguration::toMap();
    map.insert(buildStepEnabledKey, m_enabled);
    return map;
}

BuildConfiguration *BuildStep::buildConfiguration() const
{
    auto config = qobject_cast<BuildConfiguration *>(parent()->parent());
    if (config)
        return config;

    // step is not part of a build configuration, use active build configuration of step's target
    return target()->activeBuildConfiguration();
}

DeployConfiguration *BuildStep::deployConfiguration() const
{
    auto config = qobject_cast<DeployConfiguration *>(parent()->parent());
    if (config)
        return config;
    // See comment in buildConfiguration()
    QTC_CHECK(false);
    // step is not part of a deploy configuration, use active deploy configuration of step's target
    return target()->activeDeployConfiguration();
}

ProjectConfiguration *BuildStep::projectConfiguration() const
{
    return static_cast<ProjectConfiguration *>(parent()->parent());
}

BuildSystem *BuildStep::buildSystem() const
{
    if (auto bc = buildConfiguration())
        return bc->buildSystem();
    return target()->buildSystem();
}

Environment BuildStep::buildEnvironment() const
{
    if (const auto bc = qobject_cast<BuildConfiguration *>(parent()->parent()))
        return bc->environment();
    if (const auto bc = target()->activeBuildConfiguration())
        return bc->environment();
    return Environment::systemEnvironment();
}

FilePath BuildStep::buildDirectory() const
{
    if (auto bc = buildConfiguration())
        return bc->buildDirectory();
    return {};
}

BuildConfiguration::BuildType BuildStep::buildType() const
{
    if (auto bc = buildConfiguration())
        return bc->buildType();
    return BuildConfiguration::Unknown;
}

MacroExpander *BuildStep::macroExpander() const
{
    if (auto bc = buildConfiguration())
        return bc->macroExpander();
    return globalMacroExpander();
}

QString BuildStep::fallbackWorkingDirectory() const
{
    if (buildConfiguration())
        return {Constants::DEFAULT_WORKING_DIR};
    return {Constants::DEFAULT_WORKING_DIR_ALTERNATE};
}

void BuildStep::setupOutputFormatter(OutputFormatter *formatter)
{
    if (qobject_cast<BuildConfiguration *>(parent()->parent())) {
        for (const Id id : buildConfiguration()->customParsers()) {
            if (Internal::CustomParser * const parser = Internal::CustomParser::createFromId(id))
                formatter->addLineParser(parser);
        }

        formatter->addLineParser(new Internal::SanitizerParser);
        formatter->setForwardStdOutToStdError(buildConfiguration()->parseStdOut());
    }
    FileInProjectFinder fileFinder;
    fileFinder.setProjectDirectory(project()->projectDirectory());
    fileFinder.setProjectFiles(project()->files(Project::AllFiles));
    formatter->setFileFinder(fileFinder);
}

bool BuildStep::widgetExpandedByDefault() const
{
    return m_widgetExpandedByDefault;
}

void BuildStep::setWidgetExpandedByDefault(bool widgetExpandedByDefault)
{
    m_widgetExpandedByDefault = widgetExpandedByDefault;
}

QVariant BuildStep::data(Id id) const
{
    Q_UNUSED(id)
    return {};
}

bool BuildStep::isCanceled() const
{
    return m_cancelFlag;
}

void BuildStep::doCancel()
{
    QTC_ASSERT(false, qWarning() << "Build step" << displayName()
               << "neeeds to implement the doCancel() function");
}

void BuildStep::addMacroExpander()
{
    m_addMacroExpander = true;
}

void BuildStep::setEnabled(bool b)
{
    if (m_enabled == b)
        return;
    m_enabled = b;
    emit enabledChanged();
}

BuildStepList *BuildStep::stepList() const
{
    return qobject_cast<BuildStepList *>(parent());
}

bool BuildStep::enabled() const
{
    return m_enabled;
}

BuildStepFactory::BuildStepFactory()
{
    g_buildStepFactories.append(this);
}

BuildStepFactory::~BuildStepFactory()
{
    g_buildStepFactories.removeOne(this);
}

const QList<BuildStepFactory *> BuildStepFactory::allBuildStepFactories()
{
    return g_buildStepFactories;
}

bool BuildStepFactory::canHandle(BuildStepList *bsl) const
{
    if (!m_supportedStepLists.isEmpty() && !m_supportedStepLists.contains(bsl->id()))
        return false;

    auto config = qobject_cast<ProjectConfiguration *>(bsl->parent());

    if (!m_supportedDeviceTypes.isEmpty()) {
        Target *target = bsl->target();
        QTC_ASSERT(target, return false);
        Id deviceType = DeviceTypeKitAspect::deviceTypeId(target->kit());
        if (!m_supportedDeviceTypes.contains(deviceType))
            return false;
    }

    if (m_supportedProjectType.isValid()) {
        if (!config)
            return false;
        Id projectId = config->project()->id();
        if (projectId != m_supportedProjectType)
            return false;
    }

    if (!m_isRepeatable && bsl->contains(m_stepId))
        return false;

    if (m_supportedConfiguration.isValid()) {
        if (!config)
            return false;
        Id configId = config->id();
        if (configId != m_supportedConfiguration)
            return false;
    }

    return true;
}

QString BuildStepFactory::displayName() const
{
    return m_displayName;
}

void BuildStepFactory::cloneStepCreator(Id exitstingStepId, Id overrideNewStepId)
{
    m_stepId = {};
    m_creator = {};
    for (BuildStepFactory *factory : BuildStepFactory::allBuildStepFactories()) {
        if (factory->m_stepId == exitstingStepId) {
            m_creator = factory->m_creator;
            m_stepId = factory->m_stepId;
            m_displayName = factory->m_displayName;
            // Other bits are intentionally not copied as they are unlikely to be
            // useful in the cloner's context. The cloner can/has to finish the
            // setup on its own.
            break;
        }
    }
    // Existence should be guaranteed by plugin dependencies. In case it fails,
    // bark and keep the factory in a state where the invalid m_stepId keeps it
    // inaction.
    QTC_ASSERT(m_creator, return);
    if (overrideNewStepId.isValid())
        m_stepId = overrideNewStepId;
}

void BuildStepFactory::setDisplayName(const QString &displayName)
{
    m_displayName = displayName;
}

void BuildStepFactory::setFlags(BuildStep::Flags flags)
{
    m_flags = flags;
}

void BuildStepFactory::setSupportedStepList(Id id)
{
    m_supportedStepLists = {id};
}

void BuildStepFactory::setSupportedStepLists(const QList<Id> &ids)
{
    m_supportedStepLists = ids;
}

void BuildStepFactory::setSupportedConfiguration(Id id)
{
    m_supportedConfiguration = id;
}

void BuildStepFactory::setSupportedProjectType(Id id)
{
    m_supportedProjectType = id;
}

void BuildStepFactory::setSupportedDeviceType(Id id)
{
    m_supportedDeviceTypes = {id};
}

void BuildStepFactory::setSupportedDeviceTypes(const QList<Id> &ids)
{
    m_supportedDeviceTypes = ids;
}

BuildStep::Flags BuildStepFactory::stepFlags() const
{
    return m_flags;
}

Id BuildStepFactory::stepId() const
{
    return m_stepId;
}

BuildStep *BuildStepFactory::create(BuildStepList *parent)
{
    QTC_ASSERT(m_creator, return nullptr);
    BuildStep *step = m_creator(parent);
    step->setDefaultDisplayName(m_displayName);
    return step;
}

BuildStep *BuildStepFactory::restore(BuildStepList *parent, const QVariantMap &map)
{
    BuildStep *bs = create(parent);
    if (!bs)
        return nullptr;
    if (!bs->fromMap(map)) {
        QTC_CHECK(false);
        delete bs;
        return nullptr;
    }
    return bs;
}

QString BuildStep::summaryText() const
{
    if (m_summaryText.isEmpty())
        return QString("<b>%1</b>").arg(displayName());

    return m_summaryText;
}

void BuildStep::setSummaryText(const QString &summaryText)
{
    if (summaryText != m_summaryText) {
        m_summaryText = summaryText;
        emit updateSummary();
    }
}

void BuildStep::setSummaryUpdater(const std::function<QString()> &summaryUpdater)
{
    m_summaryUpdater = summaryUpdater;
}

} // ProjectExplorer
