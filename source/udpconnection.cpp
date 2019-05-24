/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "udpconnection.h"
#include "mrserver.h"
#include "udpbuilder.h"
#include "udpsender.h"
#include <tuple>
#include <QThread>
#include <QUdpSocket>
#include <QDataStream>

namespace MREdge {

/**
 * @brief UdpConnection::UdpConnection
 * @param parent The owner.
 * @param port Port to listen to.
 *
 * Starts the UDP server listener.
 * Status can be checked with getPort(), where 0 means
 * that the serve initialization wasn't successful (the port was in use).
 */
UdpConnection::UdpConnection(MRServer *parent, quint16 port)
{
  this->mMRServer = parent;
  mRecvsock = new QUdpSocket(this);
  bool bound = mRecvsock->bind(QHostAddress::Any, port);
  mPort = bound ? port : 0;
  QObject::connect(mRecvsock, &QUdpSocket::readyRead,
                   this, &UdpConnection::readyRead);
  // Create a sender instance and move it to a new thread.
  mUdpSender = new UdpSender();
  mUdpSender->moveToThread(new QThread(this));
  mUdpSender->thread()->start();
  QObject::connect(this, &UdpConnection::fileToSendUdp,
                   mUdpSender, &UdpSender::sendFileUdp);
}

/**
 * @brief UdpConnection::~UdpConnection
 *
 * Close receiver and sender threads.
 */
UdpConnection::~UdpConnection()
{
  for (auto builder : mUdpBuilders) {
    builder->thread()->deleteLater();
  }
  mUdpSender->thread()->deleteLater();
  mRecvsock->close();
}

/**
 * @brief UdpConnection::setPacketSize
 * @param session Session id
 * @param packetsize Packet size for this session. [100-2000]
 *
 * Sets the packet size to be used when sending files to
 * the specified session.
 * Packet sizes should not be too big, to prevent the packet from
 * being broken up by the underlying network layers, or too small to prevent
 * noise or packet being received in the wrong order.
 * Recommended values are 500-800 bytes.
 */
void UdpConnection::setPacketSize(qint32 session, qint32 packetsize)
{
  fDebug << "setPacketSize " << session << " " << packetsize;
  mSessionsMutex.lock();
  if (mSessions.contains(session)) {
    auto obj = mSessions.value(session);
    mSessions.insert(session, std::make_tuple(std::get<0>(obj), std::get<1>(obj), packetsize));
  } else {
    mSessions.insert(session, std::make_tuple("", 0, packetsize));
  }
  mSessionsMutex.unlock();
}

/**
 * @brief UdpConnection::sendFile
 * @param session The session id for the connection.
 * @param file NetworkConnection::File to be sent.
 *
 * Sends a NetworkConnection::File to the host and port for the specified session.
 * If the file's an image and this instance's setSendImagesForSession() has
 * been set to false for this session id the call will be ignored.
 */
void UdpConnection::sendFile(qint32 session, File file)
{
  if (!(file.type == NetworkConnection::FileType::IMAGE ||
        file.type == NetworkConnection::FileType::IMAGE_WITH_METADATA) ||
      !sendImagesForSession(session)) {
    return;
  }
  mSessionsMutex.lock();
  bool contains = mSessions.contains(session);
  auto dest = mSessions.value(session);
  mSessionsMutex.unlock();
  if (!contains) {
    fDebug << "Can not find session id=" << session;
    return;
  }
  fDebug << QString("sendFile: type=%1, length=%2")
            .arg(file.type).arg(file.data->size());
  // Read target IP, target port, session packet size and pass it to the UDP sender
  fDebug << "sendFile: fileToSendUdp";
  emit fileToSendUdp(session, std::get<0>(dest), std::get<1>(dest), std::get<2>(dest), file);
}

/**
 * @brief UdpConnection::sendFileIfLatest
 * @param session The session id for the connection.
 * @param file NetworkConnection::File to be sent.
 *
 * Sends a NetworkConnection::File to the host and port for the specified session.
 * If the file's an image and this instance's setSendImagesForSession() has
 * been set to false for this session id the call will be ignored.
 * Before the socket is ready and the file is sent, any future calls to
 * this function will replace this file with the new one.
 */
void UdpConnection::sendFileIfLatest(qint32 session, File file)
{
  if (!(file.type == NetworkConnection::FileType::IMAGE ||
        file.type == NetworkConnection::FileType::IMAGE_WITH_METADATA) ||
      !sendImagesForSession(session)) {
    return;
  }
  mSessionsMutex.lock();
  bool contains = mSessions.contains(session);
  auto dest = mSessions.value(session);
  mSessionsMutex.unlock();
  if (!contains) {
    fDebug << "Can not find session id=" << session;
    return;
  }
  mUdpSender->sendFileIfLatest(session, file);
  file.data = nullptr;
  emit fileToSendUdp(session, std::get<0>(dest), std::get<1>(dest), std::get<2>(dest), file);
}

/**
 * @brief UdpConnection::readyRead
 *
 * Called by the system network layer when a new UDP datagram (packet)
 * has been received on the server port.
 */
void UdpConnection::readyRead()
{
  while (mRecvsock->hasPendingDatagrams()) {
    QByteArrayPtr bufferPtr(new QByteArray);
    bufferPtr->resize(static_cast<int>(mRecvsock->pendingDatagramSize()));
    QHostAddress host;
    quint16 senderPort;
    mRecvsock->readDatagram(bufferPtr->data(), bufferPtr->size(),
                            &host, &senderPort);
    QString senderHost = host.toString();
    if (!mUdpBuilders.contains(std::make_pair(senderHost, senderPort))) {
      // This client host and client port has not been seen before.
      // Create a new UDP session.
      fDebug << "New UDP connection";
      auto udpbuilder = new UdpBuilder(senderHost, senderPort);
      // Send all received data to the UDP stream parser.
      QObject::connect(this, &UdpConnection::dataAvailable,
                       udpbuilder, &UdpBuilder::readData);
      mUdpBuilders.insert(std::make_pair(senderHost, senderPort), udpbuilder);
      // A new file object has been received from the UDP stream parser.
      QObject::connect(udpbuilder, &UdpBuilder::fileReady,
                       [=](qint32 session, File file) {
        if (file.type == NetworkConnection::FileType::CONNECTION) {
          // The client has sent a session ID. We now know which TCP session
          // this UDP session is bound to.
          QDataStream dstream(file.data.data(), QIODevice::OpenModeFlag::ReadOnly);
          dstream.setByteOrder(QDataStream::BigEndian);
          qint32 session;
          dstream >> session;
          fDebug << "Register TCP session id=" << session;
          udpbuilder->setSession(session);
          mSessionsMutex.lock();
          if (mSessions.contains(session)) {
            // Packet size has already been defined for this session. Reuse the old session.
            auto obj = mSessions.value(session);
            mSessions.insert(session, std::make_tuple(senderHost, senderPort, std::get<2>(obj)));
          } else {
            // Create new entry in the session list.
            mSessions.insert(session, std::make_tuple(senderHost, senderPort, mDefaultPacketSize));
          }
          mSessionsMutex.unlock();
        } else {
          emit fileReady(session, file);
        }
      });
      // Create a new thread and move the UDP stream parser to that one.
      udpbuilder->moveToThread(new QThread(this));
      udpbuilder->thread()->start();
    }
    emit dataAvailable(senderHost, senderPort, bufferPtr);
  }
}

}
