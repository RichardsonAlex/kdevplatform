/***************************************************************************
 *   Copyright (C) 2010 by Aleix Pol Gonzalez <aleixpol@kde.org>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "projectsourcepage.h"
#include "ui_projectsourcepage.h"
#include <interfaces/iplugin.h>
#include <interfaces/icore.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iruncontroller.h>
#include <vcs/interfaces/ibasicversioncontrol.h>
#include <vcs/widgets/vcslocationwidget.h>
#include <vcs/vcsjob.h>
#include <QVBoxLayout>
#include <QDebug>
#include <KMessageBox>
#include <KColorScheme>
#include <interfaces/iprojectprovider.h>

using namespace KDevelop;

ProjectSourcePage::ProjectSourcePage(const KUrl& initial, QWidget* parent)
    : QWidget(parent)
{
    m_ui = new Ui::ProjectSourcePage;
    m_ui->setupUi(this);
    
    m_ui->workingDir->setUrl(initial);
    m_ui->workingDir->setMode(KFile::Directory);
    m_ui->remoteWidget->setLayout(new QVBoxLayout(m_ui->remoteWidget));
    
    m_ui->sources->addItem(KIcon("folder"), i18n("Do not obtain"));
    m_plugins.append(0);
    
    IPluginController* pluginManager = ICore::self()->pluginController();
    foreach( IPlugin* p, pluginManager->allPluginsForExtension( "org.kdevelop.IBasicVersionControl" ) )
    {
        m_plugins.append(p);
        m_ui->sources->addItem(KIcon(pluginManager->pluginInfo(p).icon()), p->extension<IBasicVersionControl>()->name());
    }
    
    foreach( IPlugin* p, pluginManager->allPluginsForExtension( "org.kdevelop.IProjectProvider" ) )
    {
        m_plugins.append(p);
        m_ui->sources->addItem(KIcon(pluginManager->pluginInfo(p).icon()), p->extension<IProjectProvider>()->name());
    }
    
    connect(m_ui->workingDir, SIGNAL(textChanged(QString)), SLOT(reevaluateCorrection()));
    connect(m_ui->sources, SIGNAL(currentIndexChanged(int)), SLOT(sourceChanged(int)));
    connect(m_ui->get, SIGNAL(clicked()), SLOT(getVcsProject()));
    
    sourceChanged(0);
}

void ProjectSourcePage::sourceChanged(int index)
{
    m_locationWidget=0;
    QLayout* remoteWidgetLayout = m_ui->remoteWidget->layout();
    QLayoutItem *child;
    while ((child = remoteWidgetLayout->takeAt(0)) != 0) {
        delete child->widget();
        delete child;
    }
    
    IBasicVersionControl* vcIface = vcsPerIndex(index);
    IProjectProvider* providerIface;
    bool found=false;
    if(vcIface) {
        found=true;
        m_locationWidget=vcIface->vcsLocation(m_ui->sourceBox);
        connect(m_locationWidget, SIGNAL(changed()), SLOT(locationChanged()));
        
        remoteWidgetLayout->addWidget(m_locationWidget);
    } else {
        providerIface = providerPerIndex(index);
        if(providerIface) {
            found=true;
            m_providerWidget=providerIface->providerWidget(m_ui->sourceBox);
            
            remoteWidgetLayout->addWidget(m_providerWidget);   
        }
    }
    reevaluateCorrection();
    
    m_ui->sourceBox->setVisible(found);
}

IBasicVersionControl* ProjectSourcePage::vcsPerIndex(int index)
{
    IPlugin* p=m_plugins[index];
    if(!p)
        return 0;
    else
        return p->extension<KDevelop::IBasicVersionControl>();
}

IProjectProvider* ProjectSourcePage::providerPerIndex(int index)
{
    IPlugin* p=m_plugins[index];
    if(!p)
        return 0;
    else
        return p->extension<KDevelop::IProjectProvider>();
}

VcsJob* ProjectSourcePage::jobPerCurrent()
{
    KUrl url=m_ui->workingDir->url();
    IPlugin* p=m_plugins[m_ui->sources->currentIndex()];
    VcsJob* job=0;
    
    if(IBasicVersionControl* iface=p->extension<IBasicVersionControl>()) {
        Q_ASSERT(iface && m_locationWidget);
        job=iface->createWorkingCopy(m_locationWidget->location(), url);
    } else if(m_providerWidget) {
        job=m_providerWidget->createWorkingCopy(url);
    }
    Q_ASSERT(job);
    return job;
}

void ProjectSourcePage::getVcsProject()
{
    emit isCorrect(false);
    KUrl url=m_ui->workingDir->url();
    QDir d(url.toLocalFile());
    if(!url.isLocalFile() && !d.exists()) {
        bool corr = d.mkpath(d.path());
        if(!corr) {
            KMessageBox::error(0, i18n("Could not create the directory: %1", d.path()));
            return;
        }
    }
    
    VcsJob* job=jobPerCurrent();
    
    m_ui->creationProgress->setValue(m_ui->creationProgress->minimum());
    connect(job, SIGNAL(result(KJob*)), SLOT(projectReceived(KJob*)));
    connect(job, SIGNAL(percent(KJob*, unsigned long)), SLOT(progressChanged(KJob*, unsigned long)));
    connect(job, SIGNAL(infoMessage(KJob*,QString,QString)), SLOT(infoMessage(KJob*,QString,QString)));
    ICore::self()->runController()->registerJob(job);
}

void ProjectSourcePage::progressChanged(KJob*, unsigned long value)
{
    m_ui->creationProgress->setValue(value);
}

void ProjectSourcePage::infoMessage(KJob* , const QString& text, const QString& rich)
{
    m_ui->creationProgress->setFormat(i18nc("Format of the progress bar text. progress and info",
                                            "%1 : %p%", text));
}

void ProjectSourcePage::projectReceived(KJob* job)
{
    m_ui->creationProgress->setValue(m_ui->creationProgress->maximum());
    
    reevaluateCorrection();
    m_ui->creationProgress->setFormat("%p%");
}

void ProjectSourcePage::reevaluateCorrection()
{
    //TODO: Probably we should just ignore remote URL's, I don't think we're ever going
    //to support checking out to remote directories
    KUrl cwd=m_ui->workingDir->url();
    bool correct=!cwd.isEmpty() && (!cwd.isLocalFile() || QDir(cwd.upUrl().toLocalFile()).exists());
    emit isCorrect(correct);
    
    QDir d(cwd.toLocalFile());
    bool validToCheckout=correct && (!m_locationWidget || m_locationWidget->isCorrect()); //To checkout, if it exists, it should be an empty dir
    if(correct && cwd.isLocalFile() && d.exists()) {
        validToCheckout = d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
    }
    
    m_ui->get->setEnabled(validToCheckout);
    m_ui->creationProgress->setEnabled(validToCheckout);
    
    if(!correct)
        setStatus(i18n("You need to specify a valid or unexistent directory to check out a project"));
    else if(!m_ui->get->isEnabled())
        setStatus(i18n("You need to specify a valid location for the project"));
    else
        validStatus();
}

void ProjectSourcePage::locationChanged()
{
    Q_ASSERT(m_locationWidget);
    if(m_locationWidget->isCorrect()) {
        QString currentUrl = m_ui->workingDir->text();
        currentUrl = currentUrl.left(currentUrl.lastIndexOf('/')+1);
        
        KUrl current = currentUrl;
        current.addPath(m_locationWidget->projectName());
        
        m_ui->workingDir->setUrl(current);
    }
    else
        reevaluateCorrection();
}

void ProjectSourcePage::setStatus(const QString& message)
{
    KColorScheme scheme(QPalette::Normal);
    m_ui->status->setText(QString("<font color='%1'>%2</font>").arg(scheme.foreground(KColorScheme::NegativeText).color().name()).arg(message));
}

void ProjectSourcePage::validStatus()
{
    m_ui->status->clear();
}

KUrl ProjectSourcePage::workingDir() const
{
    return m_ui->workingDir->url();
}
