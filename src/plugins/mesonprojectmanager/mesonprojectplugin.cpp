// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mesonprojectplugin.h"

#include "machinefilemanager.h"
#include "mesonactionsmanager.h"
#include "mesonbuildconfiguration.h"
#include "mesonproject.h"
#include "mesonrunconfiguration.h"
#include "mesontoolkitaspect.h"
#include "ninjabuildstep.h"
#include "ninjatoolkitaspect.h"
#include "settings.h"
#include "toolssettingsaccessor.h"
#include "toolssettingspage.h"

#include <coreplugin/icore.h>

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/runcontrol.h>

#include <utils/fsengine/fileiconprovider.h>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace MesonProjectManager::Internal {

class MesonProjectPluginPrivate : public QObject
{
    Q_OBJECT
public:
    MesonProjectPluginPrivate()
    {
        MesonTools::setTools(m_toolsSettings.loadMesonTools(ICore::dialogParent()));
        connect(ICore::instance(),
                &ICore::saveSettingsRequested,
                this,
                &MesonProjectPluginPrivate::saveAll);
    }

    ~MesonProjectPluginPrivate() {}

private:
    Settings m_settings;
    ToolsSettingsPage m_toolslSettingsPage;
    ToolsSettingsAccessor m_toolsSettings;
    MesonToolKitAspect m_mesonKitAspect;
    NinjaToolKitAspect m_ninjaKitAspect;
    MesonBuildStepFactory m_buildStepFactory;
    MesonBuildConfigurationFactory m_buildConfigurationFactory;
    MesonRunConfigurationFactory m_runConfigurationFactory;
    MesonActionsManager m_actions;
    MachineFileManager m_machineFilesManager;
    SimpleTargetRunnerFactory m_mesonRunWorkerFactory{{m_runConfigurationFactory.runConfigurationId()}};

    void saveAll()
    {
        m_toolsSettings.saveMesonTools(MesonTools::tools(), ICore::dialogParent());
    }
};

MesonProjectPlugin::~MesonProjectPlugin()
{
    delete d;
}

void MesonProjectPlugin::initialize()
{
    d = new MesonProjectPluginPrivate;

    ProjectManager::registerProjectType<MesonProject>(Constants::Project::MIMETYPE);
    FileIconProvider::registerIconOverlayForFilename(Constants::Icons::MESON, "meson.build");
    FileIconProvider::registerIconOverlayForFilename(Constants::Icons::MESON, "meson_options.txt");
}

} // MesonProjectManager::Internal

#include "mesonprojectplugin.moc"
