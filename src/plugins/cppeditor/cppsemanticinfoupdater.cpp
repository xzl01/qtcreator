// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppsemanticinfoupdater.h"

#include "cppmodelmanager.h"

#include <utils/async.h>
#include <utils/qtcassert.h>

#include <cplusplus/Control.h>
#include <cplusplus/CppDocument.h>
#include <cplusplus/TranslationUnit.h>

#include <QLoggingCategory>

enum { debug = 0 };

using namespace CPlusPlus;

static Q_LOGGING_CATEGORY(log, "qtc.cppeditor.semanticinfoupdater", QtWarningMsg)

namespace CppEditor {

class SemanticInfoUpdaterPrivate
{
public:
    class FuturizedTopLevelDeclarationProcessor: public TopLevelDeclarationProcessor
    {
    public:
        explicit FuturizedTopLevelDeclarationProcessor(QPromise<void> &promise): m_promise(promise) {}
        bool processDeclaration(DeclarationAST *) override { return !isCanceled(); }
        bool isCanceled() { return m_promise.isCanceled(); }
    private:
        QPromise<void> &m_promise;
    };

public:
    explicit SemanticInfoUpdaterPrivate(SemanticInfoUpdater *q);
    ~SemanticInfoUpdaterPrivate();

    SemanticInfo semanticInfo() const;
    void setSemanticInfo(const SemanticInfo &semanticInfo, bool emitSignal);

    SemanticInfo update(const SemanticInfo::Source &source,
                        bool emitSignalWhenFinished,
                        FuturizedTopLevelDeclarationProcessor *processor);

    bool reuseCurrentSemanticInfo(const SemanticInfo::Source &source, bool emitSignalWhenFinished);

    void update_helper(QPromise<void> &promise, const SemanticInfo::Source &source);

public:
    SemanticInfoUpdater *q;
    mutable QMutex m_lock;
    SemanticInfo m_semanticInfo;
    QFuture<void> m_future;
};

SemanticInfoUpdaterPrivate::SemanticInfoUpdaterPrivate(SemanticInfoUpdater *q)
    : q(q)
{
}

SemanticInfoUpdaterPrivate::~SemanticInfoUpdaterPrivate()
{
    m_future.cancel();
    m_future.waitForFinished();
}

SemanticInfo SemanticInfoUpdaterPrivate::semanticInfo() const
{
    QMutexLocker locker(&m_lock);
    return m_semanticInfo;
}

void SemanticInfoUpdaterPrivate::setSemanticInfo(const SemanticInfo &semanticInfo, bool emitSignal)
{
    {
        QMutexLocker locker(&m_lock);
        m_semanticInfo = semanticInfo;
    }
    if (emitSignal) {
        qCDebug(log) << "emiting new info";
        emit q->updated(semanticInfo);
    }
}

SemanticInfo SemanticInfoUpdaterPrivate::update(const SemanticInfo::Source &source,
                                                bool emitSignalWhenFinished,
                                                FuturizedTopLevelDeclarationProcessor *processor)
{
    SemanticInfo newSemanticInfo;
    newSemanticInfo.revision = source.revision;
    newSemanticInfo.snapshot = source.snapshot;

    Document::Ptr doc = newSemanticInfo.snapshot.preprocessedDocument(source.code,
                                          Utils::FilePath::fromString(source.fileName));
    if (processor)
        doc->control()->setTopLevelDeclarationProcessor(processor);
    doc->check();
    if (processor && processor->isCanceled())
        newSemanticInfo.complete = false;
    newSemanticInfo.doc = doc;

    qCDebug(log) << "update() for source revision:" << source.revision
                 << "canceled:" << !newSemanticInfo.complete;

    setSemanticInfo(newSemanticInfo, emitSignalWhenFinished);
    return newSemanticInfo;
}

bool SemanticInfoUpdaterPrivate::reuseCurrentSemanticInfo(const SemanticInfo::Source &source,
                                                          bool emitSignalWhenFinished)
{
    const SemanticInfo currentSemanticInfo = semanticInfo();

    if (!source.force
            && currentSemanticInfo.complete
            && currentSemanticInfo.revision == source.revision
            && currentSemanticInfo.doc
            && currentSemanticInfo.doc->translationUnit()->ast()
            && currentSemanticInfo.doc->filePath().toString() == source.fileName
            && !currentSemanticInfo.snapshot.isEmpty()
            && currentSemanticInfo.snapshot == source.snapshot) {
        SemanticInfo newSemanticInfo;
        newSemanticInfo.revision = source.revision;
        newSemanticInfo.snapshot = source.snapshot;
        newSemanticInfo.doc = currentSemanticInfo.doc;
        setSemanticInfo(newSemanticInfo, emitSignalWhenFinished);
        qCDebug(log) << "re-using current semantic info, source revision:" << source.revision;
        return true;
    }

    return false;
}

void SemanticInfoUpdaterPrivate::update_helper(QPromise<void> &promise,
                                               const SemanticInfo::Source &source)
{
    FuturizedTopLevelDeclarationProcessor processor(promise);
    update(source, true, &processor);
}

SemanticInfoUpdater::SemanticInfoUpdater()
    : d(new SemanticInfoUpdaterPrivate(this))
{
}

SemanticInfoUpdater::~SemanticInfoUpdater()
{
    d->m_future.cancel();
    d->m_future.waitForFinished();
}

SemanticInfo SemanticInfoUpdater::update(const SemanticInfo::Source &source)
{
    qCDebug(log) << "update() - synchronous";
    d->m_future.cancel();

    const bool emitSignalWhenFinished = false;
    if (d->reuseCurrentSemanticInfo(source, emitSignalWhenFinished)) {
        d->m_future = QFuture<void>();
        return semanticInfo();
    }

    return d->update(source, emitSignalWhenFinished, nullptr);
}

void SemanticInfoUpdater::updateDetached(const SemanticInfo::Source &source)
{
    qCDebug(log) << "updateDetached() - asynchronous";
    d->m_future.cancel();

    const bool emitSignalWhenFinished = true;
    if (d->reuseCurrentSemanticInfo(source, emitSignalWhenFinished)) {
        d->m_future = QFuture<void>();
        return;
    }

    d->m_future = Utils::asyncRun(CppModelManager::instance()->sharedThreadPool(),
                                  &SemanticInfoUpdaterPrivate::update_helper, d.data(), source);
}

SemanticInfo SemanticInfoUpdater::semanticInfo() const
{
    return d->semanticInfo();
}

} // namespace CppEditor
