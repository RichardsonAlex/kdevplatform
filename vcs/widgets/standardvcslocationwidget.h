/* KDevPlatform Vcs Support
 *
 * Copyright 2010 Aleix Pol <aleixpol@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef KDEVPLATFORM_STANDARDVCSLOCATIONWIDGET_H
#define KDEVPLATFORM_STANDARDVCSLOCATIONWIDGET_H

#include <vcs/widgets/vcslocationwidget.h>
#include <vcs/vcsexport.h>

class QUrl;
class KUrlRequester;
namespace KDevelop
{

class KDEVPLATFORMVCS_EXPORT StandardVcsLocationWidget : public VcsLocationWidget
{
    Q_OBJECT
    public:
        explicit StandardVcsLocationWidget(QWidget* parent = nullptr, Qt::WindowFlags f = nullptr);
        VcsLocation location() const override;
        bool isCorrect() const override;
        QUrl url() const;
        QString projectName() const override;
        void setLocation(const QUrl& remoteLocation) override;
        void setUrl(const QUrl& url);

    public slots:
        void textChanged(const QString& str);
        
    private:
        KUrlRequester* m_urlWidget;
};

}
#endif // KDEVPLATFORM_STANDARDVCSLOCATIONWIDGET_H
