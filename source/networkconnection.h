/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef NETWORKCONNECTION_H
#define NETWORKCONNECTION_H

#include "global.h"
#include <QDebug>
#include <QMutex>
#include <QJsonObject>

namespace MREdge {

class NetworkConnection : public QObject {
  Q_OBJECT

public:
  virtual quint16 getPort() = 0;

  class FileType {
  public:
    static const qint16 NONE = 0;
    static const qint16 CONNECTION = 1;
    static const qint16 JSON = 2;
    static const qint16 IMAGE = 3;
    static const qint16 IMAGE_WITH_METADATA = 4;
    static const qint16 CALIBRATION = 5;
    static const qint16 TRIGGER_A = 6;
    static const qint16 TRIGGER_B = 7;
    static const qint16 TRIGGER_C = 8;
  };

  class File {
  public:
    File(qint16 _type = 0, qint32 _id = 0,
         QByteArrayPtr _data = nullptr, qint32 _metadata = 0)
      : id(_id), type(_type), data(_data), metadata(_metadata) {}
    ~File() {}
    File(const File &rhs) :
      id(rhs.id), type(rhs.type),
      data(rhs.data), metadata(rhs.metadata) {}
    qint32 id;
    qint16 type;
    QByteArrayPtr data;
    qint32 metadata;
  };

signals:
  void fileReady(qint32 session, File file);
  void matReady(qint32 session, cvMatPtr image, int frameid);
  void newSession(qint32 session, QString host, quint16 port);
  void sessionDestroyed(qint32);

public slots:
  virtual void sendFileIfLatest(qint32 /*session*/, File /*file*/) {}
  virtual void sendFile(qint32 /*session*/, File /*file*/) {}
  virtual void setSendImagesForSession(qint32 sessionId, bool enabled);
  virtual bool sendImagesForSession(qint32 sessionId);

protected:
  virtual void bytesWritten(qint32, qint64) {}

  QMap<qint32, qint64> mSendBufferLevels;
  QMap<qint32, File> mSendBufferFiles;
  QMap<qint32, bool> mSendImagesForSessionList;
  QMutex mSendBufferFilesMutex;
  QMutex mSendImagesForSessionListMutex;
};

}

#endif // NETWORKCONNECTION_H
