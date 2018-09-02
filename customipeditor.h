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

#ifndef CUSTOMIPEDITOR_H
#define CUSTOMIPEDITOR_H

#include "customlineedit.h"

#include <QFrame>

class CustomIpEditor : public QFrame
{
    Q_OBJECT

public:
    explicit CustomIpEditor(QWidget *parent = 0);
    virtual ~CustomIpEditor() {}
    QString getValue();

private:
    QList <CustomLineEdit*> lines;
};

#endif // CUSTOMIPEDITOR_H
