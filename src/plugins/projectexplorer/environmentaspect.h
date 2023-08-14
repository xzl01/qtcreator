// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.h"

#include "runconfiguration.h"

#include <utils/aspects.h>
#include <utils/environment.h>

#include <QList>
#include <QVariantMap>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT EnvironmentAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    EnvironmentAspect();

    // The environment including the user's modifications.
    Utils::Environment environment() const;

    // Environment including modifiers, but without explicit user changes.
    Utils::Environment modifiedBaseEnvironment() const;

    int baseEnvironmentBase() const;
    void setBaseEnvironmentBase(int base);

    Utils::EnvironmentItems userEnvironmentChanges() const { return m_userChanges; }
    void setUserEnvironmentChanges(const Utils::EnvironmentItems &diff);

    int addSupportedBaseEnvironment(const QString &displayName,
                                    const std::function<Utils::Environment()> &getter);
    int addPreferredBaseEnvironment(const QString &displayName,
                                    const std::function<Utils::Environment()> &getter);

    void setSupportForBuildEnvironment(Target *target);

    QString currentDisplayName() const;

    const QStringList displayNames() const;

    using EnvironmentModifier = std::function<void(Utils::Environment &)>;
    void addModifier(const EnvironmentModifier &);

    bool isLocal() const { return m_isLocal; }

    bool isPrintOnRunAllowed() const { return m_allowPrintOnRun; }
    bool isPrintOnRunEnabled() const { return m_printOnRun; }
    void setPrintOnRun(bool enabled) { m_printOnRun = enabled; }

    struct Data : BaseAspect::Data
    {
        Utils::Environment environment;
    };

    using Utils::BaseAspect::setupLabel;
    using Utils::BaseAspect::label;

signals:
    void baseEnvironmentChanged();
    void userEnvironmentChangesChanged(const Utils::EnvironmentItems &diff);
    void environmentChanged();

protected:
    void fromMap(const QVariantMap &map) override;
    void toMap(QVariantMap &map) const override;

    void setIsLocal(bool local) { m_isLocal = local; }
    void setAllowPrintOnRun(bool allow) { m_allowPrintOnRun = allow; }

    static constexpr char BASE_KEY[] = "PE.EnvironmentAspect.Base";
    static constexpr char CHANGES_KEY[] = "PE.EnvironmentAspect.Changes";

private:
    // One possible choice in the Environment aspect.
    struct BaseEnvironment {
        Utils::Environment unmodifiedBaseEnvironment() const;

        std::function<Utils::Environment()> getter;
        QString displayName;
    };

    Utils::EnvironmentItems m_userChanges;
    QList<EnvironmentModifier> m_modifiers;
    QList<BaseEnvironment> m_baseEnvironments;
    int m_base = -1;
    bool m_isLocal = false;
    bool m_allowPrintOnRun = true;
    bool m_printOnRun = false;
};

} // namespace ProjectExplorer
