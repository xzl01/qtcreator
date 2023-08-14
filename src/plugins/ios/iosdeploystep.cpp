// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "iosdeploystep.h"

#include "iosconstants.h"
#include "iosdevice.h"
#include "iosrunconfiguration.h"
#include "iossimulator.h"
#include "iostoolhandler.h"
#include "iostr.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/devicesupport/devicemanager.h>

#include <utils/temporaryfile.h>

#include <QFile>
#include <QSettings>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ios::Internal {

class IosDeployStep final : public BuildStep
{
public:
    enum TransferStatus {
        NoTransfer,
        TransferInProgress,
        TransferOk,
        TransferFailed
    };

    IosDeployStep(BuildStepList *bc, Utils::Id id);

private:
    void cleanup();

    void doRun() final;
    void doCancel() final;

    void handleIsTransferringApp(IosToolHandler *handler, const FilePath &bundlePath,
                                 const QString &deviceId, int progress, int maxProgress,
                                 const QString &info);
    void handleDidTransferApp(IosToolHandler *handler, const FilePath &bundlePath,
                              const QString &deviceId, IosToolHandler::OpStatus status);
    void handleFinished(IosToolHandler *handler);
    void handleErrorMsg(IosToolHandler *handler, const QString &msg);
    void updateDisplayNames();

    bool init() final;
    QWidget *createConfigWidget() final;
    IDevice::ConstPtr device() const;
    IosDevice::ConstPtr iosdevice() const;
    IosSimulator::ConstPtr iossimulator() const;

    QString deviceId() const;
    void checkProvisioningProfile();

    TransferStatus m_transferStatus = NoTransfer;
    IosToolHandler *m_toolHandler = nullptr;
    IDevice::ConstPtr m_device;
    FilePath m_bundlePath;
    IosDeviceType m_deviceType;
    bool m_expectFail = false;
};

IosDeployStep::IosDeployStep(BuildStepList *parent, Utils::Id id)
    : BuildStep(parent, id)
{
    setImmutable(true);
    updateDisplayNames();
    connect(DeviceManager::instance(), &DeviceManager::updated,
            this, &IosDeployStep::updateDisplayNames);
    connect(target(), &Target::kitChanged,
            this, &IosDeployStep::updateDisplayNames);
}

void IosDeployStep::updateDisplayNames()
{
    IDevice::ConstPtr dev = DeviceKitAspect::device(kit());
    const QString devName = dev.isNull() ? IosDevice::name() : dev->displayName();
    setDisplayName(Tr::tr("Deploy to %1").arg(devName));
}

bool IosDeployStep::init()
{
    QTC_ASSERT(m_transferStatus == NoTransfer, return false);
    m_device = DeviceKitAspect::device(kit());
    auto runConfig = qobject_cast<const IosRunConfiguration *>(
        this->target()->activeRunConfiguration());
    QTC_ASSERT(runConfig, return false);
    m_bundlePath = runConfig->bundleDirectory();

    if (iosdevice()) {
        m_deviceType = IosDeviceType(IosDeviceType::IosDevice, deviceId());
    } else if (iossimulator()) {
        m_deviceType = runConfig->deviceType();
    } else {
        emit addOutput(Tr::tr("Error: no device available, deploy failed."),
                       OutputFormat::ErrorMessage);
        return false;
    }
    return true;
}

void IosDeployStep::doRun()
{
    QTC_CHECK(m_transferStatus == NoTransfer);
    if (m_device.isNull()) {
        TaskHub::addTask(
                    DeploymentTask(Task::Error, Tr::tr("Deployment failed. No iOS device found.")));
        emit finished(!iossimulator().isNull());
        cleanup();
        return;
    }
    m_toolHandler = new IosToolHandler(m_deviceType, this);
    m_transferStatus = TransferInProgress;
    emit progress(0, Tr::tr("Transferring application"));
    connect(m_toolHandler, &IosToolHandler::isTransferringApp,
            this, &IosDeployStep::handleIsTransferringApp);
    connect(m_toolHandler, &IosToolHandler::didTransferApp,
            this, &IosDeployStep::handleDidTransferApp);
    connect(m_toolHandler, &IosToolHandler::finished,
            this, &IosDeployStep::handleFinished);
    connect(m_toolHandler, &IosToolHandler::errorMsg,
            this, &IosDeployStep::handleErrorMsg);
    checkProvisioningProfile();
    m_toolHandler->requestTransferApp(m_bundlePath, m_deviceType.identifier);
}

void IosDeployStep::doCancel()
{
    if (m_toolHandler)
        m_toolHandler->stop();
}

void IosDeployStep::cleanup()
{
    QTC_CHECK(m_transferStatus != TransferInProgress);
    m_transferStatus = NoTransfer;
    m_device.clear();
    m_toolHandler = nullptr;
    m_expectFail = false;
}

void IosDeployStep::handleIsTransferringApp(IosToolHandler *handler, const FilePath &bundlePath,
                                            const QString &deviceId, int progress, int maxProgress,
                                            const QString &info)
{
    Q_UNUSED(handler); Q_UNUSED(bundlePath); Q_UNUSED(deviceId)
    QTC_CHECK(m_transferStatus == TransferInProgress);
    emit this->progress(progress * 100 / maxProgress, info);
}

void IosDeployStep::handleDidTransferApp(IosToolHandler *handler, const FilePath &bundlePath,
                                         const QString &deviceId, IosToolHandler::OpStatus status)
{
    Q_UNUSED(handler); Q_UNUSED(bundlePath); Q_UNUSED(deviceId)
    QTC_CHECK(m_transferStatus == TransferInProgress);
    if (status == IosToolHandler::Success) {
        m_transferStatus = TransferOk;
    } else {
        m_transferStatus = TransferFailed;
        if (!m_expectFail)
            TaskHub::addTask(DeploymentTask(Task::Error,
                 Tr::tr("Deployment failed. The settings in the Devices window of Xcode might be incorrect.")));
    }
    emit finished(status == IosToolHandler::Success);
}

void IosDeployStep::handleFinished(IosToolHandler *handler)
{
    switch (m_transferStatus) {
    case TransferInProgress:
        m_transferStatus = TransferFailed;
        TaskHub::addTask(DeploymentTask(Task::Error, Tr::tr("Deployment failed.")));
        emit finished(false);
        break;
    case NoTransfer:
    case TransferOk:
    case TransferFailed:
        break;
    }
    cleanup();
    handler->deleteLater();
    // move it when result is reported? (would need care to avoid problems with concurrent runs)
}

void IosDeployStep::handleErrorMsg(IosToolHandler *handler, const QString &msg)
{
    Q_UNUSED(handler)
    if (msg.contains(QLatin1String("AMDeviceInstallApplication returned -402653103")))
        TaskHub::addTask(DeploymentTask(Task::Warning, Tr::tr("The Info.plist might be incorrect.")));

    emit addOutput(msg, OutputFormat::ErrorMessage);
}

QWidget *IosDeployStep::createConfigWidget()
{
    auto widget = new QWidget;

    widget->setObjectName("IosDeployStepWidget");

    connect(this, &ProjectConfiguration::displayNameChanged,
            this, &BuildStep::updateSummary);

    return widget;
}

QString IosDeployStep::deviceId() const
{
    if (iosdevice().isNull())
        return QString();
    return iosdevice()->uniqueDeviceID();
}

void IosDeployStep::checkProvisioningProfile()
{
    IosDevice::ConstPtr device = iosdevice();
    if (device.isNull())
        return;

    const FilePath provisioningFilePath = m_bundlePath.pathAppended("embedded.mobileprovision");

    // the file is a signed plist stored in DER format
    // we simply search for start and end of the plist instead of decoding the DER payload
    if (!provisioningFilePath.exists())
        return;
    QFile provisionFile(provisioningFilePath.toString());
    if (!provisionFile.open(QIODevice::ReadOnly))
        return;
    QByteArray provisionData = provisionFile.readAll();
    int start = provisionData.indexOf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    int end = provisionData.indexOf("</plist>");
    if (start == -1 || end == -1)
        return;
    end += 8;

    TemporaryFile f("iosdeploy");
    if (!f.open())
        return;
    f.write(provisionData.mid(start, end - start));
    f.flush();
    QSettings provisionPlist(f.fileName(), QSettings::NativeFormat);

    if (!provisionPlist.contains(QLatin1String("ProvisionedDevices")))
        return;
    const QStringList deviceIds = provisionPlist.value("ProvisionedDevices").toStringList();
    const QString targetId = device->uniqueDeviceID();
    for (const QString &deviceId : deviceIds) {
        if (deviceId == targetId)
            return;
    }

    m_expectFail = true;
    QString provisioningProfile = provisionPlist.value(QLatin1String("Name")).toString();
    QString provisioningUid = provisionPlist.value(QLatin1String("UUID")).toString();
    CompileTask task(Task::Warning,
              Tr::tr("The provisioning profile \"%1\" (%2) used to sign the application "
                 "does not cover the device %3 (%4). Deployment to it will fail.")
              .arg(provisioningProfile, provisioningUid, device->displayName(),
                   targetId));
    emit addTask(task);
}

IosDevice::ConstPtr IosDeployStep::iosdevice() const
{
    return m_device.dynamicCast<const IosDevice>();
}

IosSimulator::ConstPtr IosDeployStep::iossimulator() const
{
    return m_device.dynamicCast<const IosSimulator>();
}

// IosDeployStepFactory

IosDeployStepFactory::IosDeployStepFactory()
{
    registerStep<IosDeployStep>(Constants::IOS_DEPLOY_STEP_ID);
    setDisplayName(Tr::tr("Deploy to iOS device"));
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_DEPLOY);
    setSupportedDeviceTypes({Constants::IOS_DEVICE_TYPE, Constants::IOS_SIMULATOR_TYPE});
    setRepeatable(false);
}

} // Ios::Internal
