// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abstractview.h"

#include <QPointer>
#include <QQuickImageProvider>

namespace QmlDesigner {
namespace Internal {

class StatesEditorView;

class StatesEditorImageProvider : public QQuickImageProvider
{
public:
    StatesEditorImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setNodeInstanceView(const NodeInstanceView *nodeInstanceView);

private:
    QPointer<const NodeInstanceView> m_nodeInstanceView;
};

} // namespace Internal
} // namespace QmlDesigner
