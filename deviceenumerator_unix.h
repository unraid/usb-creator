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

#ifndef DEVICEENUMERATOR_UNIX_H
#define DEVICEENUMERATOR_UNIX_H

#include "deviceenumerator.h"
#include <QStringList>

class DeviceEnumerator_unix : public DeviceEnumerator
{
public:
    QStringList getRemovableDeviceNames() const;
    QStringList getUserFriendlyNames(const QStringList& devices) const;
    bool unmountDevicePartitions(const QString &device) const;
    qint64 getSizeOfDevice(const QString &device) const;
    int loadEjectDrive(const QString &device, const loadEject action) const
    {
        Q_UNUSED(device);
        Q_UNUSED(action);
        return 0;
    }
    int removeDrive(const QString &device) const
    {
        Q_UNUSED(device);
        return 0;
    }
    QList<QVariantMap> listBlockDevices() const
    {
        QList<QVariantMap> ValidList;

        QStringList devNames = getRemovableDeviceNames();

        foreach (QString device, devNames) {
            qint64 size = getSizeOfDevice(device);
            QString label = getFirstPartitionLabel(device);

            QVariantMap projectData;
            projectData.insert("pid", "");      // todo
            projectData.insert("vid", "");      // todo
            projectData.insert("serial", "");   // todo
            projectData.insert("guid", "");     // todo
            projectData.insert("name", device);
            projectData.insert("size", size);
            projectData.insert("dev", device);

            ValidList.append(projectData);
        }

        return ValidList;
    }

private:
    bool checkIsMounted(const QString& device) const;
    bool checkIfUSB(const QString& device) const;
    QStringList getDeviceNamesFromSysfs() const;
    QString getFirstPartitionLabel(const QString& device) const;
#if defined(Q_OS_LINUX)
    QStringList getPartitionsInfo(const QString &device) const;
    bool unmount(const QString& what) const;
#endif
};

#endif // DEVICEENUMERATOR_UNIX_H
