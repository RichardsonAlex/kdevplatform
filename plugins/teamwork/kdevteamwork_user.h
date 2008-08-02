/***************************************************************************
  Copyright 2006 David Nolden <david.nolden.kdevelop@art-master.de>
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KDEVTEAMWORK_USER_H
#define KDEVTEAMWORK_USER_H

#include <QObject>
#include <QMetaType>
#include <QString>
#include  <kiconloader.h>

#include "lib/network/networkfwd.h"
#include "teamworkfwd.h"
#include "lib/network/user.h"

using namespace Teamwork;
class QIcon;

namespace Teamwork {
  class IdentificationMessage;
}

class KDevTeamworkUser : public QObject, public User {
    Q_OBJECT
  public:
    KDevTeamworkUser( const User* user = 0 );

    KDevTeamworkUser( IdentificationMessage* msg );

    virtual void setSession( const SessionPointer& sess );

    QIcon icon( KIconLoader::Group = KIconLoader::Small );

    QString toolTip();

    template <class Archive>
    void serialize( Archive& arch, const uint /*version*/ ) {
      arch & boost::serialization::base_object<User>( *this );
    }

  signals:
    ///All signals in this class should be connected using queued connections, because they get called from other threads
    void userStateChanged( KDevTeamworkUserPointer user );
};

typedef SafeSharedPtr<KDevTeamworkUser, BoostSerialization> KDevTeamworkUserPointer;

#endif

// kate: space-indent on; indent-width 2; tab-width 2; replace-tabs on
