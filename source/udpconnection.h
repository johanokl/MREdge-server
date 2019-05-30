/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef UDPCONNECTION_H
#define UDPCONNECTION_H

#include "global.h"
#include "networkconnection.h"
#include <tuple>
#include <QMap>
#include <QObject>

class QUdpSocket;

namespace MREdge {

class MRServer;
class UdpSender;
class UdpBuilder;

/**
 * @brief The UdpConnection class
 *
 * A UDP server. Manages UDP client sessions, receiving and sending files
 * and UDP related threads.
 * Data sent to the server port must follow the MREdge network communication format
 * for header parsing to work.
 * Uses QUdpSocket as the underlying layer to the system network platform.
 */
class UdpConnection : public NetworkConnection {
  Q_OBJECT

public:
  UdpConnection(MRServer *parent, quint16 port);
  ~UdpConnection() override;
  quint16 getPort() override { return mPort; }
  QMap<quint32, qint64> getProcessingTimes(qint32 session) override;

signals:
  void dataAvailable(QString, quint16, QByteArrayPtr);
  void fileToSendUdp(qint32 session, QString host, quint16 port,
                     qint32 packetsize, File file);

public slots:
  void readyRead();
  void setPacketSize(qint32 session, qint32 packetsize);
  void sendFile(qint32 session, File file) override;
  void sendFileIfLatest(qint32 session, File file) override;
  void setLogTime(bool enable, QElapsedTimer* timer) override;

private:
  QUdpSocket *mRecvsock;
  UdpSender *mUdpSender;
  MRServer *mMRServer;
  QMap<std::pair<QString, quint16>, UdpBuilder *> mUdpBuilders;
  QMap<std::pair<QString, quint16>, QByteArrayDataPtr> data;
  QMutex mSessionsMutex;
  QMap<qint32, std::tuple<QString, quint16, quint16>> mSessions;
  quint16 mPort = 0;
  qint32 mDefaultPacketSize;
};

}

#endif // UDPCONNECTION_H
