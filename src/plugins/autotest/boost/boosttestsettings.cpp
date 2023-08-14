// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "boosttestsettings.h"

#include "boosttestconstants.h"

#include "../autotestconstants.h"
#include "../autotesttr.h"

#include <utils/layoutbuilder.h>

using namespace Layouting;
using namespace Utils;

namespace Autotest::Internal {

BoostTestSettings::BoostTestSettings(Id settingsId)
{
    setId(settingsId);
    setCategory(Constants::AUTOTEST_SETTINGS_CATEGORY);
    setDisplayName(Tr::tr(BoostTest::Constants::FRAMEWORK_SETTINGS_CATEGORY));
    setSettingsGroups("Autotest", "BoostTest");

    setLayouter([this] {
        return Row { Form {
            logLevel, br,
            reportLevel, br,
            randomize, Row { seed }, br,
            systemErrors, br,
            fpExceptions, br,
            memLeaks,
        }, st};
    });

    logLevel.setSettingsKey("LogLevel");
    logLevel.setDisplayStyle(SelectionAspect::DisplayStyle::ComboBox);
    logLevel.addOption("All");
    logLevel.addOption("Success");
    logLevel.addOption("Test Suite");
    logLevel.addOption("Unit Scope");
    logLevel.addOption("Message");
    logLevel.addOption("Warning");
    logLevel.addOption("Error");
    logLevel.addOption("C++ Exception");
    logLevel.addOption("System Error");
    logLevel.addOption("Fatal Error");
    logLevel.addOption("Nothing");
    logLevel.setDefaultValue(int(LogLevel::Warning));
    logLevel.setLabelText(Tr::tr("Log format:"));

    reportLevel.setSettingsKey("ReportLevel");
    reportLevel.setDisplayStyle(SelectionAspect::DisplayStyle::ComboBox);
    reportLevel.addOption("Confirm");
    reportLevel.addOption("Short");
    reportLevel.addOption("Detailed");
    reportLevel.addOption("No");
    reportLevel.setDefaultValue(int(ReportLevel::Confirm));
    reportLevel.setLabelText(Tr::tr("Report level:"));

    seed.setSettingsKey("Seed");
    seed.setEnabled(false);
    seed.setRange(0, INT_MAX); // UINT_MAX would be correct, but inner QSpinBox is limited to int
    seed.setLabelText(Tr::tr("Seed:"));
    seed.setToolTip(Tr::tr("A seed of 0 means no randomization. A value of 1 uses the current "
                           "time, any other value is used as random seed generator."));
    seed.setEnabler(&randomize);

    randomize.setSettingsKey("Randomize");
    randomize.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBoxWithoutDummyLabel);
    randomize.setLabelText(Tr::tr("Randomize"));
    randomize.setToolTip(Tr::tr("Randomize execution order."));

    systemErrors.setSettingsKey("SystemErrors");
    systemErrors.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBoxWithoutDummyLabel);
    systemErrors.setLabelText(Tr::tr("Catch system errors"));
    systemErrors.setToolTip(Tr::tr("Catch or ignore system errors."));

    fpExceptions.setSettingsKey("FPExceptions");
    fpExceptions.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBoxWithoutDummyLabel);
    fpExceptions.setLabelText(Tr::tr("Floating point exceptions"));
    fpExceptions.setToolTip(Tr::tr("Enable floating point exception traps."));

    memLeaks.setSettingsKey("MemoryLeaks");
    memLeaks.setLabelPlacement(BoolAspect::LabelPlacement::AtCheckBoxWithoutDummyLabel);
    memLeaks.setDefaultValue(true);
    memLeaks.setLabelText(Tr::tr("Detect memory leaks"));
    memLeaks.setToolTip(Tr::tr("Enable memory leak detection."));
}

QString BoostTestSettings::logLevelToOption(const LogLevel logLevel)
{
    switch (logLevel) {
    case LogLevel::All: return QString("all");
    case LogLevel::Success: return QString("success");
    case LogLevel::TestSuite: return QString("test_suite");
    case LogLevel::UnitScope: return QString("unit_scope");
    case LogLevel::Message: return QString("message");
    case LogLevel::Error: return QString("error");
    case LogLevel::CppException: return QString("cpp_exception");
    case LogLevel::SystemError: return QString("system_error");
    case LogLevel::FatalError: return QString("fatal_error");
    case LogLevel::Nothing: return QString("nothing");
    case LogLevel::Warning: return QString("warning");
    }
    return {};
}

QString BoostTestSettings::reportLevelToOption(const ReportLevel reportLevel)
{
    switch (reportLevel) {
    case ReportLevel::Confirm: return QString("confirm");
    case ReportLevel::Short: return QString("short");
    case ReportLevel::Detailed: return QString("detailed");
    case ReportLevel::No: return QString("no");
    }
    return {};
}

} // Autotest::Internal
