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


namespace MREdge {

class GStreamerReceiver;
class GStreamerReceiverContext;

class VideoReceiver : public VideoStreamer {
  Q_OBJECT

public:
  explicit VideoReceiver(qint32 session, QString senderip);
  ~VideoReceiver() override;
  void start(Format format, bool useJitterbuffer);
  quint16 getPort() override { return mPort; }
  void newMat(cvMatPtr image);
  void statisticsUpdated(QJsonObject statJson);
  Format getFormat() { return mFormat; }

signals:
  void startReceiver(Format format, QString senderip, bool useJitterbuffer);
  void ready(qint32 session, VideoStreamer::Format format, quint16 port);
  void statistics(qint32 session, QJsonObject statistics);

private:
  GStreamerReceiver *mGStreamerReceiver;
  QString mSenderHost;
  qint32 mSession;
  Format mFormat;
  quint16 mPort;
  int mFramesSent = 0;
  QTimer *mWriteLogTimer;
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
  void matReady(cvMatPtr image, int frameid);
  void ready(VideoStreamer::Format format, quint16 port);

public:
  VideoReceiver *parent() { return mParent; }
private:
  GStreamerReceiverContext *mCtx;
  VideoReceiver *mParent;
};

}

#endif // VIDEORECEIVER_H
