
#include "SVToolsM.h"
#include "SVGrafsM_Im.h"
#include "SVGrafsM_Ni.h"
#include "SView.h"

#include <QVBoxLayout>




SViewM_Im::SViewM_Im( SVGrafsM_Im *&vw, GraphsWindow *gw, DAQ::Params &p )
{
    vw = new SVGrafsM_Im( gw, p );
    SVToolsM    *tb = new SVToolsM( vw );

    vw->init( tb );

    QVBoxLayout *L = new QVBoxLayout( this );
    L->setSpacing( 0 );
    L->setMargin( 0 );
    L->addWidget( tb );
    L->addWidget( vw );
}


SViewM_Ni::SViewM_Ni( SVGrafsM_Ni* &vw, GraphsWindow *gw, DAQ::Params &p )
{
    vw = new SVGrafsM_Ni( gw, p );
    SVToolsM    *tb = new SVToolsM( vw );

    vw->init( tb );

    QVBoxLayout *L = new QVBoxLayout( this );
    L->setSpacing( 0 );
    L->setMargin( 0 );
    L->addWidget( tb );
    L->addWidget( vw );
}


