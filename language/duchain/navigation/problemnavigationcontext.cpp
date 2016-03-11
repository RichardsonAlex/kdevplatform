/*
   Copyright 2009 David Nolden <david.nolden.kdevelop@art-master.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "problemnavigationcontext.h"
#include <QListView>

#include "util/debug.h"

#include <QHBoxLayout>
#include <QMenu>

#include <KIconLoader>
#include <KLocalizedString>

#include <language/duchain/declaration.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/problem.h>
#include <util/richtextpushbutton.h>

using namespace KDevelop;

namespace {

QString KEY_START_ASSISTANT() { return QStringLiteral("start_assistant"); }
QString KEY_INVOKE_ACTION(int num) { return QStringLiteral("invoke_action_%1").arg(num); }

QString iconForSeverity(IProblem::Severity severity)
{
  KIconLoader loader;
  switch (severity) {
  case IProblem::Hint:
    return loader.iconPath(QStringLiteral("dialog-information"), KIconLoader::Small);
  case IProblem::Warning:
    return loader.iconPath(QStringLiteral("dialog-warning"), KIconLoader::Small);
  case IProblem::Error:
    return loader.iconPath(QStringLiteral("dialog-error"), KIconLoader::Small);
  }
  return {};
}

}


KDevelop::AssistantNavigationContext::AssistantNavigationContext(const IAssistant::Ptr& assistant)
  : m_assistant(assistant)
{
}

KDevelop::AssistantNavigationContext::~AssistantNavigationContext()
{
}


QString KDevelop::AssistantNavigationContext::name() const
{
  return i18n("Assistant");
}

QString KDevelop::AssistantNavigationContext::html(bool shorten)
{
  clear();
  m_shorten = shorten;

  modifyHtml() += i18n("Solutions:") + "<br/>";

  int index = 0;
  foreach (auto assistantAction, m_assistant->actions()) {
    makeLink(i18n("Apply %1", index + 1), KEY_INVOKE_ACTION(index),
             NavigationAction(KEY_INVOKE_ACTION(index)));
    modifyHtml() += ": " + assistantAction->description().toHtmlEscaped() + "<br/>";
    ++index;
  }

  return currentHtml();
}

NavigationContextPointer AssistantNavigationContext::executeKeyAction(QString key)
{
  if (key.startsWith(QLatin1String("invoke_action_"))) {
    if (!m_assistant)
      return {};

    const auto index = key.replace(QLatin1String("invoke_action_"), QString()).toInt();
    auto action = m_assistant->actions().value(index);
    if (action) {
      action->execute();
    } else {
      qCWarning(LANGUAGE()) << "Action got removed in-between";
      return {};
    }
  }

  return {};
}


ProblemNavigationContext::ProblemNavigationContext(const IProblem::Ptr& problem)
  : m_problem(problem)
  , m_widget(nullptr)
{
}

ProblemNavigationContext::~ProblemNavigationContext()
{
  delete m_widget;
}

QWidget* ProblemNavigationContext::widget() const
{
    return m_widget;
}

bool ProblemNavigationContext::isWidgetMaximized() const
{
    return false;
}

QString ProblemNavigationContext::name() const
{
  return i18n("Problem");
}

QString ProblemNavigationContext::html(bool shorten)
{
  // TODO: Remove me
  static const bool inlineSolutions = true;

  clear();
  m_shorten = shorten;
  auto iconPath = iconForSeverity(m_problem->severity());

  modifyHtml() += QStringLiteral("<html><body>");

  modifyHtml() += QStringLiteral("<table><tr>");

  modifyHtml() += QStringLiteral("<td><img width='32' height='32' src='%1'/></td>").arg(iconPath);

  // BEGIN: right column
  modifyHtml() += QStringLiteral("<td>").arg(iconPath);

  modifyHtml() += i18n("Problem in <i>%1</i> ", m_problem->sourceString());
  modifyHtml() += QStringLiteral("<br/>");

  modifyHtml() += m_problem->description().toHtmlEscaped();
  modifyHtml() += QStringLiteral("<br/>");
  modifyHtml() += "<i style=\"white-space:pre-wrap\">" + m_problem->explanation().toHtmlEscaped() + "</i>";

  auto assistant = m_problem->solutionAssistant();
  if (assistant && !assistant->actions().isEmpty()) {
    if (inlineSolutions) {
      modifyHtml() += "<br/>";

      int index = 0;
      foreach (auto assistantAction, assistant->actions()) {
        makeLink(i18n("Solution (%1)", index + 1), KEY_INVOKE_ACTION(index),
                 NavigationAction(KEY_INVOKE_ACTION(index)));
        modifyHtml() += ": " + assistantAction->description().toHtmlEscaped() + "<br/>";
        ++index;
      }
    } else {
      makeLink(i18np("Start Assistant (%1 solution)", "Start Assistant (%1 solutions)",
               assistant->actions().count()), KEY_START_ASSISTANT(),
               NavigationAction(KEY_START_ASSISTANT()));
    }
    modifyHtml() += QStringLiteral("<br/>");
  }

  const QVector<IProblem::Ptr> diagnostics = m_problem->diagnostics();
  if (!diagnostics.isEmpty()) {
    modifyHtml() += QStringLiteral("<br/>");

    DUChainReadLocker lock;
    for (auto diagnostic : diagnostics) {
      modifyHtml() += labelHighlight(QStringLiteral("%1: ").arg(diagnostic->severityString()));
      modifyHtml() += diagnostic->description();

      const DocumentRange range = diagnostic->finalLocation();
      Declaration* declaration = DUChainUtils::itemUnderCursor(range.document.toUrl(), range.start());
      if (declaration) {
        modifyHtml() += i18n("<br/>See: ");
        makeLink(declaration->toString(), DeclarationPointer(declaration), NavigationAction::NavigateDeclaration);
        modifyHtml() += i18n(" in ");
        makeLink(QStringLiteral("%1 :%2")
                  .arg(declaration->url().toUrl().fileName())
                  .arg(declaration->rangeInCurrentRevision().start().line() + 1),
                 DeclarationPointer(declaration), NavigationAction::NavigateDeclaration);
      } else if (range.start().isValid()) {
        modifyHtml() += i18n("<br/>See: ");
        const auto url = range.document.toUrl();
        makeLink(QStringLiteral("%1 :%2")
                   .arg(url.fileName())
                   .arg(range.start().line() + 1),
                 url.toDisplayString(QUrl::PreferLocalFile), NavigationAction(url, range.start()));
      }
      modifyHtml() += QStringLiteral("<br/>");
    }
  }

  modifyHtml() += QStringLiteral("</td>");
  // END: right column

  modifyHtml() += QStringLiteral("</tr></table></body></html>");
  return currentHtml();
}

NavigationContextPointer ProblemNavigationContext::executeKeyAction(QString key)
{
  auto assistant = m_problem->solutionAssistant();
  if (!assistant)
    return {};
  if (key == KEY_START_ASSISTANT()) {
    return NavigationContextPointer(new AssistantNavigationContext(assistant));
  }
  if (key.startsWith(QLatin1String("invoke_action_"))) {
    const auto index = key.replace(QLatin1String("invoke_action_"), QString()).toInt();
    auto action = assistant->actions().value(index);
    if (action) {
      action->execute();
    } else {
      qCWarning(LANGUAGE()) << "Action got removed in-between";
      return {};
    }
  }

  return {};
}
