#!/bin/bash

################################################################################
#      This file is part of makebootable_mac used for the creation
#	   of a bootable USB thumb drive that unRAID can be run from
#      Copyright (C) 2009-2012 Kyle Hiltner (stephan@openelec.tv)
#      Copyright (C) 2013 Lime Technology
#
#  This Program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  This Program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
################################################################################

VERBOSE="TRUE" # set to TRUE to put this script in debug mode
TARGET=$1

if [ "${VERBOSE}" = "TRUE" ]; then
    exec 3>&1
    exec 4>&2
else
    exec 3> /dev/null
    exec 4> /dev/null
fi

unmount_target(){
    MOUNTED=$(mount | grep ${TARGET})
    if [ ! -z "${MOUNTED}" ]; then
       	echo "INFO: Unmounting ${TARGET}"
	sleep 2
    diskutil unmountDisk force ${TARGET} 1>&3 2>&4
    fi
}

mount_target(){
    MOUNTED=$(mount | grep ${TARGET})
    if [ -z "${MOUNTED}" ]; then
        echo "INFO: Mounting ${TARGET}"
	sleep 2
	diskutil mountDisk ${TARGET} 1>&3 2>&4
    fi
}

unmount_target
echo "INFO: Writing MBR on $TARGET"
dd if=/tmp/UNRAID/syslinux/mbr.bin of=${TARGET} 1>&3 2>&4
mount_target
/tmp/UNRAID/syslinux/syslinux -f --install ${TARGET}s1 1>&3 2>&4

echo ""	
echo "INFO: the Unraid OS USB Flash drive is now bootable and may be ejected."
echo ""

exit 0
