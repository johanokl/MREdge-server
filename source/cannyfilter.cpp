/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "cannyfilter.h"
#include <QtCore/qbuffer.h>
#include <QDebug>
#include <QImage>
#include <QtMath>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>

namespace MREdge {

/**
 * @brief CannyFilter::medianMat
 * @param mat OpenCV Mat image
 * @return The median value for all pixels in the mat image.
 */
double CannyFilter::medianMat(cv::Mat mat)
{
  mat = mat.reshape(0, 1); // spread Input Mat to single row
  std::vector<double> vecFromMat;
  mat.copyTo(vecFromMat);
  // Partial sort the vector.
  std::nth_element(vecFromMat.begin(),
                   vecFromMat.begin() + vecFromMat.size() / 2,
                   vecFromMat.end());
  // Pick the middle element, the median
  return vecFromMat[vecFromMat.size() / 2];
}

/**
 * @brief CannyFilter::process
 * @param session Session id
 * @param image OpenCV Mat
 * @param frameid Image id.
 *
 * Run Canny edge detector on the received image.
 */
void CannyFilter::process(qint32 session, quint32 frameid, cvMatPtr image)
{
  fDebug << "CannyFilter::process";
  Q_UNUSED(session);
  cv::Mat dst;
  dst.create(image->size(), image->type());
  cv::Mat src_gray;
  cvtColor(*image, src_gray, cv::COLOR_RGB2GRAY);
  const int kernel_size = 3;
  cv::Mat detected_edges;
  blur(src_gray, detected_edges, cv::Size(3, 3));
  double sigma = 0.33;
  double v = medianMat(detected_edges);
  int lower = static_cast<int>(std::max(0.0, (1.0 - sigma) * v));
  int upper = static_cast<int>(std::min(255.0, (1.0 + sigma) * v));
  cv::Canny(detected_edges, detected_edges, lower, upper, kernel_size);
  dst = cv::Scalar::all(0);
  image->copyTo(dst, detected_edges);
  if (mEmitJPEG) {
    fDebug << "Emitting JPEG";
    emit sendFile(mSession, NetworkConnection::File(
                    NetworkConnection::FileType::IMAGE, frameid, jpegFromMat(dst)));
  }
  if (mEmitQImage) {
    fDebug << "Emitting QImage";
    emit sendQImage(mSession, frameid,
                    QImagePtr(new QImage(qImageFromMat(dst))));
  }
}

}
