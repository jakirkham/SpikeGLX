
#include "ui_RgtSrvDialog.h"

#include "Util.h"
#include "MainApp.h"
#include "Run.h"
#include "ConsoleWindow.h"
#include "RgtServer.h"
#include "RgtSrvDlg.h"

#include <QMessageBox>
#include <QSettings>




RgtSrvDlg::RgtSrvDlg() : QObject(0), rgtServer(0)
{
}


RgtSrvDlg::~RgtSrvDlg()
{
    if( rgtServer ) {
        delete rgtServer;
        rgtServer = 0;
    }
}


void RgtSrvDlg::loadSettings( QSettings &S )
{
    S.beginGroup( "RgtSrv" );

    p.iface         = S.value( "iface", getMyIPAddress() ).toString();
    p.port          = S.value( "port", RGT_DEF_PORT ).toUInt();
    p.timeout_ms    = S.value( "timeoutMS", RGT_TOUT_MS ).toInt();

    S.endGroup();
}


void RgtSrvDlg::saveSettings( QSettings &S ) const
{
    S.beginGroup( "RgtSrv" );

    S.setValue( "iface", p.iface );
    S.setValue( "port",  p.port );
    S.setValue( "timeoutMS", p.timeout_ms );

    S.endGroup();
}


bool RgtSrvDlg::startServer( bool isAppStrtup )
{
    MainApp *app = mainApp();
    Run     *run = app->getRun();

    if( rgtServer )
        delete rgtServer;

    rgtServer = new ns_RgtServer::RgtServer( app );

    if( !rgtServer->beginListening( p.iface, p.port, p.timeout_ms ) ) {

        if( !isAppStrtup ) {

            QMessageBox::critical(
                0,
                "Gate/Trigger Listen Error",
                QString(
                "Gate/Trigger server could not listen on (%1:%2).")
                .arg( p.iface )
                .arg( p.port ) );
        }

        Error() << "Failed starting gate/trigger server!";
        delete rgtServer;
        rgtServer = 0;
        return false;
    }

    Connect( rgtServer, SIGNAL(rgtSetGate(bool)), run, SLOT(rgtSetGate(bool)) );
    Connect( rgtServer, SIGNAL(rgtSetTrig(bool)), run, SLOT(rgtSetTrig(bool)) );
    Connect( rgtServer, SIGNAL(rgtSetMetaData(KeyValMap)), run, SLOT(rgtSetMetaData(KeyValMap)) );

    return true;
}


void RgtSrvDlg::showStartupMessage()
{
    if( rgtServer )
        return;

    int but = QMessageBox::critical(
        0,
        "Gate/Trigger Listen Error",
        QString(
        "Gate/Trigger server could not listen on (%1:%2).\n\n"
        "You can ignore this for now and select different settings\n"
        "later using the Options menu.\n\n"
        "(If mouse not working, actuate these buttons with keyboard.)"
        "            ")
        .arg( p.iface )
        .arg( p.port ),
        QMessageBox::Abort,
        QMessageBox::Ignore );

    if( but == QMessageBox::Abort )
        QMetaObject::invokeMethod( mainApp(), "quit", Qt::QueuedConnection );
}


void RgtSrvDlg::showOptionsDlg()
{
    QDialog dlg;

    rgtUI = new Ui::RgtSrvDialog;
    rgtUI->setupUi( &dlg );
    rgtUI->ipLE->setText( p.iface );
    rgtUI->portSB->setValue( p.port );
    rgtUI->toSB->setValue( p.timeout_ms );
    ConnectUI( rgtUI->ipBut, SIGNAL(clicked()), this, SLOT(ipBut()) );
    ConnectUI( rgtUI->buttonBox, SIGNAL(accepted()), this, SLOT(okBut()) );

    if( QDialog::Accepted != dlg.exec() && !rgtServer ) {

        // If failed to set new listener...restore former.
        startServer();
    }

    rgtUI->buttonBox->disconnect();
    delete rgtUI;
}


void RgtSrvDlg::ipBut()
{
    rgtUI->ipLE->setText( getMyIPAddress() );
}


void RgtSrvDlg::okBut()
{
    RgtSrvParams    pSave = p;

    p.iface         = rgtUI->ipLE->text();
    p.port          = rgtUI->portSB->value();
    p.timeout_ms    = rgtUI->toSB->value();

    if( startServer() ) {
        mainApp()->saveSettings();
        dynamic_cast<QDialog*>(rgtUI->buttonBox->parent())->accept();
    }
    else
        p = pSave;
}

