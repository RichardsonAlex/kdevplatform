/* This file is part of KDevelop
Copyright 2001-2002 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
Copyright 2001-2002 Bernd Gehrmann <bernd@kdevelop.org>
Copyright 2001 Sandy Meier <smeier@kdevelop.org>
Copyright 2002 Daniel Engelschalt <daniel.engelschalt@gmx.net>
Copyright 2002 Simon Hausmann <hausmann@kde.org>
Copyright 2002-2003 Roberto Raggi <roberto@kdevelop.org>
Copyright 2003 Mario Scalas <mario.scalas@libero.it>
Copyright 2003 Harald Fernengel <harry@kdevelop.org>
Copyright 2003,2006-2007 Hamish Rodda <rodda@kde.org>
Copyright 2004 Alexander Dymo <adymo@kdevelop.org>
Copyright 2006 Adam Treat <treat@kde.org>
Copyright 2007 Andreas Pakulat <apaku@gmx.org>

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

#ifndef CODECONTEXT_H
#define CODECONTEXT_H

#include <interfaces/context.h>

#include "../duchain/duchainpointer.h"

namespace KDevelop
{

/**
A context for definition-use chain objects.
 */
class KDEVPLATFORMLANGUAGE_EXPORT CodeContext: public Context
{
public:
    /**Builds the context.
    @param item The item to build the context from.*/
    CodeContext( const DUChainBasePointer& item );

    /**Destructor.*/
    virtual ~CodeContext();

    /// Returns the type of this context.
    virtual int type() const;

    /**
     * Retrieve the definition-use chain item pointer.
     *
     * The duchain lock is not held when plugins are queried for
     * context menu actions with this context, so you will need to lock
     * it in order to access the duchain.
     *
     * @return The duchain item.
     */
    const DUChainBasePointer& item() const;

private:
    class Private;
    Private *d;

    Q_DISABLE_COPY(CodeContext)
};

}

#endif

