/*  This file is part of KDevelop
    Copyright 2012 Miha Čančula <miha@noughmad.eu>

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

#ifndef KDEVELOP_ICREATECLASSHELPER_H
#define KDEVELOP_ICREATECLASSHELPER_H

#include "../languageexport.h"

namespace KDevelop {

class ClassGenerator;
class ClassIdentifierPage;
class OverridesPage;

/**
 * @brief A language-specific helper interface for creating new classes
 *
 * This interface contains methods that the "create class" dialog cannot determine
 * by itself and which cannot be specified in a template.
 *
 * They are mostly taken from CreateClassAssistant
 **/
class KDEVPLATFORMLANGUAGE_EXPORT ICreateClassHelper
{
public:
    virtual ~ICreateClassHelper();

    /**
     * Should return a new dialog page for setting the class identifier
     **/
    virtual ClassIdentifierPage* identifierPage() = 0;
    /**
     * Should return a new dialog page for choosing overridden virtual methods
     **/
    virtual OverridesPage* overridesPage() = 0;
    /**
     * Should return a new class generator.
     * 
     * To support class generation from templates, the generator should inherit from TemplateClassGenerator.
     **/
    virtual ClassGenerator* generator() = 0;
};

}

#endif // KDEVELOP_ICREATECLASSHELPER_H
