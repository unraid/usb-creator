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

#include "deviceenumerator_macos.h"

#include <QDebug>
#include <QTextStream>
#include <QDir>
#include <QProcess>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOBSD.h>
#include <IOKit/usb/IOUSBLib.h>
#include <sys/param.h>

// show only USB devices
#define SHOW_ONLY_USB_DEVICES

QStringList DeviceEnumerator_macos::getRemovableDeviceNames() const
{
    QStringList names;
    QStringList unmounted;
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
}

QStringList DeviceEnumerator_macos::getUserFriendlyNames(const QStringList &devices) const
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

bool DeviceEnumerator_macos::checkIsMounted(const QString &device) const
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

bool DeviceEnumerator_macos::checkIfUSB(const QString &device) const
{
#ifndef SHOW_ONLY_USB_DEVICES
    return true;
#endif

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
}

QStringList DeviceEnumerator_macos::getDeviceNamesFromSysfs() const
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

qint64 DeviceEnumerator_macos::getSizeOfDevice(const QString& device) const
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

QString DeviceEnumerator_macos::getFirstPartitionLabel(const QString& device) const
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

QList<QVariantMap> DeviceEnumerator_macos::listBlockDevices() const
{
    QList<QVariantMap> ValidList;

    CFMutableDictionaryRef matchingDict;
    io_iterator_t iter;
    kern_return_t kr;
    io_service_t        usbDevice;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32              score;
    HRESULT             res;

    /* set up a matching dictionary for the class */
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);

    if (matchingDict == NULL)
    {
        return ValidList; // fail
    }

    /* Now we have a dictionary, get an iterator.*/
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);
    if (kr != KERN_SUCCESS)
    {
        return ValidList;
    }

    /* iterate */
    while ((usbDevice = IOIteratorNext(iter)))
    {
        /* do something with device, eg. check properties */
        /* ... */
        IOUSBDeviceInterface300 **deviceInterface = NULL;
        io_name_t            deviceName;

        UInt32               locationID;
        UInt16               vendorId;
        UInt16               productId;
        UInt16               addr;
        qint64               size;

        CFStringRef          deviceNameAsCFString;
        CFStringRef          manufacturerAsCFString;
        CFStringRef          serialNumberAsCFString;
        CFStringRef          bsdNameAsCFString;
        QString              deviceNameString;
        QString              manufacturerString;
        QString              serialNumberString;
        QString              vendorIdString;
        QString              productIdString;
        QString              bsdNameString;
        QString              SerialPadded;
        QString              GUID;

        QVariantMap          projectData;


        // Get the USB device's name.
        kr = IORegistryEntryGetName(usbDevice, deviceName);
        if (KERN_SUCCESS != kr) {
            deviceName[0] = '\0';
        }

        deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, deviceName, kCFStringEncodingASCII);
        deviceNameString = QString::fromCFString(deviceNameAsCFString);
        if (deviceNameAsCFString) CFRelease(deviceNameAsCFString);

        manufacturerAsCFString = (CFStringRef)IORegistryEntrySearchCFProperty(usbDevice, kIOServicePlane, CFSTR(kUSBVendorString), kCFAllocatorDefault, kIORegistryIterateRecursively);
        manufacturerString = QString::fromCFString(manufacturerAsCFString);
        if (manufacturerAsCFString) CFRelease(manufacturerAsCFString);

        serialNumberAsCFString = (CFStringRef)IORegistryEntrySearchCFProperty(usbDevice, kIOServicePlane, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, kIORegistryIterateRecursively);
        serialNumberString = QString::fromCFString(serialNumberAsCFString);
        if (serialNumberAsCFString) CFRelease(serialNumberAsCFString);

        if (serialNumberString.isEmpty()) {
            // Skip
            goto releaseObj;
        }

        // Now, get the locationID of this device. In order to do this, we need to create an IOUSBDeviceInterface
        // for our device. This will create the necessary connections between our userland application and the
        // kernel object for the USB Device.
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

        if((kIOReturnSuccess != kr) || !plugInInterface) {
            fprintf(stderr, "IOCreatePlugInInterfaceForService returned 0x%08x for device name %s.\n", kr, deviceName);
            goto releaseObj;
        }

        // Use the plugin interface to retrieve the device interface.
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID300), (LPVOID*) &deviceInterface);

        // Now done with the plugin interface.
        (*plugInInterface)->Release(plugInInterface);

        if(res || deviceInterface == NULL) {
            fprintf(stderr, "QueryInterface returned 0x%08x.\n", (int) res);
            goto releaseObj;
        }

        // Now that we have the IOUSBDeviceInterface, we can call the routines in IOUSBLib.h.
        // In this case, fetch the locationID. The locationID uniquely identifies the device
        // and will remain the same, even across reboots, so long as the bus topology doesn't change.

        kr = (*deviceInterface)->GetLocationID(deviceInterface, &locationID);
        if(KERN_SUCCESS != kr) {
            fprintf(stderr, "GetLocationID returned 0x%08x.\n", kr);
            goto releaseObj;
        }
        kr = (*deviceInterface)->GetDeviceAddress(deviceInterface, &addr);
        if(KERN_SUCCESS != kr) {
            fprintf(stderr, "GetDeviceAddress returned 0x%08x.\n", kr);
            goto releaseObj;
        }

        kr = (*deviceInterface)->GetDeviceVendor(deviceInterface, &vendorId);
        if(KERN_SUCCESS != kr) {
            fprintf(stderr, "GetDeviceVendor returned 0x%08x.\n", kr);
            goto releaseObj;
        }

        kr = (*deviceInterface)->GetDeviceProduct(deviceInterface, &productId);
        if(KERN_SUCCESS != kr) {
            fprintf(stderr, "GetDeviceProduct returned 0x%08x.\n", kr);
            goto releaseObj;
        }


        bsdNameAsCFString = (CFStringRef)IORegistryEntrySearchCFProperty(usbDevice, kIOServicePlane, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, kIORegistryIterateRecursively);
        bsdNameString = "/dev/" + QString::fromCFString(bsdNameAsCFString);
        if (bsdNameAsCFString) CFRelease(bsdNameAsCFString);

        vendorIdString = QString::number(vendorId, 16).rightJustified(4, '0').right(4);
        productIdString = QString::number(productId, 16).rightJustified(4, '0').right(4);
        SerialPadded = QString(serialNumberString).rightJustified(16, '0').right(16);
        GUID = (vendorIdString + "-" + productIdString + "-" + SerialPadded.left(4) + "-" + SerialPadded.mid(4)).toUpper();

        size = getSizeOfDevice(bsdNameString);
        if (size == 0) {
            // Skip
            goto releaseObj;
        }

        projectData.insert("pid", productIdString);
        projectData.insert("vid", vendorIdString);
        projectData.insert("serial", serialNumberString);
        projectData.insert("guid", GUID);
        projectData.insert("name", deviceNameString);
        projectData.insert("size", size);
        projectData.insert("dev", bsdNameString);

        ValidList.append(projectData);

releaseObj:
        /* And free the reference taken before continuing to the next item */
        IOObjectRelease(usbDevice);
    }

    /* Done, release the iterator */
    IOObjectRelease(iter);

    return ValidList;
}
