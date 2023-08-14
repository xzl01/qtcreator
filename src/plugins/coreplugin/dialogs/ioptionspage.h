// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <coreplugin/core_global.h>

#include <utils/aspects.h>
#include <utils/icon.h>
#include <utils/id.h>

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <functional>

namespace Layouting { class LayoutItem; };

namespace Utils { class AspectContainer; };

namespace Core {

class CORE_EXPORT IOptionsPageWidget : public QWidget
{
    Q_OBJECT

public:
    void setOnApply(const std::function<void ()> &func) { m_onApply = func; }
    void setOnFinish(const std::function<void ()> &func) { m_onFinish = func; }

protected:
    friend class IOptionsPage;
    virtual void apply() { if (m_onApply) m_onApply(); }
    virtual void finish() { if (m_onFinish) m_onFinish(); }

private:
    std::function<void()> m_onApply;
    std::function<void()> m_onFinish;
};

class CORE_EXPORT IOptionsPage
{
    Q_DISABLE_COPY_MOVE(IOptionsPage)

public:
    explicit IOptionsPage(bool registerGlobally = true);
    virtual ~IOptionsPage();

    static const QList<IOptionsPage *> allOptionsPages();

    Utils::Id id() const { return m_id; }
    QString displayName() const { return m_displayName; }
    Utils::Id category() const { return m_category; }
    QString displayCategory() const { return m_displayCategory; }
    QIcon categoryIcon() const;

    using WidgetCreator = std::function<IOptionsPageWidget *()>;
    void setWidgetCreator(const WidgetCreator &widgetCreator);

    virtual QWidget *widget();
    virtual void apply();
    virtual void finish();

    virtual bool matches(const QRegularExpression &regexp) const;

protected:
    virtual QStringList keywords() const;

    void setId(Utils::Id id) { m_id = id; }
    void setDisplayName(const QString &displayName) { m_displayName = displayName; }
    void setCategory(Utils::Id category) { m_category = category; }
    void setDisplayCategory(const QString &displayCategory) { m_displayCategory = displayCategory; }
    void setCategoryIcon(const Utils::Icon &categoryIcon) { m_categoryIcon = categoryIcon; }
    void setCategoryIconPath(const Utils::FilePath &categoryIconPath);
    void setSettings(Utils::AspectContainer *settings);
    void setLayouter(const std::function<Layouting::LayoutItem()> &layouter);

    // Used in FontSettingsPage. FIXME?
    QPointer<QWidget> m_widget; // Used in conjunction with m_widgetCreator

private:
    Utils::Id m_id;
    Utils::Id m_category;
    QString m_displayName;
    QString m_displayCategory;
    Utils::Icon m_categoryIcon;
    WidgetCreator m_widgetCreator;

    mutable bool m_keywordsInitialized = false;
    mutable QStringList m_keywords;

    Utils::AspectContainer *m_settings = nullptr;
};

/*
    Alternative way for providing option pages instead of adding IOptionsPage
    objects into the plugin manager pool. Should only be used if creation of the
    actual option pages is not possible or too expensive at Qt Creator startup.
    (Like the designer integration, which needs to initialize designer plugins
    before the options pages get available.)
*/

class CORE_EXPORT IOptionsPageProvider
{
    Q_DISABLE_COPY_MOVE(IOptionsPageProvider);

public:
    IOptionsPageProvider();
    virtual ~IOptionsPageProvider();

    static const QList<IOptionsPageProvider *> allOptionsPagesProviders();

    Utils::Id category() const { return m_category; }
    QString displayCategory() const { return m_displayCategory; }
    QIcon categoryIcon() const;

    virtual QList<IOptionsPage *> pages() const = 0;
    virtual bool matches(const QRegularExpression &regexp) const = 0;

protected:
    void setCategory(Utils::Id category) { m_category = category; }
    void setDisplayCategory(const QString &displayCategory) { m_displayCategory = displayCategory; }
    void setCategoryIcon(const Utils::Icon &categoryIcon) { m_categoryIcon = categoryIcon; }

    Utils::Id m_category;
    QString m_displayCategory;
    Utils::Icon m_categoryIcon;
};

class CORE_EXPORT PagedSettings : public Utils::AspectContainer, public IOptionsPage
{
public:
    PagedSettings();

    using AspectContainer::readSettings; // FIXME: Remove.
    void readSettings(); // Intentionally hides AspectContainer::readSettings()
};

} // namespace Core
