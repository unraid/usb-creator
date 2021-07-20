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

#include "jsonparser.h"

#include <QDebug>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

void JsonParser::addExtra(const QByteArray &data, const QString label)
{
    parseVersions(data, label);
}

QVariantMap JsonParser::getNextBranchToFetch()
{
    if (dataBranches.empty())
        return QVariantMap();

    if (dataList.empty())
        return dataBranches.first();

    for (int ix = 0; ix < dataBranches.size(); ix++) {
        bool found = false;
        for (int ij = 0; ij < dataList.size(); ij++) {
            if (dataList.at(ij).name == dataBranches.at(ix).value("name").toString()) {
                found = true;
                break;
            }
        }

        if (!found)
            return dataBranches.at(ix);
    }

    return QVariantMap();
}

void JsonParser::parseBranches(const QByteArray &data)
{
    //qDebug() << "parseBranches data:" << data;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(data);
    QJsonArray jsonArray = jsonDocument.array();
    for (const QJsonValue branch : jsonArray)
    {
        dataBranches.append(branch.toObject().toVariantMap());
        qDebug() << "parseBranches added branch" << branch.toObject().value("name").toString();
    }
}

void JsonParser::parseVersions(const QByteArray &data, const QString &projectName)
{
    //qDebug() << "parseVersions data:" << data;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(data);
    QJsonArray jsonArray = jsonDocument.array();
    QList<QVariantMap> images;
    for (const QJsonValue version : jsonArray)
    {
        images.append(version.toObject().toVariantMap());
        qDebug() << "parseVersions added version" << version.toObject().value("name").toString() << "to branch" << projectName;
    }
    JsonData projectData(projectName, images);
    dataList.append(projectData);
}

QList<JsonData> JsonParser::getJsonData() const
{
    return dataList;
}

QList<QVariantMap> JsonParser::getBranches() const
{
    return dataBranches;
}
