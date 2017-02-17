/*
    Copyright 2015 Milian Wolff <mail@milianw.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ktexteditorpluginintegration.h"

#include <QWidget>
#include <QVBoxLayout>

#include <KParts/MainWindow>
#include <KTextEditor/View>
#include <KTextEditor/Editor>
#include <KTextEditor/Application>

#include <sublime/area.h>
#include <sublime/view.h>

#include "core.h"
#include "partcontroller.h"
#include "uicontroller.h"
#include "documentcontroller.h"
#include "plugincontroller.h"
#include "mainwindow.h"
#include "textdocument.h"

#include <util/objectlist.h>

using namespace KDevelop;

namespace {

KTextEditor::MainWindow *toKteWrapper(KParts::MainWindow *window)
{
    if (auto mainWindow = dynamic_cast<KDevelop::MainWindow*>(window)) {
        return mainWindow->kateWrapper() ? mainWindow->kateWrapper()->interface() : nullptr;
    } else {
        return nullptr;
    }
}

KTextEditor::View *toKteView(Sublime::View *view)
{
    if (auto textView = dynamic_cast<KDevelop::TextView*>(view)) {
        return textView->textView();
    } else {
        return nullptr;
    }
}

class ToolViewFactory;

/**
 * This HACK is required to massage the KTextEditor plugin API into the
 * GUI concepts we apply in KDevelop. Kate does not allow the user to
 * delete toolviews and then readd them. We do. To support our use case
 * we prevent the widget we return to KTextEditor plugins from
 * MainWindow::createToolView from getting destroyed. This widget class
 * unsets the parent of the so called container in its dtor. The
 * ToolViewFactory handles the ownership and destroys the kate widget
 * as needed.
 */
class KeepAliveWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KeepAliveWidget(ToolViewFactory *factory, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_factory(factory)
    {
    }

    ~KeepAliveWidget() override;

private:
    ToolViewFactory *m_factory;
};

class ToolViewFactory : public QObject, public KDevelop::IToolViewFactory
{
    Q_OBJECT
public:
    ToolViewFactory(const QString &text, const QIcon &icon, const QString &identifier,
                    KTextEditor::MainWindow::ToolViewPosition pos)
        : m_text(text)
        , m_icon(icon)
        , m_identifier(identifier)
        , m_container(new QWidget)
        , m_pos(pos)
    {
        m_container->setLayout(new QVBoxLayout);
    }

    ~ToolViewFactory() override
    {
        delete m_container;
    }

    QWidget *create(QWidget *parent = nullptr) override
    {
        auto widget = new KeepAliveWidget(this, parent);
        widget->setWindowTitle(m_text);
        widget->setWindowIcon(m_icon);
        widget->setLayout(new QVBoxLayout);
        widget->layout()->addWidget(m_container);
        widget->addActions(m_container->actions());
        return widget;
    }

    Qt::DockWidgetArea defaultPosition() override
    {
        switch (m_pos) {
            case KTextEditor::MainWindow::Left:
                return Qt::LeftDockWidgetArea;
            case KTextEditor::MainWindow::Right:
                return Qt::RightDockWidgetArea;
            case KTextEditor::MainWindow::Top:
                return Qt::TopDockWidgetArea;
            case KTextEditor::MainWindow::Bottom:
                return Qt::BottomDockWidgetArea;
        }
        Q_UNREACHABLE();
    }

    QString id() const override
    {
        return m_identifier;
    }

    QWidget *container() const
    {
        return m_container;
    }

private:
    QString m_text;
    QIcon m_icon;
    QString m_identifier;
    QPointer<QWidget> m_container;
    KTextEditor::MainWindow::ToolViewPosition m_pos;
    friend class KeepAliveWidget;
};

KeepAliveWidget::~KeepAliveWidget()
{
    // if the container is still valid, unparent it to prevent it from getting deleted
    // this happens when the user removes a toolview
    // on shutdown, the container does get deleted, thus we must guard against that.
    if (m_factory->container()) {
        Q_ASSERT(m_factory->container()->parentWidget() == this);
        m_factory->container()->setParent(nullptr);
    }
}

}

namespace KTextEditorIntegration {

Application::Application(QObject *parent)
    : QObject(parent)
{
}

Application::~Application()
{
    KTextEditor::Editor::instance()->setApplication(nullptr);
}

KTextEditor::MainWindow *Application::activeMainWindow() const
{
    return toKteWrapper(Core::self()->uiController()->activeMainWindow());
}

QList<KTextEditor::MainWindow *> Application::mainWindows() const
{
    return {activeMainWindow()};
}

bool Application::closeDocument(KTextEditor::Document *document) const
{
    auto documentController = Core::self()->documentControllerInternal();
    return documentController->closeDocument(document->url());
}

KTextEditor::Plugin *Application::plugin(const QString &id) const
{
    auto kdevPlugin = Core::self()->pluginController()->loadPlugin(id);
    const auto plugin = dynamic_cast<Plugin*>(kdevPlugin);
    return plugin ? plugin->interface() : nullptr;
}

MainWindow::MainWindow(KDevelop::MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
    , m_interface(new KTextEditor::MainWindow(this))
{
    connect(mainWindow, &Sublime::MainWindow::viewAdded, this, [this] (Sublime::View *view) {
        if (auto kteView = toKteView(view)) {
            emit m_interface->viewCreated(kteView);
        }
    });
    connect(mainWindow, &Sublime::MainWindow::activeViewChanged, this, [this] (Sublime::View *view) {
        emit m_interface->viewChanged(toKteView(view));
    });
}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::createToolView(KTextEditor::Plugin* plugin, const QString &identifier,
                                    KTextEditor::MainWindow::ToolViewPosition pos,
                                    const QIcon &icon, const QString &text)
{
    auto factory = new ToolViewFactory(text, icon, identifier, pos);
    Core::self()->uiController()->addToolView(text, factory);
    connect(plugin, &QObject::destroyed, this, [=] {
        Core::self()->uiController()->removeToolView(factory);
    });
    return factory->container();
}

KXMLGUIFactory *MainWindow::guiFactory() const
{
    return m_mainWindow->guiFactory();
}

QWidget *MainWindow::window() const
{
    return m_mainWindow;
}

QList<KTextEditor::View *> MainWindow::views() const
{
    QList<KTextEditor::View *> views;
    foreach (auto area, m_mainWindow->areas()) {
        foreach (auto view, area->views()) {
            if (auto kteView = toKteView(view)) {
                views << kteView;
            }
        }
    }
    return views;
}

KTextEditor::View *MainWindow::activeView() const
{
    return toKteView(m_mainWindow->activeView());
}

QObject *MainWindow::pluginView(const QString &id) const
{
    return m_pluginViews.value(id);
}

QWidget *MainWindow::createViewBar(KTextEditor::View* view)
{
    Q_UNUSED(view);
    // TODO
    return nullptr;
}

void MainWindow::deleteViewBar(KTextEditor::View* view)
{
    Q_UNUSED(view);
    // TODO
}

KTextEditor::MainWindow *MainWindow::interface() const
{
    return m_interface;
}

void MainWindow::addPluginView(const QString &id, QObject *view)
{
    m_pluginViews.insert(id, view);
    emit m_interface->pluginViewCreated(id, view);
}

void MainWindow::removePluginView(const QString &id)
{
    auto view = m_pluginViews.take(id).data();
    delete view;
    emit m_interface->pluginViewDeleted(id, view);
}

Plugin::Plugin(KTextEditor::Plugin *plugin, QObject *parent)
    : IPlugin({}, parent)
    , m_plugin(plugin)
    , m_tracker(new ObjectListTracker(ObjectListTracker::CleanupWhenDone, this))
{
}

Plugin::~Plugin() = default;

void Plugin::unload()
{
    if (auto mainWindow = KTextEditor::Editor::instance()->application()->activeMainWindow()) {
        auto integration = dynamic_cast<MainWindow*>(mainWindow->parent());
        if (integration) {
            integration->removePluginView(pluginId());
        }
    }
    m_tracker->deleteAll();
    delete m_plugin;
}

KXMLGUIClient *Plugin::createGUIForMainWindow(Sublime::MainWindow* window)
{
    auto ret = IPlugin::createGUIForMainWindow(window);
    auto mainWindow = dynamic_cast<KDevelop::MainWindow*>(window);
    Q_ASSERT(mainWindow);

    auto wrapper = mainWindow->kateWrapper();
    auto view = m_plugin->createView(wrapper->interface());
    wrapper->addPluginView(pluginId(), view);
    // ensure that unloading the plugin kills all views
    m_tracker->append(view);

    return ret;
}

KTextEditor::Plugin *Plugin::interface() const
{
    return m_plugin.data();
}

QString Plugin::pluginId() const
{
    return Core::self()->pluginController()->pluginInfo(this).pluginId();
}

void initialize()
{
    auto app = new KTextEditor::Application(new Application(Core::self()));
    KTextEditor::Editor::instance()->setApplication(app);
}

void MainWindow::splitView(Qt::Orientation orientation)
{
    m_mainWindow->split(orientation);
}

}

#include "ktexteditorpluginintegration.moc"
