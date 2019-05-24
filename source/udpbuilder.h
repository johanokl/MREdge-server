/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef UDPBUILDER_H
#define UDPBUILDER_H

#include "global.h"
#include "networkconnection.h"
#include <QHostAddress>
#include <QObject>

namespace MREdge {

/**
 * @brief The UdpBuilder class
 *
 * Parse the UDP datagram stream for a client session and sends out
 * file objects found in it.
 * Implements the MREdge network protocol to overcome issues related
 * to UDP packet loss and packet corruption.
 */
class UdpBuilder : public QObject {
  Q_OBJECT

public:
  UdpBuilder(QString host, quint16 port);
  ~UdpBuilder() {}
  void setSession(qint32 session) {
    mSession = session;
  }

public slots:
  void readData(QString host, quint16 port, QByteArrayPtr data);

signals:
  void fileReady(qint32 session, NetworkConnection::File file);

private:
  explicit UdpBuilder();

  QString mHost;
  quint16 mPort;
  qint32 mSession;
  QByteArrayPtr mData;
  int mNumReceivedBytes;
  int mCurrentFileId;
  qint16 mCurrentType;
  int mCurrentSize;
  bool mSent;
  static const int HEADER_SIZE = 16;
};

}

#endif // UDPBUILDER_H
