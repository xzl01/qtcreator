// Copyright (C) 2016 Lorenz Haas
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

// Tested with version 3.3, 3.4 and 3.4.1

#include "clangformat.h"

#include "clangformatconstants.h"

#include "../beautifierconstants.h"
#include "../beautifierplugin.h"
#include "../beautifiertr.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <texteditor/formattexteditor.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <utils/algorithm.h>
#include <utils/fileutils.h>

#include <QAction>
#include <QMenu>
#include <QTextBlock>
#include <QTextCodec>

using namespace TextEditor;

namespace Beautifier::Internal {

ClangFormat::ClangFormat()
{
    Core::ActionContainer *menu = Core::ActionManager::createMenu("ClangFormat.Menu");
    menu->menu()->setTitle(Tr::tr("&ClangFormat"));

    m_formatFile = new QAction(BeautifierPlugin::msgFormatCurrentFile(), this);
    Core::Command *cmd
            = Core::ActionManager::registerAction(m_formatFile, "ClangFormat.FormatFile");
    menu->addAction(cmd);
    connect(m_formatFile, &QAction::triggered, this, &ClangFormat::formatFile);

    m_formatLines = new QAction(BeautifierPlugin::msgFormatLines(), this);
    cmd = Core::ActionManager::registerAction(m_formatLines, "ClangFormat.FormatLines");
    menu->addAction(cmd);
    connect(m_formatLines, &QAction::triggered, this, &ClangFormat::formatLines);

    m_formatRange = new QAction(BeautifierPlugin::msgFormatAtCursor(), this);
    cmd = Core::ActionManager::registerAction(m_formatRange, "ClangFormat.FormatAtCursor");
    menu->addAction(cmd);
    connect(m_formatRange, &QAction::triggered, this, &ClangFormat::formatAtCursor);

    m_disableFormattingSelectedText
        = new QAction(BeautifierPlugin::msgDisableFormattingSelectedText(), this);
    cmd = Core::ActionManager::registerAction(
        m_disableFormattingSelectedText, "ClangFormat.DisableFormattingSelectedText");
    menu->addAction(cmd);
    connect(m_disableFormattingSelectedText, &QAction::triggered,
            this, &ClangFormat::disableFormattingSelectedText);

    Core::ActionManager::actionContainer(Constants::MENU_ID)->addMenu(menu);

    connect(&m_settings.supportedMimeTypes, &Utils::BaseAspect::changed,
            this, [this] { updateActions(Core::EditorManager::currentEditor()); });
}

QString ClangFormat::id() const
{
    return QLatin1String(Constants::CLANGFORMAT_DISPLAY_NAME);
}

void ClangFormat::updateActions(Core::IEditor *editor)
{
    const bool enabled = editor && m_settings.isApplicable(editor->document());
    m_formatFile->setEnabled(enabled);
    m_formatRange->setEnabled(enabled);
}

void ClangFormat::formatFile()
{
    formatCurrentFile(command());
}

void ClangFormat::formatAtPosition(const int pos, const int length)
{
    const TextEditorWidget *widget = TextEditorWidget::currentTextEditorWidget();
    if (!widget)
        return;

    const QTextCodec *codec = widget->textDocument()->codec();
    if (!codec) {
        formatCurrentFile(command(pos, length));
        return;
    }

    const QString &text = widget->textAt(0, pos + length);
    const QStringView buffer(text);
    const int encodedOffset = codec->fromUnicode(buffer.left(pos)).size();
    const int encodedLength = codec->fromUnicode(buffer.mid(pos, length)).size();
    formatCurrentFile(command(encodedOffset, encodedLength));
}

void ClangFormat::formatAtCursor()
{
    const TextEditorWidget *widget = TextEditorWidget::currentTextEditorWidget();
    if (!widget)
        return;

    const QTextCursor tc = widget->textCursor();

    if (tc.hasSelection()) {
        const int selectionStart = tc.selectionStart();
        formatAtPosition(selectionStart, tc.selectionEnd() - selectionStart);
    } else {
        // Pretend that the current line was selected.
        // Note that clang-format will extend the range to the next bigger
        // syntactic construct if needed.
        const QTextBlock block = tc.block();
        formatAtPosition(block.position(), block.length());
    }
}

void ClangFormat::formatLines()
{
    const TextEditorWidget *widget = TextEditorWidget::currentTextEditorWidget();
    if (!widget)
        return;

    const QTextCursor tc = widget->textCursor();
    // Current line by default
    int lineStart = tc.blockNumber() + 1;
    int lineEnd = lineStart;

    // Note that clang-format will extend the range to the next bigger
    // syntactic construct if needed.
    if (tc.hasSelection()) {
        const QTextBlock start = tc.document()->findBlock(tc.selectionStart());
        const QTextBlock end = tc.document()->findBlock(tc.selectionEnd());
        lineStart = start.blockNumber() + 1;
        lineEnd = end.blockNumber() + 1;
    }

    auto cmd = command();
    cmd.addOption(QString("-lines=%1:%2").arg(QString::number(lineStart)).arg(QString::number(lineEnd)));
    formatCurrentFile(cmd);
}

void ClangFormat::disableFormattingSelectedText()
{
    TextEditorWidget *widget = TextEditorWidget::currentTextEditorWidget();
    if (!widget)
        return;

    const QTextCursor tc = widget->textCursor();
    if (!tc.hasSelection())
        return;

    // Insert start marker
    const QTextBlock selectionStartBlock = tc.document()->findBlock(tc.selectionStart());
    QTextCursor insertCursor(tc.document());
    insertCursor.beginEditBlock();
    insertCursor.setPosition(selectionStartBlock.position());
    insertCursor.insertText("// clang-format off\n");
    const int positionToRestore = tc.position();

    // Insert end marker
    QTextBlock selectionEndBlock = tc.document()->findBlock(tc.selectionEnd());
    insertCursor.setPosition(selectionEndBlock.position() + selectionEndBlock.length() - 1);
    insertCursor.insertText("\n// clang-format on");
    insertCursor.endEditBlock();

    // Reset the cursor position in order to clear the selection.
    QTextCursor restoreCursor(tc.document());
    restoreCursor.setPosition(positionToRestore);
    widget->setTextCursor(restoreCursor);

    // The indentation of these markers might be undesired, so reformat.
    // This is not optimal because two undo steps will be needed to remove the markers.
    const int reformatTextLength = insertCursor.position() - selectionStartBlock.position();
    formatAtPosition(selectionStartBlock.position(), reformatTextLength);
}

Command ClangFormat::command() const
{
    Command command;
    command.setExecutable(m_settings.command());
    command.setProcessing(Command::PipeProcessing);

    if (m_settings.usePredefinedStyle()) {
        const QString predefinedStyle = m_settings.predefinedStyle.stringValue();
        command.addOption("-style=" + predefinedStyle);
        if (predefinedStyle == "File") {
            const QString fallbackStyle = m_settings.fallbackStyle.stringValue();
            if (fallbackStyle != "Default")
                command.addOption("-fallback-style=" + fallbackStyle);
        }

        command.addOption("-assume-filename=%file");
    } else {
        command.addOption("-style=file");
        const QString path =
                QFileInfo(m_settings.styleFileName(m_settings.customStyle())).absolutePath();
        command.addOption("-assume-filename=" + path + QDir::separator() + "%filename");
    }

    return command;
}

bool ClangFormat::isApplicable(const Core::IDocument *document) const
{
    return m_settings.isApplicable(document);
}

Command ClangFormat::command(int offset, int length) const
{
    Command c = command();
    c.addOption("-offset=" + QString::number(offset));
    c.addOption("-length=" + QString::number(length));
    return c;
}

} // Beautifier::Internal
