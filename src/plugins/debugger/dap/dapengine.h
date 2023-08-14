// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <debugger/debuggerengine.h>
#include <utils/process.h>

#include <QVariant>

namespace Debugger::Internal {

class DebuggerCommand;
class GdbMi;

/*
 * A debugger engine for the debugger adapter protocol.
 */

class DapEngine : public DebuggerEngine
{
public:
    DapEngine();

private:
    void executeStepIn(bool) override;
    void executeStepOut() override;
    void executeStepOver(bool) override;

    void setupEngine() override;
    void shutdownInferior() override;
    void shutdownEngine() override;

    bool canHandleToolTip(const DebuggerToolTipContext &) const override;

    void continueInferior() override;
    void interruptInferior() override;

    void executeRunToLine(const ContextData &data) override;
    void executeRunToFunction(const QString &functionName) override;
    void executeJumpToLine(const ContextData &data) override;

    void activateFrame(int index) override;
    void selectThread(const Thread &thread) override;

    bool acceptsBreakpoint(const BreakpointParameters &bp) const override;
    void insertBreakpoint(const Breakpoint &bp) override;
    void updateBreakpoint(const Breakpoint &bp) override;
    void removeBreakpoint(const Breakpoint &bp) override;

    void assignValueInDebugger(WatchItem *item,
        const QString &expr, const QVariant &value) override;
    void executeDebuggerCommand(const QString &command) override;

    void loadSymbols(const Utils::FilePath &moduleName) override;
    void loadAllSymbols() override;
    void requestModuleSymbols(const Utils::FilePath &moduleName) override;
    void reloadModules() override;
    void reloadRegisters() override {}
    void reloadSourceFiles() override {}
    void reloadFullStack() override {}

    bool supportsThreads() const { return true; }
    void updateItem(const QString &iname) override;

    void runCommand(const DebuggerCommand &cmd) override;
    void postDirectCommand(const QJsonObject &ob);

    void refreshLocation(const GdbMi &reportedLocation);
    void refreshStack(const GdbMi &stack);
    void refreshLocals(const GdbMi &vars);
    void refreshModules(const GdbMi &modules);
    void refreshState(const GdbMi &reportedState);
    void refreshSymbols(const GdbMi &symbols);

    QString errorMessage(QProcess::ProcessError error) const;
    bool hasCapability(unsigned cap) const override;

    void claimInitialBreakpoints();

    void handleDabStarted();
    void handleDabLaunch();
    void handleDabConfigurationDone();

    void handleDapDone();
    void readDapStandardOutput();
    void readDapStandardError();
    void handleOutput(const QJsonDocument &data);
    void handleResponse(const QString &ba);
    void updateAll() override;
    void updateLocals() override;

    QByteArray m_inbuffer;
    Utils::Process m_proc;
    int m_nextBreakpointId = 1;
};

} // Debugger::Internal
