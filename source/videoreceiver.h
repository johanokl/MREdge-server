/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef VIDEORECEIVER_H
#define VIDEORECEIVER_H

#include <QObject>
#include <QWaitCondition>
#include "global.h"
#include "networkconnection.h"
#include "videostreamer.h"

class QElapsedTimer;

namespace MREdge {

class GStreamerReceiver;
class GStreamerReceiverContext;

class VideoReceiver : public VideoStreamer {
  Q_OBJECT

public:
  explicit VideoReceiver(qint32 session, QString senderip);
  ~VideoReceiver() override;
  quint16 getPort() override { return mPort; }
  Format getFormat() { return mFormat; }
  QMap<quint32, qint64> getProcessingTimes() const;

public slots:
  void start(Format format, bool useJitterbuffer);
  void newMat(quint32 id, cvMatPtr image);
  void setLogTime(bool enable, QElapsedTimer* timer=nullptr) {
    mLogTime = enable; mUptime=timer; }

signals:
  void startReceiver(Format format, QString senderip, bool useJitterbuffer);
  void ready(qint32 session, VideoStreamer::Format format, quint16 port);

private:
  GStreamerReceiver *mGStreamerReceiver;
  QString mSenderHost;
  qint32 mSession;
  Format mFormat;
  quint16 mPort;
  QTimer *mWriteLogTimer;
  QElapsedTimer* mUptime;
  bool mLogTime = false;
  QMap<quint32, qint64> mFramesArrivedNsec;
};

class GStreamerReceiver : public QObject {
  Q_OBJECT

public:
  explicit GStreamerReceiver(VideoReceiver *parent);
  ~GStreamerReceiver();
  void stop();
public slots:
  void start(VideoStreamer::Format format, QString host, bool useJitterbuffer);
  void writeLog();
signals:
  void ready(VideoStreamer::Format format, quint16 port);

public:
  VideoReceiver *parent() { return mParent; }
private:
  GStreamerReceiverContext *mCtx;
  VideoReceiver *mParent;
  bool mLogWritten;
};

}

#endif // VIDEORECEIVER_H
