/* This file is part of KDevelop
    Copyright 2005 Roberto Raggi <roberto@kdevelop.org>
    Copyright 2007 Andreas Pakulat <apaku@gmx.de>
    Copyright 2009 Aleix Pol <aleixpol@kde.org>

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

#include "projecttreeview.h"


#include <QAction>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <KConfigGroup>
#include <KLocalizedString>

#include <project/projectmodel.h>
#include <language/duchain/duchainutils.h>
#include <interfaces/contextmenuextension.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/context.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/icore.h>
#include <interfaces/iselectioncontroller.h>
#include <interfaces/isession.h>
#include <project/interfaces/iprojectfilemanager.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include "projectmanagerviewplugin.h"
#include "projectmodelsaver.h"
#include "projectmodelitemdelegate.h"
#include "debug.h"
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchain.h>
#include <language/util/navigationtooltip.h>
#include <project/projectutils.h>
#include <widgetcolorizer.h>

using namespace KDevelop;

namespace {
const char settingsConfigGroup[] = "ProjectTreeView";

QList<ProjectFileItem*> fileItemsWithin(const QList<ProjectBaseItem*>& items)
{
    QList<ProjectFileItem*> fileItems;
    fileItems.reserve(items.size());
    foreach(ProjectBaseItem* item, items)
    {
        if (ProjectFileItem *file = item->file())
            fileItems.append(file);
        else if (item->folder())
            fileItems.append(fileItemsWithin(item->children()));
    }
    return fileItems;
}

QList<ProjectBaseItem*> topLevelItemsWithin(QList<ProjectBaseItem*> items)
{
    std::sort(items.begin(), items.end(), ProjectBaseItem::pathLessThan);
    Path lastFolder;
    for (int i = items.size() - 1; i >= 0; --i)
    {
        if (lastFolder.isParentOf(items[i]->path()))
            items.removeAt(i);
        else if (items[i]->folder())
            lastFolder = items[i]->path();
    }
    return items;
}

template<class T>
void filterDroppedItems(QList<T*> &items, ProjectBaseItem* dest)
{
    for (int i = items.size() - 1; i >= 0; --i)
    {
        //No drag and drop from and to same location
        if (items[i]->parent() == dest)
            items.removeAt(i);
        //No moving between projects (technically feasible if the projectmanager is the same though...)
        else if (items[i]->project() != dest->project())
            items.removeAt(i);
    }
}

//TODO test whether this could be replaced by projectbuildsetwidget.cpp::showContextMenu_appendActions
void popupContextMenu_appendActions(QMenu& menu, const QList<QAction*>& actions)
{
    menu.addActions(actions);
    menu.addSeparator();
}

}

ProjectTreeView::ProjectTreeView( QWidget *parent )
        : QTreeView( parent ), m_previousSelection ( nullptr )
{
    header()->hide();

    setEditTriggers( QAbstractItemView::EditKeyPressed );

    setContextMenuPolicy( Qt::CustomContextMenu );
    setSelectionMode( QAbstractItemView::ExtendedSelection );

    setIndentation(10);

    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setAutoScroll(true);
    setAutoExpandDelay(300);
    setItemDelegate(new ProjectModelItemDelegate(this));

    connect( this, &ProjectTreeView::customContextMenuRequested, this, &ProjectTreeView::popupContextMenu );
    connect( this, &ProjectTreeView::activated, this, &ProjectTreeView::slotActivated );

    connect( ICore::self(), &ICore::aboutToShutdown,
             this, &ProjectTreeView::aboutToShutdown);
    connect( ICore::self()->projectController(), &IProjectController::projectOpened,
             this, &ProjectTreeView::restoreState );
    connect( ICore::self()->projectController(), &IProjectController::projectClosed,
             this, &ProjectTreeView::projectClosed );
}

ProjectTreeView::~ProjectTreeView()
{
}

ProjectBaseItem* ProjectTreeView::itemAtPos(QPoint pos)
{
    return indexAt(pos).data(ProjectModel::ProjectItemRole).value<ProjectBaseItem*>();
}

void ProjectTreeView::dropEvent(QDropEvent* event)
{
    ProjectItemContext* selectionCtxt =
            static_cast<ProjectItemContext*>(KDevelop::ICore::self()->selectionController()->currentSelection());
    ProjectBaseItem* destItem = itemAtPos(event->pos());
    if (destItem && (dropIndicatorPosition() == AboveItem || dropIndicatorPosition() == BelowItem))
            destItem = destItem->parent();
    if (selectionCtxt && destItem)
    {
        if (ProjectFolderItem *folder = destItem->folder())
        {
            QMenu dropMenu(this);

            QString seq = QKeySequence( Qt::ShiftModifier ).toString();
            seq.chop(1); // chop superfluous '+'
            QAction* move = new QAction(i18n( "&Move Here" ) + '\t' + seq, &dropMenu);
            move->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));
            dropMenu.addAction(move);

            seq = QKeySequence( Qt::ControlModifier ).toString();
            seq.chop(1);
            QAction* copy = new QAction(i18n( "&Copy Here" ) + '\t' + seq, &dropMenu);
            copy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
            dropMenu.addAction(copy);

            dropMenu.addSeparator();

            QAction* cancel = new QAction(i18n( "C&ancel" ) + '\t' + QKeySequence( Qt::Key_Escape ).toString(), &dropMenu);
            cancel->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
            dropMenu.addAction(cancel);

            QAction *executedAction = nullptr;

            Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
            if (modifiers == Qt::ControlModifier) {
                executedAction = copy;
            } else if (modifiers == Qt::ShiftModifier) {
                executedAction = move;
            } else {
                executedAction = dropMenu.exec(this->mapToGlobal(event->pos()));
            }

            QList<ProjectBaseItem*> usefulItems = topLevelItemsWithin(selectionCtxt->items());
            filterDroppedItems(usefulItems, destItem);
            Path::List paths;
            foreach (ProjectBaseItem* i, usefulItems) {
                paths << i->path();
            }
            bool success = false;
            if (executedAction == copy) {
                success = destItem->project()->projectFileManager()->copyFilesAndFolders(paths, folder);
            } else if (executedAction == move) {
                success = destItem->project()->projectFileManager()->moveFilesAndFolders(usefulItems, folder);
            }

            if (success) {
                //expand target folder
                expand( mapFromItem(folder));

                //and select new items
                QItemSelection selection;
                foreach (const Path &path, paths) {
                    const Path targetPath(folder->path(), path.lastPathSegment());
                    foreach (ProjectBaseItem *item, folder->children()) {
                        if (item->path() == targetPath) {
                            QModelIndex indx = mapFromItem( item );
                            selection.append(QItemSelectionRange(indx, indx));
                            setCurrentIndex(indx);
                        }
                    }
                }
                selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
            }
        }
        else if (destItem->target() && destItem->project()->buildSystemManager())
        {
            QMenu dropMenu(this);

            QString seq = QKeySequence( Qt::ControlModifier ).toString();
            seq.chop(1);
            QAction* addToTarget = new QAction(i18n( "&Add to Target" ) + '\t' + seq, &dropMenu);
            addToTarget->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
            dropMenu.addAction(addToTarget);

            dropMenu.addSeparator();

            QAction* cancel = new QAction(i18n( "C&ancel" ) + '\t' + QKeySequence( Qt::Key_Escape ).toString(), &dropMenu);
            cancel->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
            dropMenu.addAction(cancel);

            QAction *executedAction = nullptr;

            Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
            if (modifiers == Qt::ControlModifier) {
                executedAction = addToTarget;
            } else {
                executedAction = dropMenu.exec(this->mapToGlobal(event->pos()));
            }
            if (executedAction == addToTarget) {
                QList<ProjectFileItem*> usefulItems = fileItemsWithin(selectionCtxt->items());
                filterDroppedItems(usefulItems, destItem);
                destItem->project()->buildSystemManager()->addFilesToTarget(usefulItems, destItem->target());
            }
        }
    }
    event->accept();
}

QModelIndex ProjectTreeView::mapFromSource(const QAbstractProxyModel* proxy, const QModelIndex& sourceIdx)
{
    const QAbstractItemModel* next = proxy->sourceModel();
    Q_ASSERT(next == sourceIdx.model() || qobject_cast<const QAbstractProxyModel*>(next));
    if(next == sourceIdx.model())
        return proxy->mapFromSource(sourceIdx);
    else {
        const QAbstractProxyModel* nextProxy = qobject_cast<const QAbstractProxyModel*>(next);
        QModelIndex idx = mapFromSource(nextProxy, sourceIdx);
        Q_ASSERT(idx.model() == nextProxy);
        return proxy->mapFromSource(idx);
    }
}

QModelIndex ProjectTreeView::mapFromItem(const ProjectBaseItem* item)
{
    QModelIndex ret = mapFromSource(qobject_cast<const QAbstractProxyModel*>(model()), item->index());
    Q_ASSERT(ret.model() == model());
    return ret;
}

void ProjectTreeView::slotActivated( const QModelIndex &index )
{
    if ( QApplication::keyboardModifiers() & Qt::CTRL || QApplication::keyboardModifiers() & Qt::SHIFT ) {
        // Do not open file when Ctrl or Shift is pressed; that's for selection
        return;
    }
    KDevelop::ProjectBaseItem *item = index.data(ProjectModel::ProjectItemRole).value<ProjectBaseItem*>();
    if ( item && item->file() )
    {
        emit activate( item->file()->path() );
    }
}

void ProjectTreeView::projectClosed(KDevelop::IProject* project)
{
    if ( project == m_previousSelection )
        m_previousSelection = nullptr;
}


QList<ProjectBaseItem*> ProjectTreeView::selectedProjects()
{
    QList<ProjectBaseItem*> itemlist;
    if ( selectionModel()->hasSelection() ) {
        QModelIndexList indexes = selectionModel()->selectedRows();
        for ( const QModelIndex& index: indexes ) {
            ProjectBaseItem* item = index.data( ProjectModel::ProjectItemRole ).value<ProjectBaseItem*>();
            if ( item ) {
                itemlist << item;
                m_previousSelection = item->project();
            }
        }
    }

    // add previous selection if nothing is selected right now
    if ( itemlist.isEmpty() && m_previousSelection ) {
        itemlist << m_previousSelection->projectItem();
    }

    return itemlist;
}

KDevelop::IProject* ProjectTreeView::getCurrentProject()
{
    auto itemList = selectedProjects();
    if ( !itemList.isEmpty() ) {
        return itemList.at( 0 )->project();
    }
    return nullptr;
}

void ProjectTreeView::popupContextMenu( const QPoint &pos )
{
    QList<ProjectBaseItem*> itemlist;
    if ( indexAt( pos ).isValid() ) {
        itemlist = selectedProjects();
    }
    QMenu menu( this );

    KDevelop::ProjectItemContextImpl context(itemlist);
    QList<ContextMenuExtension> extensions = ICore::self()->pluginController()->queryPluginsForContextMenuExtensions( &context );

    QList<QAction*> buildActions;
    QList<QAction*> vcsActions;
    QList<QAction*> analyzeActions;
    QList<QAction*> extActions;
    QList<QAction*> projectActions;
    QList<QAction*> fileActions;
    QList<QAction*> runActions;
    foreach( const ContextMenuExtension& ext, extensions )
    {
        buildActions += ext.actions(ContextMenuExtension::BuildGroup);
        fileActions += ext.actions(ContextMenuExtension::FileGroup);
        projectActions += ext.actions(ContextMenuExtension::ProjectGroup);
        vcsActions += ext.actions(ContextMenuExtension::VcsGroup);
        analyzeActions += ext.actions(ContextMenuExtension::AnalyzeGroup);
        extActions += ext.actions(ContextMenuExtension::ExtensionGroup);
        runActions += ext.actions(ContextMenuExtension::RunGroup);
    }

    if ( analyzeActions.count() )
    {
        QMenu* analyzeMenu = new QMenu( i18n("Analyze With"), this );
        analyzeMenu->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));
        foreach( QAction* act, analyzeActions )
        {
            analyzeMenu->addAction( act );
        }
        analyzeActions = {analyzeMenu->menuAction()};
    }

    popupContextMenu_appendActions(menu, buildActions);
    popupContextMenu_appendActions(menu, runActions );
    popupContextMenu_appendActions(menu, fileActions);
    popupContextMenu_appendActions(menu, vcsActions);
    popupContextMenu_appendActions(menu, analyzeActions);
    popupContextMenu_appendActions(menu, extActions);

    if ( !itemlist.isEmpty() && itemlist.size() == 1 && itemlist[0]->folder() && !itemlist[0]->folder()->parent() )
    {
        QAction* projectConfig = new QAction(i18n("Open Configuration..."), this);
        projectConfig->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
        connect( projectConfig, &QAction::triggered, this, &ProjectTreeView::openProjectConfig );
        projectActions << projectConfig;
    }
    popupContextMenu_appendActions(menu, projectActions);

    if(!itemlist.isEmpty())
        KDevelop::populateParentItemsMenu(itemlist.front(), &menu);

    if ( !menu.isEmpty() ) {
        menu.exec( mapToGlobal( pos ) );
    }
}

void ProjectTreeView::openProjectConfig()
{
    if ( IProject* project = getCurrentProject() ) {
        IProjectController* ip = ICore::self()->projectController();
        ip->configureProject( project );
    }
}

void ProjectTreeView::saveState( IProject* project )
{
    // nullptr won't create a usable saved state, so spare the effort
    if ( !project ) {
        return;
    }

    KConfigGroup configGroup( ICore::self()->activeSession()->config(),
                              QString( settingsConfigGroup ).append( project->name() ) );

    ProjectModelSaver saver;
    saver.setProject( project );
    saver.setView( this );
    saver.saveState( configGroup );
}

void ProjectTreeView::restoreState( IProject* project )
{
    if ( !project ) {
        return;
    }

    KConfigGroup configGroup( ICore::self()->activeSession()->config(),
                              QString( settingsConfigGroup ).append( project->name() ) );
    ProjectModelSaver saver;
    saver.setProject( project );
    saver.setView( this );
    saver.restoreState( configGroup );
}

void ProjectTreeView::rowsInserted( const QModelIndex& parent, int start, int end )
{
    QTreeView::rowsInserted( parent, start, end );

    // automatically select row if there is only one
    if ( model()->rowCount() == 1 ) {
        selectionModel()->select( model()->index( 0, 0 ), QItemSelectionModel::Select );
    }

    if ( !parent.model() ) {
        for ( const auto& project: selectedProjects() ) {
            restoreState( project->project() );
        }
    }
}

void ProjectTreeView::rowsAboutToBeRemoved( const QModelIndex& parent, int start, int end )
{
    // automatically select row if there is only one
    if ( model()->rowCount() == 1 ) {
        selectionModel()->select( model()->index( 0, 0 ), QItemSelectionModel::Select );
    }

    if ( !parent.model() ) {
        for ( const auto& project: selectedProjects() ) {
            saveState( project->project() );
        }
    }

    QTreeView::rowsAboutToBeRemoved( parent, start, end );
}

void ProjectTreeView::aboutToShutdown()
{
    // save all projects, not just the selected ones
    const auto projects = ICore::self()->projectController()->projects();
    for ( const auto& project: projects ) {
        saveState( project );
    }
}

bool ProjectTreeView::event(QEvent* event)
{
    if(event->type()==QEvent::ToolTip)
    {
        QPoint p = mapFromGlobal(QCursor::pos());
        QModelIndex idxView = indexAt(p);

        ProjectBaseItem* it = idxView.data(ProjectModel::ProjectItemRole).value<ProjectBaseItem*>();
        QModelIndex idx;
        if(it)
            idx = it->index();

        if((m_idx!=idx || !m_tooltip) && it && it->file())
        {
            m_idx=idx;
            ProjectFileItem* file=it->file();
            KDevelop::DUChainReadLocker lock(KDevelop::DUChain::lock());
            TopDUContext* top= DUChainUtils::standardContextForUrl(file->path().toUrl());

            if(m_tooltip)
                m_tooltip->close();

            if(top)
            {
                QWidget* navigationWidget = top->createNavigationWidget();
                if( navigationWidget )
                {
                    m_tooltip = new KDevelop::NavigationToolTip(this, mapToGlobal(p) + QPoint(40, 0), navigationWidget);
                    m_tooltip->resize( navigationWidget->sizeHint() + QSize(10, 10) );
                    qCDebug(PLUGIN_PROJECTMANAGERVIEW) << "tooltip size" << m_tooltip->size();
                    ActiveToolTip::showToolTip(m_tooltip);
                    return true;
                }
            }
        }
    }

    return QAbstractItemView::event(event);
}

void ProjectTreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return && currentIndex().isValid() && state()!=QAbstractItemView::EditingState)
    {
        event->accept();
        slotActivated(currentIndex());
    }
    else
        QTreeView::keyPressEvent(event);
}

void ProjectTreeView::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    if (WidgetColorizer::colorizeByProject()) {
        const auto projectPath = index.data(ProjectModel::ProjectRole).value<IProject *>()->path();
        const QColor color = WidgetColorizer::colorForId(qHash(projectPath), palette(), true);
        WidgetColorizer::drawBranches(this, painter, rect, index, color);
    }

    QTreeView::drawBranches(painter, rect, index);
}
