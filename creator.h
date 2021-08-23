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

#ifndef CREATOR_H
#define CREATOR_H

#include <QCryptographicHash>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QFile>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTime>

#include "downloadmanager.h"
#include "jsonparser.h"
#include "movingaverage.h"
#include "translator.h"

#ifdef Q_OS_UNIX
#include "privileges_unix.h"
#else
#include "privileges.h"
#endif

// useful macro
#define isFilled()  isEmpty() == false

class QThread;
class DiskWriter;
class DeviceEnumerator;

namespace Ui {
class Creator;
}

class Creator : public QDialog
{
    Q_OBJECT

public:
    explicit Creator(Privileges &priv, QWidget *parent = 0);
    ~Creator();
    void setArgFile(QString argFile);
    void retranslateUi();
    void keyPressEvent(QKeyEvent *);
    //void closeEvent(QCloseEvent *);
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void refreshMountedList();
    void handleExtractFiles(QString targetpath);
    void handleRemovablesList(QList<QVariantMap> blockDevices);

private:
    Ui::Creator *ui;
    DownloadManager* manager;
    Translator *translator;

    void parseAndSetLinks(const QByteArray &data);
    void saveAndUpdateProgress(QNetworkReply *reply);
    void disableControls();
    bool isChecksumValid(const QString);

    QByteArray rangeByteArray(qlonglong first, qlonglong last);
    QNetworkRequest createRequest(QUrl &url, qlonglong first, qlonglong last);
    unsigned int getUncompressedImageSize(const QString &filename);
    void setImageFileName(QString filename);
    QString getDefaultSaveDir();
    bool showRootMessageBox();

    enum {
        RESPONSE_OK = 200,
        RESPONSE_PARTIAL = 206,
        RESPONSE_FOUND = 302,
        RESPONSE_REDIRECT = 307,
        RESPONSE_BAD_REQUEST = 400,
        RESPONSE_NOT_FOUND = 404
    };
    enum {
        STATE_IDLE,
        STATE_GET_VERSION,
        STATE_GET_RELEASES,
        STATE_DOWNLOADING_IMAGE,
        STATE_DOWNLOADING_VALIDATION,
        STATE_WRITING_IMAGE,
        STATE_EXTRACTING_FILES,
        STATE_WAITING_FOR_EXTRACTION
    } state;
    enum {
        STACK_WIDGET_MAIN = 0,
        STACK_WIDGET_ABOUT = 1
    };

    int timerId;
    QTimer *devicesTimer;
    qlonglong bytesDownloaded;
    QCryptographicHash imageHash;
    QFile imageFile;
    QFile bootimageFile;
    QString downloadUrl;
    QString downloadFileSize;
    QString checksum;
    QString selectedImage;
    QMap<QString, QString> checksumMap;
    DiskWriter *diskWriter;
    QThread* diskWriterThread;
    DeviceEnumerator* devEnumerator;
    static const int timerValue;
    static const QString branchesUrl;
    static const QString versionUrl;
    static const QString validatorUrl;
    static const QString helpUrl;
    static const QString efiInfoUrl;
    JsonParser *parserData;
    QElapsedTimer speedTime;
    qlonglong bytesLast;
    MovingAverage *averageSpeed;
    unsigned int uncompressedImageSize;
    Privileges privileges;
    QString deviceEjected;
    QFutureWatcher<void> enumeratorThreadWatcher;

protected:
    void timerEvent(QTimerEvent *event);

private:
    QString getFriendlyName(const QVariantMap& data) const;

signals:
    void proceedToWriteImageToDevice(const QString& image, const QString& device, quint32 partitionlength, quint8 clustersize);
    void proceedToExtractFiles(const QString& sourcezip, const QString& targetpath);
    void proceedToWriteSyslinux(const QString& device);
    void error(const QString& message);
    void bytesExtracted(int);
    void bytesTotal(int);

private slots:
    void httpsUrlHandler(const QUrl &url);
    void changeEvent(QEvent * e);
    void showAbout();
    void closeAbout();

    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleFinishedDownload(const QByteArray& data);
    void handlePartialData(const QByteArray& data, qlonglong total);
    void handleDownloadError(QNetworkReply *reply);
    void downloadVersionCheck();
    void checkNewVersion(const QString &version);
    void downloadBranches();
    void downloadNextBranch();
    void populateBranches();
    void setProjectImages();
    void localZipPickerButtonClicked();
    void refreshRemovablesList();
    void infoEFIClicked();
    void downloadAndWriteButtonClicked();
    void downloadImage();
    void writeFlash();
    void writingSyncing();
    void writingFinished();
    void writingError(QString);
    void reset(const QString& message = "");
    void languageChange();
    void checkWriteFlashAvailable();
    void flashProgressBarText(const QString &text = "");
    void handleWriteProgress(int written);
    void handleExtractProgress(int files);
    void handleExtractFilesComplete(const QString &targetpath);
    void handleWriteSyslinux();

};

#endif // CREATOR_H
