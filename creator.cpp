////////////////////////////////////////////////////////////////////////////////
//      This file is part of Unraid USB Creator - https://github.com/limetech/usb-creator
//      Copyright (C) 2013-2015 RasPlex project
//      Copyright (C) 2016 Team LibreELEC
//      Copyright (C) 2018-2020 Lime Technology, Inc
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
#include "ui_creator.h"
#include "version.h"

#include <QDebug>
#include <QString>
#include <QScopedPointer>
#include <QFile>
#include <QFileDialog>
#include <iostream>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QPlainTextEdit>
#include <QLinkedList>
#include <QStyleFactory>
#include <QDesktopServices>
#include <QMimeData>
#include <QProcess>
#include <QVersionNumber>
#include <QFuture>
#include <QtConcurrent>
#include <QVBoxLayout>
#include <QKeyEvent>

#include "customlineedit.h"
#include "customipeditor.h"
#include "libs/quazip/JlCompress.h"
//#include "libs/sha256crypt/sha256crypt.h"

#if defined(Q_OS_WIN)
#include "diskwriter_windows.h"
#include "deviceenumerator_windows.h"
#elif defined(Q_OS_MACOS)
#include <unistd.h>
#include "diskwriter_unix.h"
#include "deviceenumerator_macos.h"
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include "diskwriter_unix.h"
#include "deviceenumerator_linux.h"
#endif

// force update notification dialog
//#define FORCE_UPDATE_NOTIFICATION "1.3"

const QString Creator::branchesUrl = "https://s3.amazonaws.com/dnld.lime-technology.com/creator_branches.json";
const QString Creator::versionUrl = "https://s3.amazonaws.com/dnld.lime-technology.com/creator_version";
const QString Creator::validatorUrl = "https://keys.lime-technology.com/validate/guid";
const QString Creator::helpUrl = "https://unraid.net/download/";
const int Creator::timerValue = 1500;  // msec

static DeviceEnumerator* makeEnumerator()
{
#if defined(Q_OS_WIN)
    return new DeviceEnumerator_windows();
#elif defined(Q_OS_MACOS)
    return new DeviceEnumerator_macos();
#elif defined(Q_OS_LINUX)
    return new DeviceEnumerator_linux();
#endif
}

Creator::Creator(Privileges &privilegesArg, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Creator),
    manager(new DownloadManager(this)),
    state(STATE_IDLE),
    imageHash(QCryptographicHash::Md5),
    privileges(privilegesArg)
{
    // dummy strings used for translation buttons on message box
    QString forMsgBoxTranslationStrings = tr("Yes") + \
                                          tr("No") + \
                                          tr("OK");
    Q_UNUSED(forMsgBoxTranslationStrings);

    timerId = 0;

    ui->setupUi(this);
    installEventFilter(this);

    parserData = new JsonParser();

#if defined(Q_OS_WIN)
    diskWriter = new DiskWriter_windows();
#elif defined(Q_OS_MACOS)
    diskWriter = new DiskWriter_unix();
#elif defined(Q_OS_LINUX)
    diskWriter = new DiskWriter_unix();
#endif

    devEnumerator = makeEnumerator();
    diskWriterThread = new QThread(this);
    diskWriter->moveToThread(diskWriterThread);

    // hide ? button
    this->setWindowFlags(this->windowFlags() & ~(Qt::WindowContextHelpButtonHint));
    // add minimize button on Windows
    this->setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);

    connect(diskWriterThread, SIGNAL(finished()), diskWriter, SLOT(deleteLater()));
    connect(this, SIGNAL(proceedToWriteImageToDevice(QString,QString,quint32,quint8)),
            diskWriter, SLOT(writeImageToRemovableDevice(QString,QString,quint32,quint8)));
    connect(this, SIGNAL(proceedToExtractFiles(QString,QString)), diskWriter, SLOT(extractFiles(QString,QString)));
    connect(this, SIGNAL(proceedToWriteSyslinux(QString)), diskWriter, SLOT(writeSyslinux(QString)));

    connect(diskWriter, SIGNAL(bytesWritten(int)),this, SLOT(handleWriteProgress(int)));
    connect(diskWriter, SIGNAL(filesExtracted(int)),this, SLOT(handleExtractProgress(int)));
    connect(diskWriter, SIGNAL(syncing()), this, SLOT(writingSyncing()));
    connect(diskWriter, SIGNAL(finished()), this, SLOT(writingFinished()));
    connect(diskWriter, SIGNAL(error(QString)), this, SLOT(writingError(QString)));
    connect(diskWriter, SIGNAL(finished_extract(QString)), this, SLOT(handleExtractFilesComplete(QString)));
    connect(diskWriter, SIGNAL(finished_syslinux()), this, SLOT(handleWriteSyslinux()));
    diskWriterThread->start();

    connect(ui->refreshRemovablesButton,SIGNAL(clicked()), this,SLOT(refreshRemovablesList()));

    connect(manager, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(handleDownloadProgress(qint64, qint64)));
    connect(manager, SIGNAL(downloadComplete(QByteArray)), this, SLOT(handleFinishedDownload(QByteArray)));
    connect(manager, SIGNAL(partialData(QByteArray,qlonglong)), this, SLOT(handlePartialData(QByteArray,qlonglong)));
    connect(manager, SIGNAL(downloadError(QNetworkReply*)), this, SLOT(handleDownloadError(QNetworkReply*)));

    connect(ui->writeFlashButton, SIGNAL(clicked()), this, SLOT(downloadAndWriteButtonClicked()));
    connect(ui->LocalZipPickerButton, SIGNAL(clicked()), this, SLOT(localZipPickerButtonClicked()));
    connect(ui->projectSelectBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setProjectImages()));
    connect(ui->imageSelectBox, &QComboBox::currentTextChanged, [=] { flashProgressBarText(); });
    connect(ui->removableDevicesComboBox, &QComboBox::currentTextChanged, [=] { 
        flashProgressBarText(); 
        checkWriteFlashAvailable();
    });
    connect(ui->LocalZipText, &QLineEdit::textChanged, [=] { checkWriteFlashAvailable(); });
    connect(ui->CustomizeButton, &QPushButton::toggled, [=] (bool active) { ui->CustomizePanel->setVisible(active); });
    connect(ui->NetworkStaticRadioButton, &QRadioButton::toggled, [=] (bool active) { ui->StaticIPPanel->setVisible(active); });

    connect(ui->ShowPasswordButton, &QPushButton::toggled, [=] (bool active) {
        ui->RootPasswordText->setEchoMode((active ? QLineEdit::Normal : QLineEdit::Password));
    });

    connect(ui->showAboutButton, SIGNAL(clicked()), this, SLOT(showAbout()));
    connect(ui->closeAboutButton, SIGNAL(clicked()), this, SLOT(closeAbout()));
    connect(ui->closeAppButton, SIGNAL(clicked()), this, SLOT(close()));

    ui->NetmaskComboBox->addItem("255.255.0.0");
    ui->NetmaskComboBox->addItem("255.255.128.0");
    ui->NetmaskComboBox->addItem("255.255.192.0");
    ui->NetmaskComboBox->addItem("255.255.224.0");
    ui->NetmaskComboBox->addItem("255.255.240.0");
    ui->NetmaskComboBox->addItem("255.255.248.0");
    ui->NetmaskComboBox->addItem("255.255.252.0");
    ui->NetmaskComboBox->addItem("255.255.254.0");
    ui->NetmaskComboBox->addItem("255.255.255.0");
    ui->NetmaskComboBox->addItem("255.255.255.128");
    ui->NetmaskComboBox->addItem("255.255.255.192");
    ui->NetmaskComboBox->addItem("255.255.255.224");
    ui->NetmaskComboBox->addItem("255.255.255.240");
    ui->NetmaskComboBox->addItem("255.255.255.248");
    ui->NetmaskComboBox->addItem("255.255.255.252");
    ui->NetmaskComboBox->setCurrentText("255.255.255.0");

    ui->LocalZipText->hide();
    ui->LocalZipPickerButton->hide();
    ui->StaticIPPanel->hide();
    ui->CustomizePanel->hide();
    ui->EFIBootLocalCheckBox->hide();

    //TODO: make the crypt256 lib available on Windows and then the app can provide a password field
    ui->RootPasswordLabel->hide();
    ui->RootPasswordText->hide();
    ui->ShowPasswordButton->hide();

    QRegExp rgx("^[A-Za-z0-9]([A-Za-z0-9\\-\\.]{0,13}[A-Za-z0-9])?$");
    QValidator *comValidator = new QRegExpValidator(rgx, this);
    ui->ServerNameText->setValidator(comValidator);

    refreshRemovablesList();

    // create a timer that refreshes the device list every 1.5 second
    // if there is any change then list is changed and current device removed
    timerId = 0;
    timerId = startTimer(timerValue);

    setImageFileName("");
    bootimageFile.setFileName("");
    ui->writeFlashButton->setEnabled(false);

    setAcceptDrops(true);    // allow droping files on a window
    showRootMessageBox();

    // call web browser through our wrapper for Linux
    QDesktopServices::setUrlHandler("http", this, "httpsUrlHandler");
    QDesktopServices::setUrlHandler("https", this, "httpsUrlHandler");

    retranslateUi();  // retranslate dynamic texts

    downloadVersionCheck();
}

bool Creator::showRootMessageBox()
{
#ifdef Q_OS_WIN
    return false;   // always run with administrative privileges
#else
    if (getuid() == 0)  // root == 0, real user != 0
        return false;

    QMessageBox msgBox(this);
    msgBox.setText(tr("Root privileges required to write image.\nRun application with sudo."));
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setButtonText(QMessageBox::Ok, tr("OK"));
    msgBox.exec();
    return true;
#endif
}

Creator::~Creator()
{
    if (imageFile.exists() && state == STATE_DOWNLOADING_IMAGE) {
        qDebug() << "Removing file" << imageFile.fileName();
        imageFile.remove();
    } else if (state == STATE_WRITING_IMAGE) {
        //privileges.SetUser();    // back to user
    }

    delete ui;
    diskWriter->cancelWrite();
    diskWriterThread->quit();
    diskWriterThread->wait();
    delete diskWriterThread;
    delete devEnumerator;
}

void Creator::httpsUrlHandler(const QUrl &url)
{
    // on windows open web browser directly
    // for linux use a wrapper to set uid/gid correctly
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    QDesktopServices::openUrl(url);
#else
    qDebug() << "httpsUrlHandler called" << url;

    pid_t pid = fork();
    if (pid == 0) {
        // child process, set both real and effective uid/gid
        // because GTK+ applications check this and doesn't run
        setenv("DBUS_SESSION_BUS_ADDRESS", privileges.GetUserEnvDbusSession().toLatin1().data(), 1);
        setenv("LOGNAME", privileges.GetUserEnvLogname().toLatin1().data(), 1);
        privileges.SetRoot();       // no need to switch back
        privileges.SetRealUser();   // no need to switch back
        QDesktopServices::openUrl(QUrl(url));
        _exit(0);
    }

#if 0
    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = QStringList("--browser");

    // root is needed to start the process which
    // will be dropped back to user (both real and effective uid/gid)
    privileges.SetRoot();

    setenv("DBUS_SESSION_BUS_ADDRESS", privileges.GetUserEnvDbusSession().toLatin1().data(), 1);
    setenv("LOGNAME", privileges.GetUserEnvLogname().toLatin1().data(), 1);
    setenv("LE_URL_ADDRESS", url.toString().toLatin1().data(), 1);

    QProcess myProcess;
    myProcess.startDetached(program, arguments);
    myProcess.waitForStarted();
    myProcess.waitForFinished();

    privileges.SetUser();    // back to user
#endif

    qDebug() << "httpsUrlHandler done";
#endif
}

void Creator::setArgFile(QString file)
{
    if (file.isEmpty())
        return;

    QFileInfo checkFile(file);
    if (!checkFile.exists() || !checkFile.isFile())
        return;

    QFileInfo infoFile(file);
    file = infoFile.absoluteFilePath();
    setImageFileName(file);
}

void Creator::retranslateUi()
{
    // retranslate dynamic texts
    ui->labelVersion->setText(tr("Version: %1\nBuild date: %2").arg(BUILD_VERSION).arg(BUILD_DATE));
}

void Creator::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        return;   // ignore Esc key for close
}

void Creator::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void Creator::dropEvent(QDropEvent *event)
{
    foreach (const QUrl &url, event->mimeData()->urls()) {
        QString file = url.toLocalFile();
        QFileInfo infoFile(file);
        file = infoFile.absoluteFilePath();

        ui->projectSelectBox->setCurrentText("Local Zip");
        ui->LocalZipText->setText(file);

        break;  // only first file
    }
}

void Creator::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    refreshRemovablesList();
}

void Creator::changeEvent(QEvent *e) {
    switch (e->type()) {
    case QEvent::ActivationChange:
        if (this->isActiveWindow()) {
            // got focus
            if (timerId == 0)
                timerId = startTimer(timerValue);
        } else {
            // lost focus
            if (timerId > 0) {
                killTimer(timerId);
                timerId = 0;
            }
        }

        break;
    case QEvent::LanguageChange:
        ui->retranslateUi(this);  // retranslate texts from .ui file
        retranslateUi();  // retranslate dynamic texts
        break;
    default:
        break;
    }
}

void Creator::showAbout()
{
    ui->stackedWidget->setCurrentIndex(STACK_WIDGET_ABOUT);
}

void Creator::closeAbout()
{
    ui->stackedWidget->setCurrentIndex(STACK_WIDGET_MAIN);
}

void Creator::flashProgressBarText(const QString &text)
{
    //ui->flashProgressBar->setFormat("   " + text);
    //ui->flashProgressBar->repaint();
    ui->flashProgressBar->update();
    ui->labelProgress->setText(text);
    //qApp->processEvents();
}

void Creator::populateBranches()
{
    // parse local file if exist
    QFile fileLocalReleases("releases-user.json");
    if (fileLocalReleases.open(QIODevice::ReadOnly | QIODevice::Text)) {
        parserData->addExtra(fileLocalReleases.readAll(), "User");
        fileLocalReleases.close();
    }

    ui->projectSelectBox->clear();

    QList<JsonData> dataList = parserData->getJsonData();
    for (int ix = 0; ix < dataList.size(); ix++) {
        QString projectName = dataList.at(ix).name;
        ui->projectSelectBox->insertItem(0, projectName);
    }

    ui->projectSelectBox->addItem("Local Zip");

    // Stable is default project
    QString defaultSelectedProject = "Stable";

    int idx = ui->projectSelectBox->findText(defaultSelectedProject,
                                             Qt::MatchFixedString);
    if (idx >= 0)
        ui->projectSelectBox->setCurrentIndex(idx);

    setProjectImages();

    ui->flashProgressBar->setValue(0);
    flashProgressBarText();  // it is affected with all downloads
}

void Creator::setProjectImages()
{
    flashProgressBarText();

    if (ui->projectSelectBox->currentText() == "Local Zip") {
        ui->LocalZipText->show();
        ui->LocalZipPickerButton->show();
        ui->EFIBootLocalCheckBox->show();
        ui->imageSelectBox->hide();
        ui->CustomizeButton->setChecked(false);
        ui->CustomizeButton->hide();
        reset();
        return;
    } else {
        ui->LocalZipText->hide();
        ui->LocalZipPickerButton->hide();
        ui->EFIBootLocalCheckBox->hide();
        ui->imageSelectBox->show();
        ui->CustomizeButton->show();
    }

    ui->imageSelectBox->clear();

    QList<JsonData> dataList = parserData->getJsonData();
    for (int ix = 0; ix < dataList.size(); ix++) {
        QString projectName = dataList.at(ix).name;

        // show images only for selected project
        if (projectName != ui->projectSelectBox->currentText())
            continue;

        QList<QVariantMap> releases = dataList.at(ix).images;
        for (QList<QVariantMap>::const_iterator it = releases.constBegin();
             it != releases.constEnd();
             it++)
        {
            QString imageName = (*it)["name"].toString();
            QString imageChecksum = (*it)["md5"].toString();
            QString imageSize = (*it)["size"].toString();
            QString imageUrl = (*it)["url"].toString();

            int size = imageSize.toInt();
            if (size < 1024) {
                imageSize = QString::number(size) + " B";
            } else if (size < 1024*1024) {
                size /= 1024;
                imageSize = QString::number(size) + " KB";
            } else {
                size /= 1024*1024;
                imageSize = QString::number(size) + " MB";
            }

            checksumMap[imageName] = imageChecksum;

            QVariantMap projectData;
            projectData.insert("name", imageName);
            projectData.insert("url", imageUrl);

            ui->imageSelectBox->addItem(imageName + "  (" + imageSize + ")", projectData);
        }
    }

    reset();
    flashProgressBarText();
}

void Creator::localZipPickerButtonClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
                    tr("Open Local Zip"),
                    ui->LocalZipText->text(),
                    tr("Unraid Flash backup (*.zip);;All files (*.*)"));

    if (!filename.isEmpty()) {
        ui->LocalZipText->setText(filename);
    }
}

void Creator::reset(const QString& message)
{
    bytesDownloaded = 0;
    bytesLast = 0;

    if (imageFile.isOpen())
        imageFile.close();

    ui->projectSelectBox->blockSignals(false);
    ui->projectSelectBox->setEnabled(true);

    ui->imageSelectBox->blockSignals(false);
    ui->imageSelectBox->setEnabled(true);

    ui->CustomizeButton->setEnabled(true);
    ui->CustomizePanel->setEnabled(true);

    ui->EFIBootLocalCheckBox->setEnabled(true);

    ui->LocalZipText->setEnabled(true);
    ui->LocalZipPickerButton->setEnabled(true);

    ui->refreshRemovablesButton->setEnabled(true);
    ui->removableDevicesComboBox->setEnabled(true);

    checkWriteFlashAvailable();

    ui->writeFlashButton->setText(tr("&Write"));

    if (message.isNull() == false) {
        if (state == STATE_DOWNLOADING_IMAGE) {
            ;
        } else if (state == STATE_WRITING_IMAGE) {
            flashProgressBarText(message);
        }
    }

    state = STATE_IDLE;

    // TBD - USB eject/load/remove
}

void Creator::checkWriteFlashAvailable()
{
    QString destination = ui->removableDevicesComboBox->currentData().toMap()["dev"].toString();

    bool passedDestination = (destination.isNull() == false);
    bool passedSource = true;

    if (ui->projectSelectBox->currentText() == "Local Zip")
    {
        QFileInfo checkFile(ui->LocalZipText->text());
        if (ui->LocalZipText->text().isEmpty() || !checkFile.exists() || !checkFile.isFile())
            passedSource = false;
    }

    if (passedDestination && passedSource)
        ui->writeFlashButton->setEnabled(true);
    else
        ui->writeFlashButton->setEnabled(false);
}

void Creator::disableControls()
{
    ui->projectSelectBox->setEnabled(false);
    ui->projectSelectBox->blockSignals(true);
    ui->imageSelectBox->setEnabled(false);
    ui->imageSelectBox->blockSignals(true);
    ui->CustomizeButton->setEnabled(false);
    ui->CustomizePanel->setEnabled(false);
    ui->EFIBootLocalCheckBox->setEnabled(false);
    ui->LocalZipText->setEnabled(false);
    ui->LocalZipPickerButton->setEnabled(false);
    ui->refreshRemovablesButton->setEnabled(false);
    ui->removableDevicesComboBox->setEnabled(false);
    ui->writeFlashButton->setEnabled(false);

    // TBD - USB eject/load/remove
}

bool Creator::isChecksumValid(const QString checksumMd5)
{
    checksum = checksumMap[selectedImage];

    if (checksumMd5.isFilled() && checksumMd5 == checksum)
        return true;  // checksum calculated at download stage

    QByteArray referenceSum, downloadSum;
    QCryptographicHash c(QCryptographicHash::Md5);

    // calculate the md5 sum of the downloaded file
    imageFile.open(QFile::ReadOnly);
    while (!imageFile.atEnd())
        c.addData(imageFile.read(4096));

    downloadSum = c.result().toHex();

    imageFile.close();

    qDebug() << selectedImage << checksum;

    if (checksum.isEmpty() || downloadSum != checksum.toUtf8())
        return false;

    return true;
}

// From http://www.gamedev.net/topic/591402-gzip-uncompressed-file-size/
// Might not be portable!
unsigned int Creator::getUncompressedImageSize(const QString &filename)
{
    FILE *file;
    unsigned int len;
    unsigned char bufSize[4];
    unsigned int fileSize;

#if defined(_WIN32)
    // toStdString internally converts filename to utf8, which
    // windows does not support for fileaccess
    // so use unchanged 16 Bit unicode here (QChar is 16 Bit)
    file = _wfopen((const wchar_t *)filename.utf16(), L"rb");
#else
    file = fopen(filename.toStdString().c_str(), "rb");
#endif
    if (file == NULL)
    {
        emit error("Couldn't open " + filename);
        return 0;
    }

    if (filename.endsWith(".gz")) {
        if (fseek(file, -4, SEEK_END) != 0)
            return 0;

        len = fread(&bufSize[0], sizeof(unsigned char), 4, file);
        if (len != 4) {
            fclose(file);
            return 0;
        }

        fileSize = (unsigned int) ((bufSize[3] << 24) | (bufSize[2] << 16) | (bufSize[1] << 8) | bufSize[0]);
        qDebug() << "Uncompressed gz file size:" << fileSize;
    } else if (filename.endsWith(".zip")) {
        // first check uncompressed size from header
        if (fseek(file, 22, SEEK_SET) != 0)
            return 0;

        len = fread(&bufSize[0], sizeof(unsigned char), 4, file);
        if (len != 4) {
            fclose(file);
            return 0;
        }

        fileSize = (unsigned int) ((bufSize[3] << 24) | (bufSize[2] << 16) | (bufSize[1] << 8) | bufSize[0]);

        // check general-purpose flags
        if (fseek(file, 6, SEEK_SET) != 0)
            return 0;

        len = fread(&bufSize[0], sizeof(unsigned char), 2, file);
        if (len != 2) {
            fclose(file);
            return 0;
        }

        qDebug() << "fileSize" << fileSize << "general-purpose flag" << (bufSize[0] & 0x08);
        if (fileSize == 0 && (bufSize[0] & 0x08) != 0) {
            // get size from structure immediately after the
            // compressed data (at the end of the file)

            // get End of central directory record (EOCD)
            long off;
            for (off = 0;; off++) {
                qDebug() << "off:" << off;
                if (fseek(file, -22 - off, SEEK_END) == -1)
                    break;  // error

                len = fread(&bufSize[0], sizeof(unsigned char), 4, file);
                if (len != 4) {
                    fclose(file);
                    return 0;
                }

                // check End of central directory signature = 0x06054b50
                if (bufSize[3] == 0x06 && bufSize[2] == 0x05 && \
                    bufSize[1] == 0x4b && bufSize[0] == 0x50)
                {
                    qDebug() << "found End of central directory signature = 0x06054b50";
                    break;  // exit loop
                }
            } // for

            off = 16 - 4;  // 4 B already read

            // Offset of start of central directory, relative to start of archive
            if (fseek(file, off, SEEK_CUR) == -1) {
                fclose(file);
                return 0;
            }

            len = fread(&bufSize[0], sizeof(unsigned char), 4, file);
            if (len != 4) {
                fclose(file);
                return 0;
            }

            // calculate offset
            off = (long) ((bufSize[3] << 24) | (bufSize[2] << 16) | (bufSize[1] << 8) | bufSize[0]);

            if (fseek(file, off, SEEK_SET) == -1) {
                fclose(file);
                return 0;
            }

            len = fread(&bufSize[0], sizeof(unsigned char), 4, file);
            if (len != 4) {
                fclose(file);
                return 0;
            }

            // check Central directory file header signature = 0x02014b50
            if (bufSize[3] == 0x02 && bufSize[2] == 0x01 && bufSize[1] == 0x4b && bufSize[0] == 0x50) {
                qDebug() << "found Central directory file header signature = 0x02014b50";
                off = 24 - 4;  // 4 B already read
            }

            if (fseek(file, off, SEEK_CUR) == -1) {
                fclose(file);
                return 0;
            }

            len = fread(&bufSize[0], sizeof(unsigned char), 4, file);
            if (len != 4) {
                fclose(file);
                return 0;
            }

            // Uncompressed size
            fileSize = (unsigned int) ((bufSize[3] << 24) | (bufSize[2] << 16) | (bufSize[1] << 8) | bufSize[0]);

            if (fileSize == 0) {
                qDebug() << "fileSize unknown - set 512 MB";
                fileSize = 512 * 1024 * 1024;   // test
            }
        } // fileSize == 0

        qDebug() << "Uncompressed zip file size:" << fileSize;
    } else {
        fseek(file, 0L, SEEK_END);
        fileSize = ftell(file);
        qDebug() << "Regular file size:" << fileSize;
    }

    fclose(file);
    return fileSize;
}

void Creator::setImageFileName(QString filename)
{
    if (imageFile.isOpen()) {
        qDebug() << "Tried to change filename while imageFile was open!";
        return;
    }

    imageFile.setFileName(filename);
}

QString Creator::getDefaultSaveDir()
{
    static QString defaultDir;
    if (defaultDir.isEmpty()) {
        defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (defaultDir.isEmpty())
            defaultDir = QDir::homePath();
    }

    return defaultDir;
}

void Creator::handleDownloadError(QNetworkReply *reply)
{
    QString message = reply->errorString();
    qDebug() << "Something went wrong with download:" << message;

    switch (state)
    {
        case STATE_GET_VERSION:
            downloadBranches();
            break;

        case STATE_DOWNLOADING_VALIDATION:
            // parse JSON to see if error key exists
            if (reply->error() == QNetworkReply::ContentAccessDenied)
            {
                QJsonDocument jsonDocument = QJsonDocument::fromJson(reply->readAll());
                QJsonObject jsonObject = jsonDocument.object();
                QJsonValue errorKey = jsonObject.value("error");

                QString errorString = "Unknown error occured with this Flash device. Please chose another Flash device for Unraid";
                if (errorKey != QJsonValue::Undefined) {
                    errorString = errorKey.toString();
                }

                QMessageBox::warning(this, "Flash device issue", errorString);
                reset();
            }
            else
            {
                // Maybe couldn't reach key servers? continue with writing image
                downloadImage();
            }
            break;

        default:
            flashProgressBarText(message);
            break;
    }
}

void Creator::handleFinishedDownload(const QByteArray &data)
{
    switch (state) {
        case STATE_GET_VERSION:
            state = STATE_IDLE;
#ifdef FORCE_UPDATE_NOTIFICATION
            checkNewVersion(FORCE_UPDATE_NOTIFICATION);
#else
            checkNewVersion(data);
#endif
            downloadBranches();
            break;

        case STATE_GET_RELEASES:
            if (parserData->getBranches().empty())
                parserData->parseBranches(data);
            else
                parserData->parseVersions(data, manager->currentBranch.value("name").toString());

            downloadNextBranch();
            break;

        case STATE_DOWNLOADING_VALIDATION:
            if (ui->projectSelectBox->currentText() == "Local Zip")
            {
                setImageFileName(ui->LocalZipText->text());
                state = STATE_IDLE;
                writeFlash();
            }
            else
                downloadImage();
            break;

        case STATE_DOWNLOADING_IMAGE:
            // whole data at once (no partial)
            if (bytesDownloaded == 0) {
                flashProgressBarText(tr("Download complete, syncing file..."));
                qApp->processEvents();  // fix this
                handlePartialData(data, data.size());
            }

            imageFile.close();

            ui->flashProgressBar->setValue(0);
            flashProgressBarText(tr("Download complete, verifying checksum..."));

            if (isChecksumValid(imageHash.result().toHex()))
                flashProgressBarText(tr("Download complete, checksum ok."));
            else
                flashProgressBarText(tr("Download complete, checksum not ok."));

            // rename file
            if (imageFile.fileName().endsWith(".temp")) {
                QString newFileName = imageFile.fileName().left(imageFile.fileName().lastIndexOf("."));

                bool success = imageFile.rename(newFileName);
                if (success)
                  qDebug() << "rename ok";
                else
                  qDebug() << "rename error";
            }

            delete averageSpeed;
            state = STATE_IDLE;
            writeFlash();
            break;

        default:
            qDebug() << "handleFinishedDownload default";
            break;
    }
}

void Creator::handlePartialData(const QByteArray &data, qlonglong total)
{
    Q_UNUSED(total);

    if (state != STATE_DOWNLOADING_IMAGE) {
        // what to do in this case?
        qDebug() << "handlePartialData: got unexpected data!";
        return;
    }

    imageFile.write(data);
    imageHash.addData(data);
    bytesDownloaded += data.size();
}

void Creator::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (state != STATE_DOWNLOADING_IMAGE)
        return;

    if (bytesTotal < 10000)
        return;   // skip json file (to be fixed)

    // Update progress bar
    ui->flashProgressBar->setMaximum(bytesTotal);
    ui->flashProgressBar->setValue(bytesReceived);

    // calculate current download speed
    double speed;
    int elapsedTime = speedTime.elapsed();
    if (elapsedTime <= 0)
        elapsedTime = 1;  // don't delete by zero

    speed = (bytesReceived - bytesLast) * 1000.0 / elapsedTime;
    averageSpeed->AddValue(speed);
    speed = averageSpeed->AverageValue();

    double remainingTime = (bytesTotal - bytesReceived) / speed;  // in seconds
    bytesLast = bytesReceived;

    QString unit;
    if (speed < 1024) {
        unit = "bytes/sec";
    } else if (speed < 1024*1024) {
        speed /= 1024;
        unit = "KB/s";
    } else {
        speed /= 1024*1024;
        unit = "MB/s";

        if (speed > 1000)
            speed = 0;
    }

    QString speedText = QString::fromLatin1("%1 %2").arg(speed, 3, 'f', 1).arg(unit);
    QString timeText = QString::number(remainingTime, 'f', 0);
    int percentage = ((double) bytesReceived) / bytesTotal * 100;

    flashProgressBarText(tr("Downloading") + ": " + tr("%1 seconds remaining - %2% at %3").arg(timeText, QString::number(percentage), speedText));

    speedTime.restart();   // start again to get current speed
}

void Creator::downloadVersionCheck()
{
    state = STATE_GET_VERSION;
    disableControls();

    QUrl url(versionUrl);
    manager->get(url);
}

void Creator::downloadBranches()
{
    state = STATE_GET_RELEASES;
    disableControls();

    QUrl url(branchesUrl);
    manager->get(url);
}

void Creator::downloadNextBranch()
{
    state = STATE_GET_RELEASES;
    disableControls();

    QVariantMap branch = parserData->getNextBranchToFetch();

    if (branch.empty()) { // When all the branch files have been downloaded, this will be empty
        populateBranches();
        state = STATE_IDLE;
        return;
    }

    QUrl url(branch.value("url").toString());
    manager->currentBranch = branch;
    manager->get(url);
}

void Creator::checkNewVersion(const QString &verNewStr)
{
    QVersionNumber qVersionNew = QVersionNumber::fromString(verNewStr);
    QVersionNumber qVersionOld = QVersionNumber::fromString(BUILD_VERSION);


    int QVersionCompare = QVersionNumber::compare(qVersionNew, qVersionOld);
    qDebug() << "QVersionCompare" << QVersionCompare;

    if (QVersionCompare <= 0) {
        qDebug() << "no new version";
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("Update Notification"));
#ifdef Q_OS_MACOS
    QAbstractButton *visitButton = msgBox.addButton(tr("&Visit Website"), QMessageBox::NoRole);
    msgBox.addButton(tr("&Close"), QMessageBox::YesRole);
#else
    QAbstractButton *visitButton = msgBox.addButton(tr("&Visit Website"), QMessageBox::YesRole);
    msgBox.addButton(tr("&Close"), QMessageBox::NoRole);
#endif
    QString verHtml = "<font color=\"blue\">" + verNewStr + "</font>";
    QString msg = tr("Unraid USB Creator %1 is available.").arg(verHtml);
    msgBox.setText("<p align='center' style='margin-right:30px'><br>" + msg + "<br></p>");

    msgBox.exec();
    if (msgBox.clickedButton() == visitButton)
      QDesktopServices::openUrl(QUrl(helpUrl));
}

void Creator::downloadAndWriteButtonClicked()
{
    if (state == STATE_WRITING_IMAGE || state == STATE_WAITING_FOR_EXTRACTION) {
        state = STATE_IDLE;
        // cancel flashing
        //privileges.SetUser();
        reset();
        ui->flashProgressBar->setValue(0);
        flashProgressBarText(tr("Writing canceled."));
        diskWriter->cancelWrite();

        QMessageBox::warning(this, tr("Writing canceled."), tr(
            "Writing to the USB key did not finish properly. "
            "If you cancelled the writing process because it appeared to hang, "
            "try reformatting the key with FAT32 or exFAT using your operating system tools."));
        return;
    }

    if (state == STATE_DOWNLOADING_VALIDATION || state == STATE_DOWNLOADING_IMAGE) {
        state = STATE_IDLE;
        // cancel download
        manager->cancelDownload();

        // remove temp file

        if (imageFile.exists() && state == STATE_DOWNLOADING_IMAGE) {
            qDebug() << "Removing file" << imageFile.fileName();
            imageFile.remove();
        }

        reset();
        ui->flashProgressBar->setValue(0);
        flashProgressBarText(tr("Download canceled."));
        return;
    }

    disableControls();

    QString destinationText = ui->removableDevicesComboBox->currentText();
    QString destination = ui->removableDevicesComboBox->currentData().toMap()["dev"].toString();

    if (destination.isNull()) {
        reset();
        qDebug() << "destination is not set";
        return;
    }

    if (devEnumerator->supportsGuid()) {
        QString pid = ui->removableDevicesComboBox->currentData().toMap()["pid"].toString();
        QString serial = ui->removableDevicesComboBox->currentData().toMap()["serial"].toString();
        QString vid = ui->removableDevicesComboBox->currentData().toMap()["vid"].toString();
        
        if (vid.isEmpty() || pid.isEmpty() || serial.isEmpty()) {
            // Missing identifier, GUID will be invalid
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Error"));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(tr("This key is not compatible with Unraid. It contains no unique identifier to generate a license key. Please try with another key."));
            msgBox.exec();
            reset();
            return;
        }
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm write"));
    msgBox.setIcon(QMessageBox::Warning);
    
    QString text = tr("Selected device:\n  %1\n").arg(destinationText);

    if (ui->removableDevicesComboBox->count() > 1) {
        text += "\n\n" + tr("To avoid accidentally selecting a wrong key, we recommend that you remove all keys except the one you wish to use for the Unraid boot drive.");
    }
    
    static const qint64 GB = 1024 * 1024 * 1024;
    static const qint64 WARNING_SIZE = 4 * GB;
    qint64 deviceSize = ui->removableDevicesComboBox->currentData().toMap()["size"].toULongLong();

    if (deviceSize >= WARNING_SIZE) {
        text += "\n\n" + tr("Unraid needs less than 1 Gb of space to run. The USB key you have chosen is quite large for this purpose, and the extra space will not provide additional benefits.");
    }

    text += "\n\n" + tr("Your USB device will be wiped!");
    text += "\n\n" + tr("Are you sure you want to write the image?");

    msgBox.setText(text);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setButtonText(QMessageBox::Yes, tr("Erase and Write"));
    msgBox.setButtonText(QMessageBox::No, tr("Cancel"));
    int ret = msgBox.exec();
    if (ret != QMessageBox::Yes) {
        reset();
        return;
    }

    state = STATE_DOWNLOADING_VALIDATION;
    ui->writeFlashButton->setText(tr("Cance&l"));
    ui->writeFlashButton->setEnabled(true);

    if (devEnumerator->supportsGuid()) {
        // start guid validation
        QString guid = ui->removableDevicesComboBox->currentData().toMap()["guid"].toString();
        qDebug() << "Validating GUID" << guid;
        manager->post(validatorUrl, QString("guid=" + guid).toLocal8Bit());
    } else {
        // skip guid validation and continue on to the next step
        handleFinishedDownload(QByteArray());
    }
}


void Creator::downloadImage()
{
    // start download
    state = STATE_DOWNLOADING_IMAGE;
    ui->writeFlashButton->setText(tr("Cance&l"));
    ui->writeFlashButton->setEnabled(true);

    selectedImage = ui->imageSelectBox->itemData(ui->imageSelectBox->currentIndex()).toMap()["name"].toString();
    qDebug() << "selectedImage" << selectedImage;

    QUrl url = ui->imageSelectBox->itemData(ui->imageSelectBox->currentIndex()).toMap()["url"].toString();
    qDebug() << "Downloading" << url;

    QString saveDir = QDir::tempPath();
    qDebug() << "Saving to" << saveDir + '/' + url.fileName();

    if (saveDir.isEmpty() || url.fileName().isEmpty()) {
        reset();
        return;
    }

    if (imageFile.isOpen())
        imageFile.close();

    privileges.SetRoot();    // root need for downloading

    // remove both files
    QFile fileToRemove(saveDir + "/" + url.fileName());
    if (fileToRemove.exists()) {
        qDebug() << "Removing" << saveDir + "/" + url.fileName();
        fileToRemove.remove();
    }
    fileToRemove.setFileName(saveDir + "/" + url.fileName() + ".temp");
    if (fileToRemove.exists()) {
        qDebug() << "Removing" << saveDir + "/" + url.fileName() + ".temp";
        fileToRemove.remove();
    }

    setImageFileName(saveDir + "/" + url.fileName() + ".temp");

    if (!imageFile.open(QFile::WriteOnly | QFile::Truncate)) {
        reset();
        flashProgressBarText(tr("Failed to open file for writing!"));
        return;
    }

    imageHash.reset();

    qDebug() << "Downloading to" << saveDir + "/" + url.fileName();
    manager->get(url);
    speedTime.start();
    averageSpeed = new MovingAverage(50);
}

void Creator::writeFlash()
{
    QString destinationText = ui->removableDevicesComboBox->currentText();
    QString destination = ui->removableDevicesComboBox->currentData().toMap()["dev"].toString();

    if (destination.isNull()) {
        reset();
        qDebug() << "destination is not set";
        return;
    }

    // unmount partitions (on Linux only)
    privileges.SetRoot();    // root need for opening a device
    bool unmounted = devEnumerator->unmountDevicePartitions(destination);
    qint64 deviceSize = ui->removableDevicesComboBox->currentData().toMap()["size"].toULongLong();

    // Limit to 256GB max (we could do up to 2TB but we would need to use larger than 512 sectors for FAT)
    if (deviceSize > 274877906943) {
        deviceSize = 274877906943;
    }

    quint32 partitionlength = (deviceSize - 1048576) / 512;
    quint8 clustersize = 32;

    // Determine appropriate FAT cluster size to use for this device size
    if (partitionlength < 1046528) {
        reset();
        flashProgressBarText(tr("Flash disk must be at least 512MB or larger"));
        return;
    } else if (partitionlength < 16775168) {
        clustersize = 4;
    } else if (partitionlength < 33552384) {
        clustersize = 8;
    } else if (partitionlength < 67106816) {
        clustersize = 16;
    }

    // extracted the embedded boot img to the same folder as the unraid zip
    QString tempDir = QDir::tempPath();
    bootimageFile.setFileName(tempDir+"/unraid.bootimg.gz");
    qDebug() << "copying" << ":/bin/boot_" + QString::number(clustersize) + "k.img.gz to" << bootimageFile.fileName();
    QFile::copy(":/bin/boot_" + QString::number(clustersize) + "k.img.gz", bootimageFile.fileName());

    uncompressedImageSize = getUncompressedImageSize(bootimageFile.fileName());
    //privileges.SetUser();    // back to user
    if (unmounted == false) {
        reset();
        flashProgressBarText(tr("Cannot unmount partititons on device %1").arg(destinationText));
        return;
    }

    ui->flashProgressBar->setValue(0);
    ui->flashProgressBar->setMaximum(uncompressedImageSize);

    qDebug() << "deviceSize" << deviceSize << "uncompressedImageSize" << uncompressedImageSize;
    if (uncompressedImageSize > deviceSize) {
        QString uncompressedSizeStr = devEnumerator->sizeToHuman(uncompressedImageSize);
        QString deviceSizeStr = devEnumerator->sizeToHuman(deviceSize);
        reset();
        flashProgressBarText(tr("Not enough space on %1 [%2 < %3]").arg(destinationText).arg(deviceSizeStr).arg(uncompressedSizeStr));
        return;
    }

    state = STATE_WRITING_IMAGE;
    privileges.SetRoot();    // root need for opening a device

    ui->writeFlashButton->setText(tr("Cance&l"));
    ui->writeFlashButton->setEnabled(true);

    emit proceedToWriteImageToDevice(bootimageFile.fileName(), destination, partitionlength, clustersize);

    speedTime.start();
    averageSpeed = new MovingAverage(20);
    bytesLast = 0;
}

void Creator::writingSyncing()
{
    qDebug() << "writingSyncing";
    if (state == STATE_WRITING_IMAGE)
        flashProgressBarText(tr("Syncing file system..."));
}

void Creator::writingFinished()
{
    qDebug() << "writingFinished";

    /* if error happened leave it visible */
    if (state != STATE_IDLE) {
        state = STATE_WAITING_FOR_EXTRACTION;

        // remove temp files
        if (bootimageFile.exists()) {
            qDebug() << "Removing" << bootimageFile.fileName();
            bootimageFile.remove();
            bootimageFile.setFileName("");
        }

        // wait for mounted list to show new UNRAID flash mounted
        refreshMountedList();
    }
}

void Creator::writingError(QString message)
{
    qDebug() << "Writing error:" << message;
    //privileges.SetUser();    // back to user
    reset(tr("Error: %1").arg(message));
    delete averageSpeed;
    state = STATE_IDLE;

    QApplication::beep();
}

void Creator::refreshMountedList()
{
    qDebug() << "Refreshing mounted list";

    QTimer::singleShot(500, [&] {
        if (state != STATE_WAITING_FOR_EXTRACTION)
            return;

        qDebug() << "timer fired --> Refreshing mounted list now";

        foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
            if (storage.isValid() && storage.isReady()) {
                if (!storage.isReadOnly()) {
                    if (storage.name() == "UNRAID") {
                        qDebug() << "Found UNRAID drive at: " << storage.rootPath();
                        handleExtractFiles(storage.rootPath());
                        return;
                    }
                }
            }
        }

        refreshMountedList();
    });
}

void Creator::refreshRemovablesList()
{
    // timer is always running but don't enumerate when writing image
    if (state != STATE_IDLE)
        return;

    // don't start the enumeration if one is in progress
    if (enumeratorThreadWatcher.isRunning())
        return;

    //qDebug() << "Refreshing removable devices list";

    enumeratorThreadWatcher.setFuture(QtConcurrent::run([](QPointer<Creator> creator)
    {
        Privileges privileges;
        QScopedPointer<DeviceEnumerator> devEnumerator(makeEnumerator());

        privileges.SetRoot();    // root need for opening a device
        //QStringList devNames = devEnumerator->getRemovableDeviceNames();
        //QStringList friendlyNames = devEnumerator->getUserFriendlyNames(devNames);
        //privileges.SetUser();    // back to user


        //qDebug() << "Scanning for block devices ...";
        QList<QVariantMap> blockDevices = devEnumerator->listBlockDevices();
        //qDebug() << "Found" << blockDevices.count() << "block devices";

        if (creator) {
            creator->handleRemovablesList(blockDevices);
        }
    }, this));
}

void Creator::handleExtractFiles(QString targetpath)
{
    // timer is always running but don't enumerate when writing image
    if (state != STATE_WAITING_FOR_EXTRACTION)
        return;

    state = STATE_EXTRACTING_FILES;
    disableControls();

    qDebug() << "Extracting files to" << targetpath;
    ui->flashProgressBar->setValue(0);
    flashProgressBarText(tr("Extracting files"));
    qApp->processEvents();

    // close the image file if it was still open from the downloader
    if (imageFile.isOpen())
        imageFile.close();

    QFile File(imageFile.fileName());
    if (!File.exists())
    {
        qDebug() << "Zip File not found!";
        return;
    }

    QStringList lst = JlCompress::getFileList(imageFile.fileName());

    ui->flashProgressBar->setMaximum(lst.count());

    emit proceedToExtractFiles(imageFile.fileName(), targetpath);
}

void Creator::handleExtractFilesComplete(const QString &targetpath)
{
    qDebug() << "Extraction complete";

    if (ui->projectSelectBox->currentText() != "Local Zip")
    {
        // remove downloaded files
        if (imageFile.exists())
        {
            qDebug() << "Removing" << imageFile.fileName();
            imageFile.remove();
        }

        // write custom Unraid settings
        if (ui->CustomizeButton->isChecked())
        {
            QFile fileIdent(targetpath+"/config/ident.cfg");
            if (fileIdent.exists())
            {
                fileIdent.open(QIODevice::ReadOnly);
                QString dataText = fileIdent.readAll();
                fileIdent.close();

                dataText.replace("NAME=\"Tower\"", "NAME=\"" + ui->ServerNameText->text() + "\"");

                if (fileIdent.open(QFile::WriteOnly | QFile::Truncate))
                {
                    QTextStream out(&fileIdent);
                    out << dataText;
                }
                fileIdent.close();
            }

            if (ui->EFIBootCheckBox->isChecked())
            {
                QDir dirEFI(targetpath+"/EFI-");
                if (dirEFI.exists())
                {
                    dirEFI.rename(targetpath+"/EFI-", targetpath+"/EFI");
                }
            }

            /*
            // TODO: Open /config/shadow, replace  root:!:  with  root:<password_hash>:
            if (!ui->RootPasswordText->text().isEmpty())
            {
                QFile::copy(":/bin/shadow", targetpath+"/config/shadow");

                //TODO - generate random salt
                QString salt = "S7nS65721g/d";

                QString newHash = sha256_crypt(ui->RootPasswordText->text().toUtf8(), salt.toUtf8());

                qDebug() << "SHA-256 Hashed password:" << newHash;


                QFile fileShadow(":/bin/shadow");
                if (fileShadow.exists())
                {
                    fileShadow.open(QIODevice::ReadOnly);
                    QString dataText = fileShadow.readAll();
                    fileShadow.close();

                    dataText.replace("root:!:", "root:"+newHash+":");

                    QFile fileShadowOut(targetpath+"/config/shadow");
                    if (fileShadowOut.open(QFile::WriteOnly | QFile::Truncate))
                    {
                        QTextStream out(&fileShadow);
                        out << dataText;
                    }
                    fileShadowOut.close();
                }

            }
            */

            if (ui->NetworkStaticRadioButton->isChecked())
            {
                QFile fileNetwork(targetpath+"/config/network.cfg");
                if (fileNetwork.exists())
                {
                    fileNetwork.open(QIODevice::ReadOnly);
                    QString dataText = fileNetwork.readAll();
                    fileNetwork.close();

                    dataText.replace("USE_DHCP=\"yes\"", "USE_DHCP=\"no\"");
                    dataText.replace("IPADDR=", "IPADDR=\"" + ui->IPAddressText->getValue() + "\"");
                    dataText.replace("NETMASK=", "NETMASK=\"" + ui->NetmaskComboBox->currentText() + "\"");
                    dataText.replace("GATEWAY=", "GATEWAY=\"" + ui->GatewayText->getValue() + "\"");
                    dataText.append("DNS_SERVER1=\"" + ui->DNSText->getValue() + "\"\r\n");

                    if (fileNetwork.open(QFile::WriteOnly | QFile::Truncate))
                    {
                        QTextStream out(&fileNetwork);
                        out << dataText;
                    }
                    fileNetwork.close();
                }
            }
        }
    } 
    else 
    {
        if (ui->EFIBootLocalCheckBox->isChecked())
        {
            QDir dirEFI(targetpath+"/EFI-");
            if (dirEFI.exists())
            {
                dirEFI.rename(targetpath+"/EFI-", targetpath+"/EFI");
            }
        }
    }

    QDir dirTarget(targetpath);
    if (dirTarget.mkdir("syslinux"))
    {
        flashProgressBarText(tr("Restoring Syslinux folder"));
        qDebug() << "copying" << ":/bin/syslinux/ldlinux.c32 to" << targetpath+"/syslinux/ldlinux.c32";
        QFile::copy(":/bin/syslinux/ldlinux.c32", targetpath+"/syslinux/ldlinux.c32");
        qDebug() << "copying" << ":/bin/syslinux/libcom32.c32 to" << targetpath+"/syslinux/libcom32.c32";
        QFile::copy(":/bin/syslinux/libcom32.c32", targetpath+"/syslinux/libcom32.c32");
        qDebug() << "copying" << ":/bin/syslinux/libutil.c32 to" << targetpath+"/syslinux/libutil.c32";
        QFile::copy(":/bin/syslinux/libutil.c32", targetpath+"/syslinux/libutil.c32");
        qDebug() << "copying" << ":/bin/syslinux/make_bootable_linux.sh to" << targetpath+"/syslinux/make_bootable_linux.sh";
        QFile::copy(":/bin/syslinux/make_bootable_linux.sh", targetpath+"/syslinux/make_bootable_linux.sh");
        qDebug() << "copying" << ":/bin/syslinux/make_bootable_mac.sh to" << targetpath+"/syslinux/make_bootable_mac.sh";
        QFile::copy(":/bin/syslinux/make_bootable_mac.sh", targetpath+"/syslinux/make_bootable_mac.sh");
        qDebug() << "copying" << ":/bin/syslinux/mboot.c32 to" << targetpath+"/syslinux/mboot.c32";
        QFile::copy(":/bin/syslinux/mboot.c32", targetpath+"/syslinux/mboot.c32");
        qDebug() << "copying" << ":/bin/syslinux/mbr.bin to" << targetpath+"/syslinux/mbr.bin";
        QFile::copy(":/bin/syslinux/mbr.bin", targetpath+"/syslinux/mbr.bin");
        qDebug() << "copying" << ":/bin/syslinux/menu.c32 to" << targetpath+"/syslinux/menu.c32";
        QFile::copy(":/bin/syslinux/menu.c32", targetpath+"/syslinux/menu.c32");
        qDebug() << "copying" << ":/bin/syslinux/syslinux to" << targetpath+"/syslinux/syslinux";
        QFile::copy(":/bin/syslinux/syslinux", targetpath+"/syslinux/syslinux");
        qDebug() << "copying" << ":/bin/syslinux/syslinux_linux to" << targetpath+"/syslinux/syslinux_linux";
        QFile::copy(":/bin/syslinux/syslinux_linux", targetpath+"/syslinux/syslinux_linux");
        qDebug() << "copying" << ":/bin/syslinux/syslinux.cfg to" << targetpath+"/syslinux/syslinux.cfg";
        QFile::copy(":/bin/syslinux/syslinux.cfg", targetpath+"/syslinux/syslinux.cfg");
        qDebug() << "copying" << ":/bin/syslinux/syslinux.cfg- to" << targetpath+"/syslinux/syslinux.cfg-";
        QFile::copy(":/bin/syslinux/syslinux.cfg-", targetpath+"/syslinux/syslinux.cfg-");
        qDebug() << "copying" << ":/bin/syslinux/syslinux.exe to" << targetpath+"/syslinux/syslinux.exe";
        QFile::copy(":/bin/syslinux/syslinux.exe", targetpath+"/syslinux/syslinux.exe");
    }

    flashProgressBarText(tr("Writing Syslinux"));

    QString targetdev = ui->removableDevicesComboBox->currentData().toMap()["dev"].toString();

#if defined(Q_OS_WIN)
    emit proceedToWriteSyslinux(targetpath.left(2));
#elif defined(Q_OS_LINUX)
    emit proceedToWriteSyslinux(targetdev+"1");
#elif defined(Q_OS_MACOS)
    emit proceedToWriteSyslinux(targetdev+"s1");
#endif
}

void Creator::handleWriteSyslinux()
{
    qDebug() << "Writing syslinux complete";

    //privileges.SetUser();    // back to user
    reset();
    ui->flashProgressBar->setValue(0);
    flashProgressBarText(tr("Writing done!"));
    delete averageSpeed;
    state = STATE_IDLE;
    QApplication::beep();
}

QString Creator::getFriendlyName(const QVariantMap& data) const
{
    const QString guid = data["guid"].toString();

    return data["name"].toString() + " " +
        DeviceEnumerator::sizeToHuman(data["size"].toULongLong()) +
        " [" +
        (guid.isEmpty() && devEnumerator->supportsGuid() ? 
            tr("incompatible") : 
            guid) +
        "]";
}

void Creator::handleRemovablesList(QList<QVariantMap> blockDevices)
{
    QVariant previouslySelectedDevice = ui->removableDevicesComboBox->currentData();

    // check for changes
    if (blockDevices.size() == ui->removableDevicesComboBox->count()) {
        // same number, check values too
        bool sameDevices = true;
        for (int i = 0; i < blockDevices.size(); i++) {
            if (getFriendlyName(blockDevices.at(i)).compare(ui->removableDevicesComboBox->itemText(i)) != 0 ||
                blockDevices.at(i)["dev"].toString().compare(ui->removableDevicesComboBox->itemData(i).toMap()["dev"].toString()) != 0) {
                sameDevices = false;
                break;
            }
        }   // for

        if (sameDevices)
            return;
    }

    // disable saving settings
    ui->removableDevicesComboBox->blockSignals(true);
    ui->removableDevicesComboBox->clear();

    for (int i = 0; i < blockDevices.size(); i++) {
        ui->removableDevicesComboBox->addItem(getFriendlyName(blockDevices.at(i)), blockDevices.at(i));
    }

    int idx = ui->removableDevicesComboBox->findData(previouslySelectedDevice, Qt::UserRole);
    if (idx >= 0)
        ui->removableDevicesComboBox->setCurrentIndex(idx);
    else {
        if (state == STATE_DOWNLOADING_IMAGE) {
            state = STATE_IDLE;
            // cancel download
            manager->cancelDownload();

            // remove temp file
            if (imageFile.exists()) {
                qDebug() << "Removing file" << imageFile.fileName();
                imageFile.remove();
            }

            reset();
            ui->flashProgressBar->setValue(0);
            flashProgressBarText(tr("Download canceled because chosen USB Flash device was removed."));
        }
        else
            ui->removableDevicesComboBox->setCurrentIndex(0);  // first one
    }

    // enable saving settings
    ui->removableDevicesComboBox->blockSignals(false);

    checkWriteFlashAvailable();
}

void Creator::handleExtractProgress(int files)
{
   ui->flashProgressBar->setValue(files);
}

void Creator::handleWriteProgress(int written)
{
    if (state != STATE_WRITING_IMAGE)
        return;

    int elapsedTime = speedTime.elapsed();
    if (elapsedTime < 100)
        return;  // at least 100 msec interval

    ui->flashProgressBar->setValue(written);

    // calculate current write speed
    double speed = (written - bytesLast) * 1000.0 / elapsedTime;
    averageSpeed->AddValue(speed);
    speed = averageSpeed->AverageValue();

    double remainingTime = (uncompressedImageSize - written) / speed;  // in seconds
    bytesLast = written;

    QString unit;
    if (speed < 1024) {
        unit = "bytes/sec";
    } else if (speed < 1024*1024) {
        speed /= 1024;
        unit = "KB/s";
    } else {
        speed /= 1024*1024;
        unit = "MB/s";
    }

    QString speedText = QString::fromLatin1("%1 %2").arg(speed, 3, 'f', 1).arg(unit);
    QString timeText = QString::number(remainingTime, 'f', 0);
    int percentage = ((double) written) / uncompressedImageSize * 100;

    flashProgressBarText(tr("Writing") + ": " + tr("%1 seconds remaining - %2% at %3").arg(timeText, QString::number(percentage), speedText));

    speedTime.restart();   // start again to get current speed
}


//=============================================================================
CustomLineEdit::CustomLineEdit(const QString & contents, QWidget *parent) :
    QLineEdit(contents, parent), selectOnMouseRelease(false)
{
    QIntValidator *valid = new QIntValidator(0, 255, this);
    setValidator(valid);
}

void CustomLineEdit::jumpIn()
{
    setFocus();

    selectOnMouseRelease = false;
    selectAll();
}

void CustomLineEdit::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    selectOnMouseRelease = true;
}

void CustomLineEdit::keyPressEvent(QKeyEvent * event)
{
    int key = event->key();
    int cursorPos = cursorPosition();

    // Jump forward by Space or Period
    if (key == Qt::Key_Space || (key == Qt::Key_Period)) {
        emit jumpForward();
        event->accept();
        return;
    }

    // Jump Backward only from 0 cursor position
    if (cursorPos == 0) {
        if ((key == Qt::Key_Left) || (key == Qt::Key_Backspace)) {
            emit jumpBackward();
            event->accept();
            return;
        }
    }

    // Jump forward from last postion by right arrow
    if (cursorPos == text().count()) {
        if (key == Qt::Key_Right) {
            emit jumpForward();
            event->accept();
            return;
        }
    }

    // After key is placed cursor has new position
    QLineEdit::keyPressEvent(event);
    int freshCurPos = cursorPosition();

    if ((freshCurPos == 3) && (key != Qt::Key_Right))
        emit jumpForward();
}

void CustomLineEdit::mouseReleaseEvent(QMouseEvent *event)
{
    if(!selectOnMouseRelease)
        return;

    selectOnMouseRelease = false;
    selectAll();

    QLineEdit::mouseReleaseEvent(event);
}

//=============================================================================
void makeCommonStyle(QLineEdit* line) {
    line->setContentsMargins(0, 0, 0, 0);
    line->setAlignment(Qt::AlignCenter);
    line->setStyleSheet("QLineEdit { border: 0px none; }");
    line->setFrame(false);
    //line->setMinimumHeight(20);
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

QLineEdit* makeIpSpliter() {
    QLineEdit *spliter = new QLineEdit(".");
    makeCommonStyle(spliter);

    spliter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    spliter->setMaximumWidth(6);
    spliter->setReadOnly(true);
    spliter->setFocusPolicy(Qt::NoFocus);
    return spliter;
}

CustomIpEditor::CustomIpEditor(QWidget *parent) :
    QFrame(parent)
{
    //setContentsMargins(0, 0, 0, 0);
    //setMaximumWidth(130);
    //setMinimumWidth(130);
    setFrameStyle(QFrame::StyledPanel);

    QList <CustomLineEdit *>::iterator linesIterator;

    lines.append(new CustomLineEdit);
    lines.append(new CustomLineEdit);
    lines.append(new CustomLineEdit);
    lines.append(new CustomLineEdit);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    for (linesIterator = lines.begin(); linesIterator != lines.end(); ++linesIterator) {
        makeCommonStyle(*linesIterator);
        mainLayout->addWidget(*linesIterator);

        if (*linesIterator != lines.last()) {
            connect(*linesIterator, &CustomLineEdit::jumpForward,
                    *(linesIterator+1), &CustomLineEdit::jumpIn);
            mainLayout->addWidget(makeIpSpliter());
        }
        if (*linesIterator != lines.first()) {
            connect(*linesIterator, &CustomLineEdit::jumpBackward,
                    *(linesIterator-1), &CustomLineEdit::jumpIn);
        }
    }
}

QString CustomIpEditor::getValue() {
    QString returnValue;
    QList <CustomLineEdit *>::iterator linesIterator;

    for (linesIterator = lines.begin(); linesIterator != lines.end(); ++linesIterator) {
        returnValue.append((*linesIterator)->text());

        if (*linesIterator != lines.last()) {
            returnValue.append(".");
        }
    }

    return returnValue;
}
