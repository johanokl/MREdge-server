/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef UDPServer_H
#define UDPServer_H

#include "global.h"
#include "networkconnection.h"
#include <QMutex>
#include <QUdpSocket>
#include <QImage>
#include <QJsonObject>
#include "ORBVocabulary.h"
#include "videoreceiver.h"
#ifdef ENABLE_WIDGET_SUPPORT
#include <QLabel>
#endif

class QLabel;
class QElapsedTimer;

namespace MREdge {

class UdpSender;
class ImageWriter;
class MockClient;
class ImageProcesser;
class VideoReceiver;
class VideoTransmitter;
class Session;
class TcpConnection;
class UdpConnection;

/**
 * @brief The MRServer class
 *
 * The overarching class for the server.
 */
class MRServer : public QObject {
  Q_OBJECT

public:
  enum CV_FRAMEWORKS {
    CANNYFILTER=0,
    ECHOIMAGE=1,
    ORB_SLAM2=2,
    ORB_SLAM2_NO_LC=3
  };

  MRServer();
  ~MRServer();
  void addMockClient(bool useWebcam, QString path="");
  void addFilewriter(QString path="");
  void forceVideoInputFromCamera(QString path);

signals:
  void startMock(unsigned long delay, unsigned long interval, bool repeat,
                 bool webcam, QString path, bool sendCommands);
  void sendFile(qint32 session, NetworkConnection::File file);

public slots:
  void dataReceived(qint32 session, NetworkConnection::File file);
  void displayImage(qint32 session, quint32 frameid, QImage image);
  void newSession(qint32 session, QString host, quint16 port);
  void removeSession(qint32 session);
  void videoReceiverReady(qint32 session, VideoStreamer::Format format, quint16 port);
  void setDisplayResults(bool enable) { mDisplayResult = enable; }
  void setBenchmarkingMode(bool enable);
  void setLogTime(bool enable) { mLogTime = enable; }
  void setIdentifyColorFrame(bool enable) { mIdentifyColorFrame = enable; }
  void startServer(quint16 tcpPort, quint16 udpPort);
  void loadVoc(QString path, int poolSize);
  void setMixedRealityFramework(CV_FRAMEWORKS framework) {
    mCvFramework = framework; }

private:
  ORB_SLAM2::ORBVocabulary * getVocabulary();
  CV_FRAMEWORKS mCvFramework;
  TcpConnection *mTcpCon;
  UdpConnection *mUdpCon;
  ImageWriter *mFileWriter;
  QList<MockClient *> mMockClients;
  QMap<qint32, Session *> sessions;
  QMutex mSessionsListmutex;
  QMap<ORB_SLAM2::ORBVocabulary *, bool> mVocabularyPool;
  QString mVocabularyPath;
  QMap<qint32, QLabel *> mWindows;
  bool mDisplayResult;
  bool mBenchmarking;
  bool mReplaceVideoFeed;
  bool mLogTime;
  QElapsedTimer* mUptime;
  bool mIdentifyColorFrame;
};

/**
 * @brief Class for a clickable QLabel to display images.
 */
class ClickableLabel
#ifdef ENABLE_WIDGET_SUPPORT
    : public QLabel
#else
    : public QObject
#endif
{
  Q_OBJECT
public:
#ifdef ENABLE_WIDGET_SUPPORT
  ClickableLabel(QWidget *p=0)
    : QLabel(p) {}
  virtual ~ClickableLabel() {}
#endif
signals:
  void clicked();
#ifdef ENABLE_WIDGET_SUPPORT
protected:
  void mousePressEvent(QMouseEvent *event);
#endif
};


}

#endif // UDPServer_H
