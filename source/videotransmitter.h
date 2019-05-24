/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef VIDEOTRANSMITTER_H
#define VIDEOTRANSMITTER_H

#include <QElapsedTimer>
#include <QObject>
#include <QMutex>
#include "global.h"
#include "videostreamer.h"

namespace MREdge {

class GStreamerTransmitter;
class GStreamerTransmitterContext;

/**
 * @brief The VideoTransmitter class
 */
class VideoTransmitter : public VideoStreamer {
  Q_OBJECT

public:
  explicit VideoTransmitter(qint32 session, QString desthost);
  virtual ~VideoTransmitter() override;
  void startStream(VideoStreamer::Format format, quint16 port);
  void setImageSize(QSize size);
  quint16 getPort() override { return mDestPort; }
  void setBitrate(int bitrate);

public slots:
  void addQImageToProcessQueue(qint32 , QImagePtr);
  void processQImage(qint32, QImagePtr image);
  void writeLog();

signals:
  void startTransmitter(QString, quint16, VideoStreamer::Format,
                        QSize size, int bitrate);

private:
  void init();
  GStreamerTransmitter *mGStreamerTransmitter;
  QElapsedTimer *mTimer;
  GStreamerTransmitterContext *mStreamContext;
  QString mDestHost;
  qint32 mSession;
  quint16 mDestPort;
  QImagePtr mImage;
  QMutex mMutex;
  Format mFormat;
  QSize mImageSize;
  int mBitrate;
  QTimer *mWriteLogTimer;
};

class GStreamerTransmitter : public QObject {
  Q_OBJECT

public:
  explicit GStreamerTransmitter() {}
  ~GStreamerTransmitter() {}

public slots:
  void start(QString dest, quint16 port, VideoStreamer::Format format,
             QSize size, int bitrate);

signals:
  void started(GStreamerTransmitterContext *);

private:
  QString mClientHost;
};

}

#endif // VIDEOTRANSMITTER_H
