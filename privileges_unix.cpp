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

// ideas from
//   Secure Programming Cookbook for C and C++
//   1.3 Dropping Privileges in setuid Programs

#include "privileges_unix.h"

Privileges::Privileges()
{
    struct passwd *pwd;
    pid_t ppid, pppid;
    uid_t uid;

    orig_uid = -1;  // init variables
    orig_gid = -1;
    orig_ngroups = -1;
    new_uid = -1;
    new_gid = -1;

    origUser = USER_ROOT;

    // save information about the privileges that are
    // being dropped so that they be restored later
    orig_uid = geteuid();   // root
    orig_gid = getegid();
    orig_ngroups = getgroups(NGROUPS_MAX, orig_groups);
    
    ppid = getppid();
    pppid = ppid;
    uid = userUidFromPid(ppid, &pppid);
    // try again with parent's parent
    if (uid == 0 && pppid != ppid)
        uid = userUidFromPid(pppid, &pppid);

    SaveUserEnv(pppid);

    pwd = getpwuid(uid);
    if (pwd) {
        new_uid = pwd->pw_uid;
        new_gid = pwd->pw_gid;
    }
}

void Privileges::SetRoot()
{
    if (origUser == USER_ROOT)
        return;

    origUser = USER_ROOT;

    if (geteuid() != orig_uid) {
        if (seteuid(orig_uid) == -1 || geteuid() != orig_uid)
            abort();
    }

    if (getegid() != orig_gid) {
        if (setegid(orig_gid) == -1 || getegid() != orig_gid)
            abort();
    }

    if (!orig_uid)
        setgroups(orig_ngroups, orig_groups);

#if 0
    uid_t ruid = getuid ();
    uid_t rgid = getgid ();
    uid_t euid = geteuid ();
    uid_t egid = getegid ();
    qDebug() << "SetRoot real/effective:" << ruid << rgid << euid << egid;
#endif
}

void Privileges::SetUser()
{
    gid_t old_uid = geteuid();   // root
    gid_t old_gid = getegid();
    int rv;
    Q_UNUSED(rv);

    if (origUser == USER_NORMAL)
        return;

    origUser = USER_NORMAL;

    // if root privileges are to be dropped, be sure to pare down the ancillary
    // groups for the process before doing anything else because the setgroups()
    // system call requires root privileges.  Drop ancillary groups regardless of
    // whether privileges are being dropped temporarily or permanently.
    if (!old_uid)
        setgroups(1, &new_gid);

    // we can't set real gid/uid for user
    // because then we can't set back to root
    if (new_gid != old_gid) {
#ifdef linux
        if (setregid(-1, new_gid) == -1)
            abort();
#else
        rv = setegid(new_gid);
#endif
    }

    if (new_uid != old_uid) {
#ifdef linux
        if (setreuid(-1, new_uid) == -1)
            abort();
#else
        rv = seteuid(new_uid);
#endif
    }

    // verify that the changes were successful
    if (new_gid != old_gid && getegid() != new_gid)
        abort();

    if (new_uid != old_uid && geteuid() != new_uid)
        abort();

#if 0
    uid_t ruid = getuid ();
    uid_t rgid = getgid ();
    uid_t euid = geteuid ();
    uid_t egid = getegid ();
    qDebug() << "SetUser real/effective:" << ruid << rgid << euid << egid;
#endif
}

void Privileges::SetRealUser()
{
    gid_t old_uid = geteuid();   // root
    gid_t old_gid = getegid();
    int rv;
    Q_UNUSED(rv);

    if (origUser == USER_NORMAL)
        return;

    origUser = USER_NORMAL;

    // if root privileges are to be dropped, be sure to pare down the ancillary
    // groups for the process before doing anything else because the setgroups()
    // system call requires root privileges.  Drop ancillary groups regardless of
    // whether privileges are being dropped temporarily or permanently.
    if (!old_uid)
        setgroups(1, &new_gid);

    // we can't set real gid/uid for user
    // because then we can't set back to root
    if (new_gid != old_gid) {
#ifdef linux
        if (setregid(new_gid, new_gid) == -1)
            abort();
#else
        rv = setegid(new_gid);
#endif
    }

    if (new_uid != old_uid) {
#ifdef linux
        if (setreuid(new_uid, new_uid) == -1)
            abort();
#else
        rv = seteuid(new_uid);
#endif
    }

    // verify that the changes were successful
    if (new_gid != old_gid && getegid() != new_gid)
        abort();

    if (new_uid != old_uid && geteuid() != new_uid)
        abort();

#if 0
    uid_t ruid = getuid ();
    uid_t rgid = getgid ();
    uid_t euid = geteuid ();
    uid_t egid = getegid ();
    qDebug() << "SetRealUser real/effective:" << ruid << rgid << euid << egid;
#endif
}

uid_t Privileges::userUidFromPid(pid_t pid, pid_t *ppid)
{
#ifdef Q_OS_OSX
    // function for os x from
    //   http://stackoverflow.com/questions/6457682/how-to-programatically-get-uid-from-pid-in-osx-using-c

    struct kinfo_proc *sProcesses = NULL;
    struct kinfo_proc *sNewProcesses;
    int    aiNames[4];
    size_t iNamesLength;
    int    i, iRetCode, iNumProcs;
    size_t iSize;

    iSize = 0;
    aiNames[0] = CTL_KERN;
    aiNames[1] = KERN_PROC;
    aiNames[2] = KERN_PROC_ALL;
    aiNames[3] = 0;
    iNamesLength = 3;

    iRetCode = sysctl(aiNames, iNamesLength, NULL, &iSize, NULL, 0);

    // allocate memory and populate info in the  processes structure
    do {
        iSize += iSize / 10;
        sNewProcesses = (kinfo_proc *)realloc(sProcesses, iSize);
        if (sNewProcesses == 0) {
            if (sProcesses)
                free(sProcesses);

            return -1;  // could not realloc memory, just return
        }

        sProcesses = sNewProcesses;
        iRetCode = sysctl(aiNames, iNamesLength, sProcesses, &iSize, NULL, 0);
    } while (iRetCode == -1 && errno == ENOMEM);

    iNumProcs = iSize / sizeof(struct kinfo_proc);

    for (i = 0; i < iNumProcs; i++) {
        if (sProcesses[i].kp_proc.p_pid == pid) {
            *ppid = sProcesses[i].kp_eproc.e_ppid;
            free(sProcesses);
            return sProcesses[i].kp_eproc.e_ucred.cr_uid;
        }
    }

    // clean up and return
    free(sProcesses);
    return -1;
#elif defined Q_OS_LINUX
    uid_t ruid;  // real uid

    *ppid = -1;
    ruid = -1;

    QString fileName = "/proc/" + QString::number(pid) + "/status";
    QFile file(fileName);
    if (! file.open(QIODevice::ReadOnly | QIODevice::Text))
      return -1;

    if (!file.isReadable()) {
        file.close();
        return -1;
    }

    QByteArray contents = file.readAll();
    QTextStream in(&contents);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.indexOf("Uid:") == 0)
            ruid = line.section('\t', 1, 1).toInt();
        else if (line.indexOf("PPid:") == 0)
            *ppid = line.section('\t', 1, 1).toInt();
    }

    file.close();
    return ruid;
#else
    Q_UNUSED(pid);
    Q_UNUSED(ppid);
    return -1;
#endif
}

void Privileges::Whoami()
{
  struct passwd *pw;
  uid_t uid;

  uid = geteuid();
  pw = getpwuid(uid);
  if (pw)
      qDebug() << "Whoami:" << pw->pw_name;
  else
      qDebug() << "Whoami: error";
}

void Privileges::SaveUserEnv(pid_t pid)
{
    Q_UNUSED(pid);

#ifdef Q_OS_LINUX
    QString fileName = "/proc/" + QString::number(pid) + "/environ";
    QFile file(fileName);
    if (! file.open(QIODevice::ReadOnly | QIODevice::Text))
      return;

    if (!file.isReadable()) {
        file.close();
        return;
    }

    // null terminated strings
    QByteArray contents = file.readAll().replace('\0', '\n');
    file.close();

    QTextStream in(&contents);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.indexOf("DBUS_SESSION_BUS_ADDRESS") == 0)
            envDbus = line.section('=', 1, -1);
        else if (line.indexOf("LOGNAME") == 0)
            envLogname = line.section('=', 1, -1);
    }
#endif
}
