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

#include "deviceenumerator_linux.h"

#include <QDebug>
#include <QTextStream>
#include <QDir>
#include <QProcess>
#include <blkid/blkid.h>
#include <unistd.h>
#include <sys/mount.h>

// show only USB devices
#define SHOW_ONLY_USB_DEVICES

QStringList DeviceEnumerator_linux::getRemovableDeviceNames() const
{
    QStringList names;
    QStringList unmounted;

    names = getDeviceNamesFromSysfs();

    foreach (QString device, names) {
        // show all devices but unmount it before writing
        //if (! checkIsMounted(device))
            unmounted << "/dev/"+device;
    }

    return unmounted;
}

QStringList DeviceEnumerator_linux::getUserFriendlyNames(const QStringList &devices) const
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

bool DeviceEnumerator_linux::unmountDevicePartitions(const QString &device) const
{
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
}

bool DeviceEnumerator_linux::checkIsMounted(const QString &device) const
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

bool DeviceEnumerator_linux::checkIfUSB(const QString &device) const
{
#ifndef SHOW_ONLY_USB_DEVICES
    return true;
#endif

    QString path = "/sys/block/" + device;
    QByteArray devPath(256, '\0');
    ssize_t rc = readlink(path.toLocal8Bit().data(), devPath.data(), devPath.size());
    if (rc && devPath.contains("usb"))
        return true;

    return false;
}

QStringList DeviceEnumerator_linux::getDeviceNamesFromSysfs() const
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

qint64 DeviceEnumerator_linux::getSizeOfDevice(const QString& device) const
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

QStringList DeviceEnumerator_linux::getPartitionsInfo(const QString& device) const
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

QString DeviceEnumerator_linux::getFirstPartitionLabel(const QString& device) const
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

bool DeviceEnumerator_linux::unmount(const QString& what) const
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

QList<QVariantMap> DeviceEnumerator_linux::listBlockDevices() const
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
