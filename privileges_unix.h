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

#ifndef PRIVILEGES_H
#define PRIVILEGES_H

#include <pwd.h>
#include <errno.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <grp.h>

#include <QFile>
#include <QDebug>

class Privileges
{
public:
    Privileges();
    void SetRoot();
    void SetUser();
    void SetRealUser();
    void Whoami();
    void SaveUserEnv(pid_t);
    QString GetUserEnvDbusSession() {
        return envDbus;
    }
    QString GetUserEnvLogname() {
        return envLogname;
    }

private:
    uid_t userUidFromPid(pid_t pid, pid_t *ppid);

    QString envDbus;
    QString envLogname;
    uid_t orig_uid;
    gid_t orig_gid;
    int   orig_ngroups;
    gid_t orig_groups[NGROUPS_MAX];
    uid_t new_uid;
    gid_t new_gid;

    enum {
        USER_ROOT,
        USER_NORMAL
    } origUser;  // which user selected
};

#endif // PRIVILEGES_H
