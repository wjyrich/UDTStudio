/**
 ** This file is part of the UDTStudio project.
 ** Copyright 2019-2020 UniSwarm
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

#include <QDebug>
#include <QDataStream>

#include "sdo.h"
#include "canopenbus.h"

#define SDO_SCS(data)       ((data)[0] & 0xE0)  // E0 -> mask for css
#define SDO_E_MASK          0x2
#define SDO_E_NORMAL        0x0              // E: transfer type. See CIA 310 7.2.4.3.3 -> 0: normal transfer
#define SDO_E_EXPEDITED     0x1              // E: transfer type. See CIA 310 7.2.4.3.3 -> 1: expedited transfer

#define SDO_S_MASK          0x1
#define SDO_S               0x1              // S : size indicator. See CIA 310 7.2.4.3.3
#define SDO_C_MASK          0x1
#define SDO_C               0x1              // C: indicates whether there are still more segments to be downloaded.

#define SDO_N_MASK          0x03
#define SDO_N(data)         ((((data)[0]) >> 2) & 0x03) // Used for download/upload initiate. \
    // If valid it indicates the number of bytes in d that do not contain data
#define SDO_N_SEG(data)     ((((data)[0]) >> 1) & 0x07) // Used for download/upload segment. \
    // indicates the number of bytes in seg-data that do not contain segment data.

#define SDO_TOG_BIT(data)   ((data)[0] & 0x10)  // 0x10 -> mask for toggle bit
#define SDO_TOGGLE_MASK     1 << 4

// CCS : Client Command Specifier from Client to Server
#define SDO_CCS_CLIENT_DOWNLOAD_INITIATE        0x20    // ccs:1
#define SDO_CCS_CLIENT_DOWNLOAD_SEGMENT         0x00    // ccs:0
#define SDO_CCS_CLIENT_UPLOAD_INITIATE          0x40    // ccs:2 : initiate upload request
#define SDO_CCS_CLIENT_UPLOAD_SEGMENT           0x60    // ccs:3

// SCS : Server Command Specifier from Server to Client
#define SDO_SCS_SERVER_DOWNLOAD_INITIATE        0x60    // scs:3
#define SDO_SCS_SERVER_DOWNLOAD_SEGMENT         0x20    // scs:1
#define SDO_SCS_SERVER_UPLOAD_INITIATE          0x40    // scs:2
#define SDO_SCS_SERVER_UPLOAD_SEGMENT           0x00    // scs:0

/************************************************************************************/
/*                            BLOCK DOWNLOAD                                        */
// CCS : Client Command Specifier from Client to Server
#define SDO_CCS_CLIENT_BLOCK_DOWNLOAD_INITIATE      0xC0    // ccs:6
#define SDO_CCS_CLIENT_BLOCK_DOWNLOAD_END           0xC0    // ccs:6
#define SDO_CCS_CLIENT_BLOCK_DOWNLOAD_SUB_BLOCK     0xA0    // ccs:5
//cs: client subcommand for DOWNLOAD
#define SDO_CCS_CLIENT_BLOCK_DOWNLOAD_CS_MASK       0x1 // Maks for cs: client subcommand
#define SDO_CCS_CLIENT_BLOCK_DOWNLOAD_CS_INIT_REQ   0x0 // cs:0: initiate download request
#define SDO_CCS_CLIENT_BLOCK_DOWNLOAD_CS_END_REQ    0x1 // cs:1: end block download request

#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_S             0x2 // s: size indicator

// SCS : Server Command Specifier from Server to Client
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_INITIATE      0xA0    // scs:5
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_RESP          0xA0    // scs:5
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_END           0xA0    // scs:5
// ss: server subcommand for DOWNLOAD
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_MASK       0x3 // ss :0: initiate download response
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_INIT_RESP  0x0 // ss :0: initiate download response
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_RESP       0x2 // ss :2: block download response
#define SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_END_RESP   0x1 // ss :1: end block download response

/***********************************************************************************/
/*                            BLOCK UPLOAD                                         */
// CCS : Client Command Specifier from Client to Server
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_INITIATE        0xA0// ccs:5
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_END             0xA0// ccs:5
//cs: client subcommand for UPLOAD
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_CS_MASK         0x3 // Mask for cs: client subcommand
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_CS_INIT_REQ     0x0 // 0: initiate upload request
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_CS_END_REQ      0x1 // 1: end block upload request
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_CS_RESP         0x2 // 2: block upload response
#define SDO_CCS_CLIENT_BLOCK_UPLOAD_CS_START        0x3 // 3: start upload

// SCS : Server Command Specifier from Server to Client
#define SDO_SCS_SERVER_BLOCK_UPLOAD_INITIATE        0xC0    // scs:6
#define SDO_SCS_SERVER_BLOCK_UPLOAD_END             0xC0    // scs:6
// ss: server subcommand
#define SDO_SCS_SERVER_BLOCK_UPLOAD_SS_INIT_RESP    0x0 // 0: initiate upload response
#define SDO_SCS_SERVER_BLOCK_UPLOAD_SS_END_RESP     0x1 // 1: end block upload response
// s: size indicator
#define SDO_SCS_SERVER_BLOCK_UPLOAD_S_MASK          0x1 // s: size indicator
#define SDO_SCS_SERVER_BLOCK_UPLOAD_S               0x1 // s: size indicator

#define SDO_BLOCK_CRC_MASK   0x04  // Mask CRC support
#define SDO_BLOCK_CRC_CC     0x04  // cc: client CRC support
#define SDO_BLOCK_CRC_SC     0x04  // sc: server CRC support

#define SDO_INDEX(data) static_cast<quint16>(((data)[2]) << 8) + static_cast<quint8>((data)[1])
#define SDO_SUBINDEX(data) static_cast<quint8>((data)[3])
#define SDO_SG_SIZE       7         // size max by segment

#define CO_SDO_CS_ABORT                         0x80

SDO::SDO(Node *node)
    : Service (node)
{
    _bus = node->bus();
    _nodeId = node->nodeId();
    _cobIdClientToServer = 0x600;
    _cobIdServerToClient = 0x580;
    _cobIds.append(_cobIdClientToServer + _nodeId);
    _cobIds.append(_cobIdServerToClient + _nodeId);
}

QString SDO::type() const
{
    return QLatin1String("SDO");
}

quint32 SDO::cobIdClientToServer()
{
    return _cobIdClientToServer;
}

quint32 SDO::cobIdServerToClient()
{
    return _cobIdServerToClient;
}

void SDO::parseFrame(const QCanBusFrame &frame)
{
    quint8 scs = static_cast<quint8>SDO_SCS(frame.payload());
    qDebug() << QString::number(frame.frameId(), 16) << QString::number(scs, 16);

    switch (scs)
    {
    case SDO_SCS_SERVER_UPLOAD_INITIATE:
        sdoUploadInitiate(frame);
        break;

    case SDO_SCS_SERVER_UPLOAD_SEGMENT:
        sdoUploadSegment(frame);
        break;

    case SDO_SCS_SERVER_BLOCK_UPLOAD_INITIATE:
        sdoBlockUpload(frame);
        break;

    case SDO_SCS_SERVER_DOWNLOAD_INITIATE:
        sdoDownloadInitiate(frame);
        break;

    case SDO_SCS_SERVER_DOWNLOAD_SEGMENT:
        sdoDownloadSegment(frame);
        break;

    case SDO_SCS_SERVER_BLOCK_DOWNLOAD_INITIATE:
        sdoBlockDownload(frame);
        break;

    case CO_SDO_CS_ABORT:
        qDebug() << "ABORT received";
        _co_sdo.state = CO_SDO_STATE_FREE;
        break;

    default:
        break;
    }
}

qint32 SDO::uploadData(NodeIndex &index, quint8 subindex)
{
    bool error;
    quint8 cmd = 0;
    _co_sdo.index = &index;
    _co_sdo.subIndex = subindex;

    if (_co_sdo.index->subIndex(subindex)->dataType() != SubIndex::DDOMAIN)
    {
        cmd = SDO_CCS_CLIENT_UPLOAD_INITIATE;
        error = sendSdoRequest(cmd, _co_sdo.index->index(), _co_sdo.subIndex);
        if (error)
        {
            // TODO ABORT management error with framesWritten() and errorOccurred()
        }
    }
    else
    {
         cmd = SDO_CCS_CLIENT_BLOCK_UPLOAD_INITIATE;
         // TODO
    }

    return 0;
}

qint32 SDO::sdoUploadInitiate(const QCanBusFrame &frame)
{
    quint8 transferType = (static_cast<quint8>(frame.payload()[0] & SDO_E_MASK)) >> 1;
    quint8 sizeIndicator = static_cast<quint8>(frame.payload()[0] & SDO_S_MASK);
    quint8 cmd = 0;
    bool error;

    _co_sdo.data.clear();
    _co_sdo.index->subIndex(_co_sdo.subIndex);
    if (transferType == SDO_E_EXPEDITED)
    {
        if (sizeIndicator == 0)  // data set size is not indicated
        {
        }
        else if (sizeIndicator == 1)  // data set size is indicated
        {
            _co_sdo.stay = (4 - SDO_N(frame.payload()));
            _co_sdo.index->subIndex(_co_sdo.subIndex)->setValue( frame.payload().mid(4, static_cast<quint8>(_co_sdo.stay)));

        }
        else
        {
        }

        _co_sdo.state = CO_SDO_STATE_FREE;
    }
    else if (transferType == SDO_E_NORMAL)
    {
        if (sizeIndicator == 0)  // data set size is not indicated
        {
            // NOT USED
        }
        else if (sizeIndicator == 1)  // data set size is indicated
        {

        }
        else
        {

        }
        _co_sdo.stay = static_cast<quint32>(frame.payload()[4]);
        cmd = SDO_CCS_CLIENT_UPLOAD_SEGMENT;
        _co_sdo.toggle = 0;
        cmd |= (_co_sdo.toggle << 4) & SDO_TOGGLE_MASK;

        error = sendSdoRequest(cmd);
        if (error)
        {
            // TODO ABORT management error with framesWritten() and errorOccurred()
        }
        _co_sdo.stay = CO_SDO_STATE_UPLOAD_SEGMENT;
    }
    else
    {
        // ERROR
        _co_sdo.state = CO_SDO_STATE_FREE;
    }

    return 0;
}
qint32 SDO::sdoUploadSegment(const QCanBusFrame &frame)
{
    bool error;
    quint8 cmd = 0;
    quint8 toggle = SDO_TOG_BIT(frame.payload());
    quint8 size = 0;

    if (toggle != (_co_sdo.toggle & SDO_TOGGLE_MASK))
    {
        // ABORT        
        _co_sdo.state = CO_SDO_STATE_FREE;
        return 1;
    }
    else
    {
        size = (7 - SDO_N(frame.payload()));
        _co_sdo.data.append(frame.payload().mid(4, size));
         _co_sdo.stay -= size;

        if ((frame.payload()[0] & SDO_C_MASK) == SDO_C)  // no more segments to be uploaded
        {
            _co_sdo.index->subIndex(_co_sdo.subIndex)->setValue(_co_sdo.data);
            _co_sdo.stay = CO_SDO_STATE_FREE;
            emit dataObjetAvailable();
        }
        else // more segments to be uploaded (C=0)
        {

            cmd = SDO_CCS_CLIENT_UPLOAD_SEGMENT;
            _co_sdo.toggle = ~_co_sdo.toggle;
            cmd |= (_co_sdo.toggle << 4) & SDO_TOGGLE_MASK;
            qDebug() << "Send upload segment request : ccs :" << cmd;
            error = sendSdoRequest(cmd);
            if (error)
            {
                // TODO ABORT management error with framesWritten() and errorOccurred()
            }
            _co_sdo.stay = CO_SDO_STATE_UPLOAD_SEGMENT;
        }
    }
    return 0;
}

qint32 SDO::sdoBlockUpload(const QCanBusFrame &frame)
{
    Q_UNUSED(frame);
    return 0;
}

qint32 SDO::downloadData(NodeIndex &index, quint8 subindex)
{
    bool error = true;
    quint8 cmd = 0;

    _co_sdo.index = &index;
    _co_sdo.subIndex = subindex;

    if (_co_sdo.index->subIndex(subindex)->dataType() == SubIndex::DDOMAIN)
    {
        _co_sdo.stay = static_cast<quint32>(_co_sdo.index->subIndex(subindex)->value().toByteArray().size());
        // block download
        cmd = SDO_CCS_CLIENT_BLOCK_DOWNLOAD_INITIATE;
        // No support crc
        if (_co_sdo.stay < 0xFFFF)
        {
            cmd |= SDO_SCS_SERVER_BLOCK_DOWNLOAD_S;

            error = sendSdoRequest(cmd, _co_sdo.index->index(), _co_sdo.subIndex, _co_sdo.stay);
            if (error)
            {
                // TODO ABORT management error with framesWritten() and errorOccurred()
            }
        }
        else
        {  // Overload size so no indicate the size in frame S=0
            error = sendSdoRequest(cmd, _co_sdo.index->index(), _co_sdo.subIndex, 0);
            if (error)
            {
                // TODO ABORT management error with framesWritten() and errorOccurred()
            }
        }
        _co_sdo.seqno = 1;
        _co_sdo.state = CO_SDO_STATE_FREE;
    }
    else
    {
         // expedited transfer
        if (_co_sdo.index->subIndex(subindex)->length() <= 4)
        {
            cmd = SDO_CCS_CLIENT_DOWNLOAD_INITIATE;
            cmd |= SDO_E_EXPEDITED << 1;
            cmd |= SDO_S;
            cmd |= ((4 - _co_sdo.index->subIndex(subindex)->length()) & SDO_N_MASK) << 2;
            sendSdoRequest(cmd, _co_sdo.index->index(), _co_sdo.subIndex, _co_sdo.index->subIndex(subindex)->value());
            _co_sdo.state = CO_SDO_STATE_FREE;
        }
        else
        {   // normal transfer
            cmd = SDO_CCS_CLIENT_DOWNLOAD_INITIATE;
            cmd |= SDO_S;
            sendSdoRequest(cmd, _co_sdo.index->index(), _co_sdo.subIndex, _co_sdo.index->subIndex(subindex)->value());
            _co_sdo.stay = static_cast<quint32>(_co_sdo.index->subIndex(subindex)->length());
            _co_sdo.state = CO_SDO_STATE_UPLOAD_SEGMENT;
        }
    }

    return 0;
}

qint32 SDO::sdoDownloadInitiate(const QCanBusFrame &frame)
{
    quint32 seek = 0;
    QByteArray buffer;
    quint8 cmd = 0;
    quint16 index = 0;
    quint8 subindex = 0;
    index = SDO_INDEX(frame.payload());
    subindex = SDO_SUBINDEX(frame.payload());

    if ((index != _co_sdo.index->index()) || (subindex |= _co_sdo.subIndex))
    {
        // ABORT
        _co_sdo.state = CO_SDO_STATE_FREE;
        return 1;
    }
    else
    {
        if (_co_sdo.state == CO_SDO_STATE_UPLOAD_SEGMENT)
        {
            cmd = SDO_CCS_CLIENT_UPLOAD_SEGMENT;
            _co_sdo.toggle = 0;
            cmd |= (_co_sdo.toggle << 4) & SDO_TOGGLE_MASK;
            cmd |= ((7 - _co_sdo.index->subIndex(_co_sdo.subIndex)->length()) & SDO_N_MASK) << 2;

            seek = (static_cast<quint32>(_co_sdo.index->subIndex(subindex)->length()) - _co_sdo.stay);
            buffer.clear();
            buffer = _co_sdo.index->subIndex(_co_sdo.subIndex)->value().toByteArray().mid(static_cast<int32_t>(seek), SDO_SG_SIZE);
            _co_sdo.stay -= SDO_SG_SIZE;

            if (_co_sdo.stay < SDO_SG_SIZE)
            {
                // no more segments to be downloaded
                cmd |= SDO_C;
                _co_sdo.state = CO_SDO_STATE_FREE;
                emit dataObjetWritten();
            }
            _co_sdo.state = CO_SDO_STATE_UPLOAD_SEGMENT;
            sendSdoRequest(cmd, buffer);
        }
        else
        {
            emit dataObjetWritten();
        }
    }
    return 0;
}
qint32 SDO::sdoDownloadSegment(const QCanBusFrame &frame)
{
    quint32 seek = 0;
    QByteArray buffer;
    quint8 cmd = 0;

    if (_co_sdo.state == CO_SDO_STATE_UPLOAD_SEGMENT)
    {
        quint8 toggle = SDO_TOG_BIT(frame.payload());
        if (toggle != _co_sdo.toggle)
        {
            // ABORT
            _co_sdo.state = CO_SDO_STATE_FREE;
            return 1;
        }
        else
        {
            cmd = SDO_CCS_CLIENT_UPLOAD_SEGMENT;
            _co_sdo.toggle = ~_co_sdo.toggle;
            cmd |= (_co_sdo.toggle << 4) & SDO_TOGGLE_MASK;
            cmd |= ((7 - _co_sdo.index->subIndex(_co_sdo.subIndex)->length()) & SDO_N_MASK) << 2;

            seek = (static_cast<quint32>(_co_sdo.index->subIndex(_co_sdo.subIndex)->length()) - _co_sdo.stay);
            buffer.clear();
            buffer = _co_sdo.index->subIndex(_co_sdo.subIndex)->value().toByteArray().mid(static_cast<int32_t>(seek), SDO_SG_SIZE);
            _co_sdo.stay -= SDO_SG_SIZE;

            if (_co_sdo.stay < SDO_SG_SIZE)
            {
                // no more segments to be downloaded
                cmd |= SDO_C;
                _co_sdo.state = CO_SDO_STATE_FREE;
                emit dataObjetWritten();
            }
            _co_sdo.state = CO_SDO_STATE_UPLOAD_SEGMENT;
            sendSdoRequest(cmd, buffer);
        }
    }

    return 0;
}

qint32 SDO::sdoBlockDownload(const QCanBusFrame &frame)
{
    quint16 index = 0;
    quint8 subindex = 0;
    quint8 cmd = 0;
    quint8 sizeSeg = 7;
    quint32 seek = 0;
    QByteArray buffer;

    // ss : server subcommand
    quint8 ss = static_cast<quint8> (frame.payload()[0] & SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_MASK);

    if ((ss == SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_INIT_RESP) && (_co_sdo.state == CO_SDO_STATE_FREE))
    {
        // STATE SS = 0 -> Initiale download response
        index = SDO_INDEX(frame.payload());
        subindex = SDO_SUBINDEX(frame.payload());
        if (index != _co_sdo.index->index() || subindex != _co_sdo.subIndex)
        {
            qDebug() << "ERROR index, subindex not corresponding";
            _co_sdo.state = CO_SDO_STATE_FREE;
            return 1;
        }
        else
        {
            _co_sdo.blksize = static_cast<quint8>(frame.payload().data()[4]);
            _co_sdo.state = CO_SDO_STATE_BLOCK_DOWNLOAD;
            _co_sdo.seqno = 1;
        }
    }
    else if ((ss == SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_RESP) && (_co_sdo.state == CO_SDO_STATE_BLOCK_DOWNLOAD))
    {
        _co_sdo.blksize = static_cast<quint8>(frame.payload().data()[2]);
        quint8 ackseq = static_cast<quint8>(frame.payload().data()[1]);
        if (ackseq == 0)
        {
            // ERROR sequence detection from server
            // Re-Send block
            qDebug() << "ERROR sequence detection from server, ackseq : " << ackseq;
            _co_sdo.seqno = 1;
            _co_sdo.state = CO_SDO_STATE_BLOCK_DOWNLOAD;
            return 1;
        }
        //blksize++;
    }
    else if (ss == SDO_SCS_SERVER_BLOCK_DOWNLOAD_SS_END_RESP)
    {
        _co_sdo.state = CO_SDO_STATE_FREE;
    }

    if (_co_sdo.state == CO_SDO_STATE_BLOCK_DOWNLOAD_END)
    {
        cmd = SDO_CCS_CLIENT_BLOCK_DOWNLOAD_END;
        cmd |= SDO_CCS_CLIENT_BLOCK_DOWNLOAD_CS_END_REQ;
        cmd |= (sizeSeg - _co_sdo.stay) << 2;
        quint16 crc = 0;
        sendSdoRequest(cmd, crc);

        _co_sdo.state = CO_SDO_STATE_FREE;
        emit dataObjetWritten();
    }
    else
    {
        _co_sdo.seqno = 1;
        _co_sdo.state = CO_SDO_STATE_BLOCK_DOWNLOAD;
    }

    if (_co_sdo.state == CO_SDO_STATE_BLOCK_DOWNLOAD)
    {
        while (_co_sdo.seqno <= (_co_sdo.blksize) && (_co_sdo.stay > sizeSeg))
        {
            seek = (static_cast<quint32>(_co_sdo.index->subIndex(_co_sdo.subIndex)->value().toByteArray().size()) - _co_sdo.stay);
            buffer.clear();
            buffer = _co_sdo.index->subIndex(_co_sdo.subIndex)->value().toByteArray().mid(static_cast<int32_t>(seek), sizeSeg);

            sendSdoRequest(true, _co_sdo.seqno, buffer);

            _co_sdo.stay -= sizeSeg;
            _co_sdo.seqno++;

            if (_co_sdo.stay < sizeSeg)
            {
                seek = (static_cast<quint32>(_co_sdo.index->subIndex(_co_sdo.subIndex)->value().toByteArray().size()) - _co_sdo.stay);
                buffer.clear();
                buffer = _co_sdo.index->subIndex(_co_sdo.subIndex)->value().toByteArray().mid(static_cast<int32_t>(seek), sizeSeg);
                _co_sdo.seqno++;

                sendSdoRequest(false, _co_sdo.seqno, buffer);
                _co_sdo.state = CO_SDO_STATE_BLOCK_DOWNLOAD_END;
            }
        }
    }
    return 0;
}

// SDO upload initiate
bool SDO::sendSdoRequest(quint8 cmd, quint16 index, quint8 subindex)
{
    if (!_bus)
    {
        return false;
    }
    if (!_bus->canDevice())
    {
        return false;
    }
    QByteArray sdoWriteReqPayload;
    QDataStream data(&sdoWriteReqPayload, QIODevice::WriteOnly);
    data.setByteOrder(QDataStream::LittleEndian);
    data << cmd;
    data << index;
    data << subindex;

    QCanBusFrame frame;
    frame.setFrameId(_cobIdClientToServer + _nodeId);
    frame.setPayload(sdoWriteReqPayload);
    return _bus->canDevice()->writeFrame(frame);
}
// SDO upload segment, SDO block upload initiate, SDO block upload ends
bool SDO::sendSdoRequest(quint8 cmd)
{
    if (!_bus)
    {
        return false;
    }
    if (!_bus->canDevice())
    {
        return false;
    }
    QByteArray sdoWriteReqPayload;
    QDataStream data(&sdoWriteReqPayload, QIODevice::WriteOnly);
    data.setByteOrder(QDataStream::LittleEndian);
    data << cmd;

    QCanBusFrame frame;
    frame.setFrameId(_cobIdClientToServer + _nodeId);
    frame.setPayload(sdoWriteReqPayload);
    return _bus->canDevice()->writeFrame(frame);
}

// SDO download initiate
bool SDO::sendSdoRequest(quint8 cmd, quint16 index, quint8 subindex, const QVariant &data)
{
    if (!_bus)
    {
        return false;
    }
    if (!_bus->canDevice())
    {
        return false;
    }
    QByteArray sdoWriteReqPayload;
    QDataStream request(&sdoWriteReqPayload, QIODevice::WriteOnly);
    request.setByteOrder(QDataStream::LittleEndian);
    request << cmd;
    request << index;
    request << subindex;
    request << data.toUInt();

    QCanBusFrame frame;
    frame.setFrameId(_cobIdClientToServer + _nodeId);
    frame.setPayload(sdoWriteReqPayload);
    return _bus->canDevice()->writeFrame(frame);
}

// SDO download segment
bool SDO::sendSdoRequest(quint8 cmd, const QByteArray &value)
{
    if (!_bus)
    {
        return false;
    }
    if (!_bus->canDevice())
    {
        return false;
    }
    QByteArray sdoWriteReqPayload;
    QDataStream request(&sdoWriteReqPayload, QIODevice::WriteOnly);
    request.setByteOrder(QDataStream::LittleEndian);
    request << cmd;
    request << value.toUInt();

    QCanBusFrame frame;
    frame.setFrameId(_cobIdClientToServer + _nodeId);
    frame.setPayload(sdoWriteReqPayload);
    return _bus->canDevice()->writeFrame(frame);
}

// SDO block download end
bool SDO::sendSdoRequest(quint8 cmd, quint16 &crc)
{
    if (!_bus)
    {
        return false;
    }
    if (!_bus->canDevice())
    {
        return false;
    }
    QByteArray sdoWriteReqPayload;
    QDataStream data(&sdoWriteReqPayload, QIODevice::WriteOnly);
    data.setByteOrder(QDataStream::LittleEndian);
    data << cmd;
    data << crc;

    QCanBusFrame frame;
    frame.setFrameId(_cobIdClientToServer + _nodeId);
    frame.setPayload(sdoWriteReqPayload);
    return _bus->canDevice()->writeFrame(frame);
}

// SDO block upload initiate
bool SDO::sendSdoRequest(quint8 cmd, quint16 index, quint8 subindex, quint8 blksize, quint8 pst)
{
    Q_UNUSED(cmd);
    Q_UNUSED(index);
    Q_UNUSED(subindex);
    Q_UNUSED(blksize);
    Q_UNUSED(pst);
    return true;
}
// SDO block upload sub-block
bool SDO::sendSdoRequest(quint8 cmd, quint8 &ackseq, quint8 blksize)
{
    Q_UNUSED(cmd);
    Q_UNUSED(ackseq);
    Q_UNUSED(blksize);
    return true;
}

// SDO block download sub-block
bool SDO::sendSdoRequest(bool moreSegments, quint8 seqno, const QByteArray &segData)
{
    QByteArray sdoWriteReqPayload;
    QDataStream data(&sdoWriteReqPayload, QIODevice::WriteOnly);
    QCanBusFrame frame;
    data.setByteOrder(QDataStream::LittleEndian);

    if (moreSegments == false)
    {
        seqno |= 0x80;
        data << static_cast<quint8>(seqno);
    }
    else
    {
        data << static_cast<quint8>(seqno);
    }
    //data << segData.toUInt();

    sdoWriteReqPayload.append(segData);
    frame.setFrameId(_cobIdClientToServer + _nodeId);
    frame.setPayload(sdoWriteReqPayload);

    return _bus->canDevice()->writeFrame(frame);
}

