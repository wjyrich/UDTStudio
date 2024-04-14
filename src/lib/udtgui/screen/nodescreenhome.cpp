/**
 ** This file is part of the UDTStudio project.
 ** Copyright 2019-2024 UniSwarm
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

#include "nodescreenhome.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>

#include "canopen/indexWidget/indexlabel.h"
#include "screen/nodescreenswidget.h"

#include "bootloader/bootloader.h"

#include "canopen/bootloaderWidget/bootloaderwidget.h"

NodeScreenHome::NodeScreenHome(QWidget *parent)
    : NodeScreen(parent)
{
    createWidgets();
}

void NodeScreenHome::updateFirmware()
{
    BootloaderWidget bootloaderWidget(node(), this);
    bootloaderWidget.exec();
}

void NodeScreenHome::resetHardware()
{
    node()->bootloader()->resetProgram();
}

void NodeScreenHome::createWidgets()
{
    QLayout *glayout = new QVBoxLayout();
    glayout->setContentsMargins(0, 0, 0, 0);

    QLayout *toolBarLayout = new QVBoxLayout();
    toolBarLayout->setContentsMargins(2, 2, 2, 0);
    QToolBar *toolBar = new QToolBar(tr("Node screen commands"));
    toolBar->setIconSize(QSize(20, 20));

    // read all action
    QAction *actionReadMappings = toolBar->addAction(tr("Read all"));
    actionReadMappings->setIcon(QIcon(QStringLiteral(":/icons/img/icons8-update.png")));
    actionReadMappings->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    actionReadMappings->setStatusTip(tr("Read all the objects of the current window"));
    connect(actionReadMappings, &QAction::triggered, this, &NodeScreenHome::readAll);

    toolBarLayout->addWidget(toolBar);
    glayout->addItem(toolBarLayout);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    QWidget *widget = new QWidget(this);
    QLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(createSumaryWidget());
    _statusWidget = createStatusWidget();
    layout->addWidget(_statusWidget);
    layout->addWidget(createOdWidget());
    widget->setLayout(layout);
    scrollArea->setWidget(widget);

    glayout->addWidget(scrollArea);
    setLayout(glayout);
}

QWidget *NodeScreenHome::createSumaryWidget()
{
    QGroupBox *groupBox = new QGroupBox(tr("Summary"));
    QHBoxLayout *hlayout = new QHBoxLayout();

    _summaryIconLabel = new QLabel();
    _summaryIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    _summaryIconLabel->setAlignment(Qt::AlignTop | Qt::AlignRight);
    hlayout->addWidget(_summaryIconLabel);

    QFormLayout *sumaryLayout = new QFormLayout();

    IndexLabel *indexLabel;
    indexLabel = new IndexLabel(NodeObjectId(0x1008, 0));
    sumaryLayout->addRow(tr("Device name:"), indexLabel);
    addIndexWidget(indexLabel);

    _summaryProfileLabel = new QLabel();
    sumaryLayout->addRow(tr("Profile:"), _summaryProfileLabel);

    indexLabel = new IndexLabel(NodeObjectId(0x1009, 0));
    sumaryLayout->addRow(tr("Hardware version:"), indexLabel);
    addIndexWidget(indexLabel);

    indexLabel = new IndexLabel(NodeObjectId(0x2001, 0));
    sumaryLayout->addRow(tr("Manufacture date:"), indexLabel);
    addIndexWidget(indexLabel);

    indexLabel = new IndexLabel(NodeObjectId(0x1018, 4));
    sumaryLayout->addRow(tr("Serial number:"), indexLabel);
    addIndexWidget(indexLabel);

    indexLabel = new IndexLabel(NodeObjectId(0x100A, 0));
    sumaryLayout->addRow(tr("Software version:"), indexLabel);
    addIndexWidget(indexLabel);

    indexLabel = new IndexLabel(NodeObjectId(0x2003, 0));
    sumaryLayout->addRow(tr("Software build:"), indexLabel);
    addIndexWidget(indexLabel);

    hlayout->addItem(sumaryLayout);

    QVBoxLayout *buttonlayout = new QVBoxLayout();
    buttonlayout->setSpacing(3);
    QPushButton *updateFirmwareButton = new QPushButton(tr("&Update firmware"));
    updateFirmwareButton->setFixedWidth(200);
    connect(updateFirmwareButton,
            &QPushButton::released,
            this,
            [=]()
            {
                updateFirmware();
            });
    buttonlayout->addWidget(updateFirmwareButton);

    QPushButton *resetHardwareButton = new QPushButton(tr("&Reset hardware"));
    resetHardwareButton->setFixedWidth(200);
    connect(resetHardwareButton,
            &QPushButton::released,
            this,
            [=]()
            {
                resetHardware();
            });
    buttonlayout->addWidget(resetHardwareButton);

    buttonlayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    hlayout->addItem(buttonlayout);

    groupBox->setLayout(hlayout);
    return groupBox;
}

QWidget *NodeScreenHome::createStatusWidget()
{
    IndexLabel *indexLabel;
    QGroupBox *groupBox = new QGroupBox(tr("Status"));
    QFormLayout *sumaryLayout = new QFormLayout();

    indexLabel = new IndexLabel(NodeObjectId(0x2000, 1));
    indexLabel->setDisplayHint(AbstractIndexWidget::DisplayFloat);
    indexLabel->setScale(1.0 / 100.0);
    indexLabel->setUnit(QStringLiteral(" V"));
    sumaryLayout->addRow(tr("Board voltage:"), indexLabel);
    addIndexWidget(indexLabel);

    indexLabel = new IndexLabel(NodeObjectId(0x2020, 1));
    indexLabel->setDisplayHint(AbstractIndexWidget::DisplayFloat);
    indexLabel->setScale(1.0 / 10.0);
    indexLabel->setUnit(QStringLiteral(" °C"));
    sumaryLayout->addRow(tr("CPU temperature:"), indexLabel);
    addIndexWidget(indexLabel);

    groupBox->setLayout(sumaryLayout);
    return groupBox;
}

QWidget *NodeScreenHome::createOdWidget()
{
    QGroupBox *groupBox = new QGroupBox(tr("Object dictionary"));
    QHBoxLayout *hlayout = new QHBoxLayout();

    QFormLayout *odLayout = new QFormLayout();
    _odEdsFileLabel = new QLabel();
    _odEdsFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    _odEdsFileLabel->setCursor(Qt::IBeamCursor);
    odLayout->addRow(tr("Eds file:"), _odEdsFileLabel);

    _odFileInfosLabel = new QLabel();
    _odFileInfosLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    _odFileInfosLabel->setCursor(Qt::IBeamCursor);
    odLayout->addRow(tr("Eds file infos:"), _odFileInfosLabel);

    _odCountLabel = new QLabel();
    odLayout->addRow(tr("Index count:"), _odCountLabel);

    _odSubIndexCountLabel = new QLabel();
    odLayout->addRow(tr("Sub index count:"), _odSubIndexCountLabel);

    hlayout->addItem(odLayout);

    QVBoxLayout *buttonlayout = new QVBoxLayout();
    QPushButton *goODButton = new QPushButton(tr("Go to OD tab"));
    goODButton->setFixedWidth(200);
    connect(goODButton,
            &QPushButton::released,
            this,
            [=]()
            {
                screenWidget()->setActiveTab(QStringLiteral("od"));
            });
    buttonlayout->addWidget(goODButton);
    buttonlayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    hlayout->addItem(buttonlayout);

    groupBox->setLayout(hlayout);
    return groupBox;
}

void NodeScreenHome::updateInfos()
{
    if (node() == nullptr)
    {
        _summaryProfileLabel->clear();
        _summaryIconLabel->clear();

        _odEdsFileLabel->clear();
        _odFileInfosLabel->clear();
        _odCountLabel->clear();
        _odSubIndexCountLabel->clear();

        _statusWidget->setEnabled(false);
        return;
    }

    _summaryProfileLabel->setText(QStringLiteral("DS%1").arg(node()->profileNumber()));
    if (node()->vendorId() == 0x04A2)  // UniSwarm
    {
        _statusWidget->setEnabled(true);
        switch (node()->profileNumber())
        {
            case 401:
            case 428:
                _summaryIconLabel->setPixmap(QPixmap(QStringLiteral(":/uBoards/uio.png")));
                break;

            case 402:
                _summaryIconLabel->setPixmap(QPixmap(QStringLiteral(":/uBoards/umc.png")));
                break;
        }
    }
    else
    {
        _statusWidget->setEnabled(false);
    }

    if (!node()->edsFileName().isEmpty())
    {
        _odEdsFileLabel->setText(node()->edsFileName());
    }
    else
    {
        _odEdsFileLabel->setText(tr("<span style='color:red'>No eds file found</span>"));
    }

    QString fileInfos;
    fileInfos.append(QStringLiteral("<table><tbody>"));
    QMapIterator<QString, QString> i(node()->nodeOd()->edsFileInfos());
    while (i.hasNext())
    {
        i.next();
        fileInfos.append("<tr><td>" + i.key() + ": </td><td>" + i.value() + "</td></tr>");
    }
    fileInfos.append(QStringLiteral("</tbody></table>"));
    _odFileInfosLabel->setText(fileInfos);

    _odCountLabel->setNum(node()->nodeOd()->indexCount());
    _odSubIndexCountLabel->setNum(node()->nodeOd()->subIndexCount());
}

QString NodeScreenHome::title() const
{
    return tr("Node");
}

void NodeScreenHome::setNodeInternal(Node *node, uint8_t axis)
{
    Q_UNUSED(axis)

    updateInfos();

    if (node != nullptr)
    {
        connect(node, &Node::edsFileChanged, this, &NodeScreenHome::updateInfos);
    }
}
