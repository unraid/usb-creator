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

#ifndef DISKWRITER_WINDOWS_H
#define DISKWRITER_WINDOWS_H

#include "diskwriter.h"
#include <QFile>
#include <windows.h>

class DiskWriter_windows : public DiskWriter
{
    Q_OBJECT
public:
    explicit DiskWriter_windows(QObject *parent = 0);
    ~DiskWriter_windows();

    static QString errorAsString(DWORD error);

private:
    friend class DeviceEnumerator_windows;
    QFile dev;

    bool open(const QString& device);
    void close();
    void sync();
    bool isOpen();
    bool write(const QByteArray &data);

    HANDLE hVolume;
    HANDLE hRawDisk;

    static HANDLE getHandleOnPhysicalDrive(const QString &devicename, DWORD access);
    static HANDLE getHandleOnDevice(const QString &device, DWORD access);
    static HANDLE getHandleOnVolume(const QString &volume, DWORD access);
    bool getLockOnVolume(HANDLE handle) const;
    bool removeLockOnVolume(HANDLE handle) const;
    bool unmountVolume(HANDLE handle) const;
    bool isVolumeMounted(HANDLE handle) const;
    static ULONG deviceNumberFromName(const QString &device);
};

#endif // DISKWRITER_WINDOWS_H
