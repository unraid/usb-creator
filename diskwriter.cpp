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

#include "diskwriter.h"

#ifdef WINDOWS_DUMMY_WRITE
  #include "windows.h"
#endif

#include "libs/quazip/JlCompress.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <QMapIterator>
#include <QDir>
#include <QProcess>

void DiskWriter::cancelWrite()
{
    isCancelled = true;
}

void DiskWriter::writeImageToRemovableDevice(const QString &filename, const QString &device, quint32 partitionlength, quint8 clustersize)
{
    if (!open(device)) {
        emit error("Couldn't open " + device);
        return;
    }

    isCancelled = false;

    if (filename.endsWith(".gz"))
        writeGzCompressedImage(filename, device, partitionlength, clustersize);
    else if (filename.endsWith(".zip"))
        writeZipCompressedImage(filename, device, partitionlength, clustersize);
    else
        writeUncompressedImage(filename, device, partitionlength, clustersize);

    if (isCancelled)
        emit bytesWritten(0);
    else
        emit finished();
}

void DiskWriter::extractFiles(const QString& sourcezip, const QString& targetpath)
{
    QStringList lst = JlCompress::getFileList(sourcezip);

    QDir directory(targetpath);
    for (int i=0; i<lst.count(); i++)
    {
        QString absFilePath = directory.absoluteFilePath(lst.at(i));
        if (JlCompress::extractFile(sourcezip, lst.at(i), absFilePath).isEmpty())
        {
            qDebug() << "Failed to extract file:" << lst.at(i);
            //TODO - emit error signal instead
            break;
        }

        emit filesExtracted(i+1);
    }

    emit finished_extract(targetpath);
}

void DiskWriter::writeSyslinux(const QString& device)
{
    QString tempDir = QDir::tempPath();

#if defined(Q_OS_WIN)
    QString syslinuxbin = "syslinux.exe";
#elif defined(Q_OS_LINUX)
    QString syslinuxbin = "syslinux_linux";
#elif defined(Q_OS_MACOS)
    QString syslinuxbin = "syslinux";
#endif

    qDebug() << "copying" << ":/bin/syslinux/"+syslinuxbin+" to" << tempDir+"/"+syslinuxbin;
    QFile::copy(":/bin/syslinux/"+syslinuxbin, tempDir+"/"+syslinuxbin);

#if defined(Q_OS_UNIX)
    qDebug() << "setting execute permissions on" << tempDir+"/"+syslinuxbin;
    QFile::setPermissions(tempDir+"/"+syslinuxbin, QFileDevice::ReadOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup);
#endif

    qDebug() << "running" << tempDir+"/"+syslinuxbin+" --install --force "+device;
    QProcess syslinux;
    syslinux.start(tempDir+"/"+syslinuxbin+" --install --force "+device, QIODevice::ReadOnly);
    syslinux.waitForStarted();
    syslinux.waitForFinished();

    for (QString s = syslinux.readLine(); !syslinux.atEnd(); s = syslinux.readLine())
    {
         qDebug() << "syslinux:" << s;
    }

    QFile fileToRemove(tempDir+"/"+syslinuxbin);
    if (fileToRemove.exists()) {
        qDebug() << "Removing" << tempDir+"/"+syslinuxbin;
        fileToRemove.remove();
    }

    emit finished_syslinux();
}

void DiskWriter::writeGzCompressedImage(const QString &filename, const QString& device, quint32 partitionlength, quint8 clustersize)
{
    int read;
    QByteArray buf(512*10024*sizeof(char), 0);
    off_t bytesRead = 0;

    quint64 emptyspacesectors, extraclusters;
    quint16 totalfatsectors, extrafatsectors;
    off_t endfat1, endfat2;

    qDebug() << "partitionlength: " << QString::number(partitionlength);
    qDebug() << "clustersize: " << QString::number(clustersize);

    // 1) Calculate the space difference (in sectors) between pre-made image and this device's partition length
    // 2) Calculate how many extra clusters we can grow the filesystem by (cluster size + 4 bytes FAT1 + 4 bytes FAT2)
    // 3) Calculate extra padding between each FAT we need to splice in
    // 4) Calculate the total number of fat sectors (sum in the number of existing fat sectors included in the base image)
    // 5) Predefine end of FATs to know where to splice in extra padding
    switch (clustersize) {
        case 4:
            emptyspacesectors = partitionlength - 1046528;
            extraclusters = (emptyspacesectors * 512) / ((clustersize * 1024) + 4 + 4);
            extrafatsectors = (extraclusters * 4) / 512;
            totalfatsectors = 1024 + extrafatsectors;
            endfat1 = 1589248;
            endfat2 = 2113536;
            break;
        case 8:
            emptyspacesectors = partitionlength - 16775168;
            extraclusters = (emptyspacesectors * 512) / ((clustersize * 1024) + 4 + 4);
            extrafatsectors = (extraclusters * 4) / 512;
            totalfatsectors = 8192 + extrafatsectors;
            endfat1 = 5259264;
            endfat2 = 9453568;
            break;
        case 16:
            emptyspacesectors = partitionlength - 33552384;
            extraclusters = (emptyspacesectors * 512) / ((clustersize * 1024) + 4 + 4);
            extrafatsectors = (extraclusters * 4) / 512;
            totalfatsectors = 8192 + extrafatsectors;
            endfat1 = 5259264;
            endfat2 = 9453568;
            break;
        case 32:
            emptyspacesectors = partitionlength - 67106816;
            extraclusters = (emptyspacesectors * 512) / ((clustersize * 1024) + 4 + 4);
            extrafatsectors = (extraclusters * 4) / 512;
            totalfatsectors = 8192 + extrafatsectors;
            endfat1 = 5275648;
            endfat2 = 9469952;
            break;

    }

    qDebug() << "emptyspacesectors: " << QString::number(emptyspacesectors);
    qDebug() << "extraclusters: " << QString::number(extraclusters);
    qDebug() << "extrafatsectors: " << QString::number(extrafatsectors);
    qDebug() << "totalfatsectors: " << QString::number(totalfatsectors);


    QByteArray tmp;
    QDataStream dataStream(&tmp, QIODevice::WriteOnly);
    dataStream.setByteOrder(QDataStream::LittleEndian);
    dataStream << partitionlength;
    dataStream << totalfatsectors;


    QMap<int, char> map;

    // Partition 1 CHS endding offset
    //map.insert(451, 170);
    //map.insert(452, 2);
    //map.insert(453, 40);

    // Partition 1 LBA length (partition end offset minus begin offset)
    map.insert(458, tmp[0]);
    map.insert(459, tmp[1]);
    map.insert(460, tmp[2]);
    map.insert(461, tmp[3]);


    // FAT's Total logical sectors (primary)
    map.insert(1048608, tmp[0]);
    map.insert(1048609, tmp[1]);
    map.insert(1048610, tmp[2]);
    map.insert(1048611, tmp[3]);

    // FAT's Logical sectors per file allocation table (primary)
    map.insert(1048612, tmp[4]);
    map.insert(1048613, tmp[5]);


    // FAT's Total logical sectors (backup)
    map.insert(1051680, tmp[0]);
    map.insert(1051681, tmp[1]);
    map.insert(1051682, tmp[2]);
    map.insert(1051683, tmp[3]);

    // FAT's Logical sectors per file allocation table (backup)
    map.insert(1051684, tmp[4]);
    map.insert(1051685, tmp[5]);


    // FAT's FS Information Sector
    // Last known number of free data clusters on the volume, or 0xFFFFFFFF if unknown.
    // Should be set to 0xFFFFFFFF during format and updated by the operating system later on.
    map.insert(1049576, -1);
    map.insert(1049577, -1);
    map.insert(1049578, -1);
    map.insert(1049579, -1);


    // Open source
    gzFile src = gzopen(filename.toStdString().c_str(), "rb");

    if (src == NULL) {
        emit error("Couldn't open " + filename);
        this->close();
        return;
    }

    if (gzbuffer(src, buf.size()) != 0) {
        emit error("Failed to set gz buffer size");
        gzclose_r(src);
        this->close();
        return;
    }

    while (isCancelled == false) {
        read = gzread(src, buf.data(), buf.size());
        if (read == 0)
            break;  // all read
        else if (read < 0) {
            emit error("Failed to read from " + filename);
            gzclose_r(src);
            this->close();
            return;
        }

        // write data exactly as read from image and nothing more
        if (read < buf.size())
            buf.truncate(read);

        QMapIterator<int, char> i(map);
        while (i.hasNext()) {
            i.next();
            if (bytesRead <= i.key() && i.key() < bytesRead + read) {
                qDebug() << "offset" << i.key() << "was" << QString::number(buf.at(i.key() - bytesRead), 16).toUpper() << "changed to" << QString::number(i.value(), 16).toUpper();
                buf.replace(i.key() - bytesRead, 1, &i.value(), 1);
            }
        }

        // start of backup FAT, splice in extra padding for primary FAT
        if (bytesRead <= endfat1 && endfat1 < bytesRead + read) {
            qDebug() << "Adding primary FAT padding at offset:" << endfat1;
            buf.insert(endfat1 - bytesRead, extrafatsectors*512, 0);
        }

        // start of data, splice in extra padding for backup FAT
        if (bytesRead <= endfat2 && endfat2 < bytesRead + read) {
            qDebug() << "Adding backup FAT padding at offset:" << endfat2;
            buf.insert(endfat2 - bytesRead, extrafatsectors*512, 0);
        }

        if (this->write(buf) == false) {
            emit error("Failed to write to " + device);
            gzclose(src);
            this->close();
            return;
        }

        // reduce buffer size if it inflated
        if (read < buf.size())
            buf.truncate(read);

        this->sync();
        bytesRead += read;
        emit bytesWritten(bytesRead);
    } // while

    emit syncing();
    gzclose_r(src);
    this->sync();
    this->close();
}

void DiskWriter::writeUncompressedImage(const QString &filename, const QString& device, quint32 partitionlength, quint8 clustersize)
{
    // if input file is not in gzip format then
    // gzread reads directly from the file
    writeGzCompressedImage(filename, device, partitionlength, clustersize);
}

// zip parts from zipcat.c -- inflate a single-file PKZIP archive to stdout
// by Sam Hocevar <sam@zoy.org>
void DiskWriter::writeZipCompressedImage(const QString &filename, const QString& device, quint32 partitionlength, quint8 clustersize)
{
    int read;
    uint8_t buf4[4];
    QByteArray bufOut(512*1024*sizeof(char), 0);
    QByteArray bufZip(512*1024*sizeof(char), 0);
    unsigned int skipSize = 0;
    z_stream stream;
    off_t bytesRead = 0;

    // Open source
    gzFile src = gzopen(filename.toStdString().c_str(), "rb");

    if (src == NULL) {
        emit error("Couldn't open " + filename);
        this->close();
        return;
    }

    // Parse ZIP file (check header signature)
    read = gzread(src, buf4, 4);
    if (memcmp(buf4, "PK\3\4", 4) != 0) {
        emit error("Not ZIP file " + filename);
        gzclose_r(src);
        this->close();
        return ;
    }

    // https://en.wikipedia.org/wiki/Zip_(file_format)
    // go to start of first file
    gzseek(src, 22, SEEK_CUR);

    read = gzread(src, buf4, 2); // Uncompressed size
    if (read <= 0) {
        emit error("Failed to get filename size");
        gzclose_r(src);
        this->close();
        return ;
    }

    skipSize += (uint16_t) buf4[0] | ((uint16_t) buf4[1] << 8);

    read = gzread(src, buf4, 2); // Extra field size
    if (read <= 0) {
        emit error("Failed to get extra field size");
        gzclose_r(src);
        this->close();
        return ;
    }

    skipSize += (uint16_t) buf4[0] | ((uint16_t) buf4[1] << 8);

    gzseek(src, skipSize, SEEK_CUR);

    // Initialize inflate stream
    stream.total_out = 0;
    stream.zalloc = NULL;
    stream.zfree = NULL;
    stream.opaque = NULL;
    stream.next_in = NULL;
    stream.avail_in = 0;

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        emit error("Failed to initialize decompression stream.");
        gzclose_r(src);
        this->close();
        return ;
    }

    while (isCancelled == false) {
        read = zipRead(src, &stream, bufOut, bufZip);
        if (read == 0)
            break;  // all read
        else if (read < 0) {
            emit error("Failed to read from " + filename);
            gzclose_r(src);
            this->close();
            return;
        }

        // write data exactly as read from image and nothing more
        if (read < bufOut.size())
            bufOut.truncate(read);

        if (this->write(bufOut) == false) {
            emit error("Failed to write to " + device + "!");
            gzclose(src);
            this->close();
            return;
        }

        this->sync();
        bytesRead += read;
        emit bytesWritten(bytesRead);
    } // while

    emit syncing();
    gzclose_r(src);
    this->sync();
    this->close();
}

int DiskWriter::zipRead(gzFile src, z_streamp stream, QByteArray &bufOut, QByteArray &bufZip)
{
    unsigned int total = 0;

    if (bufOut.size() == 0 || bufZip.size() == 0)
        return 0;

    stream->next_out = (Bytef *) bufOut.data();
    stream->avail_out = bufOut.size();

    while (stream->avail_out > 0) {
        unsigned int tmp;
        int ret = 0;

        if (stream->avail_in == 0 && !gzeof(src)) {
            int read = gzread(src, bufZip.data(), bufZip.size());
            if (read < 0)
                return -1;

            stream->next_in = (Bytef *) bufZip.data();
            stream->avail_in = read;
        }

        tmp = stream->total_out;
        ret = inflate(stream, Z_SYNC_FLUSH);
        total += stream->total_out - tmp;

        if (ret == Z_STREAM_END)
            return total;

        if (ret != Z_OK)
            return ret;
    }

    return total;
}
