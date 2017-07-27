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

#include "deviceenumerator_windows.h"

#include <windows.h>
#include <setupapi.h>

#include "diskwriter_windows.h"

// Adapted from http://skilinium.com/blog/?p=134
QStringList DeviceEnumerator_windows::getRemovableDeviceNames() const
{
    QStringList names;
#ifdef WINDOWS_DUMMY_WRITE
    names << "dummy_image_device";
    return names;
#endif

    WCHAR *szDriveLetters;
    WCHAR szDriveInformation[1024];

    GetLogicalDriveStrings(1024, szDriveInformation);

    szDriveLetters = szDriveInformation;
    while (*szDriveLetters != '\0') {
        if (GetDriveType(szDriveLetters) == DRIVE_REMOVABLE)
            names << QString::fromWCharArray(szDriveLetters);

        szDriveLetters = &szDriveLetters[wcslen(szDriveLetters) + 1];
    }

    return names;
}

QStringList DeviceEnumerator_windows::getUserFriendlyNames(const QStringList &devices) const
{
    QStringList names;
#ifdef WINDOWS_DUMMY_WRITE
    Q_UNUSED(devices);
    names << "dummy_image_device [ local file ]";
    return names;
#endif

    foreach (const QString &dev, devices) {
        qint64 size = getSizeOfDevice(dev);
        QString label = getLabelOfDevice(dev);
        if (label.isEmpty())
            names << dev + " [" + sizeToHuman(size) + "]";
        else
            names << dev + " [" + label + ", " + sizeToHuman(size) + "]";
    }

    return names;
}

qint64 DeviceEnumerator_windows::getSizeOfDevice(const QString &device) const
{
#ifdef WINDOWS_DUMMY_WRITE
    Q_UNUSED(device);
    return std::numeric_limits<qint64>::max();
#endif

    HANDLE handle = DiskWriter_windows::getHandleOnDevice(device, GENERIC_READ);
    if (handle == INVALID_HANDLE_VALUE)
        return 0;

    DWORD junk;
    qint64 size = 0;
    GET_LENGTH_INFORMATION lenInfo;
    bool ok = DeviceIoControl(handle,
                              IOCTL_DISK_GET_LENGTH_INFO,
                              NULL, 0,
                              &lenInfo, sizeof(lenInfo),
                              &junk,
                              (LPOVERLAPPED) NULL);
    if (ok)
        size = lenInfo.Length.QuadPart;

    CloseHandle(handle);
    return size;
}

QString DeviceEnumerator_windows::getLabelOfDevice(const QString &device)
{
    TCHAR label[MAX_PATH + 1] = { 0 };

    if (GetVolumeInformation((const wchar_t *) device.utf16(), \
          label, ARRAYSIZE(label), \
          NULL, NULL, NULL, NULL, 0))
    {
        return QString::fromUtf16((const ushort *) label);
    }

    return "";
}

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <windows.h>
#include <winioctl.h>

// http://www.codeproject.com/Articles/259577/How-to-flush-a-storage-volumes-the-file-cache-lock
// EjectMediaByLetter.cpp by Uwe Sieber - www.uwe-sieber.de
// Simple demonstration how to flush, lock and dismount a volume and eject a media from a drive
// Works under W2K, XP, W2K3, Vista, Win7, Win8, not tested under Win9x
// you are free to use this code in your projects
int DeviceEnumerator_windows::loadEjectDrive(const QString &device, const loadEject action) const
{
    bool ForceEject = false;  // dismount and ejecting even we got no lock

    if (action == LOADEJECT_LOAD)
        qDebug() << "Loading device" << device;
    else
        qDebug() << "Ejecting device" << device;

    if (device.at(0) < 'A' || device.at(0) > 'Z' || device.at(1) != ':')
        return ERRL_INVALID_PARAM;

    // "\\.\X:"  -> to open the volume
    QString device1 = "\\\\.\\" + device.left(2);
    LPCWSTR LPCWS_device = (const wchar_t *) device1.utf16();

    int res;
    DWORD dwRet;

    // try to flush, write access required which only admins will get
    HANDLE hVolWrite = CreateFile(LPCWS_device, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVolWrite != INVALID_HANDLE_VALUE) {
        qDebug() << "Flushing cache...";
        res = FlushFileBuffers(hVolWrite);
        if (!res)
            qDebug() << "failed err=" << GetLastError();

        CloseHandle(hVolWrite);
    }

    HANDLE hVolRead = CreateFile(LPCWS_device, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVolRead == INVALID_HANDLE_VALUE) {
        qDebug() << "error opening the volume for read access -> abort\n" << GetLastError();
        return ERRL_NO_VOLREAD;
    }

    if (action == LOADEJECT_EJECT) {
        // allowing (unlocking) eject, usually for CD/DVD only, but does not hurt (and returns TRUE) for other drives
        qDebug() << "Allowing eject...";
        PREVENT_MEDIA_REMOVAL pmr = {0}; // pmr.PreventMediaRemoval = FALSE;
        res = DeviceIoControl(hVolRead, IOCTL_STORAGE_MEDIA_REMOVAL, &pmr, sizeof(pmr), NULL, 0, &dwRet, NULL);
        if (!res)
            qDebug() << "failed err=" << GetLastError();
    }

    // try to lock the volume, seems to flush too, maybe even with read access...
    qDebug() << "Locking volume...";
    int Locked = DeviceIoControl(hVolRead, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwRet, NULL);
    if (!Locked)
        qDebug() << "failed err=" << GetLastError();

    if (!Locked && !ForceEject)
        return ERRL_NO_LOCK;

    if (action == LOADEJECT_EJECT) {
        // dismount the file system if either we got a lock or we want to force it
        qDebug() << "Dismounting volume...";
        res = DeviceIoControl(hVolRead, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwRet, NULL);
        if (!res)
            qDebug() << "failed err=" << GetLastError();
    }

    if (action == LOADEJECT_EJECT) {
        qDebug() << "Ejecting media...";
        res = DeviceIoControl(hVolRead, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &dwRet, NULL);
        if (!res)
            qDebug() << "failed err=" << GetLastError();
    } else {
        qDebug() << "Loading media...";
        res = DeviceIoControl(hVolRead, IOCTL_STORAGE_LOAD_MEDIA, NULL, 0, NULL, 0, &dwRet, NULL);
        if (!res)
            qDebug() << "failed err=" << GetLastError();
    }

    if (Locked)
        DeviceIoControl(hVolRead, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwRet, NULL);

    CloseHandle(hVolRead);

    if (res) {
        qDebug() << "success";
        return ERRL_SUCCESS;
    }

    qDebug() << "no eject";
    return ERRL_NO_EJECT;
}

int DeviceEnumerator_windows::removeDrive(const QString &device) const
{
    Q_UNUSED(device);
    qDebug() << "unimplemented: DeviceEnumerator_windows::removeDrive";
    return 0;
}


#define START_ERROR_CHK()           \
    DWORD error = ERROR_SUCCESS;    \
    DWORD failedLine;               \
    QString failedApi;

#define CHK( expr, api )            \
    if ( !( expr ) ) {              \
        error = GetLastError( );    \
        failedLine = __LINE__;      \
        failedApi = ( api );        \
        goto Error_Exit;            \
    }

#define END_ERROR_CHK()             \
    error = ERROR_SUCCESS;          \
    Error_Exit:                     \
    if ( ERROR_SUCCESS != error ) { \
        qDebug() << failedApi << " failed at " << failedLine << " : Error Code - " << error << endl;    \
    }


QList<QVariantMap> DeviceEnumerator_windows::listBlockDevices() const
{
    START_ERROR_CHK();


    const GUID GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10L, 0x6530, 0x11D2,
    {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED} };

    const GUID GUID_DEVINTERFACE_DISK = { 0x53F56307L, 0xB6BF, 0x11D0,
    {0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B} };


    HDEVINFO hInfo;
    const DWORD bufferSize = 131072;
    TCHAR buf[bufferSize];

    SP_DEVINFO_DATA *DeviceInfoData = (SP_DEVINFO_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
    DeviceInfoData->cbSize = sizeof(SP_DEVINFO_DATA);

    SP_INTERFACE_DEVICE_DATA *Interface_Info = (SP_INTERFACE_DEVICE_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_INTERFACE_DEVICE_DATA));
    Interface_Info->cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

    SP_DEVICE_INTERFACE_DETAIL_DATA *pDetData = (SP_DEVICE_INTERFACE_DETAIL_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + 1024);
    pDetData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    DWORD dwDetDataSize = pDetData->cbSize + 1024;


    QList<QVariantMap> stagingList;
    QList<QVariantMap> ValidList;
    QStringList UsedStorageDevices;


    // 1st pass to get the usb device's Serial, PID and VID

    hInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    CHK( INVALID_HANDLE_VALUE != hInfo, "SetupDiGetClassDevs" );

    for (DWORD i=0; SetupDiEnumDeviceInfo(hInfo, i, DeviceInfoData); i++)
    {
        DWORD nSize = 0;

        if (!SetupDiGetDeviceInstanceId(hInfo, DeviceInfoData, buf, sizeof(buf), &nSize)) {
            break;
        }

        QString strDeviceInstanceID = QString::fromWCharArray(buf);


        if (strDeviceInstanceID.startsWith("USB\\") && strDeviceInstanceID.contains(QRegExp("\\\\[0-9A-Z]+$")))
        {
            int PIDpos = strDeviceInstanceID.indexOf("PID_");
            int VIDpos = strDeviceInstanceID.indexOf("VID_");
            int Serialpos = strDeviceInstanceID.lastIndexOf("\\");

            if (PIDpos == -1 || VIDpos == -1 || Serialpos == -1) continue;

            QVariantMap projectData;
            projectData.insert("pid", strDeviceInstanceID.mid(PIDpos+4, 4));
            projectData.insert("vid", strDeviceInstanceID.mid(VIDpos+4, 4));
            projectData.insert("serial", strDeviceInstanceID.mid(Serialpos+1));

            QString SerialPadded = projectData["serial"].toString().rightJustified(16, '0', true);
            QString GUID = (projectData["vid"].toString() + "-" + projectData["pid"].toString() + "-" + SerialPadded.left(4) + "-" + SerialPadded.mid(4)).toUpper();

            projectData.insert("guid", GUID);
            stagingList.append(projectData);
///*
            qDebug() << "Instance ID:" << strDeviceInstanceID << endl \
                     << "PID:" << projectData["pid"].toString() << endl \
                     << "VID:" << projectData["vid"].toString() << endl \
                     << "Serial:" << projectData["serial"].toString() << endl \
                     << "GUID:" << projectData["guid"].toString();
//*/
        }
    }

    if (hInfo) SetupDiDestroyDeviceInfoList(hInfo);





    // 2nd pass to get the disk's friendly name

    hInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    CHK( INVALID_HANDLE_VALUE != hInfo, "SetupDiGetClassDevs" );

    for (DWORD i=0; SetupDiEnumDeviceInfo(hInfo, i, DeviceInfoData); i++)
    {
        DWORD nSize = 0;

        if (!SetupDiGetDeviceInstanceId(hInfo, DeviceInfoData, buf, sizeof(buf), &nSize)) {
            break;
        }

        QString strDeviceInstanceID = QString::fromWCharArray(buf);

        if (strDeviceInstanceID.startsWith("USBSTOR\\"))
        {
            for (QList<QVariantMap>::iterator it = stagingList.begin();
                 it != stagingList.end();
                 it++)
            {
                QString match = QString("\\").append((*it)["serial"].toString());
                if (strDeviceInstanceID.contains(match))
                {
                    if (UsedStorageDevices.filter(match).count() > 0)
                    {
                        // Duplicate Storage device with a different port... smells like a sd card reader
                        qDebug() << "Removing Duplicate Storage device(s):" << strDeviceInstanceID;
                        stagingList.erase(it);
                        break;
                    }

                    UsedStorageDevices.append(strDeviceInstanceID);

                    DWORD DataT;

                    if (SetupDiGetDeviceRegistryProperty(hInfo, DeviceInfoData, SPDRP_FRIENDLYNAME, &DataT, (PBYTE)buf, bufferSize, &nSize))
                    {
                        it->insert("name", QString::fromWCharArray(buf));
///*
                        qDebug() << "Instance ID:" << strDeviceInstanceID << endl \
                                 << "Friendly Name:" << (*it)["name"].toString();
//*/
                    }
                }
            }
        }
    }

    if (hInfo) SetupDiDestroyDeviceInfoList(hInfo);





    // 3rd pass to get the disk's physical disk name
    hInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    CHK( INVALID_HANDLE_VALUE != hInfo, "SetupDiGetClassDevs" );

    for (DWORD j=0; SetupDiEnumDeviceInterfaces(hInfo, NULL, &GUID_DEVINTERFACE_DISK, j, Interface_Info); j++)
    {
        if (SetupDiGetDeviceInterfaceDetail(hInfo,
            Interface_Info, pDetData, dwDetDataSize, NULL,
            DeviceInfoData))
        {
            QString strDevicePath = QString::fromWCharArray(pDetData->DevicePath).toUpper();

            if (strDevicePath.startsWith("\\\\?\\USBSTOR#")) {
                for (QList<QVariantMap>::iterator it = stagingList.begin();
                     it != stagingList.end();
                     it++)
                {
                    if (strDevicePath.contains(QString("#").append((*it)["serial"].toString())))
                    {
                        HANDLE disk = CreateFile(pDetData->DevicePath,
                                       GENERIC_READ,
                                       FILE_SHARE_READ,
                                       NULL,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);

                        CHK( INVALID_HANDLE_VALUE != disk,
                         "CreateFile" );

                        STORAGE_DEVICE_NUMBER diskNumber;
                        DWORD bytesReturned;

                        CHK( DeviceIoControl(disk,
                                          IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                          NULL,
                                          0,
                                          &diskNumber,
                                          sizeof( STORAGE_DEVICE_NUMBER ),
                                          &bytesReturned,
                                          NULL),
                         "IOCTL_STORAGE_GET_DEVICE_NUMBER" );

                        it->insert("dev", QString("\\\\?\\PhysicalDrive").append(QString::number(diskNumber.DeviceNumber)));


                        GET_LENGTH_INFORMATION lenInfo;

                        if (DeviceIoControl(disk,
                                            IOCTL_DISK_GET_LENGTH_INFO,
                                            NULL, 0,
                                            &lenInfo, sizeof(lenInfo),
                                            &bytesReturned,
                                            (LPOVERLAPPED) NULL))
                        {
                            it->insert("size", lenInfo.Length.QuadPart);
                        }
                        else
                        {
                            //it->insert("size", 0);
                            break;  // probably a sd card reader
                        }

/*
                        // Set the input data structure
                        STORAGE_PROPERTY_QUERY storagePropertyQuery;
                        ZeroMemory(&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY));
                        storagePropertyQuery.PropertyId = StorageDeviceProperty;
                        storagePropertyQuery.QueryType = PropertyStandardQuery;

                        // Get the necessary output buffer size
                        STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader = {0};
                        DWORD dwBytesReturned = 0;
                        if(!DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY,
                            &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
                            &storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER),
                            &dwBytesReturned, NULL))
                        {
                            //dwRet = ::GetLastError();
                            //::CloseHandle(disk);
                            //return dwRet;
                        }

                        // Alloc the output buffer
                        const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
                        BYTE* pOutBuffer = new BYTE[dwOutBufferSize];
                        ZeroMemory(pOutBuffer, dwOutBufferSize);

                        // Get the storage device descriptor
                        if(! DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY,
                                &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
                                buf, dwOutBufferSize,
                                &dwBytesReturned, NULL))
                        {
                            //dwRet = ::GetLastError();
                            //delete []pOutBuffer;
                            //::CloseHandle(disk);
                            //return dwRet;
                        }

                        //STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)pOutBuffer;
                        //STORAGE_BUS_TYPE dwBusType = pDeviceDescriptor->BusType;

                        //if (STORAGE_BUS_TYPE::BusTypeUsb == dwBusType)
                        //    qDebug() << "Bus Type: USB";
*/
                        CloseHandle( disk );

                        ValidList.append((*it));

///*
                        qDebug() << "Device path:" << strDevicePath << endl \
                                 << "Physical path:" << (*it)["dev"].toString() << endl \
                                 << "Size:" << QString::number((*it)["size"].toULongLong());
//*/
                        break;
                    }
                }
            }
        } else {
            qDebug() << " failed" << GetLastError();
        }
    }

    END_ERROR_CHK();

    if (pDetData) {
        HeapFree(GetProcessHeap(), 0, pDetData);
    }

    if (Interface_Info) {
        HeapFree(GetProcessHeap(), 0, Interface_Info);
    }

    if (DeviceInfoData) {
        HeapFree(GetProcessHeap(), 0, DeviceInfoData);
    }

    if (hInfo) SetupDiDestroyDeviceInfoList(hInfo);


    return ValidList;
}
