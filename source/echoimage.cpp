/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "echoimage.h"
#include <QtCore/qbuffer.h>
#include <QDebug>
#include <QImage>
#include <QtMath>
#include <QElapsedTimer>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>

namespace MREdge {

/**
 * @brief EchoImage::EchoImage
 * @param session
 */
EchoImage::EchoImage(qint32 session, bool benchmarking) {
  mRunning = true;
  mSession = session;
  mBenchmarking = benchmarking;
}

/**
 * @brief EchoImage::~EchoImage
 */
EchoImage::~EchoImage() {
  mRunning = false;
}

/**
 * @brief EchoImage::process
 * @param session Session id
 * @param image OpenCV Mat
 * @param frameid Image id.
 *
 * Return the received image.
 */
void EchoImage::process(qint32 session, quint32 frameid, cvMatPtr image)
{
  Q_UNUSED(session);
  if (mRunning && mLogTime) {
    mProcessingFinishedTimes.insert(frameid, mUptime->nsecsElapsed());
  }
  int metadata = 0;
  cv::Mat dstImage = *image;

  bool colorFrame = false;
  if (mIdentifyColorFrame) {
    colorFrame = true;
    for (int i = 0; i < 10; i++) {
      unsigned char * p = dstImage.ptr(20 * i,  20 * i);
      if (p[0] != 252 || p[1] != 0 || p[2] != 252) {
         colorFrame = false;
         break;
      }
    }
    if (colorFrame) {
      //fDebug << "Color frame found";
      metadata = 2;
      dstImage = cv::Scalar(255, 0, 255);
    }
  }
  if (mEmitJPEG) {
    cvtColor(*image, dstImage, cv::COLOR_BGR2RGB);
    emit sendFile(mSession, NetworkConnection::File(
                    (mBenchmarking ?
                       NetworkConnection::FileType::IMAGE_WITH_METADATA :
                       NetworkConnection::FileType::IMAGE),
                    frameid, jpegFromMat(dstImage, mBenchmarking, metadata)));
  }
  if (mEmitQImage) {
    emit sendQImage(mSession, frameid, qImageFromMat(dstImage));
  }
}

}
