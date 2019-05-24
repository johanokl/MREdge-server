/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "udpbuilder.h"
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QThread>

namespace MREdge {

/**
 * @brief UdpBuilder::UdpBuilder
 * @param host Sender port.
 * @param port Sender port.
 */
UdpBuilder::UdpBuilder(QString host, quint16 port)
{
  mHost = host;
  mPort = port;
  mNumReceivedBytes = 0;
  mCurrentFileId = -1;
  mData = QByteArrayPtr(new QByteArray);
  mCurrentType = 0;
  mCurrentSize = 0;
  mSession = 0;
  mSent = true;
}

/**
 * @brief UdpBuilder::readData
 * @param host Sender host.
 * @param port Sender port.
 * @param data The packet including header and data.
 *
 * New data received from the network layer.
 */
void UdpBuilder::readData(QString host, quint16 port, QByteArrayPtr data)
{
  if (host != mHost || port != mPort) {
    // The data is not for this session.
    return;
  }

  // Parse the datagram header
  QDataStream dstream(data.data(), QIODevice::OpenModeFlag::ReadOnly);
  dstream.setByteOrder(QDataStream::BigEndian);
  qint16 type, packetsize;
  qint32 fileid, offset, totalsize;
  dstream >> type;
  dstream >> fileid;
  dstream >> totalsize;
  dstream >> offset;
  dstream >> packetsize;

  if (fileid < mCurrentFileId) {
    // The packet file id is for a file already passed along.
    // Discard late-arriving packets.
    return;
  }

  if (!mSent && fileid > mCurrentFileId) {
    // While we haven't received 100% of this file we have now
    // received a packet with a new file. Send out the old received data so
    // that we can start with the new file.
    mData->truncate(mCurrentSize);
    emit fileReady(mSession, NetworkConnection::File(
                     mCurrentType, mCurrentFileId, mData));
    mSent = true;
  }

  if (mSent == true) {
    // Start with a new file. Set up a new file session
    // with the data from the header.
    mSent = false;
    mData = QByteArrayPtr(new QByteArray);
    mData->resize(totalsize);
    mCurrentSize = totalsize;
    mCurrentType = type;
    mNumReceivedBytes = 0;
    mCurrentFileId = fileid;
  }

  int thisPacketSize = data->length() - HEADER_SIZE;
  mNumReceivedBytes += thisPacketSize;

  if (packetsize != thisPacketSize) {
    // The datagram does not contain all the data it reports that it contains.
    fDebug << QString("UdpBuilder: ERROR: Packet sizes incorrect %1 %2")
              .arg(packetsize).arg(thisPacketSize);
  }
  if (offset > totalsize) {
    // The datagram wants to be inserted in a strange position.
    fDebug << "UdpBuilder: ERROR: Offset too large: " << offset;
  }

  mData->insert(offset, data->mid(HEADER_SIZE, packetsize));
  if (!mSent && mNumReceivedBytes >= mCurrentSize) {
    // We have received 100% of all packets for this file.
    mData->truncate(mCurrentSize);
    emit fileReady(mSession, NetworkConnection::File(
                     mCurrentType, mCurrentFileId, mData));
    mSent = true;
  }
}

}
