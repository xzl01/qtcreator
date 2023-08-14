// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "catchtestsettings.h"

#include "../autotestconstants.h"
#include "../autotesttr.h"

#include <utils/layoutbuilder.h>

using namespace Layouting;
using namespace Utils;

namespace Autotest::Internal {

CatchTestSettings::CatchTestSettings(Id settingsId)
{
    setId(settingsId);
    setCategory(Constants::AUTOTEST_SETTINGS_CATEGORY);
    setDisplayName(Tr::tr("Catch Test"));
    setSettingsGroups("Autotest", "Catch2");

    setLayouter([this] {
        return Row { Form {
            showSuccess, br,
            breakOnFailure, br,
            noThrow, br,
            visibleWhitespace, br,
            abortAfterChecked, abortAfter, br,
            samplesChecked, benchmarkSamples, br,
            resamplesChecked, benchmarkResamples, br,
            confidenceIntervalChecked, confidenceInterval, br,
            warmupChecked, benchmarkWarmupTime, br,
            noAnalysis
        }, st };
    });

    abortAfter.setSettingsKey("AbortAfter");
    abortAfter.setRange(1, 9999);
    abortAfter.setEnabler(&abortAfterChecked);

    benchmarkSamples.setSettingsKey("BenchSamples");
    benchmarkSamples.setRange(1, 999999);
    benchmarkSamples.setDefaultValue(100);
    benchmarkSamples.setEnabler(&samplesChecked);

    benchmarkResamples.setSettingsKey("BenchResamples");
    benchmarkResamples.setRange(1, 9999999);
    benchmarkResamples.setDefaultValue(100000);
    benchmarkResamples.setToolTip(Tr::tr("Number of resamples for bootstrapping."));
    benchmarkResamples.setEnabler(&resamplesChecked);

    confidenceInterval.setSettingsKey("BenchConfInt");
    confidenceInterval.setRange(0., 1.);
    confidenceInterval.setSingleStep(0.05);
    confidenceInterval.setDefaultValue(0.95);
    confidenceInterval.setEnabler(&confidenceIntervalChecked);

    benchmarkWarmupTime.setSettingsKey("BenchWarmup");
    benchmarkWarmupTime.setSuffix(Tr::tr(" ms"));
    benchmarkWarmupTime.setRange(0, 10000);
    benchmarkWarmupTime.setEnabler(&warmupChecked);

    abortAfterChecked.setSettingsKey("AbortChecked");
    abortAfterChecked.setLabelText(Tr::tr("Abort after"));
    abortAfterChecked.setToolTip(Tr::tr("Aborts after the specified number of failures."));

    samplesChecked.setSettingsKey("SamplesChecked");
    samplesChecked.setLabelText(Tr::tr("Benchmark samples"));
    samplesChecked.setToolTip(Tr::tr("Number of samples to collect while running benchmarks."));

    resamplesChecked.setSettingsKey("ResamplesChecked");
    resamplesChecked.setLabelText(Tr::tr("Benchmark resamples"));
    resamplesChecked.setToolTip(Tr::tr("Number of resamples used for statistical bootstrapping."));

    confidenceIntervalChecked.setSettingsKey("ConfIntChecked");
    confidenceIntervalChecked.setToolTip(Tr::tr("Confidence interval used for statistical bootstrapping."));
    confidenceIntervalChecked.setLabelText(Tr::tr("Benchmark confidence interval"));

    warmupChecked.setSettingsKey("WarmupChecked");
    warmupChecked.setLabelText(Tr::tr("Benchmark warmup time"));
    warmupChecked.setToolTip(Tr::tr("Warmup time for each test."));

    noAnalysis.setSettingsKey("NoAnalysis");
    noAnalysis.setLabelText(Tr::tr("Disable analysis"));
    noAnalysis.setToolTip(Tr::tr("Disables statistical analysis and bootstrapping."));

    showSuccess.setSettingsKey("ShowSuccess");
    showSuccess.setLabelText(Tr::tr("Show success"));
    showSuccess.setToolTip(Tr::tr("Show success for tests."));

    breakOnFailure.setSettingsKey("BreakOnFailure");
    breakOnFailure.setDefaultValue(true);
    breakOnFailure.setLabelText(Tr::tr("Break on failure while debugging"));
    breakOnFailure.setToolTip(Tr::tr("Turns failures into debugger breakpoints."));

    noThrow.setSettingsKey("NoThrow");
    noThrow.setLabelText(Tr::tr("Skip throwing assertions"));
    noThrow.setToolTip(Tr::tr("Skips all assertions that test for thrown exceptions."));

    visibleWhitespace.setSettingsKey("VisibleWS");
    visibleWhitespace.setLabelText(Tr::tr("Visualize whitespace"));
    visibleWhitespace.setToolTip(Tr::tr("Makes whitespace visible."));

    warnOnEmpty.setSettingsKey("WarnEmpty");
    warnOnEmpty.setLabelText(Tr::tr("Warn on empty tests"));
    warnOnEmpty.setToolTip(Tr::tr("Warns if a test section does not check any assertion."));
}

} // Autotest::Internal
