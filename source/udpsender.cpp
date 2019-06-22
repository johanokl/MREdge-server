/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "udpsender.h"
#include <QDataStream>
#include <QUdpSocket>
#include <QtMath>
#include <QElapsedTimer>
#include <mutex>

namespace MREdge {

/**
 * @brief UdpSender::UdpSender
 */
UdpSender::UdpSender()
{
  sendsock = new QUdpSocket(this);
  sendsock->bind(0);
}

/**
 * @brief UdpSender::~UdpSender
 */
UdpSender::~UdpSender()
{
  delete sendsock;
}

/**
 * @brief UdpSender::sendFileIfLatest
 * @param session
 * @param file
 */
void UdpSender::sendFileIfLatest(qint32 session, File file)
{
  mSendBufferFilesMutex.lock();
  if (mSendBufferFiles.contains(session)) {
    fDebug << "New file replaces old file in buffer.";
  }
  mSendBufferFiles.insert(session, file);
  mSendBufferFilesMutex.unlock();
}

/**
 * @brief UdpSender::sendFileUdp
 * @param session Session ID
 * @param host Target host IP number.
 * @param port Target port.
 * @param packetsize Packet size to use.
 * @param file The file being sent.
 *
 * Sends a file to to session using [(file size) / (packet size)] number of UDP datagrams.
 */
void UdpSender::sendFileUdp(qint32 session, QString host, quint16 port, qint32 packetsize, File file)
{
  sendmutex.lock();
  mSendBufferFilesMutex.lock();
  if (mSendBufferFiles.contains(session)) {
    file = mSendBufferFiles.take(session);
  }
  if (mLogTime && mUptime) {
    mTimeLogsMutex.lock();
    if (!mTimeLogs.contains(session)) {
      mTimeLogs.insert(session, new QMap<quint32, qint64>());
    }
    auto timelogs = mTimeLogs.value(session, nullptr);
    if (timelogs) {
      timelogs->insert(file.id, mUptime->nsecsElapsed());
    }
    mTimeLogsMutex.unlock();
  }

  mSendBufferFilesMutex.unlock();
  // Take the latest file in the pipeline. As this function
  // fDebug << QString("Sending (session=%1, host=%2, port=%3, type=%4, length=%5)")
  //           .arg(session).arg(host).arg(port).arg(file.type)
  //           .arg((file.data != nullptr) ? file.data->length() : 0);

  qint32 offset = 0;
  qint32 totalbytes = (file.data != nullptr) ? file.data->length() : 0;
  QHostAddress hostAddress(host);
  while (true) {
    // There are still data to be sent. Create a new packet and send it.
    qint32 currpacketsize = qMin(packetsize, (totalbytes - offset));
    QByteArray datagram;
    // Create a header.
    QDataStream dstream(&datagram, QIODevice::OpenModeFlag::WriteOnly);
    dstream.setByteOrder(QDataStream::BigEndian);
    dstream << static_cast<qint16>(file.type);
    dstream << static_cast<qint32>(file.id);
    dstream << static_cast<qint32>(totalbytes);
    dstream << static_cast<qint32>(offset);
    dstream << static_cast<qint16>(currpacketsize);
    if (file.data != nullptr) {
      datagram.insert(16, file.data->data() + offset, currpacketsize);
    }
    offset += currpacketsize;
    sendsock->writeDatagram(datagram, hostAddress, port);
    if (offset >= totalbytes) {
      break;
    }
  }
  emit fileSent();
  sendmutex.unlock();
}

}
