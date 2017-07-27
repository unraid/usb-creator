////////////////////////////////////////////////////////////////////////////////
//      This file is part of unRAID USB Creator - https://github.com/limetech/usb-creator
//      Copyright (C) 2013-2015 RasPlex project
//      Copyright (C) 2016 Team LibreELEC
//      Copyright (C) 2017 Lime Technology, Inc
//
//  unRAID USB Creator is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  unRAID USB Creator is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with unRAID USB Creator.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////////////

#include "jsonparser.h"

#include <QDebug>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QStandardPaths>
#include <QCollator>
#include <algorithm>

JsonParser::JsonParser(const QByteArray &data)
{
    parseAndSet(data, "");
}

void JsonParser::addExtra(const QByteArray &data, const QString label)
{
    parseAndSet(data, label);
}

void JsonParser::parseAndSet(const QByteArray &data, const QString label)
{
    //qDebug() << "parseAndSet data:" << data;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(data);
    QJsonObject jsonObject = jsonDocument.object();

    // get project versions (7.0, 8.0, ...)
    for (QJsonObject::Iterator itProjectVersions  = jsonObject.begin();
                               itProjectVersions != jsonObject.end();
                               itProjectVersions++)
    {
        QString projectName = itProjectVersions.key();

        if (label != "")
            projectName = projectName + " - " + label;

        // get projects (unRAID 6.3.5, unRAID 6.4.0, ...)
        QList<QVariantMap> images;
        QJsonArray val = itProjectVersions.value().toArray();
        foreach (const QJsonValue & itProjects, val)
        {
            images.append(itProjects.toObject().toVariantMap());
        }

        JsonData projectData(projectName, images);
        dataList.append(projectData);
    }
/*
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseSensitive);

    std::sort(dataList.begin(), dataList.end(),
              [&collator](const JsonData &proj1, const JsonData &proj2)
         {return collator.compare(proj1.name, proj2.name) > 0;});

    for (int ix = 0; ix < dataList.size(); ix++)
    {
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseSensitive);

        std::sort(dataList[ix].images.begin(), dataList[ix].images.end(),
                  [&collator](const QVariantMap &image1, const QVariantMap &image2)
             {return collator.compare(image1["name"].toString(),
                                      image2["name"].toString()) > 0;});
    }
    */
}

QList<JsonData> JsonParser::getJsonData() const
{
    return dataList;
}
