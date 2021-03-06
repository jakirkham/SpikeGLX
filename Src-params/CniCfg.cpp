
#include "Util.h"
#include "CniCfg.h"
#include "Subset.h"

#ifdef HAVE_NIDAQmx
#include "NI/NIDAQmx.h"
#else
#pragma message("*** Message to self: Building simulated NI-DAQ version ***")
#endif

#include <QSettings>


/* ---------------------------------------------------------------- */
/* Param methods -------------------------------------------------- */
/* ---------------------------------------------------------------- */

double CniCfg::chanGain( int ic ) const
{
    double  g = 1.0;

    if( ic > -1 ) {

        if( ic < niCumTypCnt[niTypeMN] )
            g = mnGain;
        else if( ic < niCumTypCnt[niTypeMA] )
            g = maGain;

        if( g < 0.01 )
            g = 0.01;
    }

    return g;
}


// Given input fields:
// - uiMNStr1, uiMAStr1, uiXAStr1, uiXDStr1
// - uiMNStr2, uiMAStr2, uiXAStr2, uiXDStr2
// - muxFactor
// - isDualDevMode
//
// Derive:
// - niCumTypCnt[]
//
// Note:
// Nobody except the NI-DAQ card reader CniAcqDmx needs to distinguish
// counts of channels that separately come from dev1 and dev2.
//
void CniCfg::deriveChanCounts()
{
    QVector<uint>   vc;

// --------------------------------
// First count each type separately
// --------------------------------

    Subset::rngStr2Vec( vc,
        QString("%1,%2").arg( uiMNStr1 ).arg( uiMNStr2() ) );
    niCumTypCnt[niTypeMN] = vc.size() * muxFactor;

    Subset::rngStr2Vec( vc,
        QString("%1,%2").arg( uiMAStr1 ).arg( uiMAStr2() ) );
    niCumTypCnt[niTypeMA] = vc.size() * muxFactor;

    Subset::rngStr2Vec( vc,
        QString("%1,%2").arg( uiXAStr1 ).arg( uiXAStr2() ) );
    niCumTypCnt[niTypeXA] = vc.size();

    Subset::rngStr2Vec( vc,
        QString("%1,%2").arg( uiXDStr1 ).arg( uiXDStr2() ) );
    niCumTypCnt[niTypeXD] = (vc.size() + 15) / 16;

// ---------
// Integrate
// ---------

    for( int i = 1; i < niNTypes; ++i )
        niCumTypCnt[i] += niCumTypCnt[i - 1];
}


int CniCfg::vToInt16( double v, int ic ) const
{
    return 65535 * range.voltsToUnity( v * chanGain( ic ) ) - 32768;
}


double CniCfg::int16ToV( int i16, int ic ) const
{
    return range.unityToVolts( (i16 + 32768) / 65536.0 ) / chanGain( ic );
}


void CniCfg::loadSettings( QSettings &S )
{
    range.rmin =
    S.value( "niAiRangeMin", -2.0 ).toDouble();

    range.rmax =
    S.value( "niAiRangeMax", 2.0 ).toDouble();

    srate =
    S.value( "niSampRate", 19737 ).toDouble();

    mnGain =
    S.value( "niMNGain", 200.0 ).toDouble();

    maGain =
    S.value( "niMAGain", 1.0 ).toDouble();

    dev1 =
    S.value( "niDev1", "" ).toString();

    dev2 =
    S.value( "niDev2", "" ).toString();

    clockStr1 =
    S.value( "niClock1", "PFI2" ).toString();

    clockStr2 =
    S.value( "niClock2", "PFI2" ).toString();

    uiMNStr1 =
    S.value( "niMNChans1", "0:5" ).toString();

    uiMAStr1 =
    S.value( "niMAChans1", "6,7" ).toString();

    uiXAStr1 =
    S.value( "niXAChans1", "" ).toString();

    uiXDStr1 =
    S.value( "niXDChans1", "" ).toString();

    setUIMNStr2(
    S.value( "niMNChans2", "0:5" ).toString() );

    setUIMAStr2(
    S.value( "niMAChans2", "6,7" ).toString() );

    setUIXAStr2(
    S.value( "niXAChans2", "" ).toString() );

    setUIXDStr2(
    S.value( "niXDChans2", "" ).toString() );

    muxFactor =
    S.value( "niMuxFactor", 32 ).toUInt();

    termCfg = (TermConfig)
    S.value( "niAiTermConfig", (int)Default ).toInt();

    enabled =
    S.value( "niEnabled", true ).toBool();

    isDualDevMode =
    S.value( "niDualDevMode", false ).toBool();

    syncEnable =
    S.value( "niSyncEnable", true ).toBool();

    syncLine =
    S.value( "niSyncLine", "" ).toString();
}


void CniCfg::saveSettings( QSettings &S ) const
{
    S.setValue( "niAiRangeMin", range.rmin );
    S.setValue( "niAiRangeMax", range.rmax );
    S.setValue( "niSampRate", srate );
    S.setValue( "niMNGain", mnGain );
    S.setValue( "niMAGain", maGain );
    S.setValue( "niDev1", dev1 );
    S.setValue( "niDev2", dev2 );
    S.setValue( "niClock1", clockStr1 );
    S.setValue( "niClock2", clockStr2 );
    S.setValue( "niMNChans1", uiMNStr1 );
    S.setValue( "niMAChans1", uiMAStr1 );
    S.setValue( "niXAChans1", uiXAStr1 );
    S.setValue( "niXDChans1", uiXDStr1 );
    S.setValue( "niMNChans2", uiMNStr2Bare() );
    S.setValue( "niMAChans2", uiMAStr2Bare() );
    S.setValue( "niXAChans2", uiXAStr2Bare() );
    S.setValue( "niXDChans2", uiXDStr2Bare() );
    S.setValue( "niMuxFactor", muxFactor );
    S.setValue( "niAiTermConfig", (int)termCfg );
    S.setValue( "niEnabled", enabled );
    S.setValue( "niDualDevMode", isDualDevMode );
    S.setValue( "niSyncEnable", syncEnable );
    S.setValue( "niSyncLine", syncLine );
}

/* ---------------------------------------------------------------- */
/* Statics -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// ------
// Macros
// ------

#define SIMAIDEV    "Dev1"
#define SIMAODEV    "Dev3"

#define STRMATCH( s, target ) !(s).compare( target, Qt::CaseInsensitive )

#define DAQmxErrChk(functionCall)                           \
    do {                                                    \
    if( DAQmxFailed(dmxErrNum = (functionCall)) )           \
        {dmxFnName = STR(functionCall); goto Error_Out;}    \
    } while( 0 )

#define DAQmxErrChkNoJump(functionCall)                     \
    (DAQmxFailed(dmxErrNum = (functionCall)) &&             \
    (dmxFnName = STR(functionCall)))

// ----
// Data
// ----

#ifdef HAVE_NIDAQmx
static QVector<char>    dmxErrMsg;
static const char       *dmxFnName;
static int32            dmxErrNum;
#endif

static bool noDaqErrPrint = false;

CniCfg::DeviceRangeMap  CniCfg::aiDevRanges,
                        CniCfg::aoDevRanges;
CniCfg::DeviceChanCount CniCfg::aiDevChanCount,
                        CniCfg::aoDevChanCount,
                        CniCfg::diDevLineCount;

// -------
// Methods
// -------

/* ---------------------------------------------------------------- */
/* clearDmxErrors ------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
static void clearDmxErrors()
{
    dmxErrMsg.clear();
    dmxFnName   = "";
    dmxErrNum   = 0;
}
#endif

/* ---------------------------------------------------------------- */
/* lastDAQErrMsg -------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Capture latest dmxErrNum as a descriptive C-string.
// Call as soon as possible after offending operation.
//
#ifdef HAVE_NIDAQmx
static void lastDAQErrMsg()
{
    const int msgbytes = 2048;
    dmxErrMsg.resize( msgbytes );
    dmxErrMsg[0] = 0;
    DAQmxGetExtendedErrorInfo( &dmxErrMsg[0], msgbytes );
}
#endif

/* ---------------------------------------------------------------- */
/* destroyTask ---------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
static void destroyTask( TaskHandle &taskHandle )
{
    if( taskHandle ) {
        DAQmxStopTask( taskHandle );
        DAQmxClearTask( taskHandle );
        taskHandle = 0;
    }
}
#endif

/* ---------------------------------------------------------------- */
/* getPhysChans --------------------------------------------------- */
/* ---------------------------------------------------------------- */

#if HAVE_NIDAQmx
typedef int32 (__CFUNC *QueryFunc_t)( const char [], char*, uInt32 );

static QStringList getPhysChans(
    const QString   &dev,
    QueryFunc_t     queryFunc,
    const QString   &fn = QString::null )
{
    QString         funcName = fn;
    QVector<char>   buf( 65536 );

    if( !funcName.length() )
        funcName = "??";

    buf[0] = 0;

    clearDmxErrors();

    DAQmxErrChk( queryFunc( STR2CHR( dev ), &buf[0], buf.size() ) );

    // "\\s*,\\s*" encodes <optional wh spc><comma><optional wh spc>
    return QString( &buf[0] )
            .split( QRegExp("\\s*,\\s*"), QString::SkipEmptyParts );

Error_Out:
    if( DAQmxFailed( dmxErrNum ) ) {

        lastDAQErrMsg();

        if( !noDaqErrPrint ) {

            Error()
                << "DAQmx Error: Fun=<"
                << funcName
                << "> Bufsz=("
                << buf.size()
                << ") Err=<"
                << dmxErrNum
                << "> '" << &dmxErrMsg[0] << "'.";
        }
    }

    return QStringList();
}
#endif

/* ---------------------------------------------------------------- */
/* getAIChans ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Gets entries of type "Dev6/ai5"
//
#ifdef HAVE_NIDAQmx
static QStringList getAIChans( const QString &dev )
{
    return getPhysChans( dev,
            DAQmxGetDevAIPhysicalChans,
            "DAQmxGetDevAIPhysicalChans" );
}
#else // !HAVE_NIDAQmx, emulated, 60 chans
static QStringList getAIChans( const QString &dev )
{
    QStringList L;

    if( dev == SIMAIDEV ) {

        for( int i = 0; i < 60; ++i ) {

            L.push_back(
                QString( "%1/ai%2" ).arg( dev ).arg( i ) );
        }
    }

    return L;
}
#endif

/* ---------------------------------------------------------------- */
/* getAOChans ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Gets entries of type "Dev6/ao5"
//
#ifdef HAVE_NIDAQmx
static QStringList getAOChans( const QString &dev )
{
    return getPhysChans( dev,
            DAQmxGetDevAOPhysicalChans,
            "DAQmxGetDevAOPhysicalChans" );
}
#else // !HAVE_NIDAQmx, emulated, 2 chans
static QStringList getAOChans( const QString &dev )
{
    QStringList L;

    if( dev == SIMAODEV ) {

        for( int i = 0; i < 2; ++i ) {

            L.push_back(
                QString( "%1/ao%2" ).arg( dev ).arg( i ) );
        }
    }

    return L;
}
#endif

/* ---------------------------------------------------------------- */
/* getDILines ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Gets entries of type "Dev6/port0/line3"
//
#ifdef HAVE_NIDAQmx
static QStringList getDILines( const QString &dev )
{
    return getPhysChans( dev,
            DAQmxGetDevDILines,
            "DAQmxGetDevDILines" );
}
#else
static QStringList getDILines( const QString &dev )
{
    if( dev == SIMAIDEV ) {
        return QStringList( QString("%1/port0/line0").arg( dev ) )
                << QString("%1/port0/line1").arg( dev );
    }
    else
        return QStringList();
}
#endif

/* ---------------------------------------------------------------- */
/* getDOLines ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Gets entries of type "Dev6/port0/line3"
//
#ifdef HAVE_NIDAQmx
static QStringList getDOLines( const QString &dev )
{
    return getPhysChans( dev,
            DAQmxGetDevDOLines,
            "DAQmxGetDevDOLines" );
}
#else
static QStringList getDOLines( const QString &dev )
{
    return QStringList( QString("%1/port0/line0").arg( dev ) );
}
#endif

/* ---------------------------------------------------------------- */
/* probeAllAIRanges ----------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
static void probeAllAIRanges()
{
    CniCfg::aiDevRanges.clear();

    VRange  r;
    float64 dArr[512];

    for( int idev = 0; idev <= 16; ++idev ) {

        memset( dArr, 0, sizeof(dArr) );

        QString dev = QString( "Dev%1" ).arg( idev );

        if( !DAQmxFailed( DAQmxGetDevAIVoltageRngs(
                STR2CHR( dev ),
                dArr,
                512 ) ) ) {

            for( int i = 0; i < 512; i +=2 ) {

                r.rmin = dArr[i];
                r.rmax = dArr[i+1];

                if( r.rmin == r.rmax )
                    break;

                CniCfg::aiDevRanges.insert( dev, r );
            }
        }
    }
}
#else
static void probeAllAIRanges()
{
    CniCfg::aiDevRanges.clear();

    VRange  r;

    r.rmin = -2.5;
    r.rmax = 2.5;
    CniCfg::aiDevRanges.insert( SIMAIDEV, r );

    r.rmin = -5.0;
    r.rmax = 5.0;
    CniCfg::aiDevRanges.insert( SIMAIDEV, r );
}
#endif

/* ---------------------------------------------------------------- */
/* probeAllAORanges ----------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
static void probeAllAORanges()
{
    CniCfg::aoDevRanges.clear();

    VRange  r;
    float64 dArr[512];

    for( int idev = 0; idev <= 16; ++idev ) {

        memset( dArr, 0, sizeof(dArr) );

        QString dev = QString( "Dev%1" ).arg( idev );

        if( !DAQmxFailed( DAQmxGetDevAOVoltageRngs(
                STR2CHR( dev ),
                dArr,
                512 ) ) ) {

            for( int i = 0; i < 512; i +=2 ) {

                r.rmin = dArr[i];
                r.rmax = dArr[i+1];

                if( r.rmin == r.rmax )
                    break;

                CniCfg::aoDevRanges.insert( dev, r );
            }
        }
    }
}
#else
static void probeAllAORanges()
{
    CniCfg::aoDevRanges.clear();

    VRange  r;

    r.rmin = -2.5;
    r.rmax = 2.5;
    CniCfg::aoDevRanges.insert( SIMAODEV, r );

    r.rmin = -5.0;
    r.rmax = 5.0;
    CniCfg::aoDevRanges.insert( SIMAODEV, r );
}
#endif

/* ---------------------------------------------------------------- */
/* probeAllAIChannels --------------------------------------------- */
/* ---------------------------------------------------------------- */

static void probeAllAIChannels()
{
    CniCfg::aiDevChanCount.clear();

    bool    savedPrt = noDaqErrPrint;

    noDaqErrPrint = true;

    for( int idev = 0; idev <= 16; ++idev ) {

        QString     dev = QString( "Dev%1" ).arg( idev );
        QStringList L   = getAIChans( dev );

        if( !L.empty() )
            CniCfg::aiDevChanCount[dev] = L.count();
    }

    noDaqErrPrint = savedPrt;
}

/* ---------------------------------------------------------------- */
/* probeAllAOChannels --------------------------------------------- */
/* ---------------------------------------------------------------- */

static void probeAllAOChannels()
{
    CniCfg::aoDevChanCount.clear();

    bool    savedPrt = noDaqErrPrint;

    noDaqErrPrint = true;

    for( int idev = 0; idev <= 16; ++idev ) {

        QString     dev = QString( "Dev%1" ).arg( idev );
        QStringList L   = getAOChans( dev );

        if( !L.empty() )
            CniCfg::aoDevChanCount[dev] = L.count();
    }

    noDaqErrPrint = savedPrt;
}

/* ---------------------------------------------------------------- */
/* _sampleFreq1 --------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
static double _sampleFreq1( const QString &dev, const QString &pfi )
{
    TaskHandle      taskHandle  = 0;
    const QString   ctrStr      = QString("%1/ctr0").arg( dev );
    float64         sampSecs    = 0.01,
                    freq        = 0;

    clearDmxErrors();

    DAQmxErrChk( DAQmxCreateTask( "", &taskHandle ) );
    DAQmxErrChk( DAQmxCreateCIFreqChan(
                    taskHandle,
                    STR2CHR( ctrStr ),
                    "",                     // N.A.
                    1e2,                    // min expected freq
                    2e6,                    // max expected freq
                    DAQmx_Val_Hz,           // units
                    DAQmx_Val_Rising,
                    DAQmx_Val_LargeRng2Ctr, // method
                    sampSecs,
                    75,                     // divisor (min=4)
                    "") );
    DAQmxErrChk( DAQmxSetCIFreqTerm(
                    taskHandle,
                    STR2CHR( ctrStr ),
                    STR2CHR( pfi ) ) );

    DAQmxErrChk( DAQmxStartTask( taskHandle ) );
    DAQmxErrChk( DAQmxReadCounterScalarF64(
                    taskHandle,
                    sampSecs + 1,   // timeout secs
                    &freq,
                    NULL ) );

Error_Out:
    if( DAQmxFailed( dmxErrNum ) )
        lastDAQErrMsg();

    destroyTask( taskHandle );

    if( DAQmxFailed( dmxErrNum ) ) {

        QString e = QString("DAQmx Error:\nFun=<%1>\n").arg( dmxFnName );
        e += QString("ErrNum=<%1>\n").arg( dmxErrNum );
        e += QString("ErrMsg='%1'.").arg( &dmxErrMsg[0] );

        Error() << e;
    }

    return freq;
}
#else
static double _sampleFreq1( const QString &dev, const QString &pfi )
{
    Q_UNUSED( dev )
    Q_UNUSED( pfi )

    return 10000.0;
}
#endif

/* ---------------------------------------------------------------- */
/* stringToTermConfig --------------------------------------------- */
/* ---------------------------------------------------------------- */

CniCfg::TermConfig CniCfg::stringToTermConfig( const QString &txt )
{
    if( STRMATCH( txt, "RSE" ) )
        return RSE;
    else if( STRMATCH( txt, "NRSE" ) )
        return NRSE;
    else if( STRMATCH( txt, "Differential" )
             || STRMATCH( txt, "Diff" ) ) {

        return Diff;
    }
    else if( STRMATCH( txt, "PseudoDifferential" )
             || STRMATCH( txt, "PseudoDiff" ) ) {

        return PseudoDiff;
    }

    return Default;
}

/* ---------------------------------------------------------------- */
/* termConfigToString --------------------------------------------- */
/* ---------------------------------------------------------------- */

QString CniCfg::termConfigToString( TermConfig t )
{
    switch( t ) {

        case RSE:
            return "RSE";
        case NRSE:
            return "NRSE";
        case Diff:
            return "Differential";
        case PseudoDiff:
            return "PseudoDifferential";
        default:
            break;
    }

    return "Default";
}

/* ---------------------------------------------------------------- */
/* getPFIChans ---------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Gets entries of type "Dev6/PFI0"
//
#ifdef HAVE_NIDAQmx
QStringList CniCfg::getPFIChans( const QString &dev )
{
    return getPhysChans( dev,
            DAQmxGetDevTerminals,
            "DAQmxGetDevTerminals" )
            .filter( "/PFI" );
}
#else // !HAVE_NIDAQmx, emulated, 16 chans
QStringList CniCfg::getPFIChans( const QString &dev )
{
    QStringList L;

    if( dev == SIMAIDEV || dev == SIMAODEV ) {

        for( int i = 0; i < 16; ++i ) {

            L.push_back(
                QString( "%1/PFI%2" ).arg( dev ).arg( i ) );
        }
    }

    return L;
}
#endif

/* ---------------------------------------------------------------- */
/* getAllDOLines -------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
QStringList CniCfg::getAllDOLines()
{
    QStringList L, devL = diDevLineCount.uniqueKeys();

    foreach( QString dev, devL )
        L += getDOLines( dev );

    return L;
}
#else // !HAVE_NIDAQmx, emulated, dev1 only
QStringList CniCfg::getAllDOLines()
{
    return getDOLines( SIMAIDEV );
}
#endif

/* ---------------------------------------------------------------- */
/* parseDIStr ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Determine device and total line indices from DI string
// with pattern "Dev6/port0/line3".
//
// Return error string (or empty).
//
QString CniCfg::parseDIStr(
    QString         &dev,
    int             &line,
    const QString   &lineStr )
{
    QString err;

    QRegExp re("(dev\\d+)");
    re.setCaseSensitivity( Qt::CaseInsensitive );

    if( lineStr.contains( re ) ) {

        dev = re.cap(1);

        QStringList L = getDILines( dev );

        line = L.indexOf( lineStr );

        if( line < 0 )
            err = "Line not supported on given device.";
    }
    else
        err = "Device name not found in DI string.";

    return err;
}

/* ---------------------------------------------------------------- */
/* isHardware ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
bool CniCfg::isHardware()
{
    char    data[2048] = {0};

    if( DAQmxFailed( DAQmxGetSysDevNames( data, sizeof(data) ) ) )
        return false;

    return data[0] != 0;
}
#else
bool CniCfg::isHardware()
{
    return true;
}
#endif

/* ---------------------------------------------------------------- */
/* probeAIHardware ------------------------------------------------ */
/* ---------------------------------------------------------------- */

void CniCfg::probeAIHardware()
{
    probeAllAIRanges();
    probeAllAIChannels();
}

/* ---------------------------------------------------------------- */
/* probeAOHardware ------------------------------------------------ */
/* ---------------------------------------------------------------- */

void CniCfg::probeAOHardware()
{
    probeAllAORanges();
    probeAllAOChannels();
}

/* ---------------------------------------------------------------- */
/* probeAllDILines ------------------------------------------------ */
/* ---------------------------------------------------------------- */

void CniCfg::probeAllDILines()
{
    diDevLineCount.clear();

    bool    savedPrt = noDaqErrPrint;

    noDaqErrPrint = true;

    for( int idev = 0; idev <= 16; ++idev ) {

        QString     dev = QString( "Dev%1" ).arg( idev );
        QStringList L   = getDILines( dev );

        if( !L.empty() )
            diDevLineCount[dev] = L.count();
    }

    noDaqErrPrint = savedPrt;
}

/* ---------------------------------------------------------------- */
/* supportsAISimultaneousSampling --------------------------------- */
/* ---------------------------------------------------------------- */

// Return true iff (device) supports AI simultaneous sampling.
//
#ifdef HAVE_NIDAQmx
bool CniCfg::supportsAISimultaneousSampling( const QString &dev )
{
    bool32 impl = false;

    if( DAQmxFailed(
        DAQmxGetDevAISimultaneousSamplingSupported(
        STR2CHR( dev ), &impl ) ) ) {

        Error()
            << "Failed during query whether AI dev "
            << dev << " supports simultaneous sampling.";
    }

    return impl;
}
#else
bool CniCfg::supportsAISimultaneousSampling( const QString & )
{
    return true;
}
#endif

/* ---------------------------------------------------------------- */
/* maxSampleRate -------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
double CniCfg::maxSampleRate( const QString &dev, int nChans )
{
    double  rate = 1e6;
    float64 val;
    int32   e;

    if( nChans <= 0 )
        nChans = 1;

    if( nChans == 1 )
        e = DAQmxGetDevAIMaxSingleChanRate( STR2CHR( dev ), &val );
    else
        e = DAQmxGetDevAIMaxMultiChanRate( STR2CHR( dev ), &val );

    if( DAQmxFailed( e ) ) {
        Error()
            << "Failed during query of max sample rate for dev "
            << dev << ".";
    }
    else {

        rate = val;

        if( nChans > 1
            && !supportsAISimultaneousSampling( dev ) ) {

            rate = rate / nChans;
        }
    }

    return rate;
}
#else
double CniCfg::maxSampleRate( const QString &, int )
{
    return 1e6;
}
#endif

/* ---------------------------------------------------------------- */
/* minSampleRate -------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
double CniCfg::minSampleRate( const QString &dev )
{
    double  rate = 10.0;
    float64 val;

    if( DAQmxFailed(
        DAQmxGetDevAIMinRate( STR2CHR( dev ), &val ) ) ) {

        Error()
            << "Failed during query of min sample rate for dev "
            << dev << ".";
    }
    else
        rate = val;

    return rate;
}
#else
double CniCfg::minSampleRate( const QString & )
{
    return 10.0;
}
#endif

/* ---------------------------------------------------------------- */
/* getProductName ------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef HAVE_NIDAQmx
QString CniCfg::getProductName( const QString &dev )
{
    QVector<char>   buf( 65536 );
    strcpy( &buf[0], "Unknown" );

    if( DAQmxFailed(
        DAQmxGetDevProductType(
        STR2CHR( dev ), &buf[0], buf.size() ) ) ) {

        Error()
            << "Failed during query of product name for dev "
            << dev << ".";
    }

    return &buf[0];
}
#else
QString CniCfg::getProductName( const QString & )
{
    return "FakeDAQ";
}
#endif

/* ---------------------------------------------------------------- */
/* setDO ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Return err string of null if success.
//
#ifdef HAVE_NIDAQmx
QString CniCfg::setDO( const QString &line, bool onoff )
{
    TaskHandle  taskHandle  = 0;
    uInt32      w_data      = (onoff ? 1 : 0);

    clearDmxErrors();

    DAQmxErrChk( DAQmxCreateTask( "", &taskHandle ) );
    DAQmxErrChk( DAQmxCreateDOChan(
                    taskHandle,
                    STR2CHR( line ),
                    "",
                    DAQmx_Val_ChanPerLine ) );
    DAQmxErrChk( DAQmxWriteDigitalScalarU32(
                    taskHandle,
                    true,           // autostart
                    2.5,            // timeout secs
                    w_data,
                    NULL ) );

Error_Out:
    if( DAQmxFailed( dmxErrNum ) )
        lastDAQErrMsg();

    destroyTask( taskHandle );

    if( DAQmxFailed( dmxErrNum ) ) {

        QString e = QString("DAQmx Error:\nFun=<%1>\n").arg( dmxFnName );
        e += QString("ErrNum=<%1>\n").arg( dmxErrNum );
        e += QString("ErrMsg='%1'.").arg( &dmxErrMsg[0] );

        Error() << e;

        return e;
    }

    return QString::null;
}
#else
QString CniCfg::setDO( const QString &line, bool onoff )
{
    Q_UNUSED( line )
    Q_UNUSED( onoff )

    return QString::null;
}
#endif

/* ---------------------------------------------------------------- */
/* sampleFreq ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

double CniCfg::sampleFreq(
    const QString   &dev,
    const QString   &pfi,
    const QString   &line )
{
    const int   nSamp = 100;

    double  freq = 0;

    if( !setDO( line, true ).isEmpty() )
        return 0.0;

    msleep( 100 );

    for( int i = 0; i < nSamp; ++i ) {

        double  f = _sampleFreq1( dev, pfi );

        if( !f )
            break;

        freq += f;
    }

    setDO( line, false );

    return freq / nSamp;
}


