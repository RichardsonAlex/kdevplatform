/*
 * This file is part of KDevelop
 * Copyright 2010 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#include "vcschangesview.h"
#include <interfaces/icore.h>
#include <interfaces/context.h>
#include <interfaces/iproject.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/contextmenuextension.h>
#include <vcs/vcsstatusinfo.h>
#include <project/projectchangesmodel.h>
#include <project/projectmodel.h>
#include <project/projectutils.h>

#include <QIcon>
#include <QMenu>
#include <QPointer>

#include <KActionCollection>
#include <KLocalizedString>

#include "vcschangesviewplugin.h"

using namespace KDevelop;

VcsChangesView::VcsChangesView(VcsProjectIntegrationPlugin* plugin, QWidget* parent)
    : QTreeView(parent)
{
    setRootIsDecorated(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(ContiguousSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setTextElideMode(Qt::ElideLeft);
    setWordWrap(true);
    setWindowIcon(QIcon::fromTheme(QStringLiteral("exchange-positions"), windowIcon()));
    
    connect(this, &VcsChangesView::customContextMenuRequested, this, &VcsChangesView::popupContextMenu);
    
    foreach(QAction* action, plugin->actionCollection()->actions())
        addAction(action);
    
    QAction* action = plugin->actionCollection()->action(QStringLiteral("locate_document"));
    connect(action, &QAction::triggered, this, &VcsChangesView::selectCurrentDocument);
    connect(this, &VcsChangesView::doubleClicked, this, &VcsChangesView::openSelected);
}

static void appendActions(QMenu* menu, const QList<QAction*>& actions)
{
    menu->addSeparator();
    menu->addActions(actions);
}

void VcsChangesView::popupContextMenu( const QPoint &pos )
{
    QList<QUrl> urls;
    QList<IProject*> projects;
    QModelIndexList selectionIdxs = selectedIndexes();
    if(selectionIdxs.isEmpty())
        return;
    
    foreach(const QModelIndex& idx, selectionIdxs) {
        if(idx.column()==0) {
            if(idx.parent().isValid())
                urls += idx.data(KDevelop::VcsFileChangesModel::VcsStatusInfoRole).value<VcsStatusInfo>().url();
            else {
                IProject* project = ICore::self()->projectController()->findProjectByName(idx.data(ProjectChangesModel::ProjectNameRole).toString());
                if (project) {
                    projects += project;
                } else {
                    qWarning() << "Couldn't find a project for project: " << idx.data(ProjectChangesModel::ProjectNameRole).toString();
                }
            }
        }
    }

    QPointer<QMenu> menu = new QMenu;
    QAction* refreshAction = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"));
    QList<ContextMenuExtension> extensions;
    if(!urls.isEmpty()) {
        KDevelop::FileContext context(urls);
        extensions = ICore::self()->pluginController()->queryPluginsForContextMenuExtensions( &context );
    } else {
        QList<ProjectBaseItem*> items;
        foreach(IProject* p, projects)
            items += p->projectItem();
        
        KDevelop::ProjectItemContextImpl context(items);
        extensions = ICore::self()->pluginController()->queryPluginsForContextMenuExtensions( &context );
        
    }

    QList<QAction*> buildActions;
    QList<QAction*> vcsActions;
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
        extActions += ext.actions(ContextMenuExtension::ExtensionGroup);
        runActions += ext.actions(ContextMenuExtension::RunGroup);
    }

    appendActions(menu, buildActions);
    appendActions(menu, runActions );
    appendActions(menu, fileActions);
    appendActions(menu, vcsActions);
    appendActions(menu, extActions);
    appendActions(menu, projectActions);

    if ( !menu->isEmpty() ) {
        QAction* res = menu->exec( mapToGlobal( pos ) );
        if(res == refreshAction) {
            if(!urls.isEmpty())
                emit reload(urls);
            else
                emit reload(projects);
        }
    }
    delete menu;
}

void VcsChangesView::selectCurrentDocument()
{
    IDocument* doc = ICore::self()->documentController()->activeDocument();
    if(!doc)
        return;
    
    QUrl url = doc->url();
    IProject* p = ICore::self()->projectController()->findProjectForUrl(url);
    QModelIndex idx = (p ?
        model()->match(model()->index(0, 0), ProjectChangesModel::UrlRole,
                       url, 1, Qt::MatchExactly).value(0) :
        QModelIndex());
    if(idx.isValid()) {
        expand(idx.parent());
        setCurrentIndex(idx);
    } else
        collapseAll();
}

void VcsChangesView::setModel(QAbstractItemModel* model)
{
    connect(model, &QAbstractItemModel::rowsInserted, this, &VcsChangesView::expand);
    QTreeView::setModel(model);
}

void VcsChangesView::openSelected(const QModelIndex& index)
{
    if(!index.parent().isValid()) //then it's a project
        return;
    QModelIndex idx = index.sibling(index.row(), 1);
    VcsStatusInfo info = idx.data(ProjectChangesModel::VcsStatusInfoRole).value<VcsStatusInfo>();
    QUrl url = info.url();
    
    ICore::self()->documentController()->openDocument(url);
}
