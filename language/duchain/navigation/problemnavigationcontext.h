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

#ifndef KDEVPLATFORM_PROBLEMNAVIGATIONCONTEXT_H
#define KDEVPLATFORM_PROBLEMNAVIGATIONCONTEXT_H

#include <interfaces/iproblem.h>
#include <interfaces/iassistant.h>

#include <language/duchain/navigation/abstractnavigationcontext.h>
#include <language/languageexport.h>
#include <qpointer.h>

namespace KDevelop {

/// TODO: Kill me?
class KDEVPLATFORMLANGUAGE_EXPORT AssistantNavigationContext : public AbstractNavigationContext
{
  Q_OBJECT
  public:
    explicit AssistantNavigationContext(const IAssistant::Ptr& assistant);
    ~AssistantNavigationContext();

    virtual QString name() const override;
    virtual QString html(bool shorten = false) override;

    NavigationContextPointer executeKeyAction(QString key) override;

  private:
    IAssistant::Ptr m_assistant;
};

class KDEVPLATFORMLANGUAGE_EXPORT ProblemNavigationContext : public AbstractNavigationContext
{
  Q_OBJECT
  public:
    explicit ProblemNavigationContext(const IProblem::Ptr& problem);
    ~ProblemNavigationContext() override;

    QString name() const override;
    QString html(bool shorten = false) override;
    QWidget* widget() const override;
    bool isWidgetMaximized() const override;

    NavigationContextPointer executeKeyAction(QString key) override;

  private:
    IProblem::Ptr m_problem;

    QPointer<QWidget> m_widget;
};

}

#endif // KDEVPLATFORM_PROBLEMNAVIGATIONCONTEXT_H
