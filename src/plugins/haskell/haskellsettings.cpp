// Copyright (c) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "haskellsettings.h"

#include "haskellconstants.h"
#include "haskelltr.h"

#include <utils/hostosinfo.h>
#include <utils/layoutbuilder.h>

using namespace Utils;

namespace Haskell::Internal {

static HaskellSettings *theSettings;

HaskellSettings &settings()
{
    return *theSettings;
}

HaskellSettings::HaskellSettings()
{
    theSettings = this;

    setId(Constants::OPTIONS_GENERAL);
    setDisplayName(Tr::tr("General"));
    setCategory("J.Z.Haskell");
    setDisplayCategory(Tr::tr("Haskell"));
    setCategoryIconPath(":/haskell/images/settingscategory_haskell.png");

    stackPath.setSettingsKey("Haskell/StackExecutable");
    stackPath.setExpectedKind(PathChooser::ExistingCommand);
    stackPath.setPromptDialogTitle(Tr::tr("Choose Stack Executable"));
    stackPath.setCommandVersionArguments({"--version"});

    // stack from brew or the installer script from https://docs.haskellstack.org
    // install to /usr/local/bin.
    stackPath.setDefaultFilePath(HostOsInfo::isAnyUnixHost()
        ? FilePath::fromString("/usr/local/bin/stack")
        : FilePath::fromString("stack"));

    setLayouter([this] {
        using namespace Layouting;
        return Column {
            Group {
                title(Tr::tr("General")),
                Row { Tr::tr("Stack executable:"), stackPath }
            },
            st,
        };
    });

    readSettings();
}

} // Haskell::Internal
