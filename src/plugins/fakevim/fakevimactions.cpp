// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fakevimactions.h"
#include "fakevimhandler.h"
#include "fakevimtr.h"

// Please do not add any direct dependencies to other Qt Creator code  here.
// Instead emit signals and let the FakeVimPlugin channel the information to
// Qt Creator. The idea is to keep this file here in a "clean" state that
// allows easy reuse with any QTextEdit or QPlainTextEdit derived class.

#ifndef FAKEVIM_STANDALONE
#include <texteditor/icodestylepreferences.h>
#include <texteditor/tabsettings.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/typingsettings.h>
#endif

#include <utils/hostosinfo.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>

#include <QDebug>

using namespace Utils;

namespace FakeVim::Internal {

#ifdef FAKEVIM_STANDALONE
FvBaseAspect::FvBaseAspect()
{
}

void FvBaseAspect::setValue(const QVariant &value)
{
    m_value = value;
}

QVariant FvBaseAspect::value() const
{
    return m_value;
}

void FvBaseAspect::setDefaultValue(const QVariant &value)
{
    m_defaultValue = value;
    m_value = value;
}

QVariant FvBaseAspect::defaultValue() const
{
    return m_defaultValue;
}

void FvBaseAspect::setSettingsKey(const QString &group, const QString &key)
{
    m_settingsGroup = group;
    m_settingsKey = key;
}

QString FvBaseAspect::settingsKey() const
{
    return m_settingsKey;
}

// unused but kept for compile
void setAutoApply(bool ) {}
#endif


static FakeVimSettings *s_settings;

FakeVimSettings &settings()
{
    return *s_settings;
}

FakeVimSettings::FakeVimSettings()
{
    s_settings = this;

#ifndef FAKEVIM_STANDALONE
    const char SETTINGS_CATEGORY[]              = "D.FakeVim";
    const char SETTINGS_ID[]                    = "A.FakeVim.General";

    setId(SETTINGS_ID);
    setDisplayName(Tr::tr("General"));
    setCategory(SETTINGS_CATEGORY);
    setDisplayCategory(Tr::tr("FakeVim"));
    setCategoryIconPath(":/fakevim/images/settingscategory_fakevim.png");

    setup(&useFakeVim,     false, "UseFakeVim",     {},    Tr::tr("Use FakeVim"));
#endif

    // Specific FakeVim settings
    setup(&readVimRc,      false, "ReadVimRc",      {},    Tr::tr("Read .vimrc from location:"));
    setup(&vimRcPath,      QString(), "VimRcPath",  {},    {}); // Tr::tr("Path to .vimrc")
    setup(&showMarks,      false, "ShowMarks",      "sm",  Tr::tr("Show position of text marks"));
    setup(&passControlKey, false, "PassControlKey", "pck", Tr::tr("Pass control keys"));
    setup(&passKeys,       true,  "PassKeys",       "pk",  Tr::tr("Pass keys in insert mode"));

    // Emulated Vsetting
    setup(&startOfLine,    true,  "StartOfLine",    "sol", Tr::tr("Start of line"));
    setup(&tabStop,        8,     "TabStop",        "ts",  Tr::tr("Tabulator size:"));
    setup(&smartTab,       false, "SmartTab",       "sta", Tr::tr("Smart tabulators"));
    setup(&hlSearch,       true,  "HlSearch",       "hls", Tr::tr("Highlight search results"));
    setup(&shiftWidth,     8,     "ShiftWidth",     "sw",  Tr::tr("Shift width:"));
    setup(&expandTab,      false, "ExpandTab",      "et",  Tr::tr("Expand tabulators"));
    setup(&autoIndent,     false, "AutoIndent",     "ai",  Tr::tr("Automatic indentation"));
    setup(&smartIndent,    false, "SmartIndent",    "si",  Tr::tr("Smart indentation"));
    setup(&incSearch,      true,  "IncSearch",      "is",  Tr::tr("Incremental search"));
    setup(&useCoreSearch,  false, "UseCoreSearch",  "ucs", Tr::tr("Use search dialog"));
    setup(&smartCase,      false, "SmartCase",      "scs", Tr::tr("Use smartcase"));
    setup(&ignoreCase,     false, "IgnoreCase",     "ic",  Tr::tr("Use ignorecase"));
    setup(&wrapScan,       true,  "WrapScan",       "ws",  Tr::tr("Use wrapscan"));
    setup(&tildeOp,        false, "TildeOp",        "top", Tr::tr("Use tildeop"));
    setup(&showCmd,        true,  "ShowCmd",        "sc",  Tr::tr("Show partial command"));
    setup(&relativeNumber, false, "RelativeNumber", "rnu", Tr::tr("Show line numbers relative to cursor"));
    setup(&blinkingCursor, false, "BlinkingCursor", "bc",  Tr::tr("Blinking cursor"));
    setup(&scrollOff,      0,     "ScrollOff",      "so",  Tr::tr("Scroll offset:"));
    setup(&backspace,      "indent,eol,start",
                                  "Backspace",      "bs",  Tr::tr("Backspace:"));
    setup(&isKeyword,      "@,48-57,_,192-255,a-z,A-Z",
                                  "IsKeyword",      "isk", Tr::tr("Keyword characters:"));
    setup(&clipboard,      {},    "Clipboard",      "cb",  Tr::tr(""));
    setup(&formatOptions,  {},    "formatoptions",  "fo",  Tr::tr(""));

    // Emulated plugins
    setup(&emulateVimCommentary, false, "commentary", {}, "vim-commentary");
    setup(&emulateReplaceWithRegister, false, "ReplaceWithRegister", {}, "ReplaceWithRegister");
    setup(&emulateExchange, false, "exchange", {}, "vim-exchange");
    setup(&emulateArgTextObj, false, "argtextobj", {}, "argtextobj.vim");
    setup(&emulateSurround, false, "surround", {}, "vim-surround");

    // Some polish
    useFakeVim.setDisplayName(Tr::tr("Use Vim-style Editing"));

    relativeNumber.setToolTip(Tr::tr("Displays line numbers relative to the line containing "
        "text cursor."));

    passControlKey.setToolTip(Tr::tr("Does not interpret key sequences like Ctrl-S in FakeVim "
        "but handles them as regular shortcuts. This gives easier access to core functionality "
        "at the price of losing some features of FakeVim."));

    passKeys.setToolTip(Tr::tr("Does not interpret some key presses in insert mode so that "
        "code can be properly completed and expanded."));

    tabStop.setToolTip(Tr::tr("Vim tabstop option."));

#ifndef FAKEVIM_STANDALONE
    backspace.setDisplayStyle(FvStringAspect::LineEditDisplay);
    isKeyword.setDisplayStyle(FvStringAspect::LineEditDisplay);

    const QString vimrcDefault = QLatin1String(HostOsInfo::isAnyUnixHost()
                ? "$HOME/.vimrc" : "%USERPROFILE%\\_vimrc");
    vimRcPath.setExpectedKind(PathChooser::File);
    vimRcPath.setToolTip(Tr::tr("Keep empty to use the default path, i.e. "
               "%USERPROFILE%\\_vimrc on Windows, ~/.vimrc otherwise."));
    vimRcPath.setPlaceHolderText(Tr::tr("Default: %1").arg(vimrcDefault));
    vimRcPath.setDisplayStyle(FvStringAspect::PathChooserDisplay);

    setLayouter([this] {
        using namespace Layouting;
        using namespace TextEditor;

        Row bools {
            Column {
                autoIndent,
                smartIndent,
                expandTab,
                smartTab,
                hlSearch,
                showCmd,
                startOfLine,
                passKeys,
                blinkingCursor
            },
            Column {
                incSearch,
                useCoreSearch,
                ignoreCase,
                smartCase,
                wrapScan,
                showMarks,
                passControlKey,
                relativeNumber,
                tildeOp
            }
        };

        Row ints { shiftWidth, tabStop, scrollOff, st };

        vimRcPath.setEnabler(&readVimRc);

        Column strings {
            backspace,
            isKeyword,
            Row {readVimRc, vimRcPath}
        };

        return Column {
            useFakeVim,

            Group {
                title(Tr::tr("Vim Behavior")),
                Column {
                    bools,
                    ints,
                    strings
                }
            },

            Group {
                title(Tr::tr("Plugin Emulation")),
                Column {
                    emulateVimCommentary,
                    emulateReplaceWithRegister,
                    emulateArgTextObj,
                    emulateExchange,
                    emulateSurround
                }
            },

            Row {
                PushButton {
                    text(Tr::tr("Copy Text Editor Settings")),
                    onClicked([this] {
                        TabSettings ts = TextEditorSettings::codeStyle()->tabSettings();
                        TypingSettings tps = TextEditorSettings::typingSettings();
                        expandTab.setValue(ts.m_tabPolicy != TabSettings::TabsOnlyTabPolicy);
                        tabStop.setValue(ts.m_tabSize);
                        shiftWidth.setValue(ts.m_indentSize);
                        smartTab.setValue(tps.m_smartBackspaceBehavior
                                          == TypingSettings::BackspaceFollowsPreviousIndents);
                        autoIndent.setValue(true);
                        smartIndent.setValue(tps.m_autoIndent);
                        incSearch.setValue(true);
                    }),
                },
                PushButton {
                    text(Tr::tr("Set Qt Style")),
                    onClicked([this] {
                        expandTab.setVolatileValue(true);
                        tabStop.setVolatileValue(4);
                        shiftWidth.setVolatileValue(4);
                        smartTab.setVolatileValue(true);
                        autoIndent.setVolatileValue(true);
                        smartIndent.setVolatileValue(true);
                        incSearch.setVolatileValue(true);
                        backspace.setVolatileValue(QString("indent,eol,start"));
                        passKeys.setVolatileValue(true);
                    }),
                },
                PushButton {
                    text(Tr::tr("Set Plain Style")),
                    onClicked([this] {
                        expandTab.setVolatileValue(false);
                        tabStop.setVolatileValue(8);
                        shiftWidth.setVolatileValue(8);
                        smartTab.setVolatileValue(false);
                        autoIndent.setVolatileValue(false);
                        smartIndent.setVolatileValue(false);
                        incSearch.setVolatileValue(false);
                        backspace.setVolatileValue(QString());
                        passKeys.setVolatileValue(false);
                    }),
                 },
                 st
            },
            st
        };
    });

    readSettings();

#endif
}

FakeVimSettings::~FakeVimSettings() = default;

FvBaseAspect *FakeVimSettings::item(const QString &name)
{
    return m_nameToAspect.value(name, nullptr);
}

QString FakeVimSettings::trySetValue(const QString &name, const QString &value)
{
    FvBaseAspect *aspect = m_nameToAspect.value(name, nullptr);
    if (!aspect)
        return Tr::tr("Unknown option: %1").arg(name);
    if (aspect == &tabStop || aspect == &shiftWidth) {
        if (value.toInt() <= 0)
            return Tr::tr("Argument must be positive: %1=%2")
                    .arg(name).arg(value);
    }
    aspect->setValue(value);
    return QString();
}

void FakeVimSettings::setup(FvBaseAspect *aspect,
                            const QVariant &value,
                            const QString &settingsKey,
                            const QString &shortName,
                            const QString &labelText)
{
    aspect->setSettingsKey("FakeVim", settingsKey);
    aspect->setDefaultValue(value);
#ifndef FAKEVIM_STANDALONE
    aspect->setLabelText(labelText);
    aspect->setAutoApply(false);
    registerAspect(aspect);

    if (auto boolAspect = dynamic_cast<FvBoolAspect *>(aspect))
        boolAspect->setLabelPlacement(FvBoolAspect::LabelPlacement::AtCheckBox);
#else
    Q_UNUSED(labelText)
#endif

    const QString longName = settingsKey.toLower();
    if (!longName.isEmpty()) {
        m_nameToAspect[longName] = aspect;
        m_aspectToName[aspect] = longName;
    }
    if (!shortName.isEmpty())
        m_nameToAspect[shortName] = aspect;
}

} // FakeVim::Internal
