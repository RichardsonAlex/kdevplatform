/* This file is part of KDevelop
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

#ifndef KDEVPLATFORM_PROJECTPROXYMODEL_H
#define KDEVPLATFORM_PROJECTPROXYMODEL_H

#include <QSortFilterProxyModel>
#include "projectexport.h"

#include <QList>
#include <QSharedPointer>
#include <QRegExp>
#include <QHash>

namespace KDevelop {
    class ProjectModel;
    class ProjectBaseItem;
}

class KDEVPLATFORMPROJECT_EXPORT ProjectProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    public:
        explicit ProjectProxyModel(QObject *parent);
        bool lessThan (const QModelIndex & left, const QModelIndex & right) const override;

        QModelIndex proxyIndexFromItem(KDevelop::ProjectBaseItem* item) const;
        KDevelop::ProjectBaseItem* itemFromProxyIndex(const QModelIndex&) const;

        void showTargets(bool visible);

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    private:
        KDevelop::ProjectModel* projectModel() const;
        bool m_showTargets;

};

#endif
