/***************************************************************************
 *   Copyright 2007 Robert Gruber <rgruber@users.sourceforge.net>          *
 *   Copyright 2007 Andreas Pakulat <apaku@gmx.de>                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "importmetadatawidget.h"

#include <KMessageBox>

#include <vcs/vcslocation.h>

ImportMetadataWidget::ImportMetadataWidget(QWidget *parent)
    : KDevelop::VcsImportMetadataWidget(parent), m_ui( new Ui::ImportMetadataWidget )
{
    m_ui->setupUi(this);

    m_ui->sourceLoc->setEnabled( false );
    m_ui->sourceLoc->setMode( KFile::Directory );

    connect( m_ui->sourceLoc, &KUrlRequester::textChanged, this, &ImportMetadataWidget::changed );
    connect( m_ui->sourceLoc, &KUrlRequester::urlSelected, this, &ImportMetadataWidget::changed );
    connect( m_ui->comment, &KTextEdit::textChanged, this, &ImportMetadataWidget::changed );
    connect( m_ui->module, &QLineEdit::textEdited, this, &ImportMetadataWidget::changed );
    connect( m_ui->releaseTag, &QLineEdit::textEdited, this, &ImportMetadataWidget::changed );
    connect( m_ui->repository, &QLineEdit::textEdited, this, &ImportMetadataWidget::changed );
    connect( m_ui->vendorTag, &QLineEdit::textEdited, this, &ImportMetadataWidget::changed );

}

ImportMetadataWidget::~ImportMetadataWidget()
{
    delete m_ui;
}

QUrl ImportMetadataWidget::source() const
{
    return m_ui->sourceLoc->url() ;
}

KDevelop::VcsLocation ImportMetadataWidget::destination() const
{
    KDevelop::VcsLocation destloc;
    destloc.setRepositoryServer(m_ui->repository->text() );
    destloc.setRepositoryModule(m_ui->module->text());
    destloc.setRepositoryTag(m_ui->vendorTag->text());
    destloc.setUserData(m_ui->releaseTag->text());
    return destloc;
}

QString ImportMetadataWidget::message( ) const
{
    return m_ui->comment->toPlainText();
}

void ImportMetadataWidget::setSourceLocation( const KDevelop::VcsLocation& url )
{
    m_ui->sourceLoc->setUrl( url.localUrl() );
}

void ImportMetadataWidget::setSourceLocationEditable( bool enable )
{
    m_ui->sourceLoc->setEnabled( enable );
}

void ImportMetadataWidget::setMessage(const QString& message)
{
    m_ui->comment->setText(message);
}

bool ImportMetadataWidget::hasValidData() const
{
    return !m_ui->comment->toPlainText().isEmpty() && !m_ui->sourceLoc->text().isEmpty()
    && !m_ui->module->text().isEmpty() && !m_ui->repository->text().isEmpty();
}

