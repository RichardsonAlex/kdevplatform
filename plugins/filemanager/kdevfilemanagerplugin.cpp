/***************************************************************************
 *   Copyright 2006 Alexander Dymo <adymo@kdevelop.org>             *
 *   Copyright 2007 Andreas Pakulat <apaku@gmx.de>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
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
#include "kdevfilemanagerplugin.h"

#include <QTimer>

#include <klocale.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

#include <icore.h>
#include <iuicontroller.h>
// #include <mainwindow.h>

#include "filemanager.h"

K_PLUGIN_FACTORY(KDevFileManagerFactory, registerPlugin<KDevFileManagerPlugin>(); )
K_EXPORT_PLUGIN(KDevFileManagerFactory("kdevfilemanager"))


class KDevFileManagerViewFactory: public KDevelop::IToolViewFactory{
public:
    KDevFileManagerViewFactory(KDevFileManagerPlugin *plugin): m_plugin(plugin) {}
    virtual QWidget* create(QWidget *parent = 0)
    {
        Q_UNUSED(parent)
        return new FileManager(m_plugin, parent);
    }
    virtual Qt::DockWidgetArea defaultPosition(const QString &/*areaName*/)
    {
        return Qt::LeftDockWidgetArea;
    }
private:
    KDevFileManagerPlugin *m_plugin;
};

KDevFileManagerPlugin::KDevFileManagerPlugin(QObject *parent, const QVariantList &/*args*/)
    :KDevelop::IPlugin(KDevFileManagerFactory::componentData(), parent)
{
    setXMLFile("kdevfilemanager.rc");

    init();
}

void KDevFileManagerPlugin::init()
{
    m_factory = new KDevFileManagerViewFactory(this);
    core()->uiController()->addToolView("File Manager", m_factory);
}

KDevFileManagerPlugin::~KDevFileManagerPlugin()
{
}

void KDevFileManagerPlugin::unload()
{
    core()->uiController()->removeToolView(m_factory);
}

#include "kdevfilemanagerplugin.moc"

