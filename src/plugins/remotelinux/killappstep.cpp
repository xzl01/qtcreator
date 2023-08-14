// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "killappstep.h"

#include "abstractremotelinuxdeploystep.h"
#include "remotelinux_constants.h"
#include "remotelinuxtr.h"

#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>
#include <projectexplorer/target.h>

#include <utils/qtcassert.h>

using namespace ProjectExplorer;
using namespace Tasking;
using namespace Utils;

namespace RemoteLinux::Internal {

class KillAppStep : public AbstractRemoteLinuxDeployStep
{
public:
    KillAppStep(BuildStepList *bsl, Id id) : AbstractRemoteLinuxDeployStep(bsl, id)
    {
        setWidgetExpandedByDefault(false);

        setInternalInitializer([this] {
            Target * const theTarget = target();
            QTC_ASSERT(theTarget, return CheckResult::failure());
            RunConfiguration * const rc = theTarget->activeRunConfiguration();
            m_remoteExecutable =  rc ? rc->runnable().command.executable() : FilePath();
            return CheckResult::success();
        });
    }

private:
    bool isDeploymentNecessary() const final { return !m_remoteExecutable.isEmpty(); }
    Group deployRecipe() final;

    FilePath m_remoteExecutable;
};

Group KillAppStep::deployRecipe()
{
    const auto setupHandler = [this](DeviceProcessKiller &killer) {
        killer.setProcessPath(m_remoteExecutable);
        addProgressMessage(Tr::tr("Trying to kill \"%1\" on remote device...")
                                  .arg(m_remoteExecutable.path()));
    };
    const auto doneHandler = [this](const DeviceProcessKiller &) {
        addProgressMessage(Tr::tr("Remote application killed."));
    };
    const auto errorHandler = [this](const DeviceProcessKiller &) {
        addProgressMessage(Tr::tr("Failed to kill remote application. "
                                    "Assuming it was not running."));
    };
    return Group { DeviceProcessKillerTask(setupHandler, doneHandler, errorHandler) };
}

KillAppStepFactory::KillAppStepFactory()
{
    registerStep<KillAppStep>(Constants::KillAppStepId);
    setDisplayName(Tr::tr("Kill current application instance"));
    setSupportedConfiguration(RemoteLinux::Constants::DeployToGenericLinux);
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_DEPLOY);
}

} // RemoteLinux::Internal
