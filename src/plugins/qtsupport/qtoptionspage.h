// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <coreplugin/dialogs/ioptionspage.h>

namespace QtSupport::Internal {

class QtOptionsPage final : public Core::IOptionsPage
{
public:
    QtOptionsPage();

    QStringList keywords() const final;

    static bool canLinkWithQt();
    static bool isLinkedWithQt();
    static void linkWithQt();
};

} // QtSupport::Internal
