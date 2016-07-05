/***************************************************************************
   Copyright 2006-2009 David Nolden <david.nolden.kdevelop@art-master.de>
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "localpatchsource.h"

#include <QIcon>
#include <QTemporaryFile>
#include <QWidget>

#include <KLineEdit>
#include <KLocalizedString>
#include <KProcess>
#include <KShell>


#include "ui_localpatchwidget.h"
#include "debug.h"

LocalPatchSource::LocalPatchSource()
    : m_applied(false)
    , m_widget(0)
{
}

LocalPatchSource::~LocalPatchSource()
{
    if ( !m_command.isEmpty() && !m_filename.isEmpty() ) {
        QFile::remove( m_filename.toLocalFile() );
    }
}

QString LocalPatchSource::name() const
{
    return i18n( "Custom Patch" );
}

QIcon LocalPatchSource::icon() const
{
    return QIcon::fromTheme(QStringLiteral("text-x-patch"));
}

void LocalPatchSource::update()
{
    if( !m_command.isEmpty() ) {
        QTemporaryFile temp(QDir::tempPath() + QLatin1String("/patchreview_XXXXXX.diff"));
        if( temp.open() ) {
            temp.setAutoRemove( false );
            QString filename = temp.fileName();
            qCDebug(PLUGIN_PATCHREVIEW) << "temp file: " << filename;
            temp.close();
            KProcess proc;
            proc.setWorkingDirectory( m_baseDir.toLocalFile() );
            proc.setOutputChannelMode( KProcess::OnlyStdoutChannel );
            proc.setStandardOutputFile( filename );
            ///Try to apply, if it works, the patch is not applied
            proc << KShell::splitArgs( m_command );

            qCDebug(PLUGIN_PATCHREVIEW) << "calling " << m_command;

            if ( proc.execute() ) {
                qWarning() << "returned with bad exit code";
                return;
            }
            if ( !m_filename.isEmpty() ) {
                QFile::remove( m_filename.toLocalFile() );
            }
            m_filename = QUrl::fromLocalFile( filename );
            qCDebug(PLUGIN_PATCHREVIEW) << "success, diff: " << m_filename;
        }else{
            qWarning() << "PROBLEM";
        }
    }
    if (m_widget) {
        m_widget->updatePatchFromEdit();
    }
    emit patchChanged();
}

void LocalPatchSource::createWidget()
{
    delete m_widget;
    m_widget = new LocalPatchWidget(this, 0);
}

QWidget* LocalPatchSource::customWidget() const
{
    return m_widget;
}

LocalPatchWidget::LocalPatchWidget(LocalPatchSource* lpatch, QWidget* parent)
    : QWidget(parent)
    , m_lpatch(lpatch)
    , m_ui(new Ui::LocalPatchWidget)
{
    m_ui->setupUi(this);
    m_ui->baseDir->setMode( KFile::Directory );
    syncPatch();
    connect(m_lpatch, &LocalPatchSource::patchChanged, this, &LocalPatchWidget::syncPatch);
}

void LocalPatchWidget::syncPatch()
{
    m_ui->command->setText( m_lpatch->command());
    m_ui->filename->setUrl( m_lpatch->file() );
    m_ui->baseDir->setUrl( m_lpatch->baseDir() );
    m_ui->applied->setCheckState( m_lpatch->isAlreadyApplied() ? Qt::Checked : Qt::Unchecked );

    if ( m_lpatch->command().isEmpty() )
        m_ui->tabWidget->setCurrentIndex( m_ui->tabWidget->indexOf( m_ui->fileTab ) );
    else
        m_ui->tabWidget->setCurrentIndex( m_ui->tabWidget->indexOf( m_ui->commandTab ) );
}

void LocalPatchWidget::updatePatchFromEdit()
{
    m_lpatch->setCommand(m_ui->command->text());
    m_lpatch->setFilename(m_ui->filename->url());
    m_lpatch->setBaseDir(m_ui->baseDir->url());
    m_lpatch->setAlreadyApplied(m_ui->applied->checkState() == Qt::Checked );
}
