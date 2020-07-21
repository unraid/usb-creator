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

#ifndef DEVICEENUMERATOR_LINUX_H
#define DEVICEENUMERATOR_LINUX_H

#include "deviceenumerator.h"
#include <QStringList>

class DeviceEnumerator_linux : public DeviceEnumerator
{
public:
    QStringList getRemovableDeviceNames() const override;
    QStringList getUserFriendlyNames(const QStringList& devices) const override;
    bool unmountDevicePartitions(const QString &device) const override;
    qint64 getSizeOfDevice(const QString &device) const override;
    int loadEjectDrive(const QString &device, const loadEject action) const override
    {
        Q_UNUSED(device);
        Q_UNUSED(action);
        return 0;
    }
    int removeDrive(const QString &device) const override
    {
        Q_UNUSED(device);
        return 0;
    }
    QList<QVariantMap> listBlockDevices() const override;
    bool supportsGuid() const override { return false; } // TODO 

private:
    bool checkIsMounted(const QString& device) const;
    bool checkIfUSB(const QString& device) const;
    QStringList getDeviceNamesFromSysfs() const;
    QString getFirstPartitionLabel(const QString& device) const;
    QStringList getPartitionsInfo(const QString &device) const;
    bool unmount(const QString& what) const;
};

#endif // DEVICEENUMERATOR_LINUX_H
