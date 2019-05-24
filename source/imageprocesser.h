/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef IMAGEPROCESSER_H
#define IMAGEPROCESSER_H

#include "global.h"
#include "networkconnection.h"
#include <QImage>
#include <QMutex>
#include <QObject>

namespace MREdge {

class ImageProcesser : public QObject
{
  Q_OBJECT

public:
  virtual ~ImageProcesser() {}
  static QImage qImageFromMat(cv::Mat img);

signals:
  void sendFile(qint32 session, NetworkConnection::File file);
  void sendQImage(qint32 session, QImagePtr image);

public slots:
  void setEmitJPEG(bool enable) { mEmitJPEG = enable; }
  void setEmitMetadata(bool enable) { mEmitMetadata = enable; }
  void setEmitQImage(bool enable) { mEmitQImage = enable; }
  virtual void setDebugMode(bool enable) { mDebugMode = enable; }
  void setAllowAllSources(bool allow) { mAllowAllSources = allow; }
  void triggerActionA();
  void triggerActionB();
  void triggerActionC();
  void addFileToProcessQueue(qint32 session, NetworkConnection::File file);
  void addMatToProcessQueue(qint32 session, cvMatPtr image, int frameid);
  void addQImageToProcessQueue(qint32 session, QImage image, int frameid);
  void processFile(qint32 session, NetworkConnection::File file);
  void processMat(qint32 session, cvMatPtr image, int frameid);
  void processQImage(qint32 session, QImage image, int frameid);
  virtual void setConfig(QJsonObject calibration) = 0;
  virtual void setCalibrateMode(bool enabled) = 0;
  virtual void calibrateCamera() = 0;
  virtual void setUserInteractionConfiguration(QJsonObject obj) = 0;

protected:
  virtual void process(qint32 session, cvMatPtr image, int frameid) = 0;

  QByteArrayPtr jpegFromQImage(
      QImage image, bool addmetadata=false, int metadata=0);
  QByteArrayPtr jpegFromMat(
      cv::Mat image, bool addmetadata=false, int metadata=0);
  cv::Mat matFromQImage(QImage img);

  qint32 mSession;
  QMutex mProcessQueueMutex;
  NetworkConnection::File mFileProcessQueue;
  cvMatPtr mMatProcessQueue;
  QImage mQImageProcessQueue;
  int mMatQueuePosition = 0;
  int mFileQueuePosition = 0;
  int mQImageQueuePosition = 0;
  bool mEmitMetadata = false;
  bool mEmitJPEG = false;
  bool mEmitQImage = false;
  bool mTriggeredA = false;
  bool mTriggeredB = false;
  bool mTriggeredC = false;
  bool mDebugMode = false;
  bool mAllowAllSources = false;
};

}

#endif // IMAGEPROCESSER_H
