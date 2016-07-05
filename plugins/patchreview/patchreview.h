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

#ifndef KDEVPLATFORM_PLUGIN_PATCHREVIEW_H
#define KDEVPLATFORM_PLUGIN_PATCHREVIEW_H

#include <QObject>
#include <QPointer>

#include <interfaces/iplugin.h>
#include <interfaces/ipatchsource.h>

class PatchHighlighter;
class PatchReviewToolViewFactory;

class QTimer;

namespace KDevelop {
class IDocument;
}
namespace Sublime {
class Area;
}

namespace Diff2
{
class KompareModelList;
class DiffModel;
}
namespace Kompare
{
struct Info;
}

class DiffSettings;
class PatchReviewPlugin;

class PatchReviewPlugin : public KDevelop::IPlugin, public KDevelop::IPatchReview
{
    Q_OBJECT
    Q_INTERFACES( KDevelop::IPatchReview )

public :
    explicit PatchReviewPlugin( QObject *parent, const QVariantList & = QVariantList() );
    ~PatchReviewPlugin() override;
    void unload() override;

    KDevelop::IPatchSource::Ptr patch() const {
        return m_patch;
    }

    Diff2::KompareModelList* modelList() const {
        return m_modelList.data();
    }

    void seekHunk( bool forwards, const QUrl& file = QUrl() );

    void setPatch( KDevelop::IPatchSource* patch );

    void startReview( KDevelop::IPatchSource* patch, ReviewMode mode ) override;

    void finishReview( QList< QUrl > selection );

    QUrl urlForFileModel( const Diff2::DiffModel* model );
    QAction* finishReviewAction() const { return m_finishReview; }

    KDevelop::ContextMenuExtension contextMenuExtension( KDevelop::Context* context ) override;

Q_SIGNALS:
    void startingNewReview();
    void patchChanged();

public Q_SLOTS :
    //Does parts of the review-starting that are problematic to do directly in startReview, as they may open dialogs etc.
    void updateReview();

    void cancelReview();
    void clearPatch( QObject* patch );
    void notifyPatchChanged();
    void highlightPatch();
    void updateKompareModel();
    void forceUpdate();
    void areaChanged(Sublime::Area* area);
    void executeFileReviewAction();

private Q_SLOTS :
    void documentClosed( KDevelop::IDocument* );
    void textDocumentCreated( KDevelop::IDocument* );
    void documentSaved( KDevelop::IDocument* );
    void closeReview();

private:
    void switchToEmptyReviewArea();

    /// Makes sure that this working set is active only in the @p area, and that its name starts with "review".
    void setUniqueEmptyWorkingSet(Sublime::Area* area);

    void addHighlighting( const QUrl& file, KDevelop::IDocument* document = nullptr );
    void removeHighlighting( const QUrl& file = QUrl() );

    KDevelop::IPatchSource::Ptr m_patch;

    QTimer* m_updateKompareTimer;

    PatchReviewToolViewFactory* m_factory;
    QAction* m_finishReview;

    #if 0
    void determineState();
    #endif

    QPointer< DiffSettings > m_diffSettings;
    QScopedPointer< Kompare::Info > m_kompareInfo;
    QScopedPointer< Diff2::KompareModelList > m_modelList;
    uint m_depth = 0; // depth of the patch represented by m_modelList
    typedef QMap< QUrl, QPointer< PatchHighlighter > > HighlightMap;
    HighlightMap m_highlighters;

    friend class PatchReviewToolView; // to access slot exporterSelected();
};

#endif

// kate: space-indent on; indent-width 2; tab-width 2; replace-tabs on
