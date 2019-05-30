/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include <QDebug>
#include <QImage>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <QtCore/QBuffer>
#include <QElapsedTimer>
#include "imageprocesser.h"
#include <mutex>

namespace MREdge {

ImageProcesser::ImageProcesser()
{
  mUptime = new QElapsedTimer;
  mUptime->start();
}

/**
 * @brief ImageProcesser::matFromQImage
 * @param img QImage
 * @return OpenCV Mat image
 *
 * Convert the image. Supports 24 bit RGB and 32 bit RGBA.
 */
cv::Mat ImageProcesser::matFromQImage(QImage img)
{
  if (img.format() == QImage::Format_RGB32) {
    return cv::Mat(img.height(), img.width(), CV_8UC4, img.bits(),
                   static_cast<size_t>(img.bytesPerLine())).clone();
  }
  return cv::Mat(img.height(), img.width(), CV_8UC3, img.bits(),
                 static_cast<size_t>(img.bytesPerLine())).clone();
}

/**
 * @brief ImageProcesser::qImageFromMat
 * @param mat OpenCV Mat image
 * @return QImage
 *
 * Converts the Mat image to 24 bit RGB QImage.
 */
QImage ImageProcesser::qImageFromMat(cv::Mat mat)
{
  return QImage(static_cast<uchar *>(mat.data), mat.cols, mat.rows,
                static_cast<int>(mat.step), QImage::Format_RGB888);
}

/**
 * @brief ImageProcesser::jpegFromQImage
 * @param image The image to convert.
 * @return Byte array with JPEG file.
 *
 * Converts the QImage to a JPEG byte array with header.
 */
QByteArrayPtr ImageProcesser::jpegFromQImage(
    QImage image, bool addmetadata, int metadata)
{
  QByteArrayPtr retarray(new QByteArray());
  if (!image.isNull()) {
    QBuffer qBuffer(retarray.data());
    qBuffer.open(QIODevice::WriteOnly);
    image.save(&qBuffer, "JPG", 85);
  }
  if (addmetadata) {
    retarray->append(4, reinterpret_cast<unsigned char>(static_cast<quint8>(metadata)));
  }
  return retarray;
}

/**
 * @brief ImageProcesser::jpegFromMat
 * @param image The image to convert
 * @return Byte array with JPEG file.
 *
 * Converts the OpenCV Mat to a JPEG byte array with header.
 */
QByteArrayPtr ImageProcesser::jpegFromMat(
    cv::Mat mat, bool addmetadata, int metadata)
{
  std::vector<uchar> buffer;
  std::vector<int> param(2);
  param[0] = cv::IMWRITE_JPEG_QUALITY;
  param[1] = 85; // 0-100
  cv::imencode(".jpg", mat, buffer, param);
  auto retarray = QByteArrayPtr(
        new QByteArray(
          reinterpret_cast<const char *>(buffer.data()),
          static_cast<int>(buffer.size())));
  if (addmetadata) {
    retarray->append(4, static_cast<char>(metadata));
  }
  return retarray;
}

/**
 * @brief ImageProcesser::addFileToProcessQueue
 * @param session Session id
 * @param file Image to be added.
 *
 * Add a JPEG file to the process queue.
 */
void ImageProcesser::addFileToProcessQueue(qint32 session, NetworkConnection::File file)
{
  if (mAllowAllSources || session == mSession) {
    mProcessQueueMutex.lock();
    mFileProcessQueue = file;
    mFileQueuePosition = file.id;
    mProcessQueueMutex.unlock();
  }
}

/**
 * @brief ImageProcesser::addQImageToProcessQueue
 * @param session Session id
 * @param image Image to be added.
 * @param frameid Image's id number.
 *
 * Add a QImage to the process queue.
 */
void ImageProcesser::addQImageToProcessQueue(qint32 session, quint32 frameid, QImage image)
{
  if (mAllowAllSources || session == mSession) {
    mProcessQueueMutex.lock();
    mQImageProcessQueue = image;
    mQImageQueuePosition = frameid;
    mProcessQueueMutex.unlock();
  }
}

/**
 * @brief ImageProcesser::addQImageToProcessQueue
 * @param session Session id
 * @param image Image to be added.
 * @param frameid Image's id number.
 *
 * Add an OpenCV Mat to the process queue.
 */
void ImageProcesser::addMatToProcessQueue(qint32 session, quint32 frameid, cvMatPtr image)
{
  if (mAllowAllSources || session == mSession) {
    mProcessQueueMutex.lock();
    mMatProcessQueue = image;
    mMatQueuePosition = frameid;
    mProcessQueueMutex.unlock();
  }
}

/**
 * @brief ImageProcesser::processFile
 * @param session
 * @param image Replaced if more recent image exists in process queue.
 *
 * Process the latest image in the JPEG file process queue.
 */
void ImageProcesser::processFile(qint32 session, NetworkConnection::File file)
{
  if ((!mAllowAllSources && session != mSession)  ||
      file.type != NetworkConnection::FileType::IMAGE) {
    return;
  }
  if (file.id <= mFileQueuePosition) {
    mProcessQueueMutex.lock();
    file = mFileProcessQueue;
    mFileProcessQueue = NetworkConnection::File(NetworkConnection::FileType::NONE, 0, nullptr);
    mMatProcessQueue = nullptr;
    mProcessQueueMutex.unlock();
  }
  if (file.data.isNull()) {
    fDebug << "Image " << file.id << " is null";
    return;
  }
  process(mSession, file.id, cvMatPtr(new cv::Mat(matFromQImage(QImage::fromData(*file.data, "JPEG")))));
}

/**
 * @brief ImageProcesser::processQImage
 * @param session Session id
 * @param image QImage replaced if more recent image exists in process queue.
 * @param frameid Image id.
 *
 * Process the latest image in the QImage process queue.
 */
void ImageProcesser::processQImage(qint32 session, quint32 frameid, QImage image)
{
  if (!mAllowAllSources && session != mSession) {
    return;
  }
  if (frameid <= mQImageQueuePosition) {
    mProcessQueueMutex.lock();
    image = mQImageProcessQueue;
    frameid = mQImageQueuePosition;
    mQImageProcessQueue = QImage();
    mProcessQueueMutex.unlock();
  }
  if (image.isNull()) {
    fDebug << "Image " << frameid << " is null";
    return;
  }
  process(mSession, frameid, cvMatPtr(new cv::Mat(matFromQImage(image))));
}

/**
 * @brief ImageProcesser::processMat
 * @param session Session id
 * @param mat OpenCV Mat replaced if more recent image exists in process queue.
 * @param frameid Image id
 *
 * Process the latest image in the Mat process queue.
 */
void ImageProcesser::processMat(qint32 session, quint32 frameid, cvMatPtr mat)
{
  if (!mAllowAllSources && session != mSession) {
    return;
  }
  if (frameid <= mMatQueuePosition) {
    mProcessQueueMutex.lock();
    mat = mMatProcessQueue;
    frameid = mMatQueuePosition;
    mMatProcessQueue = nullptr;
    mProcessQueueMutex.unlock();
  }
  if (mat.isNull()) {
    mSkippedImages++;
    return;
  }
  if (mSkippedImages > 0) {
    fDebug << "Images arrived faster than could be processed. Skipped:"
           << mSkippedImages;
    mSkippedImages = 0;
  }
  if (mLogTime) {
    processingstarted.insert(frameid, mUptime->nsecsElapsed());
  }
  process(mSession, frameid, mat);
}


/**
 * @brief ImageProcesser::getProcessingTimes
 * @return
 */
const QMap<quint32, qint32> ImageProcesser::getProcessingTimes()
{
  mRunning = false;
  QMapIterator<quint32, qint64> processedIt(processingfinished);
  QMap<quint32, qint32> retMap;
  while (processedIt.hasNext()) {
    processedIt.next();
    if (processingstarted.contains(processedIt.key())) {
      retMap.insert(processedIt.key(),
                    ((processedIt.value() - processingstarted.value(processedIt.key()))
                     / 1000000));
    }
  }
  return retMap;
}


/**
 * @brief ImageProcesser::triggerActionA
 * Triggers action A.
 * What action A means needs to be defined in the
 * implementing class.
 */
void ImageProcesser::triggerActionA()
{
  fDebug << "ImageProcesser::triggerA";
  // After parsing the value the implementing class
  // needs to set it to false.
  mTriggeredA = true;
}

/**
 * @brief ImageProcesser::triggerActionB
 * Triggers action B.
 * What action B means needs to be defined in the
 * implementing class.
 */
void ImageProcesser::triggerActionB()
{
  fDebug << "ImageProcesser::triggerB";
  // After parsing the value the implementing class
  // needs to set it to false.
  mTriggeredB = true;
}

/**
 * @brief ImageProcesser::triggerActionC
 * Triggers action C.
 * What action C means needs to be defined in the
 * implementing class.
 */
void ImageProcesser::triggerActionC()
{
  fDebug << "ImageProcesser::triggerC";
  // After parsing the value the implementing class
  // needs to set it to false.
  mTriggeredC = true;
}

}
