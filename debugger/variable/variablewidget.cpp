// **************************************************************************
//    begin                : Sun Aug 8 1999
//    copyright            : (C) 1999 by John Birch
//    email                : jbb@kdevelop.org
// **************************************************************************
// * Copyright 2006 Vladimir Prus <ghost@cs.msu.su>
// **************************************************************************
// *                                                                        *
// *   This program is free software; you can redistribute it and/or modify *
// *   it under the terms of the GNU General Public License as published by *
// *   the Free Software Foundation; either version 2 of the License, or    *
// *   (at your option) any later version.                                  *
// *                                                                        *
// **************************************************************************

#include "variablewidget.h"

#include <QApplication>
#include <QCursor>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QVBoxLayout>

#include <KHistoryComboBox>
#include <KLocalizedString>
#include <QSortFilterProxyModel>

#include "../util/treemodel.h"
#include "../../interfaces/icore.h"
#include <interfaces/idebugcontroller.h>
#include "../interfaces/ivariablecontroller.h"
#include "variablecollection.h"
#include "variablesortmodel.h"
#include "util/debug.h"

/** The variables widget is passive, and is invoked by the rest of the
    code via two main Q_SLOTS:
    - slotDbgStatus
    - slotCurrentFrame

    The first is received the program status changes and the second is
    received after current frame in the debugger can possibly changes.

    The widget has a list item for each frame/thread combination, with
    variables as children. However, at each moment only one item is shown.
    When handling the slotCurrentFrame, we check if variables for the
    current frame are available. If yes, we simply show the corresponding item.
    Otherwise, we fetch the new data from debugger.

    Fetching the data is done by emitting the produceVariablesInfo signal.
    In response, we get slotParametersReady and slotLocalsReady signal,
    in that order.

    The data is parsed and changed variables are highlighted. After that,
    we 'trim' variable items that were not reported by gdb -- that is, gone
    out of scope.
*/

// **************************************************************************
// **************************************************************************
// **************************************************************************

namespace KDevelop
{

VariableCollection *variableCollection()
{
    return ICore::self()->debugController()->variableCollection();
}


VariableWidget::VariableWidget(IDebugController* controller, QWidget *parent)
: QWidget(parent), m_variablesRoot(controller->variableCollection()->root())
{
  //setWindowIcon(QIcon::fromTheme("math_brace"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("debugger"), windowIcon()));
    setWindowTitle(i18n("Debugger Variables"));

    m_proxy = new VariableSortProxyModel(this);
    m_varTree = new VariableTree(controller, this, m_proxy);
    setFocusProxy(m_varTree);

    m_watchVarEditor = new KHistoryComboBox( this );

    QVBoxLayout *topLayout = new QVBoxLayout(this);
    topLayout->addWidget(m_varTree, 10);
    topLayout->addWidget(m_watchVarEditor);
    topLayout->setMargin(0);

    connect(m_watchVarEditor, static_cast<void(KHistoryComboBox::*)(const QString&)>(&KHistoryComboBox::returnPressed),
            this, &VariableWidget::slotAddWatch);

    //TODO
    //connect(plugin, SIGNAL(raiseVariableViews()), this, SIGNAL(requestRaise()));

    // Setup help items.

    setWhatsThis( i18n(
        "<b>Variable tree</b>"
        "The variable tree allows you to see the values of local "
        "variables and arbitrary expressions.<br />"
        "Local variables are displayed automatically and are updated "
        "as you step through your program. "
        "For each expression you enter, you can either evaluate it once, "
        "or \"watch\" it (make it auto-updated). Expressions that are not "
        "auto-updated can be updated manually from the context menu. "
        "Expressions can be renamed to more descriptive names by clicking "
        "on the name column.<br />"
        "To change the value of a variable or an expression, "
        "click on the value.<br />"));

    m_watchVarEditor->setWhatsThis(
                    i18n("<b>Expression entry</b>"
                         "Type in expression to watch."));

}

void VariableWidget::slotAddWatch(const QString &expression)
{
    if (!expression.isEmpty())
    {
        m_watchVarEditor->addToHistory(expression);
        qCDebug(DEBUGGER) << "Trying to add watch";
        Variable* v = m_variablesRoot->watches()->add(expression);
        if (v) {
            /* For watches on structures, we really do want them to be shown
            expanded.  Except maybe for structure with custom pretty printing,
            but will handle that later.
            FIXME: it does not actually works now.
            */
            //QModelIndex index = variableCollection()->indexForItem(v, 0);
            //varTree_->setExpanded(index, true);
        }
        m_watchVarEditor->clearEditText();
    }
}

void VariableWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    variableCollection()->variableWidgetHidden();
}

void VariableWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    variableCollection()->variableWidgetShown();
}

// **************************************************************************
// **************************************************************************
// **************************************************************************

VariableTree::VariableTree(IDebugController *controller,
                           VariableWidget *parent, QSortFilterProxyModel *proxy)
    : AsyncTreeView(controller->variableCollection(), proxy, parent)
    , m_proxy(proxy)
#if 0
,
      activePopup_(0),
      toggleWatch_(0)
#endif
{
    setRootIsDecorated(true);
    setAllColumnsShowFocus(true);

    // setting proxy model
    m_model = static_cast<TreeModel *>(controller->variableCollection());
    m_proxy->setSourceModel(m_model);
    setModel(m_proxy);
    setSortingEnabled(true);
    sortByColumn(VariableCollection::NameColumn, Qt::AscendingOrder);

    QModelIndex index = controller->variableCollection()->indexForItem(
        controller->variableCollection()->watches(), 0);
    setExpanded(index, true);

    m_signalMapper = new QSignalMapper(this);
    setupActions();
}


VariableCollection* VariableTree::collection() const
{
    Q_ASSERT(qobject_cast<VariableCollection*>(static_cast<QSortFilterProxyModel*>(model())->sourceModel()));
    return static_cast<VariableCollection*>(model());
}


VariableTree::~VariableTree()
{
}

void VariableTree::setupActions()
{
    // TODO decorate this properly to make nice menu title
    m_contextMenuTitle = new QAction(this);
    m_contextMenuTitle->setEnabled(false);

    // make Format menu action group
    m_formatMenu = new QMenu(i18n("&Format"), this);
    QActionGroup *ag= new QActionGroup(m_formatMenu);

    QAction* act;

    act = new QAction(i18n("&Natural"), ag);
    act->setData(Variable::Natural);
    act->setShortcut(Qt::Key_N);
    m_formatMenu->addAction(act);

    act = new QAction(i18n("&Binary"), ag);
    act->setData(Variable::Binary);
    act->setShortcut(Qt::Key_B);
    m_formatMenu->addAction(act);

    act = new QAction(i18n("&Octal"), ag);
    act->setData(Variable::Octal);
    act->setShortcut(Qt::Key_O);
    m_formatMenu->addAction(act);

    act = new QAction(i18n("&Decimal"), ag);
    act->setData(Variable::Decimal);
    act->setShortcut(Qt::Key_D);
    m_formatMenu->addAction(act);

    act = new QAction(i18n("&Hexadecimal"), ag);
    act->setData(Variable::Hexadecimal);
    act->setShortcut(Qt::Key_H);
    m_formatMenu->addAction(act);

    foreach(QAction* act, m_formatMenu->actions())
    {
        act->setCheckable(true);
        act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_signalMapper->setMapping(act, act->data().toInt());
        connect(act, &QAction::triggered, m_signalMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map));
        addAction(act);
    }
    connect(m_signalMapper, static_cast<void(QSignalMapper::*)(int)>(&QSignalMapper::mapped), this, &VariableTree::changeVariableFormat);

    m_watchDelete = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-delete")), i18n( "Remove Watch Variable" ), this);

    m_watchDelete->setShortcut(Qt::Key_Delete);
    m_watchDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_watchDelete);
    connect(m_watchDelete, &QAction::triggered, this, &VariableTree::watchDelete);

    m_copyVariableValue = new QAction(i18n("&Copy Value"), this);
    m_copyVariableValue->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_copyVariableValue->setShortcut(QKeySequence::Copy);
    connect(m_copyVariableValue, &QAction::triggered, this, &VariableTree::copyVariableValue);

    m_stopOnChange = new QAction(i18n("&Stop on Change"), this);
    connect(m_stopOnChange, &QAction::triggered, this, &VariableTree::stopOnChange);
}

Variable* VariableTree::selectedVariable() const
{
    if (selectionModel()->selectedRows().isEmpty()) return 0;
    auto item = selectionModel()->currentIndex().data(TreeModel::ItemRole).value<TreeItem*>();
    if (!item) return 0;
    return qobject_cast<Variable*>(item);
}

void VariableTree::contextMenuEvent(QContextMenuEvent* event)
{
    if (!selectedVariable()) return;

    // set up menu
    QMenu contextMenu(this->parentWidget());
    m_contextMenuTitle->setText(selectedVariable()->expression());
    contextMenu.addAction(m_contextMenuTitle);

    if(selectedVariable()->canSetFormat())
        contextMenu.addMenu(m_formatMenu);

    foreach(QAction* act, m_formatMenu->actions()) {
        if(act->data().toInt()==selectedVariable()->format())
            act->setChecked(true);
    }

    if (qobject_cast<Watches*>(selectedVariable()->parent())) {
        contextMenu.addAction(m_watchDelete);
    }

    contextMenu.addSeparator();
    contextMenu.addAction(m_copyVariableValue);
    contextMenu.addAction(m_stopOnChange);

    contextMenu.exec(event->globalPos());
}

void VariableTree::changeVariableFormat(int format)
{
    if (!selectedVariable()) return;
    selectedVariable()->setFormat(static_cast<Variable::format_t>(format));
}

void VariableTree::watchDelete()
{
    if (!selectedVariable()) return;
    if (!qobject_cast<Watches*>(selectedVariable()->parent())) return;
    selectedVariable()->die();
}

void VariableTree::copyVariableValue()
{
    if (!selectedVariable()) return;
    QApplication::clipboard()->setText(selectedVariable()->value());
}

void VariableTree::stopOnChange()
{
    if (!selectedVariable()) return;
    IDebugSession *session = ICore::self()->debugController()->currentSession();
    if (session && session->state() != IDebugSession::NotStartedState && session->state() != IDebugSession::EndedState) {
        session->variableController()->addWatchpoint(selectedVariable());
    }
}

#if 0
void VariableTree::contextMenuEvent(QContextMenuEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    if (!index.isValid())
        return;

    AbstractVariableItem* item = collection()->itemForIndex(index);

    if (RecentItem* recent = qobject_cast<RecentItem*>(item))
    {
        QMenu popup(this);
        popup.addTitle(i18n("Recent Expressions"));
        QAction* remove = popup.addAction(QIcon::fromTheme("editdelete"), i18n("Remove All"));
        QAction* reevaluate = popup.addAction(QIcon::fromTheme("reload"), i18n("Re-evaluate All"));

        if (controller()->stateIsOn(s_dbgNotStarted))
            reevaluate->setEnabled(false);

        QAction* res = popup.exec(QCursor::pos());

        if (res == remove)
        {
            collection()->deleteItem(recent);
        }
        else if (res == reevaluate)
        {
            foreach (AbstractVariableItem* item, recent->children())
            {
                if (VariableItem* variable = qobject_cast<VariableItem*>(item))
                    variable->updateValue();
            }
        }
    }
    else
    {
        activePopup_ = new QMenu(this);
        QMenu format(this);

        QAction* remember = 0;
        QAction* remove = 0;
        QAction* reevaluate = 0;
        QAction* watch = 0;

        QAction* natural = 0;
        QAction* hex = 0;
        QAction* decimal = 0;
        QAction* character = 0;
        QAction* binary = 0;

#define MAYBE_DISABLE(action) if (!var->isAlive()) action->setEnabled(false)

        VariableItem* var = qobject_cast<VariableItem*>(item);

        AbstractVariableItem* root = item->abstractRoot();

        RecentItem* recentRoot = qobject_cast<RecentItem*>(root);

        if (!recentRoot)
        {
            remember = activePopup_->addAction(QIcon::fromTheme("draw-freehand"), i18n("Remember Value"));
            MAYBE_DISABLE(remember);
        }

        if (!recentRoot) {
            watch = activePopup_->addAction(i18n("Watch Variable"));
            MAYBE_DISABLE(watch);
        }

        if (recentRoot) {
            reevaluate = activePopup_->addAction(QIcon::fromTheme("reload"), i18n("Reevaluate Expression"));
            MAYBE_DISABLE(reevaluate);
            remove = activePopup_->addAction(QIcon::fromTheme("editdelete"), i18n("Remove Expression"));
            remove->setShortcut(Qt::Key_Delete);
        }

        if (var)
        {
            toggleWatch_ = activePopup_->addAction( i18n("Data write breakpoint") );
            toggleWatch_->setCheckable(true);
            toggleWatch_->setEnabled(false);
        }

        /* This code can be executed when debugger is stopped,
           and we invoke popup menu on a var under "recent expressions"
           just to delete it. */
        if (var && var->isAlive() && !controller()->stateIsOn(s_dbgNotStarted)) {
            GDBCommand* cmd =  new GDBCommand(DataEvaluateExpression,
                                                QStringLiteral("&%1")
                                                .arg(var->gdbExpression()));
            cmd->setHandler(this, &VariableTree::handleAddressComputed, true /*handles error*/);
            cmd->setThread(var->thread());
            cmd->setFrame(var->frame());
            controller_->addCommand(cmd);
        }


        QAction* res = activePopup_->exec(event->globalPos());
        delete activePopup_;
        activePopup_ = 0;

        if (res == remember)
        {
            if (var)
            {
                ((VariableWidget*)parent())->
                    slotEvaluateExpression(var->gdbExpression());
            }
        }
        else if (res == watch)
        {
            if (var)
            {
                ((VariableWidget*)parent())->
                    slotAddWatchVariable(var->gdbExpression());
            }
        }
        else if (res == remove)
        {
            delete item;
        }
        else if (res == toggleWatch_)
        {
            if (var)
                emit toggleWatchpoint(var->gdbExpression());
        }
        else if (res == reevaluate)
        {
            if (var)
            {
                var->updateValue();
            }
        }

        event->accept();
    }
}


void VariableTree::updateCurrentFrame()
{
}

// **************************************************************************

void VariableTree::handleAddressComputed(const GDBMI::ResultRecord& r)
{
    if (r.reason == "error")
    {
        // Not lvalue, leave item disabled.
        return;
    }

    if (activePopup_)
    {
        toggleWatch_->setEnabled(true);

        //quint64 address = r["value"].literal().toULongLong(0, 16);
        /*if (breakpointWidget_->hasWatchpointForAddress(address))
        {
            toggleWatch_->setChecked(true);
        }*/
    }
}

VariableCollection * VariableTree::collection() const
{
    return controller_->variables();
}

GDBController * VariableTree::controller() const
{
    return controller_;
}

void VariableTree::showEvent(QShowEvent * event)
{
    Q_UNUSED(event)

    for (int i = 0; i < model()->columnCount(); ++i)
        resizeColumnToContents(i);
}
#endif

// **************************************************************************
// **************************************************************************
// **************************************************************************

}

