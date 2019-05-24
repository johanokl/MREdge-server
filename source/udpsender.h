/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef UDPSENDER_H
#define UDPSENDER_H

#include "global.h"
#include "networkconnection.h"
#include <QHostAddress>
#include <QMutex>
#include <QObject>

class QUdpSocket;

namespace MREdge {

class UdpConnection;

/**
 * @brief The UdpSender class
 *
 * To be used by UdpConnection to Send Data.
 * Currently only supports having files sent with sendFileIfLatest, i.e. files sent
 * to this instance might be dropped before they are sent to the network layer.
 * The class is optimized to transmit real time data like video where latency is
 * most important and the second-latest file has little value.
 */
class UdpSender : public NetworkConnection {
  Q_OBJECT

  // Class only to be used by UdpConnection.
  friend class UdpConnection;

public:
  virtual ~UdpSender() override;

public slots:
  void sendFileIfLatest(qint32 session, File file) override;

signals:
  void fileSent();

private:
  explicit UdpSender();
  quint16 getPort() override { return 513; }

private slots:
  void sendFileUdp(qint32 session, QString host, quint16 port, qint32 packetsize, File file);
  void sendFile(qint32, File) override {
    // Only sendFileIfLatest is suported at the moment.
  }

private:
  QMutex sendmutex;
  QUdpSocket *sendsock;
};

}

#endif // UDPSENDER_H
