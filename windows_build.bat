@echo off
rem ################################################################################
rem #      This file is part of Unraid USB Creator - https://github.com/limetech/usb-creator
rem #      Copyright (C) 2013-2015 RasPlex project
rem #      Copyright (C) 2016 Team LibreELEC
rem #      Copyright (C) 2018 Lime Technology, Inc
rem #
rem #  Unraid USB Creator is free software: you can redistribute it and/or modify
rem #  it under the terms of the GNU General Public License as published by
rem #  the Free Software Foundation, either version 2 of the License, or
rem #  (at your option) any later version.
rem #
rem #  Unraid USB Creator is distributed in the hope that it will be useful,
rem #  but WITHOUT ANY WARRANTY; without even the implied warranty of
rem #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem #  GNU General Public License for more details.
rem #
rem #  You should have received a copy of the GNU General Public License
rem #  along with Unraid USB Creator.  If not, see <http://www.gnu.org/licenses/>.
rem ################################################################################

set PATH=c:\Qt\5.11.1-static\bin;%PATH%

del release\Unraid.USB.Creator.Win32.exe > nul 2>&1

rem call windows_prepare_lang.bat

qmake.exe
nmake release
