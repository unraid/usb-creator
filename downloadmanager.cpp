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

#include "downloadmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>

DownloadManager::DownloadManager(QObject *parent) :
    QObject(parent),
    manager(new QNetworkAccessManager(this)),
    latestReply(NULL)
{
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(handleGetFinished(QNetworkReply*)));
}

DownloadManager::~DownloadManager()
{
    delete manager;
}

QNetworkReply* DownloadManager::get(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setRawHeader("Connection", "keep-alive");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Wget/1.14 (linux-gnu)");
    qDebug() << "Getting" << url;

    latestReply = manager->get(req);
#if defined(Q_OS_WIN)
    latestReply->ignoreSslErrors();
#endif

    connect(latestReply, SIGNAL(downloadProgress(qint64,qint64)), SLOT(handleProgress(qint64,qint64)));
    return latestReply;
}

QNetworkReply* DownloadManager::post(const QUrl &url, const QByteArray &body)
{
    QNetworkRequest req(url);
    req.setRawHeader("Connection", "keep-alive");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Wget/1.14 (linux-gnu)");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    qDebug() << "Posting" << url;

    latestReply = manager->post(req, body);
#if defined(Q_OS_WIN)
    latestReply->ignoreSslErrors();
#endif

    return latestReply;
}

void DownloadManager::cancelDownload()
{
    if (latestReply)
        latestReply->abort();
}

void DownloadManager::handleProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void DownloadManager::handleGetFinished(QNetworkReply *reply)
{
    if(reply->error() == QNetworkReply::NoError) {
        QUrl redirectionUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        int responseCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt();

        qDebug() << "Handling get request with response code" << responseCode;

        switch (responseCode) {
        case RESPONSE_FOUND:
            redirectionUrl = reply->header(QNetworkRequest::LocationHeader).toUrl();
            // Fallthrough to redirect is required, hence no break;
#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (defined(__cplusplus) && __cplusplus >= 201703L)
            [[fallthrough]];
#endif
        case RESPONSE_REDIRECT:
            if (redirectionUrl.isValid()) {
                qDebug() << reply->url() << "redirected to" << redirectionUrl;

                // make sure we don't send out unnecessary get requests
                if (latestReply && latestReply->url() == redirectionUrl)
                    break;

                // prepare for download
                get(redirectionUrl);
                connect(latestReply, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
            } else {
                qDebug() << "Redirected but no redirection url?!";
            }
            break;

        case RESPONSE_OK:
            qDebug() << "Downloaded" << reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() << "bytes from" << reply->url().toString();
            emit downloadComplete(reply->readAll());
            break;

        case RESPONSE_PARTIAL:
            handleReadyRead();
            break;

        default:
            qDebug() << "Unhandled reply:" << responseCode;
            foreach (const QByteArray& header, reply->rawHeaderList())
                qDebug() << header << reply->rawHeader(header);

            break;
        }
    } else {
        //qDebug() << "Something went wrong with the get request:" << reply->errorString();
        emit downloadError(reply);
    }

    reply->deleteLater();
    if (reply == latestReply)
        latestReply = NULL;
}

void DownloadManager::handleReadyRead()
{
    if (latestReply->error() == QNetworkReply::NoError) {
        int responseCode = latestReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt();
        if (responseCode == 200) {
            if (!latestReply->bytesAvailable()) {
                qDebug() << "ReadyRead() with 0 bytes available :S";
                return;
            }

            qlonglong total = latestReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            emit partialData(latestReply->readAll(), total);
        }
    } else {
        qDebug() << "HandleReadyRead(): Something went wrong with the get request:" << latestReply->errorString();
        latestReply->deleteLater();
        latestReply = NULL;
    }
}
