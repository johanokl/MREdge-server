/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "mrserver.h"
#include "cannyfilter.h"
#include "imagewriter.h"
#include "mockclient.h"
#include "networkconnection.h"
#include "tcpconnection.h"
#include "udpbuilder.h"
#include "orbslamprocesser.h"
#include "udpconnection.h"
#include "udpsender.h"
#include "videoreceiver.h"
#include "videotransmitter.h"

#include <QDataStream>
#include <QNetworkInterface>
#include <QThread>
#include <QSurfaceFormat>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>

namespace MREdge {

/**
 * @brief The Session class
 * Contains pointers to the session's instances.
 */
class Session {
public:
  Session()
    : imageprocesser(nullptr),
      videoreceiver(nullptr),
      videotransmitter(nullptr) {}
  ~Session() {
    if (imageprocesser) {
      imageprocesser->deleteLater();
      imageprocesser = nullptr;
    }
    if (videoreceiver) {
      videoreceiver->deleteLater();
      videoreceiver = nullptr;
    }
    if (videotransmitter) {
      videotransmitter->deleteLater();
      videotransmitter = nullptr;
    }
  }
  ImageProcesser *imageprocesser;
  VideoReceiver *videoreceiver;
  VideoTransmitter *videotransmitter;
  qint32 sessionID;
};

/**
 * @brief MRServer::MRServer
 * @param tcpPort TCP port to listen to.
 * @param udpPort UDP port to listen to.
 */
MRServer::MRServer(quint16 tcpPort, quint16 udpPort, QString vocLoc, bool benchmarking)
{
  // Set up the default OpenGL surface format.
  QSurfaceFormat format;
  format.setDepthBufferSize(32);
  format.setSamples(8);
  QSurfaceFormat::setDefaultFormat(format);
  mFileWriter = nullptr;
  mBenchmarking = benchmarking;
  mDisplayResult = false;
  mReplaceVideoFeed = false;
  mCvFramework = ORB_SLAM2;

  mTcpCon = new TcpConnection(this, tcpPort);
  mUdpCon = new UdpConnection(this, udpPort);
  QObject::connect(this, &MRServer::sendFile,
                   mUdpCon, &NetworkConnection::sendFile);
  QObject::connect(this, &MRServer::sendFile,
                   mTcpCon, &NetworkConnection::sendFile);

  QObject::connect(mTcpCon, &NetworkConnection::fileReady,
                   this, &MRServer::dataReceived);
  QObject::connect(mTcpCon, &NetworkConnection::newSession,
                   this, &MRServer::newSession,
                   Qt::DirectConnection);
  QObject::connect(mTcpCon, &NetworkConnection::sessionDestroyed,
                   this, &MRServer::removeSession);

  qDebug() << " +------------------------------------";
  qDebug() << " |               MREdge";
  if (mBenchmarking) {
    qDebug() << " |     Benchmarking mode enabled.";
  }
  qDebug() << " | ";
  foreach (const auto &netInterface, QNetworkInterface::allInterfaces()) {
    QNetworkInterface::InterfaceFlags flags = netInterface.flags();
    if ((flags & QNetworkInterface::IsRunning) &&
        !(flags & QNetworkInterface::IsLoopBack)) {
      foreach (const auto &address, netInterface.addressEntries()) {
        if (address.ip().protocol() == QAbstractSocket::IPv4Protocol) {
          qDebug() << " |      IP: " << address.ip().toString().toLatin1().data();
        }
      }
    }
  }
  qDebug() << " |      Port (TCP): " << mTcpCon->getPort();
  qDebug() << " |      Port (UDP): " << mUdpCon->getPort();
  qDebug() << " +------------------------------------";

  mpVocabulary = nullptr;

  fDebug << "Loading ORB-SLAM2's BoW vocabulary...";

  mpVocabulary = new ORB_SLAM2::ORBVocabulary();
  QFileInfo userVocabularyFile(vocLoc);
  QFileInfo textVocabularyFile("ORBvoc.txt");
  QFileInfo homeDirTextVocabularyFile(QDir::homePath() + QDir::separator() + "ORBvoc.txt");
  if (userVocabularyFile.exists() && !vocLoc.isNull()) {
    fDebug << "Loading text file from " << userVocabularyFile.absoluteFilePath();
    mpVocabulary->loadFromTextFile(userVocabularyFile.absoluteFilePath().toStdString());
  } else if (textVocabularyFile.exists()) {
    fDebug << "Loading text file from " << textVocabularyFile.absoluteFilePath();
    mpVocabulary->loadFromTextFile(textVocabularyFile.absoluteFilePath().toStdString());
  } else if (homeDirTextVocabularyFile.exists()) {
    fDebug << "Loading text file from " << homeDirTextVocabularyFile.absoluteFilePath();
    mpVocabulary->loadFromTextFile(homeDirTextVocabularyFile.absoluteFilePath().toStdString());
  }
  if (mpVocabulary->empty()) {
    fDebug << "Error: Could not load vocabulary.";
    fDebug << "Place ORBvoc.txt in executable's directory.";
    fDebug << "ORBvoc.txt can be found in /externals/ORB_SLAM2/Vocabulary/ORBvoc.txt.tar.gz";
  } else {
    fDebug << "Vocabulary loaded.";
  }
  if (mTcpCon->getPort()) {
    fDebug << "Waiting for clients to connect.";
  }
}

/**
 * @brief MRServer::addFilewriter
 */
void MRServer::addFilewriter(QString path)
{
  if (mFileWriter) {
    return;
  }
  mFileWriter = new ImageWriter(path);
  if (!mFileWriter) {
    return;
  }
  mFileWriter->setEnabled(true);
  mFileWriter->moveToThread(new QThread());
  mFileWriter->thread()->start();
  QObject::connect(mUdpCon, &NetworkConnection::fileReady,
                   mFileWriter, &ImageWriter::writeImage);
  QObject::connect(mTcpCon, &NetworkConnection::fileReady,
                   mFileWriter, &ImageWriter::writeImage);
}

/**
 * @brief MRServer::newImage
 * @param image Image to be displayed.
 *
 * Display the result image in a Qt GUI window.
 */
void MRServer::displayImage(qint32 session, QImage image)
{
  Q_UNUSED(image);
  Q_UNUSED(session);
#ifdef ENABLE_WIDGET_SUPPORT
  if (mWindows.contains(session)) {
    bool resizeToFit = false;
    auto pixmap = mMockClients.isEmpty() ?
          QPixmap::fromImage(image) :
          QPixmap::fromImage(image.rgbSwapped());
    auto window = mWindows.value(session);
    if (resizeToFit) {
      pixmap = pixmap.scaled(
            window->width(), window->height(),
            Qt::KeepAspectRatio);
    }
    window->setPixmap(pixmap);
  }
#endif
}

/**
 * @brief MRServer::displayImage
 * @param image Image to be displayed.
 * Wrapper for displayImage(qint32 session, QImage image)
 */
void MRServer::displayImagePtr(qint32 session, QImagePtr image) {
  Q_UNUSED(image);
  Q_UNUSED(session);
#ifdef ENABLE_WIDGET_SUPPORT
  displayImage(session, *image);
#endif
}

#ifdef ENABLE_WIDGET_SUPPORT
void ClickableLabel::mousePressEvent(QMouseEvent *event) {
  Q_UNUSED(event);
  emit clicked();
}
#endif

/**
 * @brief MRServer::addMockClient
 * @param useCamera
 */
void MRServer::addMockClient(bool useCamera, QString path)
{
  auto mockClient = new MockClient();
  if (!mockClient) {
    return;
  }
#ifdef ENABLE_WIDGET_SUPPORT
  auto label = new ClickableLabel;
  label->show();
  label->resize(640, 480);
  connect(label, &ClickableLabel::clicked, [=]() {
    this->dataReceived(mockClient->getSession(), NetworkConnection::File(
                         NetworkConnection::FileType::TRIGGER_A, 1, nullptr));
  });
  mWindows.insert(mockClient->getSession(), label);
  mDisplayResult = true;
#endif
  mMockClients.append(mockClient);
  mockClient->moveToThread(new QThread());
  mockClient->thread()->start();
  QObject::connect(mockClient, &NetworkConnection::newSession,
                   this, &MRServer::newSession);
  QObject::connect(mockClient, &NetworkConnection::fileReady,
                   this, &MRServer::dataReceived);
  mockClient->start(100, 100, true, useCamera, path, true);
}

/**
 * @brief MRServer::forceVideoInputFromCamera
 * @param path
 */
void MRServer::forceVideoInputFromCamera(QString path)
{
  auto mockClient = new MockClient();
  if (mockClient) {
    mMockClients.append(mockClient);
    mockClient->moveToThread(new QThread());
    mockClient->thread()->start();
    mockClient->start(0, 0, true, true, path, false);
    mReplaceVideoFeed = true;
  }
}

/**
 * @brief MRServer::~MRServer
 */
MRServer::~MRServer()
{
  qDeleteAll(sessions);
  qDeleteAll(mMockClients);
  mTcpCon->deleteLater();
  mUdpCon->deleteLater();
  if (mFileWriter) {
    mFileWriter->deleteLater();
  }
#ifdef ENABLE_WIDGET_SUPPORT
  qDeleteAll(mWindows);
#endif
  delete mpVocabulary;
}

/**
 * @brief MRServer::dataReceived
 * @param sessionId Session id
 * @param file NetworkConnection::File
 *
 * The TCP or UDP server has received a non-image file such as
 * a JSON configuration or direct commands.
 */
void MRServer::dataReceived(qint32 sessionId, NetworkConnection::File file)
{
  fDebug << QString("Data received: (session=%1, type=%2, length=%3)")
            .arg(sessionId).arg(file.type).arg(file.data.isNull() ? 0 : file.data->length());
  mSessionsListmutex.lock();
  Session *session = sessions.value(sessionId);
  mSessionsListmutex.unlock();
  if (session == nullptr) {
    fDebug << sessionId << ": No session found";
    return;
  }
  switch (file.type) {
  case NetworkConnection::FileType::JSON: {
    fDebug << sessionId << ": JSON: " << file.data->constData();
    auto jsonObject = QJsonDocument::fromJson(file.data->constData()).object();
    if (jsonObject.contains("TransportProtocol")) {
      bool useTcp = (jsonObject["TransportProtocol"].toString() == "TCP");
      bool useUdp = (jsonObject["TransportProtocol"].toString() == "UDP");
      fDebug << sessionId << ": TransportProtocol: " << jsonObject["TransportProtocol"].toString();
      mTcpCon->setSendImagesForSession(sessionId, useTcp);
      mUdpCon->setSendImagesForSession(sessionId, useUdp);
    }
    if (jsonObject.contains("JpegStream")) {
      bool sendmjpeg = jsonObject["JpegStream"].toBool();
      fDebug << sessionId << ": JpegStream: " << sendmjpeg;
      if (session->imageprocesser) {
        session->imageprocesser->setEmitJPEG(sendmjpeg);
        session->imageprocesser->setEmitQImage(!sendmjpeg);
      }
    }
    if (jsonObject.contains("VideoBitrate")) {
      int bitrate = jsonObject["VideoBitrate"].toInt();
      fDebug << sessionId << ": VideoBitrate: " << bitrate;
      if (session->videotransmitter) {
        session->videotransmitter->setBitrate(bitrate);
      }
    }
    if (jsonObject.contains("UserInteractionConfiguration")) {
      fDebug << sessionId << ": UserInteractionConfiguration received";
      if (session->imageprocesser) {
        session->imageprocesser->setUserInteractionConfiguration(jsonObject);
      }
    }
    if (jsonObject.contains("DebugMode")) {
      bool debugmode = jsonObject["DebugMode"].toBool();
      fDebug << sessionId << ": DebugMode: " << debugmode;
      if (session->imageprocesser) {
        session->imageprocesser->setDebugMode(debugmode);
      }
    }
    if (jsonObject.contains("PacketSize")) {
      qint32 packetSize = jsonObject["PacketSize"].toInt();
      fDebug << sessionId << ": PacketSize: " << packetSize;
      mUdpCon->setPacketSize(sessionId, packetSize);
    }
    if (jsonObject.contains("VideoReceiverPort") && jsonObject.contains("VideoReceiverFormat")) {
      auto destPort = static_cast<quint16>(jsonObject["VideoReceiverPort"].toInt());
      auto destFormatStr = jsonObject["VideoReceiverFormat"].toString();
      fDebug << sessionId << ": VideoReceiverPort: " << destPort;
      fDebug << sessionId << ": VideoReceiverFormat: " << destFormatStr;
      VideoStreamer::Format format = VideoStreamer::Format::H264_UDP;
      if (destFormatStr == "H264_UDP") {
        format = VideoStreamer::Format::H264_UDP;
      } else if (destFormatStr == "H264_TCP") {
        format = VideoStreamer::Format::H264_TCP;
      } else {
        destPort = 0;
      }
      if (destPort > 0) {
        session->videotransmitter->startStream(format, destPort);
      }
    }
    if (jsonObject.contains("VideoTransmitterFormat")) {
      auto format = VideoStreamer::Format::H264_UDP;
      auto srcFormatStr = jsonObject["VideoTransmitterFormat"].toString();
      fDebug << sessionId << ": VideoTransmitterFormat: " << srcFormatStr;
      if (srcFormatStr == "H264_UDP") {
        format = VideoStreamer::Format::H264_UDP;
      } else if (srcFormatStr == "H264_TCP") {
        format = VideoStreamer::Format::H264_TCP;
      }
      bool useJitterbuffer = false;
      if (jsonObject.contains("VideoTransmitterUseJitterBuffer")) {
        useJitterbuffer = jsonObject.value("VideoTransmitterUseJitterBuffer").toBool();
        fDebug << sessionId << ": VideoTransmitterUseJitterBuffer: " << useJitterbuffer;
      }
      if (session->videoreceiver) {
        session->videoreceiver->start(format, useJitterbuffer);
      }
    }
    if (jsonObject.contains("Camera.width") &&
        jsonObject.contains("Camera.height")) {
      int width = jsonObject["Camera.width"].toInt();
      int height = jsonObject["Camera.height"].toInt();
      fDebug << sessionId << ": Camera.width: " << width;
      fDebug << sessionId << ": Camera.height: " << height;
      if (jsonObject.contains("Camera.fx")) {
        fDebug << sessionId << ": Camera.fx: " << jsonObject["Camera.fx"].toDouble();
      }
      if (jsonObject.contains("Camera.fy")) {
        fDebug << sessionId << ": Camera.fy: " << jsonObject["Camera.fy"].toDouble();
      }
      if (jsonObject.contains("Camera.cx")) {
        fDebug << sessionId << ": Camera.cx: " << jsonObject["Camera.cx"].toDouble();
      }
      if (jsonObject.contains("Camera.cy")) {
        fDebug << sessionId << ": Camera.cy: " << jsonObject["Camera.cy"].toDouble();
      }
      if (jsonObject.contains("Camera.k1")) {
        fDebug << sessionId << ": Camera.k1: " << jsonObject["Camera.k1"].toDouble();
      }
      if (jsonObject.contains("Camera.k2")) {
        fDebug << sessionId << ": Camera.k2: " << jsonObject["Camera.k2"].toDouble();
      }
      if (jsonObject.contains("Camera.p1")) {
        fDebug << sessionId << ": Camera.p1: " << jsonObject["Camera.p1"].toDouble();
      }
      if (jsonObject.contains("Camera.p2")) {
        fDebug << sessionId << ": Camera.p2: " << jsonObject["Camera.p2"].toDouble();
      }
      if (session->imageprocesser) {
        session->imageprocesser->setConfig(jsonObject);
        session->videotransmitter->setImageSize(QSize(width, height));
      }
    }
    break;
  }
  case NetworkConnection::FileType::TRIGGER_A:
    fDebug << sessionId << ": Trigger Action A";
    if (session->imageprocesser) {
      session->imageprocesser->triggerActionA();
    }
    break;
  case NetworkConnection::FileType::TRIGGER_B:
    fDebug << sessionId << ": Trigger Action B";
    if (session->imageprocesser) {
      session->imageprocesser->triggerActionB();
    }
    break;
  case NetworkConnection::FileType::TRIGGER_C:
    fDebug << sessionId << ": Trigger Action C";
    if (session->imageprocesser) {
      session->imageprocesser->triggerActionC();
    }
    break;
  case NetworkConnection::FileType::CALIBRATION: {
    auto calibrationMode = (file.data.data()->length() == 1 &&
                            file.data.data()->data()[0] != 0);
    fDebug << sessionId << ": Calibration mode: " << calibrationMode;
    if (session->imageprocesser) {
      session->imageprocesser->setCalibrateMode(calibrationMode);
    }
  }
  }
}

/**
 * @brief MRServer::newSession
 * @param sessionId The new client's session id.
 * @param host Client host
 * @param port Client source port.
 *
 * Set up a new session. Create and start the session specific
 * class instances and threads. When that is done, inform the client
 * about the values such as which UDP port to connect to.
 */
void MRServer::newSession(qint32 sessionId, QString host, quint16 port)
{
  fDebug << QString("MRServer::newSession: %1:%2").arg(host).arg(port);
  mSessionsListmutex.lock();
  auto session = new Session;
  session->sessionID = sessionId;
  sessions.insert(sessionId, session);
  mSessionsListmutex.unlock();

  mTcpCon->setSendImagesForSession(sessionId, true);
  mUdpCon->setSendImagesForSession(sessionId, false);

  auto videotransmitter = new VideoTransmitter(sessionId, host);
  videotransmitter->setBenchmarkingMode(mBenchmarking);
  session->videotransmitter = videotransmitter;
  videotransmitter->moveToThread(new QThread(this));
  videotransmitter->thread()->start();
  if (!mReplaceVideoFeed) {
    auto videoreceiver = new VideoReceiver(sessionId, host);
    videoreceiver->setBenchmarkingMode(mBenchmarking);
    session->videoreceiver = videoreceiver;
    videoreceiver->moveToThread(new QThread(this));
    videoreceiver->thread()->start();
    QObject::connect(videoreceiver, &VideoReceiver::ready,
                     this, &MRServer::videoReceiverReady);
  }

  ImageProcesser *imageprocesser;
  if (mCvFramework == CANNYFILTER) {
    imageprocesser = new CannyFilter(sessionId);
  } else {
    imageprocesser = new OrbSlamProcesser(sessionId, mpVocabulary, mBenchmarking);
  }

  if (mReplaceVideoFeed) {
    imageprocesser->setAllowAllSources(true);
  }

  session->imageprocesser = imageprocesser;
  imageprocesser->moveToThread(new QThread(this));
  imageprocesser->thread()->start();
  session->imageprocesser->setEmitMetadata(true);

#ifdef ENABLE_WIDGET_SUPPORT 
  if (mDisplayResult && !mWindows.contains(sessionId)) {
    auto outputdisplay = new ClickableLabel;
    outputdisplay->show();
    outputdisplay->resize(640, 480);
    connect(outputdisplay, &ClickableLabel::clicked, [=]() {
      this->dataReceived(sessionId, NetworkConnection::File(
                           NetworkConnection::FileType::TRIGGER_A, 1, nullptr));
    });
    mWindows.insert(sessionId, outputdisplay);
    if (!mWindows.contains(sessionId + 1000)) {
      auto inputdisplay = new QLabel;
      inputdisplay->show();
      inputdisplay->resize(640, 480);
      mWindows.insert(sessionId + 1000, inputdisplay);
    }
  }
#endif

  // Receive data
  NetworkConnection *currCon = mTcpCon;
  for (int i = 0; i < (2 + mMockClients.size()); i++) {
    QObject::connect(currCon, &NetworkConnection::fileReady,
                     imageprocesser, &ImageProcesser::addFileToProcessQueue,
                     Qt::DirectConnection);
    QObject::connect(currCon, &NetworkConnection::fileReady,
                     imageprocesser, &ImageProcesser::processFile);
    QObject::connect(currCon, &NetworkConnection::matReady,
                     imageprocesser, &ImageProcesser::addMatToProcessQueue,
                     Qt::DirectConnection);
    QObject::connect(currCon, &NetworkConnection::matReady,
                     imageprocesser, &ImageProcesser::processMat);
    // Send data
    QObject::connect(imageprocesser, &ImageProcesser::sendFile,
                     currCon, &NetworkConnection::sendFileIfLatest,
                     Qt::DirectConnection);
    // Add mock client if available.
    if (i == 0) {
      currCon = reinterpret_cast<NetworkConnection*>(mUdpCon);
    } else if (mMockClients.size() >= i) {
      currCon = reinterpret_cast<NetworkConnection*>(mMockClients.at(i - 1));
    }
  }
  // Video receiver
  auto videoreceiver = session->videoreceiver;
  if (videoreceiver) {
    QObject::connect(videoreceiver, &NetworkConnection::matReady,
                     imageprocesser, &ImageProcesser::addMatToProcessQueue,
                     Qt::DirectConnection);
    QObject::connect(videoreceiver, &NetworkConnection::matReady,
                     imageprocesser, &ImageProcesser::processMat);
    if (mFileWriter) {
      QObject::connect(videoreceiver, &NetworkConnection::matReady,
                       mFileWriter, &ImageWriter::writeMat);
    }
  }
  // Send data
  QObject::connect(imageprocesser, &ImageProcesser::sendQImage,
                   videotransmitter, &VideoTransmitter::addQImageToProcessQueue,
                   Qt::DirectConnection);
  QObject::connect(imageprocesser, &ImageProcesser::sendQImage,
                   videotransmitter, &VideoTransmitter::processQImage);
  if (mFileWriter) {
    QObject::connect(imageprocesser, &ImageProcesser::sendFile,
                     mFileWriter, &ImageWriter::writeImage);
  }
  if (mDisplayResult) {
    QObject::connect(imageprocesser, &ImageProcesser::sendQImage,
                     this, &MRServer::displayImagePtr);
    QObject::connect(videoreceiver, &VideoReceiver::matReady,
                     [=](qint32 session, cvMatPtr image, int) {
      this->displayImage(session + 1000, ImageProcesser::qImageFromMat(*image));
      });
  }

  QJsonObject retObject;
  retObject.insert("UdpPort", mUdpCon->getPort());
  retObject.insert("SessionId", sessionId);
  QByteArrayPtr ret(new QByteArray(QJsonDocument(retObject).toJson()));
  fDebug << sessionId << ": Connection JSON: " << ret->data();
  emit sendFile(sessionId, NetworkConnection::File(
                  NetworkConnection::FileType::JSON, 1, ret));
}

/**
 * @brief MRServer::removeSession
 * @param sessionId
 * Destroys a session with its associated class instances.
 */
void MRServer::removeSession(qint32 sessionId)
{
  fDebug << "MRServer::removeSession: id: " << sessionId;
  mSessionsListmutex.lock();
  auto session = sessions.take(sessionId);
  mSessionsListmutex.unlock();
  delete session;
}

/**
 * @brief MRServer::videoReceiverReady
 * @param sessionId Session id
 * @param format The video format the receiver has been configured to.
 * @param port Server port the video receiver listens to.
 *
 * Listener for when the video receiver has been configured and is ready.
 * The port is for the open video socket the client can connect to.
 */
void MRServer::videoReceiverReady(qint32 sessionId, VideoStreamer::Format format, quint16 port)
{
  QString formatString;
  if (format == VideoStreamer::Format::H264_UDP) {
    formatString = "H264_UDP";
  } else if (format == VideoStreamer::Format::H264_TCP) {
    formatString = "H264_TCP";
  }
  fDebug << QString("VideoReceiver Ready: session=%1, format=%2, port=%3")
            .arg(sessionId).arg(formatString).arg(port);
  QJsonObject retObject;
  retObject.insert("VideoReceiverFormat", formatString);
  retObject.insert("VideoReceiverPort", port);
  emit sendFile(sessionId, NetworkConnection::File(
                  NetworkConnection::FileType::JSON, 1,
                  QByteArrayPtr(new QByteArray(QJsonDocument(retObject).toJson()))));
}

}
