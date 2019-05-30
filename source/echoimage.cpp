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
EchoImage::EchoImage(qint32 session) {
  mRunning = true;
  mSession = session;
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
    processingfinished.insert(frameid, mUptime->nsecsElapsed());
  }
  if (mEmitJPEG) {
    cv::Mat dstImage;
    cvtColor(*image, dstImage, cv::COLOR_BGR2RGB);
    emit sendFile(mSession, NetworkConnection::File(
                    (mEmitMetadata ?
                       NetworkConnection::FileType::IMAGE_WITH_METADATA :
                       NetworkConnection::FileType::IMAGE),
                    frameid, jpegFromMat(dstImage, mEmitMetadata, 0)));
  }
  if (mEmitQImage) {
    emit sendQImage(mSession, frameid,
                    QImagePtr(new QImage(qImageFromMat(*image))));
  }
}

}
