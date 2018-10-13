#!/bin/bash

################################################################################
#      This file is ported from makebootable_mac used for the creation
#	   of a bootable USB thumb drive that unRAID can be run from
#      Copyright (C) 2009-2012 Kyle Hiltner (stephan@openelec.tv)
#      Copyright (C) 2016 Lime Technology
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

if [ ! -b "${TARGET}" ]; then
    echo "FAIL: Invalid block device ${TARGET}"
    exit 1
fi

MOUNTED=$(mount | grep ${TARGET})
if [ ! -z "${MOUNTED}" ]; then
    echo "INFO: Unmounting ${TARGET}"
    umount ${TARGET} 1>&3 2>&4
fi

echo "INFO: Installing Syslinux bootloader on ${TARGET}1"
/tmp/UNRAID/syslinux/syslinux_linux -f --install ${TARGET}1 1>&3 2>&4

echo "INFO: Writing MBR on $TARGET"
dd if=/tmp/UNRAID/syslinux/mbr.bin of=${TARGET} 1>&3 2>&4

sync

echo ""
echo "INFO: the Unraid OS USB Flash drive is now bootable and may be ejected."
echo ""

exit 0
