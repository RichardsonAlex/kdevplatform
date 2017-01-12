/*
 * This file is part of KDevelop
 * Copyright 2010 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "standarddocumentationview.h"
#include "documentationfindwidget.h"
#include "debug.h"
#include <KIO/TransferJob>

#include <QFontDatabase>

using namespace KDevelop;

StandardDocumentationView::StandardDocumentationView(DocumentationFindWidget* findWidget, QWidget* parent)
    : KDevDocumentationView(parent)
{
    findWidget->setEnabled(true);
    connect(findWidget, &DocumentationFindWidget::newSearch, this, &StandardDocumentationView::search);

    QFont sansSerifFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    QFont monospaceFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QFont minimalFont = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);

#if HAVE_QTWEBKIT
    QWebSettings* s = settings();

    s->setFontFamily(QWebSettings::StandardFont, sansSerifFont.family());
    s->setFontFamily(QWebSettings::SerifFont, "Serif");
    s->setFontFamily(QWebSettings::SansSerifFont, sansSerifFont.family());
    s->setFontFamily(QWebSettings::FixedFont, monospaceFont.family());

    s->setFontSize(QWebSettings::DefaultFontSize, QFontInfo(sansSerifFont).pixelSize());
    s->setFontSize(QWebSettings::DefaultFixedFontSize, QFontInfo(monospaceFont).pixelSize());
    s->setFontSize(QWebSettings::MinimumFontSize, QFontInfo(minimalFont).pixelSize());

    // Fixes for correct positioning. The problem looks like the following:
    //
    // 1) Some page is loaded and loadFinished() signal is emitted,
    //    after this QWebView set right position inside page.
    //
    // 2) After loadFinished() emitting, page JS code finishes it's work and changes
    //    font settings (size). This leads to page contents "moving" inside view widget
    //    and as a result we have wrong position.
    //
    // Such behavior occurs for example with QtHelp pages.
    //
    // To fix the problem, first, we disable view painter updates during load to avoid content
    // "flickering" and also to hide font size "jumping". Secondly, we reset position inside page
    // after loading with using standard QWebFrame method scrollToAnchor().

    connect(this, &QWebView::loadStarted, this, [this]() {
        setUpdatesEnabled(false);
    });

    connect(this, &QWebView::loadFinished, this, [this](bool) {
        if (url().isValid()) {
            page()->mainFrame()->scrollToAnchor(url().fragment());
        }
        setUpdatesEnabled(true);
    });
#endif
}

void StandardDocumentationView::search ( const QString& text, DocumentationFindWidget::FindOptions options )
{
    //Highlighting has been commented because it doesn't let me jump around all occurrences
//     page()->findText(QString(), QWebPage::HighlightAllOccurrences);


    KDevDocumentationPage::FindFlags ff = KDevDocumentationPage::FindFlags();

    if(options & DocumentationFindWidget::Previous)
        ff |= KDevDocumentationPage::FindBackward;

    if(options & DocumentationFindWidget::MatchCase)
        ff |= KDevDocumentationPage::FindCaseSensitively;

#if HAVE_QTWEBKIT
    ff |= QWebPage::FindWrapsAroundDocument /*| QWebPage::HighlightAllOccurrences*/;
    page()->findText(text, ff);
#else
    find(text, ff);
#endif
}

void StandardDocumentationView::setDocumentation(const IDocumentation::Ptr& doc)
{
    if(m_doc)
        disconnect(m_doc.data());
    m_doc = doc;
    update();
    if(m_doc)
        connect(m_doc.data(), &IDocumentation::descriptionChanged, this, &StandardDocumentationView::update);
}

void StandardDocumentationView::update()
{
    if (m_doc) {
        auto description = m_doc->description();
        setHtml(description);
    } else {
        qCDebug(DOCUMENTATION) << "calling StandardDocumentationView::update() on an uninitialized view";
    }
}

#if !HAVE_QTWEBKIT
QVariant StandardDocumentationView::loadResource(int type, const QUrl &name) {
    Q_UNUSED(type);
    // Use KIO to load the resources
    auto job = KIO::get(name, KIO::NoReload, KIO::HideProgressInfo);
    QByteArray result;
    connect(job, &KIO::TransferJob::data, this, [&result](KIO::Job*, QByteArray data) {
       result.append(data);
    });
    if (!job->exec()) {
        qWarning() << "Failed to load documentation file" << name;
        return QVariant();
    }
    qDebug() << "Loaded" << name << "with type" << type << "->" << result;
    return QVariant(result);
}
#endif

