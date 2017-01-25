/***************************************************************************
 *   Copyright 2007 Alexander Dymo <adymo@kdevelop.org>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "appwizarddialog.h"

#include <QSignalMapper>
#include <QDir>
#include <QPushButton>

#include <KLocalizedString>

#include <interfaces/iplugincontroller.h>
#include <vcs/vcslocation.h>

#include "projecttemplatesmodel.h"
#include "projectselectionpage.h"
#include "projectvcspage.h"

AppWizardDialog::AppWizardDialog(KDevelop::IPluginController* pluginController, ProjectTemplatesModel* templatesModel, QWidget *parent, Qt::WindowFlags flags)
    :KAssistantDialog(parent, flags)
{
    setWindowTitle(i18n("Create New Project"));

    // KAssistantDialog creates a help button by default, no option to prevent that
    QPushButton *helpButton = button(QDialogButtonBox::Help);
    if (helpButton) {
        buttonBox()->removeButton(helpButton);
        delete helpButton;
    }

    m_selectionPage = new ProjectSelectionPage(templatesModel, this);
    m_vcsPage = new ProjectVcsPage( pluginController, this );
    m_vcsPage->setSourceLocation( m_selectionPage->location() );
    connect( m_selectionPage, &ProjectSelectionPage::locationChanged,
             m_vcsPage, &ProjectVcsPage::setSourceLocation );
    m_pageItems[m_selectionPage] = addPage(m_selectionPage, i18nc("Page for general configuration options", "General"));

    m_pageItems[m_vcsPage] = addPage(m_vcsPage, i18nc("Page for version control options", "Version Control") );

    setValid( m_pageItems[m_selectionPage], false );

    m_invalidMapper = new QSignalMapper(this);
    m_invalidMapper->setMapping(m_selectionPage, m_selectionPage);
    m_invalidMapper->setMapping(m_vcsPage, m_vcsPage);
    m_validMapper = new QSignalMapper(this);
    m_validMapper->setMapping(m_selectionPage, m_selectionPage);
    m_validMapper->setMapping(m_vcsPage, m_vcsPage);

    connect( m_selectionPage, &ProjectSelectionPage::invalid, m_invalidMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map) );
    connect( m_selectionPage, &ProjectSelectionPage::valid, m_validMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map) );

    connect( m_vcsPage, &ProjectVcsPage::invalid, m_invalidMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map) );
    connect( m_vcsPage, &ProjectVcsPage::valid, m_validMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map) );

    connect( m_validMapper, static_cast<void(QSignalMapper::*)(QWidget*)>(&QSignalMapper::mapped), this, &AppWizardDialog::pageValid );    
    connect( m_invalidMapper, static_cast<void(QSignalMapper::*)(QWidget*)>(&QSignalMapper::mapped), this, &AppWizardDialog::pageInValid );
}

ApplicationInfo AppWizardDialog::appInfo() const
{
    ApplicationInfo a;
    a.name = m_selectionPage->projectName();
    a.location = m_selectionPage->location();
    a.appTemplate = m_selectionPage->selectedTemplate();
    a.vcsPluginName = m_vcsPage->pluginName();

    if( !m_vcsPage->pluginName().isEmpty() )
    {
        a.repository = m_vcsPage->destination();
        a.sourceLocation = m_vcsPage->source();
        a.importCommitMessage = m_vcsPage->commitMessage();
    }
    else
    {
        a.repository = KDevelop::VcsLocation();
        a.sourceLocation.clear();
        a.importCommitMessage.clear();
    }
    return a;
}


void AppWizardDialog::pageValid( QWidget* w )
{
    if( m_pageItems.contains(w) )
        setValid( m_pageItems[w], true );
}


void AppWizardDialog::pageInValid( QWidget* w )
{
    if( m_pageItems.contains(w) )
        setValid( m_pageItems[w], false );
}

void AppWizardDialog::next()
{
    AppWizardPageWidget* w = qobject_cast<AppWizardPageWidget*>(currentPage()->widget());
    if (!w || w->shouldContinue()) {
        KAssistantDialog::next();
    }
}
