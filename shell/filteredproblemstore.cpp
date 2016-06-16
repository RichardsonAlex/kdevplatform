/*
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

#include "filteredproblemstore.h"
#include "problem.h"
#include "watcheddocumentset.h"
#include "problemstorenode.h"

#include <KLocalizedString>

using namespace KDevelop;

namespace
{

/// Adds diagnostics as sub-nodes
void addDiagnostics(ProblemStoreNode *node, const QVector<IProblem::Ptr> &diagnostics)
{
    foreach (const IProblem::Ptr &ptr, diagnostics) {
        ProblemNode *child = new ProblemNode(node, ptr);
        node->addChild(child);

        addDiagnostics(child, ptr->diagnostics());
    }
}

/**
 * @brief Base class for grouping strategy classes
 *
 * These classes build the problem tree based on the respective strategies
 */
class GroupingStrategy
{
public:
    GroupingStrategy( ProblemStoreNode *root )
        : m_rootNode(root)
        , m_groupedRootNode(new ProblemStoreNode())
    {
    }

    virtual ~GroupingStrategy(){
    }

    /// Add a problem to the appropriate group
    virtual void addProblem(const IProblem::Ptr &problem) = 0;

    /// Find the specified noe
    const ProblemStoreNode* findNode(int row, ProblemStoreNode *parent = nullptr) const
    {
        if (parent == nullptr)
            return m_groupedRootNode->child(row);
        else
            return parent->child(row);
    }

    /// Returns the number of children nodes
    int count(ProblemStoreNode *parent = nullptr)
    {
        if (parent == nullptr)
            return m_groupedRootNode->count();
        else
            return parent->count();
    }

    /// Clears the problems
    virtual void clear()
    {
        m_groupedRootNode->clear();
    }

protected:
    ProblemStoreNode *m_rootNode;
    QScopedPointer<ProblemStoreNode> m_groupedRootNode;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Implements no grouping strategy, that is just stores the problems without any grouping
class NoGroupingStrategy final : public GroupingStrategy
{
public:
    NoGroupingStrategy(ProblemStoreNode *root)
        : GroupingStrategy(root)
    {
    }

    void addProblem(const IProblem::Ptr &problem) override
    {
        ProblemNode *node = new ProblemNode(m_groupedRootNode.data(), problem);
        addDiagnostics(node, problem->diagnostics());
        m_groupedRootNode->addChild(node);

    }

};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Implements grouping based on path
class PathGroupingStrategy final : public GroupingStrategy
{
public:
    PathGroupingStrategy(ProblemStoreNode *root)
        : GroupingStrategy(root)
    {
    }

    void addProblem(const IProblem::Ptr &problem) override
    {
        QString path = problem->finalLocation().document.str();

        /// See if we already have this path
        ProblemStoreNode *parent = nullptr;
        foreach (ProblemStoreNode *node, m_groupedRootNode->children()) {
            if (node->label() == path) {
                parent = node;
                break;
            }
        }

        /// If not add it!
        if (parent == nullptr) {
            parent = new LabelNode(m_groupedRootNode.data(), path);
            m_groupedRootNode->addChild(parent);
        }

        ProblemNode *node = new ProblemNode(parent, problem);
        addDiagnostics(node, problem->diagnostics());
        parent->addChild(node);
    }

};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Implements grouping based on severity
class SeverityGroupingStrategy final : public GroupingStrategy
{
public:
    enum SeverityGroups
    {
        GroupError          = 0,
        GroupWarning        = 1,
        GroupHint           = 2
    };

    SeverityGroupingStrategy(ProblemStoreNode *root)
        : GroupingStrategy(root)
    {
        /// Create the groups on construction, so there's no need to search for them on addition
        m_groupedRootNode->addChild(new LabelNode(m_groupedRootNode.data(), i18n("Error")));
        m_groupedRootNode->addChild(new LabelNode(m_groupedRootNode.data(), i18n("Warning")));
        m_groupedRootNode->addChild(new LabelNode(m_groupedRootNode.data(), i18n("Hint")));
    }

    void addProblem(const IProblem::Ptr &problem) override
    {
        ProblemStoreNode *parent = nullptr;

        switch (problem->severity()) {
            case IProblem::Error: parent = m_groupedRootNode->child(GroupError); break;
            case IProblem::Warning: parent = m_groupedRootNode->child(GroupWarning); break;
            case IProblem::Hint: parent = m_groupedRootNode->child(GroupHint); break;
            default: break;
        }

        ProblemNode *node = new ProblemNode(m_groupedRootNode.data(), problem);
        addDiagnostics(node, problem->diagnostics());
        parent->addChild(node);
    }

    void clear() override
    {
        m_groupedRootNode->child(GroupError)->clear();
        m_groupedRootNode->child(GroupWarning)->clear();
        m_groupedRootNode->child(GroupHint)->clear();
    }
};

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace KDevelop
{

struct FilteredProblemStorePrivate
{
    FilteredProblemStorePrivate(FilteredProblemStore* q)
        : q(q)
        , m_strategy(new NoGroupingStrategy(q->rootNode()))
        , m_grouping(NoGrouping)
    {
    }

    /// Tells if the problem matches the filters
    bool match(const IProblem::Ptr &problem) const;

    FilteredProblemStore* q;
    QScopedPointer<GroupingStrategy> m_strategy;
    GroupingMethod m_grouping;
};

FilteredProblemStore::FilteredProblemStore(QObject *parent)
    : ProblemStore(parent)
    , d(new FilteredProblemStorePrivate(this))
{
}

FilteredProblemStore::~FilteredProblemStore()
{
}

void FilteredProblemStore::addProblem(const IProblem::Ptr &problem)
{
    ProblemStore::addProblem(problem);

    if (d->match(problem))
        d->m_strategy->addProblem(problem);
}

const ProblemStoreNode* FilteredProblemStore::findNode(int row, ProblemStoreNode *parent) const
{
    return d->m_strategy->findNode(row, parent);
}

int FilteredProblemStore::count(ProblemStoreNode *parent) const
{
    return d->m_strategy->count(parent);
}

void FilteredProblemStore::clear()
{
    d->m_strategy->clear();
    ProblemStore::clear();
}

void FilteredProblemStore::rebuild()
{
    emit beginRebuild();

    d->m_strategy->clear();

    foreach (ProblemStoreNode *node, rootNode()->children()) {
        IProblem::Ptr problem = node->problem();
        if (d->match(problem)) {
            d->m_strategy->addProblem(problem);
        }
    }

    emit endRebuild();
}

void FilteredProblemStore::setGrouping(int grouping)
{
    GroupingMethod g = GroupingMethod(grouping);
    if(g == d->m_grouping)
        return;

    d->m_grouping = g;

    switch (g) {
        case NoGrouping: d->m_strategy.reset(new NoGroupingStrategy(rootNode())); break;
        case PathGrouping: d->m_strategy.reset(new PathGroupingStrategy(rootNode())); break;
        case SeverityGrouping: d->m_strategy.reset(new SeverityGroupingStrategy(rootNode())); break;
    }

    rebuild();
    emit changed();
}

int FilteredProblemStore::grouping() const
{
    return d->m_grouping;
}

bool FilteredProblemStorePrivate::match(const IProblem::Ptr &problem) const
{

    if(problem->severity()!=IProblem::NoSeverity)
    {
        /// If the problem severity isn't in the filter severities it's discarded
        if(!q->severities().testFlag(problem->severity()))
            return false;
    }
    else
    {
        if(!q->severities().testFlag(IProblem::Hint))//workaround for problems wothout correctly set severity
            return false;
    }

    return true;
}

}
