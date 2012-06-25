/*
 * This file is part of KDevelop
 * Copyright 2012 Miha Čančula <miha@noughmad.eu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KDEVELOP_TEMPLATERENDERER_H
#define KDEVELOP_TEMPLATERENDERER_H

#include <QVariantHash>
#include <QStringList>

#include "../languageexport.h"

namespace Grantlee
{
class Engine;
}

class KUrl;
class KArchiveDirectory;

namespace KDevelop
{

/**
 * @brief Convenience class for rendering multiple templates with the same context
 * 
 * The TemplateRenderer provides easy access to common template operations.
 * Internally, it encapsulates a Grantlee::Engine and a Grantlee::Context.
 **/
class KDEVPLATFORMLANGUAGE_EXPORT TemplateRenderer
{
public:
    TemplateRenderer();
    virtual ~TemplateRenderer();

    /**
     * Provides access to the encapsulated Grantlee::Engine. 
     *
     **/
    Grantlee::Engine* engine();

    /**
     * Adds @p directories to the list of directories searched for templates
     *
     **/
    void addTemplateDirectories(const QStringList& directories);
    /**
     * Adds the archive @p directory to the list of places searched for templates
     * 
     **/
    void addArchive(const KArchiveDirectory* directory);

    /**
     * Adds @p variables to the Grantlee::Context passed to each template.
     * 
     * If the context already contains a variables with the same name as a key in @p variables,
     * it is overwritten.
     * 
     **/
    void addVariables(const QVariantHash& variables);

    /**
     * Adds variable with name @p name and value @p value to the Grantlee::Context passed to each template.
     * 
     * If the context already contains a variables with the same @p name, it is overwritten.
     * 
     **/
    void addVariable(const QString& name, const QVariant& value);
    
    /**
     * @brief Renders a single template
     *
     * @param content the template content
     * @param name (optional) the name of this template
     * @return the rendered template
     **/
    QString render(const QString& content, const QString& name = QString());

    /**
     * @brief Renders a single template from a file
     *
     * @param url the URL of the file from which to load the template
     * @param name (optional) the name of this template
     * @return the rendered template
     **/
    QString renderFile(const KUrl& url, const QString& name = QString());

    /**
     * @brief Renders a list of templates
     *
     * @param content the template contents
     * @return the rendered templates
     **/
    QStringList render(const QStringList& contents);

private:
    class TemplateRendererPrivate* const d;
};

}

#endif // KDEVELOP_TEMPLATERENDERER_H
