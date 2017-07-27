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

#ifndef JSONPARSER_H
#define JSONPARSER_H

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QDebug>

class JsonData
{
  public:
    JsonData() {}

    JsonData(QString name, QList<QMap<QString, QVariant>> &images)
    {
         addData(name, images);
    }

    void addData(QString name, QList<QMap<QString, QVariant>> &images)
    {
        JsonData::name = name;
        JsonData::images = images;
    }

    bool operator== (const JsonData &data) const
    {
        if (data.name == this->name)
            return true;

        return false;
    }

    QString name;
    QList<QVariantMap> images;
};

class JsonParser
{
public:
    JsonParser() {}
    JsonParser(const QByteArray &data);
    void addExtra(const QByteArray &data, const QString label);
    void parseAndSet(const QByteArray &data, const QString label);
    QList<JsonData> getJsonData() const;

private:
    QList<JsonData> dataList;
};

#endif // JSONPARSER_H
