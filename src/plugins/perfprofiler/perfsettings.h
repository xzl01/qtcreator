// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "perfprofiler_global.h"

#include <projectexplorer/runconfiguration.h>

#include <QObject>

namespace PerfProfiler {

class PERFPROFILER_EXPORT PerfSettings final : public ProjectExplorer::ISettingsAspect
{
    Q_OBJECT

public:
    explicit PerfSettings(ProjectExplorer::Target *target = nullptr);
    ~PerfSettings() final;

    void readGlobalSettings();
    void writeGlobalSettings() const;

    void addPerfRecordArguments(Utils::CommandLine *cmd) const;

    void resetToDefault();

    Utils::IntegerAspect period{this};
    Utils::IntegerAspect stackSize{this};
    Utils::SelectionAspect sampleMode{this};
    Utils::SelectionAspect callgraphMode{this};
    Utils::StringListAspect events{this};
    Utils::StringAspect extraArguments{this};
};

} // namespace PerfProfiler
