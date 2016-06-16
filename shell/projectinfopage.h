/***************************************************************************
 *   Copyright (C) 2008 by Andreas Pakulat <apaku@gmx.de                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KDEVPLATFORM_PROJECTINFOPAGE_H
#define KDEVPLATFORM_PROJECTINFOPAGE_H

#include <QWidget>

struct ProjectFileChoice {
    QString text;
    QString pluginId;
    QString iconName;
};

namespace Ui
{
    class ProjectInfoPage;
}

namespace KDevelop
{

class ProjectInfoPage : public QWidget
{
Q_OBJECT
public:
    explicit ProjectInfoPage( QWidget* parent = nullptr );
    ~ProjectInfoPage() override;
    void setProjectName( const QString& );
    void populateProjectFileCombo( const QVector<ProjectFileChoice>& choices );
signals:
    void projectNameChanged( const QString& );
    void projectManagerChanged( const QString& );
private slots:
    void changeProjectManager( int );
private:
    Ui::ProjectInfoPage* page_ui;
};

}

#endif
