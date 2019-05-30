/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "mockclient.h"
#include "networkconnection.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QString>
#include <QThread>
#include <QJsonObject>
#include <QJsonDocument>
#include "mrserver.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace MREdge {

MockClient::MockClient()
{
  mSessionId = static_cast<qint32>(qrand());
  QObject::connect(this, &MockClient::startSignal,
                   this, &MockClient::startThread);
}

/**
 * @brief MockClient::start
 * @param delay
 * @param interval
 * @param repeat
 * @param webcam
 * @param path
 */
void MockClient::start(unsigned long delay, unsigned long interval,
                       bool repeat, bool webcam, QString path,
                       bool sendCommands)
{
  fDebug << "Adding mock client.";
  emit startSignal(delay, interval, repeat, webcam, path, sendCommands);
}

/**
 * @brief MockClient::startThread
 * @param delay Milliseconds before sending the first image.
 * @param interval Milliseconds of waiting between the images.
 * @param repeat Start over when the last image in the directory has been sent.
 * @param webcam
 * @param path
 */
void MockClient::startThread(unsigned long delay, unsigned long interval,
                             bool repeat, bool webcam, QString path,
                             bool sendCommands)
{
  mRun = true;
  if (sendCommands) {
    emit newSession(getSession(), getHost(), getPort());

    QJsonObject retObject;
    retObject.insert("JpegStream", false);
    retObject.insert("TransportProtocol", "NULL");
    retObject.insert("Camera.width", 640);
    retObject.insert("Camera.height", 480);
    emit fileReady(getSession(), NetworkConnection::File(
                     NetworkConnection::FileType::JSON, 1,
                     QByteArrayPtr(new QByteArray(QJsonDocument(retObject).toJson()))));
  }
  quint32 imagecounter = 0;

  if (webcam) {
    fDebug << "Using Webcam as source";
    cv::VideoCapture cap(path.toInt());
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    if (!cap.isOpened()) {
      fDebug << "Webcam could not be opened";
      return;
    }
    while (repeat && mRun) {
      fDebug << "Image id: " << imagecounter;
      imagecounter++;
      cv::Mat frame;
      cap >> frame; // get a new frame from camera
      emit matReady(getSession(), imagecounter, cvMatPtr(new cv::Mat(frame)));
      //if ((imagecounter % 500) == 100) {
      //  fDebug << "Trigger B";
      //  emit fileReady(getSession(), NetworkConnection::File(
      //                   NetworkConnection::FileType::TRIGGER_B, 1, nullptr));
      //}
      //if ((imagecounter % 500) == 130) {
      //  fDebug << "Trigger A";
      //  emit fileReady(getSession(), NetworkConnection::File(
      //                   NetworkConnection::FileType::TRIGGER_A, 1, nullptr));
      //}
    }
  } else {
    do {
      if (path.isEmpty()) {
        path = "Mock";
      }
      QDir directory(path);
      fDebug << "Using images in " << directory.absolutePath();
      if (delay && this->thread()) {
        thread()->msleep(delay);
      }
      QStringList files = directory.entryList(
            QStringList() << "*.jpg" << "*.JPG",
            QDir::Files, QDir::Name);
      if (files.empty()) {
        fDebug << "No mock files in directory " << path;
        return;
      }
      files.sort();
      foreach (auto filename, files) {
        imagecounter++;
        QString fullpath = path + QDir::separator() + filename;
        fDebug << "Mock input: " << fullpath.toLatin1();
        QFile file(fullpath);
        if (file.open(QIODevice::ReadOnly)) {
          QByteArrayPtr data(new QByteArray(file.readAll()));
          emit fileReady(getSession(), NetworkConnection::File(
                           NetworkConnection::FileType::IMAGE, imagecounter, data));
        }
        if (interval && this->thread()) {
          thread()->msleep(interval);
        }
      }
    } while (repeat && mRun);
  }
}

}
