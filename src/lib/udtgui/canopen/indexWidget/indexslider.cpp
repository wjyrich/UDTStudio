/**
 ** This file is part of the UDTStudio project.
 ** Copyright 2019-2021 UniSwarm
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/

#include "indexslider.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QSlider>
#include <QVBoxLayout>

IndexSlider::IndexSlider(const NodeObjectId &objId)
    : AbstractIndexWidget(objId)
{
    setObjId(objId);

    _internalUpdate = false;

    QVBoxLayout *layout = new QVBoxLayout();
    _slider = new QSlider(Qt::Horizontal);
    connect(_slider, &QSlider::valueChanged, this, &IndexSlider::applyValue);
    layout->addWidget(_slider);
    setLayout(layout);
}

void IndexSlider::applyValue(int value)
{
    if (!_internalUpdate)
    {
        requestWriteValue(value);
    }
}

void IndexSlider::setDisplayValue(const QVariant &value, AbstractIndexWidget::DisplayAttribute flags)
{
    Q_UNUSED(flags)

    _internalUpdate = true;
    _slider->setValue(value.toInt());
    _internalUpdate = false;
}

bool IndexSlider::isEditing() const
{
    return false;
}

void IndexSlider::updateRange()
{
    _slider->setRange(_minValue.toInt(), _maxValue.toInt());
}

void IndexSlider::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();

    menu->exec(event->globalPos());
    delete menu;

    event->accept();
}
