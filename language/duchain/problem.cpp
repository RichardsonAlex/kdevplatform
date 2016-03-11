/* This file is part of KDevelop
Copyright 2007 Hamish Rodda <rodda@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/

#include "problem.h"

#include "duchainregister.h"
#include "topducontextdynamicdata.h"
#include "topducontext.h"
#include "topducontextdata.h"
#include "duchain.h"
#include "duchainlock.h"

#include <interfaces/iassistant.h>
#include <KLocalizedString>

namespace KDevelop {
REGISTER_DUCHAIN_ITEM(Problem);
DEFINE_LIST_MEMBER_HASH(ProblemData, diagnostics, LocalIndexedProblem)
}

using namespace KDevelop;

LocalIndexedProblem::LocalIndexedProblem(const ProblemPointer& problem, const TopDUContext* top)
  : m_index(problem->m_indexInTopContext)
{
    ENSURE_CHAIN_READ_LOCKED
    // ensure child problems are properly serialized before we serialize the parent problem
    if (static_cast<uint>(problem->m_diagnostics.size()) != problem->d_func()->diagnosticsSize()) {
        // see below, the diagnostic size is kept in sync by the mutable API of Problem
        Q_ASSERT(!problem->diagnostics().isEmpty());
        // the const cast is ugly but we don't really "change" the state as observed from the outside
        auto& serialized = const_cast<Problem*>(problem.data())->d_func_dynamic()->diagnosticsList();
        Q_ASSERT(serialized.isEmpty());
        foreach(const ProblemPointer& child, problem->m_diagnostics) {
            serialized << LocalIndexedProblem(child, top);
        }
    }

    if (!m_index) {
        m_index = top->m_dynamicData->allocateProblemIndex(problem);
    }
}

ProblemPointer LocalIndexedProblem::data(const TopDUContext* top) const
{
  if (!m_index) {
    return {};
  }
  return top->m_dynamicData->getProblemForIndex(m_index);
}

Problem::Problem()
    : DUChainBase(*new ProblemData)
    , m_topContext(nullptr)
    , m_indexInTopContext(0)
{
    d_func_dynamic()->setClassId(this);
}

Problem::Problem(ProblemData& data)
    : DUChainBase(data)
    , m_topContext(nullptr)
    , m_indexInTopContext(0)
{
}

Problem::~Problem()
{
}

TopDUContext* Problem::topContext() const
{
    return m_topContext.data();
}

IndexedString Problem::url() const
{
    return d_func()->url;
}

DocumentRange Problem::finalLocation() const
{
    return DocumentRange(d_func()->url, d_func()->m_range.castToSimpleRange());
}

void Problem::setFinalLocation(const DocumentRange& location)
{

    setRange(RangeInRevision::castFromSimpleRange(location));
    d_func_dynamic()->url = location.document;
}

void Problem::clearDiagnostics()
{
    m_diagnostics.clear();
    // keep serialization in sync, see also LocalIndexedProblem ctor above
    d_func_dynamic()->diagnosticsList().clear();
}

QVector<IProblem::Ptr> Problem::diagnostics() const
{
    QVector<IProblem::Ptr> vector;

    foreach(ProblemPointer ptr, m_diagnostics)
    {
        vector.push_back(ptr);
    }

    return vector;
}

void Problem::setDiagnostics(const QVector<IProblem::Ptr> &diagnostics)
{
    clearDiagnostics();

    foreach(const IProblem::Ptr& problem, diagnostics)
    {
        addDiagnostic(problem);
    }
}

void Problem::addDiagnostic(const IProblem::Ptr &diagnostic)
{
    Problem *problem = dynamic_cast<Problem*>(diagnostic.data());
    Q_ASSERT(problem != NULL);

    ProblemPointer ptr(problem);

    m_diagnostics << ptr;
}

QString Problem::description() const
{
    return d_func()->description.str();
}

void Problem::setDescription(const QString& description)
{
    d_func_dynamic()->description = IndexedString(description);
}

QString Problem::explanation() const
{
    return d_func()->explanation.str();
}

void Problem::setExplanation(const QString& explanation)
{
    d_func_dynamic()->explanation = IndexedString(explanation);
}

IProblem::Source Problem::source() const
{
    return d_func()->source;
}

void Problem::setSource(IProblem::Source source)
{
    d_func_dynamic()->source = source;
}

QExplicitlySharedDataPointer<IAssistant> Problem::solutionAssistant() const
{
    return {};
}

IProblem::Severity Problem::severity() const
{
    return d_func()->severity;
}

void Problem::setSeverity(Severity severity)
{
    d_func_dynamic()->severity = severity;
}

QString Problem::severityString() const
{
    switch(severity()) {
        case IProblem::Error:
            return i18n("Error");
        case IProblem::Warning:
            return i18n("Warning");
        case IProblem::Hint:
            return i18n("Hint");
    }
    return QString();
}

QString Problem::sourceString() const
{
    switch (source()) {
        case IProblem::Disk:
            return i18n("Disk");
        case IProblem::Preprocessor:
            return i18n("Preprocessor");
        case IProblem::Lexer:
            return i18n("Lexer");
        case IProblem::Parser:
            return i18n("Parser");
        case IProblem::DUChainBuilder:
            return i18n("Definition-Use Chain");
        case IProblem::SemanticAnalysis:
            return i18n("Semantic analysis");
        case IProblem::ToDo:
            return i18n("To-do");
        case IProblem::Unknown:
        default:
            return i18n("Unknown");
    }
}

QString Problem::toString() const
{
    return i18nc("<severity>: <description> in <url>:[<range>]: <explanation> (found by <source>)",
                 "%1: %2 in %3:[(%4,%5),(%6,%7)]: %8 (found by %9)"
        , severityString()
        , description()
        , url().str()
        , range().start.line
        , range().start.column
        , range().end.line
        , range().end.column
        , (explanation().isEmpty() ? i18n("<no explanation>") : explanation())
        , sourceString());
}

void Problem::rebuildDynamicData(DUContext* parent, uint ownIndex)
{
    auto top = dynamic_cast<TopDUContext*>(parent);
    Q_ASSERT(top);

    m_topContext = top;
    m_indexInTopContext = ownIndex;

    // deserialize child diagnostics here, as the top-context might get unloaded
    // but we still want to keep the child-diagnostics in-tact, as one would assume
    // a shared-ptr works.
    const auto data = d_func();
    m_diagnostics.reserve(data->diagnosticsSize());
    for (uint i = 0; i < data->diagnosticsSize(); ++i) {
        m_diagnostics << ProblemPointer(data->diagnostics()[i].data(top));
    }

    DUChainBase::rebuildDynamicData(parent, ownIndex);
}

QDebug operator<<(QDebug s, const Problem& problem)
{
    s.nospace() << problem.toString();
    return s.space();
}

QDebug operator<<(QDebug s, const ProblemPointer& problem)
{
    if (!problem) {
        s.nospace() << "<invalid problem>";
    } else {
        s.nospace() << problem->toString();
    }
    return s.space();
}
