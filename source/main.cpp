/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "global.h"
#include "mrserver.h"
#include "networkconnection.h"
#include "videotransmitter.h"
#include "videoreceiver.h"
#include <QVector3D>
#include <QMatrix4x4>
#include <QCommandLineParser>
#ifdef ENABLE_WIDGET_SUPPORT
#include <QApplication>
#else
#include <QGuiApplication>
#endif

using namespace MREdge;

Q_DECLARE_METATYPE(QByteArrayPtr)
Q_DECLARE_METATYPE(NetworkConnection::File)
Q_DECLARE_METATYPE(VideoStreamer::Format)

int main(int argc, char *argv[])
{
  qRegisterMetaType<QByteArrayPtr>("QByteArrayPtr");
  qRegisterMetaType<QList<QVector3D>>("QList<QVector3D>");
  qRegisterMetaType<QMatrix4x4>("QMatrix4x4");
  qRegisterMetaType<cvMatPtr>("cvMatPtr");
  qRegisterMetaType<QImage>("QImage");
  qRegisterMetaType<NetworkConnection::File>("File");
  qRegisterMetaType<VideoStreamer::Format>("Format");

#ifdef ENABLE_WIDGET_SUPPORT
  QApplication app(argc, argv);
#else
  QGuiApplication app(argc, argv);
#endif
  QCoreApplication::setApplicationVersion("0.1");
  QCoreApplication::setApplicationName("MREdge");
  QCoreApplication::setOrganizationName("Johan Lindqvist");

  QCommandLineParser parser;
  parser.setApplicationDescription("Mixed Reality using Edge Computing");
  parser.addHelpOption();
  parser.addVersionOption();

  quint16 tcpPort = 39200;
  quint16 udpPort = 39585;

  QCommandLineOption tcpOption(
        QStringList() << "t" << "tcp",
        QString("TCP port number. Default is %1.")
        .arg(tcpPort),
        "port");
  parser.addOption(tcpOption);

  QCommandLineOption udpOption(
        QStringList() << "u" << "udp",
        QString("UDP port number. Default is %1.")
        .arg(udpPort),
        "port");
  parser.addOption(udpOption);

  QCommandLineOption mockOption(
        QStringList() << "m" << "mock",
        "Add mock client that uses images in <directory>.",
        "directory");
  parser.addOption(mockOption);

  QCommandLineOption camOption(
        QStringList() << "c" << "camera",
        "Add mock client that sends video " \
        "retrieved from the computer's camera with id number <id> [0, 1, ..].",
        "id");
  parser.addOption(camOption);

  QCommandLineOption vocOption(
        QStringList() << "o" << "o",
        "Specify a specific ORBvoc.txt. If not specified it's " \
        "assumed the file's in the current directory.",
        "path");
  parser.addOption(vocOption);

  QCommandLineOption writerOption(
        QStringList() << "w" << "write",
        "Write result images to <directory>.",
        "directory");
  parser.addOption(writerOption);

  QCommandLineOption displayOption(
        QStringList() << "d" << "display",
        "Displays the input and output as GUI windows.");
  parser.addOption(displayOption);

  QCommandLineOption cannyFilterOption(
        QStringList() << "f" << "cannyfilter",
        "Test the video stack using only the canny filter.");
  parser.addOption(cannyFilterOption);

  QCommandLineOption echoImageOption(
        QStringList() << "e" << "echoimage",
        "Don't modify the received image, just return it.");
  parser.addOption(echoImageOption);

  QCommandLineOption benchmarkingOption(
        QStringList() << "b" << "benchmarking",
        "Fill the screen with a solid color when point cloud or MR objects are visible.");
  parser.addOption(benchmarkingOption);

  QCommandLineOption identifyColorFrameOption(
        QStringList() << "i" << "identifycolor",
        "Look for pictures filled with the data color #F0F and act on it. Used for benchmarking.");
  parser.addOption(identifyColorFrameOption);

  QCommandLineOption replaceVideoInOption(
        QStringList() << "r" << "replaceinput",
        "Replace video feeds sent from the end devices with video " \
        "retrieved from the computer's camera with id number <id> [0, 1, ..].",
        "id");
  parser.addOption(replaceVideoInOption);

  QCommandLineOption logTimeOption(
        QStringList() << "l" << "logtime",
        "Log the time and print results after 60 seconds.");
  parser.addOption(logTimeOption);

  QCommandLineOption disableLoopClosingOption(
        QStringList() << "n" << "loopclosing",
        "Disable loop closing in ORB-SLAM2.");
  parser.addOption(disableLoopClosingOption);

  QCommandLineOption jpegQualityOption(
        QStringList() << "j" << "jpegquality",
        "Set the MJPEG image quality level. Default is 80.",
        "level");
  parser.addOption(jpegQualityOption);

  QCommandLineOption vocPoolSizeOption(
        QStringList() << "p" << "pool",
        "Set the <size> [0, 1, 2, ..] of the pool of vocabulary files at system startup. " \
        "Default is 1.",
        "size");
  parser.addOption(vocPoolSizeOption);


  parser.process(app);

  if (parser.isSet(tcpOption)) {
    tcpPort = parser.value(tcpOption).toUShort();
  }
  if (parser.isSet(udpOption)) {
    udpPort = parser.value(udpOption).toUShort();
  }
  QString vocPath = "";
  if (parser.isSet(vocOption)) {
    vocPath = parser.value(vocOption);
  }

  int vocPoolSize = 1;
  if (parser.isSet(vocPoolSizeOption)) {
    vocPoolSize = parser.value(vocPoolSizeOption).toInt();
  }

  bool benchmarkingOptionEnabled = parser.isSet(benchmarkingOption);

#ifndef ENABLE_WIDGET_SUPPORT
  if (displayOptionEnabled) {
    qDebug() << "*** MREdgeServer has not been compiled with widget support and can't support parameter --display";
    qDebug() << "*** Enable DISPLAY_MOCK_DATA in MREdgeServer.pro and rebuild.";
  }
#endif

  MRServer myServer;

  myServer.setBenchmarkingMode(benchmarkingOptionEnabled);
  myServer.setLogTime(parser.isSet(logTimeOption));
  myServer.setDisplayResults(parser.isSet(displayOption));
  myServer.setIdentifyColorFrame(parser.isSet(identifyColorFrameOption));
  if (parser.isSet(jpegQualityOption)) {
    myServer.setJpegQualityLevel(parser.value(jpegQualityOption).toInt());
  }

  if (parser.isSet(cannyFilterOption)) {
    fDebug << "Using CANNY FILTER";
    myServer.setMixedRealityFramework(MRServer::CANNYFILTER);
  } else if (parser.isSet(echoImageOption)) {
    fDebug << "Using ECHO IMAGE";
    myServer.setMixedRealityFramework(MRServer::ECHOIMAGE);
  } else if (parser.isSet(disableLoopClosingOption)) {
    myServer.setMixedRealityFramework(MRServer::ORB_SLAM2_NO_LC);
    myServer.loadVoc(vocPath, vocPoolSize);
  } else {
    myServer.setMixedRealityFramework(MRServer::ORB_SLAM2);
    myServer.loadVoc(vocPath, vocPoolSize);
  }

  myServer.startServer(tcpPort, udpPort);

  if (parser.isSet(writerOption)) {
    myServer.addFilewriter(parser.value(writerOption));
  }
  if (parser.isSet(mockOption)) {
    myServer.addMockClient(false, parser.value(mockOption));
  }
  if (parser.isSet(camOption)) {
    myServer.addMockClient(true, parser.value(camOption));
    fDebug << "Adding mock client";
  }
  if (parser.isSet(replaceVideoInOption)) {
    myServer.forceVideoInputFromCamera(parser.value(replaceVideoInOption));
    fDebug << "Replacing end device's input with feed from camera "
           << parser.value(replaceVideoInOption);
  }

  return app.exec();
}
