/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <tools/pimpl.h>
#include <tools/set.h>

#include <QtGlobal>

#include <functional>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs::Internal {
class Item;
class LoaderState;
class ProductContext;
class StoredModuleProviderInfo;
enum class Deferral;

// Collects the products' dependencies and builds the list of modules from them.
// Actual loading of module files is offloaded to ModuleLoader.
class DependenciesResolver
{
public:
    DependenciesResolver(LoaderState &loaderState);
    ~DependenciesResolver();

    // Returns false if the product has unhandled product dependencies and thus needs
    // to be deferred, true otherwise.
    bool resolveDependencies(ProductContext &product, Deferral deferral);

    void checkDependencyParameterDeclarations(const Item *productItem,
                                              const QString &productName) const;

    void setStoredModuleProviderInfo(const StoredModuleProviderInfo &moduleProviderInfo);
    StoredModuleProviderInfo storedModuleProviderInfo() const;
    const Set<QString> &tempQbsFiles() const;

    void printProfilingInfo(int indent);

    // Note: This function is never called for regular loading of the base module into a product,
    //       but only for the special cases of loading the dummy base module into a project
    //       and temporarily providing a base module for product multiplexing.
    Item *loadBaseModule(ProductContext &product, Item *item);

private:
    class Private;
    Pimpl<Private> d;
};

} // namespace qbs::Internal
