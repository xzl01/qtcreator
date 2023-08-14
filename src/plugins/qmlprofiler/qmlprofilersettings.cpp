// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmlprofilerconstants.h"
#include "qmlprofilerplugin.h"
#include "qmlprofilersettings.h"
#include "qmlprofilertr.h"

#include <coreplugin/icore.h>

#include <debugger/analyzer/analyzericons.h>
#include <debugger/debuggertr.h>

#include <utils/layoutbuilder.h>

#include <QSettings>

using namespace Utils;

namespace QmlProfiler::Internal {

class QmlProfilerOptionsPageWidget : public Core::IOptionsPageWidget
{
public:
    explicit QmlProfilerOptionsPageWidget(QmlProfilerSettings *settings)
    {
        QmlProfilerSettings &s = *settings;

        using namespace Layouting;
        Form {
            s.flushEnabled, br,
            s.flushInterval, br,
            s.aggregateTraces, br,
        }.attachTo(this);
    }

    void apply() final
    {
        QmlProfilerPlugin::globalSettings()->writeGlobalSettings();
    }
};

QmlProfilerSettings::QmlProfilerSettings()
{
    setConfigWidgetCreator([this] { return new QmlProfilerOptionsPageWidget(this); });

    setSettingsGroup(Constants::ANALYZER);

    registerAspect(&flushEnabled);
    flushEnabled.setSettingsKey("Analyzer.QmlProfiler.FlushEnabled");
    flushEnabled.setLabelPlacement(BoolAspect::LabelPlacement::InExtraLabel);
    flushEnabled.setLabelText(Tr::tr("Flush data while profiling:"));
    flushEnabled.setToolTip(Tr::tr(
        "Periodically flush pending data to the profiler. This reduces the delay when loading the\n"
        "data and the memory usage in the application. It distorts the profile as the flushing\n"
        "itself takes time."));

    registerAspect(&flushInterval);
    flushInterval.setSettingsKey("Analyzer.QmlProfiler.FlushInterval");
    flushInterval.setRange(1, 10000000);
    flushInterval.setDefaultValue(1000);
    flushInterval.setLabelText(Tr::tr("Flush interval (ms):"));
    flushInterval.setEnabler(&flushEnabled);

    registerAspect(&lastTraceFile);
    lastTraceFile.setSettingsKey("Analyzer.QmlProfiler.LastTraceFile");

    registerAspect(&aggregateTraces);
    aggregateTraces.setSettingsKey("Analyzer.QmlProfiler.AggregateTraces");
    aggregateTraces.setLabelPlacement(BoolAspect::LabelPlacement::InExtraLabel);
    aggregateTraces.setLabelText(Tr::tr("Process data only when process ends:"));
    aggregateTraces.setToolTip(Tr::tr(
        "Only process data when the process being profiled ends, not when the current recording\n"
        "session ends. This way multiple recording sessions can be aggregated in a single trace,\n"
        "for example if multiple QML engines start and stop sequentially during a single run of\n"
        "the program."));

    // Read stored values
    readSettings(Core::ICore::settings());
}

void QmlProfilerSettings::writeGlobalSettings() const
{
    writeSettings(Core::ICore::settings());
}

// QmlProfilerOptionsPage

QmlProfilerOptionsPage::QmlProfilerOptionsPage()
{
    setId(Constants::SETTINGS);
    setDisplayName(Tr::tr("QML Profiler"));
    setCategory("T.Analyzer");
    setDisplayCategory(::Debugger::Tr::tr("Analyzer"));
    setCategoryIconPath(Analyzer::Icons::SETTINGSCATEGORY_ANALYZER);
    setWidgetCreator([] {
        return new QmlProfilerOptionsPageWidget(QmlProfilerPlugin::globalSettings());
    });
}

} // QmlProfiler::Internal
