#ifndef TRIGBASE_H
#define TRIGBASE_H

#include "AIQ.h"
#include "DataFileIMAP.h"
#include "DataFileIMLF.h"
#include "DataFileNI.h"

#include <QMutex>

namespace DAQ {
struct Params;
}

class GraphsWindow;

class QFileInfo;
class QThread;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class TrigBase : public QObject
{
    Q_OBJECT

private:
    struct ManOvr {
        int             usrG,
                        usrT;
        volatile bool   forceGT,
                        gateEnab;

        ManOvr( const DAQ::Params &p )
        : gateEnab(!p.mode.manOvInitOff)    {reset();}

        void reset()                {usrG=-1, usrT=-1,  forceGT=false;}
        void set( int g, int t )    {usrG=g,  usrT=t,   forceGT=true;}
        void get( int &g, int &t )  {g=usrG,  t=usrT-1, reset();}
    };

private:
    DataFileIMAP    *dfImAp;
    DataFileIMLF    *dfImLf;
    DataFileNI      *dfNi;
    ManOvr          ovr;
    mutable QMutex  dfMtx;
    mutable QMutex  startTMtx;
    KeyValMap       kvmRmt;
    double          startT,
                    gateHiT,
                    gateLoT,
                    trigHiT;
    quint64         firstCtIm,
                    firstCtNi;
    int             iGate,
                    iTrig,
                    loopPeriod_us;
    volatile bool   gateHi,
                    pleaseStop;

protected:
    enum DstStream {
        DstImec = 0,
        DstNidq = 1
    };

protected:
    DAQ::Params     &p;
    GraphsWindow    *gw;
    const AIQ       *imQ,
                    *niQ;
    mutable QMutex  runMtx;
    double          statusT;

public:
    TrigBase(
        DAQ::Params     &p,
        GraphsWindow    *gw,
        const AIQ       *imQ,
        const AIQ       *niQ );
    virtual ~TrigBase() {}

    bool allFilesClosed() const;
    bool isInUse( const QFileInfo &fi ) const;
    void setMetaData( const KeyValMap &kvm )
        {QMutexLocker ml( &dfMtx ); kvmRmt = kvm;}
    QString curNiFilename() const;
    quint64 curImFileStart() const;
    quint64 curNiFileStart() const;

    void setStartT();
    void setGateEnabled( bool enabled );

    void stop()             {QMutexLocker ml( &runMtx ); pleaseStop = true;}
    bool isStopped() const  {QMutexLocker ml( &runMtx ); return pleaseStop;}

    virtual void setGate( bool hi ) = 0;
    virtual void resetGTCounters() = 0;

    void forceGTCounters( int g, int t );

signals:
    void finished();

public slots:
    virtual void run() = 0;

protected:
    void baseSetGate( bool hi );
    void baseResetGTCounters();

    bool isGateHi() const       {QMutexLocker ml( &runMtx ); return gateHi;}
    double getGateHiT() const   {QMutexLocker ml( &runMtx ); return gateHiT;}
    double getGateLoT() const   {QMutexLocker ml( &runMtx ); return gateLoT;}

    void getGT( int &ig, int &it ) const
        {
            QMutexLocker ml( &runMtx );
            ig = iGate;
            it = iTrig;
        }

    int incTrig( int &ig )
        {
            QMutexLocker ml( &runMtx );
            ig = iGate;
            return ++iTrig;
        }

    void endTrig();
    bool newTrig( int &ig, int &it, bool trigLED = true );
    void setSyncWriteMode();
    void alignX12( quint64 &imCt, quint64 &niCt, bool testFile = true );
    void alignX12( const AIQ *qA, quint64 &cA, quint64 &cB );
    bool writeAndInvalVB(
        DstStream                   dst,
        std::vector<AIQ::AIQBlock>  &vB );
    quint64 scanCount( DstStream dst );
    void endRun();
    void statusOnSince( QString &s, double nowT, int ig, int it );
    void statusWrPerf( QString &s );
    void setYieldPeriod_ms( int loopPeriod_ms );
    void yield( double loopT );

private:
    bool openFile( DataFile *df, int ig, int it );
    bool writeVBIM( std::vector<AIQ::AIQBlock> &vB );
    bool writeVBNI( std::vector<AIQ::AIQBlock> &vB );
};


class Trigger
{
public:
    QThread     *thread;
    TrigBase    *worker;

public:
    Trigger(
        DAQ::Params     &p,
        GraphsWindow    *gw,
        const AIQ       *imQ,
        const AIQ       *niQ );
    virtual ~Trigger();
};

#endif  // TRIGBASE_H


