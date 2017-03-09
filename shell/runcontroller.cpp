/* This file is part of KDevelop
Copyright 2007-2008 Hamish Rodda <rodda@kde.org>
Copyright 2008 Aleix Pol <aleixpol@gmail.com>

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

#include "runcontroller.h"

#include <QDebug>
#include <QDBusConnection>
#include <QPalette>
#include <QSignalMapper>

#include <KAboutData>
#include <KActionCollection>
#include <KActionMenu>
#include <KColorScheme>
#include <KDialogJobUiDelegate>
#include <KMessageBox>
#include <KLocalizedString>
#include <KSelectAction>

#include <interfaces/iproject.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ilauncher.h>
#include <interfaces/ilaunchmode.h>
#include <interfaces/launchconfigurationtype.h>
#include <outputview/ioutputview.h>
#include <outputview/outputjob.h>
#include <project/projectmodel.h>
#include <project/interfaces/iprojectbuilder.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include "core.h"
#include "plugincontroller.h"
#include "uicontroller.h"
#include "projectcontroller.h"
#include "mainwindow.h"
#include "launchconfiguration.h"
#include "launchconfigurationdialog.h"
#include "unitylauncher.h"
#include "debug.h"
#include <interfaces/isession.h>

#include <interfaces/contextmenuextension.h>
#include <interfaces/context.h>
#include <sublime/area.h>

using namespace KDevelop;

namespace {
namespace Strings {
QString LaunchConfigurationsGroup()
{
    return QStringLiteral("Launch");
}

QString LaunchConfigurationsListEntry()
{
    return QStringLiteral("Launch Configurations");
}

QString CurrentLaunchConfigProjectEntry()
{
    return QStringLiteral("Current Launch Config Project");
}

QString CurrentLaunchConfigNameEntry()
{
    return QStringLiteral("Current Launch Config GroupName");
}

QString ConfiguredFromProjectItemEntry()
{
    return QStringLiteral("Configured from ProjectItem");
}
}
}

typedef QPair<QString, IProject*> Target;
Q_DECLARE_METATYPE(Target)


//TODO: Doesn't handle add/remove of launch configs in the dialog or renaming of configs
//TODO: Doesn't auto-select launch configs opened from projects

class DebugMode : public ILaunchMode
{
public:
    DebugMode() {}
    QIcon icon() const override { return QIcon::fromTheme(QStringLiteral("debug-run")); }
    QString id() const override { return QStringLiteral("debug"); }
    QString name() const override { return i18n("Debug"); }
};

class ProfileMode : public ILaunchMode
{
public:
    ProfileMode() {}
    QIcon icon() const override { return QIcon::fromTheme(QStringLiteral("office-chart-area")); }
    QString id() const override { return QStringLiteral("profile"); }
    QString name() const override { return i18n("Profile"); }
};

class ExecuteMode : public ILaunchMode
{
public:
    ExecuteMode() {}
    QIcon icon() const override { return QIcon::fromTheme(QStringLiteral("system-run")); }
    QString id() const override { return QStringLiteral("execute"); }
    QString name() const override { return i18n("Execute"); }
};

class RunController::RunControllerPrivate
{
public:
    QItemDelegate* delegate;

    IRunController::State state;

    RunController* q;

    QHash<KJob*, QAction*> jobs;
    QAction* stopAction;
    KActionMenu* stopJobsMenu;
    QAction* runAction;
    QAction* dbgAction;
    KSelectAction* currentTargetAction;
    QMap<QString,LaunchConfigurationType*> launchConfigurationTypes;
    QList<LaunchConfiguration*> launchConfigurations;
    QMap<QString,ILaunchMode*> launchModes;
    QSignalMapper* launchChangeMapper;
    QSignalMapper* launchAsMapper;
    QMap<int,QPair<QString,QString> > launchAsInfo;
    KDevelop::ProjectBaseItem* contextItem;
    DebugMode* debugMode;
    ExecuteMode* executeMode;
    ProfileMode* profileMode;
    UnityLauncher* unityLauncher;

    bool hasLaunchConfigType( const QString& typeId )
    {
        return launchConfigurationTypes.contains( typeId );
    }
    void saveCurrentLaunchAction()
    {
        if (!currentTargetAction) return;

        if( currentTargetAction->currentAction() )
        {
            KConfigGroup grp = Core::self()->activeSession()->config()->group( Strings::LaunchConfigurationsGroup() );
            LaunchConfiguration* l = static_cast<LaunchConfiguration*>( currentTargetAction->currentAction()->data().value<void*>() );
            grp.writeEntry( Strings::CurrentLaunchConfigProjectEntry(), l->project() ? l->project()->name() : QLatin1String("") );
            grp.writeEntry( Strings::CurrentLaunchConfigNameEntry(), l->configGroupName() );
            grp.sync();
        }
    }

    QString launchActionText( LaunchConfiguration* l )
    {
        QString label;
        if( l->project() )
        {
            label = QStringLiteral("%1 : %2").arg( l->project()->name(), l->name());
        } else
        {
            label = l->name();
        }
        return label;
    }

    void launchAs( int id )
    {
        //qCDebug(SHELL) << "Launching id:" << id;
        QPair<QString,QString> info = launchAsInfo[id];
        //qCDebug(SHELL) << "fetching type and mode:" << info.first << info.second;
        LaunchConfigurationType* type = launchConfigurationTypeForId( info.first );
        ILaunchMode* mode = q->launchModeForId( info.second );

        //qCDebug(SHELL) << "got mode and type:" << type << type->id() << mode << mode->id();
        if( type && mode )
        {
            ILauncher* launcher = nullptr;
            foreach (ILauncher *l, type->launchers())
            {
                //qCDebug(SHELL) << "available launcher" << l << l->id() << l->supportedModes();
                if (l->supportedModes().contains(mode->id())) {
                    launcher = l;
                    break;
                }
            }
            if (launcher)
            {
                QStringList itemPath = Core::self()->projectController()->projectModel()->pathFromIndex(contextItem->index());
                ILaunchConfiguration* ilaunch = nullptr;
                foreach (LaunchConfiguration *l, launchConfigurations) {
                    QStringList path = l->config().readEntry(Strings::ConfiguredFromProjectItemEntry(), QStringList());
                    if (l->type() == type && path == itemPath) {
                        qCDebug(SHELL) << "already generated ilaunch" << path;
                        ilaunch = l;
                        break;
                    }
                }
                if (!ilaunch) {
                    ilaunch = q->createLaunchConfiguration( type,
                                                            qMakePair( mode->id(), launcher->id() ),
                                                            contextItem->project(),
                                                            contextItem->text() );
                    LaunchConfiguration* launch = dynamic_cast<LaunchConfiguration*>( ilaunch );
                    type->configureLaunchFromItem( launch->config(), contextItem );
                    launch->config().writeEntry(Strings::ConfiguredFromProjectItemEntry(), itemPath);
                    //qCDebug(SHELL) << "created config, launching";
                } else {
                    //qCDebug(SHELL) << "reusing generated config, launching";
                }
                q->setDefaultLaunch(ilaunch);
                q->execute( mode->id(), ilaunch );
            }
        }
    }

    void updateCurrentLaunchAction()
    {
        if (!currentTargetAction) return;

        KConfigGroup launchGrp = Core::self()->activeSession()->config()->group( Strings::LaunchConfigurationsGroup() );
        QString currentLaunchProject = launchGrp.readEntry( Strings::CurrentLaunchConfigProjectEntry(), "" );
        QString currentLaunchName = launchGrp.readEntry( Strings::CurrentLaunchConfigNameEntry(), "" );

        LaunchConfiguration* l = nullptr;
        if( currentTargetAction->currentAction() )
        {
            l = static_cast<LaunchConfiguration*>( currentTargetAction->currentAction()->data().value<void*>() );
        } else if( !launchConfigurations.isEmpty() )
        {
            l = launchConfigurations.at( 0 );
        }

        if( l && ( ( !currentLaunchProject.isEmpty() && ( !l->project() || l->project()->name() != currentLaunchProject ) ) || l->configGroupName() != currentLaunchName ) )
        {
            foreach( QAction* a, currentTargetAction->actions() )
            {
                LaunchConfiguration* l = static_cast<LaunchConfiguration*>( qvariant_cast<void*>( a->data() ) );
                if( currentLaunchName == l->configGroupName()
                    && ( ( currentLaunchProject.isEmpty() && !l->project() )
                         || ( l->project() && l->project()->name() == currentLaunchProject ) ) )
                {
                    a->setChecked( true );
                    break;
                }
            }
        }
        if( !currentTargetAction->currentAction() )
        {
            qCDebug(SHELL) << "oops no current action, using first if list is non-empty";
            if( !currentTargetAction->actions().isEmpty() )
            {
                currentTargetAction->actions().at(0)->setChecked( true );
            }
        }
    }

    void addLaunchAction( LaunchConfiguration* l )
    {
        if (!currentTargetAction) return;

        QAction* action = currentTargetAction->addAction(launchActionText( l ));
        action->setData(qVariantFromValue<void*>(l));
    }
    void readLaunchConfigs( KSharedConfigPtr cfg, IProject* prj )
    {
        KConfigGroup group(cfg, Strings::LaunchConfigurationsGroup());
        QStringList configs = group.readEntry( Strings::LaunchConfigurationsListEntry(), QStringList() );

        foreach( const QString& cfg, configs )
        {
            KConfigGroup grp = group.group( cfg );
            if( launchConfigurationTypeForId( grp.readEntry( LaunchConfiguration::LaunchConfigurationTypeEntry(), "" ) ) )
            {
                q->addLaunchConfiguration( new LaunchConfiguration( grp, prj ) );
            }
        }
    }
    LaunchConfigurationType* launchConfigurationTypeForId( const QString& id )
    {
        QMap<QString, LaunchConfigurationType*>::iterator it = launchConfigurationTypes.find( id );
        if( it != launchConfigurationTypes.end() )
        {
            return it.value();
        } else
        {
            qWarning() << "couldn't find type for id:" << id << ". Known types:" << launchConfigurationTypes.keys();
        }
        return nullptr;

    }

};

RunController::RunController(QObject *parent)
    : IRunController(parent)
    , d(new RunControllerPrivate)
{
    setObjectName(QStringLiteral("RunController"));

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kdevelop/RunController"),
        this, QDBusConnection::ExportScriptableSlots);

    // TODO: need to implement compile only if needed before execute
    // TODO: need to implement abort all running programs when project closed

    d->currentTargetAction = nullptr;
    d->state = Idle;
    d->q = this;
    d->delegate = new RunDelegate(this);
    d->launchChangeMapper = new QSignalMapper( this );
    d->launchAsMapper = nullptr;
    d->contextItem = nullptr;
    d->executeMode = nullptr;
    d->debugMode = nullptr;
    d->profileMode = nullptr;

    d->unityLauncher = new UnityLauncher(this);
    d->unityLauncher->setLauncherId(KAboutData::applicationData().desktopFileName());

    if(!(Core::self()->setupFlags() & Core::NoUi)) {
        // Note that things like registerJob() do not work without the actions, it'll simply crash.
        setupActions();
    }
}

RunController::~RunController()
{
    delete d;
}

void KDevelop::RunController::launchChanged( LaunchConfiguration* l )
{
    foreach( QAction* a, d->currentTargetAction->actions() )
    {
        if( static_cast<LaunchConfiguration*>( a->data().value<void*>() ) == l )
        {
            a->setText( d->launchActionText( l ) );
            break;
        }
    }
}

void RunController::cleanup()
{
    delete d->executeMode;
    d->executeMode = nullptr;
    delete d->profileMode;
    d->profileMode = nullptr;
    delete d->debugMode;
    d->debugMode = nullptr;

    stopAllProcesses();
    d->saveCurrentLaunchAction();
}

void RunController::initialize()
{
    d->executeMode = new ExecuteMode();
    addLaunchMode( d->executeMode );
    d->profileMode = new ProfileMode();
    addLaunchMode( d->profileMode );
    d->debugMode = new DebugMode;
    addLaunchMode( d->debugMode );
    d->readLaunchConfigs( Core::self()->activeSession()->config(), nullptr );

    foreach (IProject* project, Core::self()->projectController()->projects()) {
        slotProjectOpened(project);
    }
    connect(Core::self()->projectController(), &IProjectController::projectOpened,
            this, &RunController::slotProjectOpened);
    connect(Core::self()->projectController(), &IProjectController::projectClosing,
            this, &RunController::slotProjectClosing);
    connect(Core::self()->projectController(), &IProjectController::projectConfigurationChanged,
             this, &RunController::slotRefreshProject);

    if( (Core::self()->setupFlags() & Core::NoUi) == 0 )
    {
        // Only do this in GUI mode
        d->updateCurrentLaunchAction();
    }
}

KJob* RunController::execute(const QString& runMode, ILaunchConfiguration* launch)
{
    if( !launch )
    {
        qCDebug(SHELL) << "execute called without launch config!";
        return nullptr;
    }
    LaunchConfiguration *run = dynamic_cast<LaunchConfiguration*>(launch);
    //TODO: Port to launch framework, probably needs to be part of the launcher
    //if(!run.dependencies().isEmpty())
    //    ICore::self()->documentController()->saveAllDocuments(IDocument::Silent);

    //foreach(KJob* job, run.dependencies())
    //{
    //    jobs.append(job);
    //}

    qCDebug(SHELL) << "mode:" << runMode;
    QString launcherId = run->launcherForMode( runMode );
    qCDebug(SHELL) << "launcher id:" << launcherId;

    ILauncher* launcher = run->type()->launcherForId( launcherId );

    if( !launcher ) {
        KMessageBox::error(
            qApp->activeWindow(),
            i18n("The current launch configuration does not support the '%1' mode.", runMode),
            QLatin1String(""));
        return nullptr;
    }

    KJob* launchJob = launcher->start(runMode, run);
    registerJob(launchJob);
    return launchJob;
}

void RunController::setupActions()
{
    QAction* action;

    // TODO not multi-window friendly, FIXME
    KActionCollection* ac = Core::self()->uiControllerInternal()->defaultMainWindow()->actionCollection();

    action = new QAction(i18n("Configure Launches..."), this);
    ac->addAction(QStringLiteral("configure_launches"), action);
    action->setMenuRole(QAction::NoRole); // OSX: Be explicit about role, prevent hiding due to conflict with "Preferences..." menu item
    action->setStatusTip(i18n("Open Launch Configuration Dialog"));
    action->setToolTip(i18nc("@info:tooltip", "Open Launch Configuration Dialog"));
    action->setWhatsThis(i18nc("@info:whatsthis", "Opens a dialog to setup new launch configurations, or to change the existing ones."));
    connect(action, &QAction::triggered, this, &RunController::showConfigurationDialog);

    d->runAction = new QAction( QIcon::fromTheme(QStringLiteral("system-run")), i18n("Execute Launch"), this);
    d->runAction->setIconText( i18nc("Short text for 'Execute launch' used in the toolbar", "Execute") );
    ac->setDefaultShortcut( d->runAction, Qt::SHIFT + Qt::Key_F9);
    d->runAction->setToolTip(i18nc("@info:tooltip", "Execute current launch"));
    d->runAction->setStatusTip(i18n("Execute current launch"));
    d->runAction->setWhatsThis(i18nc("@info:whatsthis", "Executes the target or the program specified in currently active launch configuration."));
    ac->addAction(QStringLiteral("run_execute"), d->runAction);
    connect(d->runAction, &QAction::triggered, this, &RunController::slotExecute);

    d->dbgAction = new QAction( QIcon::fromTheme(QStringLiteral("debug-run")), i18n("Debug Launch"), this);
    ac->setDefaultShortcut( d->dbgAction, Qt::Key_F9);
    d->dbgAction->setIconText( i18nc("Short text for 'Debug launch' used in the toolbar", "Debug") );
    d->dbgAction->setToolTip(i18nc("@info:tooltip", "Debug current launch"));
    d->dbgAction->setStatusTip(i18n("Debug current launch"));
    d->dbgAction->setWhatsThis(i18nc("@info:whatsthis", "Executes the target or the program specified in currently active launch configuration inside a Debugger."));
    ac->addAction(QStringLiteral("run_debug"), d->dbgAction);
    connect(d->dbgAction, &QAction::triggered, this, &RunController::slotDebug);
    Core::self()->uiControllerInternal()->area(0, QStringLiteral("code"))->addAction(d->dbgAction);

//     TODO: at least get a profile target, it's sad to have the menu entry without a profiler
//     QAction* profileAction = new QAction( QIcon::fromTheme(""), i18n("Profile Launch"), this);
//     profileAction->setToolTip(i18nc("@info:tooltip", "Profile current launch"));
//     profileAction->setStatusTip(i18n("Profile current launch"));
//     profileAction->setWhatsThis(i18nc("@info:whatsthis", "Executes the target or the program specified in currently active launch configuration inside a Profiler."));
//     ac->addAction("run_profile", profileAction);
//     connect(profileAction, SIGNAL(triggered(bool)), this, SLOT(slotProfile()));

    action = d->stopAction = new QAction( QIcon::fromTheme(QStringLiteral("process-stop")), i18n("Stop All Jobs"), this);
    action->setIconText(i18nc("Short text for 'Stop All Jobs' used in the toolbar", "Stop All"));
    // Ctrl+Escape would be nicer, but that is taken by the ksysguard desktop shortcut
    ac->setDefaultShortcut( action, QKeySequence(QStringLiteral("Ctrl+Shift+Escape")));
    action->setToolTip(i18nc("@info:tooltip", "Stop all currently running jobs"));
    action->setWhatsThis(i18nc("@info:whatsthis", "Requests that all running jobs are stopped."));
    action->setEnabled(false);
    ac->addAction(QStringLiteral("run_stop_all"), action);
    connect(action, &QAction::triggered, this, &RunController::stopAllProcesses);
    Core::self()->uiControllerInternal()->area(0, QStringLiteral("debug"))->addAction(action);

    action = d->stopJobsMenu = new KActionMenu( QIcon::fromTheme(QStringLiteral("process-stop")), i18n("Stop"), this);
    action->setIconText(i18nc("Short text for 'Stop' used in the toolbar", "Stop"));
    action->setToolTip(i18nc("@info:tooltip", "Menu allowing to stop individual jobs"));
    action->setWhatsThis(i18nc("@info:whatsthis", "List of jobs that can be stopped individually."));
    action->setEnabled(false);
    ac->addAction(QStringLiteral("run_stop_menu"), action);

    d->currentTargetAction = new KSelectAction( i18n("Current Launch Configuration"), this);
    d->currentTargetAction->setToolTip(i18nc("@info:tooltip", "Current launch configuration"));
    d->currentTargetAction->setStatusTip(i18n("Current launch Configuration"));
    d->currentTargetAction->setWhatsThis(i18nc("@info:whatsthis", "Select which launch configuration to run when run is invoked."));
    ac->addAction(QStringLiteral("run_default_target"), d->currentTargetAction);
}

LaunchConfigurationType* RunController::launchConfigurationTypeForId( const QString& id )
{
    return d->launchConfigurationTypeForId( id );
}

void KDevelop::RunController::slotProjectOpened(KDevelop::IProject * project)
{
    d->readLaunchConfigs( project->projectConfiguration(), project );
    d->updateCurrentLaunchAction();
}

void KDevelop::RunController::slotProjectClosing(KDevelop::IProject * project)
{
    if (!d->currentTargetAction) return;

    foreach (QAction* action, d->currentTargetAction->actions()) {
        LaunchConfiguration* l = static_cast<LaunchConfiguration*>(qvariant_cast<void*>(action->data()));
        if ( project == l->project() ) {
            l->save();
            d->launchConfigurations.removeAll(l);
            delete l;
            bool wasSelected = action->isChecked();
            delete action;
            if (wasSelected && !d->currentTargetAction->actions().isEmpty())
                d->currentTargetAction->actions().at(0)->setChecked(true);
        }
    }
}

void KDevelop::RunController::slotRefreshProject(KDevelop::IProject* project)
{
    slotProjectClosing(project);
    slotProjectOpened(project);
}

void RunController::slotDebug()
{
    if (d->launchConfigurations.isEmpty()) {
        showConfigurationDialog();
    }

    if (!d->launchConfigurations.isEmpty()) {
        executeDefaultLaunch( QStringLiteral("debug") );
    }
}

void RunController::slotProfile()
{
    if (d->launchConfigurations.isEmpty()) {
        showConfigurationDialog();
    }

    if (!d->launchConfigurations.isEmpty()) {
        executeDefaultLaunch( QStringLiteral("profile") );
    }
}

void RunController::slotExecute()
{
    if (d->launchConfigurations.isEmpty()) {
        showConfigurationDialog();
    }

    if (!d->launchConfigurations.isEmpty()) {
        executeDefaultLaunch( QStringLiteral("execute") );
    }
}

void KDevelop::RunController::showConfigurationDialog() const
{
    LaunchConfigurationDialog dlg;
    dlg.exec();
}

LaunchConfiguration* KDevelop::RunController::defaultLaunch() const
{
    QAction* projectAction = d->currentTargetAction->currentAction();
    if( projectAction )
        return static_cast<LaunchConfiguration*>(qvariant_cast<void*>(projectAction->data()));
    return nullptr;
}

void KDevelop::RunController::registerJob(KJob * job)
{
    if (!job)
        return;

    if (!(job->capabilities() & KJob::Killable)) {
        // see e.g. https://bugs.kde.org/show_bug.cgi?id=314187
        qWarning() << "non-killable job" << job << "registered - this might lead to crashes on shutdown.";
    }

    if (!d->jobs.contains(job)) {
        QAction* stopJobAction = nullptr;
        if (Core::self()->setupFlags() != Core::NoUi) {
            stopJobAction = new QAction(job->objectName().isEmpty() ? i18n("<%1> Unnamed job", job->staticMetaObject.className()) : job->objectName(), this);
            stopJobAction->setData(QVariant::fromValue(static_cast<void*>(job)));
            d->stopJobsMenu->addAction(stopJobAction);
            connect (stopJobAction, &QAction::triggered, this, &RunController::slotKillJob);

            job->setUiDelegate( new KDialogJobUiDelegate() );
        }

        d->jobs.insert(job, stopJobAction);

        connect( job, &KJob::finished, this, &RunController::finished );
        connect( job, &KJob::destroyed, this, &RunController::jobDestroyed );

        // FIXME percent is a private signal and thus we cannot use new connext syntax
        connect(job, SIGNAL(percent(KJob*,ulong)), this, SLOT(jobPercentChanged()));

        IRunController::registerJob(job);

        emit jobRegistered(job);
    }

    job->start();

    checkState();
}

void KDevelop::RunController::unregisterJob(KJob * job)
{
    IRunController::unregisterJob(job);

    Q_ASSERT(d->jobs.contains(job));

    // Delete the stop job action
    QAction *action = d->jobs.take(job);
    if (action)
        action->deleteLater();

    checkState();

    emit jobUnregistered(job);
}

void KDevelop::RunController::checkState()
{
    bool running = false;

    int jobCount = 0;
    int totalProgress = 0;

    for (auto it = d->jobs.constBegin(), end = d->jobs.constEnd(); it != end; ++it) {
        KJob *job = it.key();

        if (!job->isSuspended()) {
            running = true;

            ++jobCount;
            totalProgress += job->percent();
        }
    }

    d->unityLauncher->setProgressVisible(running);
    if (jobCount > 0) {
        d->unityLauncher->setProgress((totalProgress + 1) / jobCount);
    } else {
        d->unityLauncher->setProgress(0);
    }

    if ( ( d->state != Running ? false : true ) == running ) {
        d->state = running ? Running : Idle;
        emit runStateChanged(d->state);
    }

    if (Core::self()->setupFlags() != Core::NoUi) {
        d->stopAction->setEnabled(running);
        d->stopJobsMenu->setEnabled(running);
    }
}

void KDevelop::RunController::stopAllProcesses()
{
    // composite jobs might remove child jobs, see also:
    // https://bugs.kde.org/show_bug.cgi?id=258904
    // foreach already iterates over a copy
    foreach (KJob* job, d->jobs.keys()) {
        // now we check the real list whether it was deleted
        if (!d->jobs.contains(job))
            continue;
        if (job->capabilities() & KJob::Killable) {
            job->kill(KJob::EmitResult);
        } else {
            qWarning() << "cannot stop non-killable job: " << job;
        }
    }
}

void KDevelop::RunController::slotKillJob()
{
    QAction* action = dynamic_cast<QAction*>(sender());
    Q_ASSERT(action);

    KJob* job = static_cast<KJob*>(qvariant_cast<void*>(action->data()));
    if (job->capabilities() & KJob::Killable)
        job->kill();
}

void KDevelop::RunController::finished(KJob * job)
{
    unregisterJob(job);

    switch (job->error()) {
        case KJob::NoError:
        case KJob::KilledJobError:
        case OutputJob::FailedShownError:
            break;

        default:
        {
            ///WARNING: do *not* use a nested event loop here, it might cause
            ///         random crashes later on, see e.g.:
            ///         https://bugs.kde.org/show_bug.cgi?id=309811
            auto dialog = new QDialog(qApp->activeWindow());
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(i18n("Process Error"));
            auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
            KMessageBox::createKMessageBox(dialog, buttonBox, QMessageBox::Warning,
                                           job->errorString(), QStringList(),
                                           QString(), nullptr, KMessageBox::NoExec);
            dialog->show();
        }
    }
}

void RunController::jobDestroyed(QObject* job)
{
    KJob* kjob = static_cast<KJob*>(job);
    if (d->jobs.contains(kjob)) {
        qWarning() << "job destroyed without emitting finished signal!";
        unregisterJob(kjob);
    }
}

void RunController::jobPercentChanged()
{
    checkState();
}

void KDevelop::RunController::suspended(KJob * job)
{
    Q_UNUSED(job);

    checkState();
}

void KDevelop::RunController::resumed(KJob * job)
{
    Q_UNUSED(job);

    checkState();
}

QList< KJob * > KDevelop::RunController::currentJobs() const
{
    return d->jobs.keys();
}

QList<ILaunchConfiguration*> RunController::launchConfigurations() const
{
    QList<ILaunchConfiguration*> configs;
    foreach (LaunchConfiguration *config, launchConfigurationsInternal())
        configs << config;
    return configs;
}

QList<LaunchConfiguration*> RunController::launchConfigurationsInternal() const
{
    return d->launchConfigurations;
}

QList<LaunchConfigurationType*> RunController::launchConfigurationTypes() const
{
    return d->launchConfigurationTypes.values();
}

void RunController::addConfigurationType( LaunchConfigurationType* type )
{
    if( !d->launchConfigurationTypes.contains( type->id() ) )
    {
        d->launchConfigurationTypes.insert( type->id(), type );
    }
}

void RunController::removeConfigurationType( LaunchConfigurationType* type )
{
    foreach( LaunchConfiguration* l, d->launchConfigurations )
    {
        if( l->type() == type )
        {
            removeLaunchConfigurationInternal( l );
        }
    }
    d->launchConfigurationTypes.remove( type->id() );
}

void KDevelop::RunController::addLaunchMode(KDevelop::ILaunchMode* mode)
{
    if( !d->launchModes.contains( mode->id() ) )
    {
        d->launchModes.insert( mode->id(), mode );
    }
}

QList< KDevelop::ILaunchMode* > KDevelop::RunController::launchModes() const
{
    return d->launchModes.values();
}

void KDevelop::RunController::removeLaunchMode(KDevelop::ILaunchMode* mode)
{
    d->launchModes.remove( mode->id() );
}

KDevelop::ILaunchMode* KDevelop::RunController::launchModeForId(const QString& id) const
{
    QMap<QString,ILaunchMode*>::iterator it = d->launchModes.find( id );
    if( it != d->launchModes.end() )
    {
        return it.value();
    }
    return nullptr;
}

void KDevelop::RunController::addLaunchConfiguration(KDevelop::LaunchConfiguration* l)
{
    if( !d->launchConfigurations.contains( l ) )
    {
        d->addLaunchAction( l );
        d->launchConfigurations << l;
        if( !d->currentTargetAction->currentAction() )
        {
            if( !d->currentTargetAction->actions().isEmpty() )
            {
                d->currentTargetAction->actions().at(0)->setChecked( true );
            }
        }
        connect( l, &LaunchConfiguration::nameChanged, this, &RunController::launchChanged );
    }
}

void KDevelop::RunController::removeLaunchConfiguration(KDevelop::LaunchConfiguration* l)
{
    KConfigGroup launcherGroup;
    if( l->project() ) {
        launcherGroup = l->project()->projectConfiguration()->group( Strings::LaunchConfigurationsGroup() );
    } else {
        launcherGroup = Core::self()->activeSession()->config()->group( Strings::LaunchConfigurationsGroup() );
    }
    QStringList configs = launcherGroup.readEntry( Strings::LaunchConfigurationsListEntry(), QStringList() );
    configs.removeAll( l->configGroupName() );
    launcherGroup.deleteGroup( l->configGroupName() );
    launcherGroup.writeEntry( Strings::LaunchConfigurationsListEntry(), configs );
    launcherGroup.sync();

    removeLaunchConfigurationInternal( l );
}

void RunController::removeLaunchConfigurationInternal(LaunchConfiguration *l)
{
    foreach( QAction* a, d->currentTargetAction->actions() ) {
        if( static_cast<LaunchConfiguration*>( a->data().value<void*>() ) == l ) {
            bool wasSelected = a->isChecked();
            d->currentTargetAction->removeAction( a );
            if( wasSelected && !d->currentTargetAction->actions().isEmpty() ) {
                d->currentTargetAction->actions().at(0)->setChecked( true );
            }
            break;
        }
    }

    d->launchConfigurations.removeAll( l );

    delete l;
}

void KDevelop::RunController::executeDefaultLaunch(const QString& runMode)
{
    if (auto dl = defaultLaunch()) {
        execute(runMode, dl);
    } else {
        qWarning() << "no default launch!";
    }
}

void RunController::setDefaultLaunch(ILaunchConfiguration* l)
{
    foreach( QAction* a, d->currentTargetAction->actions() )
    {
        if( static_cast<ILaunchConfiguration*>( a->data().value<void*>() ) == l )
        {
            a->setChecked(true);
            break;
        }
    }
}

bool launcherNameExists(const QString& name)
{
    foreach(ILaunchConfiguration* config, Core::self()->runControllerInternal()->launchConfigurations()) {
        if(config->name()==name)
            return true;
    }
    return false;
}

QString makeUnique(const QString& name)
{
    if(launcherNameExists(name)) {
        for(int i=2; ; i++) {
            QString proposed = QStringLiteral("%1 (%2)").arg(name).arg(i);
            if(!launcherNameExists(proposed)) {
                return proposed;
            }
        }
    }
    return name;
}

ILaunchConfiguration* RunController::createLaunchConfiguration ( LaunchConfigurationType* type,
                                                                 const QPair<QString,QString>& launcher,
                                                                 IProject* project, const QString& name )
{
    KConfigGroup launchGroup;
    if( project )
    {
        launchGroup = project->projectConfiguration()->group( Strings::LaunchConfigurationsGroup() );

    } else
    {
        launchGroup = Core::self()->activeSession()->config()->group( Strings::LaunchConfigurationsGroup() );

    }
    QStringList configs = launchGroup.readEntry( Strings::LaunchConfigurationsListEntry(), QStringList() );
    uint num = 0;
    QString baseName = QStringLiteral("Launch Configuration");
    while( configs.contains( QStringLiteral( "%1 %2" ).arg( baseName ).arg( num ) ) )
    {
        num++;
    }
    QString groupName = QStringLiteral( "%1 %2" ).arg( baseName ).arg( num );
    KConfigGroup launchConfigGroup = launchGroup.group( groupName );
    QString cfgName = name;
    if( name.isEmpty() )
    {
        cfgName = i18n("New %1 Launcher", type->name() );
        cfgName = makeUnique(cfgName);
    }

    launchConfigGroup.writeEntry(LaunchConfiguration::LaunchConfigurationNameEntry(), cfgName );
    launchConfigGroup.writeEntry(LaunchConfiguration::LaunchConfigurationTypeEntry(), type->id() );
    launchConfigGroup.sync();
    configs << groupName;
    launchGroup.writeEntry( Strings::LaunchConfigurationsListEntry(), configs );
    launchGroup.sync();
    LaunchConfiguration* l = new LaunchConfiguration( launchConfigGroup, project );
    l->setLauncherForMode( launcher.first, launcher.second );
    Core::self()->runControllerInternal()->addLaunchConfiguration( l );
    return l;
}

QItemDelegate * KDevelop::RunController::delegate() const
{
    return d->delegate;
}

ContextMenuExtension RunController::contextMenuExtension ( Context* ctx )
{
    delete d->launchAsMapper;
    d->launchAsMapper = new QSignalMapper( this );
    connect( d->launchAsMapper, static_cast<void(QSignalMapper::*)(int)>(&QSignalMapper::mapped), this, [&] (int id) { d->launchAs(id); } );
    d->launchAsInfo.clear();
    d->contextItem = nullptr;
    ContextMenuExtension ext;
    if( ctx->type() == Context::ProjectItemContext ) {
        KDevelop::ProjectItemContext* prjctx = dynamic_cast<KDevelop::ProjectItemContext*>( ctx );
        if( prjctx->items().count() == 1 )
        {
            ProjectBaseItem* itm = prjctx->items().at( 0 );
            int i = 0;
            foreach( ILaunchMode* mode, d->launchModes )
            {
                KActionMenu* menu = new KActionMenu( i18n("%1 As...", mode->name() ), this );
                foreach( LaunchConfigurationType* type, launchConfigurationTypes() )
                {
                    bool hasLauncher = false;
                    foreach( ILauncher* launcher, type->launchers() )
                    {
                        if( launcher->supportedModes().contains( mode->id() ) )
                        {
                            hasLauncher = true;
                        }
                    }
                    if( hasLauncher && type->canLaunch(itm) )
                    {
                        d->launchAsInfo[i] = qMakePair( type->id(), mode->id() );
                        QAction* act = new QAction( d->launchAsMapper );
                        act->setText( type->name() );
                        qCDebug(SHELL) << "Setting up mapping for:" << i << "for action" << act->text() << "in mode" << mode->name();
                        d->launchAsMapper->setMapping( act, i );
                        connect( act, &QAction::triggered, d->launchAsMapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map) );
                        menu->addAction(act);
                        i++;
                    }
                }
                if( menu->menu()->actions().count() > 0 )
                {
                    ext.addAction( ContextMenuExtension::RunGroup, menu);
                }
            }
            if( ext.actions( ContextMenuExtension::RunGroup ).count() > 0 )
            {
                d->contextItem = itm;
            }
        }
    }
    return ext;
}



RunDelegate::RunDelegate( QObject* parent )
: QItemDelegate(parent), runProviderBrush( KColorScheme::View, KColorScheme::PositiveText ),
errorBrush( KColorScheme::View, KColorScheme::NegativeText )
{
}

void RunDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QStyleOptionViewItem opt = option;
//     if( status.isValid() && status.canConvert<KDevelop::IRunProvider::OutputTypes>() )
//     {
//         IRunProvider::OutputTypes type = status.value<KDevelop::IRunProvider::OutputTypes>();
//         if( type == IRunProvider::RunProvider )
//         {
//             opt.palette.setBrush( QPalette::Text, runProviderBrush.brush( option.palette ) );
//         } else if( type == IRunProvider::StandardError )
//         {
//             opt.palette.setBrush( QPalette::Text, errorBrush.brush( option.palette ) );
//         }
//     }
    QItemDelegate::paint(painter, opt, index);
}


#include "moc_runcontroller.cpp"
