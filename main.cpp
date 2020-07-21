////////////////////////////////////////////////////////////////////////////////
//      This file is part of Unraid USB Creator - https://github.com/limetech/usb-creator
//      Copyright (C) 2013-2015 RasPlex project
//      Copyright (C) 2016 Team LibreELEC
//      Copyright (C) 2018 Lime Technology, Inc
//
//  Unraid USB Creator is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  Unraid USB Creator is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Unraid USB Creator.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////////////

#include "creator.h"
#include "version.h"

#ifdef Q_OS_UNIX
#include "privileges_unix.h"
#else
#include "privileges.h"
#endif

#include <QApplication>
#include <QFileInfo>
#include <QDesktopServices>
#include <QProxyStyle>
#include <QNetworkProxy>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QTextStream>

// Get the default Qt message handler.
static const QtMessageHandler QT_DEFAULT_MESSAGE_HANDLER = qInstallMessageHandler(0);

QFile debugFile;

// show debug output always
//#define ALWAYS_DEBUG_OUTPUT
/*
#ifdef Q_OS_MACOS
class MacFontStyle : public QProxyStyle
{
protected:
    void polish(QWidget *w)
    {
        //QMenu* mn = dynamic_cast<QMenu*>(w);
        //if (!mn && !w->testAttribute(Qt::WA_MacNormalSize))
        if (!w->testAttribute(Qt::WA_MacNormalSize))
            w->setAttribute(Qt::WA_MacSmallSize);
    }
};
#endif
*/

void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString txt;
    switch (type) {
        case QtDebugMsg:
            txt = QString(" Debug: %1").arg(msg);
            break;
        case QtInfoMsg:
            txt = QString(" Info: %1").arg(msg);
            break;
        case QtWarningMsg:
            txt = QString(" Warning: %1").arg(msg);
            break;
        case QtCriticalMsg:
            txt = QString(" Critical: %1").arg(msg);
            break;
        case QtFatalMsg:
            txt = QString(" Fatal: %1").arg(msg);
            break;
    }
    if (!debugFile.isOpen()) {
        debugFile.setFileName("log.txt");
        debugFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    QTextStream ts(&debugFile);
    ts << QTime::currentTime().toString() << txt << endl;

    // Call the default handler
    (*QT_DEFAULT_MESSAGE_HANDLER)(type, context, msg);
}

void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    Q_UNUSED(msg);
}

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QString argFile = "";

//#ifdef Q_OS_MACOS
    // prevents the font size from appearing overly large on OSX
//    app.setStyle(new MacFontStyle);
//#endif

    qInstallMessageHandler(logMessageHandler);

#ifndef ALWAYS_DEBUG_OUTPUT
    if (app.arguments().contains("--debug") == false)
        qInstallMessageHandler(noMessageOutput);
#endif

#ifdef Q_OS_MACOS
    // If not running with root privileges, relaunch executable with sudo.
    if (getuid() != 0 && app.arguments().contains("--elevated") == false)
    {
        QString askPassCommand = QCoreApplication::applicationDirPath() + "/askPass.js";

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("SUDO_ASKPASS", askPassCommand);

        QProcess myProcess;
        myProcess.setProcessEnvironment(env);
        myProcess.setProgram("sudo");
        myProcess.setArguments(QStringList()
            << "-A"
            << QCoreApplication::applicationFilePath()
            << "--elevated");
        bool success = myProcess.startDetached();

        if (success)
        {
            return 0;
        } 
        else
        {
            qDebug() << "Unable to start elevated process for " << QCoreApplication::applicationFilePath();
        }
    } 
#endif

    qDebug() << "App data: Version:" << BUILD_VERSION ", Build date: " BUILD_DATE;

    if (!QSslSocket::supportsSsl()) {
        qDebug() << "SSL Support not detected";
        //QMessageBox::information(0, "Secure Socket Client",
        //                             "This system does not support OpenSSL.");
        //return -1;
    }

    if (app.arguments().contains("--no-proxy") == false) {
        QNetworkProxyQuery npq(QUrl("https://s3.amazonaws.com/"));
        QList<QNetworkProxy> listOfProxies = QNetworkProxyFactory::systemProxyForQuery(npq);
        if (listOfProxies.size()) {
            QNetworkProxy::setApplicationProxy(listOfProxies[0]);
            qDebug() << "Using" << listOfProxies[0];
        }
    }

    Privileges privileges = Privileges();
    privileges.Whoami();

    // skip program filename
    for (int i=1; i<app.arguments().size(); i++) {
        QString file = app.arguments().at(i);
        QFileInfo checkFile(file);

        if (checkFile.exists() && checkFile.isFile()) {
            argFile = file;
            break;
        }
    }

#ifndef Q_OS_MACOS
    privileges.SetUser();
#endif

    privileges.Whoami();

    Creator win(privileges, 0);
    win.setArgFile(argFile);
    win.show();

    return app.exec();
}
