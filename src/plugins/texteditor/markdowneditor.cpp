// Copyright (C) 2023 Tasuku Suzuki
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "markdowneditor.h"

#include "textdocument.h"
#include "texteditor.h"
#include "texteditortr.h"

#include <aggregation/aggregate.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/coreplugintr.h>
#include <coreplugin/icore.h>
#include <coreplugin/minisplitter.h>
#include <utils/stringutils.h>

#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>

#include <optional>

namespace TextEditor::Internal {

const char MARKDOWNVIEWER_ID[] = "Editors.MarkdownViewer";
const char MARKDOWNVIEWER_TEXT_CONTEXT[] = "Editors.MarkdownViewer.Text";
const char MARKDOWNVIEWER_MIME_TYPE[] = "text/markdown";
const char MARKDOWNVIEWER_TEXTEDITOR_RIGHT[] = "Markdown.TextEditorRight";
const char MARKDOWNVIEWER_SHOW_EDITOR[] = "Markdown.ShowEditor";
const char MARKDOWNVIEWER_SHOW_PREVIEW[] = "Markdown.ShowPreview";
const bool kTextEditorRightDefault = false;
const bool kShowEditorDefault = true;
const bool kShowPreviewDefault = true;

class MarkdownEditor : public Core::IEditor
{
public:
    MarkdownEditor()
        : m_document(new TextDocument(MARKDOWNVIEWER_ID))
    {
        m_document->setMimeType(MARKDOWNVIEWER_MIME_TYPE);

        QSettings *s = Core::ICore::settings();
        const bool textEditorRight
            = s->value(MARKDOWNVIEWER_TEXTEDITOR_RIGHT, kTextEditorRightDefault).toBool();
        const bool showPreview = s->value(MARKDOWNVIEWER_SHOW_PREVIEW, kShowPreviewDefault).toBool();
        const bool showEditor = s->value(MARKDOWNVIEWER_SHOW_EDITOR, kShowEditorDefault).toBool()
                                || !showPreview; // ensure at least one is visible

        m_splitter = new Core::MiniSplitter;

        // preview
        m_previewWidget = new QTextBrowser();
        m_previewWidget->setOpenExternalLinks(true);
        m_previewWidget->setFrameShape(QFrame::NoFrame);
        new Utils::MarkdownHighlighter(m_previewWidget->document());

        // editor
        m_textEditorWidget = new TextEditorWidget;
        m_textEditorWidget->setTextDocument(m_document);
        m_textEditorWidget->setupGenericHighlighter();
        m_textEditorWidget->setMarksVisible(false);
        auto context = new Core::IContext(this);
        context->setWidget(m_textEditorWidget);
        context->setContext(Core::Context(MARKDOWNVIEWER_TEXT_CONTEXT));
        Core::ICore::addContextObject(context);

        m_splitter->addWidget(m_textEditorWidget); // sets splitter->focusWidget() on non-Windows
        m_splitter->addWidget(m_previewWidget);

        setContext(Core::Context(MARKDOWNVIEWER_ID));

        auto widget = new QWidget;
        auto layout = new QVBoxLayout;
        layout->setContentsMargins(0, 0, 0, 0);
        widget->setLayout(layout);
        layout->addWidget(m_splitter);
        setWidget(widget);
        m_widget->installEventFilter(this);
        using namespace Aggregation;
        Aggregate *agg = Aggregate::parentAggregate(m_textEditorWidget);
        if (!agg) {
            agg = new Aggregate;
            agg->add(m_textEditorWidget);
        }
        agg->add(m_widget.get());

        m_togglePreviewVisible = new QToolButton;
        m_togglePreviewVisible->setText(Tr::tr("Show Preview"));
        m_togglePreviewVisible->setCheckable(true);
        m_togglePreviewVisible->setChecked(showPreview);
        m_previewWidget->setVisible(showPreview);

        m_toggleEditorVisible = new QToolButton;
        m_toggleEditorVisible->setText(Tr::tr("Show Editor"));
        m_toggleEditorVisible->setCheckable(true);
        m_toggleEditorVisible->setChecked(showEditor);
        m_textEditorWidget->setVisible(showEditor);

        auto swapViews = new QToolButton;
        swapViews->setText(Tr::tr("Swap Views"));
        swapViews->setEnabled(showEditor && showPreview);

        m_toolbarLayout = new QHBoxLayout(&m_toolbar);
        m_toolbarLayout->setSpacing(0);
        m_toolbarLayout->setContentsMargins(0, 0, 0, 0);
        m_toolbarLayout->addStretch();
        m_toolbarLayout->addWidget(m_togglePreviewVisible);
        m_toolbarLayout->addWidget(m_toggleEditorVisible);
        m_toolbarLayout->addWidget(swapViews);

        setWidgetOrder(textEditorRight);

        connect(m_document.data(),
                &TextDocument::mimeTypeChanged,
                m_document.data(),
                &TextDocument::changed);

        const auto updatePreview = [this] {
            // save scroll positions
            const QPoint positions = m_previewRestoreScrollPosition
                                         ? *m_previewRestoreScrollPosition
                                         : QPoint(m_previewWidget->horizontalScrollBar()->value(),
                                                  m_previewWidget->verticalScrollBar()->value());
            m_previewRestoreScrollPosition.reset();

            m_previewWidget->setMarkdown(m_document->plainText());

            m_previewWidget->horizontalScrollBar()->setValue(positions.x());
            m_previewWidget->verticalScrollBar()->setValue(positions.y());
        };

        const auto viewToggled =
            [swapViews](QWidget *view, bool visible, QWidget *otherView, QToolButton *otherButton) {
                if (view->isVisible() == visible)
                    return;
                view->setVisible(visible);
                if (visible) {
                    view->setFocus();
                } else if (otherView->isVisible()) {
                    otherView->setFocus();
                } else {
                    // make sure at least one view is visible
                    otherButton->toggle();
                }
                swapViews->setEnabled(view->isVisible() && otherView->isVisible());
            };
        const auto saveViewSettings = [this] {
            Utils::QtcSettings *s = Core::ICore::settings();
            s->setValueWithDefault(MARKDOWNVIEWER_SHOW_PREVIEW,
                                   m_togglePreviewVisible->isChecked(),
                                   kShowPreviewDefault);
            s->setValueWithDefault(MARKDOWNVIEWER_SHOW_EDITOR,
                                   m_toggleEditorVisible->isChecked(),
                                   kShowEditorDefault);
        };

        connect(m_toggleEditorVisible,
                &QToolButton::toggled,
                this,
                [this, viewToggled, saveViewSettings](bool visible) {
                    viewToggled(m_textEditorWidget,
                                visible,
                                m_previewWidget,
                                m_togglePreviewVisible);
                    saveViewSettings();
                });
        connect(m_togglePreviewVisible,
                &QToolButton::toggled,
                this,
                [this, viewToggled, updatePreview, saveViewSettings](bool visible) {
                    viewToggled(m_previewWidget, visible, m_textEditorWidget, m_toggleEditorVisible);
                    if (visible && m_performDelayedUpdate) {
                        m_performDelayedUpdate = false;
                        updatePreview();
                    }
                    saveViewSettings();
                });

        connect(swapViews, &QToolButton::clicked, m_textEditorWidget, [this] {
            const bool textEditorRight = isTextEditorRight();
            setWidgetOrder(!textEditorRight);
            // save settings
            Utils::QtcSettings *s = Core::ICore::settings();
            s->setValueWithDefault(MARKDOWNVIEWER_TEXTEDITOR_RIGHT,
                                   !textEditorRight,
                                   kTextEditorRightDefault);
        });

        // TODO directly update when we build with Qt 6.5.2
        m_previewTimer.setInterval(500);
        m_previewTimer.setSingleShot(true);
        connect(&m_previewTimer, &QTimer::timeout, this, [this, updatePreview] {
            if (m_togglePreviewVisible->isChecked())
                updatePreview();
            else
                m_performDelayedUpdate = true;
        });

        connect(m_document->document(), &QTextDocument::contentsChanged, &m_previewTimer, [this] {
            m_previewTimer.start();
        });
    }

    bool isTextEditorRight() const { return m_splitter->widget(0) == m_previewWidget; }

    void setWidgetOrder(bool textEditorRight)
    {
        QTC_ASSERT(m_splitter->count() > 1, return);
        QWidget *left = textEditorRight ? static_cast<QWidget *>(m_previewWidget)
                                        : m_textEditorWidget;
        QWidget *right = textEditorRight ? static_cast<QWidget *>(m_textEditorWidget)
                                         : m_previewWidget;
        m_splitter->insertWidget(0, left);
        m_splitter->insertWidget(1, right);
        // buttons
        QWidget *leftButton = textEditorRight ? m_togglePreviewVisible : m_toggleEditorVisible;
        QWidget *rightButton = textEditorRight ? m_toggleEditorVisible : m_togglePreviewVisible;
        const int rightIndex = m_toolbarLayout->count() - 2;
        m_toolbarLayout->insertWidget(rightIndex, leftButton);
        m_toolbarLayout->insertWidget(rightIndex, rightButton);
    }

    QWidget *toolBar() override { return &m_toolbar; }

    Core::IDocument *document() const override { return m_document.data(); }
    TextEditorWidget *textEditorWidget() const { return m_textEditorWidget; }
    int currentLine() const override { return textEditorWidget()->textCursor().blockNumber() + 1; };
    int currentColumn() const override
    {
        QTextCursor cursor = textEditorWidget()->textCursor();
        return cursor.position() - cursor.block().position() + 1;
    }
    void gotoLine(int line, int column, bool centerLine) override
    {
        if (!m_toggleEditorVisible->isChecked())
            m_toggleEditorVisible->toggle();
        textEditorWidget()->gotoLine(line, column, centerLine);
    }

    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (obj == m_widget && ev->type() == QEvent::FocusIn) {
            if (m_splitter->focusWidget())
                m_splitter->focusWidget()->setFocus();
            else if (m_textEditorWidget->isVisible())
                m_textEditorWidget->setFocus();
            else
                m_splitter->widget(0)->setFocus();
            return true;
        }
        return Core::IEditor::eventFilter(obj, ev);
    }

    QByteArray saveState() const override
    {
        QByteArray state;
        QDataStream stream(&state, QIODevice::WriteOnly);
        stream << 1; // version number
        stream << m_textEditorWidget->saveState();
        stream << m_previewWidget->horizontalScrollBar()->value();
        stream << m_previewWidget->verticalScrollBar()->value();
        stream << isTextEditorRight();
        stream << m_togglePreviewVisible->isChecked();
        stream << m_toggleEditorVisible->isChecked();
        stream << m_splitter->saveState();
        return state;
    }

    void restoreState(const QByteArray &state) override
    {
        if (state.isEmpty())
            return;
        int version;
        QByteArray editorState;
        int previewHV;
        int previewVV;
        bool textEditorRight;
        bool previewShown;
        bool textEditorShown;
        QByteArray splitterState;
        QDataStream stream(state);
        stream >> version;
        stream >> editorState;
        stream >> previewHV;
        stream >> previewVV;
        stream >> textEditorRight;
        stream >> previewShown;
        stream >> textEditorShown;
        stream >> splitterState;
        m_textEditorWidget->restoreState(editorState);
        m_previewRestoreScrollPosition.emplace(previewHV, previewVV);
        setWidgetOrder(textEditorRight);
        m_splitter->restoreState(splitterState);
        m_togglePreviewVisible->setChecked(previewShown);
        // ensure at least one is shown
        m_toggleEditorVisible->setChecked(textEditorShown || !previewShown);
    }

private:
    QTimer m_previewTimer;
    bool m_performDelayedUpdate = false;
    Core::MiniSplitter *m_splitter;
    QTextBrowser *m_previewWidget;
    TextEditorWidget *m_textEditorWidget;
    TextDocumentPtr m_document;
    QWidget m_toolbar;
    QHBoxLayout *m_toolbarLayout;
    QToolButton *m_toggleEditorVisible;
    QToolButton *m_togglePreviewVisible;
    std::optional<QPoint> m_previewRestoreScrollPosition;
};

MarkdownEditorFactory::MarkdownEditorFactory()
    : m_actionHandler(MARKDOWNVIEWER_ID,
                      MARKDOWNVIEWER_TEXT_CONTEXT,
                      TextEditor::TextEditorActionHandler::None,
                      [](Core::IEditor *editor) {
                          return static_cast<MarkdownEditor *>(editor)->textEditorWidget();
                      })
{
    setId(MARKDOWNVIEWER_ID);
    setDisplayName(::Core::Tr::tr("Markdown Editor"));
    addMimeType(MARKDOWNVIEWER_MIME_TYPE);
    setEditorCreator([] { return new MarkdownEditor; });
}

} // namespace TextEditor::Internal
