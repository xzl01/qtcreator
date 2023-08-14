// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "processstep.h"

#include "abstractprocessstep.h"
#include "buildconfiguration.h"
#include "kit.h"
#include "processparameters.h"
#include "projectexplorerconstants.h"
#include "projectexplorertr.h"

#include <utils/aspects.h>
#include <utils/fileutils.h>
#include <utils/outputformatter.h>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

const char PROCESS_COMMAND_KEY[] = "ProjectExplorer.ProcessStep.Command";
const char PROCESS_WORKINGDIRECTORY_KEY[] = "ProjectExplorer.ProcessStep.WorkingDirectory";
const char PROCESS_ARGUMENTS_KEY[] = "ProjectExplorer.ProcessStep.Arguments";

class ProcessStep final : public AbstractProcessStep
{
public:
    ProcessStep(BuildStepList *bsl, Id id);

    void setupOutputFormatter(OutputFormatter *formatter) final;
};

ProcessStep::ProcessStep(BuildStepList *bsl, Id id)
    : AbstractProcessStep(bsl, id)
{
    auto command = addAspect<FilePathAspect>();
    command->setSettingsKey(PROCESS_COMMAND_KEY);
    command->setLabelText(Tr::tr("Command:"));
    command->setExpectedKind(PathChooser::Command);
    command->setHistoryCompleter("PE.ProcessStepCommand.History");

    auto arguments = addAspect<StringAspect>();
    arguments->setSettingsKey(PROCESS_ARGUMENTS_KEY);
    arguments->setDisplayStyle(StringAspect::LineEditDisplay);
    arguments->setLabelText(Tr::tr("Arguments:"));

    auto workingDirectory = addAspect<FilePathAspect>();
    workingDirectory->setSettingsKey(PROCESS_WORKINGDIRECTORY_KEY);
    workingDirectory->setValue(Constants::DEFAULT_WORKING_DIR);
    workingDirectory->setLabelText(Tr::tr("Working directory:"));
    workingDirectory->setExpectedKind(PathChooser::Directory);

    setWorkingDirectoryProvider([this, workingDirectory] {
        const FilePath workingDir = workingDirectory->filePath();
        if (workingDir.isEmpty())
            return FilePath::fromString(fallbackWorkingDirectory());
        return workingDir;
    });

    setCommandLineProvider([command, arguments] {
        return CommandLine{command->filePath(), arguments->value(), CommandLine::Raw};
    });

    setSummaryUpdater([this] {
        QString display = displayName();
        if (display.isEmpty())
            display = Tr::tr("Custom Process Step");
        ProcessParameters param;
        setupProcessParameters(&param);
        return param.summary(display);
    });

    addMacroExpander();
}

void ProcessStep::setupOutputFormatter(OutputFormatter *formatter)
{
    formatter->addLineParsers(kit()->createOutputParsers());
    AbstractProcessStep::setupOutputFormatter(formatter);
}

// ProcessStepFactory

ProcessStepFactory::ProcessStepFactory()
{
    registerStep<ProcessStep>("ProjectExplorer.ProcessStep");
    //: Default ProcessStep display name
    setDisplayName(Tr::tr("Custom Process Step"));
}

} // Internal
} // ProjectExplorer
