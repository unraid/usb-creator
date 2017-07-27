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

#include "deviceenumerator_unix.h"

#include <QDebug>
#include <QTextStream>
#include <QDir>
#include <QProcess>
#if defined(Q_OS_LINUX)
#include <blkid/blkid.h>
#include <unistd.h>
#include <sys/mount.h>
#endif

// show only USB devices
#define SHOW_ONLY_USB_DEVICES

QStringList DeviceEnumerator_unix::getRemovableDeviceNames() const
{
    QStringList names;
    QStringList unmounted;

#ifdef Q_OS_LINUX
    names = getDeviceNamesFromSysfs();

    foreach (QString device, names) {
        // show all devices but unmount it before writing
        //if (! checkIsMounted(device))
            unmounted << "/dev/"+device;
    }

    return unmounted;
#else
    QProcess lsblk;
    lsblk.start("diskutil list", QIODevice::ReadOnly);
    lsblk.waitForStarted();
    lsblk.waitForFinished();

    QString device = lsblk.readLine();
    while (!lsblk.atEnd()) {
        device = device.trimmed(); // Odd trailing whitespace

        if (device.startsWith("/dev/disk")) {
            QString name = device.split(QRegExp("\\s+")).first();
            // We only want to add USB devics
            if (this->checkIfUSB(name))
                names << name;
        }

        device = lsblk.readLine();
    }

    return names;
#endif
}

QStringList DeviceEnumerator_unix::getUserFriendlyNames(const QStringList &devices) const
{
    QStringList returnList;

    foreach (QString device, devices) {
        qint64 size = getSizeOfDevice(device);

        QTextStream friendlyName(&device);

        QString label = getFirstPartitionLabel(device);
        if (label.isEmpty())
            friendlyName << " [" << sizeToHuman(size) << "]";
        else
            friendlyName << " [" << label << ", " << sizeToHuman(size) + "]";

        returnList.append(device);
    }

    return returnList;
}

bool DeviceEnumerator_unix::unmountDevicePartitions(const QString &device) const
{
#ifdef Q_OS_MACOS
    Q_UNUSED(device);
    return true;
#else
    if (checkIsMounted(device) == false)
        return true;    // not mounted

    blkid_probe pr;
    blkid_partlist ls;
    int nparts, i;

    pr = blkid_new_probe_from_filename(qPrintable(device));
    if (!pr) {
        qDebug() << "unmountDevicePartitions: Failed to open" << device;
        return false;
    }

    ls = blkid_probe_get_partitions(pr);
    if (ls == NULL) {
        qDebug() << "unmountDevicePartitions: Failed to get partitions";
        blkid_free_probe(pr);

        // no partitions but mounted means it is whole disk formatted
        if (unmount(device) == true)
            return true; // not mounted anymore

        return false;
    }

    nparts = blkid_partlist_numof_partitions(ls);
    blkid_free_probe(pr);

    if (nparts < 0) {
        qDebug() << "unmountDevicePartitions: Failed to determine number of partitions";
        return false;
    }

    // partitions starts with 1
    for (i = 1; i <= nparts; i++) {
        QString partition = QString("%1%2").arg(device).arg(i);

        if (unmount(partition) == false)
            return false; // still mounted
    }

    return true;
#endif
}

bool DeviceEnumerator_unix::checkIsMounted(const QString &device) const
{
    qDebug() << "checkIsMounted " << device;
    char buf[2];
    QFile mountsFile("/proc/mounts");
    if (!mountsFile.open(QFile::ReadOnly)) {
        qDebug() << "Failed to open" << mountsFile.fileName();
        return true;
    }

    // QFile::atEnd() is unreliable for proc
    while (mountsFile.read(buf, 1) > 0) {
        QString line = mountsFile.readLine();
        line.prepend(buf[0]);
        if (line.contains(device))
            return true;
    }

    return false;
}

bool DeviceEnumerator_unix::checkIfUSB(const QString &device) const
{
#ifndef SHOW_ONLY_USB_DEVICES
    return true;
#endif

#ifdef Q_OS_LINUX
    QString path = "/sys/block/" + device;
    QByteArray devPath(256, '\0');
    ssize_t rc = readlink(path.toLocal8Bit().data(), devPath.data(), devPath.size());
    if (rc && devPath.contains("usb"))
        return true;

    return false;
#else
    QProcess lssize;
    lssize.start(QString("diskutil info %1").arg(device), QIODevice::ReadOnly);
    lssize.waitForStarted();
    lssize.waitForFinished();

    QString s = lssize.readLine();
    while (!lssize.atEnd()) {
         if (s.contains("Protocol:") && s.contains("USB"))
             return true;

         s = lssize.readLine();
    }

    return false;
#endif
}

QStringList DeviceEnumerator_unix::getDeviceNamesFromSysfs() const
{
    QStringList names;

    QDir currentDir("/sys/block");
    currentDir.setFilter(QDir::Dirs);

    QStringList entries = currentDir.entryList();
    foreach (QString device, entries) {
        // Skip "." and ".." dir entries
        if (device == "." || device == "..")
            continue;

        if (device.startsWith("sd") && checkIfUSB(device))
            names << device;
    }

    return names;
}

#if defined(Q_OS_LINUX)
qint64 DeviceEnumerator_unix::getSizeOfDevice(const QString& device) const
{
    blkid_probe pr;

    pr = blkid_new_probe_from_filename(qPrintable(device));
    if (!pr) {
        qDebug() << "getSizeOfDevice: Failed to open" << device;
        return 0;
    }

    blkid_loff_t size = blkid_probe_get_size(pr);
    blkid_free_probe(pr);
    qDebug() << "getSizeOfDevice: size" << size << "device" << device;
    return size;
}

QStringList DeviceEnumerator_unix::getPartitionsInfo(const QString& device) const
{
    blkid_probe pr;
    blkid_partlist ls;
    int nparts, i;
    QStringList partList;

    pr = blkid_new_probe_from_filename(qPrintable(device));
    if (!pr) {
        qDebug() << "getPartitionsInfo: Failed to open" << device;
        return partList;
    }

    ls = blkid_probe_get_partitions(pr);
    if (ls == NULL) {
        qDebug() << "Failed to get partitions";
        blkid_free_probe(pr);
        return partList;
    }

    nparts = blkid_partlist_numof_partitions(ls);
    if (nparts < 0) {
        qDebug() << "Failed to determine number of partitions";
        blkid_free_probe(pr);
        return partList;
    }

    for (i = 0; i < nparts; i++) {
        blkid_partition par = blkid_partlist_get_partition(ls, i);
        if (par == NULL)
            continue;

        QString partition;
        QTextStream stream(&partition);
        stream << "#" << blkid_partition_get_partno(par) << " ";
        //stream << " " << blkid_partition_get_start(par);
        stream << " " << sizeToHuman(blkid_partition_get_size(par)*512);
        //stream << " 0x" << hex << blkid_partition_get_type(par);
        //stream << " (" << QString(blkid_partition_get_type_string(par)) << ")";
        //stream << " " << QString(blkid_partition_get_name(par));
        //stream << " " << QString(blkid_partition_get_uuid(par));
        partList << partition;
    }

    blkid_free_probe(pr);
    return partList;

    /*
    if (blkid_do_probe(pr) != 0) {
        qDebug() << "Probing failed on" << device;
        blkid_free_probe(pr);
        continue;
    }

    if (blkid_probe_has_value(pr, "LABEL") == 0) {
        qDebug() << "No label for" << device;
        blkid_free_probe(pr);
        continue;
    }

    if (blkid_probe_lookup_value(pr, "LABEL", &label, NULL) != 0) {
        qDebug() << "Failed to lookup LABEL for" << device;
        blkid_free_probe(pr);
        continue;
    }*/
}

QString DeviceEnumerator_unix::getFirstPartitionLabel(const QString& device) const
{
    blkid_probe pr;
    blkid_probe prPart;
    blkid_partlist ls;
    int nparts;
    int rv;
    QString qLabel;

    pr = blkid_new_probe_from_filename(qPrintable(device));
    if (!pr) {
        qDebug() << "getPartitionsInfo: Failed to open" << device;
        return qLabel;
    }

    ls = blkid_probe_get_partitions(pr);
    if (ls == NULL) {
        qDebug() << "Failed to get partitions";
        blkid_free_probe(pr);
        return qLabel;
    }

    nparts = blkid_partlist_numof_partitions(ls);
    if (nparts < 0) {
        qDebug() << "Failed to determine number of partitions";
        blkid_free_probe(pr);
        return qLabel;
    }

    // at least one partititon
    char devName[64];
    const char *label = NULL;

    if (device.startsWith("/dev/mmcblk")) {
        // check /dev/mmcblk0p1
        snprintf(devName, sizeof(devName), "%sp1", qPrintable(device));
    } else {
        // check /dev/sdb1
        snprintf(devName, sizeof(devName), "%s1", qPrintable(device));
    }

    prPart = blkid_new_probe_from_filename(devName);
    if (prPart == NULL)
        return qLabel;  // no label

    rv = blkid_do_probe(prPart);
    if (rv != 0)
        return qLabel;  // no label

    rv = blkid_probe_lookup_value(prPart, "LABEL", &label, NULL);

    blkid_free_probe(prPart);
    blkid_free_probe(pr);

    if (rv != 0)
        return qLabel;  // no label

    qDebug() << "devName" << devName << "label" << label;
    if (label != NULL)
        qLabel = QString::fromLatin1(label);

    return qLabel;
}

bool DeviceEnumerator_unix::unmount(const QString& what) const
{
    QProcess cmd;
    cmd.start("umount " + what, QIODevice::ReadOnly);
    cmd.waitForStarted();
    cmd.waitForFinished();

    qDebug() << "unmount: checking" << what;
    if (checkIsMounted(what) == true) {
        qDebug() << "unmount: failed";
        return false; // still mounted
    }

    qDebug() << "unmount: done";
    return true;
}
#else
qint64 DeviceEnumerator_unix::getSizeOfDevice(const QString& device) const
{
    QProcess lsblk;
    QString output;

    lsblk.start(QString("diskutil info %1").arg(device), QIODevice::ReadOnly);
    lsblk.waitForStarted();
    lsblk.waitForFinished();

    QString size;
    output = lsblk.readLine();
    while (!lsblk.atEnd()) {
        output = output.trimmed(); // Odd trailing whitespace
        if (output.contains("Total Size:") ||
            output.contains("Disk Size:")) {
            // Total Size:  574.6 MB (574619648 Bytes) (exactly 1122304 512-Byte-Units)
            // on 2015 Macbook Pro 15" running MacOS Sierra beta
            // Disk Size:                15.9 GB (15931539456 Bytes) (exactly 31116288 512-Byte-Units)
            QStringList sizeList = output.split('(').value(1).split(' ');
            size = sizeList.first().trimmed();
            break;
        }

        output = lsblk.readLine();
    }

    return size.toLongLong();
}

QString DeviceEnumerator_unix::getFirstPartitionLabel(const QString& device) const
{
    QProcess lsblk;
    QString output;
    QString label;

    lsblk.start(QString("diskutil info %1s1").arg(device), QIODevice::ReadOnly);
    lsblk.waitForStarted();
    lsblk.waitForFinished();

    output = lsblk.readLine();
    while (!lsblk.atEnd()) {
        output = output.trimmed(); // Odd trailing whitespace
        if (output.contains("Volume Name:")) {
            // Volume Name:              UNRAID
            QStringList tokens = output.split(":");
            label = tokens[1].trimmed();
            break;
        }

        output = lsblk.readLine();
    }

    return label;
}
#endif
