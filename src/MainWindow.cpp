/*
 *   File name: MainWindow.cpp
 *   Summary:	QDirStat main window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QSignalMapper>

#include "MainWindow.h"
#include "Logger.h"
#include "Exception.h"
#include "ExcludeRules.h"
#include "DirTree.h"
#include "DirTreeCache.h"
#include "DataColumns.h"
#include "DebugHelpers.h"


using namespace QDirStat;

using QDirStat::DataColumns;
using QDirStat::DirTreeModel;
using QDirStat::SelectionModel;


MainWindow::MainWindow():
    QMainWindow(),
    _ui( new Ui::MainWindow ),
    _modified( false ),
    _statusBarTimeOut( 3000 ), // millisec
    _treeLevelMapper(0)
{
    _ui->setupUi( this );
    _dirTreeModel = new DirTreeModel( this );
    CHECK_NEW( _dirTreeModel );

    _selectionModel = new SelectionModel( _dirTreeModel, this );
    CHECK_NEW( _selectionModel );

    _ui->dirTreeView->setModel( _dirTreeModel );
    _ui->dirTreeView->setSelectionModel( _selectionModel );

    _ui->treemapView->setDirTree( _dirTreeModel->tree() );
    _ui->treemapView->setSelectionModel( _selectionModel );


    connect( _dirTreeModel->tree(),	SIGNAL( finished()	  ),
	     this,			SLOT  ( readingFinished() ) );

    connect( _dirTreeModel->tree(),	SIGNAL( startingReading() ),
	     this,			SLOT  ( updateActions()	  ) );

    connect( _dirTreeModel->tree(),	SIGNAL( finished()	  ),
	     this,			SLOT  ( updateActions()	  ) );

    connect( _dirTreeModel->tree(),	SIGNAL( aborted()	  ),
	     this,			SLOT  ( updateActions()	  ) );

    connect( _dirTreeModel->tree(),	SIGNAL( progressInfo( QString ) ),
	     this,			SLOT  ( showProgress( QString ) ) );


    // Debug connections

    connect( _ui->dirTreeView, SIGNAL( clicked	  ( QModelIndex ) ),
	     this,	       SLOT  ( itemClicked( QModelIndex ) ) );

    connect( _selectionModel, SIGNAL( selectionChanged() ),
	     this,	      SLOT  ( selectionChanged() ) );

    connect( _selectionModel, SIGNAL( currentItemChanged( FileInfo *, FileInfo * ) ),
	     this,	      SLOT  ( currentItemChanged( FileInfo *, FileInfo * ) ) );

#if 0
    connect( _selectionModel, SIGNAL( currentChanged( QModelIndex, QModelIndex	) ),
	     this,	      SLOT  ( currentChanged( QModelIndex, QModelIndex	) ) );

    connect( _selectionModel, SIGNAL( selectionChanged( QItemSelection, QItemSelection ) ),
	     this,	      SLOT  ( selectionChanged( QItemSelection, QItemSelection ) ) );
#endif

    connectActions();

    ExcludeRules::add( ".*/\\.snapshot$" );
#if 0
    ExcludeRules::add( ".*/\\.git$" );
#endif

    updateActions();
}


MainWindow::~MainWindow()
{
    // Relying on the QObject hierarchy to properly clean this up resulted in a
    //	segfault; there was probably a problem in the deletion order.
    delete _ui->dirTreeView;
    delete _selectionModel;
    delete _dirTreeModel;
}


void MainWindow::connectActions()
{
    //
    // "File" menu
    //

    connect( _ui->actionOpen,		SIGNAL( triggered()  ),
	     this,			SLOT  ( askOpenUrl() ) );

    connect( _ui->actionRefreshAll,	SIGNAL( triggered()  ),
	     this,			SLOT  ( refreshAll() ) );

    connect( _ui->actionStopReading,	SIGNAL( triggered()   ),
	     this,			SLOT  ( stopReading() ) );

    connect( _ui->actionAskWriteCache,	SIGNAL( triggered()	),
	     this,			SLOT  ( askWriteCache() ) );

    connect( _ui->actionAskReadCache,	SIGNAL( triggered()	),
	     this,			SLOT  ( askReadCache()	) );

    connect( _ui->actionQuit,		SIGNAL( triggered() ),
	     qApp,			SLOT  ( quit()	    ) );

    //
    // "View" menu
    //

    _treeLevelMapper = new QSignalMapper( this );

    connect( _treeLevelMapper, SIGNAL( mapped		( int ) ),
	     this,	       SLOT  ( expandTreeToLevel( int ) ) );

    mapTreeExpandAction( _ui->actionExpandTreeLevel0, 0 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel1, 1 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel2, 2 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel3, 3 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel4, 4 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel5, 5 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel6, 6 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel7, 7 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel8, 8 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel9, 9 );

    mapTreeExpandAction( _ui->actionCloseAllTreeLevels, 0 );
}


void MainWindow::mapTreeExpandAction( QAction * action, int level )
{
    connect( action,	       SIGNAL( triggered() ),
	     _treeLevelMapper, SLOT  ( map()	   ) );
    _treeLevelMapper->setMapping( action, level );
}


void MainWindow::updateActions()
{
    bool reading = _dirTreeModel->tree()->isBusy();

    _ui->actionStopReading->setEnabled( reading );
    _ui->actionRefreshAll->setEnabled	( ! reading );
    _ui->actionAskReadCache->setEnabled ( ! reading );
    _ui->actionAskWriteCache->setEnabled( ! reading );
}


void MainWindow::closeEvent( QCloseEvent *event )
{
    if ( _modified )
    {
	int button = QMessageBox::question( this, tr( "Unsaved changes" ),
					    tr( "Save changes?" ),
					    QMessageBox::Save |
					    QMessageBox::Discard |
					    QMessageBox::Cancel );

	if ( button == QMessageBox::Cancel )
	{
	    event->ignore();
	    return;
	}

	if ( button == QMessageBox::Save )
	{
	    // saveFile();
	}

	event->accept();
    }
    else
    {
	event->accept();
    }
}


void MainWindow::openUrl( const QString & url )
{
    _dirTreeModel->openUrl( url );
    updateActions();
    expandTreeToLevel( 1 );
}


void MainWindow::askOpenUrl()
{
    QString url = QFileDialog::getExistingDirectory( this, // parent
						     tr("Select directory to scan") );
    if ( ! url.isEmpty() )
	openUrl( url );
}


void MainWindow::refreshAll()
{
    QString url = _dirTreeModel->tree()->url();

    if ( ! url.isEmpty() )
    {
	logDebug() << "Refreshing " << url << endl;
	_dirTreeModel->openUrl( url );
	updateActions();
    }
    else
    {
	askOpenUrl();
    }
}


void MainWindow::stopReading()
{
    if ( _dirTreeModel->tree()->isBusy() )
    {
	_dirTreeModel->tree()->abortReading();
	_ui->statusBar->showMessage( tr( "Reading aborted." ) );
    }
}


void MainWindow::askReadCache()
{
    QString fileName = QFileDialog::getOpenFileName( this, // parent
						     tr( "Select QDirStat cache file" ),
						     DEFAULT_CACHE_NAME );
    if ( ! fileName.isEmpty() )
    {
	_dirTreeModel->clear();
	_dirTreeModel->tree()->readCache( fileName );
    }
}


void MainWindow::askWriteCache()
{
    QString fileName = QFileDialog::getSaveFileName( this, // parent
						     tr( "Enter name for QDirStat cache file"),
						     DEFAULT_CACHE_NAME );
    if ( ! fileName.isEmpty() )
    {
	bool ok = _dirTreeModel->tree()->writeCache( fileName );

	QString msg = ok ? tr( "Directory tree written to file %1" ).arg( fileName ) :
			   tr( "ERROR writing cache file %1").arg( fileName );
	_ui->statusBar->showMessage( msg, _statusBarTimeOut );
    }
}


void MainWindow::expandTreeToLevel( int level )
{
    if ( level < 1 )
	_ui->dirTreeView->collapseAll();
    else
	_ui->dirTreeView->expandToDepth( level - 1 );
}


void MainWindow::showProgress( const QString & text )
{
    _ui->statusBar->showMessage( text, _statusBarTimeOut );
}


void MainWindow::notImplemented()
{
    QMessageBox::warning( this, tr( "Error" ), tr( "Not implemented!" ) );
}


void MainWindow::readingFinished()
{
    logDebug() << endl;
    _ui->statusBar->showMessage( tr( "Ready.") );
    expandTreeToLevel( 1 );
    int sortCol = QDirStat::DataColumns::toViewCol( QDirStat::TotalSizeCol );
    _ui->dirTreeView->sortByColumn( sortCol, Qt::DescendingOrder );

    // Debug::dumpModelTree( _dirTreeModel, QModelIndex(), "" );
}


void MainWindow::itemClicked( const QModelIndex & index )
{
    if ( index.isValid() )
    {
	FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );

	logDebug() << "Clicked row " << index.row()
		   << " col " << index.column()
		   << " (" << QDirStat::DataColumns::fromViewCol( index.column() ) << ")"
		   << "\t" << item
		   << endl;
	// << " data(0): " << index.model()->data( index, 0 ).toString()
	// logDebug() << "Ancestors: " << Debug::modelTreeAncestors( index ).join( " -> " ) << endl;
    }
    else
    {
	logDebug() << "Invalid model index" << endl;
    }

    // _dirTreeModel->dumpPersistentIndexList();
}


void MainWindow::selectionChanged()
{
    logDebug() << endl;
    _selectionModel->dumpSelectedItems();
}


void MainWindow::currentItemChanged( FileInfo * newCurrent, FileInfo * oldCurrent )
{
    logDebug() << "new current: " << newCurrent << endl;
    logDebug() << "old current: " << oldCurrent << endl;
    _selectionModel->dumpSelectedItems();
}


void MainWindow::currentChanged( const QModelIndex & newCurrent,
				 const QModelIndex & oldCurrent )
{
    logDebug() << "new current: " << newCurrent << endl;
    logDebug() << "old current: " << oldCurrent << endl;
    _selectionModel->dumpSelectedItems();
}


void MainWindow::selectionChanged( const QItemSelection & selected,
				   const QItemSelection & deselected )
{
    logDebug() << endl;
#if 0
    foreach ( const QModelIndex index, selected.indexes() )
    {
	logDebug() << "Selected: " << index << endl;
    }

    foreach ( const QModelIndex index, deselected.indexes() )
    {
	logDebug() << "Deselected: " << index << endl;
    }
#else
    Q_UNUSED( selected );
    Q_UNUSED( deselected );
#endif

    _selectionModel->dumpSelectedItems();
}
