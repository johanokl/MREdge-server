/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include "global.h"
#include "networkconnection.h"
#include <QMap>
#include <QObject>

class QTcpSocket;
class QTcpServer;

namespace MREdge {

class UdpSender;
class MRServer;
class TcpBuilder;
class ImageProcesser;

/**
 * @brief The TcpConnection class
 *
 * A TCP server. Manages TCP client sockets, receiving and sending files
 * and TCP related threads.
 * Data sent to the sockets must follow the MREdge network communication format
 * for header parsing to work.
 * Uses QTcpServer as the underlying layer for communication with the system platform.
 */
class TcpConnection : public NetworkConnection {
  Q_OBJECT

public:
  TcpConnection(MRServer *parent, quint16 port);
  ~TcpConnection() override;
  quint16 getPort() override { return mPort; }

public slots:
  void newConnection();
  void sendFile(qint32 sessionId, NetworkConnection::File file) override;
  void sendFileIfLatest(qint32 session, File file) override;

signals:
  void sendFileNow(qint32 session, NetworkConnection::File file);
  void dataAvailable(qint32, QByteArrayPtr);

private slots:
  void socketDisconnected(QTcpSocket *client);
  void readyRead(QTcpSocket *socket);

protected:
  void bytesWritten(qint32 session, qint64 bytesWritten) override;

private:
  MRServer *mMRServer;
  QTcpServer *mServer;
  QMap<qint32, QTcpSocket*> mSockets;
  QMap<qint32, TcpBuilder*> mTcpBuilders;
  quint16 mPort = 0;
  QMutex mSendBufferLevelsMutex;
  QMutex mSessionMutex;
};

}

#endif // TCPCONNECTION_H
