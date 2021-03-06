
#include "ui_ChanListDialog.h"

#include "Util.h"
#include "MainApp.h"
#include "ConfigCtl.h"
#include "IMROEditor.h"
#include "GraphsWindow.h"
#include "Run.h"
#include "SVGrafsM_Im.h"

#include <QAction>
#include <QStatusBar>
#include <QSettings>
#include <QMessageBox>


#define MAX10BIT    512


/* ---------------------------------------------------------------- */
/* class SVGrafsM_Im ---------------------------------------------- */
/* ---------------------------------------------------------------- */

SVGrafsM_Im::SVGrafsM_Im( GraphsWindow *gw, DAQ::Params &p )
    :   SVGrafsM( gw, p )
{
    imroAction = new QAction( "Edit Imro...", this );
    imroAction->setEnabled( p.mode.manOvInitOff );
    ConnectUI( imroAction, SIGNAL(triggered()), this, SLOT(editImro()) );

    stdbyAction = new QAction( "Edit On/Off...", this );
    stdbyAction->setEnabled( p.mode.manOvInitOff && p.im.roTbl.opt == 3 );
    ConnectUI( stdbyAction, SIGNAL(triggered()), this, SLOT(editStdby()) );
}


SVGrafsM_Im::~SVGrafsM_Im()
{
    saveSettings();
}


/*  Time Scaling
    ------------
    Each graph has its own wrapping data buffer (yval) but shares
    the time axis span. As fresh data arrive they wrap around such
    that the latest data are present as well as one span's worth of
    past data. We will draw the data using a wipe effect. Older data
    remain visible while they are progressively overwritten by the
    new from left to right. In this mode selection ranges do not
    make sense, nor do precise cursor readouts of time-coordinates.
    Rather, min_x and max_x suggest only the span of depicted data.
*/

#define V_FLT_ADJ( v, d )                                           \
    (set.filterChkOn ? v + fgain*(d[nAP] - dc.lvl[ic+nAP]) : v)

#define V_T_FLT_ADJ( v, d )                                         \
    (V_FLT_ADJ( v, d ) - dc.lvl[ic])

#define V_S_T_FLT_ADJ( d )                                          \
    (set.sAveRadius > 0 ?                                           \
    V_FLT_ADJ( s_t_Ave( d, ic ), d ) : V_T_FLT_ADJ( *d, d ))


void SVGrafsM_Im::putScans( vec_i16 &data, quint64 headCt )
{
#if 0
    double  tProf = getTime();
#endif
    double      ysc     = 1.0 / MAX10BIT;
    const int   nC      = chanCount(),
                nNu     = neurChanCount(),
                nAP     = p.im.imCumTypCnt[CimCfg::imSumAP],
                ntpts   = (int)data.size() / nC,
                dwnSmp  = theX->dwnSmp,
                dstep   = dwnSmp * nC;

// BK: We should superpose traces to see AP & LF, not add.

// -------
// Filters
// -------

    drawMtx.lock();

    if( set.dcChkOn )
        dc.updateLvl( &data[0], ntpts, dwnSmp );

// ---------------------
// Append data to graphs
// ---------------------

    QVector<float>  ybuf( ntpts );  // append en masse

    for( int ic = 0; ic < nC; ++ic ) {

        // -----------------
        // For active graphs
        // -----------------

        if( ic2iy[ic] < 0 )
            continue;

        // ----------
        // Init stats
        // ----------

        // Collect points, update mean, stddev

        GraphStats  &stat = ic2stat[ic];

        stat.clear();

        // ------------------
        // By channel type...
        // ------------------

        qint16  *d  = &data[ic];
        int     ny  = 0;

        if( ic < nAP ) {

            double  fgain = p.im.chanGain( ic ) / p.im.chanGain( ic+nAP );

            // ---------------
            // AP downsampling
            // ---------------

            // Withing each bin, report the greatest
            // amplitude (pos or neg) extremum. This
            // ensures spikes are not missed.

            if( set.binMaxOn && dwnSmp > 1 ) {

                int ndRem = ntpts;

                for( int it = 0; it < ntpts; it += dwnSmp ) {

                    qint16  *D      = d;
                    float   val     = V_S_T_FLT_ADJ( d ),
                            maxSqr  = val * val;
                    int     binWid  = dwnSmp;

                    stat.add( val );

                    d += nC;

                    if( ndRem < binWid )
                        binWid = ndRem;

                    for( int ib = 1; ib < binWid; ++ib, d += nC ) {

                        val = V_S_T_FLT_ADJ( d );

                        float   sqr = val * val;

                        stat.add( val );

                        if( sqr > maxSqr ) {
                            maxSqr  = sqr;
                            D       = d;
                        }
                    }

                    ndRem -= binWid;

                    ybuf[ny++] = V_S_T_FLT_ADJ( D ) * ysc;
                }
            }
            else if( set.sAveRadius > 0 ) {

                for( int it = 0; it < ntpts; it += dwnSmp, d += dstep ) {

                    float   val = V_FLT_ADJ( s_t_Ave( d, ic ), d );

                    stat.add( val );
                    ybuf[ny++] = val * ysc;
                }
            }
            else {

                for( int it = 0; it < ntpts; it += dwnSmp, d += dstep ) {

                    float   val = V_T_FLT_ADJ( *d, d );

                    stat.add( val );
                    ybuf[ny++] = val * ysc;
                }
            }
        }
        else if( ic < nNu ) {

            // ---
            // LFP
            // ---

            for( int it = 0; it < ntpts; it += dwnSmp, d += dstep ) {

                float   val = *d - dc.lvl[ic];

                stat.add( val );
                ybuf[ny++] = val * ysc;
            }
        }
        else {

            // ----
            // Sync
            // ----

            for( int it = 0; it < ntpts; it += dwnSmp, d += dstep )
                ybuf[ny++] = *d;
        }

        // Append points en masse
        // Renormalize x-coords -> consecutive indices.

        theX->dataMtx.lock();
        ic2Y[ic].yval.putData( &ybuf[0], ny );
        theX->dataMtx.unlock();
    }

// -----------------------
// Update pseudo time axis
// -----------------------

    double  span = theX->spanSecs();

    theX->max_x = (headCt + ntpts) / p.im.srate;
    theX->min_x = theX->max_x - span;

// ----
// Draw
// ----

    QMetaObject::invokeMethod( theM, "update", Qt::QueuedConnection );

    drawMtx.unlock();

// ---------
// Profiling
// ---------

#if 0
    tProf = getTime() - tProf;
    Log() << "Graph milis " << 1000*tProf;
#endif
}


int SVGrafsM_Im::chanCount() const
{
    return p.im.imCumTypCnt[CimCfg::imSumAll];
}


int SVGrafsM_Im::neurChanCount() const
{
    return p.im.imCumTypCnt[CimCfg::imSumNeural];
}


bool SVGrafsM_Im::isSelAnalog() const
{
    return selected < p.im.imCumTypCnt[CimCfg::imSumNeural];
}


void SVGrafsM_Im::setRecordingEnabled( bool checked )
{
    imroAction->setEnabled( !checked );
    stdbyAction->setEnabled( !checked && p.im.roTbl.opt == 3 );
}


void SVGrafsM_Im::filterChkClicked( bool checked )
{
    drawMtx.lock();
    set.filterChkOn = checked;
    drawMtx.unlock();

    saveSettings();
}


void SVGrafsM_Im::sAveRadChanged( int radius )
{
    drawMtx.lock();
    set.sAveRadius = radius;
    sAveTable(
        p.sns.imChans.shankMap,
        0, p.im.imCumTypCnt[CimCfg::imSumAP],
        radius );
    drawMtx.unlock();

    saveSettings();
}


void SVGrafsM_Im::mySaveGraphClicked( bool checked )
{
    Q_UNUSED( checked )
}


void SVGrafsM_Im::myMouseOverGraph( double x, double y, int iy )
{
    int     ic          = lastMouseOverChan = theX->Y[iy]->usrChan;
    bool    isNowOver   = true;

    if( ic < 0 || ic >= chanCount() ) {
        gw->statusBar()->clearMessage();
        return;
    }

    QWidget *w = QApplication::widgetAt( QCursor::pos() );

    if( !w || !dynamic_cast<MGraph*>(w) )
        isNowOver = false;

    double      mean, rms, stdev;
    QString     msg;
    const char  *unit,
                *swhere = (isNowOver ? "Mouse over" : "Last mouse-over");
    int         h,
                m;

    h = int(x / 3600);
    x = x - h * 3600;
    m = x / 60;
    x = x - m * 60;

    if( ic < neurChanCount() ) {

        // neural readout

        computeGraphMouseOverVars( ic, y, mean, stdev, rms, unit );

        msg = QString(
            "%1 %2 @ pos (%3h%4m%5s, %6 %7)"
            " -- {mean, rms, stdv} %7: {%8, %9, %10}")
            .arg( swhere )
            .arg( STR2CHR( myChanName( ic ) ) )
            .arg( h, 2, 10, QChar('0') )
            .arg( m, 2, 10, QChar('0') )
            .arg( x, 0, 'f', 3 )
            .arg( y, 0, 'f', 4 )
            .arg( unit )
            .arg( mean, 0, 'f', 4 )
            .arg( rms, 0, 'f', 4 )
            .arg( stdev, 0, 'f', 4 );
    }
    else {

        // sync readout

        msg = QString(
            "%1 %2 @ pos %3h%4m%5s")
            .arg( swhere )
            .arg( STR2CHR( myChanName( ic ) ) )
            .arg( h, 2, 10, QChar('0') )
            .arg( m, 2, 10, QChar('0') )
            .arg( x, 0, 'f', 3 );
    }

    gw->statusBar()->showMessage( msg );
}


void SVGrafsM_Im::myClickGraph( double x, double y, int iy )
{
    myMouseOverGraph( x, y, iy );
    selectChan( lastMouseOverChan );
}


void SVGrafsM_Im::myRClickGraph( double x, double y, int iy )
{
    myClickGraph( x, y, iy );
}


void SVGrafsM_Im::editImro()
{
    int chan = lastMouseOverChan;

    if( chan >= neurChanCount() )
        return;

// Pause acquisition

    if( !mainApp()->getRun()->imecPause( true, false ) )
        return;

// Launch editor

    quint32     pSN = mainApp()->cfgCtl()->imVers.pSN.toUInt();
    IMROEditor  ED( this, pSN, p.im.roTbl.opt );
    QString     imroFile;
    bool        changed = ED.Edit( imroFile, p.im.imroFile, chan );

    if( changed ) {
        mainApp()->cfgCtl()->graphSetsImroFile( imroFile );
        sAveRadChanged( set.sAveRadius );
    }

// Download and resume

    mainApp()->getRun()->imecPause( false, changed );
}


void SVGrafsM_Im::editStdby()
{
    int chan = lastMouseOverChan;

    if( chan >= neurChanCount() )
        return;

// Pause acquisition

    if( !mainApp()->getRun()->imecPause( true, false ) )
        return;

// Launch editor

    QString     stdbyStr;
    bool        changed = stdbyDialog( stdbyStr );

    if( changed ) {
        mainApp()->cfgCtl()->graphSetsStdbyStr( stdbyStr );
        sAveRadChanged( set.sAveRadius );
    }

// Download and resume

    mainApp()->getRun()->imecPause( false, changed );
}


void SVGrafsM_Im::myInit()
{
    theM->addAction( imroAction );
    theM->addAction( stdbyAction );
    theM->setContextMenuPolicy( Qt::ActionsContextMenu );
}


double SVGrafsM_Im::mySampRate() const
{
    return p.im.srate;
}


void SVGrafsM_Im::mySort_ig2ic()
{
    if( set.usrOrder )
        p.sns.imChans.chanMap.userOrder( ig2ic );
    else
        p.sns.imChans.chanMap.defaultOrder( ig2ic );
}


QString SVGrafsM_Im::myChanName( int ic ) const
{
    return p.sns.imChans.chanMap.name( ic, p.isTrigChan( "imec", ic ) );
}


QBitArray& SVGrafsM_Im::mySaveBits() const
{
    return p.sns.imChans.saveBits;
}


// Return type number of digital channels, or -1 if none.
//
int SVGrafsM_Im::mySetUsrTypes()
{
    int c0, cLim;

    c0      = 0;
    cLim    = p.im.imCumTypCnt[CimCfg::imSumAP];

    for( int ic = c0; ic < cLim; ++ic )
        ic2Y[ic].usrType = 0;

    c0      = p.im.imCumTypCnt[CimCfg::imSumAP];
    cLim    = p.im.imCumTypCnt[CimCfg::imSumNeural];

    for( int ic = c0; ic < cLim; ++ic )
        ic2Y[ic].usrType = 1;


    c0      = p.im.imCumTypCnt[CimCfg::imSumNeural];

    ic2Y[c0].usrType = 2;

    return 2;
}


// Called only from init().
//
void SVGrafsM_Im::loadSettings()
{
// -------------------
// Appearance defaults
// -------------------

    STDSETTINGS( settings, "graphs_M_Im" );

    settings.beginGroup( "All" );
    set.secs        = settings.value( "secs", 4.0 ).toDouble();
    set.yscl0       = settings.value( "yscl0", 1.0 ).toDouble();
    set.yscl1       = settings.value( "yscl1", 1.0 ).toDouble();
    set.yscl2       = settings.value( "yscl2", 1.0 ).toDouble();
    set.clr0        = clrFromString( settings.value( "clr0", "ffeedd82" ).toString() );
    set.clr1        = clrFromString( settings.value( "clr1", "ffff5500" ).toString() );
    set.clr2        = clrFromString( settings.value( "clr2", "ff44eeff" ).toString() );
    set.navNChan    = settings.value( "navNChan", 32 ).toInt();
    set.bandSel     = settings.value( "bandSel", 0 ).toInt();
    set.sAveRadius  = settings.value( "sAveRadius", 0 ).toInt();
    set.filterChkOn = settings.value( "filterChkOn", false ).toBool();
    set.dcChkOn     = settings.value( "dcChkOn", false ).toBool();
    set.binMaxOn    = settings.value( "binMaxOn", true ).toBool();
    set.usrOrder    = settings.value( "usrOrder", false ).toBool();
    settings.endGroup();
}


void SVGrafsM_Im::saveSettings() const
{
// -------------------
// Appearance defaults
// -------------------

    STDSETTINGS( settings, "graphs_M_Im" );

    settings.beginGroup( "All" );
    settings.setValue( "secs", set.secs );
    settings.setValue( "yscl0", set.yscl0 );
    settings.setValue( "yscl1", set.yscl1 );
    settings.setValue( "yscl2", set.yscl2 );
    settings.setValue( "clr0", clrToString( set.clr0 ) );
    settings.setValue( "clr1", clrToString( set.clr1 ) );
    settings.setValue( "clr2", clrToString( set.clr2 ) );
    settings.setValue( "navNChan", set.navNChan );
    settings.setValue( "bandSel", set.bandSel );
    settings.setValue( "sAveRadius", set.sAveRadius );
    settings.setValue( "filterChkOn", set.filterChkOn );
    settings.setValue( "dcChkOn", set.dcChkOn );
    settings.setValue( "binMaxOn", set.binMaxOn );
    settings.setValue( "usrOrder", set.usrOrder );
    settings.endGroup();
}


// Values (v) are in range [-1,1].
// (v+1)/2 is in range [0,1].
// This is mapped to range [rmin,rmax].
//
double SVGrafsM_Im::scalePlotValue( double v, double gain ) const
{
    return p.im.range.unityToVolts( (v+1)/2 ) / gain;
}


// Call this only for neural channels!
//
void SVGrafsM_Im::computeGraphMouseOverVars(
    int         ic,
    double      &y,
    double      &mean,
    double      &stdev,
    double      &rms,
    const char* &unit ) const
{
    double  gain = p.im.chanGain( ic );

    y       = scalePlotValue( y, gain );

    drawMtx.lock();

    mean    = scalePlotValue( ic2stat[ic].mean() / MAX10BIT, gain );
    stdev   = scalePlotValue( ic2stat[ic].stdDev() / MAX10BIT, gain );
    rms     = scalePlotValue( ic2stat[ic].rms() / MAX10BIT, gain );

    drawMtx.unlock();

    double  vmax = p.im.range.rmax / (ic2Y[ic].yscl * gain);

    unit = "V";

    if( vmax < 0.001 ) {
        y       *= 1e6;
        mean    *= 1e6;
        stdev   *= 1e6;
        rms     *= 1e6;
        unit     = "uV";
    }
    else if( vmax < 1.0 ) {
        y       *= 1e3;
        mean    *= 1e3;
        stdev   *= 1e3;
        rms     *= 1e3;
        unit     = "mV";
    }
}


bool SVGrafsM_Im::stdbyDialog( QString &stdbyStr )
{
    QDialog             dlg;
    Ui::ChanListDialog  ui;
    bool                changed = false;

    dlg.setWindowFlags( dlg.windowFlags()
        & (~Qt::WindowContextHelpButtonHint
            | Qt::WindowCloseButtonHint) );

    ui.setupUi( &dlg );
    dlg.setWindowTitle( "Turn Off Opt-3 Channels" );

    ui.curLbl->setText( p.im.stdbyStr.isEmpty() ? "all on" : p.im.stdbyStr );
    ui.chansLE->setText( p.im.stdbyStr );

// Run dialog until ok or cancel

    for(;;) {

        if( QDialog::Accepted == dlg.exec() ) {

            CimCfg  im;
            QString err;

            im.stdbyStr = ui.chansLE->text().trimmed();

            if( im.deriveStdbyBits(
                err, p.im.imCumTypCnt[CimCfg::imSumAP] ) ) {

                changed = p.im.stdbyBits != im.stdbyBits;

                if( changed )
                    stdbyStr = im.stdbyStr;

                break;
            }
            else
                QMessageBox::critical( this, "Channels Error", err );
        }
        else
            break;
    }

    return changed;
}


