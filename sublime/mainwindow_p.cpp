/***************************************************************************
 *   Copyright 2006-2009 Alexander Dymo  <adymo@kdevelop.org>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/
#include "mainwindow_p.h"

#include <QLayout>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>

#include <KActionMenu>
#include <KActionCollection>
#include <KLocalizedString>

#include "area.h"
#include "view.h"
#include "areaindex.h"
#include "document.h"
#include "container.h"
#include "controller.h"
#include "mainwindow.h"
#include "idealcontroller.h"
#include "holdupdates.h"
#include "idealbuttonbarwidget.h"
#include "sublimedebug.h"

class IdealToolBar : public QToolBar
{
    Q_OBJECT
    public:
        explicit IdealToolBar(const QString& title, bool hideWhenEmpty, Sublime::IdealButtonBarWidget* buttons, QMainWindow* parent)
            : QToolBar(title, parent)
            , m_timer(nullptr)
            , m_buttons(buttons)
            , m_hideWhenEmpty(hideWhenEmpty)
            , m_requestedVisibility(true)
        {
            setMovable(false);
            setFloatable(false);
            setObjectName(title);
            layout()->setMargin(0);

            addWidget(m_buttons);

            if (m_hideWhenEmpty) {
                m_timer = new QTimer(this);
                m_timer->setInterval(100);
                m_timer->setSingleShot(true);
                connect(m_timer, &QTimer::timeout, this, &IdealToolBar::refresh);
                connect(this, &IdealToolBar::visibilityChanged, m_timer, static_cast<void(QTimer::*)()>(&QTimer::start));
                connect(m_buttons, &Sublime::IdealButtonBarWidget::emptyChanged, m_timer, static_cast<void(QTimer::*)()>(&QTimer::start));
            }
        }

        void setVisible(bool visible) override
        {
            QToolBar::setVisible(visible);

            m_requestedVisibility = visible;

            if (m_hideWhenEmpty && visible) {
                m_timer->start();
            }
        }

    private slots:
        void refresh()
        {
            setVisible(m_requestedVisibility && !m_buttons->isEmpty());
        }

    private:
        QTimer* m_timer;
        Sublime::IdealButtonBarWidget* m_buttons;
        const bool m_hideWhenEmpty;
        bool m_requestedVisibility;
};

namespace Sublime {

MainWindowPrivate::MainWindowPrivate(MainWindow *w, Controller* controller)
:controller(controller), area(0), activeView(0), activeToolView(0), bgCentralWidget(0),
 ignoreDockShown(false), autoAreaSettingsSave(false), m_mainWindow(w)
{
    KActionCollection *ac = m_mainWindow->actionCollection();

    m_concentrationModeAction = new QAction(i18n("Concentration Mode"), this);
    m_concentrationModeAction->setIcon(QIcon::fromTheme(QStringLiteral("page-zoom")));
    m_concentrationModeAction->setToolTip(i18n("Removes most of the controls so you can focus on what matters."));
    m_concentrationModeAction->setCheckable(true);
    m_concentrationModeAction->setChecked(false);
    ac->setDefaultShortcut(m_concentrationModeAction, Qt::META | Qt::Key_C);
    connect(m_concentrationModeAction, &QAction::toggled, this, &MainWindowPrivate::restoreConcentrationMode);
    ac->addAction(QStringLiteral("toggle_concentration_mode"), m_concentrationModeAction);

    QAction* action = new QAction(i18n("Show Left Dock"), this);
    action->setCheckable(true);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_Left);
    connect(action, &QAction::toggled, this, &MainWindowPrivate::showLeftDock);

    ac->addAction(QStringLiteral("show_left_dock"), action);

    action = new QAction(i18n("Show Right Dock"), this);
    action->setCheckable(true);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_Right);
    connect(action, &QAction::toggled, this, &MainWindowPrivate::showRightDock);
    ac->addAction(QStringLiteral("show_right_dock"), action);

    action = new QAction(i18n("Show Bottom Dock"), this);
    action->setCheckable(true);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_Down);
    connect(action, &QAction::toggled, this, &MainWindowPrivate::showBottomDock);
    ac->addAction(QStringLiteral("show_bottom_dock"), action);

    action = new QAction(i18nc("@action", "Focus Editor"), this);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_E);
    connect(action, &QAction::triggered, this, &MainWindowPrivate::focusEditor);
    ac->addAction(QStringLiteral("focus_editor"), action);

    action = new QAction(i18n("Hide/Restore Docks"), this);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_Up);
    connect(action, &QAction::triggered, this, &MainWindowPrivate::toggleDocksShown);
    ac->addAction(QStringLiteral("hide_all_docks"), action);

    action = new QAction(i18n("Next Tool View"), this);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_N);
    action->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    connect(action, &QAction::triggered, this, &MainWindowPrivate::selectNextDock);
    ac->addAction(QStringLiteral("select_next_dock"), action);

    action = new QAction(i18n("Previous Tool View"), this);
    ac->setDefaultShortcut(action, Qt::META | Qt::CTRL | Qt::Key_P);
    action->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    connect(action, &QAction::triggered, this, &MainWindowPrivate::selectPreviousDock);
    ac->addAction(QStringLiteral("select_previous_dock"), action);

    action = new KActionMenu(i18n("Tool Views"), this);
    ac->addAction(QStringLiteral("docks_submenu"), action);

    idealController = new IdealController(m_mainWindow);

    m_leftToolBar = new IdealToolBar(i18n("Left Button Bar"), true, idealController->leftBarWidget, m_mainWindow);
    m_mainWindow->addToolBar(Qt::LeftToolBarArea, m_leftToolBar);

    m_rightToolBar = new IdealToolBar(i18n("Right Button Bar"), true, idealController->rightBarWidget, m_mainWindow);
    m_mainWindow->addToolBar(Qt::RightToolBarArea, m_rightToolBar);

    m_bottomToolBar = new IdealToolBar(i18n("Bottom Button Bar"), false, idealController->bottomBarWidget, m_mainWindow);
    m_mainWindow->addToolBar(Qt::BottomToolBarArea, m_bottomToolBar);

    // adymo: intentionally do not add a toolbar for top buttonbar
    // this doesn't work well with toolbars added via xmlgui

    centralWidget = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    centralWidget->setLayout(layout);
    layout->setMargin(0);
    splitterCentralWidget = new QSplitter(centralWidget);
    layout->addWidget(splitterCentralWidget);
    m_mainWindow->setCentralWidget(centralWidget);

    connect(idealController,
            &IdealController::dockShown,
            this,
            &MainWindowPrivate::slotDockShown);

    connect(idealController,
            &IdealController::widgetResized,
            this,
            &MainWindowPrivate::widgetResized);

   connect(idealController, &IdealController::dockBarContextMenuRequested,
            m_mainWindow, &MainWindow::dockBarContextMenuRequested);
}


MainWindowPrivate::~MainWindowPrivate()
{
    delete m_leftTabbarCornerWidget.data();
    m_leftTabbarCornerWidget.clear();
}

void MainWindowPrivate::disableConcentrationMode()
{
    m_concentrationModeAction->setChecked(false);
    restoreConcentrationMode();
}

void MainWindowPrivate::restoreConcentrationMode()
{
    const bool concentrationModeOn = m_concentrationModeAction->isChecked();
    QWidget* cornerWidget = nullptr;
    if (m_concentrateToolBar) {
        QLayout* l = m_concentrateToolBar->layout();
        QLayoutItem* li = l->takeAt(1); //ensure the cornerWidget isn't destroyed with the toolbar
        if (li) {
            cornerWidget = li->widget();
            delete li;
        }

        m_concentrateToolBar->deleteLater();
    }

    m_mainWindow->menuBar()->setVisible(!concentrationModeOn);
    m_bottomToolBar->setVisible(!concentrationModeOn);
    m_leftToolBar->setVisible(!concentrationModeOn);
    m_rightToolBar->setVisible(!concentrationModeOn);

    if (concentrationModeOn) {
        m_concentrateToolBar = new QToolBar(m_mainWindow);
        m_concentrateToolBar->setObjectName(QStringLiteral("concentrateToolBar"));
        m_concentrateToolBar->addAction(m_concentrationModeAction);
        QWidgetAction *action = new QWidgetAction(this);

        action->setDefaultWidget(m_mainWindow->menuBar()->cornerWidget(Qt::TopRightCorner));
        m_concentrateToolBar->addAction(action);
        m_concentrateToolBar->setMovable(false);

        m_mainWindow->addToolBar(Qt::TopToolBarArea, m_concentrateToolBar);
        m_mainWindow->menuBar()->setCornerWidget(0, Qt::TopRightCorner);
    } else if (cornerWidget) {
        m_mainWindow->menuBar()->setCornerWidget(cornerWidget, Qt::TopRightCorner);
        cornerWidget->show();
    }

    if (concentrationModeOn) {
        m_mainWindow->installEventFilter(this);
    } else {
        m_mainWindow->removeEventFilter(this);
    }
}

bool MainWindowPrivate::eventFilter(QObject* obj, QEvent* event)
{
    Q_ASSERT(m_mainWindow == obj);
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        Qt::KeyboardModifiers modifiers = static_cast<QKeyEvent *>(event)->modifiers();

        m_mainWindow->menuBar()->setVisible(modifiers == Qt::AltModifier && event->type() == QEvent::KeyPress);
    }

    return false;
}

void MainWindowPrivate::showLeftDock(bool b)
{
    idealController->showLeftDock(b);
}

void MainWindowPrivate::showBottomDock(bool b)
{
    idealController->showBottomDock(b);
}

void MainWindowPrivate::showRightDock(bool b)
{
    idealController->showRightDock(b);
}

void MainWindowPrivate::setBackgroundCentralWidget(QWidget* w)
{
    delete bgCentralWidget;
    QLayout* l=m_mainWindow->centralWidget()->layout();
    l->addWidget(w);
    bgCentralWidget=w;
    setBackgroundVisible(area->views().isEmpty());
}

void MainWindowPrivate::setBackgroundVisible(bool v)
{
    if(!bgCentralWidget)
        return;

    bgCentralWidget->setVisible(v);
    splitterCentralWidget->setVisible(!v);
}

void MainWindowPrivate::focusEditor()
{
    if (View* view = m_mainWindow->activeView())
        if (view->hasWidget())
            view->widget()->setFocus(Qt::ShortcutFocusReason);
}

void MainWindowPrivate::toggleDocksShown()
{
    idealController->toggleDocksShown();
}

void MainWindowPrivate::selectNextDock()
{
    idealController->goPrevNextDock(IdealController::NextDock);
}

void MainWindowPrivate::selectPreviousDock()
{
    idealController->goPrevNextDock(IdealController::PrevDock);
}

Area::WalkerMode MainWindowPrivate::IdealToolViewCreator::operator() (View *view, Sublime::Position position)
{
    if (!d->docks.contains(view))
    {
        d->docks << view;

        //add view
        d->idealController->addView(d->positionToDockArea(position), view);
    }
    return Area::ContinueWalker;
}

Area::WalkerMode MainWindowPrivate::ViewCreator::operator() (AreaIndex *index)
{
    QSplitter *splitter = d->m_indexSplitters.value(index);
    if (!splitter)
    {
        //no splitter - we shall create it and populate with views
        if (!index->parent())
        {
            qCDebug(SUBLIME) << "reconstructing root area";
            //this is root area
            splitter = d->splitterCentralWidget;
            d->m_indexSplitters[index] = splitter;
            d->centralWidget->layout()->addWidget(splitter);
        }
        else
        {
            if (!d->m_indexSplitters.value(index->parent())) {
                // can happen in working set code, as that adds a view to a child index first
                // hence, recursively reconstruct the parent indizes first
                operator()(index->parent());
            }
            QSplitter *parent = d->m_indexSplitters.value(index->parent());
            splitter = new QSplitter(parent);
            d->m_indexSplitters[index] = splitter;

            if(index == index->parent()->first())
                parent->insertWidget(0, splitter);
            else
                parent->addWidget(splitter);
        }
        Q_ASSERT(splitter);
    }

    if (index->isSplit()) //this is a visible splitter
        splitter->setOrientation(index->orientation());
    else
    {
        Container *container = 0;

        while(splitter->count() && qobject_cast<QSplitter*>(splitter->widget(0)))
        {
            // After unsplitting, we might have to remove old splitters
            QWidget* widget = splitter->widget(0);
            qCDebug(SUBLIME) << "deleting" << widget;
            widget->setParent(0);
            delete widget;
        }

        if (!splitter->widget(0))
        {
            //we need to create view container
            container = new Container(splitter);
            connect(container, &Container::activateView,
                    d->m_mainWindow, &MainWindow::activateViewAndFocus);
            connect(container, &Container::tabDoubleClicked,
                    d->m_mainWindow, &MainWindow::tabDoubleClicked);
            connect(container, &Container::tabContextMenuRequested,
                    d->m_mainWindow, &MainWindow::tabContextMenuRequested);
            connect(container, &Container::tabToolTipRequested,
                    d->m_mainWindow, &MainWindow::tabToolTipRequested);
            connect(container, static_cast<void(Container::*)(QWidget*)>(&Container::requestClose),
                    d, &MainWindowPrivate::widgetCloseRequest, Qt::QueuedConnection);
            connect(container, &Container::newTabRequested,
                    d->m_mainWindow, &MainWindow::newTabRequested);
            splitter->addWidget(container);
        }
        else
            container = qobject_cast<Container*>(splitter->widget(0));
        container->show();

        int position = 0;
        bool hadActiveView = false;
        Sublime::View* activeView = d->activeView;

        foreach (View *view, index->views())
        {
            QWidget *widget = view->widget(container);

            if (widget)
            {
                if(!container->hasWidget(widget))
                {
                    container->addWidget(view, position);
                    d->viewContainers[view] = container;
                    d->widgetToView[widget] = view;
                }
                if(activeView == view)
                {
                    hadActiveView = true;
                    container->setCurrentWidget(widget);
                }else if(topViews.contains(view) && !hadActiveView)
                    container->setCurrentWidget(widget);
            }
            position++;
        }
    }
    return Area::ContinueWalker;
}

void MainWindowPrivate::reconstructViews(QList<View*> topViews)
{
    ViewCreator viewCreator(this, topViews);
    area->walkViews(viewCreator, area->rootIndex());
    setBackgroundVisible(area->views().isEmpty());
}

void MainWindowPrivate::reconstruct()
{
    if(m_leftTabbarCornerWidget) {
        m_leftTabbarCornerWidget->hide();
        m_leftTabbarCornerWidget->setParent(0);
    }

    idealController->setWidthForArea(Qt::LeftDockWidgetArea, area->thickness(Sublime::Left));
    idealController->setWidthForArea(Qt::BottomDockWidgetArea, area->thickness(Sublime::Bottom));
    idealController->setWidthForArea(Qt::RightDockWidgetArea, area->thickness(Sublime::Right));

    IdealToolViewCreator toolViewCreator(this);
    area->walkToolViews(toolViewCreator, Sublime::AllPositions);

    reconstructViews();

    m_mainWindow->blockSignals(true);

    qCDebug(SUBLIME) << "RECONSTRUCT" << area << area->shownToolViews(Sublime::Left);
    foreach (View *view, area->toolViews())
    {
        QString id = view->document()->documentSpecifier();
        if (!id.isEmpty())
        {
            Sublime::Position pos = area->toolViewPosition(view);
            if (area->shownToolViews(pos).contains(id))
                idealController->raiseView(view, IdealController::GroupWithOtherViews);
        }
    }
    m_mainWindow->blockSignals(false);

    setTabBarLeftCornerWidget(m_leftTabbarCornerWidget.data());
}

void MainWindowPrivate::clearArea()
{
    if(m_leftTabbarCornerWidget)
        m_leftTabbarCornerWidget->setParent(0);

    //reparent toolview widgets to 0 to prevent their deletion together with dockwidgets
    foreach (View *view, area->toolViews())
    {
        // FIXME should we really delete here??
        bool nonDestructive = true;
        idealController->removeView(view, nonDestructive);

        if (view->hasWidget())
            view->widget()->setParent(0);
    }

    docks.clear();

    //reparent all view widgets to 0 to prevent their deletion together with central
    //widget. this reparenting is necessary when switching areas inside the same mainwindow
    foreach (View *view, area->views())
    {
        if (view->hasWidget())
            view->widget()->setParent(0);
    }
    cleanCentralWidget();
    m_mainWindow->setActiveView(0);
    m_indexSplitters.clear();
    area = 0;
    viewContainers.clear();

    setTabBarLeftCornerWidget(m_leftTabbarCornerWidget.data());
}

void MainWindowPrivate::cleanCentralWidget()
{
    while(splitterCentralWidget->count())
        delete splitterCentralWidget->widget(0);

    setBackgroundVisible(true);
}

struct ShownToolViewFinder {
    ShownToolViewFinder() {}
    Area::WalkerMode operator()(View *v, Sublime::Position /*position*/)
    {
        if (v->hasWidget() && v->widget()->isVisible())
            views << v;
        return Area::ContinueWalker;
    }
    QList<View *> views;
};

void MainWindowPrivate::slotDockShown(Sublime::View* /*view*/, Sublime::Position pos, bool /*shown*/)
{
    if (ignoreDockShown)
        return;

    ShownToolViewFinder finder;
    m_mainWindow->area()->walkToolViews(finder, pos);

    QStringList ids;
    foreach (View *v, finder.views) {
        ids << v->document()->documentSpecifier();
    }
    area->setShownToolViews(pos, ids);
}

void MainWindowPrivate::viewRemovedInternal(AreaIndex* index, View* view)
{
    Q_UNUSED(index);
    Q_UNUSED(view);
    setBackgroundVisible(area->views().isEmpty());
}

void MainWindowPrivate::viewAdded(Sublime::AreaIndex *index, Sublime::View *view)
{
    if(m_leftTabbarCornerWidget) {
        m_leftTabbarCornerWidget->hide();
        m_leftTabbarCornerWidget->setParent(0);
    }

    // Remove container objects in the hierarchy from the parents,
    // because they are not needed anymore, and might lead to broken splitter hierarchy and crashes.
    for(Sublime::AreaIndex* current = index; current; current = current->parent())
    {
        QSplitter *splitter = m_indexSplitters[current];
        if (current->isSplit() && splitter)
        {
            // Also update the orientation
            splitter->setOrientation(current->orientation());

            for(int w = 0; w < splitter->count(); ++w)
            {
                Container *container = qobject_cast<Sublime::Container*>(splitter->widget(w));
                //we need to remove extra container before reconstruction
                //first reparent widgets in container so that they are not deleted
                if(container)
                {
                    while (container->count())
                    {
                        container->widget(0)->setParent(0);
                    }
                    //and then delete the container
                    delete container;
                }
            }
        }
    }

    ViewCreator viewCreator(this);
    area->walkViews(viewCreator, index);
    emit m_mainWindow->viewAdded( view );

    setTabBarLeftCornerWidget(m_leftTabbarCornerWidget.data());

    setBackgroundVisible(false);
}

void Sublime::MainWindowPrivate::raiseToolView(Sublime::View * view)
{
    idealController->raiseView(view);
}

void MainWindowPrivate::aboutToRemoveView(Sublime::AreaIndex *index, Sublime::View *view)
{
    QSplitter *splitter = m_indexSplitters[index];
    if (!splitter)
        return;

    qCDebug(SUBLIME) << "index " << index << " root " << area->rootIndex();
    qCDebug(SUBLIME) << "splitter " << splitter << " container " << splitter->widget(0);
    qCDebug(SUBLIME) << "structure: " << index->print() << " whole structure: " << area->rootIndex()->print();
    //find the container for the view and remove the widget
    Container *container = qobject_cast<Container*>(splitter->widget(0));
    if (!container) {
        qWarning() << "Splitter does not have a left widget!";
        return;
    }

    emit m_mainWindow->aboutToRemoveView( view );

    if (view->widget())
        widgetToView.remove(view->widget());
    viewContainers.remove(view);

    const bool wasActive = m_mainWindow->activeView() == view;
    if (container->count() > 1)
    {
        //container is not empty or this is a root index
        //just remove a widget
        if( view->widget() ) {
            container->removeWidget(view->widget());
            view->widget()->setParent(0);
            //activate what is visible currently in the container if the removed view was active
            if (wasActive)
                return m_mainWindow->setActiveView(container->viewForWidget(container->currentWidget()));
        }
    }
    else
    {
        if(m_leftTabbarCornerWidget) {
            m_leftTabbarCornerWidget->hide();
            m_leftTabbarCornerWidget->setParent(0);
        }

        // We've about to remove the last view of this container.  It will
        // be empty, so have to delete it, as well.

        // If we have a container, then it should be the only child of
        // the splitter.
        Q_ASSERT(splitter->count() == 1);
        container->removeWidget(view->widget());

        if (view->widget())
            view->widget()->setParent(0);
        else
            qWarning() << "View does not have a widget!";

        Q_ASSERT(container->count() == 0);
        // We can be called from signal handler of container
        // (which is tab widget), so defer deleting it.
        container->deleteLater();
        container->setParent(0);

        /* If we're not at the top level, we get to collapse split views.  */
        if (index->parent())
        {
            /* The splitter used to have container as the only child, now it's
               time to get rid of it.  Make sure deleting splitter does not
               delete container -- per above comment, we'll delete it later.  */
            container->setParent(0);
            m_indexSplitters.remove(index);
            delete splitter;

            AreaIndex *parent = index->parent();
            QSplitter *parentSplitter = m_indexSplitters[parent];

            AreaIndex *sibling = parent->first() == index ? parent->second() : parent->first();
            QSplitter *siblingSplitter = m_indexSplitters[sibling];

            if(siblingSplitter)
            {
                HoldUpdates du(parentSplitter);
                //save sizes and orientation of the sibling splitter
                parentSplitter->setOrientation(siblingSplitter->orientation());
                QList<int> sizes = siblingSplitter->sizes();

                /* Parent has two children -- 'index' that we've deleted and
                'sibling'.  We move all children of 'sibling' into parent,
                and delete 'sibling'.  sibling either contains a single
                Container instance, or a bunch of further QSplitters.  */
                while (siblingSplitter->count() > 0)
                {
                    //reparent contents into parent splitter
                    QWidget *siblingWidget = siblingSplitter->widget(0);
                    siblingWidget->setParent(parentSplitter);
                    parentSplitter->addWidget(siblingWidget);
                }

                m_indexSplitters.remove(sibling);
                delete siblingSplitter;
                parentSplitter->setSizes(sizes);
            }

            qCDebug(SUBLIME) << "after deleation " << parent << " has "
                         << parentSplitter->count() << " elements";


            //find the container somewhere to activate
            Container *containerToActivate = parentSplitter->findChild<Sublime::Container*>();
            //activate the current view there
            if (containerToActivate) {
                m_mainWindow->setActiveView(containerToActivate->viewForWidget(containerToActivate->currentWidget()));
                setTabBarLeftCornerWidget(m_leftTabbarCornerWidget.data());
                return;
            }
        }
    }

    setTabBarLeftCornerWidget(m_leftTabbarCornerWidget.data());
    if ( wasActive ) {
        m_mainWindow->setActiveView(0L);
    }
}

void MainWindowPrivate::toolViewAdded(Sublime::View* /*toolView*/, Sublime::Position position)
{
    IdealToolViewCreator toolViewCreator(this);
    area->walkToolViews(toolViewCreator, position);
}

void MainWindowPrivate::aboutToRemoveToolView(Sublime::View *toolView, Sublime::Position /*position*/)
{
    if (!docks.contains(toolView))
        return;

    idealController->removeView(toolView);
    // TODO are Views unique?
    docks.removeAll(toolView);
}

void MainWindowPrivate::toolViewMoved(
    Sublime::View *toolView, Sublime::Position position)
{
    if (!docks.contains(toolView))
        return;

    idealController->moveView(toolView, positionToDockArea(position));
}

Qt::DockWidgetArea MainWindowPrivate::positionToDockArea(Position position)
{
    switch (position)
    {
        case Sublime::Left: return Qt::LeftDockWidgetArea;
        case Sublime::Right: return Qt::RightDockWidgetArea;
        case Sublime::Bottom: return Qt::BottomDockWidgetArea;
        case Sublime::Top: return Qt::TopDockWidgetArea;
        default: return Qt::LeftDockWidgetArea;
    }
}

void MainWindowPrivate::switchToArea(QAction *action)
{
    qCDebug(SUBLIME) << "for" << action;
    controller->showArea(m_actionAreas.value(action), m_mainWindow);
}

void MainWindowPrivate::updateAreaSwitcher(Sublime::Area *area)
{
    QAction* action = m_areaActions.value(area);
    if (action)
        action->setChecked(true);
}

void MainWindowPrivate::activateFirstVisibleView()
{
    QList<Sublime::View*> views = area->views();
    if (views.count() > 0)
        m_mainWindow->activateView(views.first());
}

void MainWindowPrivate::widgetResized(Qt::DockWidgetArea /*dockArea*/, int /*thickness*/)
{
    //TODO: adymo: remove all thickness business
}

void MainWindowPrivate::widgetCloseRequest(QWidget* widget)
{
    if (View *view = widgetToView.value(widget))
    {
        area->closeView(view);
    }
}


void MainWindowPrivate::setTabBarLeftCornerWidget(QWidget* widget)
{
    if(widget != m_leftTabbarCornerWidget.data()) {
        delete m_leftTabbarCornerWidget.data();
        m_leftTabbarCornerWidget.clear();
    }
    m_leftTabbarCornerWidget = widget;

    if(!widget || !area || viewContainers.isEmpty())
        return;

    AreaIndex* putToIndex = area->rootIndex();
    QSplitter* splitter = m_indexSplitters[putToIndex];
    while(putToIndex->isSplit()) {
        putToIndex = putToIndex->first();
        splitter = m_indexSplitters[putToIndex];
    }

//     Q_ASSERT(splitter || putToIndex == area->rootIndex());

    Container* c = 0;
    if(splitter) {
        c = qobject_cast<Container*>(splitter->widget(0));
    }else{
        c = *viewContainers.constBegin();
    }
    Q_ASSERT(c);

    c->setLeftCornerWidget(widget);
}

}

#include "mainwindow_p.moc"
#include "moc_mainwindow_p.cpp"

