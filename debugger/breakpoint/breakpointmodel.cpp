/* This file is part of the KDE project
   Copyright (C) 2002 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
   Copyright (C) 2002 John Firebaugh <jfirebaugh@kde.org>
   Copyright (C) 2006, 2008 Vladimir Prus <ghost@cs.msu.su>
   Copyright (C) 2007 Hamish Rodda <rodda@kde.org>
   Copyright (C) 2009 Niko Sams <niko.sams@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.
   If not, see <http://www.gnu.org/licenses/>.
*/

#include "breakpointmodel.h"

#include <QIcon>
#include <QPixmap>

#include <KParts/PartManager>
#include <KLocalizedString>
#include <KTextEditor/Document>
#include <ktexteditor/movinginterface.h>

#include "../interfaces/icore.h"
#include "../interfaces/idebugcontroller.h"
#include "../interfaces/idocumentcontroller.h"
#include "../interfaces/idocument.h"
#include "../interfaces/ipartcontroller.h"
#include <interfaces/idebugsession.h>
#include <interfaces/ibreakpointcontroller.h>
#include <interfaces/isession.h>
#include "util/debug.h"
#include "breakpoint.h"
#include <KConfigCore/KSharedConfig>
#include <KConfigGroup>
#include <QAction>
#include <QMenu>
#include <QMessageBox>

#define IF_DEBUG(x)

using namespace KDevelop;
using namespace KTextEditor;

namespace {

IBreakpointController* breakpointController()
{
    KDevelop::ICore* core = KDevelop::ICore::self();
    if (!core) {
        return nullptr;
    }
    IDebugController* controller = core->debugController();
    if (!controller) {
        return nullptr;
    }
    IDebugSession* session = controller->currentSession();
    return session ? session->breakpointController() : nullptr;
}

} // anonymous namespace

BreakpointModel::BreakpointModel(QObject* parent)
    : QAbstractTableModel(parent),
      m_dontUpdateMarks(false)
{
    connect(this, &BreakpointModel::dataChanged, this, &BreakpointModel::updateMarks);

    if (KDevelop::ICore::self()->partController()) { //TODO remove if
        foreach(KParts::Part* p, KDevelop::ICore::self()->partController()->parts())
            slotPartAdded(p);
        connect(KDevelop::ICore::self()->partController(),
                &IPartController::partAdded,
                this,
                &BreakpointModel::slotPartAdded);
    }


    connect (KDevelop::ICore::self()->documentController(),
             &IDocumentController::textDocumentCreated,
             this,
             &BreakpointModel::textDocumentCreated);
    connect (KDevelop::ICore::self()->documentController(),
                &IDocumentController::documentSaved,
                this, &BreakpointModel::documentSaved);
}

BreakpointModel::~BreakpointModel()
{
    qDeleteAll(m_breakpoints);
}

void BreakpointModel::slotPartAdded(KParts::Part* part)
{
    if (auto doc = qobject_cast<KTextEditor::Document*>(part))
    {
        MarkInterface *iface = dynamic_cast<MarkInterface*>(doc);
        if( !iface )
            return;

        iface->setMarkDescription((MarkInterface::MarkTypes)BreakpointMark, i18n("Breakpoint"));
        iface->setMarkPixmap((MarkInterface::MarkTypes)BreakpointMark, *breakpointPixmap());
        iface->setMarkPixmap((MarkInterface::MarkTypes)PendingBreakpointMark, *pendingBreakpointPixmap());
        iface->setMarkPixmap((MarkInterface::MarkTypes)ReachedBreakpointMark, *reachedBreakpointPixmap());
        iface->setMarkPixmap((MarkInterface::MarkTypes)DisabledBreakpointMark, *disabledBreakpointPixmap());
        iface->setEditableMarks( MarkInterface::Bookmark | BreakpointMark );
        updateMarks();
    }
}

void BreakpointModel::textDocumentCreated(KDevelop::IDocument* doc)
{
    KTextEditor::MarkInterface *iface =
        qobject_cast<KTextEditor::MarkInterface*>(doc->textDocument());

    if (iface) {
        // can't use new signal slot syntax here, MarkInterface is not a QObject
        connect(doc->textDocument(), SIGNAL(
                     markChanged(KTextEditor::Document*,
                                 KTextEditor::Mark,
                                 KTextEditor::MarkInterface::MarkChangeAction)),
                 this,
                 SLOT(markChanged(KTextEditor::Document*,
                                 KTextEditor::Mark,
                                  KTextEditor::MarkInterface::MarkChangeAction)));
        connect(doc->textDocument(), SIGNAL(markContextMenuRequested(KTextEditor::Document*, KTextEditor::Mark, QPoint, bool&)),
                SLOT(markContextMenuRequested(KTextEditor::Document*, KTextEditor::Mark, QPoint, bool&)));
    }
}

void BreakpointModel::markContextMenuRequested(Document* document, Mark mark, const QPoint &pos, bool& handled)
{

    int type = mark.type;
    qCDebug(DEBUGGER) << type;

    /* Is this a breakpoint mark, to begin with? */
    if (!(type & AllBreakpointMarks)) return;

    Breakpoint *b = breakpoint(document->url(), mark.line);
    if (!b) {
        QMessageBox::critical(nullptr, i18n("Breakpoint not found"), i18n("Couldn't find breakpoint at %1:%2", document->url().toString(), mark.line));
        return;
    }

    QMenu menu;
    QAction deleteAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("&Delete Breakpoint"), 0);
    QAction disableAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")), i18n("&Disable Breakpoint"), 0);
    QAction enableAction(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")), i18n("&Enable Breakpoint"), 0);
    menu.addAction(&deleteAction);
    if (b->enabled()) {
        menu.addAction(&disableAction);
    } else {
        menu.addAction(&enableAction);
    }
    QAction *a = menu.exec(pos);
    if (a == &deleteAction) {
        b->setDeleted();
    } else if (a == &disableAction) {
        b->setData(Breakpoint::EnableColumn, Qt::Unchecked);
    } else if (a == &enableAction) {
        b->setData(Breakpoint::EnableColumn, Qt::Checked);
    }

    handled = true;
}


QVariant
BreakpointModel::headerData(int section, Qt::Orientation orientation,
                                 int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();

    if (role == Qt::DecorationRole ) {
        if (section == 0)
            return QIcon::fromTheme(QStringLiteral("dialog-ok-apply"));
        else if (section == 1)
            return QIcon::fromTheme(QStringLiteral("system-switch-user"));
    }

    if (role == Qt::DisplayRole) {
        if (section == 0 || section == 1) return "";
        if (section == 2) return i18n("Type");
        if (section == 3) return i18n("Location");
        if (section == 4) return i18n("Condition");
    }

    if (role == Qt::ToolTipRole) {
        if (section == 0) return i18n("Active status");
        if (section == 1) return i18n("State");
        return headerData(section, orientation, Qt::DisplayRole);

    }
    return QVariant();
}

Qt::ItemFlags BreakpointModel::flags(const QModelIndex &index) const
{
    /* FIXME: all this logic must be in item */
    if (!index.isValid())
        return 0;

    if (index.column() == 0)
        return static_cast<Qt::ItemFlags>(
            Qt::ItemIsEnabled | Qt::ItemIsSelectable
            | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);

    if (index.column() == Breakpoint::LocationColumn
        || index.column() == Breakpoint::ConditionColumn)
        return static_cast<Qt::ItemFlags>(
            Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);

    return static_cast<Qt::ItemFlags>(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
}

QModelIndex BreakpointModel::breakpointIndex(KDevelop::Breakpoint* b, int column)
{
    int row = m_breakpoints.indexOf(b);
    if (row == -1) return QModelIndex();
    return index(row, column);
}

bool KDevelop::BreakpointModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (count < 1 || (row < 0) || (row + count) > rowCount(parent))
        return false;

    IBreakpointController* controller = breakpointController();

    beginRemoveRows(parent, row, row+count-1);
    for (int i=0; i < count; ++i) {
        Breakpoint* b = m_breakpoints.at(row);
        b->m_deleted = true;
        if (controller)
            controller->breakpointAboutToBeDeleted(row);
        m_breakpoints.removeAt(row);
        b->m_model = 0;
        // To be changed: the controller is currently still responsible for deleting the breakpoint
        // object
    }
    endRemoveRows();
    updateMarks();
    scheduleSave();
    return true;
}

int KDevelop::BreakpointModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return m_breakpoints.count();
    }
    return 0;
}

int KDevelop::BreakpointModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 5;
}

QVariant BreakpointModel::data(const QModelIndex& index, int role) const
{
    if (!index.parent().isValid() && index.row() < m_breakpoints.count()) {
        return m_breakpoints.at(index.row())->data(index.column(), role);
    }
    return QVariant();
}

bool KDevelop::BreakpointModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.parent().isValid() && index.row() < m_breakpoints.count() && (role == Qt::EditRole || role == Qt::CheckStateRole)) {
        return m_breakpoints.at(index.row())->setData(index.column(), value);
    }
    return false;
}

void BreakpointModel::updateState(int row, Breakpoint::BreakpointState state)
{
    Breakpoint* breakpoint = m_breakpoints.at(row);
    if (state != breakpoint->m_state) {
        breakpoint->m_state = state;
        reportChange(breakpoint, Breakpoint::StateColumn);
    }
}

void BreakpointModel::updateHitCount(int row, int hitCount)
{
    Breakpoint* breakpoint = m_breakpoints.at(row);
    if (hitCount != breakpoint->m_hitCount) {
        breakpoint->m_hitCount = hitCount;
        reportChange(breakpoint, Breakpoint::HitCountColumn);
    }
}

void BreakpointModel::updateErrorText(int row, const QString& errorText)
{
    Breakpoint* breakpoint = m_breakpoints.at(row);
    if (breakpoint->m_errorText != errorText) {
        breakpoint->m_errorText = errorText;
        reportChange(breakpoint, Breakpoint::StateColumn);
    }

    if (!errorText.isEmpty()) {
        emit error(row, errorText);
    }
}

void BreakpointModel::notifyHit(int row)
{
    emit hit(row);
}

void BreakpointModel::markChanged(
    KTextEditor::Document *document,
    KTextEditor::Mark mark,
    KTextEditor::MarkInterface::MarkChangeAction action)
{
    int type = mark.type;
    /* Is this a breakpoint mark, to begin with? */
    if (!(type & AllBreakpointMarks)) return;

    if (action == KTextEditor::MarkInterface::MarkAdded) {
        Breakpoint *b = breakpoint(document->url(), mark.line);
        if (b) {
            //there was already a breakpoint, so delete instead of adding
            b->setDeleted();
            return;
        }
        Breakpoint *breakpoint = addCodeBreakpoint(document->url(), mark.line);
        KTextEditor::MovingInterface *moving = qobject_cast<KTextEditor::MovingInterface*>(document);
        if (moving) {
            KTextEditor::MovingCursor* cursor = moving->newMovingCursor(KTextEditor::Cursor(mark.line, 0));
            // can't use new signal/slot syntax here, MovingInterface is not a QObject
            connect(document, SIGNAL(aboutToDeleteMovingInterfaceContent(KTextEditor::Document*)),
                    this, SLOT(aboutToDeleteMovingInterfaceContent(KTextEditor::Document*)), Qt::UniqueConnection);
            breakpoint->setMovingCursor(cursor);
        }
    } else {
        // Find this breakpoint and delete it
        Breakpoint *b = breakpoint(document->url(), mark.line);
        if (b) {
            b->setDeleted();
        }
    }

#if 0
    if ( KDevelop::ICore::self()->documentController()->activeDocument() && KDevelop::ICore::self()->documentController()->activeDocument()->textDocument() == document )
    {
        //bring focus back to the editor
        // TODO probably want a different command here
        KDevelop::ICore::self()->documentController()->activateDocument(KDevelop::ICore::self()->documentController()->activeDocument());
    }
#endif
}

const QPixmap* BreakpointModel::breakpointPixmap()
{
  static QPixmap pixmap=QIcon::fromTheme(QStringLiteral("script-error")).pixmap(QSize(22,22), QIcon::Active, QIcon::Off);
  return &pixmap;
}

const QPixmap* BreakpointModel::pendingBreakpointPixmap()
{
  static QPixmap pixmap=QIcon::fromTheme(QStringLiteral("script-error")).pixmap(QSize(22,22), QIcon::Normal, QIcon::Off);
  return &pixmap;
}

const QPixmap* BreakpointModel::reachedBreakpointPixmap()
{
  static QPixmap pixmap=QIcon::fromTheme(QStringLiteral("script-error")).pixmap(QSize(22,22), QIcon::Selected, QIcon::Off);
  return &pixmap;
}

const QPixmap* BreakpointModel::disabledBreakpointPixmap()
{
  static QPixmap pixmap=QIcon::fromTheme(QStringLiteral("script-error")).pixmap(QSize(22,22), QIcon::Disabled, QIcon::Off);
  return &pixmap;
}

void BreakpointModel::toggleBreakpoint(const QUrl& url, const KTextEditor::Cursor& cursor)
{
    Breakpoint *b = breakpoint(url, cursor.line());
    if (b) {
        b->setDeleted();
    } else {
        addCodeBreakpoint(url, cursor.line());
    }
}

void BreakpointModel::reportChange(Breakpoint* breakpoint, Breakpoint::Column column)
{
    // note: just a portion of Breakpoint::Column is displayed in this model!
    if (column >= 0 && column < columnCount()) {
        QModelIndex idx = breakpointIndex(breakpoint, column);
        Q_ASSERT(idx.isValid()); // make sure we don't pass invalid indices to dataChanged()
        emit dataChanged(idx, idx);
    }

    if (IBreakpointController* controller = breakpointController()) {
        int row = m_breakpoints.indexOf(breakpoint);
        Q_ASSERT(row != -1);
        controller->breakpointModelChanged(row, ColumnFlags(1 << column));
    }

    scheduleSave();
}

uint BreakpointModel::breakpointType(Breakpoint *breakpoint)
{
    uint type = BreakpointMark;
    if (!breakpoint->enabled()) {
        type = DisabledBreakpointMark;
    } else if (breakpoint->hitCount() > 0) {
        type = ReachedBreakpointMark;
    } else if (breakpoint->state() == Breakpoint::PendingState) {
        type = PendingBreakpointMark;
    }
    return type;
}

void KDevelop::BreakpointModel::updateMarks()
{
    if (m_dontUpdateMarks) return;

    //add marks
    foreach (Breakpoint *breakpoint, m_breakpoints) {
        if (breakpoint->kind() != Breakpoint::CodeBreakpoint) continue;
        if (breakpoint->line() == -1) continue;
        IDocument *doc = ICore::self()->documentController()->documentForUrl(breakpoint->url());
        if (!doc) continue;
        KTextEditor::MarkInterface *mark = qobject_cast<KTextEditor::MarkInterface*>(doc->textDocument());
        if (!mark) continue;
        uint type = breakpointType(breakpoint);
        IF_DEBUG( qCDebug(DEBUGGER) << type << breakpoint->url() << mark->mark(breakpoint->line()); )

        doc->textDocument()->blockSignals(true);
        if (mark->mark(breakpoint->line()) & AllBreakpointMarks) {
            if (!(mark->mark(breakpoint->line()) & type)) {
                mark->removeMark(breakpoint->line(), AllBreakpointMarks);
                mark->addMark(breakpoint->line(), type);
            }
        } else {
            mark->addMark(breakpoint->line(), type);
        }
        doc->textDocument()->blockSignals(false);
    }

    //remove marks
    foreach (IDocument *doc, ICore::self()->documentController()->openDocuments()) {
        KTextEditor::MarkInterface *mark = qobject_cast<KTextEditor::MarkInterface*>(doc->textDocument());
        if (!mark) continue;

        doc->textDocument()->blockSignals(true);
        foreach (KTextEditor::Mark *m, mark->marks()) {
            if (!(m->type & AllBreakpointMarks)) continue;
            IF_DEBUG( qCDebug(DEBUGGER) << m->line << m->type; )
            foreach (Breakpoint *breakpoint, m_breakpoints) {
                if (breakpoint->kind() != Breakpoint::CodeBreakpoint) continue;
                if (doc->url() == breakpoint->url() && m->line == breakpoint->line()) {
                    goto continueNextMark;
                }
            }
            mark->removeMark(m->line, AllBreakpointMarks);
            continueNextMark:;
        }
        doc->textDocument()->blockSignals(false);
    }
}

void BreakpointModel::documentSaved(KDevelop::IDocument* doc)
{
    IF_DEBUG( qCDebug(DEBUGGER); )
    foreach (Breakpoint *breakpoint, m_breakpoints) {
        if (breakpoint->movingCursor()) {
            if (breakpoint->movingCursor()->document() != doc->textDocument()) continue;
            if (breakpoint->movingCursor()->line() == breakpoint->line()) continue;
            m_dontUpdateMarks = true;
            breakpoint->setLine(breakpoint->movingCursor()->line());
            m_dontUpdateMarks = false;
        }
    }
}
void BreakpointModel::aboutToDeleteMovingInterfaceContent(KTextEditor::Document* document)
{
    foreach (Breakpoint *breakpoint, m_breakpoints) {
        if (breakpoint->movingCursor() && breakpoint->movingCursor()->document() == document) {
            breakpoint->setMovingCursor(0);
        }
    }
}

void BreakpointModel::load()
{
    KConfigGroup breakpoints = ICore::self()->activeSession()->config()->group("Breakpoints");
    int count = breakpoints.readEntry("number", 0);
    if (count == 0)
        return;

    beginInsertRows(QModelIndex(), 0, count - 1);
    for (int i = 0; i < count; ++i) {
        if (!breakpoints.group(QString::number(i)).readEntry("kind", "").isEmpty()) {
            new Breakpoint(this, breakpoints.group(QString::number(i)));
        }
    }
    endInsertRows();
}

void BreakpointModel::save()
{
    m_dirty = false;

    KConfigGroup breakpoints = ICore::self()->activeSession()->config()->group("Breakpoints");
    breakpoints.writeEntry("number", m_breakpoints.count());
    int i = 0;
    foreach (Breakpoint *b, m_breakpoints) {
        KConfigGroup g = breakpoints.group(QString::number(i));
        b->save(g);
        ++i;
    }
    breakpoints.sync();
}

void BreakpointModel::scheduleSave()
{
    if (m_dirty)
        return;

    m_dirty = true;
    QTimer::singleShot(0, this, SLOT(save()));
}

QList<Breakpoint*> KDevelop::BreakpointModel::breakpoints() const
{
    return m_breakpoints;
}

Breakpoint* BreakpointModel::breakpoint(int row)
{
    if (row >= m_breakpoints.count()) return 0;
    return m_breakpoints.at(row);
}

Breakpoint* BreakpointModel::addCodeBreakpoint()
{
    beginInsertRows(QModelIndex(), m_breakpoints.count(), m_breakpoints.count());
    Breakpoint* n = new Breakpoint(this, Breakpoint::CodeBreakpoint);
    endInsertRows();
    return n;
}

Breakpoint* BreakpointModel::addCodeBreakpoint(const QUrl& url, int line)
{
    Breakpoint* n = addCodeBreakpoint();
    n->setLocation(url, line);
    return n;
}

Breakpoint* BreakpointModel::addCodeBreakpoint(const QString& expression)
{
    Breakpoint* n = addCodeBreakpoint();
    n->setExpression(expression);
    return n;
}

Breakpoint* BreakpointModel::addWatchpoint()
{
    beginInsertRows(QModelIndex(), m_breakpoints.count(), m_breakpoints.count());
    Breakpoint* n = new Breakpoint(this, Breakpoint::WriteBreakpoint);
    endInsertRows();
    return n;
}

Breakpoint* BreakpointModel::addWatchpoint(const QString& expression)
{
    Breakpoint* n = addWatchpoint();
    n->setExpression(expression);
    return n;
}

Breakpoint* BreakpointModel::addReadWatchpoint()
{
    beginInsertRows(QModelIndex(), m_breakpoints.count(), m_breakpoints.count());
    Breakpoint* n = new Breakpoint(this, Breakpoint::ReadBreakpoint);
    endInsertRows();
    return n;
}

Breakpoint* BreakpointModel::addReadWatchpoint(const QString& expression)
{
    Breakpoint* n = addReadWatchpoint();
    n->setExpression(expression);
    return n;
}

Breakpoint* BreakpointModel::addAccessWatchpoint()
{
    beginInsertRows(QModelIndex(), m_breakpoints.count(), m_breakpoints.count());
    Breakpoint* n = new Breakpoint(this, Breakpoint::AccessBreakpoint);
    endInsertRows();
    return n;
}


Breakpoint* BreakpointModel::addAccessWatchpoint(const QString& expression)
{
    Breakpoint* n = addAccessWatchpoint();
    n->setExpression(expression);
    return n;
}

void BreakpointModel::registerBreakpoint(Breakpoint* breakpoint)
{
    Q_ASSERT(!m_breakpoints.contains(breakpoint));
    int row = m_breakpoints.size();
    m_breakpoints << breakpoint;
    if (IBreakpointController* controller = breakpointController()) {
        controller->breakpointAdded(row);
    }
    scheduleSave();
}

Breakpoint* BreakpointModel::breakpoint(const QUrl& url, int line) {
    foreach (Breakpoint *b, m_breakpoints) {
        if (b->url() == url && b->line() == line) {
            return b;
        }
    }
    return 0;
}
