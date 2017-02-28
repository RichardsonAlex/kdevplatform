/*
 * Copyright 2007 Hamish Rodda <rodda@kde.org>
 * Copyright 2015 Laszlo Kis-Adam <laszlo.kis-adam@kdemail.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "problemreportermodel.h"

#include <language/backgroundparser/backgroundparser.h>
#include <language/backgroundparser/parsejob.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/parsingenvironment.h>
#include <language/duchain/topducontext.h>
#include <language/duchain/problem.h>
#include <language/duchain/duchainutils.h>
#include <language/assistant/staticassistantsmanager.h>

#include <QThread>
#include <QTimer>

#include <serialization/indexedstring.h>

#include <shell/watcheddocumentset.h>
#include <shell/filteredproblemstore.h>

#include <interfaces/icore.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/idocument.h>

using namespace KDevelop;

const int ProblemReporterModel::MinTimeout = 1000;
const int ProblemReporterModel::MaxTimeout = 5000;

ProblemReporterModel::ProblemReporterModel(QObject* parent)
    : ProblemModel(parent, new FilteredProblemStore())
{
    setFeatures(CanDoFullUpdate | CanShowImports | ScopeFilter | SeverityFilter | ShowSource);

    m_minTimer = new QTimer(this);
    m_minTimer->setInterval(MinTimeout);
    m_minTimer->setSingleShot(true);
    connect(m_minTimer, &QTimer::timeout, this, &ProblemReporterModel::timerExpired);
    m_maxTimer = new QTimer(this);
    m_maxTimer->setInterval(MaxTimeout);
    m_maxTimer->setSingleShot(true);
    connect(m_maxTimer, &QTimer::timeout, this, &ProblemReporterModel::timerExpired);
    connect(store(), &FilteredProblemStore::changed, this, &ProblemReporterModel::onProblemsChanged);
    connect(ICore::self()->languageController()->staticAssistantsManager(), &StaticAssistantsManager::problemsChanged,
            this, &ProblemReporterModel::onProblemsChanged);
}

ProblemReporterModel::~ProblemReporterModel()
{
}

QVector<KDevelop::IProblem::Ptr> ProblemReporterModel::problems(const QSet<KDevelop::IndexedString>& docs) const
{
    QVector<IProblem::Ptr> result;
    DUChainReadLocker lock;

    foreach (const IndexedString& doc, docs) {
        if (doc.isEmpty())
            continue;

        TopDUContext* ctx = DUChain::self()->chainForDocument(doc);
        if (!ctx)
            continue;

        foreach (ProblemPointer p, DUChainUtils::allProblemsForContext(ctx)) {
            result.append(p);
        }
    }

    return result;
}

void ProblemReporterModel::forceFullUpdate()
{
    Q_ASSERT(thread() == QThread::currentThread());

    QSet<IndexedString> documents = store()->documents()->get();
    if (showImports())
        documents += store()->documents()->getImports();

    DUChainReadLocker lock(DUChain::lock());
    foreach (const IndexedString& document, documents) {
        if (document.isEmpty())
            continue;

        TopDUContext::Features updateType = TopDUContext::ForceUpdate;
        if (documents.size() == 1)
            updateType = TopDUContext::ForceUpdateRecursive;
        DUChain::self()->updateContextForUrl(
            document,
            (TopDUContext::Features)(updateType | TopDUContext::VisibleDeclarationsAndContexts));
    }
}

void ProblemReporterModel::onProblemsChanged()
{
    rebuildProblemList();
}

void ProblemReporterModel::timerExpired()
{
    m_minTimer->stop();
    m_maxTimer->stop();
    rebuildProblemList();
}

void ProblemReporterModel::setCurrentDocument(KDevelop::IDocument* doc)
{
    Q_ASSERT(thread() == QThread::currentThread());

    beginResetModel();

    /// Will trigger signal changed() if problems change
    store()->setCurrentDocument(IndexedString(doc->url()));
    endResetModel();
}

void ProblemReporterModel::problemsUpdated(const KDevelop::IndexedString& url)
{
    Q_ASSERT(thread() == QThread::currentThread());

    // skip update for urls outside current scope
    if (!store()->documents()->get().contains(url) &&
        !(showImports() && store()->documents()->getImports().contains(url)))
        return;

    /// m_minTimer will expire in MinTimeout unless some other parsing job finishes in this period.
    m_minTimer->start();
    /// m_maxTimer will expire unconditionally in MaxTimeout
    if (!m_maxTimer->isActive()) {
        m_maxTimer->start();
    }
}

void ProblemReporterModel::rebuildProblemList()
{
    /// No locking here, because it may be called from an already locked context
    beginResetModel();

    QVector<IProblem::Ptr> allProblems = problems(store()->documents()->get());

    if (showImports())
        allProblems += problems(store()->documents()->getImports());

    store()->setProblems(allProblems);

    endResetModel();
}
