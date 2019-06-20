/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef ORBSLAMPROCESSER_H
#define ORBSLAMPROCESSER_H

#include "global.h"
#include "networkconnection.h"
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QJsonObject>
#include <opencv/cv.hpp>
#include <opencv/cxcore.hpp>
#include "imageprocesser.h"
#include "ORBVocabulary.h"

namespace ORB_SLAM2 {
class System;
}

namespace std {
class thread;
}

namespace MREdge {

class ViewerAR;

/**
 * @brief The OrbSlamProcesser class
 *
 * Class to manage Mixed Reality.
 * Contains a SLAM image analyzer (ORB_SLAM2) and a 3D renderer for point
 * clouds and mixed reality visual objects.
 * Can be set in camera calibration mode to retrieve the camera properties for
 * better 3D localization results.
 */
class OrbSlamProcesser : public ImageProcesser {
  Q_OBJECT

  typedef std::vector<cv::Point3f>  objPoints; // object points (3D) for one image
  typedef std::vector<cv::Point2f>  imgPoints; // image point (2D) for one image

public:
  explicit OrbSlamProcesser(qint32 session, ORB_SLAM2::ORBVocabulary *voc,
                            bool benchmarking, bool logTimes, bool loopClosing);
  ~OrbSlamProcesser() override;
  void setDebugMode(bool enable) override;

public slots:
  void setConfig(QJsonObject calibration) override;
  void setUserInteractionConfiguration(QJsonObject config) override;
  void setCalibrateMode(bool enabled) override;
  void calibrateCamera() override;

protected:
  void process(qint32 session, quint32 frameid, cvMatPtr mat) override;

private:
  bool mCalibrateMode;
  cv::Mat processImageForCalibration(cv::Mat image);
  std::vector<objPoints> mArrObjPoints; //array of object points for an array of images
  std::vector<imgPoints> mArrImgPoints; //array of images points for an array of iamges
  cv::Mat mOutCamMatrix = cv::Mat(3, 3, CV_32FC1);
  cv::Mat mOutDistCoeffs; // This is what we need to undistort an image
  cv::Size mImgSizeCV;
  QSize mImgSize;
  ORB_SLAM2::ORBVocabulary *mVocabulary;
  ORB_SLAM2::System *mSLAM;
  bool mBenchmarking;
  bool mLogTime;
  cv::Mat mK;
  cv::Mat mDistCoef;
  ViewerAR *mViewerAR;
  QString m3DObjectType;
  std::thread* mViewerARthread;
  bool mLoopClosing;
};

}

#endif // ORBSLAMPROCESSER_H
