
#include "ConsoleWindow.h"
#include "Util.h"
#include "MainApp.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QCloseEvent>


ConsoleWindow::ConsoleWindow( QWidget *p, Qt::WindowFlags f )
    : QMainWindow( p, f ), nLines(0), maxLines(2000)
{
    MainApp *app = mainApp();

// Central text

    QTextEdit *te = new QTextEdit( this );
    te->installEventFilter( this );
    te->setUndoRedoEnabled( false );
    te->setReadOnly( !app->isLogEditable() );
    setCentralWidget( te );

// Menus

    app->act.initMenus( this );

// Statusbar

    statusBar()->showMessage( QString::null );
}

/* ---------------------------------------------------------------- */
/* Slots ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Note that append automatically adds trailing '\n'.
//
void ConsoleWindow::logAppendText( const QString &txt, const QColor &clr )
{
    QTextEdit   *te     = textEdit();
    QColor      clr0    = te->textColor();

// ------------
// Add new text
// ------------

    te->setTextColor( clr );
    te->append( txt );

// ----------------------------------
// Limit growth - remove oldest lines
// ----------------------------------

    nLines += 1 + txt.count( '\n' );

    if( nLines >= maxLines ) {

        int         n2del   = MAX( maxLines / 10, nLines - maxLines );
        QTextCursor cursor  = te->textCursor();

        cursor.movePosition( QTextCursor::Start );
        cursor.movePosition(
            QTextCursor::Down,
            QTextCursor::KeepAnchor,
            n2del );

        cursor.removeSelectedText();
        nLines -= n2del;
    }

// --------------
// Normalize view
// --------------

    te->setTextColor( clr0 );
    te->moveCursor( QTextCursor::End );
    te->ensureCursorVisible();
}


void ConsoleWindow::saveToFile()
{
    QString fn = QFileDialog::getSaveFileName(
                    this,
                    "Save log as text file",
                    mainApp()->runDir(),
                    "Text files (*.txt)" );

    if( fn.length() ) {

        QFile   f( fn );

        if( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {

            QTextStream ts( &f );

            ts << textEdit()->toPlainText();
        }
    }
}

/* ---------------------------------------------------------------- */
/* Protected ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Force Ctrl+A events to be treated as 'show AO-dialog',
// instead of 'text-field select-all'.
//
bool ConsoleWindow::eventFilter( QObject *watched, QEvent *event )
{
    if( event->type() == QEvent::KeyPress ) {

        QKeyEvent   *e = static_cast<QKeyEvent*>(event);

        if( e->modifiers() == Qt::ControlModifier ) {

            if( e->key() == Qt::Key_A ) {
                mainApp()->act.aoDlgAct->trigger();
                e->ignore();
                return true;
            }
        }
    }

    return QMainWindow::eventFilter( watched, event );
}


// Console close box clicked.
//
void ConsoleWindow::closeEvent( QCloseEvent *e )
{
    e->ignore();
    mainApp()->act.quitAct->trigger();
}


