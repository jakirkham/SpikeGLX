#ifndef IMREADER_H
#define IMREADER_H

#include "DAQ.h"
#include "AIQ.h"

#include <QObject>
using namespace DAQ;

class CimAcq;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class IMReaderWorker : public QObject
{
    Q_OBJECT

    friend class CimAcqImec;
    friend class CimAcqSim;

private:
    CimAcq  *imAcq;
    AIQ     *imQ;

public:
    IMReaderWorker( const Params &p, AIQ *imQ );
    virtual ~IMReaderWorker();

    const AIQ* getAIQ() const   {return imQ;}

    bool isReady() const;
    void start();
    void stayAwake();
    void wake()                 {start();}
    bool pause( bool pause, bool changed );
    void stop();

signals:
    void daqError( const QString &s );
    void finished();

public slots:
    void run();
};


class IMReader
{
public:
    QThread         *thread;
    IMReaderWorker  *worker;

public:
    IMReader( const Params &p, AIQ *imQ );
    virtual ~IMReader();

    void configure();
};

#endif  // IMREADER_H


