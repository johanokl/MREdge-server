/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "tcpconnection.h"
#include "cannyfilter.h"
#include "mrserver.h"
#include "tcpbuilder.h"
#include "udpsender.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QDateTime>

namespace MREdge {

/**
 * @brief TcpConnection::TcpConnection
 * @param parent The owner.
 * @param port Port to listen to. Use 0 if up to system to decide which.
 *
 * Starts the TCP server. If port is set to 0 the system will select an
 * available one. Status can be checked with getPort(), where 0 means
 * that the serve initialization wasn't successful (the port was in use).
 */
TcpConnection::TcpConnection(MRServer *parent, quint16 port)
{
  this->mMRServer = parent;
  mServer = new QTcpServer(this);
  bool portBound = mServer->listen(QHostAddress::Any, port);
  if (portBound) {
    mPort = mServer->serverPort();
  } else {
    mPort = 0;
  }
  QObject::connect(mServer, &QTcpServer::newConnection,
                   this, &TcpConnection::newConnection);
  QObject::connect(this, &TcpConnection::sendFileNow,
                   this, &TcpConnection::sendFile);
  // Used to generate session id.
}

/**
 * @brief TcpConnection::~TcpConnection
 * Close all sockets and TCP listeners.
 */
TcpConnection::~TcpConnection()
{
  auto begin = mTcpBuilders.begin();
  while (begin != mTcpBuilders.end()) {
    begin.value()->deleteLater();
    begin++;
  }
  mServer->close();
}

/**
 * @brief TcpConnection::sendFileIfLatest
 * @param session
 * @param file
 */
void TcpConnection::sendFileIfLatest(qint32 session, File file)
{
  if ((file.type == NetworkConnection::FileType::IMAGE ||
        file.type == NetworkConnection::FileType::IMAGE_WITH_METADATA) &&
      !sendImagesForSession(session)) {
    return;
  }
  mSendBufferFilesMutex.lock();
  if (mSendBufferFiles.contains(session)) {
    fDebug << "New file replaces old file in buffer.";
  }
  mSendBufferFiles.insert(session, file);
  fDebug << "Inserted";
  mSendBufferFilesMutex.unlock();
  bytesWritten(session, 0);
}

/**
 * @brief TcpConnection::sendFile
 * @param session The session id for the connection.
 * @param file NetworkConnection::File to be sent.
 *
 * Sends a NetworkConnection::File to the socket for the specified session.
 * If the file's an image and this instance's setSendImagesForSession() has been set
 * to false for this session id the call will be ignored.
 */
void TcpConnection::sendFile(qint32 session, File file)
{
  if ((file.type == NetworkConnection::FileType::IMAGE ||
       file.type == NetworkConnection::FileType::IMAGE_WITH_METADATA) &&
      !sendImagesForSession(session)) {
    return;
  }
  fDebug << QString("sendFile: type=%1, length=%2")
            .arg(file.type).arg(file.data->size());
  mSessionMutex.lock();
  QTcpSocket *sendsock = mSockets.value(session, nullptr);
  mSessionMutex.unlock();
  if (sendsock == nullptr || !sendsock->isOpen()) {
    fDebug << "No valid open socket";
    return;
  }
  mSendBufferLevelsMutex.lock();
  QByteArray datagram;
  // Create a header for the file. Must follow this format and be
  // big endian to be parsable by the Java clients.
  QDataStream dstream(&datagram, QIODevice::OpenModeFlag::WriteOnly);
  dstream.setByteOrder(QDataStream::BigEndian);
  dstream << static_cast<qint32>(file.data->size());
  dstream << static_cast<qint16>(file.type);
  dstream << static_cast<qint32>(file.id);
  // Add the header and file lengths to the buffer level, so that we
  // can see when the file has been sent by listening to bytesWritten().
  auto level = mSendBufferLevels.value(session);
  level += sendsock->write(datagram);
  level += sendsock->write(*file.data);
  mSendBufferLevels.insert(session, level);
  mSendBufferLevelsMutex.unlock();
}

/**
 * @brief TcpConnection::newConnection
 *
 * Called by the underlying system when a connection has been made.
 * Creates class instances that will parse incoming data and send files
 * for the new session.
 */
void TcpConnection::newConnection()
{
  while (mServer->hasPendingConnections()) {
    QTcpSocket *client = mServer->nextPendingConnection();
    QString clientHost = client->peerAddress().toString();
    quint16 clientPort = client->peerPort();
    fDebug << QString("New connection: Sender ip: %1, sender port %2")
              .arg(clientHost).arg(clientPort);
    mSessionMutex.lock();
    qint32 sessionId = 0;
    // Generate a new unique session id. Randomize id until unique.
    while (true) {
      sessionId = static_cast<qint32>(qrand());
      if (sessionId < 0) {
        // Id must be > 0.
        sessionId = 0 - sessionId;
      }
      if (!mSockets.contains(sessionId)) {
        fDebug << "New session id: " << sessionId;
        break;
      }
    }
    mSockets.insert(sessionId, client);
    mSessionMutex.unlock();
    auto tcpbuilder = new TcpBuilder(sessionId);
    mTcpBuilders.insert(sessionId, tcpbuilder);
    // Announce that a new session has been set up so that other
    // classes like videoreceiver etc. can be set up.
    emit newSession(sessionId, clientHost, clientPort);
    // Run the TCP message parser in a separate thread.
    tcpbuilder->moveToThread(new QThread(this));
    tcpbuilder->thread()->start();

    QObject::connect(client, &QTcpSocket::readyRead,
                     [=]() { this->readyRead(client); });
    QObject::connect(client, &QAbstractSocket::disconnected,
                     client, &QObject::deleteLater);
    QObject::connect(client, &QAbstractSocket::disconnected,
                     [=]() { this->socketDisconnected(client); });
    QObject::connect(this, &TcpConnection::dataAvailable,
                     tcpbuilder, &TcpBuilder::readData);
    QObject::connect(tcpbuilder, &TcpBuilder::fileReady,
                     [=](qint32 sessionId, File file) {
      emit fileReady(sessionId, file);
    });
    QObject::connect(client, &QTcpSocket::disconnected,
                     [=]() {
      mSessionMutex.lock();
      fDebug << "TCP: Socket Disconnected";
      mSockets.remove(sessionId);
      mSessionMutex.unlock();
    });
    QObject::connect(client, &QTcpSocket::bytesWritten,
                     [=](qint64 bytes) {
      bytesWritten(sessionId, bytes);
    });
    // For debugging reasons this has been disabled.
    /*
    QObject::connect(client, &QAbstractSocket::disconnected,
                     client, &QObject::deleteLater);
    QObject::connect(client, &QAbstractSocket::disconnected,
                     tcpbuilder, &QObject::deleteLater);
    QObject::connect(client, &QAbstractSocket::disconnected,
                     client, &QObject::deleteLater);
    */
  }
}

/**
 * @brief TcpConnection::bytesWritten
 * @param session Session id for socket.
 * @param bytesWritten Bytes sent since the last call to this callback.
 *
 * Callback function for socket usage for seeing if we have sent all data in the
 * pipeline and a new file can be sent.
 * If the pipeline is empty, a new file is picked from the send buffer and sent.
 * Used to control files to be sent with sendFilesIfLatest().
 */
void TcpConnection::bytesWritten(qint32 session, qint64 bytesWritten)
{
  auto level = mSendBufferLevels.value(session);
  level -= bytesWritten;
  mSendBufferLevelsMutex.lock();
  mSendBufferLevels.insert(session, level);
  mSendBufferLevelsMutex.unlock();
  if (level == 0) {
    mSendBufferFilesMutex.lock();
    File file = mSendBufferFiles.take(session);
    mSendBufferFilesMutex.unlock();
    if (!file.data.isNull()) {
      emit sendFileNow(session, file);
    }
  }
}

/**
 * @brief TcpConnection::readyRead
 * @param client Socket in use.
 *
 * Called by QTcpSocket when it has received new data. Socket is mapped to a
 * session id and data is packaged into a byte array and sent to the TcpBuilder
 * function to be parsed and rebuilt.
 */
void TcpConnection::readyRead(QTcpSocket *client)
{
  mSessionMutex.lock();
  auto session = mSockets.key(client, 0);
  mSessionMutex.unlock();
  QByteArrayPtr bufferPtr(new QByteArray(client->readAll()));
  emit dataAvailable(session, bufferPtr);
}

/**
 * @brief TcpConnection::disconnected
 * @param client Socket that was disconnected.
 *
 * Called by QTcpSocket when the client has disconnected.
 */
void TcpConnection::socketDisconnected(QTcpSocket *client)
{
  mSessionMutex.lock();
  auto session = mSockets.key(client, 0);
  emit sessionDestroyed(session);
  mSessionMutex.unlock();
}


}
