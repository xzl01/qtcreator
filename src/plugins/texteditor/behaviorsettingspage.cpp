// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "behaviorsettingspage.h"

#include "behaviorsettings.h"
#include "behaviorsettingswidget.h"
#include "codestylepool.h"
#include "extraencodingsettings.h"
#include "simplecodestylepreferences.h"
#include "storagesettings.h"
#include "tabsettings.h"
#include "tabsettingswidget.h"
#include "texteditorconstants.h"
#include "texteditorsettings.h"
#include "texteditortr.h"
#include "typingsettings.h"

#include <coreplugin/icore.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>

#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

// for opening the respective coding style preferences
#include <cppeditor/cppeditorconstants.h>
#include <qmljseditor/qmljseditorconstants.h>
#include <qmljstools/qmljstoolsconstants.h>

#include <QCoreApplication>
#include <QGridLayout>
#include <QPointer>
#include <QSettings>
#include <QSpacerItem>

namespace TextEditor {

class BehaviorSettingsPagePrivate : public QObject
{
public:
    BehaviorSettingsPagePrivate();

    const QString m_settingsPrefix{"text"};
    TextEditor::BehaviorSettingsWidget *m_behaviorWidget = nullptr;

    CodeStylePool *m_defaultCodeStylePool = nullptr;
    SimpleCodeStylePreferences *m_codeStyle = nullptr;
    SimpleCodeStylePreferences *m_pageCodeStyle = nullptr;
    TypingSettings m_typingSettings;
    StorageSettings m_storageSettings;
    BehaviorSettings m_behaviorSettings;
    ExtraEncodingSettings m_extraEncodingSettings;
};

BehaviorSettingsPagePrivate::BehaviorSettingsPagePrivate()
{
    // global tab preferences for all other languages
    m_codeStyle = new SimpleCodeStylePreferences(this);
    m_codeStyle->setDisplayName(Tr::tr("Global", "Settings"));
    m_codeStyle->setId(Constants::GLOBAL_SETTINGS_ID);

    // default pool for all other languages
    m_defaultCodeStylePool = new CodeStylePool(nullptr, this); // Any language
    m_defaultCodeStylePool->addCodeStyle(m_codeStyle);

    QSettings * const s = Core::ICore::settings();
    m_codeStyle->fromSettings(m_settingsPrefix, s);
    m_typingSettings.fromSettings(m_settingsPrefix, s);
    m_storageSettings.fromSettings(m_settingsPrefix, s);
    m_behaviorSettings.fromSettings(m_settingsPrefix, s);
    m_extraEncodingSettings.fromSettings(m_settingsPrefix, s);
}

class BehaviorSettingsWidgetImpl : public Core::IOptionsPageWidget
{
public:
    BehaviorSettingsWidgetImpl(BehaviorSettingsPagePrivate *d) : d(d)
    {
        d->m_behaviorWidget = new BehaviorSettingsWidget(this);

        auto verticalSpacer = new QSpacerItem(20, 13, QSizePolicy::Minimum, QSizePolicy::Expanding);

        auto gridLayout = new QGridLayout(this);
        if (Utils::HostOsInfo::isMacHost())
            gridLayout->setContentsMargins(-1, 0, -1, 0); // don't ask.
        gridLayout->addWidget(d->m_behaviorWidget, 0, 0, 1, 1);
        gridLayout->addItem(verticalSpacer, 1, 0, 1, 1);

        d->m_pageCodeStyle = new SimpleCodeStylePreferences(this);
        d->m_pageCodeStyle->setDelegatingPool(d->m_codeStyle->delegatingPool());
        d->m_pageCodeStyle->setTabSettings(d->m_codeStyle->tabSettings());
        d->m_pageCodeStyle->setCurrentDelegate(d->m_codeStyle->currentDelegate());
        d->m_behaviorWidget->setCodeStyle(d->m_pageCodeStyle);

        TabSettingsWidget *tabSettingsWidget = d->m_behaviorWidget->tabSettingsWidget();
        tabSettingsWidget->setCodingStyleWarningVisible(true);
        connect(tabSettingsWidget, &TabSettingsWidget::codingStyleLinkClicked,
                this, [] (TabSettingsWidget::CodingStyleLink link) {
            switch (link) {
            case TabSettingsWidget::CppLink:
                Core::ICore::showOptionsDialog(CppEditor::Constants::CPP_CODE_STYLE_SETTINGS_ID);
                break;
            case TabSettingsWidget::QtQuickLink:
                Core::ICore::showOptionsDialog(QmlJSTools::Constants::QML_JS_CODE_STYLE_SETTINGS_ID);
                break;
            }
        });

        d->m_behaviorWidget->setAssignedTypingSettings(d->m_typingSettings);
        d->m_behaviorWidget->setAssignedStorageSettings(d->m_storageSettings);
        d->m_behaviorWidget->setAssignedBehaviorSettings(d->m_behaviorSettings);
        d->m_behaviorWidget->setAssignedExtraEncodingSettings(d->m_extraEncodingSettings);
        d->m_behaviorWidget->setAssignedCodec(Core::EditorManager::defaultTextCodec());
        d->m_behaviorWidget->setAssignedLineEnding(Core::EditorManager::defaultLineEnding());
    }

    void apply() final;

    BehaviorSettingsPagePrivate *d;
};

BehaviorSettingsPage::BehaviorSettingsPage()
  : d(new BehaviorSettingsPagePrivate)
{
    // Add the GUI used to configure the tab, storage and interaction settings
    setId(Constants::TEXT_EDITOR_BEHAVIOR_SETTINGS);
    setDisplayName(Tr::tr("Behavior"));

    setCategory(TextEditor::Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
    setDisplayCategory(Tr::tr("Text Editor"));
    setCategoryIconPath(TextEditor::Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
    setWidgetCreator([this] { return new BehaviorSettingsWidgetImpl(d); });
}

BehaviorSettingsPage::~BehaviorSettingsPage()
{
    delete d;
}

void BehaviorSettingsWidgetImpl::apply()
{
    if (!d->m_behaviorWidget) // page was never shown
        return;

    TypingSettings newTypingSettings;
    StorageSettings newStorageSettings;
    BehaviorSettings newBehaviorSettings;
    ExtraEncodingSettings newExtraEncodingSettings;

    d->m_behaviorWidget->assignedTypingSettings(&newTypingSettings);
    d->m_behaviorWidget->assignedStorageSettings(&newStorageSettings);
    d->m_behaviorWidget->assignedBehaviorSettings(&newBehaviorSettings);
    d->m_behaviorWidget->assignedExtraEncodingSettings(&newExtraEncodingSettings);

    QSettings *s = Core::ICore::settings();
    QTC_ASSERT(s, return);

    if (d->m_codeStyle->tabSettings() != d->m_pageCodeStyle->tabSettings()) {
        d->m_codeStyle->setTabSettings(d->m_pageCodeStyle->tabSettings());
        d->m_codeStyle->toSettings(d->m_settingsPrefix, s);
    }

    if (d->m_codeStyle->currentDelegate() != d->m_pageCodeStyle->currentDelegate()) {
        d->m_codeStyle->setCurrentDelegate(d->m_pageCodeStyle->currentDelegate());
        d->m_codeStyle->toSettings(d->m_settingsPrefix, s);
    }

    if (newTypingSettings != d->m_typingSettings) {
        d->m_typingSettings = newTypingSettings;
        d->m_typingSettings.toSettings(d->m_settingsPrefix, s);

        emit TextEditorSettings::instance()->typingSettingsChanged(newTypingSettings);
    }

    if (newStorageSettings != d->m_storageSettings) {
        d->m_storageSettings = newStorageSettings;
        d->m_storageSettings.toSettings(d->m_settingsPrefix, s);

        emit TextEditorSettings::instance()->storageSettingsChanged(newStorageSettings);
    }

    if (newBehaviorSettings != d->m_behaviorSettings) {
        d->m_behaviorSettings = newBehaviorSettings;
        d->m_behaviorSettings.toSettings(d->m_settingsPrefix, s);

        emit TextEditorSettings::instance()->behaviorSettingsChanged(newBehaviorSettings);
    }

    if (newExtraEncodingSettings != d->m_extraEncodingSettings) {
        d->m_extraEncodingSettings = newExtraEncodingSettings;
        d->m_extraEncodingSettings.toSettings(d->m_settingsPrefix, s);

        emit TextEditorSettings::instance()->extraEncodingSettingsChanged(newExtraEncodingSettings);
    }

    s->setValue(QLatin1String(Core::Constants::SETTINGS_DEFAULTTEXTENCODING),
                d->m_behaviorWidget->assignedCodecName());
    s->setValue(QLatin1String(Core::Constants::SETTINGS_DEFAULT_LINE_TERMINATOR),
                d->m_behaviorWidget->assignedLineEnding());
}

ICodeStylePreferences *BehaviorSettingsPage::codeStyle() const
{
    return d->m_codeStyle;
}

CodeStylePool *BehaviorSettingsPage::codeStylePool() const
{
    return d->m_defaultCodeStylePool;
}

const TypingSettings &BehaviorSettingsPage::typingSettings() const
{
    return d->m_typingSettings;
}

const StorageSettings &BehaviorSettingsPage::storageSettings() const
{
    return d->m_storageSettings;
}

const BehaviorSettings &BehaviorSettingsPage::behaviorSettings() const
{
    return d->m_behaviorSettings;
}

const ExtraEncodingSettings &BehaviorSettingsPage::extraEncodingSettings() const
{
    return d->m_extraEncodingSettings;
}


} // namespace TextEditor
