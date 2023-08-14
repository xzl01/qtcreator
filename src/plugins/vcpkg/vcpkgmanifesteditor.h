// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor.h>

namespace Vcpkg::Internal {

class VcpkgManifestEditorFactory : public TextEditor::TextEditorFactory
{
public:
    VcpkgManifestEditorFactory();
};

} // namespace Vcpkg::Internal
