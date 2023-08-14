// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#ifndef FAKEVIM_STANDALONE
#   include <coreplugin/dialogs/ioptionspage.h>
#endif

#include <QCoreApplication>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace FakeVim::Internal {

#ifdef FAKEVIM_STANDALONE
class FvBaseAspect
{
public:
    FvBaseAspect();
    virtual ~FvBaseAspect() {}

    void setValue(const QVariant &value);
    QVariant value() const;
    void setDefaultValue(const QVariant &value);
    QVariant defaultValue() const;
    void setSettingsKey(const QString &group, const QString &key);
    QString settingsKey() const;
    void setCheckable(bool) {}
    void setDisplayName(const QString &) {}
    void setToolTip(const QString &) {}

private:
    QVariant m_value;
    QVariant m_defaultValue;
    QString m_settingsGroup;
    QString m_settingsKey;
};

class FvBoolAspect : public FvBaseAspect
{
public:
    bool value() const { return FvBaseAspect::value().toBool(); }
    bool operator()() const { return value(); }
};

class FvIntegerAspect : public FvBaseAspect
{
public:
    qint64 value() const { return FvBaseAspect::value().toLongLong(); }
    qint64 operator()() const { return value(); }
};

class FvStringAspect : public FvBaseAspect
{
public:
    QString value() const { return FvBaseAspect::value().toString(); }
    QString operator()() const { return value(); }
};

class FvAspectContainer : public FvBaseAspect
{
public:
};

#else

using FvAspectContainer = Core::PagedSettings;
using FvBaseAspect = Utils::BaseAspect;
using FvBoolAspect = Utils::BoolAspect;
using FvIntegerAspect = Utils::IntegerAspect;
using FvStringAspect = Utils::StringAspect;

#endif

class FakeVimSettings final : public FvAspectContainer
{
public:
    FakeVimSettings();
    ~FakeVimSettings();

    FvBaseAspect *item(const QString &name);
    QString trySetValue(const QString &name, const QString &value);

    FvBoolAspect useFakeVim;
    FvBoolAspect readVimRc;
    FvStringAspect vimRcPath;

    FvBoolAspect startOfLine;
    FvIntegerAspect tabStop;
    FvBoolAspect hlSearch;
    FvBoolAspect smartTab;
    FvIntegerAspect shiftWidth;
    FvBoolAspect expandTab;
    FvBoolAspect autoIndent;
    FvBoolAspect smartIndent;

    FvBoolAspect incSearch;
    FvBoolAspect useCoreSearch;
    FvBoolAspect smartCase;
    FvBoolAspect ignoreCase;
    FvBoolAspect wrapScan;

    // command ~ behaves as g~
    FvBoolAspect tildeOp;

    // indent  allow backspacing over autoindent
    // eol     allow backspacing over line breaks (join lines)
    // start   allow backspacing over the start of insert; CTRL-W and CTRL-U
    //         stop once at the start of insert.
    FvStringAspect backspace;

    // @,48-57,_,192-255
    FvStringAspect isKeyword;

    // other actions
    FvBoolAspect showMarks;
    FvBoolAspect passControlKey;
    FvBoolAspect passKeys;
    FvStringAspect clipboard;
    FvBoolAspect showCmd;
    FvIntegerAspect scrollOff;
    FvBoolAspect relativeNumber;
    FvStringAspect formatOptions;

    // Plugin emulation
    FvBoolAspect emulateVimCommentary;
    FvBoolAspect emulateReplaceWithRegister;
    FvBoolAspect emulateExchange;
    FvBoolAspect emulateArgTextObj;
    FvBoolAspect emulateSurround;

    FvBoolAspect blinkingCursor;

private:
    void setup(FvBaseAspect *aspect, const QVariant &value,
                      const QString &settingsKey,
                      const QString &shortName,
                      const QString &label);

    QHash<QString, FvBaseAspect *> m_nameToAspect;
    QHash<FvBaseAspect *, QString> m_aspectToName;
};

FakeVimSettings &settings();

} // FakeVim::Internal
