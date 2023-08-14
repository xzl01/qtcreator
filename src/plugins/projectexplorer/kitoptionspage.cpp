// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitoptionspage.h"

#include "filterkitaspectsdialog.h"
#include "kitmodel.h"
#include "kit.h"
#include "projectexplorerconstants.h"
#include "projectexplorertr.h"
#include "kitmanagerconfigwidget.h"
#include "kitmanager.h"

#include <utils/qtcassert.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace ProjectExplorer {
namespace Internal {

// KitOptionsPageWidget

class KitOptionsPageWidget;

static QPointer<KitOptionsPageWidget> kitOptionsPageWidget;

class KitOptionsPageWidget : public Core::IOptionsPageWidget
{
public:
    KitOptionsPageWidget();

    QModelIndex currentIndex() const;
    Kit *currentKit() const;

    void kitSelectionChanged();
    void addNewKit();
    void cloneKit();
    void removeKit();
    void makeDefaultKit();
    void updateState();

    void apply() final { m_model->apply(); }

public:
    QTreeView *m_kitsView = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_cloneButton = nullptr;
    QPushButton *m_delButton = nullptr;
    QPushButton *m_makeDefaultButton = nullptr;
    QPushButton *m_filterButton = nullptr;
    QPushButton *m_defaultFilterButton = nullptr;

    KitModel *m_model = nullptr;
    QItemSelectionModel *m_selectionModel = nullptr;
    KitManagerConfigWidget *m_currentWidget = nullptr;
};

KitOptionsPageWidget::KitOptionsPageWidget()
{
    kitOptionsPageWidget = this;
    m_kitsView = new QTreeView(this);
    m_kitsView->setUniformRowHeights(true);
    m_kitsView->header()->setStretchLastSection(true);
    m_kitsView->setSizePolicy(m_kitsView->sizePolicy().horizontalPolicy(),
                              QSizePolicy::Ignored);

    m_addButton = new QPushButton(Tr::tr("Add"), this);
    m_cloneButton = new QPushButton(Tr::tr("Clone"), this);
    m_delButton = new QPushButton(Tr::tr("Remove"), this);
    m_makeDefaultButton = new QPushButton(Tr::tr("Make Default"), this);
    m_filterButton = new QPushButton(Tr::tr("Settings Filter..."), this);
    m_filterButton->setToolTip(Tr::tr("Choose which settings to display for this kit."));
    m_defaultFilterButton = new QPushButton(Tr::tr("Default Settings Filter..."), this);
    m_defaultFilterButton->setToolTip(Tr::tr("Choose which kit settings to display by default."));

    auto buttonLayout = new QVBoxLayout;
    buttonLayout->setSpacing(6);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_cloneButton);
    buttonLayout->addWidget(m_delButton);
    buttonLayout->addWidget(m_makeDefaultButton);
    buttonLayout->addWidget(m_filterButton);
    buttonLayout->addWidget(m_defaultFilterButton);
    buttonLayout->addStretch();

    auto horizontalLayout = new QHBoxLayout;
    horizontalLayout->addWidget(m_kitsView);
    horizontalLayout->addLayout(buttonLayout);

    auto verticalLayout = new QVBoxLayout(this);
    verticalLayout->addLayout(horizontalLayout);

    m_model = new Internal::KitModel(verticalLayout, this);
    connect(m_model, &Internal::KitModel::kitStateChanged,
            this, &KitOptionsPageWidget::updateState);
    verticalLayout->setStretch(0, 1);
    verticalLayout->setStretch(1, 0);

    m_kitsView->setModel(m_model);
    m_kitsView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_kitsView->expandAll();

    m_selectionModel = m_kitsView->selectionModel();
    connect(m_selectionModel, &QItemSelectionModel::selectionChanged,
            this, &KitOptionsPageWidget::kitSelectionChanged);
    connect(KitManager::instance(), &KitManager::kitAdded,
            this, &KitOptionsPageWidget::kitSelectionChanged);
    connect(KitManager::instance(), &KitManager::kitRemoved,
            this, &KitOptionsPageWidget::kitSelectionChanged);
    connect(KitManager::instance(), &KitManager::kitUpdated,
            this, &KitOptionsPageWidget::kitSelectionChanged);

    // Set up add menu:
    connect(m_addButton, &QAbstractButton::clicked,
            this, &KitOptionsPageWidget::addNewKit);
    connect(m_cloneButton, &QAbstractButton::clicked,
            this, &KitOptionsPageWidget::cloneKit);
    connect(m_delButton, &QAbstractButton::clicked,
            this, &KitOptionsPageWidget::removeKit);
    connect(m_makeDefaultButton, &QAbstractButton::clicked,
            this, &KitOptionsPageWidget::makeDefaultKit);
    connect(m_filterButton, &QAbstractButton::clicked, this, [this] {
        QTC_ASSERT(m_currentWidget, return);
        FilterKitAspectsDialog dlg(m_currentWidget->workingCopy(), this);
        if (dlg.exec() == QDialog::Accepted) {
            m_currentWidget->workingCopy()->setIrrelevantAspects(dlg.irrelevantAspects());
            m_currentWidget->updateVisibility();
        }
    });
    connect(m_defaultFilterButton, &QAbstractButton::clicked, this, [this] {
        FilterKitAspectsDialog dlg(nullptr, this);
        if (dlg.exec() == QDialog::Accepted) {
            KitManager::setIrrelevantAspects(dlg.irrelevantAspects());
            m_model->updateVisibility();
        }
    });
    updateState();
}

void KitOptionsPageWidget::kitSelectionChanged()
{
    QModelIndex current = currentIndex();
    KitManagerConfigWidget * const newWidget = m_model->widget(current);
    if (newWidget == m_currentWidget)
        return;

    if (m_currentWidget)
        m_currentWidget->setVisible(false);

    m_currentWidget = newWidget;

    if (m_currentWidget) {
        m_currentWidget->setVisible(true);
        m_kitsView->scrollTo(current);
    }

    updateState();
}

void KitOptionsPageWidget::addNewKit()
{
    Kit *k = m_model->markForAddition(nullptr);

    QModelIndex newIdx = m_model->indexOf(k);
    m_selectionModel->select(newIdx,
                             QItemSelectionModel::Clear
                             | QItemSelectionModel::SelectCurrent
                             | QItemSelectionModel::Rows);
}

Kit *KitOptionsPageWidget::currentKit() const
{
    return m_model->kit(currentIndex());
}

void KitOptionsPageWidget::cloneKit()
{
    Kit *current = currentKit();
    if (!current)
        return;

    Kit *k = m_model->markForAddition(current);
    QModelIndex newIdx = m_model->indexOf(k);
    m_kitsView->scrollTo(newIdx);
    m_selectionModel->select(newIdx,
                             QItemSelectionModel::Clear
                             | QItemSelectionModel::SelectCurrent
                             | QItemSelectionModel::Rows);
}

void KitOptionsPageWidget::removeKit()
{
    if (Kit *k = currentKit())
        m_model->markForRemoval(k);
}

void KitOptionsPageWidget::makeDefaultKit()
{
    m_model->setDefaultKit(currentIndex());
    updateState();
}

void KitOptionsPageWidget::updateState()
{
    if (!m_kitsView)
        return;

    bool canCopy = false;
    bool canDelete = false;
    bool canMakeDefault = false;

    if (Kit *k = currentKit()) {
        canCopy = true;
        canDelete = !k->isAutoDetected();
        canMakeDefault = !m_model->isDefaultKit(k);
    }

    m_cloneButton->setEnabled(canCopy);
    m_delButton->setEnabled(canDelete);
    m_makeDefaultButton->setEnabled(canMakeDefault);
    m_filterButton->setEnabled(canCopy);
}

QModelIndex KitOptionsPageWidget::currentIndex() const
{
    if (!m_selectionModel)
        return QModelIndex();

    QModelIndexList idxs = m_selectionModel->selectedRows();
    if (idxs.count() != 1)
        return QModelIndex();
    return idxs.at(0);
}

} // namespace Internal

// --------------------------------------------------------------------------
// KitOptionsPage:
// --------------------------------------------------------------------------

KitOptionsPage::KitOptionsPage()
{
    setId(Constants::KITS_SETTINGS_PAGE_ID);
    setDisplayName(Tr::tr("Kits"));
    setCategory(Constants::KITS_SETTINGS_CATEGORY);
    setDisplayCategory(Tr::tr("Kits"));
    setCategoryIconPath(":/projectexplorer/images/settingscategory_kits.png");
    setWidgetCreator([] { return new Internal::KitOptionsPageWidget; });
}

void KitOptionsPage::showKit(Kit *k)
{
    if (!k)
        return;

    Internal::KitOptionsPageWidget *widget = Internal::kitOptionsPageWidget;
    if (!widget)
        return;

    QModelIndex index = widget->m_model->indexOf(k);
    widget->m_selectionModel->select(index,
                                QItemSelectionModel::Clear
                                | QItemSelectionModel::SelectCurrent
                                | QItemSelectionModel::Rows);
    widget->m_kitsView->scrollTo(index);
}

} // namespace ProjectExplorer
