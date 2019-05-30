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

class QElapsedTimer;

namespace MREdge {

class ImageProcesser : public QObject
{
  Q_OBJECT

public:
  static QImage qImageFromMat(cv::Mat img);
  static QByteArrayPtr jpegFromQImage(
      QImage image, bool addmetadata=false, int metadata=0);
  static QByteArrayPtr jpegFromMat(
      cv::Mat image, bool addmetadata=false, int metadata=0);
  static cv::Mat matFromQImage(QImage img);

  ImageProcesser();
  virtual ~ImageProcesser() {}

  const QMap<quint32, qint32> getProcessingTimes();

signals:
  void sendFile(qint32 session, NetworkConnection::File file);
  void sendQImage(qint32 session, quint32 frameid, QImagePtr image);

public slots:
  void setEmitJPEG(bool enable) { mEmitJPEG = enable; }
  void setEmitMetadata(bool enable) { mEmitMetadata = enable; }
  void setEmitQImage(bool enable) { mEmitQImage = enable; }
  virtual void setDebugMode(bool enable) { mDebugMode = enable; }
  void setAllowAllSources(bool allow) { mAllowAllSources = allow; }
  void setLogTime(bool enable) { mLogTime = enable; }
  void triggerActionA();
  void triggerActionB();
  void triggerActionC();
  void addFileToProcessQueue(qint32 session, NetworkConnection::File file);
  void addMatToProcessQueue(qint32 session, quint32 frameid, cvMatPtr image);
  void addQImageToProcessQueue(qint32 session, quint32 frameid, QImage image);
  void processFile(qint32 session, NetworkConnection::File file);
  void processMat(qint32 session, quint32 frameid, cvMatPtr image);
  void processQImage(qint32 session, quint32 frameid, QImage image);
  virtual void setConfig(QJsonObject calibration) = 0;
  virtual void setCalibrateMode(bool enabled) = 0;
  virtual void calibrateCamera() = 0;
  virtual void setUserInteractionConfiguration(QJsonObject obj) = 0;

protected:
  virtual void process(qint32 session, quint32 frameid, cvMatPtr image) = 0;

  qint32 mSession;
  QMutex mProcessQueueMutex;
  NetworkConnection::File mFileProcessQueue;
  cvMatPtr mMatProcessQueue;
  QImage mQImageProcessQueue;
  quint32 mMatQueuePosition = 0;
  quint32 mFileQueuePosition = 0;
  quint32 mQImageQueuePosition = 0;
  bool mEmitMetadata = false;
  bool mEmitJPEG = false;
  bool mEmitQImage = false;
  bool mTriggeredA = false;
  bool mTriggeredB = false;
  bool mTriggeredC = false;
  bool mDebugMode = false;
  bool mAllowAllSources = false;
  int mSkippedImages = 0;
  bool mLogTime = false;
  QMap<quint32, qint64> processingstarted;
  QMap<quint32, qint64> processingfinished;
  QElapsedTimer* mUptime;
  bool mRunning;
};

}

#endif // IMAGEPROCESSER_H
