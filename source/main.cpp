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
  qRegisterMetaType<QImagePtr>("QImagePtr");
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

  bool benchmarking = false;

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

  QCommandLineOption benchmarkingOption(
        QStringList() << "b" << "benchmarking",
        "Fill the screen with solid colors when point cloud or AR objects are visible.");
  parser.addOption(benchmarkingOption);

  QCommandLineOption replaceVideoInOption(
        QStringList() << "r" << "replaceinput",
        "Replace video feeds sent from the end devices with video " \
        "retrieved from the computer's camera with id number <id> [0, 1, ..].",
        "id");
  parser.addOption(replaceVideoInOption);

  parser.process(app);

  if (parser.isSet(tcpOption)) {
    tcpPort = parser.value(tcpOption).toUShort();
  }
  if (parser.isSet(udpOption)) {
    udpPort = parser.value(udpOption).toUShort();
  }
  if (parser.isSet(benchmarkingOption)) {
    benchmarking = true;
  }

  QString vocPath = "";
  if (parser.isSet(vocOption)) {
    vocPath = parser.value(vocOption);
  }

  bool displayOptionEnabled = parser.isSet(displayOption);
#ifndef ENABLE_WIDGET_SUPPORT
  if (displayOptionEnabled) {
    qDebug() << "*** MREdgeServer has not been compiled with widget support and can't support parameter --display";
    qDebug() << "*** Enable DISPLAY_MOCK_DATA in MREdgeServer.pro and rebuild.";
  }
#endif

  MRServer myServer(tcpPort, udpPort, vocPath, benchmarking);

    if (displayOptionEnabled) {
      myServer.setDisplayResults(true);
    }

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
  if (parser.isSet(displayOption)) {
    myServer.setDisplayResults(true);
  }
  if (parser.isSet(cannyFilterOption)) {
    myServer.setMixedRealityFramework(MRServer::CANNYFILTER);
  }

  return app.exec();
}
